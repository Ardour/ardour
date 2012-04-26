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

#include "evoral/Curve.hpp"
#include "pbd/memento_command.h"
#include "pbd/stateful_diff_command.h"

#include "ardour/audioregion.h"
#include "ardour/session.h"

#include "control_point.h"
#include "region_gain_line.h"
#include "audio_region_view.h"
#include "utils.h"

#include "time_axis_view.h"
#include "editor.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

AudioRegionGainLine::AudioRegionGainLine (const string & name, AudioRegionView& r, ArdourCanvas::Group& parent, boost::shared_ptr<AutomationList> l)
	: AutomationLine (name, r.get_time_axis_view(), parent, l)
	, rv (r)
{
	// If this isn't true something is horribly wrong, and we'll get catastrophic gain values
	assert(l->parameter().type() == EnvelopeAutomation);

	_time_converter->set_origin_b (r.region()->position() - r.region()->start());

	group->raise_to_top ();
	group->property_y() = 2;
	set_uses_gain_mapping (true);
	terminal_points_can_slide = false;
}

void
AudioRegionGainLine::start_drag_single (ControlPoint* cp, double x, float fraction)
{
	AutomationLine::start_drag_single (cp, x, fraction);

        // XXX Stateful need to capture automation curve data

	if (!rv.audio_region()->envelope_active()) {
		trackview.session()->add_command(new MementoCommand<AudioRegion>(*(rv.audio_region().get()), &rv.audio_region()->get_state(), 0));
		rv.audio_region()->set_envelope_active(false);
	}
}

// This is an extended copy from AutomationList
void
AudioRegionGainLine::remove_point (ControlPoint& cp)
{
	trackview.editor().session()->begin_reversible_command (_("remove control point"));
	XMLNode &before = alist->get_state();

	if (!rv.audio_region()->envelope_active()) {
                rv.audio_region()->clear_changes ();
		rv.audio_region()->set_envelope_active(true);
		trackview.session()->add_command(new StatefulDiffCommand (rv.audio_region()));
	}

	alist->erase (cp.model());

	trackview.editor().session()->add_command (new MementoCommand<AutomationList>(*alist.get(), &before, &alist->get_state()));
	trackview.editor().session()->commit_reversible_command ();
	trackview.editor().session()->set_dirty ();
}

void
AudioRegionGainLine::end_drag ()
{
	if (!rv.audio_region()->envelope_active()) {
		rv.audio_region()->set_envelope_active(true);
		trackview.session()->add_command(new MementoCommand<AudioRegion>(*(rv.audio_region().get()), 0, &rv.audio_region()->get_state()));
	}

	AutomationLine::end_drag ();
}

