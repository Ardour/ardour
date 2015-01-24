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

#include "waves_export_format_selector.h"
#include "export_format_dialog.h"
#include "ardour/export_format_specification.h"
#include "ardour/export_profile_manager.h"

#include "i18n.h"

WavesExportFormatSelector::WavesExportFormatSelector ()
  : Gtk::HBox ()
  , WavesUI ("waves_export_format_selector.xml", *this)
  , _edit_button (get_waves_button ("edit_button"))
  , _remove_button (get_waves_button ("remove_button"))
  , _new_button (get_waves_button ("new_button"))
  , _format_dropdown (get_waves_dropdown ("format_dropdown"))
{
	_edit_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportFormatSelector::on_edit_button));
	_remove_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportFormatSelector::on_remove_button));
	_new_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportFormatSelector::add_new_format));

	/* Sorted contents for dropdown */
	format_list = Gtk::ListStore::create (format_cols);
    format_list->set_sort_column (format_cols.label, Gtk::SORT_ASCENDING);

	_format_dropdown.selected_item_changed.connect (sigc::mem_fun (*this, &WavesExportFormatSelector::update_format_dropdown));
}

WavesExportFormatSelector::~WavesExportFormatSelector ()
{

}

void
WavesExportFormatSelector::set_state (ARDOUR::ExportProfileManager::FormatStatePtr const state_, ARDOUR::Session * session_)
{
	SessionHandlePtr::set_session (session_);

	state = state_;

	update_format_list ();
}

void
WavesExportFormatSelector::update_format_list ()
{
	FormatPtr format_to_select = state->format;
	format_list->clear();

	if (state->list->empty()) {
		_edit_button.set_sensitive (false);
		_remove_button.set_sensitive (false);
		return;
	} else {
		_edit_button.set_sensitive (true);
		_remove_button.set_sensitive (true);
	}

	Gtk::ListStore::iterator tree_it;

	for (FormatList::const_iterator it = state->list->begin(); it != state->list->end(); ++it) {
		tree_it = format_list->append();
		tree_it->set_value (format_cols.format, *it);
		tree_it->set_value (format_cols.label, (*it)->description());
	}

	{
		_format_dropdown.clear_items ();
		Gtk::TreeModel::Children::iterator it;
		for (it = format_list->children().begin(); it != format_list->children().end(); ++it) {
			_format_dropdown.add_menu_item (it->get_value (format_cols.label), (void*)it->get_value (format_cols.format).get ());
		}
	}

	if ((_format_dropdown.get_current_item () < 0) && 
		(_format_dropdown.get_menu ().items ().size () > 0)) {
		_format_dropdown.set_current_item (0);
	}

	select_format (format_to_select);
}

void
WavesExportFormatSelector::select_format (FormatPtr f)
{
	int size = _format_dropdown.get_menu ().items ().size ();
	for (int i = 0; i < size; i++) {
		if (_format_dropdown.get_item_associated_data (i) == (void*)f.get()) {
			_format_dropdown.set_current_item (i);
			break;
		}
	}

	CriticalSelectionChanged();
}

void
WavesExportFormatSelector::on_edit_button (WavesButton*)
{
	open_edit_dialog (false);
}

void
WavesExportFormatSelector::on_remove_button (WavesButton*)
{
	remove_format (true);
}

void
WavesExportFormatSelector::add_new_format (WavesButton*)
{
	FormatPtr new_format = state->format = NewFormat (state->format);

	if (open_edit_dialog (true) != Gtk::RESPONSE_APPLY) {
		remove_format(false);
		if (state->list->empty()) {
			state->format.reset ();
		}
	}
}

void
WavesExportFormatSelector::remove_format (bool called_from_button)
{
	if (called_from_button) {
		Gtk::MessageDialog dialog (_("Do you really want to remove the format?"),
				false,
				Gtk::MESSAGE_QUESTION,
				Gtk::BUTTONS_YES_NO);

		if (Gtk::RESPONSE_YES != dialog.run ()) {
			/* User has selected "no" or closed the dialog, better
			 * abort
			 */
			return;
		}
	}

	int current_item = _format_dropdown.get_current_item();
	if (current_item >= 0) {
		void* item_data = _format_dropdown.get_item_associated_data (current_item);

		Gtk::TreeModel::Children::iterator it;
		for (it = format_list->children().begin(); it != format_list->children().end(); ++it) {
			FormatPtr remove = it->get_value (format_cols.format);
			if (item_data == (void*)remove.get ()) {
				FormatRemoved (remove);
			}
		}
	}
}

int
WavesExportFormatSelector::open_edit_dialog (bool new_dialog)
{
	ExportFormatDialog dialog (state->format, new_dialog);
	dialog.set_session (_session);
	Gtk::ResponseType response = (Gtk::ResponseType) dialog.run();
	if (response == Gtk::RESPONSE_APPLY) {
		update_format_description ();
		FormatEdited (state->format);
		CriticalSelectionChanged();
	}
	return response;
}

void
WavesExportFormatSelector::update_format_dropdown (WavesDropdown*, int item)
{
	std::cout << "WavesExportFormatSelector::update_format_dropdown ()" << std::endl;
	Gtk::TreeModel::Children::iterator it;
	bool done = false;
	for (it = format_list->children().begin(); it != format_list->children().end(); ++it) {
		FormatPtr format = it->get_value (format_cols.format);
		if ((void*)format.get () == _format_dropdown.get_item_associated_data(item)) {
			state->format = format;
			std::cout << "    Changed format to " << format->name () << std::endl;
			done = true;
			break;
		}
	}

	if (!done) {
		if (!format_list->children().empty()) {
			_format_dropdown.set_current_item (0);
		} else {
			_edit_button.set_sensitive (false);
			_remove_button.set_sensitive (false);
		}
	}
	CriticalSelectionChanged();
}

void
WavesExportFormatSelector::update_format_description ()
{
	std::cout << "WavesExportFormatSelector::update_format_description () : " << state->format->description() << std::endl;
//	format_combo.get_active()->set_value(format_cols.label, state->format->description());
}
