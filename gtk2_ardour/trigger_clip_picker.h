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

#ifndef __gtk_ardour_trigger_clip_picker_h__
#define __gtk_ardour_trigger_clip_picker_h__

#include <set>
#include <string>

#include <gtkmm/box.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/scale.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/table.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treestore.h>
#include <gtkmm/treeview.h>

#include "ardour/types.h"
#include "ardour/session_handle.h"

#include "widgets/ardour_dropdown.h"
#include "widgets/ardour_button.h"

#include "instrument_selector.h"

class PluginUIWindow;

class TriggerClipPicker : public Gtk::VBox, public ARDOUR::SessionHandlePtr
{
public:
	TriggerClipPicker ();
	~TriggerClipPicker ();

	void set_session (ARDOUR::Session*);

	ARDOUR::PluginInfoPtr instrument_plugin () const {
		return _auditioner_combo.selected_instrument ();
	}

private:
	void list_dir (std::string const&, Gtk::TreeNodeChildren const* pc = NULL);
	void open_dir ();
	void edit_path ();
	void refill_dropdown ();
	void parameter_changed (std::string const&);
	void clip_added (std::string const&, void*);
	void row_selected ();
	void cursor_changed ();
	void row_activated (Gtk::TreeModel::Path const&, Gtk::TreeViewColumn*);
	bool test_expand (Gtk::TreeModel::iterator const&, Gtk::TreeModel::Path const&);
	void row_collapsed (Gtk::TreeModel::iterator const&, Gtk::TreeModel::Path const&);
	void drag_data_get (Glib::RefPtr<Gdk::DragContext> const&, Gtk::SelectionData&, guint, guint);
	void drag_begin (Glib::RefPtr<Gdk::DragContext> const&);
	void drag_end (Glib::RefPtr<Gdk::DragContext> const&);
	bool drag_motion (Glib::RefPtr<Gdk::DragContext> const&, int, int, guint);
	void drag_data_received (Glib::RefPtr<Gdk::DragContext> const&, int, int, Gtk::SelectionData const&, guint, guint);
	bool maybe_add_dir (std::string const&);
	void audition_selected ();
	void audition (std::string const&);
	void audition_active (bool);
	void audition_progress (ARDOUR::samplecnt_t, ARDOUR::samplecnt_t);
	void audition_processors_changed ();
	void audition_processor_going_away ();
	void audition_processor_idle ();
	bool audition_processor_viz (bool);
	void audition_show_plugin_ui ();
	void stop_audition ();
	void autoplay_toggled ();
	void open_library ();
	bool seek_button_press (GdkEventButton*);
	bool seek_button_release (GdkEventButton*);
	void auditioner_combo_changed ();

	ArdourWidgets::ArdourDropdown _clip_dir_menu;
	Gtk::FileChooserDialog        _fcd;

	struct Columns : public Gtk::TreeModel::ColumnRecord {
		Columns ()
		{
			add (name);
			add (path);
			add (read);
			add (file);
		}
		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> path;
		Gtk::TreeModelColumn<bool>        read;
		Gtk::TreeModelColumn<bool>        file;
	};

	Columns                      _columns;
	Glib::RefPtr<Gtk::TreeStore> _model;
	Gtk::TreeView                _view;
	Gtk::ScrolledWindow          _scroller;
	Gtk::Table                   _auditable;
	ArdourWidgets::ArdourButton  _play_btn;
	ArdourWidgets::ArdourButton  _stop_btn;
	ArdourWidgets::ArdourButton  _open_library_btn;
	ArdourWidgets::ArdourButton  _show_plugin_btn;
	Gtk::HScale                  _seek_slider;
	Gtk::CheckButton             _autoplay_btn;

	/* MIDI props */
	Gtk::Table _midi_prop_table;
	Gtk::Label format_text;
	Gtk::Label channels_value;

	InstrumentSelector           _auditioner_combo;

	std::string _current_path;
	std::string _clip_library_dir;
	bool        _clip_library_listed;
	bool        _ignore_list_dir;

	std::set<std::string> _root_paths;

	bool            _seeking;
	PluginUIWindow* _audition_plugnui;

	PBD::ScopedConnectionList _auditioner_connections;
	PBD::ScopedConnectionList _processor_connections;
	PBD::ScopedConnection     _config_connection;
	PBD::ScopedConnection     _clip_added_connection;
	sigc::connection          _idle_connection;
};

#endif
