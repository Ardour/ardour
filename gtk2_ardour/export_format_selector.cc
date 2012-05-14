/*
    Copyright (C) 2008 Paul Davis
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

#include "export_format_selector.h"

#include "export_format_dialog.h"

#include "ardour/export_format_specification.h"
#include "ardour/export_profile_manager.h"
#include "ardour/session.h"

#include "i18n.h"

ExportFormatSelector::ExportFormatSelector () :
  edit_button (Gtk::Stock::EDIT),
  remove_button (Gtk::Stock::REMOVE),
  new_button (Gtk::Stock::NEW)
{
	pack_start (format_combo, true, true, 0);
	pack_start (edit_button, false, false, 3);
	pack_start (remove_button, false, false, 3);
	pack_start (new_button, false, false, 3);

	format_combo.set_name ("PaddedButton");
	edit_button.set_name ("PaddedButton");
	remove_button.set_name ("PaddedButton");
	new_button.set_name ("PaddedButton");

	edit_button.signal_clicked().connect (sigc::hide_return (sigc::bind (sigc::mem_fun (*this, &ExportFormatSelector::open_edit_dialog), false)));
	remove_button.signal_clicked().connect (sigc::mem_fun (*this, &ExportFormatSelector::remove_format));
	new_button.signal_clicked().connect (sigc::mem_fun (*this, &ExportFormatSelector::add_new_format));

	/* Format combo */

	format_list = Gtk::ListStore::create (format_cols);
        format_list->set_sort_column (format_cols.label, Gtk::SORT_ASCENDING);
	format_combo.set_model (format_list);
	format_combo.pack_start (format_cols.label);
	format_combo.set_active (0);

	format_combo.signal_changed().connect (sigc::mem_fun (*this, &ExportFormatSelector::update_format_combo));
}

ExportFormatSelector::~ExportFormatSelector ()
{

}

void
ExportFormatSelector::set_state (ARDOUR::ExportProfileManager::FormatStatePtr const state_, ARDOUR::Session * session_)
{
	SessionHandlePtr::set_session (session_);

	state = state_;

	update_format_list ();
}

void
ExportFormatSelector::update_format_list ()
{
	FormatPtr format_to_select = state->format;
	format_list->clear();

	if (state->list->empty()) {
		edit_button.set_sensitive (false);
		remove_button.set_sensitive (false);
		return;
	} else {
		edit_button.set_sensitive (true);
		remove_button.set_sensitive (true);
	}

	Gtk::ListStore::iterator tree_it;

	for (FormatList::const_iterator it = state->list->begin(); it != state->list->end(); ++it) {
		tree_it = format_list->append();
		tree_it->set_value (format_cols.format, *it);
		tree_it->set_value (format_cols.label, (*it)->description());
	}

	if (format_combo.get_active_row_number() == -1 && format_combo.get_model()->children().size() > 0) {
		format_combo.set_active (0);
	}

	select_format (format_to_select);
}

void
ExportFormatSelector::select_format (FormatPtr f)
{
	Gtk::TreeModel::Children::iterator it;
	for (it = format_list->children().begin(); it != format_list->children().end(); ++it) {
		if (it->get_value (format_cols.format) == f) {
			format_combo.set_active (it);
			break;
		}
	}

	CriticalSelectionChanged();
}

void
ExportFormatSelector::add_new_format ()
{
	FormatPtr new_format = state->format = NewFormat (state->format);

	if (open_edit_dialog (true) != Gtk::RESPONSE_APPLY) {
		remove_format();
		if (state->list->empty()) {
			state->format.reset ();
		}
	}
}

void
ExportFormatSelector::remove_format ()
{
	FormatPtr remove;
	Gtk::TreeModel::iterator it = format_combo.get_active();
	remove = it->get_value (format_cols.format);
	FormatRemoved (remove);
}

int
ExportFormatSelector::open_edit_dialog (bool new_dialog)
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
ExportFormatSelector::update_format_combo ()
{
	Gtk::TreeModel::iterator it = format_combo.get_active();
	if (format_list->iter_is_valid (it)) {
		state->format = it->get_value(format_cols.format);
	} else if (!format_list->children().empty()) {
		format_combo.set_active (0);
	} else {
		edit_button.set_sensitive (false);
		remove_button.set_sensitive (false);
	}

	CriticalSelectionChanged();
}

void
ExportFormatSelector::update_format_description ()
{
	format_combo.get_active()->set_value(format_cols.label, state->format->description());
}
