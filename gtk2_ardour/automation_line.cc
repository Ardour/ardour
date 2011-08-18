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

*/

#include <cmath>
#include <climits>
#include <vector>
#include <fstream>

#include <pbd/stl_delete.h>
#include <pbd/memento_command.h>
#include <pbd/stacktrace.h>

#include <ardour/automation_event.h>
#include <ardour/curve.h>
#include <ardour/dB.h>

#include "simplerect.h"
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
using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace Gnome; // for Canvas

ControlPoint::ControlPoint (AutomationLine& al)
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

	item = new Canvas::SimpleRect (line.canvas_group());
	item->property_draw() = true;
	item->property_fill() = false;
	item->property_fill_color_rgba() =  ARDOUR_UI::config()->canvasvar_ControlPointFill.get();
	item->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_ControlPointOutline.get();
	item->property_outline_pixels() = 1;
	item->set_data ("control_point", this);
	item->signal_event().connect (mem_fun (this, &ControlPoint::event_handler));

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

	item = new Canvas::SimpleRect (line.canvas_group());
	item->property_fill() = false;
	item->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_EnteredControlPointOutline.get();
	item->property_outline_pixels() = 1;
	
	/* NOTE: no event handling in copied ControlPoints */

	hide ();
	set_visible (false);
}

ControlPoint::~ControlPoint ()
{
	delete item;
}

bool
ControlPoint::event_handler (GdkEvent* event)
{
	return PublicEditor::instance().canvas_control_point_event (event, item, this);
}

void
ControlPoint::hide ()
{
	item->hide();
}

void
ControlPoint::show()
{
	item->show();
}

void
ControlPoint::set_visible (bool yn)
{
	item->property_draw() = (gboolean) yn;
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
			item->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_EnteredControlPointSelected.get();
			set_visible(true);
		} else {
			item->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_EnteredControlPoint.get();
			if (hide_too) {
				set_visible(false);
			}
		}

	} else {
		if (selected) {
			item->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_ControlPointSelected.get();
			set_visible(true);
		} else {
			item->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_ControlPoint.get();
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
		item->property_fill() = (gboolean) TRUE;
	} else {
		item->property_fill() = (gboolean) FALSE;
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

	item->property_x1() = x1;
	item->property_x2() = x2;
	item->property_y1() = y - half_size;
	item->property_y2() = y + half_size;

	_x = x;
	_y = y;
	_shape = shape;
}

/*****/

AutomationLine::AutomationLine (const string & name, TimeAxisView& tv, ArdourCanvas::Group& parent, AutomationList& al)
	: trackview (tv),
	  _name (name),
	  alist (al),
	  _parent_group (parent)
{
	points_visible = false;
	update_pending = false;
	_vc_uses_gain_mapping = false;
	auto_is_boolean = false;
	_visible = true;
	terminal_points_can_slide = true;
	_height = 0;

	group = new ArdourCanvas::Group (parent);
	group->property_x() = 0.0;
	group->property_y() = 0.0;

	line = new ArdourCanvas::Line (*group);
	line->property_width_pixels() = (guint)1;
	line->set_data ("line", this);

	line->signal_event().connect (mem_fun (*this, &AutomationLine::event_handler));

	alist.StateChanged.connect (mem_fun(*this, &AutomationLine::list_changed));

        trackview.session().register_with_memento_command_factory(alist.id(), this);

}

AutomationLine::~AutomationLine ()
{
	vector_delete (&control_points);
	delete group;
}

bool
AutomationLine::event_handler (GdkEvent* event)
{
	return PublicEditor::instance().canvas_line_event (event, line, this);
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
AutomationLine::show () 
{
	line->show();

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
	line->hide();
	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		(*i)->hide();
	}
	_visible = false;
}

double
AutomationLine::control_point_box_size ()
{
	if (_height > TimeAxisView::hLarger) {
		return 8.0;
	} else if (_height > (guint32) TimeAxisView::hNormal) {
		return 6.0;
	} 
	return 4.0;
}

void
AutomationLine::set_height (guint32 h)
{
	if (h != _height) {
		_height = h;

		double bsz = control_point_box_size();

		for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
			(*i)->set_size (bsz);
		}

		reset ();
	}
}

void
AutomationLine::set_line_color (uint32_t color)
{
	_line_color = color;
	line->property_fill_color_rgba() = color;
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
	if (cp.view_index < line_points.size()) {
		line_points[cp.view_index].set_x (cp.get_x());
		line_points[cp.view_index].set_y (cp.get_y());
	}
}

void
AutomationLine::sync_model_with_view_line (uint32_t start, uint32_t end)
{

	ControlPoint *p;

	update_pending = true;

	for (uint32_t i = start; i <= end; ++i) {
		p = nth(i);
		sync_model_with_view_point (*p, false, 0);
	}
}

void
AutomationLine::model_representation (ControlPoint& cp, ModelRepresentation& mr)
{
	/* part one: find out where the visual control point is.
	   initial results are in canvas units. ask the
	   line to convert them to something relevant.
	*/
	
	mr.xval = (nframes_t) floor (cp.get_x());
	mr.yval = 1.0 - (cp.get_y() / _height);

	/* if xval has not changed, set it directly from the model to avoid rounding errors */

	if (mr.xval == trackview.editor.frame_to_unit((*cp.model)->when)) {
		mr.xval = (nframes_t) (*cp.model)->when;
	} else {
		mr.xval = trackview.editor.unit_to_frame (mr.xval);
	}

	/* virtual call: this will do the right thing
	   for whatever particular type of line we are.
	*/
	
	view_to_model_y (mr.yval);

	/* part 2: find out where the model point is now
	 */

	mr.xpos = (nframes_t) (*cp.model)->when;
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
		mr.xmin = (nframes_t) (*before->model)->when;
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
AutomationLine::determine_visible_control_points (ALPoints& points)
{
	uint32_t view_index, pi;
	AutomationList::iterator model;
	uint32_t npoints;
	double last_control_point_x = 0.0;
	double last_control_point_y = 0.0;
	uint32_t this_rx = 0;
	uint32_t prev_rx = 0;
	uint32_t this_ry = 0;
 	uint32_t prev_ry = 0;	
	double* slope = NULL;
	uint32_t box_size;
	uint32_t cpsize;

	/* hide all existing points, and the line */

	cpsize = 0;
	
	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		(*i)->hide();
		++cpsize;
	}

	line->hide ();

	if (points.empty()) {
		return;
	}

	npoints = points.size();

	/* compute derivative/slope for the entire line */

	//slope = new double[npoints];

	//for (n = 0; n < npoints - 1; ++n) {
	//	double xdelta = points[n+1].x - points[n].x;
	//	double ydelta = points[n+1].y - points[n].y;
	//	slope[n] = ydelta/xdelta;
	//}

	box_size = (uint32_t) control_point_box_size ();

	/* read all points and decide which ones to show as control points */

	view_index = 0;

	for (model = alist.begin(), pi = 0; pi < npoints; ++model, ++pi) {

		double tx = points[pi].x;
		double ty = points[pi].y;
		
		if (isnan (tx) || isnan (ty)) {
			warning << string_compose (_("Ignoring illegal points on AutomationLine \"%1\""),
						   _name) << endmsg;
			continue;
		}

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

//		if (pi > 0 && pi < npoints - 1) {
//			if (slope[pi] == slope[pi-1]) {

				/* no reason to display this point */
				
//				continue;
//			}
//		}
		
		/* need to round here. the ultimate coordinates are integer
		   pixels, so tiny deltas in the coords will be eliminated
		   and we end up with "colinear" line segments. since the
		   line rendering code in libart doesn't like this very
		   much, we eliminate them here. don't do this for the first and last
		   points.
		*/

		this_rx = (uint32_t) rint (tx);
      		this_ry = (uint32_t) rint (ty); 
 
 //		if (view_index && pi != npoints && /* not the first, not the last */
//		    (((this_rx == prev_rx) && (this_ry == prev_ry)) || /* same point */
//		     (((this_rx - prev_rx) < (box_size + 2)) &&  /* not identical, but still too close horizontally */
//		      (abs ((int)(this_ry - prev_ry)) < (int) (box_size + 2))))) { /* too close vertically */
//  			continue;
//		} 

		/* ok, we should display this point */

		if (view_index >= cpsize) {

			/* make sure we have enough control points */

			ControlPoint* ncp = new ControlPoint (*this);
			
			ncp->set_size (box_size); 

			control_points.push_back (ncp);
			++cpsize;
		}

		ControlPoint::ShapeType shape;

		if (!terminal_points_can_slide) {
			if (pi == 0) {
				control_points[view_index]->can_slide = false;
				if (tx == 0) {
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

		last_control_point_x = tx;
		last_control_point_y = ty;

		control_points[view_index]->reset (tx, ty, model, view_index, shape);

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

	if (view_index > 1) {

		npoints = view_index;
		
		/* reset the line coordinates */

		while (line_points.size() < npoints) {
			line_points.push_back (Art::Point (0,0));
		}

		while (line_points.size() > npoints) {
			line_points.pop_back ();
		}

		for (view_index = 0; view_index < npoints; ++view_index) {
			line_points[view_index].set_x (control_points[view_index]->get_x());
			line_points[view_index].set_y (control_points[view_index]->get_y());
		}
		
		line->property_points() = line_points;

		if (_visible) {
			line->show ();
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
			snprintf (buf, sizeof (buf), "%.1fdB", accurate_coefficient_to_dB (slider_position_to_gain (fraction)));
		}
	} else {
		snprintf (buf, sizeof (buf), "%.2f", fraction);
	}

	return buf;
}

bool
AutomationLine::invalid_point (ALPoints& p, uint32_t index)
{
	return p[index].x == max_frames && p[index].y == DBL_MAX;
}

void
AutomationLine::invalidate_point (ALPoints& p, uint32_t index)
{
	p[index].x = max_frames;
	p[index].y = DBL_MAX;
}

void
AutomationLine::start_drag (ControlPoint* cp, nframes_t x, float fraction) 
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
	trackview.editor.current_session()->add_command (new MementoCommand<AutomationList>(alist, &get_state(), 0));
	
	drag_x = x;
	drag_distance = 0;
	first_drag_fraction = fraction;
	last_drag_fraction = fraction;
	drags = 0;
	did_push = false;
}

void
AutomationLine::point_drag (ControlPoint& cp, nframes_t x, float fraction, bool with_push) 
{
	if (x > drag_x) {
		drag_distance += (x - drag_x);
	} else {
		drag_distance -= (drag_x - x);
	}

	drag_x = x;

	modify_view_point (cp, x, fraction, with_push);

	if (line_points.size() > 1) {
		line->property_points() = line_points;
	}

	drags++;
	did_push = with_push;
}

void
AutomationLine::line_drag (uint32_t i1, uint32_t i2, float fraction, bool with_push) 
{
	double ydelta = fraction - last_drag_fraction;

	did_push = with_push;
	
	last_drag_fraction = fraction;

	line_drag_cp1 = i1;
	line_drag_cp2 = i2;
	
	//check if one of the control points on the line is in a selected range
	bool range_found = false;
	ControlPoint *cp;
	for (uint32_t i = i1 ; i <= i2; i++) {
		cp = nth (i);
		if ( cp->selected )
			range_found = true;
	}
	
	if (range_found) {
		for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
			if ( (*i)->selected ) {
				modify_view_point (*(*i), trackview.editor.unit_to_frame ((*i)->get_x()), ((_height - (*i)->get_y()) /_height) + ydelta, with_push);
			}
		}
	} else {
		ControlPoint *cp;
		for (uint32_t i = i1 ; i <= i2; i++) {
			cp = nth (i);
			modify_view_point (*cp, trackview.editor.unit_to_frame (cp->get_x()), ((_height - cp->get_y()) /_height) + ydelta, with_push);
		}
	}
	
	if (line_points.size() > 1) {
		line->property_points() = line_points;
	}

	drags++;
}

void
AutomationLine::end_drag (ControlPoint* cp) 
{
	if (!drags) {
		return;
	}

	alist.freeze ();

	if (cp) {
		sync_model_with_view_point (*cp, did_push, drag_distance);
	} else {
		sync_model_with_view_line (line_drag_cp1, line_drag_cp2);
	}
	
	alist.thaw ();

	update_pending = false;

	trackview.editor.current_session()->add_command (new MementoCommand<AutomationList>(alist, 0, &alist.get_state()));
	trackview.editor.current_session()->commit_reversible_command ();
	trackview.editor.current_session()->set_dirty ();
}


void
AutomationLine::sync_model_with_view_point (ControlPoint& cp, bool did_push, int64_t distance)
{
	ModelRepresentation mr;
	double ydelta;

	model_representation (cp, mr);

	/* how much are we changing the central point by */ 

	ydelta = mr.yval - mr.ypos;

	/*
	   apply the full change to the central point, and interpolate
	   on both axes to cover all model points represented
	   by the control point.
	*/

	/* change all points before the primary point */

	for (AutomationList::iterator i = mr.start; i != cp.model; ++i) {
		
		double fract = ((*i)->when - mr.xmin) / (mr.xpos - mr.xmin);
		double y_delta = ydelta * fract;
		double x_delta = distance * fract;

		/* interpolate */
		
		if (y_delta || x_delta) {
			alist.modify (i, (*i)->when + x_delta, mr.ymin + y_delta);
		}
	}

	/* change the primary point */

	update_pending = true;
	alist.modify (cp.model, mr.xval, mr.yval);


	/* change later points */
	
	AutomationList::iterator i = cp.model;
	
	++i;
	
	while (i != mr.end) {
		
		double delta = ydelta * (mr.xmax - (*i)->when) / (mr.xmax - mr.xpos);

		/* all later points move by the same distance along the x-axis as the main point */
		
		if (delta) {
			alist.modify (i, (*i)->when + distance, (*i)->value + delta);
		}
		
		++i;
	}
		
	if (did_push) {

		/* move all points after the range represented by the view by the same distance
		   as the main point moved.
		*/

		alist.slide (mr.end, drag_distance);
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
        XMLNode &before = alist.get_state();

	alist.erase (mr.start, mr.end);

	trackview.editor.current_session()->add_command(new MementoCommand<AutomationList>(alist, &before, &alist.get_state()));
	trackview.editor.current_session()->commit_reversible_command ();
	trackview.editor.current_session()->set_dirty ();
}

void
AutomationLine::get_selectables (nframes_t& start, nframes_t& end,
				 double botfrac, double topfrac, list<Selectable*>& results)
{

	double top;
	double bot;
	nframes_t nstart;
	nframes_t nend;
	bool collecting = false;

	/* Curse X11 and its inverted coordinate system! */
	
	bot = (1.0 - topfrac) * _height;
	top = (1.0 - botfrac) * _height;
	
	nstart = max_frames;
	nend = 0;

	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		
		nframes_t when = (nframes_t) (*(*i)->model)->when;

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

void AutomationLine::set_colors() {
	set_line_color( ARDOUR_UI::config()->canvasvar_AutomationLine.get() );
	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		(*i)->show_color (false, !points_visible);
	}
}

void
AutomationLine::show_selection ()
{
	TimeSelection& time (trackview.editor.get_selection().time);

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
//	show_selection ();
}

void
AutomationLine::list_changed ()
{
	queue_reset ();
}

void
AutomationLine::reset_callback (const AutomationList& events)
{
	ALPoints tmp_points;
	uint32_t npoints = events.size();

	if (npoints == 0) {
		for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
			delete *i;
		}
		control_points.clear ();
		line->hide();
		return;
	}

	AutomationList::const_iterator ai;

	for (ai = events.const_begin(); ai != events.const_end(); ++ai) {
		
		double translated_y = (*ai)->value;
		model_to_view_y (translated_y);

		add_model_point (tmp_points, (*ai)->when, translated_y);
	}
	
	determine_visible_control_points (tmp_points);
}

void
AutomationLine::add_model_point (ALPoints& tmp_points, double frame, double yfract)
{
	tmp_points.push_back (ALPoint (trackview.editor.frame_to_unit (frame),
				       _height - (yfract * _height)));
}

void
AutomationLine::reset ()
{
	update_pending = false;

	alist.apply_to_points (*this, &AutomationLine::reset_callback);
}

void
AutomationLine::clear ()
{
	/* parent must create command */
        XMLNode &before = alist.get_state();
	alist.clear();
	trackview.editor.current_session()->add_command (new MementoCommand<AutomationList>(alist, &before, &get_state()));
	trackview.editor.current_session()->commit_reversible_command ();
	trackview.editor.current_session()->set_dirty ();
}

void
AutomationLine::change_model (AutomationList::iterator i, double x, double y)
{
}

void
AutomationLine::change_model_range (AutomationList::iterator start, AutomationList::iterator end, double xdelta, float ydelta)
{
	alist.move_range (start, end, xdelta, ydelta);
}

void
AutomationLine::show_all_control_points ()
{
	if (auto_is_boolean) {  //show the automation line but don't allow any control points
		return;
	}

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

XMLNode &
AutomationLine::get_state (void)
{
	/* function as a proxy for the model */
	return alist.get_state();
}

int 
AutomationLine::set_state (const XMLNode &node)
{
	/* function as a proxy for the model */
	return alist.set_state (node);
}
