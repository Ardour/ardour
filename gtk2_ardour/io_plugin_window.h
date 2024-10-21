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

#include "ardour_window.h"
#include "plugin_interest.h"
#include "io_button.h"
#include "window_manager.h"

namespace ARDOUR
{
	class IO;
	class IOPlug;
	class Port;
}

class IOSelectorWindow;
class PluginWindowProxy;

class IOPluginWindow : public ArdourWindow
{
public:
	IOPluginWindow ();

	void set_session (ARDOUR::Session*);

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

	class IOButton : public IOButtonBase
	{
	public:
		IOButton (std::shared_ptr<ARDOUR::IO>, bool pre);
		~IOButton ();

	private:
		void update ();
		bool button_press (GdkEventButton*);
		bool button_release (GdkEventButton*);
		void button_resized (Gtk::Allocation&);
		void port_pretty_name_changed (std::string);
		void port_connected_or_disconnected (std::weak_ptr<ARDOUR::Port>, std::weak_ptr<ARDOUR::Port>);
		void maybe_add_bundle_to_menu (std::shared_ptr<ARDOUR::Bundle>);
		void disconnect ();
		void bundle_chosen (std::shared_ptr<ARDOUR::Bundle>);
		void edit_io_configuration ();

		std::shared_ptr<ARDOUR::IO> _io;
		bool                          _pre;
		Gtk::Menu                     _menu;
		IOSelectorWindow*             _io_selector;
	};

	class IOPlugUI : public Gtk::Alignment
	{
	public:
		IOPlugUI (std::shared_ptr<ARDOUR::IOPlug>);

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
		std::shared_ptr<ARDOUR::IOPlug>   _iop;
		PBD::ScopedConnection             _going_away_connection;
	};

	/* IOPluginWindow members */
	void refill ();

	PluginBox _box_pre;
	PluginBox _box_post;
};

#endif
