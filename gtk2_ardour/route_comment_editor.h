/*
 * Copyright (C) 2025 Robin Gareus <robin@gareus.org>
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

#pragma once

#include <memory>

#include <ytkmm/box.h>
#include <ytkmm/textview.h>

#include "ardour/route.h"

#include "ardour_window.h"

class BoolOption;

class RouteCommentEditor : public ArdourWindow
{
public:
	RouteCommentEditor ();
	~RouteCommentEditor ();

	void reset ();
	void toggle (std::shared_ptr<ARDOUR::Route>);
	void open (std::shared_ptr<ARDOUR::Route>);

private:
	void comment_changed ();
	void commit_change ();

	Gtk::TextView _comment_area;
	Gtk::VBox     _vbox;
	BoolOption*   _bo;
	bool          _ignore_change;

	std::shared_ptr<ARDOUR::Route> _route;
	PBD::ScopedConnectionList      _connections;
};
