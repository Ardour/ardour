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

    $Id$
*/

#include <algorithm>

#include <ardour/region.h>
#include <gtkmm2ext/doi.h>

#include "canvas-simplerect.h"
#include "canvas-curve.h"
#include "crossfade_view.h"
#include "rgb_macros.h"
#include "audio_time_axis.h"
#include "public_editor.h"
#include "regionview.h"
#include "utils.h"

using namespace sigc;
using namespace ARDOUR;
using namespace Editing;

sigc::signal<void,CrossfadeView*> CrossfadeView::GoingAway;

CrossfadeView::CrossfadeView (GtkCanvasGroup *parent, 
			      AudioTimeAxisView &tv, 
			      Crossfade& xf, 
			      double spu,
			      GdkColor& basic_color,
			      AudioRegionView& lview,
			      AudioRegionView& rview)
			      

	: TimeAxisViewItem ("xf.name()", parent, tv, spu, basic_color, xf.position(), 
			    xf.overlap_length(), TimeAxisViewItem::Visibility (TimeAxisViewItem::ShowFrame)),
	  crossfade (xf),
	  left_view (lview),
	  right_view (rview)
	
{
	_valid = true;
	_visible = true;

	fade_in = gtk_canvas_item_new (GTK_CANVAS_GROUP(group),
				       gtk_canvas_line_get_type(),
				       "fill_color_rgba", color_map[cCrossfadeLine],
					"width_pixels", (guint) 1,
				       NULL);

	fade_out = gtk_canvas_item_new (GTK_CANVAS_GROUP(group),
					gtk_canvas_line_get_type(),
					"fill_color_rgba", color_map[cCrossfadeLine],
					"width_pixels", (guint) 1,
					NULL);
	
	set_height (get_time_axis_view().height);

	/* no frame around the xfade or overlap rects */

	gtk_canvas_item_set (frame, "outline_what", 0, NULL);

	/* never show the vestigial frame */

	gtk_canvas_item_hide (vestigial_frame);
	show_vestigial = false;

	gtk_object_set_data (GTK_OBJECT(group), "crossfadeview", this);
	gtk_signal_connect (GTK_OBJECT(group), "event",
			    (GtkSignalFunc) PublicEditor::canvas_crossfade_view_event,
			    this);

	crossfade_changed (Change (~0));

	crossfade.StateChanged.connect (slot (*this, &CrossfadeView::crossfade_changed));
}

CrossfadeView::~CrossfadeView ()
{
	 GoingAway (this) ; /* EMIT_SIGNAL */
}

std::string
CrossfadeView::get_item_name ()
{
	return "xfade";
//	return crossfade.name();
}

void
CrossfadeView::reset_width_dependent_items (double pixel_width)
{
	TimeAxisViewItem::reset_width_dependent_items (pixel_width);

	active_changed ();

	if (pixel_width < 5) {
		gtk_canvas_item_hide (fade_in);
		gtk_canvas_item_hide (fade_out);
	}
}

void
CrossfadeView::set_height (double height)
{
	if (height == TimeAxisView::Smaller ||
		height == TimeAxisView::Small)
		TimeAxisViewItem::set_height (height - 3 );
	else
		TimeAxisViewItem::set_height (height - NAME_HIGHLIGHT_SIZE - 3 );

	redraw_curves ();
}

void
CrossfadeView::crossfade_changed (Change what_changed)
{
	bool need_redraw_curves = false;

	if (what_changed & BoundsChanged) {
		set_position (crossfade.position(), this);
		set_duration (crossfade.overlap_length(), this);
		need_redraw_curves = true;
	}
	
	if (what_changed & Crossfade::ActiveChanged) {
		/* calls redraw_curves */
		active_changed ();
	} else if (need_redraw_curves) {
		redraw_curves ();
	}
}

void
CrossfadeView::redraw_curves ()
{
	GtkCanvasPoints* points; 
	int32_t npoints;
	float* vec;
	
	double h;

	/*
	 At "height - 3.0" the bottom of the crossfade touches the name highlight or the bottom of the track (if the
	 track is either Small or Smaller.
	 */
	switch(get_time_axis_view().height) {
		case TimeAxisView::Smaller:
		case TimeAxisView::Small:
			h = get_time_axis_view().height - 3.0;
			break;

		default:
			h = get_time_axis_view().height - NAME_HIGHLIGHT_SIZE - 3.0;
	}

	if (h < 0) {
		/* no space allocated yet */
		return;
	}

	npoints = get_time_axis_view().editor.frame_to_pixel (crossfade.length());
	npoints = std::min (gdk_screen_width(), npoints);

	if (!_visible || !crossfade.active() || npoints < 3) {
		gtk_canvas_item_hide (fade_in);
		gtk_canvas_item_hide (fade_out);
		return;
	} else {
		gtk_canvas_item_show (fade_in);
		gtk_canvas_item_show (fade_out);
	} 

	points = get_canvas_points ("xfade edit redraw", npoints);
	vec = new float[npoints];

	crossfade.fade_in().get_vector (0, crossfade.length(), vec, npoints);
	for (int i = 0, pci = 0; i < npoints; ++i) {
		points->coords[pci++] = i;
		points->coords[pci++] = 2.0 + h - (h * vec[i]);
	}
	gtk_canvas_item_set (fade_in, "points", points, NULL);

	crossfade.fade_out().get_vector (0, crossfade.length(), vec, npoints);
	for (int i = 0, pci = 0; i < npoints; ++i) {
		points->coords[pci++] = i;
		points->coords[pci++] = 2.0 + h - (h * vec[i]);
	}
	gtk_canvas_item_set (fade_out, "points", points, NULL);

	delete [] vec;

	gtk_canvas_points_unref (points);

	/* XXX this is ugly, but it will have to wait till Crossfades are reimplented
	   as regions. This puts crossfade views on top of a track, above all regions.
	*/

	gtk_canvas_item_raise_to_top (group);
}

void
CrossfadeView::active_changed ()
{
	if (crossfade.active()) {
		gtk_canvas_item_set (frame, "fill_color_rgba", color_map[cActiveCrossfade], NULL);
	} else {
		gtk_canvas_item_set (frame, "fill_color_rgba", color_map[cInactiveCrossfade], NULL);
	}

	redraw_curves ();
}

void
CrossfadeView::set_valid (bool yn)
{
	_valid = yn;
}

AudioRegionView&
CrossfadeView::upper_regionview () const
{
	if (left_view.region.layer() > right_view.region.layer()) {
		return left_view;
	} else {
		return right_view;
	}
}

void
CrossfadeView::show ()
{
	gtk_canvas_item_show (group);
	_visible = true;
}

void
CrossfadeView::hide ()
{
	gtk_canvas_item_hide (group);
	_visible = false;
}

void
CrossfadeView::fake_hide ()
{
	gtk_canvas_item_hide (group);
}
