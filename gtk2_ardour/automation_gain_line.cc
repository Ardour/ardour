/*
    Copyright (C) 2000-2003 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#include <sigc++/signal.h>

#include <ardour/curve.h>
#include <pbd/fastlog.h>

#include "public_editor.h"
#include "automation_gain_line.h"
#include "utils.h"

#include <ardour/session.h>

using namespace std;
using namespace ARDOUR;

AutomationGainLine::AutomationGainLine (string name, Session& s, TimeAxisView& tv, Gnome::Canvas::Item* parent,
					Curve& c, 
					gint (*point_callback)(Gnome::Canvas::Item*, GdkEvent*, gpointer),
					gint (*line_callback)(Gnome::Canvas::Item*, GdkEvent*, gpointer))
	: AutomationLine (name, tv, parent, c, point_callback, line_callback),
	  session (s)
{
	set_verbose_cursor_uses_gain_mapping (true);
}

void
AutomationGainLine::view_to_model_y (double& y)
{
	y = slider_position_to_gain (y);
	y = max (0.0, y);
	y = min (2.0, y);
}

void
AutomationGainLine::model_to_view_y (double& y)
{
	y = gain_to_slider_position (y);
}



