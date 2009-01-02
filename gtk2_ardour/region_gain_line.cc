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

#include <evoral/Curve.hpp>
#include <ardour/audioregion.h>
#include <pbd/memento_command.h>

#include "region_gain_line.h"
#include "audio_region_view.h"
#include "utils.h"

#include "time_axis_view.h"
#include "editor.h"

#include <ardour/session.h>


#include "i18n.h"


using namespace std;
using namespace ARDOUR;
using namespace PBD;

AudioRegionGainLine::AudioRegionGainLine (const string & name, Session& s, AudioRegionView& r, ArdourCanvas::Group& parent, boost::shared_ptr<AutomationList> l)
  : AutomationLine (name, r.get_time_axis_view(), parent, l),
	  session (s),
	  rv (r)
{
	// If this isn't true something is horribly wrong, and we'll get catastrophic gain values
	assert(l->parameter().type() == EnvelopeAutomation);

	group->raise_to_top ();
	set_uses_gain_mapping (true);
	terminal_points_can_slide = false;
}

void
AudioRegionGainLine::start_drag (ControlPoint* cp, nframes_t x, float fraction) 
{
	AutomationLine::start_drag (cp, x, fraction);
	if (!rv.audio_region()->envelope_active()) {
		trackview.session().add_command(new MementoCommand<AudioRegion>(*(rv.audio_region().get()), &rv.audio_region()->get_state(), 0));
		rv.audio_region()->set_envelope_active(false);
	} 
}

// This is an extended copy from AutomationList
void
AudioRegionGainLine::remove_point (ControlPoint& cp)
{
	ModelRepresentation mr;

	model_representation (cp, mr);

	trackview.editor().current_session()->begin_reversible_command (_("remove control point"));
	XMLNode &before = alist->get_state();

	if (!rv.audio_region()->envelope_active()) {
		XMLNode &region_before = rv.audio_region()->get_state();
		rv.audio_region()->set_envelope_active(true);
		XMLNode &region_after = rv.audio_region()->get_state();
		trackview.session().add_command(new MementoCommand<AudioRegion>(*(rv.audio_region().get()), &region_before, &region_after));
	} 
	
	alist->erase (mr.start, mr.end);

	trackview.editor().current_session()->add_command (new MementoCommand<AutomationList>(*alist.get(), &before, &alist->get_state()));
	trackview.editor().current_session()->commit_reversible_command ();
	trackview.editor().current_session()->set_dirty ();
}

void
AudioRegionGainLine::end_drag (ControlPoint* cp) 
{
	if (!rv.audio_region()->envelope_active()) {
		rv.audio_region()->set_envelope_active(true);
		trackview.session().add_command(new MementoCommand<AudioRegion>(*(rv.audio_region().get()), 0, &rv.audio_region()->get_state()));
	} 

	AutomationLine::end_drag(cp);
}

