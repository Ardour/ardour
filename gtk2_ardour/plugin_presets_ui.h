/*
 * Copyright (C) 2018-2019 Robin Gareus <robin@gareus.org>
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

#ifndef _gtkardour_plugin_presets_ui_h_
#define _gtkardour_plugin_presets_ui_h_

#include <gtkmm/label.h>
#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/table.h>
#include <gtkmm/textview.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treestore.h>
#include <gtkmm/treeview.h>

#include "widgets/ardour_button.h"

#include "ardour/plugin_insert.h"

class PluginPresetsUI : public Gtk::Table
{
public:
	PluginPresetsUI (boost::shared_ptr<ARDOUR::PluginInsert>);

private:
	void update_preset_list ();
	void filter_presets ();
	void preset_selected ();
	void preset_row_activated (Gtk::TreeModel::Path, Gtk::TreeViewColumn*);
	void load_preset ();

	boost::shared_ptr<ARDOUR::PluginInsert> _insert;
	PBD::ScopedConnectionList _preset_connections;

	struct PluginPreset {
		PluginPreset (ARDOUR::Plugin::PresetRecord const& p, std::string const& b = "", std::string const& t = "")
			: _preset_record (p)
			, _bank (b)
			, _type (t)
		{ }
		ARDOUR::Plugin::PresetRecord _preset_record;
		std::string _bank;
		std::string _type;

		bool operator< (PluginPreset const& o) const {
        return _preset_record.label < o._preset_record.label;
    }
	};

	std::vector<PluginPreset> _pps;

	struct TagFilterModelColumns : public Gtk::TreeModel::ColumnRecord {
		TagFilterModelColumns () {
			add (name);
			add (count);
		}
		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<size_t> count;
	};

	struct PluginPresetModelColumns : public Gtk::TreeModel::ColumnRecord {
		PluginPresetModelColumns () {
			add (name);
			add (description);
			add (plugin_preset);
		}

		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> description;
		Gtk::TreeModelColumn<ARDOUR::Plugin::PresetRecord> plugin_preset;
	};

	TagFilterModelColumns        _filter_banks_columns;
	Gtk::TreeView                _filter_banks_display;
	Glib::RefPtr<Gtk::TreeStore> _filter_banks_model;
	Gtk::ScrolledWindow          _banks_scroller;

	TagFilterModelColumns        _filter_types_columns;
	Gtk::TreeView                _filter_types_display;
	Glib::RefPtr<Gtk::TreeStore> _filter_types_model;
	Gtk::ScrolledWindow          _types_scroller;

	PluginPresetModelColumns     _plugin_preset_columns;
	Gtk::TreeView                _plugin_preset_display;
	Glib::RefPtr<Gtk::TreeStore> _plugin_preset_model;
	Gtk::ScrolledWindow          _preset_scroller;

	ArdourWidgets::ArdourButton  _load_button;
	Gtk::TextView                _preset_desc;
};

#endif
