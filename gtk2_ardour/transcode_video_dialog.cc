/*
    Copyright (C) 2010,2013 Paul Davis
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
#include <libgen.h>

#include <glib/gstdio.h>

#include "pbd/error.h"
#include "pbd/convert.h"
#include "gtkmm2ext/utils.h"
#include "ardour/session_directory.h"
#include "ardour/profile.h"
#include "ardour/template_utils.h"
#include "ardour/session.h"
#include "ardour_ui.h"
#include "gui_thread.h"

#include "utils.h"
#include "opts.h"
#include "transcode_video_dialog.h"
#include "utils_videotl.h"
#include "i18n.h"

using namespace Gtk;
using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace VideoUtils;

TranscodeVideoDialog::TranscodeVideoDialog (Session* s, std::string infile)
	: ArdourDialog (_("Transcode/Import Video File "))
	, infn (infile)
	, path_label (_("Output File:"), Gtk::ALIGN_LEFT)
	, browse_button (_("Browse"))
	, transcode_button (_("OK"))
	, abort_button (_("Abort"))
	, progress_label ()
	, aspect_checkbox (_("Height = "))
	, height_adjustment (128, 0, 1920, 1, 16, 0)
	, height_spinner (height_adjustment)
	, bitrate_checkbox (_("Manual Override"))
	, bitrate_adjustment (2000, 500, 10000, 10, 100, 0)
	, bitrate_spinner (bitrate_adjustment)
#if 1 /* tentative debug mode */
	, debug_checkbox (_("Debug Mode: Print ffmpeg command and output to stdout."))
#endif
{
	set_session (s);

	transcoder = new TranscodeFfmpeg(infile);
	audiofile = "";
	pending_audio_extract = false;
	aborted = false;

	set_name ("TranscodeVideoDialog");
	set_modal (true);
	set_skip_taskbar_hint (true);
	set_resizable (false);

	Gtk::Label* l;
	vbox = manage (new VBox);
	VBox* options_box = manage (new VBox);
	HBox* path_hbox = manage (new HBox);

	int w = 0, h = 0;
	m_aspect = 4.0/3.0;
	TranscodeFfmpeg::FFAudioStreams as; as.clear();

	path_hbox->pack_start (path_label, false, false, 3);
	path_hbox->pack_start (path_entry, true, true, 3);
	path_hbox->pack_start (browse_button, false, false, 3);
	browse_button.set_name ("PaddedButton");

	path_entry.set_width_chars(38);
	height_spinner.set_sensitive(false);
	bitrate_spinner.set_sensitive(false);

	std::string dstdir = video_dest_dir(_session->session_directory().video_path(), video_get_docroot(Config));
	std::string dstfn  = video_dest_file(dstdir, infile);
	path_entry.set_text (dstfn);

	l = manage (new Label (_("<b>File Information</b>"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	l->set_use_markup ();
	options_box->pack_start (*l, false, true, 4);

	bool ffok = false;
	if (!transcoder->ffexec_ok()) {
		l = manage (new Label (_("No ffprobe or ffmpeg executables could be found on this system. Video Import is not possible until you install those tools. See the Log window for more information."), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
		l->set_line_wrap();
		options_box->pack_start (*l, false, true, 4);
		aspect_checkbox.set_sensitive(false);
		bitrate_checkbox.set_sensitive(false);
	}
	else if (!transcoder->probe_ok()) {
		l = manage (new Label (string_compose(_("File-info can not be read. Most likely '%1' is not a valid video-file or an unsupported video codec or format."), infn), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
		options_box->pack_start (*l, false, true, 4);
		aspect_checkbox.set_sensitive(false);
		bitrate_checkbox.set_sensitive(false);
	} else {
		ffok = true;
		w = transcoder->get_width();
		h = transcoder->get_height();
		as = transcoder->get_audio();
		m_aspect = transcoder->get_aspect();

		Table* t = manage (new Table (4, 2));
		t->set_spacings (4);
		options_box->pack_start (*t, true, true, 4);
		l = manage (new Label (_("FPS:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
		t->attach (*l, 0, 1, 0, 1);
		l = manage (new Label (_("Duration:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
		t->attach (*l, 2, 3, 0, 1);
		l = manage (new Label (_("Codec:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
		t->attach (*l, 0, 1, 1, 2);
		l = manage (new Label (_("Geometry:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
		t->attach (*l, 2, 3, 1, 2);

		std::ostringstream osstream;
		osstream << transcoder->get_fps();
		l = manage (new Label (osstream.str(), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
		t->attach (*l, 1, 2, 0, 1);

		osstream.str("");
		osstream << w << "x" << h;
		l = manage (new Label (osstream.str(), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
		t->attach (*l, 3, 4, 1, 2);

		osstream.str("");
		if (transcoder->get_duration() == 0 || transcoder->get_fps() == 0) {
			osstream << _("??");
		} else {
			unsigned long sec = transcoder->get_duration() / transcoder->get_fps();
			osstream << setfill('0') << setw(2);
			osstream << (sec / 3600) << ":";
			osstream << setfill('0') << setw(2);
			osstream << ((sec /60 )%60) << ":";
			osstream << setfill('0') << setw(2);
			osstream << (sec%60)  << ":";
			osstream << setfill('0') << setw(2);
			osstream << (transcoder->get_duration() % (int) floor(transcoder->get_fps()));
		}
		l = manage (new Label (osstream.str(), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
		t->attach (*l, 3, 4, 0, 1);

		osstream.str("");
		osstream << transcoder->get_codec();
		l = manage (new Label (osstream.str(), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
		t->attach (*l, 1, 2, 1, 2);
	}

	l = manage (new Label (_("<b>Import Settings</b>"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	l->set_use_markup ();
	options_box->pack_start (*l, false, true, 4);

	video_combo.set_name ("PaddedButton");
	video_combo.append_text(_("Do Not Import Video"));
	video_combo.append_text(_("Reference From Current Location"));
	if (ffok)  {
		video_combo.append_text(_("Import/Transcode Video to Session"));
		video_combo.set_active(2);
	} else {
		video_combo.set_active(1);
		video_combo.set_sensitive(false);
		audio_combo.set_sensitive(false);
	}

	options_box->pack_start (video_combo, false, false, 4);

	Table* t = manage (new Table (4, 3));
	t->set_spacings (4);
	options_box->pack_start (*t, true, true, 4);

	l = manage (new Label (_("Scale Video: Width = "), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	t->attach (*l, 0, 1, 0, 1);
	scale_combo.set_name ("PaddedButton");
	t->attach (scale_combo, 1, 2, 0, 1);
	t->attach (aspect_checkbox, 2, 3, 0, 1);
	t->attach (height_spinner, 3, 4, 0, 1);

	scale_combo.append_text(_("Original Width"));
	if (w > 1920) { scale_combo.append_text("1920 (hd1080)"); }
	if (w > 1408) { scale_combo.append_text("1408 (16cif)"); }
	if (w > 1280) { scale_combo.append_text("1280 (sxga, hd720)"); }
	if (w > 1024) { scale_combo.append_text("1024 (xga)"); }
	if (w > 852)  { scale_combo.append_text(" 852 (hd480)"); }
	if (w > 768)  { scale_combo.append_text(" 768 (PAL)"); }
	if (w > 720)  { scale_combo.append_text(" 720 (PAL)"); }
	if (w > 640)  { scale_combo.append_text(" 640 (vga, ega)"); }
	if (w > 352)  { scale_combo.append_text(" 352 (cif)"); }
	if (w > 320)  { scale_combo.append_text(" 320 (cga, qvga)"); }
	if (w > 176)  { scale_combo.append_text(" 176 (qcif)"); }
	scale_combo.set_active(0);
	height_spinner.set_value(h);

	l = manage (new Label (_("Bitrate (KBit/s):"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	t->attach (*l, 0, 1, 1, 2);
	t->attach (bitrate_checkbox, 2, 3, 1, 2);
	t->attach (bitrate_spinner, 3, 4, 1, 2);

	l = manage (new Label (_("Extract Audio:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	t->attach (*l, 0, 1, 2, 3);
	audio_combo.set_name ("PaddedButton");
	t->attach (audio_combo, 1, 4, 2, 3);
	audio_combo.append_text("No audio");
	if (as.size() > 0) {
		for (TranscodeFfmpeg::FFAudioStreams::iterator it = as.begin(); it < as.end(); ++it) {
			audio_combo.append_text((*it).name);
		}
	}
	audio_combo.set_active(0);

#if 1 /* tentative debug mode */
	options_box->pack_start (debug_checkbox, false, true, 4);
#endif

	vbox->pack_start (*path_hbox, false, false);
	vbox->pack_start (*options_box, false, true);

	get_vbox()->set_spacing (4);
	get_vbox()->pack_start (*vbox, false, false);

	progress_box = manage (new VBox);
	progress_box->pack_start (progress_label, false, false);
	progress_box->pack_start (pbar, false, false);
	progress_box->pack_start (abort_button, false, false);
	get_vbox()->pack_start (*progress_box, false, false);

	browse_button.signal_clicked().connect (sigc::mem_fun (*this, &TranscodeVideoDialog::open_browse_dialog));
	transcode_button.signal_clicked().connect (sigc::mem_fun (*this, &TranscodeVideoDialog::launch_transcode));
	abort_button.signal_clicked().connect (sigc::mem_fun (*this, &TranscodeVideoDialog::abort_clicked));

	video_combo.signal_changed().connect (sigc::mem_fun (*this, &TranscodeVideoDialog::video_combo_changed));
	audio_combo.signal_changed().connect (sigc::mem_fun (*this, &TranscodeVideoDialog::audio_combo_changed));
	scale_combo.signal_changed().connect (sigc::mem_fun (*this, &TranscodeVideoDialog::scale_combo_changed));
	aspect_checkbox.signal_toggled().connect (sigc::mem_fun (*this, &TranscodeVideoDialog::aspect_checkbox_toggled));
	height_spinner.signal_changed().connect (sigc::mem_fun (*this, &TranscodeVideoDialog::update_bitrate));
	bitrate_checkbox.signal_toggled().connect (sigc::mem_fun (*this, &TranscodeVideoDialog::bitrate_checkbox_toggled));

	update_bitrate();

	cancel_button = add_button (Stock::CANCEL, RESPONSE_CANCEL);
	get_action_area()->pack_start (transcode_button, false, false);
	show_all_children ();
	progress_box->hide();
}

TranscodeVideoDialog::~TranscodeVideoDialog ()
{
	delete transcoder;
}

void
TranscodeVideoDialog::on_show ()
{
	Dialog::on_show ();
}

void
TranscodeVideoDialog::abort_clicked ()
{
	aborted = true;
	transcoder->cancel();
}

void
TranscodeVideoDialog::update_progress (framecnt_t c, framecnt_t a)
{
	if (a == 0 || c > a) {
		pbar.set_pulse_step(.5);
		pbar.pulse();
		return;
	}
	pbar.set_fraction ((double)c / (double) a);
}

void
TranscodeVideoDialog::finished ()
{
	if (aborted) {
		::g_unlink(path_entry.get_text().c_str());
		if (!audiofile.empty()) {
			::g_unlink(audiofile.c_str());
		}
		Gtk::Dialog::response(RESPONSE_CANCEL);
	} else {
		if (pending_audio_extract) {
			StartNextStage();
		} else {
		  Gtk::Dialog::response(RESPONSE_ACCEPT);
		}
	}
}

void
TranscodeVideoDialog::launch_audioonly ()
{
	if (audio_combo.get_active_row_number() == 0) {
		finished();
		return;
	}
	dialog_progress_mode();
#if 1 /* tentative debug mode */
	if (debug_checkbox.get_active()) {
		transcoder->set_debug(true);
	}
#endif
	transcoder->Progress.connect(*this, invalidator (*this), boost::bind (&TranscodeVideoDialog::update_progress , this, _1, _2), gui_context());
	transcoder->Finished.connect(*this, invalidator (*this), boost::bind (&TranscodeVideoDialog::finished, this), gui_context());
	launch_extract();
}

void
TranscodeVideoDialog::launch_extract ()
{
	audiofile= path_entry.get_text() + ".wav"; /* TODO: mktemp */
	int audio_stream;
	pending_audio_extract = false;
	aborted = false;
	audio_stream = audio_combo.get_active_row_number() -1;
	progress_label.set_text (_("Extracting Audio.."));

	if (!transcoder->extract_audio(audiofile, _session->nominal_frame_rate(), audio_stream)) {
		ARDOUR_UI::instance()->popup_error(_("Audio Extraction Failed."));
		audiofile="";
		Gtk::Dialog::response(RESPONSE_CANCEL);
		return;
	}
}

void
TranscodeVideoDialog::dialog_progress_mode ()
{
	vbox->hide();
	cancel_button->hide();
	transcode_button.hide();
	pbar.set_size_request(300,-1);
	progress_box->show();
}

void
TranscodeVideoDialog::launch_transcode ()
{
	if (video_combo.get_active_row_number() != 2) {
		launch_audioonly();
		return;
	}
	std::string outfn = path_entry.get_text();
	if (!confirm_video_outfn(outfn, video_get_docroot(Config))) return;
	progress_label.set_text (_("Transcoding Video.."));
	dialog_progress_mode();
#if 1 /* tentative debug mode */
	if (debug_checkbox.get_active()) {
		transcoder->set_debug(true);
	}
#endif

	aborted = false;
	if (audio_combo.get_active_row_number() != 0) {
		pending_audio_extract = true;
		StartNextStage.connect(*this, invalidator (*this), boost::bind (&TranscodeVideoDialog::launch_extract , this), gui_context());
	}

	int scale_width, scale_height, bitrate;
	if (scale_combo.get_active_row_number() == 0 ) {
		scale_width =0;
	} else {
	  scale_width = atoi(scale_combo.get_active_text());
	}
	if (!aspect_checkbox.get_active()) {
		scale_height = 0;
	} else {
		scale_height = (int) floor(height_spinner.get_value());
	}
	if (bitrate_checkbox.get_active() ){
		bitrate = (int) floor(bitrate_spinner.get_value());
	} else {
		bitrate = 0;
	}

	transcoder->Progress.connect(*this, invalidator (*this), boost::bind (&TranscodeVideoDialog::update_progress , this, _1, _2), gui_context());
	transcoder->Finished.connect(*this, invalidator (*this), boost::bind (&TranscodeVideoDialog::finished, this), gui_context());
	if (!transcoder->transcode(outfn, scale_width, scale_height, bitrate)) {
		ARDOUR_UI::instance()->popup_error(_("Transcoding Failed."));
		Gtk::Dialog::response(RESPONSE_CANCEL);
		return;
	}
}

void
TranscodeVideoDialog::video_combo_changed ()
{
	int i = video_combo.get_active_row_number();
	if (i != 2) {
		scale_combo.set_sensitive(false);
		aspect_checkbox.set_sensitive(false);
		height_spinner.set_sensitive(false);
		bitrate_checkbox.set_sensitive(false);
		bitrate_spinner.set_sensitive(false);
	} else {
		scale_combo.set_sensitive(true);
		aspect_checkbox.set_sensitive(true);
		height_spinner.set_sensitive(true);
		bitrate_checkbox.set_sensitive(true);
		bitrate_spinner.set_sensitive(true);
	}
}

void
TranscodeVideoDialog::audio_combo_changed ()
{
	;
}

void
TranscodeVideoDialog::scale_combo_changed ()
{
	if (!aspect_checkbox.get_active()) {
		int h;
		if (scale_combo.get_active_row_number() == 0 ) {
			h = transcoder->get_height();
		} else {
			h = floor(atof(scale_combo.get_active_text()) / m_aspect);
		}
		height_spinner.set_value(h);
	}
	update_bitrate();
}

void
TranscodeVideoDialog::aspect_checkbox_toggled ()
{
	height_spinner.set_sensitive(aspect_checkbox.get_active());
	scale_combo_changed();
}

void
TranscodeVideoDialog::bitrate_checkbox_toggled ()
{
	bitrate_spinner.set_sensitive(bitrate_checkbox.get_active());
	if (!bitrate_checkbox.get_active()) {
		update_bitrate();
	}
}

void
TranscodeVideoDialog::update_bitrate ()
{
	double br = .7; /* avg quality - bits per pixel */
	if (bitrate_checkbox.get_active() || !transcoder->probe_ok()) { return; }
	br *= transcoder->get_fps();
	br *= height_spinner.get_value();

	if (scale_combo.get_active_row_number() == 0 ) {
		br *= transcoder->get_width();
	} else {
		br *= atof(scale_combo.get_active_text());
	}
	if (br != 0) {
		bitrate_spinner.set_value(floor(br/10000.0)*10);
	}
}

void
TranscodeVideoDialog::open_browse_dialog ()
{
	Gtk::FileChooserDialog dialog(_("Save Transcoded Video File"), Gtk::FILE_CHOOSER_ACTION_SAVE);
	dialog.set_filename (path_entry.get_text());

	dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);

	int result = dialog.run();

	if (result == Gtk::RESPONSE_OK) {
		std::string filename = dialog.get_filename();

		if (filename.length()) {
			path_entry.set_text (filename);
		}
	}
}

enum VtlTranscodeOption
TranscodeVideoDialog::import_option() {
	int i = video_combo.get_active_row_number();
	return static_cast<VtlTranscodeOption>(i);
}
