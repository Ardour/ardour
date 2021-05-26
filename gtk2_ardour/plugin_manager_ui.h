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
	void on_show ();
	void selection_changed ();

	void rescan_all ();
	void rescan_selected ();
	void clear_log ();

	struct PluginColumns : public Gtk::TreeModel::ColumnRecord {
		PluginColumns () {
			add (status);
			add (blacklisted);
			add (name);
			add (creator);
			add (type);
			add (path);
			add (psle);
		}
		Gtk::TreeModelColumn<std::string> status;
		Gtk::TreeModelColumn<bool> blacklisted;
		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> type;
		Gtk::TreeModelColumn<std::string> creator;
		Gtk::TreeModelColumn<std::string> path;
		Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::PluginScanLogEntry> > psle;
	};

	PluginColumns                plugin_columns;
	Glib::RefPtr<Gtk::ListStore> plugin_model;
	Gtk::TreeView                plugin_display;
	Gtk::ScrolledWindow          _scroller;
	Gtk::TextView                _log;
	Gtk::ScrolledWindow          _log_scroller;
	ArdourWidgets::VPane         _pane;
	ArdourWidgets::ArdourButton  _btn_rescan_all;
	ArdourWidgets::ArdourButton  _btn_rescan_sel;
	ArdourWidgets::ArdourButton  _btn_clear;

	Gtk::Table _top;

	PBD::ScopedConnection _manager_connection;

};

#endif // _gtkardour_plugin_manager_h_
