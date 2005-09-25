/*
    Copyright (C) 2002-2003 Paul Davis 

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

#include <cmath>
#include <climits>
#include <vector>

#include <pbd/stl_delete.h>

#include <ardour/automation_event.h>
#include <ardour/curve.h>
#include <ardour/dB.h>

#include "canvas-simplerect.h"
#include "automation_line.h"
#include "rgb_macros.h"
#include "ardour_ui.h"
#include "public_editor.h"
#include "utils.h"
#include "selection.h"
#include "time_axis_view.h"
#include "point_selection.h"
#include "automation_selectable.h"
#include "automation_time_axis.h"
#include "public_editor.h"

#include <ardour/session.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace Editing;

ControlPoint::ControlPoint (AutomationLine& al, gint (*event_handler)(GnomeCanvasItem*, GdkEvent*, gpointer))
	: line (al)
{
	model = al.the_list().end();
	view_index = 0;
	can_slide = true;
	_x = 0;
	_y = 0;
	_shape = Full;
	_size = 4.0;
	selected = false;

	item = gnome_canvas_item_new (line.canvas_group(),
				    gnome_canvas_simplerect_get_type(),
				    "draw", (gboolean) TRUE,
				    "fill", (gboolean) FALSE,
				    "fill_color_rgba", color_map[cControlPointFill],
				    "outline_color_rgba", color_map[cControlPointOutline],
				    "outline_pixels", (gint) 1,
				    NULL);

	gtk_object_set_data (GTK_OBJECT(item), "control_point", this);
	gtk_signal_connect (GTK_OBJECT(item), "event", (GtkSignalFunc) event_handler, this);

	hide ();
	set_visible (false);
}

ControlPoint::ControlPoint (const ControlPoint& other, bool dummy_arg_to_force_special_copy_constructor)
	: line (other.line)
{
	if (&other == this) {
		return;
	}

	model = other.model;
	view_index = other.view_index;
	can_slide = other.can_slide;
	_x = other._x;
	_y = other._y;
	_shape = other._shape;
	_size = other._size;
	selected = false;

	item = gnome_canvas_item_new (line.canvas_group(),
				    gnome_canvas_simplerect_get_type(),
				    "fill", (gboolean) FALSE,
				    "outline_color_rgba", color_map[cEnteredControlPointOutline],
				    "outline_pixels", (gint) 1,
				    NULL);
	
	/* NOTE: no event handling in copied ControlPoints */

	hide ();
	set_visible (false);
}

ControlPoint::~ControlPoint ()
{
	gtk_object_destroy (GTK_OBJECT(item));
}

void
ControlPoint::hide ()
{
	gnome_canvas_item_hide (item);
}

void
ControlPoint::show()
{
	gnome_canvas_item_show (item);
}

void
ControlPoint::set_visible (bool yn)
{
	gnome_canvas_item_set (item, "draw", (gboolean) yn, NULL);
}

void
ControlPoint::reset (double x, double y, AutomationList::iterator mi, uint32_t vi, ShapeType shape)
{
	model = mi;
	view_index = vi;
	move_to (x, y, shape);
}

void
ControlPoint::show_color (bool entered, bool hide_too)
{
	if (entered) {
		if (selected) {
			gnome_canvas_item_set (item, "outline_color_rgba", color_map[cEnteredControlPointSelected], NULL);
			set_visible(true);
		} else {
			gnome_canvas_item_set (item, "outline_color_rgba", color_map[cEnteredControlPoint], NULL);
			if (hide_too) {
				set_visible(false);
			}
		}

	} else {
		if (selected) {
			gnome_canvas_item_set (item, "outline_color_rgba", color_map[cControlPointSelected], NULL);
			set_visible(true);
		} else {
			gnome_canvas_item_set (item, "outline_color_rgba", color_map[cControlPoint], NULL);
			if (hide_too) {
				set_visible(false);
			}
		}
	}
}

void
ControlPoint::set_size (double sz)
{
	_size = sz;

#if 0	
	if (_size > 6.0) {
		gnome_canvas_item_set (item, 
				     "fill", (gboolean) TRUE,
				     NULL);
	} else {
		gnome_canvas_item_set (item, 
				     "fill", (gboolean) FALSE,
				     NULL);
	}
#endif

	move_to (_x, _y, _shape);
}

void
ControlPoint::move_to (double x, double y, ShapeType shape)
{
	double x1 = 0;
	double x2 = 0;
	double half_size = rint(_size/2.0);

	switch (shape) {
	case Full:
		x1 = x - half_size;
		x2 = x + half_size;
		break;
	case Start:
		x1 = x;
		x2 = x + half_size;
		break;
	case End:
		x1 = x - half_size;
		x2 = x;
		break;
	}

	gnome_canvas_item_set (item, 
			     "x1", x1,
			     "x2", x2,
			     "y1", y - half_size,
			     "y2", y + half_size,
			     NULL);

	_x = x;
	_y = y;
	_shape = shape;
}

/*****/

AutomationLine::AutomationLine (string name, TimeAxisView& tv, GnomeCanvasItem* parent, AutomationList& al,
				gint (*point_handler)(GnomeCanvasItem*, GdkEvent*, gpointer),
				gint (*line_handler)(GnomeCanvasItem*, GdkEvent*, gpointer))
	: trackview (tv),
	  _name (name),
	  alist (al)
{
	point_coords = 0;
	points_visible = false;
	update_pending = false;
	_vc_uses_gain_mapping = false;
	no_draw = false;
	_visible = true;
	point_callback = point_handler;
	_parent_group = parent;
	terminal_points_can_slide = true;
	_height = 0;

	group = gnome_canvas_item_new (GNOME_CANVAS_GROUP(parent),
				     gnome_canvas_group_get_type(),
				     "x", 0.0,
				     "y", 0.0,
				     NULL);

	line = gnome_canvas_item_new (GNOME_CANVAS_GROUP(group),
					 gnome_canvas_line_get_type(),
					 "width_pixels", (guint) 1,
					 NULL);

	// cerr << _name << " line @ " << line << endl;

	gtk_object_set_data (GTK_OBJECT(line), "line", this);
	gtk_signal_connect (GTK_OBJECT(line), "event", (GtkSignalFunc) line_handler, this);

	alist.StateChanged.connect (mem_fun(*this, &AutomationLine::list_changed));
}

AutomationLine::~AutomationLine ()
{
	if (point_coords) {
		gnome_canvas_points_unref (point_coords);
	}

	vector_delete (&control_points);

	gtk_object_destroy (GTK_OBJECT(group));
}

void
AutomationLine::queue_reset ()
{
	if (!update_pending) {
		update_pending = true;
		Gtkmm2ext::UI::instance()->call_slot (mem_fun(*this, &AutomationLine::reset));
	}
}

void
AutomationLine::set_point_size (double sz)
{
	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		(*i)->set_size (sz);
	}
}	

void
AutomationLine::show () 
{
	gnome_canvas_item_show (line);

	if (points_visible) {
		for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
			(*i)->show ();
		}
	}

	_visible = true;
}

void
AutomationLine::hide () 
{
	gnome_canvas_item_hide (line);
	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		(*i)->hide();
	}
	_visible = false;
}

void
AutomationLine::set_height (guint32 h)
{
	if (h != _height) {
		_height = h;

		if (_height > (guint32) TimeAxisView::Larger) {
			set_point_size (8.0);
		} else if (_height > (guint32) TimeAxisView::Normal) {
			set_point_size (6.0);
		} else {
			set_point_size (4.0);
		}

		reset ();
	}
}

void
AutomationLine::set_line_color (uint32_t color)
{
	_line_color = color;
	gnome_canvas_item_set (line, "fill_color_rgba", color, NULL);
}

void
AutomationLine::set_verbose_cursor_uses_gain_mapping (bool yn)
{
	if (yn != _vc_uses_gain_mapping) {
		_vc_uses_gain_mapping = yn;
		reset ();
	}
}

ControlPoint*
AutomationLine::nth (uint32_t n)
{
	if (n < control_points.size()) {
		return control_points[n];
	} else {
		return 0;
	}
}

void
AutomationLine::modify_view (ControlPoint& cp, double x, double y, bool with_push)
{
	modify_view_point (cp, x, y, with_push);
	update_line ();
}

void
AutomationLine::modify_view_point (ControlPoint& cp, double x, double y, bool with_push)
{
	double delta = 0.0;
	uint32_t last_movable = UINT_MAX;
	double x_limit = DBL_MAX;

	/* this just changes the current view. it does not alter
	   the model in any way at all.
	*/

	/* clamp y-coord appropriately. y is supposed to be a normalized fraction (0.0-1.0),
	   and needs to be converted to a canvas unit distance.
	*/

	y = max (0.0, y);
	y = min (1.0, y);
	y = _height - (y * _height);

	if (cp.can_slide) {

		/* x-coord cannot move beyond adjacent points or the start/end, and is
		   already in frames. it needs to be converted to canvas units.
		*/
		
		x = trackview.editor.frame_to_unit (x);

		/* clamp x position using view coordinates */

		ControlPoint *before;
		ControlPoint *after;

		if (cp.view_index) {
			before = nth (cp.view_index - 1);
			x = max (x, before->get_x()+1.0);
		} else {
			before = &cp;
		}


		if (!with_push) {
			if (cp.view_index < control_points.size() - 1) {
		
				after = nth (cp.view_index + 1);
		
				/*if it is a "spike" leave the x alone */
 
				if (after->get_x() - before->get_x() < 2) {
					x = cp.get_x();
					
				} else {
					x = min (x, after->get_x()-1.0);
				}
			} else {
				after = &cp;
			}

		} else {

			ControlPoint* after;
			
			/* find the first point that can't move */
			
			for (uint32_t n = cp.view_index + 1; (after = nth (n)) != 0; ++n) {
				if (!after->can_slide) {
					x_limit = after->get_x() - 1.0;
					last_movable = after->view_index;
					break;
				}
			}
			
			delta = x - cp.get_x();
		}
			
	} else {

		/* leave the x-coordinate alone */

		x = trackview.editor.frame_to_unit ((*cp.model)->when);

	}

	if (!with_push) {

		cp.move_to (x, y, ControlPoint::Full);
		reset_line_coords (cp);

	} else {

		uint32_t limit = min (control_points.size(), (size_t)last_movable);
		
		/* move the current point to wherever the user told it to go, subject
		   to x_limit.
		*/
		
		cp.move_to (min (x, x_limit), y, ControlPoint::Full);
		reset_line_coords (cp);
		
		/* now move all subsequent control points, to reflect the motion.
		 */
		
		for (uint32_t i = cp.view_index + 1; i < limit; ++i) {
			ControlPoint *p = nth (i);
			double new_x;

			if (p->can_slide) {
				new_x = min (p->get_x() + delta, x_limit);
				p->move_to (new_x, p->get_y(), ControlPoint::Full);
				reset_line_coords (*p);
			}
		}
	}
}

void
AutomationLine::reset_line_coords (ControlPoint& cp)
{	
	if (point_coords) {
		point_coords->coords[cp.view_index*2] = cp.get_x();
		point_coords->coords[(cp.view_index*2) + 1] = cp.get_y();
	}
}

void
AutomationLine::update_line ()
{
	gnome_canvas_item_set (line, "points", point_coords, NULL);	
}

void
AutomationLine::sync_model_with_view_line (uint32_t start, uint32_t end)
{

	ControlPoint *p;

	update_pending = true;

	for (uint32_t i = start; i <= end; ++i) {
		p = nth(i);
		sync_model_with_view_point(*p);
	}
	
	
}

void
AutomationLine::model_representation (ControlPoint& cp, ModelRepresentation& mr)
{
	/* part one: find out where the visual control point is.
	   initial results are in canvas units. ask the
	   line to convert them to something relevant.
	*/
	
	mr.xval = (jack_nframes_t) floor (cp.get_x());
	mr.yval = 1.0 - (cp.get_y() / _height);


        /* if xval has not changed, set it directly from the model to avoid rounding errors */

	if (mr.xval == trackview.editor.frame_to_unit((*cp.model)->when)) {
		mr.xval = (jack_nframes_t) (*cp.model)->when;
	} else {
		mr.xval = trackview.editor.unit_to_frame (mr.xval);
	}


	/* virtual call: this will do the right thing
	   for whatever particular type of line we are.
	*/
	
	view_to_model_y (mr.yval);

	/* part 2: find out where the model point is now
	 */

	mr.xpos = (jack_nframes_t) (*cp.model)->when;
	mr.ypos = (*cp.model)->value;

	/* part 3: get the position of the visual control
	   points before and after us.
	*/

	ControlPoint* before;
	ControlPoint* after;

	if (cp.view_index) {
		before = nth (cp.view_index - 1);
	} else {
		before = 0;
	}

	after = nth (cp.view_index + 1);

	if (before) {
		mr.xmin = (jack_nframes_t) (*before->model)->when;
		mr.ymin = (*before->model)->value;
		mr.start = before->model;
		++mr.start;
	} else {
		mr.xmin = mr.xpos;
		mr.ymin = mr.ypos;
		mr.start = cp.model;
	}

	if (after) {
		mr.end = after->model;
	} else {
		mr.xmax = mr.xpos;
		mr.ymax = mr.ypos;
		mr.end = cp.model;
		++mr.end;
	}
}

void
AutomationLine::sync_model_from (ControlPoint& cp)
{
	ControlPoint* p;
	uint32_t lasti;

	sync_model_with_view_point (cp);

	/* we might have moved all points after `cp' by some amount
	   if we pressed the with_push modifyer some of the time during the drag
	   so all subsequent points have to be resynced
	*/

	lasti = control_points.size() - 1;
	p = nth (lasti);

	update_pending = true;

	while (p != &cp && lasti) {
		sync_model_with_view_point (*p);
		--lasti;
		p = nth (lasti);
	}
}

void
AutomationLine::sync_model_with_view_point (ControlPoint& cp)
{
	ModelRepresentation mr;
	double ydelta;

	model_representation (cp, mr);

	/* part 4: how much are we changing the central point by */ 

	ydelta = mr.yval - mr.ypos;

	/* IMPORTANT: changing the model when the x-coordinate changes
	   may invalidate the iterators that we are using. this means that we have
	   to change the points before+after the one corresponding to the visual CP
	   first (their x-coordinate doesn't change). then we change the
	   "main" point.

	   apply the full change to the central point, and interpolate
	   in each direction to cover all model points represented
	   by the control point.
	*/

	/* part 5: change all points before the primary point */

	for (AutomationList::iterator i = mr.start; i != cp.model; ++i) {

		double delta;

		delta = ydelta * ((*i)->when - mr.xmin) / (mr.xpos - mr.xmin);

		/* x-coordinate (generally time) stays where it is,
		   y-coordinate moves by a certain amount.
		*/
		
		update_pending = true;
		change_model (i, (*i)->when, mr.yval + delta);
	}

	/* part 6: change later points */

	AutomationList::iterator i = cp.model;

	++i;

	while (i != mr.end) {

		double delta;
		
		delta = ydelta * (mr.xmax - (*i)->when) / (mr.xmax - mr.xpos);

		/* x-coordinate (generally time) stays where it is,
		   y-coordinate moves by a certain amount.
		*/
		
		update_pending = true;
		change_model (i, (*i)->when, (*i)->value + delta);
		
		++i;
	}

	/* part 7: change the primary point */

	update_pending = true;
	change_model (cp.model, mr.xval, mr.yval);
}

void
AutomationLine::determine_visible_control_points (GnomeCanvasPoints* points)
{
	uint32_t xi, yi, view_index, pi;
	int n;
	AutomationList::iterator model;
	uint32_t npoints = points->num_points;
	double last_control_point_x = 0.0;
	double last_control_point_y = 0.0;
	uint32_t this_rx = 0;
	uint32_t prev_rx = 0;
	uint32_t this_ry = 0;
 	uint32_t prev_ry = 0;	
	double* slope;
	
	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		(*i)->hide();
	}
	gnome_canvas_item_hide (line);

	if (points == 0 || points->num_points == 0)  {
		return;
	}

	/* compute derivative/slope for the entire line */

	slope = new double[npoints];

	for (n = 0, xi = 2, yi = 3, view_index = 0; n < points->num_points - 1; xi += 2, yi +=2, ++n, ++view_index) {
		double xdelta;
		double ydelta;
		xdelta = points->coords[xi] - points->coords[xi-2];
		ydelta = points->coords[yi] - points->coords[yi-2];
		slope[view_index] = ydelta/xdelta;
	}

	/* read all points and decide which ones to show as control points */

	for (model = alist.begin(), pi = 0, xi = 0, yi = 1, view_index = 0; pi < npoints; ++model, xi += 2, yi +=2, ++pi) {

		/* now ensure that the control_points vector reflects the current curve
		   state, but don't plot control points too close together. also, don't
		   plot a series of points all with the same value.

		   always plot the first and last points, of course.
		*/

		if (invalid_point (points, pi)) {
			/* for some reason, we are supposed to ignore this point,
			   but still keep track of the model index.
			*/
			continue;
		}

		if (pi > 0 && pi < npoints - 1) {
			if (slope[pi] == slope[pi-1]) {

				/* no reason to display this point */
				
				continue;
			}
		}
		
		/* need to round here. the ultimate coordinates are integer
		   pixels, so tiny deltas in the coords will be eliminated
		   and we end up with "colinear" line segments. since the
		   line rendering code in libart doesn't like this very
		   much, we eliminate them here. don't do this for the first and last
		   points.
		*/

		this_rx = (uint32_t) rint (points->coords[xi]);
      		this_ry = (unsigned long) rint (points->coords[yi]); 
 
 		if (view_index && pi != npoints && (this_rx == prev_rx) && (this_ry == prev_ry)) {
 
  			continue;
		} 

		/* ok, we should display this point */

		if (view_index >= control_points.size()) {
			/* make sure we have enough control points */

			ControlPoint* ncp = new ControlPoint (*this, point_callback);

			if (_height > (guint32) TimeAxisView::Larger) {
				ncp->set_size (8.0);
			} else if (_height > (guint32) TimeAxisView::Normal) {
				ncp->set_size (6.0); 
			} else {
				ncp->set_size (4.0);
			}

			control_points.push_back (ncp);
		}

		ControlPoint::ShapeType shape;

		if (!terminal_points_can_slide) {
			if (pi == 0) {
				control_points[view_index]->can_slide = false;
				if (points->coords[xi] == 0) {
					shape = ControlPoint::Start;
				} else {
					shape = ControlPoint::Full;
				}
			} else if (pi == npoints - 1) {
				control_points[view_index]->can_slide = false;
				shape = ControlPoint::End;
			} else {
				control_points[view_index]->can_slide = true;
				shape = ControlPoint::Full;
			}
		} else {
			control_points[view_index]->can_slide = true;
			shape = ControlPoint::Full;
		}

		control_points[view_index]->reset (points->coords[xi], points->coords[yi], model, view_index, shape);

		last_control_point_x = points->coords[xi];
		last_control_point_y = points->coords[yi];

		prev_rx = this_rx;
		prev_ry = this_ry;

		/* finally, control visibility */
		
		if (_visible && points_visible) {
			control_points[view_index]->show ();
			control_points[view_index]->set_visible (true);
		} else {
			if (!points_visible) {
				control_points[view_index]->set_visible (false);
			}
		}

		view_index++;
	}

	/* discard extra CP's to avoid confusing ourselves */

	while (control_points.size() > view_index) {
		ControlPoint* cp = control_points.back();
		control_points.pop_back ();
		delete cp;
	}

	if (!terminal_points_can_slide) {
		control_points.back()->can_slide = false;
	}

	delete [] slope;

	/* Now make sure the "point_coords" array is large enough
	   to represent all the visible points.
	*/

	if (view_index > 1) {

		npoints = view_index;
		
		if (point_coords) {
			if (point_coords->num_points < (int) npoints) {
				gnome_canvas_points_unref (point_coords);
				point_coords = get_canvas_points ("autoline", npoints);
			} else {
				point_coords->num_points = npoints;
			}
		} else {
			point_coords = get_canvas_points ("autoline", npoints);
		}
		
		/* reset the line coordinates */

		uint32_t pci;
		
		for (pci = 0, view_index = 0; view_index < npoints; ++view_index) {
			point_coords->coords[pci++] = control_points[view_index]->get_x();
			point_coords->coords[pci++] = control_points[view_index]->get_y();
		}

		// cerr << "set al2 points, nc = " << point_coords->num_points << endl;
		gnome_canvas_item_set (line, "points", point_coords, NULL);

		if (_visible) {
			gnome_canvas_item_show (line);
		}
	} 

	set_selected_points (trackview.editor.get_selection().points);
}

string
AutomationLine::get_verbose_cursor_string (float fraction)
{
	char buf[32];

	if (_vc_uses_gain_mapping) {
		if (fraction == 0.0) {
			snprintf (buf, sizeof (buf), "-inf dB");
		} else {
			snprintf (buf, sizeof (buf), "%.1fdB", coefficient_to_dB (slider_position_to_gain (fraction)));
		}
	} else {
		snprintf (buf, sizeof (buf), "%.2f", fraction);
	}

	return buf;
}

bool
AutomationLine::invalid_point (GnomeCanvasPoints* p, uint32_t index)
{
	return p->coords[index*2] == max_frames && p->coords[(index*2)+1] == DBL_MAX;
}

void
AutomationLine::invalidate_point (GnomeCanvasPoints* p, uint32_t index)
{
	p->coords[index*2] = max_frames;
	p->coords[(index*2)+1] = DBL_MAX;
}

void
AutomationLine::start_drag (ControlPoint* cp, float fraction) 
{
	if (trackview.editor.current_session() == 0) { /* how? */
		return;
	}

	string str;

	if (cp) {
		str = _("automation event move");
	} else {
		str = _("automation range drag");
	}

	trackview.editor.current_session()->begin_reversible_command (str);
	trackview.editor.current_session()->add_undo (get_memento());
	
	first_drag_fraction = fraction;
	last_drag_fraction = fraction;
	drags = 0;
}

void
AutomationLine::point_drag (ControlPoint& cp, jack_nframes_t x, float fraction, bool with_push) 
{
	modify_view (cp, x, fraction, with_push);
	drags++;
}

void
AutomationLine::line_drag (uint32_t i1, uint32_t i2, float fraction, bool with_push) 
{
	double ydelta = fraction - last_drag_fraction;
	
	last_drag_fraction = fraction;

	line_drag_cp1 = i1;
	line_drag_cp2 = i2;
	
	ControlPoint *cp;

	for (uint32_t i = i1 ; i <= i2; i++) {
		cp = nth (i);
		modify_view_point (*cp, trackview.editor.unit_to_frame (cp->get_x()), ((_height - cp->get_y()) /_height) + ydelta, with_push);
	}

	update_line ();

	drags++;
}

void
AutomationLine::end_drag (ControlPoint* cp) 
{
	if (drags) {

		if (cp) {
			sync_model_from (*cp);
		} else {
			sync_model_with_view_line (line_drag_cp1, line_drag_cp2);
		}

		update_pending = false;

		trackview.editor.current_session()->add_redo_no_execute (get_memento());
		trackview.editor.current_session()->commit_reversible_command ();
		trackview.editor.current_session()->set_dirty ();
	}
}

bool 
AutomationLine::control_points_adjacent (double xval, uint32_t & before, uint32_t& after)
{
	ControlPoint *bcp = 0;
	ControlPoint *acp = 0;
	double unit_xval;

	/* xval is in frames */

	unit_xval = trackview.editor.frame_to_unit (xval);

	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		
		if ((*i)->get_x() <= unit_xval) {

			if (!bcp || (*i)->get_x() > bcp->get_x()) {
				bcp = *i;
				before = bcp->view_index;
			} 

		} else if ((*i)->get_x() > unit_xval) {
			acp = *i;
			after = acp->view_index;
			break;
		}
	}

	return bcp && acp;
}

bool
AutomationLine::is_last_point (ControlPoint& cp)
{
	ModelRepresentation mr;

	model_representation (cp, mr);

	// If the list is not empty, and the point is the last point in the list

	if (!alist.empty() && mr.end == alist.end()) {
		return true;
	}
	
	return false;
}

bool
AutomationLine::is_first_point (ControlPoint& cp)
{
	ModelRepresentation mr;

	model_representation (cp, mr);

	// If the list is not empty, and the point is the first point in the list

	if (!alist.empty() && mr.start == alist.begin()) {
		return true;
	}
	
	return false;
}

// This is copied into AudioRegionGainLine
void
AutomationLine::remove_point (ControlPoint& cp)
{
	ModelRepresentation mr;

	model_representation (cp, mr);

	trackview.editor.current_session()->begin_reversible_command (_("remove control point"));
	trackview.editor.current_session()->add_undo (get_memento());

	alist.erase (mr.start, mr.end);

	trackview.editor.current_session()->add_redo_no_execute (get_memento());
	trackview.editor.current_session()->commit_reversible_command ();
	trackview.editor.current_session()->set_dirty ();
}

void
AutomationLine::get_selectables (jack_nframes_t& start, jack_nframes_t& end,
				 double botfrac, double topfrac, list<Selectable*>& results)
{

	double top;
	double bot;
	jack_nframes_t nstart;
	jack_nframes_t nend;
	bool collecting = false;

	/* Curse X11 and its inverted coordinate system! */
	
	bot = (1.0 - topfrac) * _height;
	top = (1.0 - botfrac) * _height;
	
	nstart = max_frames;
	nend = 0;

	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		
		jack_nframes_t when = (jack_nframes_t) (*(*i)->model)->when;

		if (when >= start && when <= end) {
			
			if ((*i)->get_y() >= bot && (*i)->get_y() <= top) {

				(*i)->show();
				(*i)->set_visible(true);
				collecting = true;
				nstart = min (nstart, when);
				nend = max (nend, when);

			} else {
				
				if (collecting) {

					results.push_back (new AutomationSelectable (nstart, nend, botfrac, topfrac, trackview));
					collecting = false;
					nstart = max_frames;
					nend = 0;
				}
			}
		}
	}

	if (collecting) {
		results.push_back (new AutomationSelectable (nstart, nend, botfrac, topfrac, trackview));
	}

}

void
AutomationLine::get_inverted_selectables (Selection&, list<Selectable*>& results)
{
	// hmmm ....
}

void
AutomationLine::set_selected_points (PointSelection& points)
{
	double top;
	double bot;

	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
			(*i)->selected = false;
	}

	if (points.empty()) {
		goto out;
	} 
	
	for (PointSelection::iterator r = points.begin(); r != points.end(); ++r) {
		
		if (&(*r).track != &trackview) {
			continue;
		}

		/* Curse X11 and its inverted coordinate system! */

		bot = (1.0 - (*r).high_fract) * _height;
		top = (1.0 - (*r).low_fract) * _height;

		for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
			
			double rstart, rend;
			
			rstart = trackview.editor.frame_to_unit ((*r).start);
			rend = trackview.editor.frame_to_unit ((*r).end);
			
			if ((*i)->get_x() >= rstart && (*i)->get_x() <= rend) {
				
				if ((*i)->get_y() >= bot && (*i)->get_y() <= top) {
					
					(*i)->selected = true;
				}
			}

		}
	}

  out:
	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		(*i)->show_color (false, !points_visible);
	}

}

void
AutomationLine::show_selection ()
{
	TimeSelection& time (trackview.editor.get_selection().time);

	// cerr << "show selection\n";

	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		
		(*i)->selected = false;

		for (list<AudioRange>::iterator r = time.begin(); r != time.end(); ++r) {
			double rstart, rend;
			
			rstart = trackview.editor.frame_to_unit ((*r).start);
			rend = trackview.editor.frame_to_unit ((*r).end);
			
			if ((*i)->get_x() >= rstart && (*i)->get_x() <= rend) {
				(*i)->selected = true;
				break;
			}
		}
		
		(*i)->show_color (false, !points_visible);
	}
}

void
AutomationLine::hide_selection ()
{
	// cerr << "hide selection\n";
//	show_selection ();
}


// This is copied into AudioRegionGainLine
UndoAction
AutomationLine::get_memento ()
{
	return alist.get_memento();
}

void
AutomationLine::list_changed (Change ignored)
{
	queue_reset ();
}

void
AutomationLine::reset_callback (const AutomationList& events)
{
	GnomeCanvasPoints *tmp_points;
	uint32_t npoints = events.size();

	if (npoints == 0) {
		for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
			delete *i;
		}
		control_points.clear ();
		gnome_canvas_item_hide (line);
		return;
	}

	tmp_points = get_canvas_points ("autoline reset", max (npoints, (uint32_t) 2));

	uint32_t xi, yi;
	AutomationList::const_iterator ai;

	for (ai = events.const_begin(), xi = 0, yi = 1; ai != events.const_end(); xi += 2, yi +=2, ++ai) {

		tmp_points->coords[xi] = trackview.editor.frame_to_unit ((*ai)->when);
		double translated_y;

		translated_y = (*ai)->value;
		model_to_view_y (translated_y);
		tmp_points->coords[yi] = _height - (translated_y * _height);
	}

	tmp_points->num_points = npoints;

	determine_visible_control_points (tmp_points);
	gnome_canvas_points_unref (tmp_points);
}

void
AutomationLine::reset ()
{
	update_pending = false;

	if (no_draw) {
		return;
	}

	alist.apply_to_points (*this, &AutomationLine::reset_callback);
}

void
AutomationLine::clear ()
{
	/* parent must create command */
	trackview.editor.current_session()->add_undo (get_memento());
	alist.clear();
	trackview.editor.current_session()->add_redo_no_execute (get_memento());
	trackview.editor.current_session()->commit_reversible_command ();
	trackview.editor.current_session()->set_dirty ();
}

void
AutomationLine::change_model (AutomationList::iterator i, double x, double y)
{
	alist.modify (i, (jack_nframes_t) x, y);
}

void
AutomationLine::change_model_range (AutomationList::iterator start, AutomationList::iterator end, double xdelta, float ydelta)
{
	alist.move_range (start, end, xdelta, ydelta);
}

void
AutomationLine::show_all_control_points ()
{
	points_visible = true;

	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		(*i)->show ();
		(*i)->set_visible (true);
	}
}

void
AutomationLine::hide_all_but_selected_control_points ()
{
	points_visible = false;

	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		if (!(*i)->selected) {
			(*i)->set_visible (false);
		}
	}
}
