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
#ifndef __gtk_ardour_export_video_dialog_h__
#define __gtk_ardour_export_video_dialog_h__

#include <string>

#include <gtkmm.h>

#include "ardour/types.h"
#include "ardour/template_utils.h"
#include "ardour_dialog.h"

#include "time_selection.h"
#include "transcode_ffmpeg.h"

/** @class ExportVideoDialog
 *  @brief dialog box and controller for video-file export
 *
 *  The ExportVideoDialog includes audio export functions,
 *  video-encoder presets, progress dialogs and uses
 *  \ref TranscodeFfmpeg to communicate with ffmpeg.
 */
class ExportVideoDialog : public ArdourDialog , public PBD::ScopedConnectionList
{
  public:
	ExportVideoDialog ();
	~ExportVideoDialog ();

	std::string get_exported_filename () { return outfn_path_entry.get_text(); }

	void apply_state(TimeSelection &tme, bool range);

	XMLNode& get_state ();
	void set_state (const XMLNode &);

  private:
	TimeSelection export_range;

	void on_show ();
	void abort_clicked ();
	void launch_export ();
	void encode_pass (int);
	void change_file_extension (std::string);
	void width_value_changed ();
	void height_value_changed ();

	void set_original_file_information ();

	void open_outfn_dialog ();
	void open_invid_dialog ();
	void scale_checkbox_toggled ();
	void preset_combo_changed ();
	void video_codec_combo_changed ();
	void aspect_checkbox_toggled ();
	void fps_checkbox_toggled ();

	bool _aborted;
	bool _twopass;
	bool _firstpass;
	bool _normalize;

	void finished ();
	void update_progress (ARDOUR::framecnt_t, ARDOUR::framecnt_t);

	boost::shared_ptr<ARDOUR::ExportStatus> status;
	sigc::connection audio_progress_connection;
	gint audio_progress_display ();
	float _previous_progress;

	TranscodeFfmpeg *_transcoder;
	std::string _insnd;

	float _video_source_aspect_ratio;
	bool _suspend_signals;
	bool _suspend_dirty;

	Gtk::Label        outfn_path_label;
	Gtk::Entry        outfn_path_entry;
	Gtk::Button       outfn_browse_button;
	Gtk::Label        invid_path_label;
	Gtk::Entry        invid_path_entry;
	Gtk::Button       invid_browse_button;
	Gtk::ComboBoxText insnd_combo;
	Gtk::Button       transcode_button;

	Gtk::VBox* vbox;
	Gtk::Button *cancel_button;
	Gtk::Button abort_button;

	Gtk::VBox*  progress_box;
	Gtk::ProgressBar pbar;

	Gtk::ComboBoxText audio_codec_combo;
	Gtk::ComboBoxText video_codec_combo;
	Gtk::ComboBoxText video_bitrate_combo;
	Gtk::ComboBoxText audio_bitrate_combo;
	Gtk::ComboBoxText audio_samplerate_combo;
	Gtk::ComboBoxText preset_combo;

	Gtk::CheckButton  scale_checkbox;
	Gtk::CheckButton  scale_aspect;
	Gtk::Adjustment   width_adjustment;
	Gtk::SpinButton   width_spinner;
	Gtk::Adjustment   height_adjustment;
	Gtk::SpinButton   height_spinner;
	Gtk::CheckButton  aspect_checkbox;
	Gtk::ComboBoxText aspect_combo;
	Gtk::CheckButton  normalize_checkbox;
	Gtk::CheckButton  twopass_checkbox;
	Gtk::CheckButton  optimizations_checkbox;
	Gtk::Label        optimizations_label;
	Gtk::CheckButton  deinterlace_checkbox;
	Gtk::CheckButton  bframes_checkbox;
	Gtk::CheckButton  fps_checkbox;
	Gtk::ComboBoxText fps_combo;
	Gtk::CheckButton  meta_checkbox;
#if 1 /* tentative debug mode */
	Gtk::CheckButton  debug_checkbox;
#endif
};

#endif /* __gtk_ardour_export_video_dialog_h__ */
