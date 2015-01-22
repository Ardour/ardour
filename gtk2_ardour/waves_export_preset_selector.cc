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

#include "waves_export_preset_selector.h"

#include "ardour/export_preset.h"

#include "i18n.h"

WavesExportPresetSelector::WavesExportPresetSelector ()
  : Gtk::HBox ()
  , WavesUI ("waves_export_preset_selector.xml", *this)
  , _save_button (get_waves_button ("save_button"))
  , _remove_button (get_waves_button ("remove_button"))
  , _new_button (get_waves_button ("new_button"))
{
	list = Gtk::ListStore::create (cols);
	list->set_sort_column (cols.label, Gtk::SORT_ASCENDING);
	entry.set_model (list);
	entry.set_text_column (cols.label);

	pack_start (entry, true, true, 6);

	select_connection = entry.signal_changed().connect (sigc::mem_fun (*this, &WavesExportPresetSelector::update_selection));
	_save_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportPresetSelector::save_current));
	_new_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportPresetSelector::create_new));
	_remove_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportPresetSelector::remove_current));

	show_all_children ();
}

void
WavesExportPresetSelector::set_manager (boost::shared_ptr<ARDOUR::ExportProfileManager> manager)
{
	profile_manager = manager;
	sync_with_manager ();
}

void
WavesExportPresetSelector::sync_with_manager ()
{
	list->clear();

	PresetList const & presets = profile_manager->get_presets();
	Gtk::ListStore::iterator tree_it;

	for (PresetList::const_iterator it = presets.begin(); it != presets.end(); ++it) {
		tree_it = list->append();
		tree_it->set_value (cols.preset, *it);
		tree_it->set_value (cols.label, std::string ((*it)->name()));

		if (*it == current) {
			select_connection.block (true);
			entry.set_active (tree_it);
			select_connection.block (false);
		}
	}
}

void
WavesExportPresetSelector::update_selection ()
{
	Gtk::ListStore::iterator it = entry.get_active ();
	std::string text = entry.get_entry()->get_text();
	bool preset_name_exists = false;

	for (PresetList::const_iterator it = profile_manager->get_presets().begin(); it != profile_manager->get_presets().end(); ++it) {
		if (!(*it)->name().compare (text)) {
			preset_name_exists = true;
		}
	}

	if (list->iter_is_valid (it)) {

		previous = current = it->get_value (cols.preset);
		if (!profile_manager->load_preset (current)) {
			Gtk::MessageDialog dialog (_("The selected preset did not load successfully!\nPerhaps it references a format that has been removed?"),
			                           false, Gtk::MESSAGE_WARNING);
			dialog.run ();
		}
		sync_with_manager ();
		CriticalSelectionChanged();

		/* Make an edit, so that signal changed will be emitted on re-selection */

		select_connection.block (true);
		entry.get_entry()->set_text ("");
		entry.get_entry()->set_text (text);
		select_connection.block (false);

	} else { // Text has been edited, this should not make any changes in the profile manager
		if (previous && !text.compare (previous->name())) {
			current = previous;
		} else {
			current.reset ();
		}
	}

    _save_button.set_sensitive (current);
    _remove_button.set_sensitive (current);
	_new_button.set_sensitive (!current && !text.empty() && !preset_name_exists);
}

void
WavesExportPresetSelector::create_new (WavesButton*)
{
	if (!profile_manager) { return; }

	previous = current = profile_manager->new_preset (entry.get_entry()->get_text());
	sync_with_manager ();
	update_selection (); // Update preset widget states
}

void
WavesExportPresetSelector::save_current (WavesButton*)
{
	if (!profile_manager) { return; }

	previous = current = profile_manager->save_preset (entry.get_entry()->get_text());
	sync_with_manager ();
	update_selection (); // Update preset widget states
}

void
WavesExportPresetSelector::remove_current (WavesButton*)
{
	if (!profile_manager) { return; }

	Gtk::MessageDialog dialog (_("Do you really want to remove this preset?"),
			false,
			Gtk::MESSAGE_QUESTION,
			Gtk::BUTTONS_YES_NO);

	if (Gtk::RESPONSE_YES != dialog.run ()) {
		/* User has selected "no" or closed the dialog, better
		 * abort
		 */
		return;
	}

	profile_manager->remove_preset();
	entry.get_entry()->set_text ("");
	sync_with_manager ();
}
