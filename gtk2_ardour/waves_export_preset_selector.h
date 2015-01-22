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

#ifndef __waves_export_preset_selector_h__
#define __waves_export_preset_selector_h__

#include <sigc++/signal.h>
#include <gtkmm.h>

#include "ardour/export_profile_manager.h"
#include "waves_ui.h"

class WavesExportPresetSelector : public Gtk::HBox, public WavesUI
{

  public:

	WavesExportPresetSelector ();

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
	void create_new (WavesButton*);
	void save_current (WavesButton*);
	void remove_current (WavesButton*);

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

	Gtk::ComboBoxEntry  entry;

	WavesButton& _save_button;
	WavesButton& _remove_button;
	WavesButton& _new_button;
};

#endif // __waves_export_preset_selector_h__
