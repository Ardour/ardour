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

*/

#include <sigc++/signal.h>

#include <ardour/curve.h>

#include "canvas_impl.h"
#include "public_editor.h"
#include "automation_pan_line.h"
#include "utils.h"
#include <cmath>

using namespace ARDOUR;
using namespace PBD;

AutomationPanLine::AutomationPanLine (const string & name, TimeAxisView& tv, ArdourCanvas::Group& parent, boost::shared_ptr<AutomationList> l)

	: AutomationLine (name, tv, parent, l)
{
}

void
AutomationPanLine::view_to_model_y (double& y)
{
	// vertical coordinate axis reversal
	y = 1.0 - y;
}

void
AutomationPanLine::model_to_view_y (double& y)
{
	// vertical coordinate axis reversal
	y = 1.0 - y;
}


