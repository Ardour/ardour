/*
 * Copyright (C) 2022 Robin Gareus <robin@gareus.org>
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

#ifndef _gtkardour_ioplugin_window_h_
#define _gtkardour_ioplugin_window_h_

#include <gtkmm/alignment.h>
#include <gtkmm/box.h>
#include <gtkmm/frame.h>
#include <gtkmm/menu.h>

#include "pbd/signals.h"

#include "widgets/ardour_button.h"

#include "ardour_window.h"
#include "plugin_interest.h"
#include "window_manager.h"

namespace ARDOUR
{
	class IO;
	class IOPlug;
	class PlugInsertBase;
	class Port;
}

class IOSelectorWindow;

class IOPluginWindow : public ArdourWindow
{
public:
	IOPluginWindow ();

	void set_session (ARDOUR::Session*);

	class PluginWindowProxy : public WM::ProxyBase
	{
	public:
		PluginWindowProxy (std::string const&, boost::weak_ptr<ARDOUR::PlugInsertBase>);
		~PluginWindowProxy ();
		Gtk::Window* get (bool create = false);

		void show_the_right_window ();

		ARDOUR::SessionHandlePtr* session_handle ()
		{
			return 0;
		}

		void set_custom_ui_mode (bool use_custom)
		{
			_want_custom = use_custom;
		}

		int      set_state (const XMLNode&, int);
		XMLNode& get_state () const;

	private:
		void plugin_going_away ();

		boost::weak_ptr<ARDOUR::PlugInsertBase> _pib;

		bool _is_custom;
		bool _want_custom;

		PBD::ScopedConnection _going_away_connection;
	};

protected:
	void on_show ();
	void on_hide ();

private:
	class PluginBox : public Gtk::EventBox, public PluginInterestedObject, public ARDOUR::SessionHandlePtr
	{
	public:
		PluginBox (bool is_pre);
		void clear ();
		void add_child (Gtk::Widget&);

	private:
		bool use_plugins (SelectedPlugins const&);
		void load_plugin (ARDOUR::PluginPresetPtr const&);
		bool button_press_event (GdkEventButton*);
		void drag_data_received (Glib::RefPtr<Gdk::DragContext> const&, int, int, Gtk::SelectionData const&, guint, guint);

		Gtk::HBox     _top;
		Gtk::HBox     _hbox;
		Gtk::EventBox _base;
		bool          _is_pre;
	};

	class IOButton : public ArdourWidgets::ArdourButton
	{
	public:
		IOButton (boost::shared_ptr<ARDOUR::IO>, bool pre);
		~IOButton ();

	private:
		void update ();
		bool button_press (GdkEventButton*);
		bool button_release (GdkEventButton*);
		void button_resized (Gtk::Allocation&);
		void port_pretty_name_changed (std::string);
		void port_connected_or_disconnected (boost::weak_ptr<ARDOUR::Port>, boost::weak_ptr<ARDOUR::Port>);
		void maybe_add_bundle_to_menu (boost::shared_ptr<ARDOUR::Bundle>);
		void disconnect ();
		void bundle_chosen (boost::shared_ptr<ARDOUR::Bundle>);
		void edit_io_configuration ();

		boost::shared_ptr<ARDOUR::IO> _io;
		bool                          _pre;
		Gtk::Menu                     _menu;
		IOSelectorWindow*             _io_selector;
		PBD::ScopedConnectionList     _connections;
		PBD::ScopedConnectionList     _bundle_connections;
	};

	class IOPlugUI : public Gtk::Alignment
	{
	public:
		IOPlugUI (boost::shared_ptr<ARDOUR::IOPlug>);

	private:
		bool button_press_event (GdkEventButton*);
		void button_resized (Gtk::Allocation&);
		void self_delete (); /* implicit */
		void self_remove (); /* explicit */
		void edit_plugin (bool);

		Gtk::Frame                        _frame;
		Gtk::VBox                         _box;
		IOButton                          _btn_input;
		IOButton                          _btn_output;
		ArdourWidgets::ArdourButton       _btn_ioplug;
		PluginWindowProxy*                _window_proxy;
		boost::shared_ptr<ARDOUR::IOPlug> _iop;
		PBD::ScopedConnection             _going_away_connection;
	};

	/* IOPluginWindow members */
	void refill ();

	PluginBox _box_pre;
	PluginBox _box_post;
};

#endif
