/*
 * Copyright (C) 2005-2006 Doug McLain <doug@nostar.net>
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2011 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2006 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2009 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_plugin_selector_h__
#define __ardour_plugin_selector_h__

#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/entry.h>
#include <gtkmm/liststore.h>
#include <gtkmm/notebook.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treeview.h>

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"

#include "gtkmm2ext/dndtreeview.h"

#include "ardour/plugin.h"
#include "ardour/plugin_manager.h"
#include "ardour/session_handle.h"

#include "plugin_interest.h"
#include "ardour_dialog.h"

namespace ARDOUR {
	class Session;
	class PluginManager;
}

class PluginSelector : public ArdourDialog
{
public:
	PluginSelector (ARDOUR::PluginManager&);
	~PluginSelector ();

	void set_interested_object (PluginInterestedObject&);

	int run (); // XXX should we try not to overload the non-virtual Gtk::Dialog::run() ?

	void on_show ();

	Gtk::Menu* plugin_menu ();
	void show_manager ();

private:

	//search
	ArdourWidgets::ArdourButton* _search_name_checkbox;
	ArdourWidgets::ArdourButton* _search_tags_checkbox;
	ArdourWidgets::ArdourButton* _search_ignore_checkbox;

	//radio-button filters
	Gtk::RadioButton *_fil_effects_radio;
	Gtk::RadioButton *_fil_instruments_radio;
	Gtk::RadioButton *_fil_utils_radio;
	Gtk::RadioButton *_fil_favorites_radio;
	Gtk::RadioButton *_fil_hidden_radio;
	Gtk::RadioButton *_fil_all_radio;

	/* combobox filters */
	ArdourWidgets::ArdourDropdown _fil_type_combo;
	ArdourWidgets::ArdourDropdown _fil_creator_combo;

	PluginInterestedObject* interested_object;

	Gtk::ScrolledWindow scroller;   // Available plugins
	Gtk::ScrolledWindow ascroller;  // Added plugins

	Gtk::Entry search_entry;
	Gtk::Button search_clear_button;

	Gtk::Entry *tag_entry;
	Gtk::Button* tag_reset_button;
	void tag_reset_button_clicked ();

	void set_sensitive_widgets();

	void search_clear_button_clicked ();
	void search_entry_changed ();

	void tag_entry_changed ();
	sigc::connection tag_entry_connection;

	void tags_changed ( ARDOUR::PluginType t, std::string unique_id, std::string tag);

	struct PluginColumns : public Gtk::TreeModel::ColumnRecord {
		PluginColumns () {
			add (favorite);
			add (name);
			add (tags);
			add (creator);
			add (type_name);
			add (audio_io);
			add (midi_io);
			add (plugin);
		}
		Gtk::TreeModelColumn<bool> favorite;
		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> type_name;
		Gtk::TreeModelColumn<std::string> creator;
		Gtk::TreeModelColumn<std::string> tags;
		Gtk::TreeModelColumn<std::string> audio_io;
		Gtk::TreeModelColumn<std::string> midi_io;
		Gtk::TreeModelColumn<ARDOUR::PluginInfoPtr> plugin;
	};
	PluginColumns plugin_columns;
	Glib::RefPtr<Gtk::ListStore> plugin_model;
	Gtkmm2ext::DnDTreeView<ARDOUR::PluginInfoPtr> plugin_display;
	Gtk::Button* btn_add;
	Gtk::Button* btn_remove;

	struct AddedColumns : public Gtk::TreeModel::ColumnRecord {
		AddedColumns () {
			add (text);
			add (plugin);
		}
		Gtk::TreeModelColumn<std::string> text;
		Gtk::TreeModelColumn<ARDOUR::PluginInfoPtr> plugin;
	};
	AddedColumns acols;
	Glib::RefPtr<Gtk::ListStore> amodel;
	Gtk::TreeView added_list;

	void refill ();
	void refiller (const ARDOUR::PluginInfoList& plugs, const::std::string& filterstr, const char* type);
	void ladspa_refiller (const std::string&);
	void lv2_refiller (const std::string&);
	void vst_refiller (const std::string&);
	void lxvst_refiller (const std::string&);
	void mac_vst_refiller (const std::string&);
	void au_refiller (const std::string&);
	void lua_refiller (const std::string&);
	void vst3_refiller (const std::string&);

	Gtk::Menu* _plugin_menu;
	ARDOUR::PluginManager& manager;

	void row_activated(Gtk::TreeModel::Path path, Gtk::TreeViewColumn* col);
	void btn_add_clicked();
	void btn_remove_clicked();
	void added_list_selection_changed();
	void added_row_clicked(GdkEventButton* event);
	void display_selection_changed();
	void btn_apply_clicked();
	ARDOUR::PluginPtr load_plugin (ARDOUR::PluginInfoPtr);
	bool show_this_plugin (const ARDOUR::PluginInfoPtr&, const std::string&);

	void favorite_changed (const std::string& path);
	bool in_row_change;

	void plugin_chosen_from_menu (const ARDOUR::PluginInfoPtr&);

	void plugin_status_changed ( ARDOUR::PluginType t, std::string unique_id, ARDOUR::PluginManager::PluginStatusType s );

	Gtk::Menu* create_favs_menu (ARDOUR::PluginInfoList&);
	Gtk::Menu* create_charts_menu (ARDOUR::PluginInfoList&);
	Gtk::Menu* create_by_creator_menu (ARDOUR::PluginInfoList&);
	Gtk::Menu* create_by_tags_menu (ARDOUR::PluginInfoList&);
	void build_plugin_menu ();
	PBD::ScopedConnectionList plugin_list_changed_connection;

	bool _need_tag_save;
	bool _need_status_save;
	bool _need_menu_rebuild;
	bool _inhibit_refill;
};

#endif // __ardour_plugin_selector_h__
