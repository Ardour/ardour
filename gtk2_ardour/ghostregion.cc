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

#include "simplerect.h"
#include "waveview.h"
#include "ghostregion.h"
#include "automation_time_axis.h"
#include "rgb_macros.h"
#include "ardour_ui.h"

using namespace Editing;
using namespace ArdourCanvas;

GhostRegion::GhostRegion (AutomationTimeAxisView& atv, double initial_pos)
	: trackview (atv)
{
  //group = gnome_canvas_item_new (GNOME_CANVAS_GROUP(trackview.canvas_display),
  //			     gnome_canvas_group_get_type(),
  //			     "x", initial_pos,
  //			     "y", 0.0,
  //			     NULL);
	group = new ArdourCanvas::Group (*trackview.canvas_display);
	group->property_x() = initial_pos;
	group->property_y() = 0.0;

	base_rect = new ArdourCanvas::SimpleRect (*group);
	base_rect->property_x1() = (double) 0.0;
	base_rect->property_y1() = (double) 0.0;
	base_rect->property_y2() = (double) trackview.current_height();
	base_rect->property_outline_what() = (guint32) 0;
	base_rect->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_GhostTrackBase.get();
	base_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_GhostTrackBase.get();
	group->lower_to_bottom ();

	atv.add_ghost (this);
}

GhostRegion::~GhostRegion ()
{
	GoingAway (this);
	delete base_rect;
	delete group;
}

void
GhostRegion::set_samples_per_unit (double spu)
{
	for (vector<WaveView*>::iterator i = waves.begin(); i != waves.end(); ++i) {
		(*i)->property_samples_per_unit() = spu;
	}		
}

void
GhostRegion::set_duration (double units)
{
        base_rect->property_x2() = units;
}

void
GhostRegion::set_height ()
{
	gdouble ht;
	vector<WaveView*>::iterator i;
	uint32_t n;

	base_rect->property_y2() = (double) trackview.current_height();
	ht = ((trackview.current_height()) / (double) waves.size());
	
	for (n = 0, i = waves.begin(); i != waves.end(); ++i, ++n) {
		gdouble yoff = n * ht;
		(*i)->property_height() = ht;
		(*i)->property_y() = yoff;
	}
}

void
GhostRegion::set_colors ()
{
	base_rect->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_GhostTrackBase.get();
	base_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_GhostTrackBase.get();

    for (uint32_t n=0; n < waves.size(); ++n) {
	waves[n]->property_wave_color() = ARDOUR_UI::config()->canvasvar_GhostTrackWave.get();
	waves[n]->property_fill_color() = ARDOUR_UI::config()->canvasvar_GhostTrackWave.get();

	waves[n]->property_clip_color() = ARDOUR_UI::config()->canvasvar_GhostTrackWaveClip.get();
	waves[n]->property_zero_color() = ARDOUR_UI::config()->canvasvar_GhostTrackZeroLine.get();
    }
}
