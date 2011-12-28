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

#include "ardour/region.h"
#include <gtkmm2ext/doi.h>

#include "canvas-simplerect.h"
#include "canvas-curve.h"
#include "crossfade_view.h"
#include "global_signals.h"
#include "gui_thread.h"
#include "rgb_macros.h"
#include "audio_time_axis.h"
#include "public_editor.h"
#include "audio_region_view.h"
#include "utils.h"
#include "canvas_impl.h"
#include "ardour_ui.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace Gnome;
using namespace Canvas;

PBD::Signal1<void,CrossfadeView*> CrossfadeView::CatchDeletion;

CrossfadeView::CrossfadeView (ArdourCanvas::Group *parent,
			      RouteTimeAxisView &tv,
			      boost::shared_ptr<Crossfade> xf,
			      double spu,
			      Gdk::Color& basic_color,
			      AudioRegionView& lview,
			      AudioRegionView& rview)


	: TimeAxisViewItem ("xfade" /*xf.name()*/, *parent, tv, spu, basic_color, xf->position(),
			    xf->length(), false, false, TimeAxisViewItem::Visibility (TimeAxisViewItem::ShowFrame)),
	  crossfade (xf),
	  left_view (lview),
	  right_view (rview),
	  _all_in_view (false)
{
	_valid = true;
	_visible = true;

	fade_in = new Line (*group);
	fade_in->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_CrossfadeLine.get();
	fade_in->property_width_pixels() = 1;

	fade_out = new Line (*group);
	fade_out->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_CrossfadeLine.get();
	fade_out->property_width_pixels() = 1;

	/* no frame around the xfade or overlap rects */

	frame->property_outline_what() = 0;

	/* never show the vestigial frame */
	vestigial_frame->hide();
	show_vestigial = false;

	group->signal_event().connect (sigc::bind (sigc::mem_fun (tv.editor(), &PublicEditor::canvas_crossfade_view_event), group, this));

	PropertyChange all_crossfade_properties;
	all_crossfade_properties.add (ARDOUR::Properties::active);
	all_crossfade_properties.add (ARDOUR::Properties::follow_overlap);
	crossfade_changed (all_crossfade_properties);

	crossfade->PropertyChanged.connect (*this, invalidator (*this), ui_bind (&CrossfadeView::crossfade_changed, this, _1), gui_context());
	crossfade->FadesChanged.connect (*this, invalidator (*this), ui_bind (&CrossfadeView::crossfade_fades_changed, this), gui_context());
	ColorsChanged.connect (sigc::mem_fun (*this, &CrossfadeView::color_handler));
}

CrossfadeView::~CrossfadeView ()
{
	 CatchDeletion (this) ; /* EMIT_SIGNAL */
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
CrossfadeView::set_height (double h)
{
	if (h > TimeAxisView::preset_height (HeightSmall)) {
		h -= NAME_HIGHLIGHT_SIZE;
	}

	TimeAxisViewItem::set_height (h);

	redraw_curves ();
}

void
CrossfadeView::crossfade_changed (const PropertyChange& what_changed)
{
	bool need_redraw_curves = false;

	if (what_changed.contains (ARDOUR::bounds_change)) {
		set_position (crossfade->position(), this);
		set_duration (crossfade->length(), this);

		/* set_duration will call reset_width_dependent_items which in turn will call redraw_curves via active_changed,
		   so no need for us to call it */
		need_redraw_curves = false;
	}

	if (what_changed.contains (ARDOUR::Properties::follow_overlap)) {
		need_redraw_curves = true;
	}

	if (what_changed.contains (ARDOUR::Properties::active)) {
		/* calls redraw_curves */
		active_changed ();
	} else if (need_redraw_curves) {
		redraw_curves ();
	}
}

/** Set up our fade_in and fade_out curves to contain points for the currently visible portion
 *  of the crossfade.
 */
void
CrossfadeView::redraw_curves ()
{
	if (!crossfade->following_overlap()) {
		/* curves should not be visible */
		fade_in->hide ();
		fade_out->hide ();
		return;
	}

	if (_height < 0) {
		/* no space allocated yet */
		return;
	}

	PublicEditor& editor = get_time_axis_view().editor ();

	framepos_t const editor_left = editor.leftmost_position ();
	framepos_t const editor_right = editor_left + editor.current_page_frames ();
	framepos_t const xfade_left = crossfade->position ();
	framepos_t const xfade_right = xfade_left + crossfade->length ();

	/* Work out the range of our frames that are visible */
	framepos_t const min_frames = std::max (editor_left, xfade_left);
	framepos_t const max_frames = std::min (editor_right, xfade_right);

	_all_in_view = (editor_left <= xfade_left && editor_right >= xfade_right);

	/* Hence the number of points that we will render */
	int32_t const npoints = editor.frame_to_pixel (max_frames - min_frames);

	if (!_visible || !crossfade->active() || npoints < 3) {
		fade_in->hide();
		fade_out->hide();
		return;
	} else {
		fade_in->show();
		fade_out->show();
	}

	Points* points = get_canvas_points ("xfade edit redraw", npoints);
	float* vec = new float[npoints];

	crossfade->fade_in().curve().get_vector (min_frames - crossfade->position(), max_frames - crossfade->position(), vec, npoints);

	/* Work out the offset from the start of the crossfade to the visible part, in pixels */
	double xoff = 0;
	if (crossfade->position() < editor.leftmost_position()) {
		xoff = editor.frame_to_pixel (min_frames) - editor.frame_to_pixel (crossfade->position ());
	}

	for (int i = 0, pci = 0; i < npoints; ++i) {
		Art::Point &p = (*points)[pci++];
		p.set_x (xoff + i + 1);
		p.set_y (_height - ((_height - 2) * vec[i]));
	}

	fade_in->property_points() = *points;

	crossfade->fade_out().curve().get_vector (min_frames - crossfade->position(), max_frames - crossfade->position(), vec, npoints);

	for (int i = 0, pci = 0; i < npoints; ++i) {
		Art::Point &p = (*points)[pci++];
		p.set_x (xoff + i + 1);
		p.set_y (_height - ((_height - 2) * vec[i]));
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

void
CrossfadeView::show ()
{
	_visible = true;
	group->show();
	redraw_curves ();
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

void
CrossfadeView::crossfade_fades_changed ()
{
	redraw_curves ();
}

void
CrossfadeView::horizontal_position_changed ()
{
	/* If the crossfade curves are entirely within the editor's visible space, there is
	   no need to redraw them here as they will be completely drawn (as distinct from
	   the other case where the horizontal position change will uncover `undrawn'
	   sections).
	*/

	if (!_all_in_view) {
		redraw_curves ();
	}
}
