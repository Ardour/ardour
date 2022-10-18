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

#include "ardour_dialog.h"
#include "export_preset_selector.h"
#include "public_editor.h"

namespace ARDOUR {
	class ExportHandler;
	class ExportStatus;
	class ExportProfileManager;
}

/** Base class for audio export
 *
 * This allows one to export audio from the session's
 * master bus using a given export-preset.
 *
 * By default the current range-selection (if a time selection exists) is
 * exported. Otherwise export falls back to use the session-range.
 */
class SimpleExport : public ARDOUR::SessionHandlePtr
{
public:
	SimpleExport (PublicEditor&);
	virtual ~SimpleExport () {}

	void set_session (ARDOUR::Session*);
	bool run_export ();

	void set_name (std::string const&);
	void set_folder (std::string const&);
	void set_range (samplepos_t, samplepos_t);
	bool set_preset (std::string const&);

	std::string preset_uuid () const;
	std::string folder () const;
	bool        check_outputs () const;

protected:
	PublicEditor& _editor;

	boost::shared_ptr<ARDOUR::ExportHandler>        _handler;
	boost::shared_ptr<ARDOUR::ExportStatus>         _status;
	boost::shared_ptr<ARDOUR::ExportProfileManager> _manager;

private:
	std::string _name;
	std::string _folder;
	std::string _pset_id;
	samplepos_t _start;
	samplepos_t _end;
};

/* Quick Export Dialog */
class SimpleExportDialog : public ArdourDialog, virtual public SimpleExport
{
public:
	SimpleExportDialog (PublicEditor&);

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
	void set_error (std::string const&);
	bool progress_timeout ();

	ExportPresetSelector _eps;
	Gtk::Button*         _cancel_button;
	Gtk::Button*         _export_button;
	Gtk::ComboBoxText    _range_combo;
	Gtk::ComboBoxText    _post_export_combo;
	Gtk::Label           _error_label;
	Gtk::ProgressBar     _progress_bar;

	sigc::connection _progress_connection;
};

#endif
