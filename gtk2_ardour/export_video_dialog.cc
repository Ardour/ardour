/*
    Copyright (C) 2010 Paul Davis
    Author: Robin Gareus <robin@gareus.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/
#include <cstdio>
#include <string>
#include <sstream>
#include <iomanip>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sigc++/bind.h>

#include <glib/gstdio.h>

#include "pbd/error.h"
#include "pbd/convert.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/utils.h"
#include "ardour/session_directory.h"
#include "ardour/profile.h"
#include "ardour/template_utils.h"
#include "ardour/session.h"
#include "ardour_ui.h"
#include "gui_thread.h"

#include "ardour/export_handler.h"
#include "ardour/export_status.h"
#include "ardour/export_timespan.h"
#include "ardour/export_channel_configuration.h"
#include "ardour/export_format_specification.h"
#include "ardour/export_filename.h"
#include "ardour/route.h"
#include "ardour/session_metadata.h"
#include "ardour/broadcast_info.h"

#include "opts.h"
#include "export_video_dialog.h"
#include "utils_videotl.h"
#include "i18n.h"

#ifdef COMPILER_MSVC
#define rintf(x) round((x) + 0.5)
#endif

using namespace Gtk;
using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace VideoUtils;

ExportVideoDialog::ExportVideoDialog ()
	: ArdourDialog (_("Export Video File "))
	, _aborted(false)
	, _twopass(false)
	, _firstpass(false)
	, _normalize(false)
	, _previous_progress(0)
	, _transcoder(0)
	, _video_source_aspect_ratio(-1)
	, _suspend_signals(false)
	, outfn_path_label (_("File:"), Gtk::ALIGN_LEFT)
	, outfn_browse_button (_("Browse"))
	, invid_path_label (_("Video:"), Gtk::ALIGN_LEFT)
	, invid_browse_button (_("Browse"))
	, transcode_button (_("Export"))
	, abort_button (_("Abort"))
	, progress_box (0)
	, scale_checkbox (_("Scale Video (W x H):"))
	, scale_aspect (_("Retain Aspect"))
	, width_adjustment (768, 128, 1920, 1, 16, 0)
	, width_spinner (width_adjustment)
	, height_adjustment (576, 128, 1920, 1, 16, 0)
	, height_spinner (height_adjustment)
	, aspect_checkbox (_("Set Aspect Ratio:"))
	, normalize_checkbox (_("Normalize Audio"))
	, twopass_checkbox (_("2 Pass Encoding"))
	, optimizations_checkbox (_("Codec Optimizations:"))
	, optimizations_label ("-")
	, deinterlace_checkbox (_("Deinterlace"))
	, bframes_checkbox (_("Use [2] B-frames (MPEG 2 or 4 only)"))
	, fps_checkbox (_("Override FPS (Default is to retain FPS from the input video file):"))
	, meta_checkbox (_("Include Session Metadata"))
#if 1 /* tentative debug mode */
	, debug_checkbox (_("Debug Mode: Print ffmpeg command and output to stdout."))
#endif
{
	set_name ("ExportVideoDialog");
	set_modal (true);
	set_skip_taskbar_hint (true);
	set_resizable (false);

	Gtk::Label* l;
	vbox = manage (new VBox);
	VBox* options_box = manage (new VBox);
	HBox* path_hbox;

	/* check if ffmpeg can be found */
	_transcoder = new TranscodeFfmpeg(X_(""));
	if (!_transcoder->ffexec_ok()) {
		l = manage (new Label (_("No ffprobe or ffmpeg executables could be found on this system. Video Export is not possible until you install those tools. See the Log window for more information."), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
		l->set_line_wrap();
		vbox->pack_start (*l, false, false, 8);
		get_vbox()->pack_start (*vbox, false, false);
		add_button (Stock::OK, RESPONSE_CANCEL);
		show_all_children ();
		delete _transcoder; _transcoder = 0;
		return;
	}
	delete _transcoder; _transcoder = 0;

	l = manage (new Label (_("<b>Output:</b> (file extension defines format)"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	l->set_use_markup ();
	vbox->pack_start (*l, false, false, 4);

	path_hbox = manage (new HBox);
	path_hbox->pack_start (outfn_path_label, false, false, 3);
	path_hbox->pack_start (outfn_path_entry, true, true, 3);
	path_hbox->pack_start (outfn_browse_button, false, false, 3);
	vbox->pack_start (*path_hbox, false, false, 2);

	l = manage (new Label (_("<b>Input Video:</b>"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	l->set_use_markup ();
	vbox->pack_start (*l, false, false, 4);

	path_hbox = manage (new HBox);
	path_hbox->pack_start (invid_path_label, false, false, 3);
	path_hbox->pack_start (invid_path_entry, true, true, 3);
	path_hbox->pack_start (invid_browse_button, false, false, 3);
	vbox->pack_start (*path_hbox, false, false, 2);

	path_hbox = manage (new HBox);
	l = manage (new Label (_("Audio:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	path_hbox->pack_start (*l, false, false, 3);
	l = manage (new Label (_("Master Bus"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	path_hbox->pack_start (*l, false, false, 2);
	vbox->pack_start (*path_hbox, false, false, 2);

	insnd_combo.set_name ("PaddedButton");
	insnd_combo.append_text (string_compose (_("from the %1 session's start to the session's end"), PROGRAM_NAME));
	outfn_path_entry.set_width_chars(38);

	l = manage (new Label (_("<b>Settings:</b>"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	l->set_use_markup ();
	options_box->pack_start (*l, false, true, 4);

	Table* t = manage (new Table (4, 12));
	t->set_spacings (4);
	int ty = 0;
	options_box->pack_start (*t, true, true, 4);
	l = manage (new Label (_("Range:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	t->attach (*l, 0, 1, ty, ty+1);
	t->attach (insnd_combo, 1, 4, ty, ty+1); ty++;
	l = manage (new Label (_("Preset:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	t->attach (*l, 0, 1, ty, ty+1);
	t->attach (preset_combo, 1, 4, ty, ty+1); ty++;
	l = manage (new Label (_("Video Codec:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	t->attach (*l, 0, 1, ty, ty+1);
	t->attach (video_codec_combo, 1, 2, ty, ty+1);
	l = manage (new Label (_("Video KBit/s:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	t->attach (*l, 2, 3, ty, ty+1);
	t->attach (video_bitrate_combo, 3, 4, ty, ty+1); ty++;
	l = manage (new Label (_("Audio Codec:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	t->attach (*l, 0, 1, ty, ty+1);
	t->attach (audio_codec_combo, 1, 2, ty, ty+1);
	l = manage (new Label (_("Audio KBit/s:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	t->attach (*l, 2, 3, ty, ty+1);
	t->attach (audio_bitrate_combo, 3, 4, ty, ty+1); ty++;
	l = manage (new Label (_("Audio Samplerate:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	t->attach (*l, 0, 1, ty, ty+1);
	t->attach (audio_samplerate_combo, 1, 2, ty, ty+1);
	t->attach (normalize_checkbox, 2, 4, ty, ty+1); ty++;
	t->attach (scale_checkbox, 0, 1, ty, ty+1);
	t->attach (scale_aspect, 1, 2, ty, ty+1);
	t->attach (width_spinner, 2, 3, ty, ty+1);
	t->attach (height_spinner, 3, 4, ty, ty+1); ty++;
	t->attach (fps_checkbox, 0, 3, ty, ty+1);
	t->attach (fps_combo, 3, 4, ty, ty+1); ty++;
	t->attach (twopass_checkbox, 0, 2, ty, ty+1);
	t->attach (aspect_checkbox, 2, 3, ty, ty+1);
	t->attach (aspect_combo, 3, 4, ty, ty+1); ty++;
	t->attach (bframes_checkbox, 0, 2, ty, ty+1);
	t->attach (deinterlace_checkbox, 2, 4, ty, ty+1); ty++;
	t->attach (meta_checkbox, 2, 4, ty, ty+1); ty++;
	t->attach (optimizations_checkbox, 0, 1, ty, ty+1);
	t->attach (optimizations_label, 1, 4, ty, ty+1); ty++;
#if 1 /* tentative debug mode */
	t->attach (debug_checkbox, 0, 4, ty, ty+1); ty++;
#endif

	preset_combo.set_name ("PaddedButton");
	preset_combo.append_text("none");
	preset_combo.append_text("dvd-mp2");
	preset_combo.append_text("dvd-NTSC");
	preset_combo.append_text("dvd-PAL");
	preset_combo.append_text("flv");
	preset_combo.append_text("mpeg4");
	preset_combo.append_text("mp4/h264/aac");
	preset_combo.append_text("ogg");
	preset_combo.append_text("webm");
	preset_combo.append_text("you-tube");

	audio_codec_combo.set_name ("PaddedButton");
	audio_codec_combo.append_text(_("(default for format)"));
	audio_codec_combo.append_text("ac3");
	audio_codec_combo.append_text("aac");
	audio_codec_combo.append_text("libmp3lame");
	audio_codec_combo.append_text("libvorbis");
	audio_codec_combo.append_text("mp2");
	audio_codec_combo.append_text("pcm_s16le");

	video_codec_combo.set_name ("PaddedButton");
	video_codec_combo.append_text(_("(default for format)"));
	video_codec_combo.append_text("flv");
	video_codec_combo.append_text("libtheora");
	video_codec_combo.append_text("mjpeg");
	video_codec_combo.append_text("mpeg2video");
	video_codec_combo.append_text("mpeg4");
	video_codec_combo.append_text("h264");
	video_codec_combo.append_text("vpx (webm)");
	video_codec_combo.append_text("copy");

	audio_bitrate_combo.set_name ("PaddedButton");
	audio_bitrate_combo.append_text(_("(default)"));
	audio_bitrate_combo.append_text("64k");
	audio_bitrate_combo.append_text("128k");
	audio_bitrate_combo.append_text("192k");
	audio_bitrate_combo.append_text("256k");
	audio_bitrate_combo.append_text("320k");

	audio_samplerate_combo.set_name ("PaddedButton");
	audio_samplerate_combo.append_text("22050");
	audio_samplerate_combo.append_text("44100");
	audio_samplerate_combo.append_text("48000");

	video_bitrate_combo.set_name ("PaddedButton");
	video_bitrate_combo.append_text(_("(default)"));
	video_bitrate_combo.append_text(_("(retain)"));
	video_bitrate_combo.append_text("200k");
	video_bitrate_combo.append_text("800k");
	video_bitrate_combo.append_text("2000k");
	video_bitrate_combo.append_text("5000k");
	video_bitrate_combo.append_text("8000k");

	fps_combo.set_name ("PaddedButton");
	fps_combo.append_text("23.976");
	fps_combo.append_text("24");
	fps_combo.append_text("24.976");
	fps_combo.append_text("25");
	fps_combo.append_text("29.97");
	fps_combo.append_text("30");
	fps_combo.append_text("59.94");
	fps_combo.append_text("60");

	aspect_combo.set_name ("PaddedButton");
	aspect_combo.append_text("4:3");
	aspect_combo.append_text("16:9");

	vbox->pack_start (*options_box, false, true, 4);
	get_vbox()->set_spacing (4);
	get_vbox()->pack_start (*vbox, false, false);

	progress_box = manage (new VBox);
	progress_box->pack_start (pbar, false, false);
	progress_box->pack_start (abort_button, false, false);
	get_vbox()->pack_start (*progress_box, false, false);

	scale_checkbox.signal_toggled().connect (sigc::mem_fun (*this, &ExportVideoDialog::scale_checkbox_toggled));
	aspect_checkbox.signal_toggled().connect (sigc::mem_fun (*this, &ExportVideoDialog::aspect_checkbox_toggled));
	fps_checkbox.signal_toggled().connect (sigc::mem_fun (*this, &ExportVideoDialog::fps_checkbox_toggled));
	preset_combo.signal_changed().connect (sigc::mem_fun (*this, &ExportVideoDialog::preset_combo_changed));
	video_codec_combo.signal_changed().connect (sigc::mem_fun (*this, &ExportVideoDialog::video_codec_combo_changed));
	outfn_browse_button.signal_clicked().connect (sigc::mem_fun (*this, &ExportVideoDialog::open_outfn_dialog));
	invid_browse_button.signal_clicked().connect (sigc::mem_fun (*this, &ExportVideoDialog::open_invid_dialog));
	transcode_button.signal_clicked().connect (sigc::mem_fun (*this, &ExportVideoDialog::launch_export));
	abort_button.signal_clicked().connect (sigc::mem_fun (*this, &ExportVideoDialog::abort_clicked));

	invid_path_entry.signal_changed().connect (sigc::mem_fun (*this, &ExportVideoDialog::set_original_file_information));
	width_spinner.signal_value_changed().connect (sigc::mem_fun (*this, &ExportVideoDialog::width_value_changed));
	height_spinner.signal_value_changed().connect (sigc::mem_fun (*this, &ExportVideoDialog::height_value_changed));

	cancel_button = add_button (Stock::CANCEL, RESPONSE_CANCEL);
	get_action_area()->pack_start (transcode_button, false, false);
	show_all_children ();
	progress_box->hide();
}

ExportVideoDialog::~ExportVideoDialog ()
{
	if (_transcoder) { delete _transcoder; _transcoder = 0;}
}

void
ExportVideoDialog::set_original_file_information()
{
	assert(_transcoder == 0);
	std::string infile = invid_path_entry.get_text();

	if (scale_checkbox.get_active()) {
		// user may have set custom values already, don't touch.
		return;
	}
	if (infile == "" || !Glib::file_test(infile, Glib::FILE_TEST_EXISTS)) {
		return;
	}

	_transcoder = new TranscodeFfmpeg(infile);
	if (_transcoder->probe_ok()) {
		_video_source_aspect_ratio = -1;
		width_spinner.set_value(_transcoder->get_width());
		height_spinner.set_value(_transcoder->get_height());
		_video_source_aspect_ratio = _transcoder->get_aspect();
	}

	delete _transcoder; _transcoder = 0;
}
void
ExportVideoDialog::apply_state (TimeSelection &tme, bool range)
{
	_suspend_dirty = true; // TODO really just queue 'dirty' and mark session dirty on "Export"

	export_range = tme;
	_video_source_aspect_ratio = -1;

	outfn_path_entry.set_text (_session->session_directory().export_path() + G_DIR_SEPARATOR +"export.avi");

	// TODO remember setting for export-range.. somehow, (let explicit range override)
	frameoffset_t av_offset = ARDOUR_UI::instance()->video_timeline->get_offset();
	if (av_offset < 0 ) {
		insnd_combo.append_text (_("from 00:00:00:00 to the video's end"));
	} else {
		insnd_combo.append_text (_("from the video's start to the video's end"));
	}
	if (!export_range.empty()) {
		insnd_combo.append_text (_("Selected range"));  // TODO show export_range.start() -> export_range.end_frame()
	}
	if (range) {
		insnd_combo.set_active(2);
	} else {
		insnd_combo.set_active(0);
	}

	preset_combo.set_active(0);
	audio_codec_combo.set_active(0);
	video_codec_combo.set_active(0);
	audio_bitrate_combo.set_active(0);
	audio_samplerate_combo.set_active(2);
	video_bitrate_combo.set_active(0);
	aspect_combo.set_active(1);

	scale_checkbox.set_active(false);
	scale_aspect.set_active(true);
	aspect_checkbox.set_active(false);
	normalize_checkbox.set_active(false);
	twopass_checkbox.set_active(false);
	optimizations_checkbox.set_active(false);
	deinterlace_checkbox.set_active(false);
	bframes_checkbox.set_active(false);
	fps_checkbox.set_active(false);
	meta_checkbox.set_active(false);

	float tcfps = _session->timecode_frames_per_second();

	LocaleGuard lg (X_("C"));

	XMLNode* node = _session->extra_xml (X_("Videotimeline"));
	bool filenameset = false;
	if (node) {
		if (node->property(X_("OriginalVideoFile"))) {
			std::string filename = node->property(X_("OriginalVideoFile"))->value();
			if (Glib::file_test(filename, Glib::FILE_TEST_EXISTS)) {
				invid_path_entry.set_text (filename);
				filenameset = true;
			}
		}
		if (!filenameset
				&& node->property(X_("Filename"))
				&& node->property(X_("LocalFile"))
				&& node->property(X_("LocalFile"))->value() == X_("1")
		   )
		{
			std::string filename = node->property(X_("Filename"))->value();
			if (filename.at(0) != G_DIR_SEPARATOR)
			{
				filename = Glib::build_filename (_session->session_directory().video_path(), filename);
			}
			if (Glib::file_test(filename, Glib::FILE_TEST_EXISTS))
			{
				invid_path_entry.set_text (filename);
				filenameset = true;
			}
		}
	}
	if (!filenameset) {
		invid_path_entry.set_text (X_(""));
	}

	node = _session->extra_xml (X_("Videoexport"));
	if (node) {
		const XMLProperty* prop;
		prop = node->property (X_("ChangeGeometry"));
		if (prop) { scale_checkbox.set_active(atoi(prop->value())?true:false); }
		prop = node->property (X_("KeepAspect"));
		if (prop) { scale_aspect.set_active(atoi(prop->value())?true:false); }
		prop = node->property (X_("ChangeAspect"));
		if (prop) { aspect_checkbox.set_active(atoi(prop->value())?true:false); }
		prop = node->property (X_("NormalizeAudio"));
		if (prop) { normalize_checkbox.set_active(atoi(prop->value())?true:false); }
		prop = node->property (X_("TwoPassEncode"));
		if (prop) { twopass_checkbox.set_active(atoi(prop->value())?true:false); }
		prop = node->property (X_("CodecOptimzations"));
		if (prop) { optimizations_checkbox.set_active(atoi(prop->value())?true:false); }
		prop = node->property (X_("Deinterlace"));
		if (prop) { deinterlace_checkbox.set_active(atoi(prop->value())?true:false); }
		prop = node->property (X_("BFrames"));
		if (prop) { bframes_checkbox.set_active(atoi(prop->value())?true:false); }
		prop = node->property (X_("ChangeFPS"));
		if (prop) { fps_checkbox.set_active(atoi(prop->value())?true:false); }
		prop = node->property (X_("Metadata"));
		if (prop) { meta_checkbox.set_active(atoi(prop->value())?true:false); }

		prop = node->property (X_("Format"));
		if (prop && !prop->value().empty()) { change_file_extension( "." + prop->value()); }

		_suspend_signals = true;
		prop = node->property (X_("Width"));
		if (prop) { width_spinner.set_value(atoi(prop->value())); }
		prop = node->property (X_("Height"));
		if (prop) { height_spinner.set_value(atoi(prop->value())); }
		_suspend_signals = false;

		prop = node->property (X_("FPS"));
		if (prop && fps_checkbox.get_active()) { tcfps = atof(prop->value()); }

		prop = node->property (X_("Preset"));
		if (prop) { preset_combo.set_active_text(prop->value()); }
		prop = node->property (X_("VCodec"));
		if (prop) { video_codec_combo.set_active_text(prop->value()); }
		prop = node->property (X_("ACodec"));
		if (prop) { audio_codec_combo.set_active_text(prop->value()); }
		prop = node->property (X_("VBitrate"));
		if (prop) { video_bitrate_combo.set_active_text(prop->value()); }
		prop = node->property (X_("ABitrate"));
		if (prop) { audio_bitrate_combo.set_active_text(prop->value()); }
		prop = node->property (X_("AspectRatio"));
		if (prop) { aspect_combo.set_active_text(prop->value()); }
		prop = node->property (X_("SampleRate"));
		if (prop) { audio_samplerate_combo.set_active_text(prop->value()); }
	}

	if      (fabs(tcfps - 23.976) < 0.01) { fps_combo.set_active(0); }
	else if (fabs(tcfps - 24.0  ) < 0.01) { fps_combo.set_active(1); }
	else if (fabs(tcfps - 24.976) < 0.01) { fps_combo.set_active(2); }
	else if (fabs(tcfps - 25.0  ) < 0.01) { fps_combo.set_active(3); }
	else if (fabs(tcfps - 29.97 ) < 0.01) { fps_combo.set_active(4); }
	else if (fabs(tcfps - 30.0  ) < 0.01) { fps_combo.set_active(5); }
	else if (fabs(tcfps - 59.94 ) < 0.01) { fps_combo.set_active(6); }
	else if (fabs(tcfps - 60.0  ) < 0.01) { fps_combo.set_active(7); }
	else { fps_combo.set_active(5); }

	set_original_file_information();

	/* update sensitivity */
	scale_checkbox_toggled();
	aspect_checkbox_toggled();
	fps_checkbox_toggled();
	video_codec_combo_changed();

	_suspend_dirty = false;

	show_all_children ();
	if (progress_box) {
		progress_box->hide();
	}
}

XMLNode&
ExportVideoDialog::get_state ()
{
	LocaleGuard lg (X_("C"));
	XMLNode* node = new XMLNode (X_("Videoexport"));
	node->add_property (X_("ChangeGeometry"), scale_checkbox.get_active() ? X_("1") : X_("0"));
	node->add_property (X_("KeepAspect"), scale_aspect.get_active() ? X_("1") : X_("0"));
	node->add_property (X_("ChangeAspect"), aspect_checkbox.get_active() ? X_("1") : X_("0"));
	node->add_property (X_("NormalizeAudio"), normalize_checkbox.get_active() ? X_("1") : X_("0"));
	node->add_property (X_("TwoPassEncode"), twopass_checkbox.get_active() ? X_("1") : X_("0"));
	node->add_property (X_("CodecOptimzations"), optimizations_checkbox.get_active() ? X_("1") : X_("0"));
	node->add_property (X_("Deinterlace"), deinterlace_checkbox.get_active() ? X_("1") : X_("0"));
	node->add_property (X_("BFrames"), bframes_checkbox.get_active() ? X_("1") : X_("0"));
	node->add_property (X_("ChangeFPS"), fps_checkbox.get_active() ? X_("1") : X_("0"));
	node->add_property (X_("Metadata"), meta_checkbox.get_active() ? X_("1") : X_("0"));

	node->add_property (X_("Format"), get_file_extension(outfn_path_entry.get_text()));

	node->add_property (X_("Width"), width_spinner.get_value());
	node->add_property (X_("Height"), height_spinner.get_value());

	node->add_property (X_("Preset"), preset_combo.get_active_text());
	node->add_property (X_("VCodec"), video_codec_combo.get_active_text());
	node->add_property (X_("ACodec"), audio_codec_combo.get_active_text());
	node->add_property (X_("VBitrate"), video_bitrate_combo.get_active_text());
	node->add_property (X_("ABitrate"), audio_bitrate_combo.get_active_text());
	node->add_property (X_("AspectRatio"), aspect_combo.get_active_text());
	node->add_property (X_("SampleRate"), audio_samplerate_combo.get_active_text());
	node->add_property (X_("FPS"), fps_combo.get_active_text());

	return *node;
}

void
ExportVideoDialog::set_state (const XMLNode &)
{
}

void
ExportVideoDialog::on_show ()
{
	Dialog::on_show ();
}

void
ExportVideoDialog::abort_clicked ()
{
	_aborted = true;
	if (_transcoder) {
		_transcoder->cancel();
	}
}

void
ExportVideoDialog::update_progress (framecnt_t c, framecnt_t a)
{
	if (a == 0 || c > a) {
		pbar.set_pulse_step(.1);
		pbar.pulse();
	} else {
		double progress = (double)c / (double) a;
		progress = progress / ((_twopass ? 2.0 : 1.0) + (_normalize ? 2.0 : 1.0));
		if (_normalize && _twopass) progress += (_firstpass ? .5 : .75);
		else if (_normalize) progress += 2.0/3.0;
		else if (_twopass) progress += (_firstpass ? 1.0/3.0 : 2.0/3.0);
		else progress += .5;

		pbar.set_fraction (progress);
	}
}


gint
ExportVideoDialog::audio_progress_display ()
{
	std::string status_text;
	double progress = 0.0;
		if (status->normalizing) {
			pbar.set_text (_("Normalizing audio"));
			progress = ((float) status->current_normalize_cycle) / status->total_normalize_cycles;
			progress = progress / (_twopass ? 4.0 : 3.0) + (_twopass ? .25 : 1.0/3.0);
		} else {
			pbar.set_text (_("Exporting audio"));
			progress = ((float) status->processed_frames_current_timespan) / status->total_frames_current_timespan;
			progress = progress / ((_twopass ? 2.0 : 1.0) + (_normalize ? 2.0 : 1.0));
		}
		if (progress < _previous_progress) {
			// Work around gtk bug
			pbar.hide();
			pbar.show();
		}
		_previous_progress = progress;
		pbar.set_fraction (progress);
	return TRUE;
}

void
ExportVideoDialog::finished ()
{
	if (_aborted) {
		::g_unlink(outfn_path_entry.get_text().c_str());
		::g_unlink (_insnd.c_str());
		delete _transcoder; _transcoder = 0;
		Gtk::Dialog::response(RESPONSE_CANCEL);
	} else if (_twopass && _firstpass) {
		_firstpass = false;
		if (_transcoder) { delete _transcoder; _transcoder = 0;}
		encode_pass(2);
	} else {
		if (twopass_checkbox.get_active()) {
			std::string outfn = outfn_path_entry.get_text();
			std::string p2log = Glib::path_get_dirname (outfn) + G_DIR_SEPARATOR + "ffmpeg2pass";
			::g_unlink (p2log.c_str());
		}
		::g_unlink (_insnd.c_str());
		delete _transcoder; _transcoder = 0;
		Gtk::Dialog::response(RESPONSE_ACCEPT);
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
	_session->add_extra_xml (get_state());

	std::string outfn = outfn_path_entry.get_text();
	if (!confirm_video_outfn(outfn)) { return; }

	vbox->hide();
	cancel_button->hide();
	transcode_button.hide();
	pbar.set_size_request(300,-1);
	pbar.set_text(_("Exporting Audio..."));
	progress_box->show();
	_aborted = false;
	_twopass = twopass_checkbox.get_active();
	_firstpass = true;
	_normalize = normalize_checkbox.get_active();

	/* export audio track */
	ExportTimespanPtr tsp = _session->get_export_handler()->add_timespan();
	boost::shared_ptr<ExportChannelConfiguration> ccp = _session->get_export_handler()->add_channel_config();
	boost::shared_ptr<ARDOUR::ExportFilename> fnp = _session->get_export_handler()->add_filename();
	boost::shared_ptr<AudioGrapher::BroadcastInfo> b;
	XMLTree tree;
	std::string vtl_samplerate = audio_samplerate_combo.get_active_text();
	std::string vtl_normalize = _normalize ? "true" : "false";
	tree.read_buffer(std::string(
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
"<ExportFormatSpecification name=\"VTL-WAV-16\" id=\"3094591e-ccb9-4385-a93f-c9955ffeb1f0\">"
"  <Encoding id=\"F_WAV\" type=\"T_Sndfile\" extension=\"wav\" name=\"WAV\" has-sample-format=\"true\" channel-limit=\"256\"/>"
"  <SampleRate rate=\""+ vtl_samplerate +"\"/>"
"  <SRCQuality quality=\"SRC_SincBest\"/>"
"  <EncodingOptions>"
"    <Option name=\"sample-format\" value=\"SF_16\"/>"
"    <Option name=\"dithering\" value=\"D_None\"/>"
"    <Option name=\"tag-metadata\" value=\"true\"/>"
"    <Option name=\"tag-support\" value=\"false\"/>"
"    <Option name=\"broadcast-info\" value=\"false\"/>"
"  </EncodingOptions>"
"  <Processing>"
"    <Normalize enabled=\""+ vtl_normalize +"\" target=\"0\"/>"
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
"</ExportFormatSpecification>"
));
	boost::shared_ptr<ExportFormatSpecification> fmp = _session->get_export_handler()->add_format(*tree.root());

	/* set up range */
	framepos_t start, end;
	start = end = 0;
	if (insnd_combo.get_active_row_number() == 1) {
		_transcoder = new TranscodeFfmpeg(invid_path_entry.get_text());
		if (_transcoder->probe_ok() && _transcoder->get_fps() > 0) {
			end = _transcoder->get_duration() * _session->nominal_frame_rate() / _transcoder->get_fps();
		} else {
			warning << _("Export Video: Cannot query duration of video-file, using duration from timeline instead.") << endmsg;
			end = ARDOUR_UI::instance()->video_timeline->get_duration();
		}
		if (_transcoder) {delete _transcoder; _transcoder = 0;}

		frameoffset_t av_offset = ARDOUR_UI::instance()->video_timeline->get_offset();
#if 0 /* DEBUG */
		printf("audio-range -- AV offset: %lld\n", av_offset);
#endif
		if (av_offset > 0) {
			start = av_offset;
		}
		end += av_offset;
	}
	else if (insnd_combo.get_active_row_number() == 2) {
		start = ARDOUR_UI::instance()->video_timeline->quantify_frames_to_apv(export_range.start());
		end   = ARDOUR_UI::instance()->video_timeline->quantify_frames_to_apv(export_range.end_frame());
	}
	if (end <= 0) {
		start = _session->current_start_frame();
		end   = _session->current_end_frame();
	}
#if 0 /* DEBUG */
	printf("audio export-range %lld -> %lld\n", start, end);
#endif

	const frameoffset_t vstart = ARDOUR_UI::instance()->video_timeline->get_offset();
	const frameoffset_t vend   = vstart + ARDOUR_UI::instance()->video_timeline->get_duration();

	if ( (start >= end) || (end < vstart) || (start > vend)) {
		warning << _("Export Video: export-range does not include video.") << endmsg;
		delete _transcoder; _transcoder = 0;
		Gtk::Dialog::response(RESPONSE_CANCEL);
		return;
	}

	tsp->set_range (start, end);
	tsp->set_name ("mysession");
	tsp->set_range_id ("session");

	/* add master outs as default */
	IO* master_out = _session->master_out()->output().get();
	if (!master_out) {
		warning << _("Export Video: No Master Out Ports to Connect for Audio Export") << endmsg;
		delete _transcoder; _transcoder = 0;
		Gtk::Dialog::response(RESPONSE_CANCEL);
		return;
	}
	for (uint32_t n = 0; n < master_out->n_ports().n_audio(); ++n) {
		PortExportChannel * channel = new PortExportChannel ();
		channel->add_port (master_out->audio (n));
		ExportChannelPtr chan_ptr (channel);
		ccp->register_channel (chan_ptr);
	}

	/* outfile */
	fnp->set_timespan(tsp);
	fnp->set_label("vtl");
	fnp->include_label = true;
	_insnd = fnp->get_path(fmp);

	/* do sound export */
	fmp->set_soundcloud_upload(false);
	_session->get_export_handler()->add_export_config (tsp, ccp, fmp, fnp, b);
	_session->get_export_handler()->do_export();
	status = _session->get_export_status ();

	audio_progress_connection = Glib::signal_timeout().connect (sigc::mem_fun(*this, &ExportVideoDialog::audio_progress_display), 100);
	_previous_progress = 0.0;
	while (status->running) {
		if (_aborted) { status->abort(); }
		if (gtk_events_pending()) {
			gtk_main_iteration ();
		} else {
			Glib::usleep (10000);
		}
	}
	audio_progress_connection.disconnect();
	status->finish ();
	if (status->aborted()) {
		::g_unlink (_insnd.c_str());
		delete _transcoder; _transcoder = 0;
		Gtk::Dialog::response(RESPONSE_CANCEL);
		return;
	}
	pbar.set_text (_("Encoding Video..."));
	encode_pass(1);
}

void
ExportVideoDialog::encode_pass (int pass)
{
	std::string outfn = outfn_path_entry.get_text();
	std::string invid = invid_path_entry.get_text();

	_transcoder = new TranscodeFfmpeg(invid);
	if (!_transcoder->ffexec_ok()) {
		/* ffmpeg binary was not found. TranscodeFfmpeg prints a warning */
		::g_unlink (_insnd.c_str());
		delete _transcoder; _transcoder = 0;
		Gtk::Dialog::response(RESPONSE_CANCEL);
		return;
	}
	if (!_transcoder->probe_ok()) {
		/* video input file can not be read */
		warning << _("Export Video: Video input file cannot be read.") << endmsg;
		::g_unlink (_insnd.c_str());
		delete _transcoder; _transcoder = 0;
		Gtk::Dialog::response(RESPONSE_CANCEL);
		return;
	}

	std::string preset = preset_combo.get_active_text();
	TranscodeFfmpeg::FFSettings ffs ; /* = transcoder->default_encoder_settings(); */
	ffs.clear();

	if (fps_checkbox.get_active()) {
		ffs["-r"] = fps_combo.get_active_text();
		_transcoder->set_fps(atof(fps_combo.get_active_text()));
	}

	if (scale_checkbox.get_active()) {
		ffs["-s"] = string_compose("%1x%2", width_spinner.get_value(), height_spinner.get_value());
	}

	if (video_codec_combo.get_active_text() != _("(default for format)")) {
		ffs["-vcodec"] = video_codec_combo.get_active_text();
	}
	if (audio_codec_combo.get_active_text() != _("(default for format)")) {
		ffs["-acodec"] = audio_codec_combo.get_active_text();
	}

	if (video_bitrate_combo.get_active_text() == _("(default)") ) {
		;
	}
	else if (video_bitrate_combo.get_active_text() == _("(retain)") ) {
		ffs["-qscale"]  = "0";
	} else {
		ffs["-b:v"]  = video_bitrate_combo.get_active_text();
	}

	if (audio_bitrate_combo.get_active_text() != _("(default)") ) {
		ffs["-b:a"] = audio_bitrate_combo.get_active_text();
	}

	if (audio_codec_combo.get_active_text() == "aac" ) {
		ffs["-strict"] = "-2";
	}

	if (video_codec_combo.get_active_text() == "h264" ) {
		ffs["-vcodec"] = "libx264";
	}
	else if (video_codec_combo.get_active_text() == "vpx (webm)" ) {
		ffs["-vcodec"] = "libvpx";
		ffs["-g"] = "120";
		ffs["-qmin"] = "11";
		ffs["-qmax"] = "51";
	}

	if (optimizations_checkbox.get_active()) {
	  if (video_codec_combo.get_active_text() == "mpeg2video") {
			ffs["-mbd"] = "rd";
			ffs["-trellis"] = "2";
			ffs["-cmp"] = "2";
			ffs["-subcmp"] = "2";
		}
		else if (video_codec_combo.get_active_text() == "mpeg4") {
			ffs["-mbd"] = "rd";
			ffs["-flags"] = "+mv4+aic";
			ffs["-trellis"] = "2";
			ffs["-cmp"] = "2";
			ffs["-subcmp"] = "2";
			ffs["-g"] = "300";
		}
		else if (video_codec_combo.get_active_text() == "flv") {
			ffs["-mbd"] = "2";
			ffs["-cmp"] = "2";
			ffs["-subcmp"] = "2";
			ffs["-trellis"] = "2";
			ffs["-flags"] = "+aic+mv0+mv4";
			ffs["-g"] = "160";
		}
	}

	if (bframes_checkbox.get_active() && (
		   video_codec_combo.get_active_text() == "mpeg2video"
		|| video_codec_combo.get_active_text() == "mpeg4"
		)) {
		ffs["-bf"] = "2";
	}

	if (preset == "dvd-PAL") {
		ffs.clear(); /* ignore all prev settings */
		ffs["-target"] = "pal-dvd";
		ffs["-aspect"] = "4:3"; /* required for DVD - may be overridden below */
	}
	else if (preset == "dvd-NTSC") {
		ffs.clear(); /* ignore all prev settings */
		ffs["-target"] = "ntsc-dvd";
		ffs["-aspect"] = "4:3"; /* required for DVD - may be overridden below */
	}

	if (aspect_checkbox.get_active()) {
		ffs["-aspect"] = aspect_combo.get_active_text();
	}
	if (deinterlace_checkbox.get_active()) {
		ffs["-deinterlace"] = "-y"; // we use '-y' as dummy parameter for non key/value options
	}

	bool map = true;
	if (pass == 1 && _twopass) {
		pbar.set_text (_("Encoding Video.. Pass 1/2"));
		map = false;
		ffs["-pass"] = "1";
		ffs["-an"] = "-y";
		ffs["-passlogfile"] =  Glib::path_get_dirname (outfn) + G_DIR_SEPARATOR + "ffmpeg2pass";
		ffs["-f"] = get_file_extension(invid).empty()?"mov":get_file_extension(invid);
#ifdef PLATFORM_WINDOWS
		outfn = "NUL";
#else
		outfn = "/dev/null";
#endif
	} else if (pass == 2) {
		pbar.set_text (_("Encoding Video.. Pass 2/2"));
		ffs["-pass"] = "2";
		ffs["-passlogfile"] =  Glib::path_get_dirname (outfn) + G_DIR_SEPARATOR + "ffmpeg2pass";
	}

	frameoffset_t av_offset = ARDOUR_UI::instance()->video_timeline->get_offset();
	double duration_s  = 0;

	if (insnd_combo.get_active_row_number() == 0) {
		/* session start to session end */
		framecnt_t duration_f = _session->current_end_frame() - _session->current_start_frame();
		duration_s = (double)duration_f / (double)_session->nominal_frame_rate();
	} else if (insnd_combo.get_active_row_number() == 2) {
		/* selected range */
		duration_s = export_range.length() / (double)_session->nominal_frame_rate();
	} else {
		/* video start to end */
		framecnt_t duration_f = ARDOUR_UI::instance()->video_timeline->get_duration();
		if (av_offset < 0 ) {
			duration_f += av_offset;
		}
		duration_s = (double)duration_f / (double)_session->nominal_frame_rate();
	}

	std::ostringstream osstream; osstream << duration_s;
	ffs["-t"] = osstream.str();
	_transcoder->set_duration(duration_s * _transcoder->get_fps());

	if (insnd_combo.get_active_row_number() == 0 || insnd_combo.get_active_row_number() == 2) {
		framepos_t start, snend;
		const frameoffset_t vid_duration = ARDOUR_UI::instance()->video_timeline->get_duration();
		if (insnd_combo.get_active_row_number() == 0) {
			start = _session->current_start_frame();
			snend = _session->current_end_frame();
		} else {
			start = export_range.start();
			snend = export_range.end_frame();
		}

#if 0 /* DEBUG */
		printf("AV offset: %lld Vid-len: %lld Vid-end: %lld || start:%lld || end:%lld\n",
				av_offset, vid_duration, av_offset+vid_duration, start, snend); // XXX
#endif

		if (av_offset > start && av_offset + vid_duration < snend) {
			_transcoder->set_leadinout((av_offset - start) / (double)_session->nominal_frame_rate(),
				(snend - (av_offset + vid_duration)) / (double)_session->nominal_frame_rate());
		} else if (av_offset > start) {
			_transcoder->set_leadinout((av_offset - start) / (double)_session->nominal_frame_rate(), 0);
		} else if (av_offset + vid_duration < snend) {
			_transcoder->set_leadinout(0, (snend - (av_offset + vid_duration)) / (double)_session->nominal_frame_rate());
			_transcoder->set_avoffset((av_offset - start) / (double)_session->nominal_frame_rate());
		}
#if 0
		else if (start > av_offset) {
			std::ostringstream osstream; osstream << ((start - av_offset) / (double)_session->nominal_frame_rate());
			ffs["-ss"] = osstream.str();
		}
#endif
		else {
			_transcoder->set_avoffset((av_offset - start) / (double)_session->nominal_frame_rate());
		}

	} else if (av_offset < 0) {
		/* from 00:00:00:00 to video-end */
		_transcoder->set_avoffset(av_offset / (double)_session->nominal_frame_rate());
	}

	TranscodeFfmpeg::FFSettings meta = _transcoder->default_meta_data();
	if (meta_checkbox.get_active()) {
		ARDOUR::SessionMetadata * session_data = ARDOUR::SessionMetadata::Metadata();
		if (session_data->year() > 0 ) {
			std::ostringstream osstream; osstream << session_data->year();
			meta["year"] = osstream.str();
		}
		if (session_data->track_number() > 0 ) {
			std::ostringstream osstream; osstream << session_data->track_number();
			meta["track"] = osstream.str();
		}
		if (session_data->disc_number() > 0 ) {
			std::ostringstream osstream; osstream << session_data->disc_number();
			meta["disc"] = osstream.str();
		}
		if (!session_data->title().empty())     {meta["title"] = session_data->title();}
		if (!session_data->artist().empty())    {meta["author"] = session_data->artist();}
		if (!session_data->album_artist().empty()) {meta["album_artist"] = session_data->album_artist();}
		if (!session_data->album().empty())     {meta["album"] = session_data->album();}
		if (!session_data->genre().empty())     {meta["genre"] = session_data->genre();}
		if (!session_data->composer().empty())  {meta["composer"] = session_data->composer();}
		if (!session_data->comment().empty())   {meta["comment"] = session_data->comment();}
		if (!session_data->copyright().empty()) {meta["copyright"] = session_data->copyright();}
		if (!session_data->subtitle().empty())  {meta["description"] = session_data->subtitle();}
	}

#if 1 /* tentative debug mode */
	if (debug_checkbox.get_active()) {
		_transcoder->set_debug(true);
	}
#endif

	_transcoder->Progress.connect(*this, invalidator (*this), boost::bind (&ExportVideoDialog::update_progress , this, _1, _2), gui_context());
	_transcoder->Finished.connect(*this, invalidator (*this), boost::bind (&ExportVideoDialog::finished, this), gui_context());
	if (!_transcoder->encode(outfn, _insnd, invid, ffs, meta, map)) {
		ARDOUR_UI::instance()->popup_error(_("Transcoding failed."));
		delete _transcoder; _transcoder = 0;
		Gtk::Dialog::response(RESPONSE_CANCEL);
		return;
	}
}

void
ExportVideoDialog::change_file_extension (std::string ext)
{
	if (ext == "") return;
	outfn_path_entry.set_text (
		strip_file_extension(outfn_path_entry.get_text()) + ext
	);
}

void
ExportVideoDialog::width_value_changed ()
{
	if (_suspend_signals) {
		return;
	}
	if (_session && !_suspend_dirty) _session->set_dirty ();
	if (!scale_checkbox.get_active() || !scale_aspect.get_active()) {
		return;
	}
	if (_video_source_aspect_ratio <= 0) {
		return;
	}
	_suspend_signals = true;
	height_spinner.set_value(rintf(width_spinner.get_value() / _video_source_aspect_ratio));
	_suspend_signals = false;
}

void
ExportVideoDialog::height_value_changed ()
{
	if (_suspend_signals) {
		return;
	}
	if (_session && !_suspend_dirty) _session->set_dirty ();
	if (!scale_checkbox.get_active() || !scale_aspect.get_active()) {
		return;
	}
	if (_video_source_aspect_ratio <= 0) {
		return;
	}
	_suspend_signals = true;
	width_spinner.set_value(rintf(height_spinner.get_value() * _video_source_aspect_ratio));
	_suspend_signals = false;
}

void
ExportVideoDialog::scale_checkbox_toggled ()
{
	scale_aspect.set_sensitive(scale_checkbox.get_active());
	width_spinner.set_sensitive(scale_checkbox.get_active());
	height_spinner.set_sensitive(scale_checkbox.get_active());
	if (_session && !_suspend_dirty) _session->set_dirty ();
}

void
ExportVideoDialog::fps_checkbox_toggled ()
{
	fps_combo.set_sensitive(fps_checkbox.get_active());
	if (_session && !_suspend_dirty) _session->set_dirty ();
}

void
ExportVideoDialog::aspect_checkbox_toggled ()
{
	aspect_combo.set_sensitive(aspect_checkbox.get_active());
	if (_session && !_suspend_dirty) _session->set_dirty ();
}

void
ExportVideoDialog::video_codec_combo_changed ()
{
	if ((  video_codec_combo.get_active_text() == "mpeg4"
	     ||video_codec_combo.get_active_text() == "mpeg2video"
			) && !(
	       preset_combo.get_active_text() == "dvd-PAL"
	     ||preset_combo.get_active_text() == "dvd-NTSC"
	   )) {
		bframes_checkbox.set_sensitive(true);
		optimizations_checkbox.set_sensitive(true);
		if (video_codec_combo.get_active_text() == "mpeg2video") {
			optimizations_label.set_text("-mbd rd -trellis 2 -cmp 2 -subcmp 2"); // mpeg2
		} else if (video_codec_combo.get_active_text() == "mpeg4") {
			optimizations_label.set_text("-mbd rd -flags +mv4+aic -trellis 2 -cmp 2 -subcmp 2 -g 300"); // mpeg4
		} else {
			optimizations_label.set_text("-mbd 2 -cmp 2 -subcmp 2 -trellis 2 -flags +aic+mv0+mv4 -g 160"); // flv
		}
	} else {
		bframes_checkbox.set_sensitive(false);
		bframes_checkbox.set_active(false);
		optimizations_checkbox.set_sensitive(false);
		optimizations_checkbox.set_active(false);
		optimizations_label.set_text("-");
	}
	if (_session && !_suspend_dirty) _session->set_dirty ();
}

void
ExportVideoDialog::preset_combo_changed ()
{
	std::string p = preset_combo.get_active_text();
	scale_checkbox.set_sensitive(true);

	if (p == "flv") {
		change_file_extension(".flv");
		audio_codec_combo.set_active(2);
		video_codec_combo.set_active(1);
		audio_bitrate_combo.set_active(2);
		video_bitrate_combo.set_active(3);
		audio_samplerate_combo.set_active(1);
	}
	else if (p == "you-tube") {
		change_file_extension(".avi");
		audio_codec_combo.set_active(3);
		video_codec_combo.set_active(6);
		audio_bitrate_combo.set_active(2);
		video_bitrate_combo.set_active(4);
		if (_session->nominal_frame_rate() == 48000 || _session->nominal_frame_rate() == 96000) {
			audio_samplerate_combo.set_active(2);
		} else {
			audio_samplerate_combo.set_active(1);
		}
	}
	else if (p == "ogg") {
		change_file_extension(".ogv");
		audio_codec_combo.set_active(4);
		video_codec_combo.set_active(2);
		audio_bitrate_combo.set_active(3);
		video_bitrate_combo.set_active(4);
		if (_session->nominal_frame_rate() == 48000 || _session->nominal_frame_rate() == 96000) {
			audio_samplerate_combo.set_active(2);
		} else {
			audio_samplerate_combo.set_active(1);
		}
	}
	else if (p == "webm") {
		change_file_extension(".webm");
		audio_codec_combo.set_active(4);
		video_codec_combo.set_active(7);
		audio_bitrate_combo.set_active(3);
		video_bitrate_combo.set_active(4);
		if (_session->nominal_frame_rate() == 48000 || _session->nominal_frame_rate() == 96000) {
			audio_samplerate_combo.set_active(2);
		} else {
			audio_samplerate_combo.set_active(1);
		}
	}
	else if (p == "dvd-mp2") {
		change_file_extension(".mpg");
		audio_codec_combo.set_active(5);
		video_codec_combo.set_active(4);
		audio_bitrate_combo.set_active(4);
		video_bitrate_combo.set_active(5);
		audio_samplerate_combo.set_active(2);
	}
	else if (p == "dvd-NTSC" || p == "dvd-PAL") {
		change_file_extension(".mpg");
		audio_codec_combo.set_active(6);
		video_codec_combo.set_active(4);
		audio_bitrate_combo.set_active(4);
		video_bitrate_combo.set_active(5);
		audio_samplerate_combo.set_active(2);

		scale_checkbox.set_active(false);
		scale_checkbox.set_sensitive(false);
	}
	else if (p == "mpeg4") {
		change_file_extension(".mp4");
		audio_codec_combo.set_active(1);
		video_codec_combo.set_active(5);
		audio_bitrate_combo.set_active(4);
		video_bitrate_combo.set_active(5);
		if (_session->nominal_frame_rate() == 48000 || _session->nominal_frame_rate() == 96000) {
			audio_samplerate_combo.set_active(2);
		} else {
			audio_samplerate_combo.set_active(1);
		}
	}
	else if (p == "mp4/h264/aac") {
		change_file_extension(".mp4");
		audio_codec_combo.set_active(2);
		video_codec_combo.set_active(6);
		audio_bitrate_combo.set_active(0);
		video_bitrate_combo.set_active(0);
		if (_session->nominal_frame_rate() == 48000 || _session->nominal_frame_rate() == 96000) {
			audio_samplerate_combo.set_active(2);
		} else {
			audio_samplerate_combo.set_active(1);
		}
	}

	if (p == "none") {
		audio_codec_combo.set_sensitive(true);
		video_codec_combo.set_sensitive(true);
		audio_bitrate_combo.set_sensitive(true);
		video_bitrate_combo.set_sensitive(true);
		audio_samplerate_combo.set_sensitive(true);
	} else {
		audio_codec_combo.set_sensitive(false);
		video_codec_combo.set_sensitive(false);
		audio_bitrate_combo.set_sensitive(false);
		video_bitrate_combo.set_sensitive(false);
		audio_samplerate_combo.set_sensitive(false);
	}

	Gtk::Table *t = (Gtk::Table*) preset_combo.get_parent();
	Gtk::Table_Helpers::TableList c = t->children();
	Gtk::Table_Helpers::TableList::iterator it;
	if (p == "dvd-PAL" || p == "dvd-NTSC") {
		for (it = c.begin(); it != c.end(); ++it) {
			int row = it->get_top_attach();
			if (row == 2 || row == 3 || row== 5 || row== 6 || row == 9) {
				it->get_widget()->hide();
			}
		}
	} else {
		for (it = c.begin(); it != c.end(); ++it) {
			int row = it->get_top_attach();
			if (row == 2 || row == 3 || row== 5 || row== 6 || row == 9) {
				it->get_widget()->show();
			}
		}
	}

	video_codec_combo_changed();
}

void
ExportVideoDialog::open_outfn_dialog ()
{
	Gtk::FileChooserDialog dialog(_("Save Exported Video File"), Gtk::FILE_CHOOSER_ACTION_SAVE);
	dialog.set_filename (outfn_path_entry.get_text());

	dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);

	int result = dialog.run();

	if (result == Gtk::RESPONSE_OK) {
		std::string filename = dialog.get_filename();

		if (filename.length()) {
			outfn_path_entry.set_text (filename);
		}
	}
}

void
ExportVideoDialog::open_invid_dialog ()
{
	Gtk::FileChooserDialog dialog(_("Save Exported Video File"), Gtk::FILE_CHOOSER_ACTION_SAVE);
	dialog.set_filename (invid_path_entry.get_text());

	dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);

	int result = dialog.run();

	if (result == Gtk::RESPONSE_OK) {
		std::string filename = dialog.get_filename();

		if (filename.length()) {
			invid_path_entry.set_text (filename);
		}
	}
}
