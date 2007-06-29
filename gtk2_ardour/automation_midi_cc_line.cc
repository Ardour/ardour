/*
    Copyright (C) 2000-2007 Paul Davis 

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

#include "automation_midi_cc_line.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

AutomationMidiCCLine::AutomationMidiCCLine (const string & name, TimeAxisView& tv, ArdourCanvas::Group& parent, boost::shared_ptr<AutomationList> l)

	: AutomationLine (name, tv, parent, l)
{
	set_verbose_cursor_uses_gain_mapping (true);
}

void
AutomationMidiCCLine::view_to_model_y (double& y)
{
	assert(y >=	0);
	assert(y <= 1);
	
	y = (int)(y * 127.0);
	
	assert(y >=	0);
	assert(y <= 127);
}

void
AutomationMidiCCLine::model_to_view_y (double& y)
{
	assert(y >=	0);
	assert(y <= 127);
	
	y = y / 127.0;
	
	assert(y >=	0);
	assert(y <= 1);
}

string
AutomationMidiCCLine::get_verbose_cursor_string (float fraction)
{
	static const size_t MAX_VAL_LEN = 4; // 4 for "127\0"
	char buf[MAX_VAL_LEN];

	double cc_val = fraction;
	view_to_model_y(cc_val); // 0..127

	snprintf (buf, MAX_VAL_LEN, "%u", (unsigned)cc_val);

	return buf;
}



