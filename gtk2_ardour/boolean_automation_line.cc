/*
    Copyright (C) 2007 Paul Davis 

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

#include "boolean_automation_line.h"
#include "time_axis_view.h"
#include "public_editor.h"

using namespace std;
using namespace sigc;
using namespace ARDOUR;
using namespace PBD;

void
BooleanAutomationLine::model_to_view_y (double& y)
{
	if (y > 0.5) {
		y = 1.0;
	} else {
		y = 0.0;
	}
}

void
BooleanAutomationLine::view_to_model_y (double& y)
{
	if (y > 0.5) {
		y = 1.0;
	} else {
		y = 0.0;
	}
}

void
BooleanAutomationLine::add_model_point (AutomationLine::ALPoints& tmp_points, double frame, double yfract)
{
	/* add two points, one to represent "on" and one to represent "off" */

	double x = trackview.editor.frame_to_unit (frame);

	tmp_points.push_back (ALPoint (x > 0 ? x - 1 : 0, _height > 4 ? 2 : 0));
	tmp_points.push_back (ALPoint (x + 1, _height > 2 ? _height - 2 : _height));
}

