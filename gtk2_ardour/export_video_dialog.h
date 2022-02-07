/*
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
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
#ifndef __gtk_ardour_export_video_dialog_h__
#define __gtk_ardour_export_video_dialog_h__

#include <string>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>

#include "ardour/export_status.h"
#include "ardour/types.h"

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
class ExportVideoDialog : public ArdourDialog, public PBD::ScopedConnectionList
{
public:
	ExportVideoDialog ();
	~ExportVideoDialog ();

	std::string get_exported_filename ()
	{
		return outfn_path_entry.get_text ();
	}

	void apply_state (TimeSelection& tme, bool range);

	XMLNode& get_state ();
	void     set_state (const XMLNode&);

private:

	void abort_clicked ();
	void launch_export ();
	void encode_video ();
	void finished (int);

	void set_original_file_information ();
	void update_progress (ARDOUR::samplecnt_t, ARDOUR::samplecnt_t);
	gint audio_progress_display ();

	void open_outfn_dialog ();
	void open_invid_dialog ();

	bool _aborted;
	bool _normalize;

	boost::shared_ptr<ARDOUR::ExportStatus> status;

	TimeSelection    _export_range;
	sigc::connection _audio_progress_connection;
	float            _previous_progress;
	TranscodeFfmpeg* _transcoder;
	std::string      _insnd;

	Gtk::Label        outfn_path_label;
	Gtk::Entry        outfn_path_entry;
	Gtk::Button       outfn_browse_button;
	Gtk::Label        invid_path_label;
	Gtk::Entry        invid_path_entry;
	Gtk::Button       invid_browse_button;
	Gtk::ComboBoxText insnd_combo;
	Gtk::Button       transcode_button;

	Gtk::VBox*   vbox;
	Gtk::Button* cancel_button;
	Gtk::Button  abort_button;

	Gtk::VBox*       progress_box;
	Gtk::ProgressBar pbar;

	Gtk::CheckButton normalize_checkbox;
	Gtk::CheckButton meta_checkbox;
	Gtk::CheckButton debug_checkbox;
};

#endif /* __gtk_ardour_export_video_dialog_h__ */
