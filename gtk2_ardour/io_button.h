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

#include <boost/shared_ptr.hpp>
#include <gtkmm/menu.h>

#include "widgets/ardour_button.h"

namespace ARDOUR
{
	class Bundle;
	class IO;
	class Route;
	class Track;
	class Port;
}

class RouteUI;

class IOButton : public ArdourWidgets::ArdourButton
{
public:
	IOButton (bool input);
	~IOButton ();

	void set_route (boost::shared_ptr<ARDOUR::Route>, RouteUI*);

private:
	void update ();
	bool button_press (GdkEventButton*);
	bool button_release (GdkEventButton*);
	void button_resized (Gtk::Allocation&);
	void port_pretty_name_changed (std::string);
	void port_connected_or_disconnected (boost::weak_ptr<ARDOUR::Port>, boost::weak_ptr<ARDOUR::Port>);
	void maybe_add_bundle_to_menu (boost::shared_ptr<ARDOUR::Bundle>, ARDOUR::BundleList const&, ARDOUR::DataType = ARDOUR::DataType::NIL);
	void disconnect ();
	void add_port (ARDOUR::DataType);
	void bundle_chosen (boost::shared_ptr<ARDOUR::Bundle>);

	boost::shared_ptr<ARDOUR::IO>    io () const;
	boost::shared_ptr<ARDOUR::Track> track () const;
	ARDOUR::DataType                 guess_main_type (bool favor_connected = true) const;

	bool                                          _input;
	boost::shared_ptr<ARDOUR::Route>              _route;
	RouteUI*                                      _route_ui;
	Gtk::Menu                                     _menu;
	std::list<boost::shared_ptr<ARDOUR::Bundle> > _menu_bundles;
	PBD::ScopedConnectionList                     _connections;
	PBD::ScopedConnectionList                     _bundle_connections;
};

#endif
