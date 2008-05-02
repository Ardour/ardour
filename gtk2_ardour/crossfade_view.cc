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

#include <algorithm>

#include <ardour/region.h>
#include <gtkmm2ext/doi.h>

#include "canvas-simplerect.h"
#include "canvas-curve.h"
#include "crossfade_view.h"
#include "rgb_macros.h"
#include "audio_time_axis.h"
#include "public_editor.h"
#include "audio_region_view.h"
#include "utils.h"
#include "canvas_impl.h"
#include "ardour_ui.h"

using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace Gnome;
using namespace Canvas;

sigc::signal<void,CrossfadeView*> CrossfadeView::GoingAway;

CrossfadeView::CrossfadeView (ArdourCanvas::Group *parent, 
			      RouteTimeAxisView &tv, 
			      boost::shared_ptr<Crossfade> xf, 
			      double spu,
			      Gdk::Color& basic_color,
			      AudioRegionView& lview,
			      AudioRegionView& rview)
			      

	: TimeAxisViewItem ("xfade" /*xf.name()*/, *parent, tv, spu, basic_color, xf->position(), 
			    xf->length(), false, TimeAxisViewItem::Visibility (TimeAxisViewItem::ShowFrame)),
	  crossfade (xf),
	  left_view (lview),
	  right_view (rview)
	
{
	_valid = true;
	_visible = true;

	fade_in = new Line (*group);
	fade_in->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_CrossfadeLine.get();
	fade_in->property_width_pixels() = 1;

	fade_out = new Line (*group);
	fade_out->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_CrossfadeLine.get();
	fade_out->property_width_pixels() = 1;
	
	set_height (get_time_axis_view().current_height());

	/* no frame around the xfade or overlap rects */

	frame->property_outline_what() = 0;

	/* never show the vestigial frame */

	vestigial_frame->hide();
	show_vestigial = false;
	
	group->signal_event().connect (bind (mem_fun (tv.editor, &PublicEditor::canvas_crossfade_view_event), group, this));
	
	crossfade_changed (Change (~0));

	crossfade->StateChanged.connect (mem_fun(*this, &CrossfadeView::crossfade_changed));
	ColorsChanged.connect (mem_fun (*this, &CrossfadeView::color_handler));
}

CrossfadeView::~CrossfadeView ()
{
	GoingAway (this) ; /* EMIT_SIGNAL */
}

void
CrossfadeView::reset_width_dependent_items (double pixel_width)
{
	TimeAxisViewItem::reset_width_dependent_items (pixel_width);

	active_changed ();

	if (pixel_width < 5) {
		fade_in->hide();
		fade_out->hide();
	}
}

void
CrossfadeView::set_height (double height)
{
	if (height <= TimeAxisView::hSmaller) {
		TimeAxisViewItem::set_height (height - 3);
	} else {
		TimeAxisViewItem::set_height (height - NAME_HIGHLIGHT_SIZE - 3 );
	}

	redraw_curves ();
}

void
CrossfadeView::crossfade_changed (Change what_changed)
{
	bool need_redraw_curves = false;

	if (what_changed & BoundsChanged) {
		set_position (crossfade->position(), this);
		set_duration (crossfade->length(), this);
		need_redraw_curves = true;
	}

	if (what_changed & Crossfade::FollowOverlapChanged) {
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
	Points* points; 
	int32_t npoints;
	float* vec;
	double h;

	if (!crossfade->following_overlap()) {
		/* curves should not be visible */
		fade_in->hide ();
		fade_out->hide ();
		return;
	}

	/*
	 At "height - 3.0" the bottom of the crossfade touches the name highlight or the bottom of the track (if the
	 track is either Small or Smaller.
	 */
	double tav_height = get_time_axis_view().current_height();
	if (tav_height == TimeAxisView::hSmaller ||
	    tav_height == TimeAxisView::hSmall) {
		h = tav_height - 3.0;
	} else {
		h = tav_height - NAME_HIGHLIGHT_SIZE - 3.0;
	}

	if (h < 0) {
		/* no space allocated yet */
		return;
	}

	npoints = get_time_axis_view().editor.frame_to_pixel (crossfade->length());
	npoints = std::min (gdk_screen_width(), npoints);

	if (!_visible || !crossfade->active() || npoints < 3) {
		fade_in->hide();
		fade_out->hide();
		return;
	} else {
		fade_in->show();
		fade_out->show();
	} 

	points = get_canvas_points ("xfade edit redraw", npoints);
	vec = new float[npoints];

	crossfade->fade_in().get_vector (0, crossfade->length(), vec, npoints);
	for (int i = 0, pci = 0; i < npoints; ++i) {
		Art::Point &p = (*points)[pci++];
		p.set_x(i);
		p.set_y(2.0 + h - (h * vec[i]));
	}
	fade_in->property_points() = *points;

	crossfade->fade_out().get_vector (0, crossfade->length(), vec, npoints);
	for (int i = 0, pci = 0; i < npoints; ++i) {
		Art::Point &p = (*points)[pci++];
		p.set_x(i);
		p.set_y(2.0 + h - (h * vec[i]));
	}
	fade_out->property_points() = *points;

	delete [] vec;

	delete points;

	/* XXX this is ugly, but it will have to wait till Crossfades are reimplented
	   as regions. This puts crossfade views on top of a track, above all regions.
	*/

	group->raise_to_top();
}

void
CrossfadeView::active_changed ()
{
	if (crossfade->active()) {
		frame->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_ActiveCrossfade.get();
	} else {
		frame->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_InactiveCrossfade.get();
	}

	redraw_curves ();
}

void
CrossfadeView::color_handler ()
{
	active_changed ();
}

void
CrossfadeView::set_valid (bool yn)
{
	_valid = yn;
}

AudioRegionView&
CrossfadeView::upper_regionview () const
{
	if (left_view.region()->layer() > right_view.region()->layer()) {
		return left_view;
	} else {
		return right_view;
	}
}

void
CrossfadeView::show ()
{
	group->show();
	_visible = true;
}

void
CrossfadeView::hide ()
{
	group->hide();
	_visible = false;
}

void
CrossfadeView::fake_hide ()
{
	group->hide();
}
