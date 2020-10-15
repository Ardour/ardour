/*
 * Copyright (C) 2005-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2015 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#include "evoral/Curve.h"
#include "pbd/memento_command.h"
#include "pbd/stateful_diff_command.h"

#include "ardour/audioregion.h"
#include "ardour/session.h"

#include "control_point.h"
#include "region_gain_line.h"
#include "audio_region_view.h"

#include "time_axis_view.h"
#include "editor.h"
#include "gui_thread.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

AudioRegionGainLine::AudioRegionGainLine (const string & name, AudioRegionView& r, ArdourCanvas::Container& parent, boost::shared_ptr<AutomationList> l)
	: AutomationLine (name, r.get_time_axis_view(), parent, l, l->parameter(), Temporal::DistanceMeasure (r.region()->nt_position()))
	, rv (r)
{
	// If this isn't true something is horribly wrong, and we'll get catastrophic gain values
	assert(l->parameter().type() == EnvelopeAutomation);

	r.region()->PropertyChanged.connect (_region_changed_connection, invalidator (*this), boost::bind (&AudioRegionGainLine::region_changed, this, _1), gui_context());

	group->raise_to_top ();
	group->set_y_position (2);
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
	trackview.editor().begin_reversible_command (_("remove control point"));
	XMLNode &before = alist->get_state();

	if (!rv.audio_region()->envelope_active()) {
		rv.audio_region()->clear_changes ();
		rv.audio_region()->set_envelope_active(true);
		trackview.session()->add_command(new StatefulDiffCommand (rv.audio_region()));
	}

	trackview.editor ().get_selection ().clear_points ();
	alist->erase (cp.model());

	trackview.editor().session()->add_command (new MementoCommand<AutomationList>(*alist.get(), &before, &alist->get_state()));
	trackview.editor().commit_reversible_command ();
	trackview.editor().session()->set_dirty ();
}

void
AudioRegionGainLine::end_drag (bool with_push, uint32_t final_index)
{
	if (!rv.audio_region()->envelope_active()) {
		rv.audio_region()->set_envelope_active(true);
		trackview.session()->add_command(new MementoCommand<AudioRegion>(*(rv.audio_region().get()), 0, &rv.audio_region()->get_state()));
	}

	AutomationLine::end_drag (with_push, final_index);
}

void
AudioRegionGainLine::region_changed (const PropertyChange& what_changed)
{
	PropertyChange interesting_stuff;

	interesting_stuff.add (ARDOUR::Properties::start);
	interesting_stuff.add (ARDOUR::Properties::position);

	if (what_changed.containts (ARDOUR::Properties::position)) {
		set_distance_measure_origin (rv.region()->nt_position());
	}

	if (what_changed.contains (interesting_stuff)) {
		reset ();
	}
}
