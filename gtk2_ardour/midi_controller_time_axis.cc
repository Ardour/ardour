/*
    Copyright (C) 2003 Paul Davis 

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

#include <ardour/automation_event.h>
#include <ardour/route.h>
#include <pbd/memento_command.h>

#include "midi_controller_time_axis.h"
#include "automation_line.h"
#include "canvas.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;

MidiControllerTimeAxisView::MidiControllerTimeAxisView (Session& s, boost::shared_ptr<Route> r, 
							PublicEditor& e, TimeAxisView& parent, 
							ArdourCanvas::Canvas& canvas, const string & n,
							ParamID param, ARDOUR::AutomationList& l)

	: AxisView (s),
	  AutomationTimeAxisView (s, r, e, parent, canvas, n, param.to_string(), ""),
	  _list (l),
	  _param (param)
{
}

MidiControllerTimeAxisView::~MidiControllerTimeAxisView ()
{
}

void
MidiControllerTimeAxisView::add_automation_event (ArdourCanvas::Item* item, GdkEvent* event, nframes_t when, double y)
{
	double x = 0;

	canvas_display->w2i (x, y);

	/* compute vertical fractional position */

	y = 1.0 - (y / height);

	/* map using line */

	lines.front()->view_to_model_y (y);

	_session.begin_reversible_command (_("add midi controller automation event"));
	XMLNode& before = _list.get_state();
	_list.add (when, y);
	XMLNode& after = _list.get_state();
	_session.commit_reversible_command (new MementoCommand<ARDOUR::AutomationList>(_list, &before, &after));
	_session.set_dirty ();
}

void
MidiControllerTimeAxisView::set_automation_state (AutoState state)
{
	if (!ignore_state_request) {
		cerr << "FIXME: set midi controller automation state" << endl;
		//route->set_midi_controller_state (state);
	}
}

