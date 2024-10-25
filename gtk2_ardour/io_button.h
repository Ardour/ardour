/*
 * Copyright (C) 2005-2021 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2021 Robin Gareus <robin@gareus.org>
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

#ifndef _gtkardour_io_button_h_
#define _gtkardour_io_button_h_

#include <list>
#include <memory>

#include <gtkmm/menu.h>

#include "ardour/data_type.h"
#include "ardour/types.h"

#include "widgets/ardour_button.h"

namespace PBD
{
	class PropertyChange;
}

namespace ARDOUR
{
	class Bundle;
	class IO;
	class Route;
	class Session;
	class Track;
	class Port;
}

class RouteUI;

class IOButtonBase : public ArdourWidgets::ArdourButton
{
public:
	virtual ~IOButtonBase () {}

protected:
	static void             set_label (IOButtonBase&, ARDOUR::Session&, std::shared_ptr<ARDOUR::Bundle>&, std::shared_ptr<ARDOUR::IO>);
	static ARDOUR::DataType guess_main_type (std::shared_ptr<ARDOUR::IO>);

	virtual void update () = 0;
	void         maybe_update (PBD::PropertyChange const& what_changed);

	PBD::ScopedConnectionList _connections;
	PBD::ScopedConnectionList _bundle_connections;
};

class IOButton : public IOButtonBase
{
public:
	IOButton (bool input);
	~IOButton ();

	void set_route (std::shared_ptr<ARDOUR::Route>, RouteUI*);

private:
	void update ();
	bool button_press (GdkEventButton*);
	bool button_release (GdkEventButton*);
	void button_resized (Gtk::Allocation&);
	void port_pretty_name_changed (std::string);
	void port_connected_or_disconnected (std::weak_ptr<ARDOUR::Port>, std::weak_ptr<ARDOUR::Port>);
	void maybe_add_bundle_to_menu (std::shared_ptr<ARDOUR::Bundle>, ARDOUR::BundleList const&, ARDOUR::DataType = ARDOUR::DataType::NIL);
	void disconnect ();
	void add_port (ARDOUR::DataType);
	void bundle_chosen (std::shared_ptr<ARDOUR::Bundle>);

	std::shared_ptr<ARDOUR::IO>    io () const;
	std::shared_ptr<ARDOUR::Track> track () const;

	bool                                          _input;
	std::shared_ptr<ARDOUR::Route>              _route;
	RouteUI*                                      _route_ui;
	Gtk::Menu                                     _menu;
	std::list<std::shared_ptr<ARDOUR::Bundle> > _menu_bundles;
};

#endif
