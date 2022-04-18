/*
 * Copyright (C) 2013-2015 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2013-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2022 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 André Nusser <andre.nusser@googlemail.com>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>

#include <sigc++/bind.h>

#include <gtkmm/filechooserdialog.h>
#include <gtkmm/frame.h>
#include <gtkmm/stock.h>
#include <gtkmm/table.h>

#include "pbd/convert.h"
#include "pbd/error.h"

#include "ardour/session.h"
#include "ardour/session_directory.h"

#include "ardour_ui.h"
#include "gui_thread.h"

#include "ardour/export_channel_configuration.h"
#include "ardour/export_filename.h"
#include "ardour/export_format_specification.h"
#include "ardour/export_handler.h"
#include "ardour/export_timespan.h"
#include "ardour/session_metadata.h"

#include "gtkmm2ext/utils.h"
#include "widgets/tooltips.h"

#include "ardour_message.h"
#include "export_video_dialog.h"
#include "utils_videotl.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace VideoUtils;

ExportVideoDialog::ExportVideoDialog ()
	: ArdourDialog (_("Export Video File "))
	, _aborted (false)
	, _normalize (false)
	, _previous_progress (0)
	, _transcoder (0)
	, outfn_path_label (_("File:"), Gtk::ALIGN_START)
	, outfn_browse_button (_("Browse"))
	, invid_path_label (_("Video:"), Gtk::ALIGN_START)
	, invid_browse_button (_("Browse"))
	, transcode_button (_("Export"))
	, abort_button (_("Abort"))
	, progress_box (0)
	, normalize_checkbox (_("Normalize audio"))
	, copy_video_codec_checkbox (_("Mux only - copy video codec"))
	, meta_checkbox (_("Include session metadata"))
	, debug_checkbox (_("Debug Mode: Print ffmpeg command and output to stdout."))
{
	set_name ("ExportVideoDialog");
	set_modal (true);
	set_skip_taskbar_hint (true);
	set_resizable (false);

	Gtk::Label* l;
	vbox = manage (new VBox);
	HBox* path_hbox;

	/* check if ffmpeg can be found */
	_transcoder = new TranscodeFfmpeg (X_(""));
	if (!_transcoder->ffexec_ok ()) {
		l = manage (new Label (_("ffmpeg installation was not found. Video Export is not possible. See the Log window for more information."), Gtk::ALIGN_START, Gtk::ALIGN_CENTER, false));
		l->set_line_wrap ();
		vbox->pack_start (*l, false, false, 8);
		get_vbox ()->pack_start (*vbox, false, false);
		add_button (Stock::OK, RESPONSE_CANCEL);
		show_all_children ();
		delete _transcoder;
		_transcoder = 0;
		return;
	}
	delete _transcoder;
	_transcoder = 0;

	Gtk::Frame* f;
	Table*      t;

	f         = manage (new Gtk::Frame (_("Output (file extension defines format)")));
	path_hbox = manage (new HBox);
	path_hbox->pack_start (outfn_path_label, false, false, 3);
	path_hbox->pack_start (outfn_path_entry, true, true, 3);
	path_hbox->pack_start (outfn_browse_button, false, false, 3);
	f->add (*path_hbox);
	path_hbox->set_border_width (2);
	vbox->pack_start (*f, false, false, 4);

	f               = manage (new Gtk::Frame (_("Input")));
	VBox* input_box = manage (new VBox);
	path_hbox       = manage (new HBox);
	path_hbox->pack_start (invid_path_label, false, false, 3);
	path_hbox->pack_start (invid_path_entry, true, true, 3);
	path_hbox->pack_start (invid_browse_button, false, false, 3);
	input_box->pack_start (*path_hbox, false, false, 2);

	path_hbox = manage (new HBox);
	l         = manage (new Label (_("Audio:"), ALIGN_START, ALIGN_CENTER, false));
	path_hbox->pack_start (*l, false, false, 3);
	l = manage (new Label (_("Master Bus"), ALIGN_START, ALIGN_CENTER, false));
	path_hbox->pack_start (*l, false, false, 2);
	input_box->pack_start (*path_hbox, false, false, 2);

	input_box->set_border_width (2);
	f->add (*input_box);
	vbox->pack_start (*f, false, false, 4);

	outfn_path_entry.set_width_chars (38);

	audio_bitrate_combo.append (_("(default for codec)"));
	audio_bitrate_combo.append ("64k");
	audio_bitrate_combo.append ("128k");
	audio_bitrate_combo.append ("192k");
	audio_bitrate_combo.append ("256k");
	audio_bitrate_combo.append ("320k");

	audio_sample_rate_combo.append (_("Session rate"));
	audio_sample_rate_combo.append ("44100");
	audio_sample_rate_combo.append ("48000");

	f = manage (new Gtk::Frame (_("Settings")));
	t = manage (new Table (3, 2));
	t->set_border_width (2);
	t->set_spacings (4);
	int ty = 0;

	l = manage (new Label (_("Range:"), ALIGN_START, ALIGN_CENTER, false));
	t->attach (*l, 0, 1, ty, ty + 1);
	t->attach (insnd_combo, 1, 2, ty, ty + 1);
	ty++;

	l = manage (new Label (_("Sample rate:"), ALIGN_START, ALIGN_CENTER, false));
	t->attach (*l, 0, 1, ty, ty + 1);
	t->attach (audio_sample_rate_combo, 1, 2, ty, ty + 1);
	ty++;

	l = manage (new Label (_("Audio Quality:"), ALIGN_START, ALIGN_CENTER, false));
	t->attach (*l, 0, 1, ty, ty + 1);
	t->attach (audio_bitrate_combo, 1, 2, ty, ty + 1);
	ty++;

	t->attach (normalize_checkbox, 0, 2, ty, ty + 1);
	ty++;
	t->attach (copy_video_codec_checkbox, 0, 2, ty, ty + 1);
	ty++;
	t->attach (meta_checkbox, 0, 2, ty, ty + 1);
	ty++;
	t->attach (debug_checkbox, 0, 2, ty, ty + 1);
	ty++;

	f->add (*t);
	vbox->pack_start (*f, false, true, 4);

	get_vbox ()->set_spacing (4);
	get_vbox ()->pack_start (*vbox, false, false);

	progress_box = manage (new VBox);
	progress_box->pack_start (pbar, false, false);
	progress_box->pack_start (abort_button, false, false);
	get_vbox ()->pack_start (*progress_box, false, false);

	set_tooltip (normalize_checkbox, _("<b>When enabled</b>, the audio is normalized to 0dBFS during export."));
	set_tooltip (copy_video_codec_checkbox, _("<b>When enabled</b>, the video is not re-encoded, but the original video codec is reused. In some cases this can lead to audio/video synchronization issues. This also only works if the exported range is not longer than the video. Adding black space at the start or end requires encoding.\n<b>When disabled</b>, the video is re-encoded, this may lead to quality loss, but this is the safer option and generally preferable."));
	set_tooltip (meta_checkbox, _("<b>When enabled</b>, information from Menu > Session > Metadata is included in the video file."));
	set_tooltip (audio_sample_rate_combo, _("Select the sample rate of the audio track. Prefer 48kHz, which is the standard for video files."));
	set_tooltip (audio_bitrate_combo, _("Select the bitrate of the audio track in kbit/sec. Higher values result in better quality, but also a larger file."));

	outfn_browse_button.signal_clicked ().connect (sigc::mem_fun (*this, &ExportVideoDialog::open_outfn_dialog));
	invid_browse_button.signal_clicked ().connect (sigc::mem_fun (*this, &ExportVideoDialog::open_invid_dialog));
	transcode_button.signal_clicked ().connect (sigc::mem_fun (*this, &ExportVideoDialog::launch_export));
	abort_button.signal_clicked ().connect (sigc::mem_fun (*this, &ExportVideoDialog::abort_clicked));

	invid_path_entry.signal_changed ().connect (sigc::mem_fun (*this, &ExportVideoDialog::set_original_file_information));

	cancel_button = add_button (Stock::CANCEL, RESPONSE_CANCEL);
	get_action_area ()->pack_start (transcode_button, false, false);
	show_all_children ();

	progress_box->set_no_show_all ();
	progress_box->hide ();
}

ExportVideoDialog::~ExportVideoDialog ()
{
	if (_transcoder) {
		delete _transcoder;
		_transcoder = 0;
	}
}

void
ExportVideoDialog::set_original_file_information ()
{
	assert (_transcoder == 0);
	string infile = invid_path_entry.get_text ();

	if (infile.empty () || !Glib::file_test (infile, Glib::FILE_TEST_EXISTS)) {
		transcode_button.set_sensitive (false);
		return;
	}

	_transcoder = new TranscodeFfmpeg (infile);
	transcode_button.set_sensitive (_transcoder->probe_ok ());
	delete _transcoder;
	_transcoder = 0;
}

void
ExportVideoDialog::apply_state (TimeSelection& tme, bool range)
{
	_export_range = tme;

	outfn_path_entry.set_text (_session->session_directory ().export_path () + G_DIR_SEPARATOR + "export.mp4");

	// TODO remember setting for export-range.. somehow, (let explicit range override)
	sampleoffset_t av_offset = ARDOUR_UI::instance ()->video_timeline->get_offset ();

	insnd_combo.remove_all ();
	insnd_combo.append (_("from session start marker to session end marker"));

	if (av_offset < 0) {
		insnd_combo.append (_("from 00:00:00:00 to the video end"));
	} else {
		insnd_combo.append (_("from video start to video end"));
	}
	if (!_export_range.empty ()) {
		insnd_combo.append (_("Selected range")); // TODO show _export_range.start() -> _export_range.end_sample()
	}

	/* default settings */
	if (range) {
		insnd_combo.set_active (2);
	} else {
		insnd_combo.set_active (1);
	}
	audio_bitrate_combo.set_active (0);
	audio_sample_rate_combo.set_active (0);
	normalize_checkbox.set_active (false);
	copy_video_codec_checkbox.set_active (false);
	meta_checkbox.set_active (false);

	/* set original video file */
	XMLNode* node        = _session->extra_xml (X_("Videotimeline"));
	bool     filenameset = false;
	if (node) {
		string filename;
		if (node->get_property (X_("OriginalVideoFile"), filename)) {
			if (Glib::file_test (filename, Glib::FILE_TEST_EXISTS)) {
				invid_path_entry.set_text (filename);
				filenameset = true;
			}
		}

		bool local_file;

		if (!filenameset && node->get_property (X_("Filename"), filename) &&
		    node->get_property (X_("LocalFile"), local_file) && local_file) {
			if (filename.at (0) != G_DIR_SEPARATOR) {
				filename = Glib::build_filename (_session->session_directory ().video_path (), filename);
			}
			if (Glib::file_test (filename, Glib::FILE_TEST_EXISTS)) {
				invid_path_entry.set_text (filename);
				filenameset = true;
			}
		}
	}

	if (!filenameset) {
		invid_path_entry.set_text (X_(""));
	}

	/* apply saved state, if any */
	node = _session->extra_xml (X_("Videoexport"));
	if (node) {
		bool   yn;
		int    num;
		string str;
		if (node->get_property (X_("NormalizeAudio"), yn)) {
			normalize_checkbox.set_active (yn);
		}
		if (node->get_property (X_("CopyVCodec"), yn)) {
			copy_video_codec_checkbox.set_active (yn);
		}
		if (node->get_property (X_("Metadata"), yn)) {
			meta_checkbox.set_active (yn);
		}
		if (node->get_property (X_("ExportRange"), num)) {
			if (!range && num >= 0 && num < 1) {
				insnd_combo.set_active (num);
			}
		}
		if (node->get_property (X_("AudioSRChoice"), num)) {
			if (num >= 0 && num < 3) {
				audio_sample_rate_combo.set_active (num);
			}
		}
		if (node->get_property (X_("OutputFile"), str)) {
			outfn_path_entry.set_text (str);
		}
		if (node->get_property (X_("AudioBitrate"), str)) {
			Gtkmm2ext::set_active_text_if_present (audio_bitrate_combo, str);
		}
	}

	set_original_file_information ();

	show_all_children ();
	if (progress_box) {
		progress_box->hide ();
	}
}

XMLNode&
ExportVideoDialog::get_state () const
{
	XMLNode* node = new XMLNode (X_("Videoexport"));
	node->set_property (X_("OutputFile"), outfn_path_entry.get_text ());
	node->set_property (X_("ExportRange"), insnd_combo.get_active_row_number ());
	node->set_property (X_("AudioSRChoice"), audio_sample_rate_combo.get_active_row_number ());
	node->set_property (X_("AudioBitrate"), audio_bitrate_combo.get_active_text ());
	node->set_property (X_("NormalizeAudio"), normalize_checkbox.get_active ());
	node->set_property (X_("CopyVCodec"), copy_video_codec_checkbox.get_active ());
	node->set_property (X_("Metadata"), meta_checkbox.get_active ());
	return *node;
}

void
ExportVideoDialog::set_state (const XMLNode&)
{
}

void
ExportVideoDialog::abort_clicked ()
{
	_aborted = true;
	if (_transcoder) {
		_transcoder->cancel ();
	}
}

void
ExportVideoDialog::update_progress (samplecnt_t c, samplecnt_t a)
{
	if (a == 0 || c > a) {
		pbar.set_pulse_step (.1);
		pbar.pulse ();
	} else {
		double progress = (double)c / (double)a;
		progress        = progress / (_normalize ? 3.0 : 2.0);
		if (_normalize) {
			progress += 2.0 / 3.0;
		} else {
			progress += .5;
		}
		pbar.set_fraction (progress);
	}
}

gint
ExportVideoDialog::audio_progress_display ()
{
	string status_text;
	double progress = -1.0;
	switch (status->active_job) {
		case ExportStatus::Normalizing:
			pbar.set_text (_("Normalizing audio"));
			progress = ((float)status->current_postprocessing_cycle) / status->total_postprocessing_cycles;
			progress = (progress + 1.0) / 3.0;
			break;
		case ExportStatus::Exporting:
			pbar.set_text (_("Exporting audio"));
			progress = ((float)status->processed_samples_current_timespan) / status->total_samples_current_timespan;
			progress = progress / (_normalize ? 3.0 : 2.0);
			break;
		default:
			pbar.set_text (_("Exporting audio"));
			break;
	}

	if (progress < _previous_progress) {
		/* Work around gtk bug */
		pbar.hide ();
		pbar.show ();
	}

	_previous_progress = progress;

	if (progress >= 0) {
		pbar.set_fraction (progress);
	} else {
		pbar.set_pulse_step (.1);
		pbar.pulse ();
	}
	return TRUE;
}

void
ExportVideoDialog::finished (int status)
{
	delete _transcoder;
	_transcoder = 0;
	if (_aborted || status != 0) {
		if (!_aborted) {
			ARDOUR_UI::instance ()->popup_error (_("Video transcoding failed."));
		}
		::g_unlink (outfn_path_entry.get_text ().c_str ());
		::g_unlink (_insnd.c_str ());
		Gtk::Dialog::response (RESPONSE_CANCEL);
	} else {
		if (!debug_checkbox.get_active ()) {
			::g_unlink (_insnd.c_str ());
		}
		Gtk::Dialog::response (RESPONSE_ACCEPT);
	}
}

void
ExportVideoDialog::launch_export ()
{
	/* remember current settings.
	 * needed because apply_state() acts on both:
	 * "Videotimeline" and "Video Export" extra XML
	 * as well as current _session settings
	 */
	_session->add_extra_xml (get_state ());

	string outfn = outfn_path_entry.get_text ();
	if (!confirm_video_outfn (*this, outfn)) {
		return;
	}

	vbox->hide ();
	cancel_button->hide ();
	transcode_button.hide ();
	pbar.set_size_request (300, -1);
	pbar.set_text (_("Exporting Audio..."));
	progress_box->show ();
	_aborted   = false;
	_normalize = normalize_checkbox.get_active ();

	/* export audio track */
	ExportTimespanPtr                              tsp = _session->get_export_handler ()->add_timespan ();
	boost::shared_ptr<ExportChannelConfiguration>  ccp = _session->get_export_handler ()->add_channel_config ();
	boost::shared_ptr<ARDOUR::ExportFilename>      fnp = _session->get_export_handler ()->add_filename ();
	boost::shared_ptr<AudioGrapher::BroadcastInfo> b;

	XMLTree tree;
	string  vtl_normalize = _normalize ? "true" : "false";
	string  vtl_samplerate;

	switch (audio_sample_rate_combo.get_active_row_number ()) {
		case 0:
			vtl_samplerate = string_compose ("%1", _session->nominal_sample_rate ());
			break;
		case 1:
			vtl_samplerate = "44100";
			break;
		default:
			vtl_samplerate = "48000";
			break;
	}

	/* clang-format off */
	tree.read_buffer (std::string (
	                  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
	                  "<ExportFormatSpecification name=\"VTL-WAV-16\" id=\"3094591e-ccb9-4385-a93f-c9955ffeb1f0\">"
	                  "  <Encoding id=\"F_WAV\" type=\"T_Sndfile\" extension=\"wav\" name=\"WAV\" has-sample-format=\"true\" channel-limit=\"256\"/>"
	                  "  <SampleRate rate=\"" + vtl_samplerate + "\"/>"
	                  "  <SRCQuality quality=\"SRC_SincBest\"/>"
	                  "  <EncodingOptions>"
	                  "    <Option name=\"sample-format\" value=\"SF_16\"/>"
	                  "    <Option name=\"dithering\" value=\"D_None\"/>"
	                  "    <Option name=\"tag-metadata\" value=\"true\"/>"
	                  "    <Option name=\"tag-support\" value=\"false\"/>"
	                  "    <Option name=\"broadcast-info\" value=\"false\"/>"
	                  "  </EncodingOptions>"
	                  "  <Processing>"
	                  "    <Normalize enabled=\"" + vtl_normalize + "\" target=\"0\"/>"
	                  "    <Silence>"
	                  "      <Start>"
	                  "        <Trim enabled=\"false\"/>"
	                  "        <Add enabled=\"false\">"
	                  "          <Duration format=\"Timecode\" hours=\"0\" minutes=\"0\" seconds=\"0\" frames=\"0\"/>"
	                  "        </Add>"
	                  "      </Start>"
	                  "      <End>"
	                  "        <Trim enabled=\"false\"/>"
	                  "        <Add enabled=\"false\">"
	                  "          <Duration format=\"Timecode\" hours=\"0\" minutes=\"0\" seconds=\"0\" frames=\"0\"/>"
	                  "        </Add>"
	                  "      </End>"
	                  "    </Silence>"
	                  "  </Processing>"
	                  "</ExportFormatSpecification>")
	                  .c_str ());
	/* clang-format on */

	boost::shared_ptr<ExportFormatSpecification> fmp = _session->get_export_handler ()->add_format (*tree.root ());

	/* set up range */
	samplepos_t start, end;
	start = end = 0;
	if (insnd_combo.get_active_row_number () == 1) {
		_transcoder = new TranscodeFfmpeg (invid_path_entry.get_text ());
		if (_transcoder->probe_ok () && _transcoder->get_fps () > 0) {
			end = _transcoder->get_duration () * _session->nominal_sample_rate () / _transcoder->get_fps ();
		} else {
			warning << _("Export Video: Cannot query duration of video-file, using duration from timeline instead.") << endmsg;
			end = ARDOUR_UI::instance ()->video_timeline->get_duration ();
		}
		if (_transcoder) {
			delete _transcoder;
			_transcoder = 0;
		}

		sampleoffset_t av_offset = ARDOUR_UI::instance ()->video_timeline->get_offset ();
#if 0 /* DEBUG */
		printf("audio-range -- AV offset: %lld\n", av_offset);
#endif
		if (av_offset > 0) {
			start = av_offset;
		}
		end += av_offset;
	} else if (insnd_combo.get_active_row_number () == 2) {
		start = ARDOUR_UI::instance ()->video_timeline->quantify_samples_to_apv (_export_range.start_sample ());
		end   = ARDOUR_UI::instance ()->video_timeline->quantify_samples_to_apv (_export_range.end_sample ());
	}
	if (end <= 0) {
		start = _session->current_start_sample ();
		end   = _session->current_end_sample ();
	}
#if 0 /* DEBUG */
	printf("audio export-range %lld -> %lld\n", start, end);
#endif

	const sampleoffset_t vstart = ARDOUR_UI::instance ()->video_timeline->get_offset ();
	const sampleoffset_t vend   = vstart + ARDOUR_UI::instance ()->video_timeline->get_duration ();

	if ((start >= end) || (end < vstart) || (start > vend)) {
		delete _transcoder;
		_transcoder = 0;
		ArdourMessageDialog msg (_("Export Video: The export-range does not include video."));
		msg.run ();
		Gtk::Dialog::response (RESPONSE_CANCEL);
		return;
	}

	if ((start < vstart || end > vend) && copy_video_codec_checkbox.get_active ()) {
		ArdourMessageDialog msg (
				_("The export-range is longer than the video file. "
				  "To add black frames the video has to be encoded. "
				  "Copying the codec may fail or not produce the intended result.\n"
				  "Continue anyway?"),
				false,
				Gtk::MESSAGE_INFO,
				Gtk::BUTTONS_YES_NO,
				true
				);
		msg.set_default_response (Gtk::RESPONSE_YES);

		if (msg.run() != Gtk::RESPONSE_YES) {
			delete _transcoder;
			_transcoder = 0;
			Gtk::Dialog::response (RESPONSE_CANCEL);
			return;
		}
	}

	tsp->set_range (start, end);
	tsp->set_name ("mysession");
	tsp->set_range_id ("session");

	/* add master outs as default */
	IO* master_out = _session->master_out ()->output ().get ();
	if (!master_out) {
		warning << _("Export Video: No Master Out Ports to Connect for Audio Export") << endmsg;
		delete _transcoder;
		_transcoder = 0;
		Gtk::Dialog::response (RESPONSE_CANCEL);
		return;
	}
	for (uint32_t n = 0; n < master_out->n_ports ().n_audio (); ++n) {
		PortExportChannel* channel = new PortExportChannel ();
		channel->add_port (master_out->audio (n));
		ExportChannelPtr chan_ptr (channel);
		ccp->register_channel (chan_ptr);
	}

	/* outfile */
	fnp->set_timespan (tsp);
	fnp->set_label ("vtl");
	fnp->include_label = true;
	_insnd             = fnp->get_path (fmp);

	/* do sound export */
	fmp->set_soundcloud_upload (false);
	_session->get_export_handler ()->reset ();
	_session->get_export_handler ()->add_export_config (tsp, ccp, fmp, fnp, b);
	_session->get_export_handler ()->do_export ();
	status = _session->get_export_status ();

	_audio_progress_connection = Glib::signal_timeout ().connect (sigc::mem_fun (*this, &ExportVideoDialog::audio_progress_display), 100);
	_previous_progress         = 0.0;
	while (status->running ()) {
		if (_aborted) {
			status->abort ();
		}
		if (gtk_events_pending ()) {
			gtk_main_iteration ();
		} else {
			Glib::usleep (10000);
		}
	}
	_audio_progress_connection.disconnect ();
	status->finish (TRS_UI);
	if (status->aborted ()) {
		::g_unlink (_insnd.c_str ());
		delete _transcoder;
		_transcoder = 0;
		Gtk::Dialog::response (RESPONSE_CANCEL);
		return;
	}
	pbar.set_text (_("Encoding Video..."));
	encode_video ();
}

void
ExportVideoDialog::encode_video ()
{
	std::string outfn = outfn_path_entry.get_text ();
	std::string invid = invid_path_entry.get_text ();

	_transcoder = new TranscodeFfmpeg (invid);
	if (!_transcoder->ffexec_ok ()) {
		/* ffmpeg binary was not found. TranscodeFfmpeg prints a warning */
		::g_unlink (_insnd.c_str ());
		delete _transcoder;
		_transcoder = 0;
		Gtk::Dialog::response (RESPONSE_CANCEL);
		return;
	}
	if (!_transcoder->probe_ok ()) {
		/* video input file can not be read */
		warning << _("Export Video: Video input file cannot be read.") << endmsg;
		::g_unlink (_insnd.c_str ());
		delete _transcoder;
		_transcoder = 0;
		Gtk::Dialog::response (RESPONSE_CANCEL);
		return;
	}

	TranscodeFfmpeg::FFSettings ffs; /* = transcoder->default_encoder_settings(); */
	ffs.clear ();

	bool map = true;

	sampleoffset_t av_offset  = ARDOUR_UI::instance ()->video_timeline->get_offset ();
	double         duration_s = 0;

	if (insnd_combo.get_active_row_number () == 0) {
		/* session start to session end */
		samplecnt_t duration_f = _session->current_end_sample () - _session->current_start_sample ();
		duration_s             = (double)duration_f / (double)_session->nominal_sample_rate ();
	} else if (insnd_combo.get_active_row_number () == 2) {
		/* selected range */
		duration_s = _export_range.length_samples () / (double)_session->nominal_sample_rate ();
	} else {
		/* video start to end */
		samplecnt_t duration_f = ARDOUR_UI::instance ()->video_timeline->get_duration ();
		if (av_offset < 0) {
			duration_f += av_offset;
		}
		duration_s = (double)duration_f / (double)_session->nominal_sample_rate ();
	}

	std::ostringstream osstream;
	osstream << duration_s;
	ffs["-t"] = osstream.str ();

	_transcoder->set_duration (duration_s * _transcoder->get_fps ());

	if (insnd_combo.get_active_row_number () == 0 || insnd_combo.get_active_row_number () == 2) {
		samplepos_t          start, snend;
		const sampleoffset_t vid_duration = ARDOUR_UI::instance ()->video_timeline->get_duration ();
		if (insnd_combo.get_active_row_number () == 0) {
			start = _session->current_start_sample ();
			snend = _session->current_end_sample ();
		} else {
			start = _export_range.start_sample ();
			snend = _export_range.end_sample ();
		}

#if 0 /* DEBUG */
		printf("AV offset: %lld Vid-len: %lld Vid-end: %lld || start:%lld || end:%lld\n",
				av_offset, vid_duration, av_offset+vid_duration, start, snend); // XXX
#endif

		if (av_offset > start && av_offset + vid_duration < snend) {
			_transcoder->set_leadinout ((av_offset - start) / (double)_session->nominal_sample_rate (),
			                            (snend - (av_offset + vid_duration)) / (double)_session->nominal_sample_rate ());
		} else if (av_offset > start) {
			_transcoder->set_leadinout ((av_offset - start) / (double)_session->nominal_sample_rate (), 0);
		} else if (av_offset + vid_duration < snend) {
			_transcoder->set_leadinout (0, (snend - (av_offset + vid_duration)) / (double)_session->nominal_sample_rate ());
			_transcoder->set_avoffset ((av_offset - start) / (double)_session->nominal_sample_rate ());
		}
#if 0
		else if (start > av_offset) {
			std::ostringstream osstream; osstream << ((start - av_offset) / (double)_session->nominal_sample_rate());
			ffs["-ss"] = osstream.str();
		}
#endif
		else {
			_transcoder->set_avoffset ((av_offset - start) / (double)_session->nominal_sample_rate ());
		}

	} else if (av_offset < 0) {
		/* from 00:00:00:00 to video-end */
		_transcoder->set_avoffset (av_offset / (double)_session->nominal_sample_rate ());
	}

	/* NOTE: type (MetaDataMap) == type (FFSettings) == map<string, string> */
	ARDOUR::SessionMetadata::MetaDataMap meta = _transcoder->default_meta_data ();
	if (meta_checkbox.get_active ()) {
		ARDOUR::SessionMetadata* session_data = ARDOUR::SessionMetadata::Metadata ();
		session_data->av_export_tag (meta);
	}

	if (debug_checkbox.get_active ()) {
		_transcoder->set_debug (true);
	}

	if (copy_video_codec_checkbox.get_active ()) {
		ffs["-codec:v"] = "copy";
	}

	if (audio_bitrate_combo.get_active_row_number () > 0) {
		ffs["-b:a"] = audio_bitrate_combo.get_active_text ();
	}

	_transcoder->Progress.connect (*this, invalidator (*this), boost::bind (&ExportVideoDialog::update_progress, this, _1, _2), gui_context ());
	_transcoder->Finished.connect (*this, invalidator (*this), boost::bind (&ExportVideoDialog::finished, this, _1), gui_context ());

	if (!_transcoder->encode (outfn, _insnd, invid, ffs, meta, map)) {
		ARDOUR_UI::instance ()->popup_error (_("Transcoding failed."));
		delete _transcoder;
		_transcoder = 0;
		Gtk::Dialog::response (RESPONSE_CANCEL);
		return;
	}
}

void
ExportVideoDialog::open_outfn_dialog ()
{
	FileChooserDialog dialog (_("Save Exported Video File"), FILE_CHOOSER_ACTION_SAVE);
	Gtkmm2ext::add_volume_shortcuts (dialog);
	dialog.set_filename (outfn_path_entry.get_text ());

	dialog.add_button (Stock::CANCEL, RESPONSE_CANCEL);
	dialog.add_button (Stock::OK, RESPONSE_OK);

	int result = dialog.run ();

	if (result == RESPONSE_OK) {
		std::string filename = dialog.get_filename ();

		if (filename.length ()) {
			outfn_path_entry.set_text (filename);
		}
		std::string ext = get_file_extension (filename);
		if (ext != "mp4" || ext != "mov" || ext != "mkv") {
			dialog.hide ();
			ArdourMessageDialog msg (_("The file extension defines the format and codec.\nPrefer to use .mp4, .mov or .mkv. Otherwise encoding may fail."));
			msg.run ();
		}
	}
}

void
ExportVideoDialog::open_invid_dialog ()
{
	FileChooserDialog dialog (_("Input Video File"), FILE_CHOOSER_ACTION_OPEN);
	Gtkmm2ext::add_volume_shortcuts (dialog);
	dialog.set_filename (invid_path_entry.get_text ());

	dialog.add_button (Stock::CANCEL, RESPONSE_CANCEL);
	dialog.add_button (Stock::OK, RESPONSE_OK);

	int result = dialog.run ();

	if (result == RESPONSE_OK) {
		std::string filename = dialog.get_filename ();
		if (!filename.empty ()) {
			invid_path_entry.set_text (filename);
		}
	}
}
