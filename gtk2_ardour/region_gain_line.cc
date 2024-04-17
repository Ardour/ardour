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

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;

AudioRegionGainLine::AudioRegionGainLine (const string & name, AudioRegionView& r, ArdourCanvas::Container& parent, std::shared_ptr<AutomationList> l)
	: RegionFxLine (name, r, parent, l, l->parameter ())
	, arv (r)
{

	terminal_points_can_slide = false;
}

void
AudioRegionGainLine::start_drag_single (ControlPoint* cp, double x, float fraction)
{
	RegionFxLine::start_drag_single (cp, x, fraction);

	// XXX Stateful need to capture automation curve data

	if (!arv.audio_region()->envelope_active()) {
		trackview.session()->add_command(new MementoCommand<AudioRegion>(*(arv.audio_region().get()), &arv.audio_region()->get_state(), 0));
		arv.audio_region()->set_envelope_active(false);
	}
}

void
AudioRegionGainLine::start_drag_line (uint32_t i1, uint32_t i2, float fraction)
{
	RegionFxLine::start_drag_line (i1, i2, fraction);

	if (!arv.audio_region()->envelope_active()) {
		trackview.session()->add_command(new MementoCommand<AudioRegion>(*(arv.audio_region().get()), &arv.audio_region()->get_state(), 0));
		arv.audio_region()->set_envelope_active(false);
	}
}

void
AudioRegionGainLine::start_drag_multiple (list<ControlPoint*> cp, float fraction, XMLNode* state)
{
	RegionFxLine::start_drag_multiple (cp, fraction, state);

	if (!arv.audio_region()->envelope_active()) {
		trackview.session()->add_command(new MementoCommand<AudioRegion>(*(arv.audio_region().get()), &arv.audio_region()->get_state(), 0));
		arv.audio_region()->set_envelope_active(false);
	}
}

// This is an extended copy from AutomationList
void
AudioRegionGainLine::remove_point (ControlPoint& cp)
{
	trackview.editor().begin_reversible_command (_("remove control point"));
	XMLNode &before = alist->get_state();

	if (!arv.audio_region()->envelope_active()) {
		arv.audio_region()->clear_changes ();
		arv.audio_region()->set_envelope_active(true);
		trackview.session()->add_command(new PBD::StatefulDiffCommand (arv.audio_region()));
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
	if (!arv.audio_region()->envelope_active()) {
		arv.audio_region()->set_envelope_active(true);
		trackview.session()->add_command(new MementoCommand<AudioRegion>(*(arv.audio_region().get()), 0, &arv.audio_region()->get_state()));
	}

	RegionFxLine::end_drag (with_push, final_index);
}

void
AudioRegionGainLine::end_draw_merge ()
{
	enable_autoation ();
	RegionFxLine::end_draw_merge ();
}

void
AudioRegionGainLine::enable_autoation ()
{
	if (!arv.audio_region()->envelope_active()) {
		XMLNode& before = arv.audio_region()->get_state();
		arv.audio_region()->set_envelope_active(true);
		trackview.session()->add_command(new MementoCommand<AudioRegion>(*(arv.audio_region().get()), &before, &arv.audio_region()->get_state()));
	}
}
