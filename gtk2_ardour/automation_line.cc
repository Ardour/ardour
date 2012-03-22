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

#include "pbd/stl_delete.h"
#include "pbd/memento_command.h"
#include "pbd/stacktrace.h"

#include "ardour/automation_list.h"
#include "ardour/dB.h"
#include "evoral/Curve.hpp"

#include "simplerect.h"
#include "automation_line.h"
#include "control_point.h"
#include "gui_thread.h"
#include "rgb_macros.h"
#include "ardour_ui.h"
#include "public_editor.h"
#include "utils.h"
#include "selection.h"
#include "time_axis_view.h"
#include "point_selection.h"
#include "automation_time_axis.h"
#include "public_editor.h"

#include "ardour/event_type_map.h"
#include "ardour/session.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace Gnome; // for Canvas

/** @param converter A TimeConverter whose origin_b is the start time of the AutomationList in session frames.
 *  This will not be deleted by AutomationLine.
 */
AutomationLine::AutomationLine (const string& name, TimeAxisView& tv, ArdourCanvas::Group& parent,
		boost::shared_ptr<AutomationList> al,
		Evoral::TimeConverter<double, framepos_t>* converter)
	: trackview (tv)
	, _name (name)
	, alist (al)
	, _time_converter (converter ? converter : new Evoral::IdentityConverter<double, framepos_t>)
	, _parent_group (parent)
	, _offset (0)
	, _maximum_time (max_framepos)
{
	if (converter) {
		_time_converter = converter;
		_our_time_converter = false;
	} else {
		_time_converter = new Evoral::IdentityConverter<double, framepos_t>;
		_our_time_converter = true;
	}
	
	points_visible = false;
	update_pending = false;
	_uses_gain_mapping = false;
	no_draw = false;
	_visible = true;
	_is_boolean = false;
	terminal_points_can_slide = true;
	_height = 0;

	group = new ArdourCanvas::Group (parent);
	group->property_x() = 0.0;
	group->property_y() = 0.0;

	line = new ArdourCanvas::Line (*group);
	line->property_width_pixels() = (guint)1;
	line->set_data ("line", this);

	line->signal_event().connect (sigc::mem_fun (*this, &AutomationLine::event_handler));

	trackview.session()->register_with_memento_command_factory(alist->id(), this);

	if (alist->parameter().type() == GainAutomation ||
	    alist->parameter().type() == EnvelopeAutomation) {
		set_uses_gain_mapping (true);
	}

	interpolation_changed (alist->interpolation ());

	connect_to_list ();
}

AutomationLine::~AutomationLine ()
{
	vector_delete (&control_points);
	delete group;

	if (_our_time_converter) {
		delete _time_converter;
	}
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
		Gtkmm2ext::UI::instance()->call_slot (invalidator (*this), boost::bind (&AutomationLine::reset, this));
	}
}

void
AutomationLine::show ()
{
	if (alist->interpolation() != AutomationList::Discrete) {
		line->show();
	}

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
	if (alist->interpolation() == AutomationList::Discrete) {
		return max((_height*4.0) / (double)(alist->parameter().max() - alist->parameter().min()),
				4.0);
	}

	if (_height > TimeAxisView::preset_height (HeightLarger)) {
		return 8.0;
	} else if (_height > (guint32) TimeAxisView::preset_height (HeightNormal)) {
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
AutomationLine::set_uses_gain_mapping (bool yn)
{
	if (yn != _uses_gain_mapping) {
		_uses_gain_mapping = yn;
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

ControlPoint const *
AutomationLine::nth (uint32_t n) const
{
	if (n < control_points.size()) {
		return control_points[n];
	} else {
		return 0;
	}
}

void
AutomationLine::modify_point_y (ControlPoint& cp, double y)
{
	/* clamp y-coord appropriately. y is supposed to be a normalized fraction (0.0-1.0),
	   and needs to be converted to a canvas unit distance.
	*/

	y = max (0.0, y);
	y = min (1.0, y);
	y = _height - (y * _height);

	double const x = trackview.editor().frame_to_unit (_time_converter->to((*cp.model())->when) - _offset);

	trackview.editor().session()->begin_reversible_command (_("automation event move"));
	trackview.editor().session()->add_command (
		new MementoCommand<AutomationList> (memento_command_binder(), &get_state(), 0)
		);

	cp.move_to (x, y, ControlPoint::Full);

	reset_line_coords (cp);

	if (line_points.size() > 1) {
		line->property_points() = line_points;
	}

	alist->freeze ();
	sync_model_with_view_point (cp, false, 0);
	alist->thaw ();

	update_pending = false;

	trackview.editor().session()->add_command (
		new MementoCommand<AutomationList> (memento_command_binder(), 0, &alist->get_state())
		);

	trackview.editor().session()->commit_reversible_command ();
	trackview.editor().session()->set_dirty ();
}

void
AutomationLine::reset_line_coords (ControlPoint& cp)
{
	if (cp.view_index() < line_points.size()) {
		line_points[cp.view_index()].set_x (cp.get_x());
		line_points[cp.view_index()].set_y (cp.get_y());
	}
}

void
AutomationLine::sync_model_with_view_points (list<ControlPoint*> cp, bool did_push, int64_t distance)
{
	update_pending = true;

	for (list<ControlPoint*>::iterator i = cp.begin(); i != cp.end(); ++i) {
		sync_model_with_view_point (**i, did_push, distance);
	}
}

void
AutomationLine::model_representation (ControlPoint& cp, ModelRepresentation& mr)
{
	/* part one: find out where the visual control point is.
	   initial results are in canvas units. ask the
	   line to convert them to something relevant.
	*/

	mr.xval = cp.get_x();
	mr.yval = 1.0 - (cp.get_y() / _height);

	/* if xval has not changed, set it directly from the model to avoid rounding errors */

	if (mr.xval == trackview.editor().frame_to_unit(_time_converter->to((*cp.model())->when)) - _offset) {
		mr.xval = (*cp.model())->when - _offset;
	} else {
		mr.xval = trackview.editor().unit_to_frame (mr.xval);
		mr.xval = _time_converter->from (mr.xval + _offset);
	}

	/* convert y to model units; the x was already done above
	*/

	view_to_model_coord_y (mr.yval);

	/* part 2: find out where the model point is now
	 */

	mr.xpos = (*cp.model())->when - _offset;
	mr.ypos = (*cp.model())->value;

	/* part 3: get the position of the visual control
	   points before and after us.
	*/

	ControlPoint* before;
	ControlPoint* after;

	if (cp.view_index()) {
		before = nth (cp.view_index() - 1);
	} else {
		before = 0;
	}

	after = nth (cp.view_index() + 1);

	if (before) {
		mr.xmin = (*before->model())->when;
		mr.ymin = (*before->model())->value;
		mr.start = before->model();
		++mr.start;
	} else {
		mr.xmin = mr.xpos;
		mr.ymin = mr.ypos;
		mr.start = cp.model();
	}

	if (after) {
		mr.end = after->model();
	} else {
		mr.xmax = mr.xpos;
		mr.ymax = mr.ypos;
		mr.end = cp.model();
		++mr.end;
	}
}

/** @param points AutomationLine points to consider.  These will correspond 1-to-1 to
 *  points in the AutomationList, but will have been transformed so that they are in pixels;
 *  the x coordinate being the pixel distance from the start of the line (0, or the start
 *  of the AutomationRegionView if we are in one).
 *
 *  @param skipped Number of points in the AutomationList that were skipped before
 *  `points' starts.
 */

void
AutomationLine::determine_visible_control_points (ALPoints& points, int skipped)
{
	uint32_t view_index, pi, n;
	uint32_t npoints;
	uint32_t this_rx = 0;
	uint32_t prev_rx = 0;
	uint32_t this_ry = 0;
	uint32_t prev_ry = 0;
	double* slope;
	uint32_t box_size;

	/* hide all existing points, and the line */

	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		(*i)->hide();
	}

	line->hide ();

	if (points.empty()) {
		return;
	}

	npoints = points.size();

	/* compute derivative/slope for the entire line */

	slope = new double[npoints];

	for (n = 0; n < npoints - 1; ++n) {
		double xdelta = points[n+1].x - points[n].x;
		double ydelta = points[n+1].y - points[n].y;
		slope[n] = ydelta/xdelta;
	}

	box_size = (uint32_t) control_point_box_size ();

	/* read all points and decide which ones to show as control points */

	view_index = 0;

	/* skip over unused AutomationList points before we start */

	AutomationList::iterator model = alist->begin ();
	for (int i = 0; i < skipped; ++i) {
		++model;
	}

	for (pi = 0; pi < npoints; ++model, ++pi) {

		/* If this line is in an AutomationRegionView, this is an offset from the region position, in pixels */
		double tx = points[pi].x;
		double ty = points[pi].y;

		if (find (_always_in_view.begin(), _always_in_view.end(), (*model)->when) != _always_in_view.end()) {
			add_visible_control_point (view_index, pi, tx, ty, model, npoints);
			prev_rx = this_rx;
			prev_ry = this_ry;
			++view_index;
			continue;
		}

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

		this_rx = (uint32_t) rint (tx);
		this_ry = (uint32_t) rint (ty);

		if (view_index && pi != npoints && /* not the first, not the last */
		    (((this_rx == prev_rx) && (this_ry == prev_ry)) || /* same point */
		     (((this_rx - prev_rx) < (box_size + 2)) &&  /* not identical, but still too close horizontally */
		      (abs ((int)(this_ry - prev_ry)) < (int) (box_size + 2))))) { /* too close vertically */
			continue;
		}

		/* ok, we should display this point */

		add_visible_control_point (view_index, pi, tx, ty, model, npoints);

		prev_rx = this_rx;
		prev_ry = this_ry;

		view_index++;
	}

	/* discard extra CP's to avoid confusing ourselves */

	while (control_points.size() > view_index) {
		ControlPoint* cp = control_points.back();
		control_points.pop_back ();
		delete cp;
	}

	if (!terminal_points_can_slide) {
		control_points.back()->set_can_slide(false);
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

		if (_visible && alist->interpolation() != AutomationList::Discrete) {
			line->show();
		}

	}

	set_selected_points (trackview.editor().get_selection().points);
}

string
AutomationLine::get_verbose_cursor_string (double fraction) const
{
	std::string s = fraction_to_string (fraction);
	if (_uses_gain_mapping) {
		s += " dB";
	}

	return s;
}

/**
 *  @param fraction y fraction
 *  @return string representation of this value, using dB if appropriate.
 */
string
AutomationLine::fraction_to_string (double fraction) const
{
	char buf[32];

	if (_uses_gain_mapping) {
		if (fraction == 0.0) {
			snprintf (buf, sizeof (buf), "-inf");
		} else {
			snprintf (buf, sizeof (buf), "%.1f", accurate_coefficient_to_dB (slider_position_to_gain_with_max (fraction, Config->get_max_gain())));
		}
	} else {
		view_to_model_coord_y (fraction);
		if (EventTypeMap::instance().is_integer (alist->parameter())) {
			snprintf (buf, sizeof (buf), "%d", (int)fraction);
		} else {
			snprintf (buf, sizeof (buf), "%.2f", fraction);
		}
	}

	return buf;
}


/**
 *  @param s Value string in the form as returned by fraction_to_string.
 *  @return Corresponding y fraction.
 */
double
AutomationLine::string_to_fraction (string const & s) const
{
	if (s == "-inf") {
		return 0;
	}

	double v;
	sscanf (s.c_str(), "%lf", &v);

	if (_uses_gain_mapping) {
		v = gain_to_slider_position_with_max (dB_to_coefficient (v), Config->get_max_gain());
	} else {
		double dummy = 0.0;
		model_to_view_coord (dummy, v);
	}

	return v;
}

bool
AutomationLine::invalid_point (ALPoints& p, uint32_t index)
{
	return p[index].x == max_framepos && p[index].y == DBL_MAX;
}

void
AutomationLine::invalidate_point (ALPoints& p, uint32_t index)
{
	p[index].x = max_framepos;
	p[index].y = DBL_MAX;
}

/** Start dragging a single point, possibly adding others if the supplied point is selected and there
 *  are other selected points.
 *
 *  @param cp Point to drag.
 *  @param x Initial x position (units).
 *  @param fraction Initial y position (as a fraction of the track height, where 0 is the bottom and 1 the top)
 */
void
AutomationLine::start_drag_single (ControlPoint* cp, double x, float fraction)
{
	trackview.editor().session()->begin_reversible_command (_("automation event move"));
	trackview.editor().session()->add_command (
		new MementoCommand<AutomationList> (memento_command_binder(), &get_state(), 0)
		);

	_drag_points.clear ();
	_drag_points.push_back (cp);

	if (cp->get_selected ()) {
		for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
			if (*i != cp && (*i)->get_selected()) {
				_drag_points.push_back (*i);
			}
		}
	}

	start_drag_common (x, fraction);
}

/** Start dragging a line vertically (with no change in x)
 *  @param i1 Control point index of the `left' point on the line.
 *  @param i2 Control point index of the `right' point on the line.
 *  @param fraction Initial y position (as a fraction of the track height, where 0 is the bottom and 1 the top)
 */
void
AutomationLine::start_drag_line (uint32_t i1, uint32_t i2, float fraction)
{
	trackview.editor().session()->begin_reversible_command (_("automation range move"));
	trackview.editor().session()->add_command (
		new MementoCommand<AutomationList> (memento_command_binder (), &get_state(), 0)
		);

	_drag_points.clear ();
	for (uint32_t i = i1; i <= i2; i++) {
		_drag_points.push_back (nth (i));
	}

	start_drag_common (0, fraction);
}

/** Start dragging multiple points (with no change in x)
 *  @param cp Points to drag.
 *  @param fraction Initial y position (as a fraction of the track height, where 0 is the bottom and 1 the top)
 */
void
AutomationLine::start_drag_multiple (list<ControlPoint*> cp, float fraction, XMLNode* state)
{
	trackview.editor().session()->begin_reversible_command (_("automation range move"));
	trackview.editor().session()->add_command (
		new MementoCommand<AutomationList> (memento_command_binder(), state, 0)
		);

	_drag_points = cp;
	start_drag_common (0, fraction);
}


struct ControlPointSorter
{
	bool operator() (ControlPoint const * a, ControlPoint const * b) {
		return a->get_x() < b->get_x();
	}
};

/** Common parts of starting a drag.
 *  @param x Starting x position in units, or 0 if x is being ignored.
 *  @param fraction Starting y position (as a fraction of the track height, where 0 is the bottom and 1 the top)
 */
void
AutomationLine::start_drag_common (double x, float fraction)
{
	_drag_x = x;
	_drag_distance = 0;
	_last_drag_fraction = fraction;
	_drag_had_movement = false;
	did_push = false;

	_drag_points.sort (ControlPointSorter ());

	/* find the additional points that will be dragged when the user is holding
	   the "push" modifier
	*/

	uint32_t i = _drag_points.back()->view_index () + 1;
	ControlPoint* p = 0;
	_push_points.clear ();
	while ((p = nth (i)) != 0 && p->can_slide()) {
		_push_points.push_back (p);
		++i;
	}
}

/** Should be called to indicate motion during a drag.
 *  @param x New x position of the drag in units, or undefined if ignore_x == true.
 *  @param fraction New y fraction.
 *  @return x position and y fraction that were actually used (once clamped).
 */
pair<double, float>
AutomationLine::drag_motion (double x, float fraction, bool ignore_x, bool with_push)
{
	/* setup the points that are to be moved this time round */
	list<ControlPoint*> points = _drag_points;
	if (with_push) {
		copy (_push_points.begin(), _push_points.end(), back_inserter (points));
		points.sort (ControlPointSorter ());
	}

	double dx = ignore_x ? 0 : (x - _drag_x);
	double dy = fraction - _last_drag_fraction;

	/* find x limits */
	ControlPoint* before = 0;
	ControlPoint* after = 0;

	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		if ((*i)->get_x() < points.front()->get_x()) {
			before = *i;
		}
		if ((*i)->get_x() > points.back()->get_x() && after == 0) {
			after = *i;
		}
	}

	double const before_x = before ? before->get_x() : 0;
	double const after_x = after ? after->get_x() : DBL_MAX;

	/* clamp x */
	for (list<ControlPoint*>::iterator i = points.begin(); i != points.end(); ++i) {

		if ((*i)->can_slide() && !ignore_x) {

			/* clamp min x */
			double const a = (*i)->get_x() + dx;
			double const b = before_x + 1;
			if (a < b) {
				dx += b - a;
			}

			/* clamp max x */
			if (after) {

				if (after_x - before_x < 2) {
					/* after and before are very close, so just leave this alone */
					dx = 0;
				} else {
					double const a = (*i)->get_x() + dx;
					double const b = after_x - 1;
					if (a > b) {
						dx -= a - b;
					}
				}
			}
		}
	}

	/* clamp y */
	for (list<ControlPoint*>::iterator i = points.begin(); i != points.end(); ++i) {
		double const y = ((_height - (*i)->get_y()) / _height) + dy;
		if (y < 0) {
			dy -= y;
		}
		if (y > 1) {
			dy -= (y - 1);
		}
	}

	pair<double, float> const clamped (_drag_x + dx, _last_drag_fraction + dy);
	_drag_distance += dx;
	_drag_x = x;
	_last_drag_fraction = fraction;

	for (list<ControlPoint*>::iterator i = _drag_points.begin(); i != _drag_points.end(); ++i) {
		(*i)->move_to ((*i)->get_x() + dx, (*i)->get_y() - _height * dy, ControlPoint::Full);
		reset_line_coords (**i);
	}

	if (with_push) {
		/* move push points, preserving their y */
		for (list<ControlPoint*>::iterator i = _push_points.begin(); i != _push_points.end(); ++i) {
			(*i)->move_to ((*i)->get_x() + dx, (*i)->get_y(), ControlPoint::Full);
			reset_line_coords (**i);
		}
	}

	if (line_points.size() > 1) {
		line->property_points() = line_points;
	}

	_drag_had_movement = true;
	did_push = with_push;

	return clamped;
}

/** Should be called to indicate the end of a drag */
void
AutomationLine::end_drag ()
{
	if (!_drag_had_movement) {
		return;
	}

	alist->freeze ();

	/* set up the points that were moved this time round */
	list<ControlPoint*> points = _drag_points;
	if (did_push) {
		copy (_push_points.begin(), _push_points.end(), back_inserter (points));
		points.sort (ControlPointSorter ());
	}

	sync_model_with_view_points (points, did_push, rint (_drag_distance * trackview.editor().get_current_zoom ()));

	alist->thaw ();

	update_pending = false;

	trackview.editor().session()->add_command (
		new MementoCommand<AutomationList>(memento_command_binder (), 0, &alist->get_state())
		);

	trackview.editor().session()->set_dirty ();
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

	for (AutomationList::iterator i = mr.start; i != cp.model(); ++i) {

		double fract = ((*i)->when - mr.xmin) / (mr.xpos - mr.xmin);
		double y_delta = ydelta * fract;
		double x_delta = distance * fract;

		/* interpolate */

		if (y_delta || x_delta) {
			alist->modify (i, (*i)->when + x_delta, mr.ymin + y_delta);
		}
	}

	/* change the primary point */

	update_pending = true;
	alist->modify (cp.model(), mr.xval, mr.yval);

	/* change later points */

	AutomationList::iterator i = cp.model();

	++i;

	while (i != mr.end) {

		double delta = ydelta * (mr.xmax - (*i)->when) / (mr.xmax - mr.xpos);

		/* all later points move by the same distance along the x-axis as the main point */

		if (delta) {
			alist->modify (i, (*i)->when + distance, (*i)->value + delta);
		}

		++i;
	}

	if (did_push) {

		/* move all points after the range represented by the view by the same distance
		   as the main point moved.
		*/

		alist->slide (mr.end, distance);
	}
}

bool
AutomationLine::control_points_adjacent (double xval, uint32_t & before, uint32_t& after)
{
	ControlPoint *bcp = 0;
	ControlPoint *acp = 0;
	double unit_xval;

	unit_xval = trackview.editor().frame_to_unit (xval);

	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {

		if ((*i)->get_x() <= unit_xval) {

			if (!bcp || (*i)->get_x() > bcp->get_x()) {
				bcp = *i;
				before = bcp->view_index();
			}

		} else if ((*i)->get_x() > unit_xval) {
			acp = *i;
			after = acp->view_index();
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

	if (!alist->empty() && mr.end == alist->end()) {
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

	if (!alist->empty() && mr.start == alist->begin()) {
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

	trackview.editor().session()->begin_reversible_command (_("remove control point"));
	XMLNode &before = alist->get_state();

	alist->erase (mr.start, mr.end);

	trackview.editor().session()->add_command(
		new MementoCommand<AutomationList> (memento_command_binder (), &before, &alist->get_state())
		);

	trackview.editor().session()->commit_reversible_command ();
	trackview.editor().session()->set_dirty ();
}

/** Get selectable points within an area.
 *  @param start Start position in session frames.
 *  @param end End position in session frames.
 *  @param bot Bottom y range, as a fraction of line height, where 0 is the bottom of the line.
 *  @param top Top y range, as a fraction of line height, where 0 is the bottom of the line.
 *  @param result Filled in with selectable things; in this case, ControlPoints.
 */
void
AutomationLine::get_selectables (
	framepos_t start, framepos_t end, double botfrac, double topfrac, list<Selectable*>& results
	)
{
	/* convert fractions to display coordinates with 0 at the top of the track */
	double const bot_track = (1 - topfrac) * trackview.current_height ();
	double const top_track = (1 - botfrac) * trackview.current_height ();

	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		double const model_when = (*(*i)->model())->when;

		/* model_when is relative to the start of the source, so we just need to add on the origin_b here
		   (as it is the session frame position of the start of the source)
		*/
		
		framepos_t const session_frames_when = _time_converter->to (model_when) + _time_converter->origin_b ();

		if (session_frames_when >= start && session_frames_when <= end && (*i)->get_y() >= bot_track && (*i)->get_y() <= top_track) {
			results.push_back (*i);
		}
	}
}

void
AutomationLine::get_inverted_selectables (Selection&, list<Selectable*>& /*results*/)
{
	// hmmm ....
}

/** Take a PointSelection and find ControlPoints that fall within it */
list<ControlPoint*>
AutomationLine::point_selection_to_control_points (PointSelection const & s)
{
	list<ControlPoint*> cp;

	for (PointSelection::const_iterator i = s.begin(); i != s.end(); ++i) {

		if (i->track != &trackview) {
			continue;
		}

		double const bot = (1 - i->high_fract) * trackview.current_height ();
		double const top = (1 - i->low_fract) * trackview.current_height ();

		for (vector<ControlPoint*>::iterator j = control_points.begin(); j != control_points.end(); ++j) {

			double const rstart = trackview.editor().frame_to_unit (_time_converter->to (i->start) - _offset);
			double const rend = trackview.editor().frame_to_unit (_time_converter->to (i->end) - _offset);

			if ((*j)->get_x() >= rstart && (*j)->get_x() <= rend) {
				if ((*j)->get_y() >= bot && (*j)->get_y() <= top) {
					cp.push_back (*j);
				}
			}
		}

	}

	return cp;
}

void
AutomationLine::set_selected_points (PointSelection const & points)
{
	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		(*i)->set_selected (false);
	}

	if (!points.empty()) {
		list<ControlPoint*> cp = point_selection_to_control_points (points);
		for (list<ControlPoint*>::iterator i = cp.begin(); i != cp.end(); ++i) {
			(*i)->set_selected (true);
		}
	}

	set_colors ();
}

void AutomationLine::set_colors ()
{
	set_line_color (ARDOUR_UI::config()->canvasvar_AutomationLine.get());
	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		(*i)->set_color ();
	}
}

void
AutomationLine::list_changed ()
{
	queue_reset ();
}

void
AutomationLine::reset_callback (const Evoral::ControlList& events)
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
	int skipped = 0;

	for (ai = events.begin(); ai != events.end(); ++ai) {

		double translated_x = (*ai)->when;
		double translated_y = (*ai)->value;
		model_to_view_coord (translated_x, translated_y);

		if (translated_x >= 0 && translated_x < _maximum_time) {
			tmp_points.push_back (ALPoint (
						      trackview.editor().frame_to_unit (translated_x),
						      _height - (translated_y * _height))
				);
		} else if (translated_x < 0) {
			++skipped;
		}
	}

	determine_visible_control_points (tmp_points, skipped);
}

void
AutomationLine::reset ()
{
	update_pending = false;

	if (no_draw) {
		return;
	}

	alist->apply_to_points (*this, &AutomationLine::reset_callback);
}

void
AutomationLine::clear ()
{
	/* parent must create and commit command */
	XMLNode &before = alist->get_state();
	alist->clear();

	trackview.editor().session()->add_command (
		new MementoCommand<AutomationList> (memento_command_binder (), &before, &alist->get_state())
		);
}

void
AutomationLine::change_model (AutomationList::iterator /*i*/, double /*x*/, double /*y*/)
{
}

void
AutomationLine::set_list (boost::shared_ptr<ARDOUR::AutomationList> list)
{
	alist = list;
	queue_reset ();
	connect_to_list ();
}

void
AutomationLine::show_all_control_points ()
{
	if (_is_boolean) {
		// show the line but don't allow any control points
		return;
	}

	points_visible = true;

	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		if (!(*i)->visible()) {
			(*i)->show ();
			(*i)->set_visible (true);
		}
	}
}

void
AutomationLine::hide_all_but_selected_control_points ()
{
	if (alist->interpolation() == AutomationList::Discrete) {
		return;
	}

	points_visible = false;

	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		if (!(*i)->get_selected()) {
			(*i)->set_visible (false);
		}
	}
}

void
AutomationLine::track_entered()
{
	if (alist->interpolation() != AutomationList::Discrete) {
		show_all_control_points();
	}
}

void
AutomationLine::track_exited()
{
	if (alist->interpolation() != AutomationList::Discrete) {
		hide_all_but_selected_control_points();
	}
}

XMLNode &
AutomationLine::get_state (void)
{
	/* function as a proxy for the model */
	return alist->get_state();
}

int
AutomationLine::set_state (const XMLNode &node, int version)
{
	/* function as a proxy for the model */
	return alist->set_state (node, version);
}

void
AutomationLine::view_to_model_coord (double& x, double& y) const
{
	x = _time_converter->from (x);
	view_to_model_coord_y (y);
}

void
AutomationLine::view_to_model_coord_y (double& y) const
{
	/* TODO: This should be more generic ... */
	if (alist->parameter().type() == GainAutomation ||
	    alist->parameter().type() == EnvelopeAutomation) {
		y = slider_position_to_gain_with_max (y, Config->get_max_gain());
		y = max (0.0, y);
		y = min (2.0, y);
	} else if (alist->parameter().type() == PanAzimuthAutomation ||
                   alist->parameter().type() == PanElevationAutomation ||
                   alist->parameter().type() == PanWidthAutomation) {
		y = 1.0 - y;
	} else if (alist->parameter().type() == PluginAutomation) {
		y = y * (double)(alist->get_max_y()- alist->get_min_y()) + alist->get_min_y();
	} else {
		y = rint (y * alist->parameter().max());
	}
}

void
AutomationLine::model_to_view_coord (double& x, double& y) const
{
	/* TODO: This should be more generic ... */
	if (alist->parameter().type() == GainAutomation ||
	    alist->parameter().type() == EnvelopeAutomation) {
		y = gain_to_slider_position_with_max (y, Config->get_max_gain());
	} else if (alist->parameter().type() == PanAzimuthAutomation ||
                   alist->parameter().type() == PanElevationAutomation ||
                   alist->parameter().type() == PanWidthAutomation) {
		// vertical coordinate axis reversal
		y = 1.0 - y;
	} else if (alist->parameter().type() == PluginAutomation) {
		y = (y - alist->get_min_y()) / (double)(alist->get_max_y()- alist->get_min_y());
	} else {
		y = y / (double)alist->parameter().max(); /* ... like this */
	}

	x = _time_converter->to (x) - _offset;
}

/** Called when our list has announced that its interpolation style has changed */
void
AutomationLine::interpolation_changed (AutomationList::InterpolationStyle style)
{
	if (style == AutomationList::Discrete) {
		show_all_control_points();
		line->hide();
	} else {
		hide_all_but_selected_control_points();
		line->show();
	}
}

void
AutomationLine::add_visible_control_point (uint32_t view_index, uint32_t pi, double tx, double ty, AutomationList::iterator model, uint32_t npoints)
{
	if (view_index >= control_points.size()) {

		/* make sure we have enough control points */

		ControlPoint* ncp = new ControlPoint (*this);
		ncp->set_size (control_point_box_size ());

		control_points.push_back (ncp);
	}

	ControlPoint::ShapeType shape;

	if (!terminal_points_can_slide) {
		if (pi == 0) {
			control_points[view_index]->set_can_slide(false);
			if (tx == 0) {
				shape = ControlPoint::Start;
			} else {
				shape = ControlPoint::Full;
			}
		} else if (pi == npoints - 1) {
			control_points[view_index]->set_can_slide(false);
			shape = ControlPoint::End;
		} else {
			control_points[view_index]->set_can_slide(true);
			shape = ControlPoint::Full;
		}
	} else {
		control_points[view_index]->set_can_slide(true);
		shape = ControlPoint::Full;
	}

	control_points[view_index]->reset (tx, ty, model, view_index, shape);

	/* finally, control visibility */

	if (_visible && points_visible) {
		control_points[view_index]->show ();
		control_points[view_index]->set_visible (true);
	} else {
		if (!points_visible) {
			control_points[view_index]->set_visible (false);
		}
	}
}

void
AutomationLine::add_always_in_view (double x)
{
	_always_in_view.push_back (x);
	alist->apply_to_points (*this, &AutomationLine::reset_callback);
}

void
AutomationLine::clear_always_in_view ()
{
	_always_in_view.clear ();
	alist->apply_to_points (*this, &AutomationLine::reset_callback);
}

void
AutomationLine::connect_to_list ()
{
	_list_connections.drop_connections ();

	alist->StateChanged.connect (_list_connections, invalidator (*this), boost::bind (&AutomationLine::list_changed, this), gui_context());

	alist->InterpolationChanged.connect (
		_list_connections, invalidator (*this), boost::bind (&AutomationLine::interpolation_changed, this, _1), gui_context()
		);
}

MementoCommandBinder<AutomationList>*
AutomationLine::memento_command_binder ()
{
	return new SimpleMementoCommandBinder<AutomationList> (*alist.get());
}

/** Set the maximum time that points on this line can be at, relative
 *  to the start of the track or region that it is on.
 */
void
AutomationLine::set_maximum_time (framecnt_t t)
{
	if (_maximum_time == t) {
		return;
	}

	_maximum_time = t;
	reset ();
}


/** @return min and max x positions of points that are in the list, in session frames */
pair<framepos_t, framepos_t>
AutomationLine::get_point_x_range () const
{
	pair<framepos_t, framepos_t> r (max_framepos, 0);

	for (AutomationList::const_iterator i = the_list()->begin(); i != the_list()->end(); ++i) {
		r.first = min (r.first, _time_converter->to ((*i)->when) + _offset + _time_converter->origin_b ());
		r.second = max (r.second, _time_converter->to ((*i)->when) + _offset + _time_converter->origin_b ());
	}

	return r;
}

void
AutomationLine::set_offset (framepos_t off)
{
	if (_offset == off) {
		return;
	}

	_offset = off;
	reset ();
}
