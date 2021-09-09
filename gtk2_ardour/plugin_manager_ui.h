/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#ifndef _gtkardour_plugin_manager_h_
#define _gtkardour_plugin_manager_h_

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/table.h>
#include <gtkmm/textview.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treerowreference.h>
#include <gtkmm/treestore.h>
#include <gtkmm/treeview.h>

#include "widgets/ardour_button.h"
#include "widgets/pane.h"

#include "ardour/plugin_manager.h"

#include "ardour_window.h"

class PluginManagerUI : public ArdourWindow
{
public:
	PluginManagerUI ();
	~PluginManagerUI ();

private:
	void refill ();
	void maybe_refill (ArdourWidgets::ArdourButton*);
	void selection_changed ();
	void blacklist_changed (std::string const&);
	void favorite_changed (std::string const&);
	void hidden_changed (std::string const&);
	void row_activated (Gtk::TreeModel::Path const&, Gtk::TreeViewColumn*);

	void reindex ();
	void discover ();
	void rescan_all ();
	void rescan_faulty ();
	void rescan_selected ();
	void clear_log ();
	void show_plugin_prefs ();
	void vst_path_cb (ARDOUR::PluginType);
	void edit_vst_path (std::string const&, std::string const&, sigc::slot<std::string>, sigc::slot<bool, std::string>);

	void search_entry_changed ();
	void search_clear_button_clicked ();
	bool show_this_plugin (boost::shared_ptr<ARDOUR::PluginScanLogEntry>, ARDOUR::PluginInfoPtr, const std::string&);

	void plugin_status_changed (ARDOUR::PluginType, std::string, ARDOUR::PluginManager::PluginStatusType);

	struct PluginCount {
		PluginCount ()
			: total (0)
			, error (0)
			, stale (0)
			, ndscn (0)
		{}

		void set (ARDOUR::PluginScanLogEntry const& psle);

		unsigned int total;
		unsigned int error;
		unsigned int stale;
		unsigned int ndscn;
	};

	struct PluginColumns : public Gtk::TreeModel::ColumnRecord {
		PluginColumns ()
		{
			add (status);
			add (blacklisted);
			add (favorite);
			add (hidden);
			add (type);
			add (path);
			add (name);
			add (creator);
			add (tags);
			add (psle);
			add (plugin);
			add (can_blacklist);
			add (can_fav_hide);
		}
		Gtk::TreeModelColumn<std::string>                                    status;
		Gtk::TreeModelColumn<bool>                                           blacklisted;
		Gtk::TreeModelColumn<bool>                                           favorite;
		Gtk::TreeModelColumn<bool>                                           hidden;
		Gtk::TreeModelColumn<std::string>                                    name;
		Gtk::TreeModelColumn<std::string>                                    type;
		Gtk::TreeModelColumn<std::string>                                    creator;
		Gtk::TreeModelColumn<std::string>                                    tags;
		Gtk::TreeModelColumn<std::string>                                    path;
		Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::PluginScanLogEntry> > psle;
		Gtk::TreeModelColumn<ARDOUR::PluginInfoPtr>                          plugin;
		Gtk::TreeModelColumn<bool>                                           can_blacklist;
		Gtk::TreeModelColumn<bool>                                           can_fav_hide;
	};

	PluginColumns                plugin_columns;
	Glib::RefPtr<Gtk::ListStore> plugin_model;
	Gtk::TreeView                plugin_display;
	Gtk::ScrolledWindow          _scroller;
	Gtk::TextView                _log;
	Gtk::ScrolledWindow          _log_scroller;
	ArdourWidgets::VPane         _pane;
	ArdourWidgets::ArdourButton  _btn_reindex;
	ArdourWidgets::ArdourButton  _btn_discover;
	ArdourWidgets::ArdourButton  _btn_rescan_all;
	ArdourWidgets::ArdourButton  _btn_rescan_err;
	ArdourWidgets::ArdourButton  _btn_rescan_sel;
	ArdourWidgets::ArdourButton  _btn_clear;
	ArdourWidgets::ArdourButton  _btn_prefs;
	Gtk::Table                   _tbl_nfo;
	Gtk::Table                   _tbl_search;
	Gtk::Entry                   _entry_search;
	Gtk::Button                  _btn_search_clear;
	ArdourWidgets::ArdourButton  _cb_search_name;
	ArdourWidgets::ArdourButton  _cb_search_type;
	ArdourWidgets::ArdourButton  _cb_search_tags;
	ArdourWidgets::ArdourButton  _cb_search_creator;
	ArdourWidgets::ArdourButton  _cb_search_base_name;
	ArdourWidgets::ArdourButton  _cb_search_full_path;

	Gtk::Table _top;

	bool _in_row_change;
	bool _in_search_change;

	PBD::ScopedConnectionList _manager_connections;
};

#endif // _gtkardour_plugin_manager_h_
