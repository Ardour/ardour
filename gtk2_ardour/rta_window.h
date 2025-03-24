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

#include <glibmm/thread.h>

#include <ytkmm/box.h>
#include <ytkmm/drawingarea.h>

#include "ardour/ardour.h"
#include "ardour/session_handle.h"
#include "ardour/types.h"

#include "ardour_window.h"

class RTAWindow
	: public ArdourWindow
{
public:
	RTAWindow ();

	void set_session (ARDOUR::Session*);
	XMLNode& get_state () const;

private:
	void session_going_away ();
	void update_title ();
	void on_theme_changed ();

	void darea_size_request (Gtk::Requisition*);
	void darea_size_allocate (Gtk::Allocation&);
	bool darea_expose_event (GdkEventExpose*);

	Gtk::VBox         _vpacker;
	Gtk::DrawingArea  _darea;
};
