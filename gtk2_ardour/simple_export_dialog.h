/*
 * Copyright (C) 2022 Robin Gareus <robin@gareus.org>
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

#ifndef _gtkardour_simple_export_dialog_h_
#define _gtkardour_simple_export_dialog_h_

#include <gtkmm/button.h>
#include <gtkmm/combobox.h>
#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>

#include "ardour/simple_export.h"

#include "ardour_dialog.h"
#include "export_preset_selector.h"
#include "public_editor.h"

namespace ARDOUR {
	class ExportHandler;
	class ExportStatus;
	class ExportProfileManager;
}
/* Quick Export Dialog */
class SimpleExportDialog : public ArdourDialog, virtual public ARDOUR::SimpleExport
{
public:
	SimpleExportDialog (PublicEditor&, bool vapor_export = false);

	void set_session (ARDOUR::Session*);

protected:
	void on_response (int response_id)
	{
		Gtk::Dialog::on_response (response_id);
	}

	XMLNode& get_state () const;
	void     set_state (XMLNode const&);

private:
	void start_export ();
	void close_dialog ();
	void show_progress ();
	void check_manager ();
	void set_error (std::string const&);
	bool progress_timeout ();

	class ExportRangeCols : public Gtk::TreeModelColumnRecord
	{
	public:
		ExportRangeCols ()
		{
			add (label);
			add (name);
			add (start);
			add (end);
		}
		Gtk::TreeModelColumn<std::string> label;
		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<samplepos_t> start;
		Gtk::TreeModelColumn<samplepos_t> end;
	};

	PublicEditor&        _editor;
	ExportPresetSelector _eps;
	Gtk::Button*         _cancel_button;
	Gtk::Button*         _export_button;
	Gtk::ComboBox        _range_combo;
	Gtk::ComboBoxText    _post_export_combo;
	Gtk::Label           _error_label;
	Gtk::ProgressBar     _progress_bar;
	bool                 _vapor_export;

	ExportRangeCols              _range_cols;
	Glib::RefPtr<Gtk::ListStore> _range_list;

	sigc::connection _progress_connection;
	sigc::connection _preset_cfg_connection;
};

#endif
