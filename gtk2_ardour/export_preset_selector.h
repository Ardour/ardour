/*
 * Copyright (C) 2008-2011 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#pragma once

#include <sigc++/signal.h>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/label.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treemodel.h>

#include "ardour/export_profile_manager.h"

class ExportPresetSelector : public Gtk::HBox
{
public:
	ExportPresetSelector (bool readonly = false);

	void set_manager (std::shared_ptr<ARDOUR::ExportProfileManager> manager);

	sigc::signal<void> CriticalSelectionChanged;

	Gtk::ComboBox& the_combo () { return combo; }

private:
	typedef std::shared_ptr<ARDOUR::ExportProfileManager> ManagerPtr;
	typedef ARDOUR::ExportPresetPtr                         PresetPtr;
	typedef ARDOUR::ExportProfileManager::PresetList        PresetList;

	void sync_with_manager ();
	void update_selection ();
	void create_new ();
	void save_current ();
	void remove_current ();
	void selection_changed ();

	ManagerPtr profile_manager;

	struct PresetCols : public Gtk::TreeModelColumnRecord {
	public:
		Gtk::TreeModelColumn<std::string> label;
		Gtk::TreeModelColumn<PresetPtr>   preset;

		PresetCols ()
		{
			add (label);
			add (preset);
		}
	};

	PresetCols                   cols;
	Glib::RefPtr<Gtk::ListStore> list;
	PresetPtr                    current;
	PresetPtr                    previous;

	Gtk::Label    label;
	Gtk::ComboBox combo;

	Gtk::Button save_button;
	Gtk::Button remove_button;
	Gtk::Button new_button;

	sigc::connection select_connection;
};

