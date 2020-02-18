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

#ifndef __export_preset_selector_h__
#define __export_preset_selector_h__

#include <sigc++/signal.h>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/comboboxentry.h>
#include <gtkmm/label.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treemodel.h>

#include "ardour/export_profile_manager.h"

class ExportPresetSelector : public Gtk::HBox
{
public:

	ExportPresetSelector ();

	void set_manager (boost::shared_ptr<ARDOUR::ExportProfileManager> manager);

	sigc::signal<void> CriticalSelectionChanged;

private:

	typedef boost::shared_ptr<ARDOUR::ExportProfileManager> ManagerPtr;
	typedef ARDOUR::ExportPresetPtr PresetPtr;
	typedef ARDOUR::ExportProfileManager::PresetList PresetList;

	ManagerPtr       profile_manager;
	sigc::connection select_connection;

	void sync_with_manager ();
	void update_selection ();
	void create_new ();
	void save_current ();
	void remove_current ();

	struct PresetCols : public Gtk::TreeModelColumnRecord
	{
	public:
		Gtk::TreeModelColumn<PresetPtr>      preset;
		Gtk::TreeModelColumn<std::string>  label;

		PresetCols () { add (preset); add (label); }
	};
	PresetCols                   cols;
	Glib::RefPtr<Gtk::ListStore> list;
	PresetPtr                    current;
	PresetPtr                    previous;

	Gtk::Label          label;
	Gtk::ComboBoxEntry  entry;

	Gtk::Button         save_button;
	Gtk::Button         remove_button;
	Gtk::Button         new_button;
};

#endif
