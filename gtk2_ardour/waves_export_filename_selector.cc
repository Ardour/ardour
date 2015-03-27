/*
    Copyright (C) 2008 Paul Davis
    Copyright (C) 2015 Waves Audio Ltd.
    Author: Sakari Bergen

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

#include <gtkmm/messagedialog.h>
#include "open_file_dialog.h"

#include "waves_export_filename_selector.h"
#include "waves_message_dialog.h"

#include "i18n.h"

using namespace ARDOUR;

WavesExportFilenameSelector::WavesExportFilenameSelector ()
 : Gtk::VBox ()
 , WavesUI ("waves_export_filename_selector.xml", *this)
 , _date_format_dropdown (get_waves_dropdown ("date_format_dropdown"))
 , _time_format_dropdown (get_waves_dropdown ("time_format_dropdown"))
 , _session_button (get_waves_button ("session_button"))
 , _revision_button (get_waves_button ("revision_button"))
 , _label_entry (get_entry ("label_entry"))
 , _path_label (get_label ("path_label"))
 , _revision_entry (get_entry ("revision_entry"))
 , _revision_inc_button (get_waves_button ("revision_inc_button"))
 , _revision_dec_button (get_waves_button ("revision_dec_button"))
 , _browse_button (get_waves_button ("browse_button"))
 , _example_filename_label (get_label("example_filename_label"))
{
	/* Date */
	_date_format_dropdown.selected_item_changed.connect (sigc::mem_fun (*this, &WavesExportFilenameSelector::change_date_format));

	/* Time */
	_time_format_dropdown.selected_item_changed.connect (sigc::mem_fun (*this, &WavesExportFilenameSelector::change_time_format));

	/* Signals */
	_label_entry.signal_changed().connect (sigc::mem_fun (*this, &WavesExportFilenameSelector::update_label));
	_revision_entry.signal_changed().connect (sigc::mem_fun (*this, &WavesExportFilenameSelector::update_revision));

	_session_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportFilenameSelector::change_session_selection));
	_revision_inc_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportFilenameSelector::on_revision_inc_button));
	_revision_dec_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportFilenameSelector::on_revision_dec_button));
	_revision_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportFilenameSelector::change_revision_selection));
	_browse_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportFilenameSelector::open_browse_dialog));
}

WavesExportFilenameSelector::~WavesExportFilenameSelector ()
{

}

void
WavesExportFilenameSelector::load_state ()
{
	if (!filename) {
		return;
	}

	_label_entry.set_text (filename->include_label ? filename->get_label() : "");
	_session_button.set_active (filename->include_session ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);;
	_revision_button.set_active_state (filename->include_revision ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	_revision_entry.set_text (string_compose ("%1", filename->get_revision()));
	_revision_entry.set_sensitive (filename->include_revision);
	_revision_inc_button.set_sensitive (filename->include_revision);
	_revision_dec_button.set_sensitive (filename->include_revision);
	_path_label.set_text (filename->get_folder());

	int size = _date_format_dropdown.get_menu ().items ().size ();
	for (int i = 0; i < size; i++) {
		if (_date_format_dropdown.get_item_data_u (i) == filename->get_date_format()) {
			_date_format_dropdown.set_current_item (i);
            break;
		}
	}

	size = _time_format_dropdown.get_menu ().items ().size ();
	for (int i = 0; i < size; i++) {
		if (_time_format_dropdown.get_item_data_u (i) == filename->get_time_format()) {
			_time_format_dropdown.set_current_item (i);
            break;
		}
	}
}

void
WavesExportFilenameSelector::set_state (ARDOUR::ExportProfileManager::FilenameStatePtr state_, ARDOUR::Session * session_)
{
	SessionHandlePtr::set_session (session_);

	filename = state_->filename;

	/* Fill dropdowns */

	/* Dates */
	_date_format_dropdown.clear_items ();
	_date_format_dropdown.add_menu_item (filename->get_date_format_str (ExportFilename::D_None), (void*)ExportFilename::D_None);
	_date_format_dropdown.add_menu_item (filename->get_date_format_str (ExportFilename::D_ISO), (void*)ExportFilename::D_ISO);
	_date_format_dropdown.add_menu_item (filename->get_date_format_str (ExportFilename::D_ISOShortY), (void*)ExportFilename::D_ISOShortY);
	_date_format_dropdown.add_menu_item (filename->get_date_format_str (ExportFilename::D_BE), (void*)ExportFilename::D_BE);
	_date_format_dropdown.add_menu_item (filename->get_date_format_str (ExportFilename::D_BEShortY), (void*)ExportFilename::D_BEShortY);

	/* Times */

	_time_format_dropdown.clear_items ();
	_time_format_dropdown.add_menu_item (filename->get_time_format_str (ExportFilename::T_None), (void*)ExportFilename::T_None);
	_time_format_dropdown.add_menu_item (filename->get_time_format_str (ExportFilename::T_NoDelim), (void*)ExportFilename::T_NoDelim);
	_time_format_dropdown.add_menu_item (filename->get_time_format_str (ExportFilename::T_Delim), (void*)ExportFilename::T_Delim);

	/* Load state */

	load_state();
}

void
WavesExportFilenameSelector::set_example_filename (std::string filename)
{
	if (filename == "") {
		_example_filename_label.set_markup (_("Sorry, no example filename can be shown at the moment"));
	} else {
		_example_filename_label.set_markup (string_compose(_("Current (approximate) filename: \"%1\""), filename));
	}
}

void
WavesExportFilenameSelector::update_label ()
{
	if (!filename) {
		return;
	}

	filename->set_label (_label_entry.get_text());

	filename->include_label = !_label_entry.get_text().empty();
	CriticalSelectionChanged();
}

void
WavesExportFilenameSelector::on_revision_inc_button (WavesButton*)
{
	change_revision_value (1);
}

void
WavesExportFilenameSelector::on_revision_dec_button (WavesButton*)
{
	change_revision_value (-1);
}

void
WavesExportFilenameSelector::change_revision_value (int change)
{
	int revision = (int)filename->get_revision() + change;
	filename->set_revision (revision < 1 ? 1 : revision);
	_revision_entry.set_text (string_compose ("%1", filename->get_revision()));
	CriticalSelectionChanged();
}

void
WavesExportFilenameSelector::change_date_format (WavesDropdown*, int item)
{
	if (!filename) {
		return;
	}

	filename->set_date_format ((DateFormat)_date_format_dropdown.get_item_data_u(item));
	CriticalSelectionChanged();
}

void
WavesExportFilenameSelector::change_time_format (WavesDropdown*, int item)
{
	if (!filename) {
		return;
	}

	filename->set_time_format ((TimeFormat)_time_format_dropdown.get_item_data_u(item));
	CriticalSelectionChanged();
}

void
WavesExportFilenameSelector::change_session_selection (WavesButton*)
{
	if (!filename) {
		return;
	}

	filename->include_session = (_session_button.active_state () == Gtkmm2ext::ExplicitActive);
	CriticalSelectionChanged();
}

void
WavesExportFilenameSelector::change_revision_selection (WavesButton*)
{
	if (!filename) {
		return;
	}

	bool selected = (_revision_button.active_state () == Gtkmm2ext::ExplicitActive);
	filename->include_revision = selected;

	_revision_entry.set_sensitive (selected);
	_revision_inc_button.set_sensitive (selected);
	_revision_dec_button.set_sensitive (selected);

	CriticalSelectionChanged();
}

void
WavesExportFilenameSelector::update_revision ()
{
	if (!filename) {
		return;
	}
	int revision = (uint32_t)PBD::atoi(_revision_entry.get_text());
	filename->set_revision (revision < 1 ? 1 : revision);
	CriticalSelectionChanged();
}

void
WavesExportFilenameSelector::open_browse_dialog (WavesButton*)
{
	std::string filename = ARDOUR::choose_folder_dialog (_path_label.get_text (), _("Choose export folder"));
	if (!filename.empty ()) {
		_path_label.set_text (filename);
      	this->filename->set_folder (filename);
	}
	CriticalSelectionChanged();
	return;
}
