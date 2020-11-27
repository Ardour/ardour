/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006 Hans Fugal <hans@fugal.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2016 Nick Mainsbridge <mainsbridge@gmail.com>
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

#include <cmath>

#ifdef COMPILER_MSVC
#include <float.h>

// 'std::isnan()' is not available in MSVC.
#define isnan_local(val) (bool)_isnan((double)val)
#else
#define isnan_local std::isnan
#endif

#include <climits>
#include <vector>

#include "boost/shared_ptr.hpp"

#include "pbd/floating.h"
#include "pbd/memento_command.h"
#include "pbd/stl_delete.h"

#include "ardour/automation_list.h"
#include "ardour/dB.h"
#include "ardour/debug.h"
#include "ardour/parameter_types.h"
#include "ardour/tempo.h"

#include "temporal/range.h"

#include "evoral/Curve.h"

#include "canvas/debug.h"

#include "automation_line.h"
#include "control_point.h"
#include "gui_thread.h"
#include "rgb_macros.h"
#include "public_editor.h"
#include "selection.h"
#include "time_axis_view.h"
#include "point_selection.h"
#include "automation_time_axis.h"
#include "ui_config.h"

#include "ardour/event_type_map.h"
#include "ardour/session.h"
#include "ardour/value_as_string.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace Temporal;

#define TIME_TO_SAMPLES(x) (_distance_measure (x, Temporal::AudioTime))
#define SAMPLES_TO_TIME(x) (_distance_measure (x, alist->time_domain()))

/** @param converter A TimeConverter whose origin_b is the start time of the AutomationList in session samples.
 *  This will not be deleted by AutomationLine.
 */
AutomationLine::AutomationLine (const string&                              name,
                                TimeAxisView&                              tv,
                                ArdourCanvas::Item&                        parent,
                                boost::shared_ptr<AutomationList>          al,
                                const ParameterDescriptor&                 desc,
                                Temporal::DistanceMeasure const &          m)
	: trackview (tv)
	, _name (name)
	, alist (al)
	, _parent_group (parent)
	, _offset (0)
	, _maximum_time (timepos_t::max (al->time_domain()))
	, _fill (false)
	, _desc (desc)
	, _distance_measure (m)
{
	_visible = Line;

	update_pending = false;
	have_timeout = false;
	no_draw = false;
	_is_boolean = false;
	terminal_points_can_slide = true;
	_height = 0;

	group = new ArdourCanvas::Container (&parent, ArdourCanvas::Duple(0, 1.5));
	CANVAS_DEBUG_NAME (group, "region gain envelope group");

	line = new ArdourCanvas::PolyLine (group);
	CANVAS_DEBUG_NAME (line, "region gain envelope line");
	line->set_data ("line", this);
	line->set_outline_width (2.0);
	line->set_covers_threshold (4.0);

	line->Event.connect (sigc::mem_fun (*this, &AutomationLine::event_handler));

	trackview.session()->register_with_memento_command_factory(alist->id(), this);

	interpolation_changed (alist->interpolation ());

	connect_to_list ();
}

AutomationLine::~AutomationLine ()
{
	delete group; // deletes child items

	for (std::vector<ControlPoint *>::iterator i = control_points.begin(); i != control_points.end(); i++) {
		(*i)->unset_item ();
		delete *i;
	}
	control_points.clear ();
}

bool
AutomationLine::event_handler (GdkEvent* event)
{
	return PublicEditor::instance().canvas_line_event (event, line, this);
}

bool
AutomationLine::is_stepped() const
{
	return (_desc.toggled ||
	        (alist && alist->interpolation() == AutomationList::Discrete));
}

void
AutomationLine::update_visibility ()
{
	if (_visible & Line) {
		/* Only show the line when there are some points, otherwise we may show an out-of-date line
		   when automation points have been removed (the line will still follow the shape of the
		   old points).
		*/
		if (control_points.size() >= 2) {
			line->show();
		} else {
			line->hide ();
		}

		if (_visible & ControlPoints) {
			for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
				(*i)->show ();
			}
		} else if (_visible & SelectedControlPoints) {
			for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
				if ((*i)->selected()) {
					(*i)->show ();
				} else {
					(*i)->hide ();
				}
			}
		} else {
			for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
				(*i)->hide ();
			}
		}

	} else {
		line->hide ();
		for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
			if (_visible & ControlPoints) {
				(*i)->show ();
			} else {
				(*i)->hide ();
			}
		}
	}
}

bool
AutomationLine::get_uses_gain_mapping () const
{
	switch (_desc.type) {
		case GainAutomation:
		case BusSendLevel:
		case EnvelopeAutomation:
		case TrimAutomation:
			return true;
		default:
			return false;
	}
}

void
AutomationLine::hide ()
{
	/* leave control points setting unchanged, we are just hiding the
	   overall line
	*/

	set_visibility (AutomationLine::VisibleAspects (_visible & ~Line));
}

double
AutomationLine::control_point_box_size ()
{
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

		if (_fill) {
			line->set_fill_y1 (_height);
		} else {
			line->set_fill_y1 (0);
		}
		reset ();
	}
}

void
AutomationLine::set_line_color (uint32_t color)
{
	_line_color = color;
	line->set_outline_color (color);

	Gtkmm2ext::SVAModifier mod = UIConfiguration::instance().modifier ("automation line fill");

	line->set_fill_color ((color & 0xffffff00) + mod.a()*255);
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

	trackview.editor().begin_reversible_command (_("automation event move"));
	trackview.editor().session()->add_command (
		new MementoCommand<AutomationList> (memento_command_binder(), &get_state(), 0));

	cp.move_to (cp.get_x(), y, ControlPoint::Full);

	alist->freeze ();
	sync_model_with_view_point (cp);
	alist->thaw ();

	reset_line_coords (cp);

	if (line_points.size() > 1) {
		line->set_steps (line_points, is_stepped());
	}

	update_pending = false;

	trackview.editor().session()->add_command (
		new MementoCommand<AutomationList> (memento_command_binder(), 0, &alist->get_state()));

	trackview.editor().commit_reversible_command ();
	trackview.editor().session()->set_dirty ();
}

void
AutomationLine::reset_line_coords (ControlPoint& cp)
{
	if (cp.view_index() < line_points.size()) {
		line_points[cp.view_index()].x = cp.get_x ();
		line_points[cp.view_index()].y = cp.get_y ();
	}
}

bool
AutomationLine::sync_model_with_view_points (list<ControlPoint*> cp)
{
	update_pending = true;

	bool moved = false;
	for (list<ControlPoint*>::iterator i = cp.begin(); i != cp.end(); ++i) {
		moved = sync_model_with_view_point (**i) || moved;
	}

	return moved;
}

string
AutomationLine::get_verbose_cursor_string (double fraction) const
{
	return fraction_to_string (fraction);
}

string
AutomationLine::get_verbose_cursor_relative_string (double fraction, double delta) const
{
	std::string s = fraction_to_string (fraction);
	std::string d = delta_to_string (delta);
	return s + " (" + d + ")";
}

/**
 *  @param fraction y fraction
 *  @return string representation of this value, using dB if appropriate.
 */
string
AutomationLine::fraction_to_string (double fraction) const
{
	view_to_model_coord_y (fraction);
	return ARDOUR::value_as_string (_desc, fraction);
}

string
AutomationLine::delta_to_string (double delta) const
{
	if (!get_uses_gain_mapping () && _desc.logarithmic) {
		return "x " + ARDOUR::value_as_string (_desc, delta);
	} else {
		return "\u0394 " + ARDOUR::value_as_string (_desc, delta);
	}
}

/**
 *  @param s Value string in the form as returned by fraction_to_string.
 *  @return Corresponding y fraction.
 */
double
AutomationLine::string_to_fraction (string const & s) const
{
	double v;
	sscanf (s.c_str(), "%lf", &v);

	switch (_desc.type) {
		case GainAutomation:
		case BusSendLevel:
		case EnvelopeAutomation:
		case TrimAutomation:
			if (s == "-inf") { /* translation */
				v = 0;
			} else {
				v = dB_to_coefficient (v);
			}
			break;
		default:
			break;
	}
	model_to_view_coord_y (v);
	return v;
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
	trackview.editor().session()->add_command (
		new MementoCommand<AutomationList> (memento_command_binder(), &get_state(), 0));

	_drag_points.clear ();
	_drag_points.push_back (cp);

	if (cp->selected ()) {
		for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
			if (*i != cp && (*i)->selected()) {
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
	trackview.editor().session()->add_command (
		new MementoCommand<AutomationList> (memento_command_binder (), &get_state(), 0));

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
	trackview.editor().session()->add_command (
		new MementoCommand<AutomationList> (memento_command_binder(), state, 0));

	_drag_points = cp;
	start_drag_common (0, fraction);
}

struct ControlPointSorter
{
	bool operator() (ControlPoint const * a, ControlPoint const * b) const {
		if (floateq (a->get_x(), b->get_x(), 1)) {
			return a->view_index() < b->view_index();
		}
		return a->get_x() < b->get_x();
	}
};

AutomationLine::ContiguousControlPoints::ContiguousControlPoints (AutomationLine& al)
	: line (al), before_x (0), after_x (DBL_MAX)
{
}

void
AutomationLine::ContiguousControlPoints::compute_x_bounds (PublicEditor& e)
{
	uint32_t sz = size();

	if (sz > 0 && sz < line.npoints()) {
		const TempoMap::SharedPtr map (TempoMap::use());

		/* determine the limits on x-axis motion for this
		   contiguous range of control points
		*/

		if (front()->view_index() > 0) {
			before_x = line.nth (front()->view_index() - 1)->get_x();

			const samplepos_t pos = e.pixel_to_sample(before_x);
			const TempoMetric& metric = map->metric_at (pos);
			const samplecnt_t len = ceil (metric.samples_per_bar (pos) / (Temporal::ticks_per_beat * metric.meter().divisions_per_bar()));
			const double one_tick_in_pixels = e.sample_to_pixel_unrounded (len);

			before_x += one_tick_in_pixels;
		}

		/* if our last point has a point after it in the line,
		   we have an "after" bound
		*/

		if (back()->view_index() < (line.npoints() - 1)) {
			after_x = line.nth (back()->view_index() + 1)->get_x();

			const samplepos_t pos = e.pixel_to_sample(after_x);
			const TempoMetric& metric = map->metric_at (pos);
			const samplecnt_t len = ceil (metric.samples_per_bar (pos) / (Temporal::ticks_per_beat * metric.meter().divisions_per_bar()));
			const double one_tick_in_pixels = e.sample_to_pixel_unrounded (len);

			after_x -= one_tick_in_pixels;
		}
	}
}

double
AutomationLine::ContiguousControlPoints::clamp_dx (double dx)
{
	if (empty()) {
		return dx;
	}

	/* get the maximum distance we can move any of these points along the x-axis
	 */

	double tx; /* possible position a point would move to, given dx */
	ControlPoint* cp;

	if (dx > 0) {
		/* check the last point, since we're moving later in time */
		cp = back();
	} else {
		/* check the first point, since we're moving earlier in time */
		cp = front();
	}

	tx = cp->get_x() + dx; // new possible position if we just add the motion
	tx = max (tx, before_x); // can't move later than following point
	tx = min (tx, after_x);  // can't move earlier than preceeding point
	return  tx - cp->get_x ();
}

void
AutomationLine::ContiguousControlPoints::move (double dx, double dvalue)
{
	for (std::list<ControlPoint*>::iterator i = begin(); i != end(); ++i) {
		// compute y-axis delta
		double view_y = 1.0 - (*i)->get_y() / line.height();
		line.view_to_model_coord_y (view_y);
		line.apply_delta (view_y, dvalue);
		line.model_to_view_coord_y (view_y);
		view_y = (1.0 - view_y) * line.height();

		(*i)->move_to ((*i)->get_x() + dx, view_y, ControlPoint::Full);
		line.reset_line_coords (**i);
	}
}

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

	/* they are probably ordered already, but we have to make sure */

	_drag_points.sort (ControlPointSorter());
}


/** Should be called to indicate motion during a drag.
 *  @param x New x position of the drag in canvas units, or undefined if ignore_x == true.
 *  @param fraction New y fraction.
 *  @return x position and y fraction that were actually used (once clamped).
 */
pair<float, float>
AutomationLine::drag_motion (double const x, float fraction, bool ignore_x, bool with_push, uint32_t& final_index)
{
	if (_drag_points.empty()) {
		return pair<double,float> (fraction, _desc.is_linear () ? 0 : 1);
	}

	double dx = ignore_x ? 0 : (x - _drag_x);
	double dy = fraction - _last_drag_fraction;

	if (!_drag_had_movement) {

		/* "first move" ... do some stuff that we don't want to do if
		   no motion ever took place, but need to do before we handle
		   motion.
		*/

		/* partition the points we are dragging into (potentially several)
		 * set(s) of contiguous points. this will not happen with a normal
		 * drag, but if the user does a discontiguous selection, it can.
		 */

		uint32_t expected_view_index = 0;
		CCP contig;

		for (list<ControlPoint*>::iterator i = _drag_points.begin(); i != _drag_points.end(); ++i) {
			if (i == _drag_points.begin() || (*i)->view_index() != expected_view_index) {
				contig.reset (new ContiguousControlPoints (*this));
				contiguous_points.push_back (contig);
			}
			contig->push_back (*i);
			expected_view_index = (*i)->view_index() + 1;
		}

		if (contiguous_points.back()->empty()) {
			contiguous_points.pop_back ();
		}

		for (vector<CCP>::iterator ccp = contiguous_points.begin(); ccp != contiguous_points.end(); ++ccp) {
			(*ccp)->compute_x_bounds (trackview.editor());
		}
		_drag_had_movement = true;
	}

	/* OK, now on to the stuff related to *this* motion event. First, for
	 * each contiguous range, figure out the maximum x-axis motion we are
	 * allowed (because of neighbouring points that are not moving.
	 *
	 * if we are moving forwards with push, we don't need to do this,
	 * since all later points will move too.
	 */

	if (dx < 0 || ((dx > 0) && !with_push)) {
		for (vector<CCP>::iterator ccp = contiguous_points.begin(); ccp != contiguous_points.end(); ++ccp) {
			double dxt = (*ccp)->clamp_dx (dx);
			if (fabs (dxt) < fabs (dx)) {
				dx = dxt;
			}
		}
	}

	/* compute deflection */
	double delta_value;
	{
		double value0 = _last_drag_fraction;
		double value1 = _last_drag_fraction + dy;
		view_to_model_coord_y (value0);
		view_to_model_coord_y (value1);
		delta_value = compute_delta (value0, value1);
	}

	/* special case -inf */
	if (delta_value == 0 && dy > 0 && !_desc.is_linear ()) {
		assert (_desc.lower == 0);
		delta_value = 1.0;
	}

	/* clamp y */
	for (list<ControlPoint*>::iterator i = _drag_points.begin(); i != _drag_points.end(); ++i) {
		double vy = 1.0 - (*i)->get_y() / _height;
		view_to_model_coord_y (vy);
		const double orig = vy;
		apply_delta (vy, delta_value);
		if (vy < _desc.lower) {
			delta_value = compute_delta (orig, _desc.lower);
		}
		if (vy > _desc.upper) {
			delta_value = compute_delta (orig, _desc.upper);
		}
	}

	if (dx || dy) {
		/* and now move each section */
		for (vector<CCP>::iterator ccp = contiguous_points.begin(); ccp != contiguous_points.end(); ++ccp) {
			(*ccp)->move (dx, delta_value);
		}

		if (with_push) {
			final_index = contiguous_points.back()->back()->view_index () + 1;
			ControlPoint* p;
			uint32_t i = final_index;
			while ((p = nth (i)) != 0 && p->can_slide()) {
				p->move_to (p->get_x() + dx, p->get_y(), ControlPoint::Full);
				reset_line_coords (*p);
				++i;
			}
		}

		/* update actual line coordinates (will queue a redraw) */

		if (line_points.size() > 1) {
			line->set_steps (line_points, is_stepped());
		}
	}

	/* calculate effective delta */
	ControlPoint* cp = _drag_points.front();
	double vy = 1.0 - cp->get_y() / (double)_height;
	view_to_model_coord_y (vy);
	float val = (*(cp->model ()))->value;
	float effective_delta = _desc.compute_delta (val, vy);
	/* special case recovery from -inf */
	if (val == 0 && effective_delta == 0 && vy > 0) {
		assert (!_desc.is_linear ());
		effective_delta = HUGE_VAL; // +Infinity
	}

	double const result_frac = _last_drag_fraction + dy;
	_drag_distance += dx;
	_drag_x += dx;
	_last_drag_fraction = result_frac;
	did_push = with_push;

	return pair<float, float> (result_frac, effective_delta);
}

/** Should be called to indicate the end of a drag */
void
AutomationLine::end_drag (bool with_push, uint32_t final_index)
{
	if (!_drag_had_movement) {
		return;
	}

	alist->freeze ();
	bool moved = sync_model_with_view_points (_drag_points);

	if (with_push) {
		ControlPoint* p;
		uint32_t i = final_index;
		while ((p = nth (i)) != 0 && p->can_slide()) {
			moved = sync_model_with_view_point (*p) || moved;
			++i;
		}
	}

	alist->thaw ();

	update_pending = false;

	if (moved) {
		/* A point has moved as a result of sync (clamped to integer or boolean
		   value), update line accordingly. */
		line->set_steps (line_points, is_stepped());
	}

	trackview.editor().session()->add_command (
		new MementoCommand<AutomationList>(memento_command_binder (), 0, &alist->get_state()));

	trackview.editor().session()->set_dirty ();
	did_push = false;

	contiguous_points.clear ();
}

bool
AutomationLine::sync_model_with_view_point (ControlPoint& cp)
{
	/* find out where the visual control point is.
	   initial results are in canvas units. ask the
	   line to convert them to something relevant.
	*/

	double view_x = cp.get_x();
	double view_y = 1.0 - cp.get_y() / (double)_height;

	/* if xval has not changed, set it directly from the model to avoid rounding errors */

	timepos_t model_x = (*cp.model())->when;

	if (view_x != trackview.editor().time_to_pixel_unrounded (model_x.earlier (_offset))) {
		/* convert from view coordinates, via pixels->samples->timepos_t
		 */
		const timecnt_t p = timecnt_t (trackview.editor().pixel_to_sample (view_x), timepos_t()); /* samples */
		model_x = SAMPLES_TO_TIME (p + _offset); /* correct time domain for list */
	}

	update_pending = true;

	view_to_model_coord_y (view_y);

	alist->modify (cp.model(), model_x, view_y);

	/* convert back from model to view y for clamping position (for integer/boolean/etc) */
	model_to_view_coord_y (view_y);
	const double point_y = _height - (view_y * _height);
	if (point_y != cp.get_y()) {
		cp.move_to (cp.get_x(), point_y, ControlPoint::Full);
		reset_line_coords (cp);
		return true;
	}

	return false;
}

bool
AutomationLine::control_points_adjacent (double xval, uint32_t & before, uint32_t& after)
{
	ControlPoint *bcp = 0;
	ControlPoint *acp = 0;
	double unit_xval;

	unit_xval = trackview.editor().sample_to_pixel_unrounded (xval);

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
	// If the list is not empty, and the point is the last point in the list

	if (alist->empty()) {
		return false;
	}

	AutomationList::const_iterator i = alist->end();
	--i;

	if (cp.model() == i) {
		return true;
	}

	return false;
}

bool
AutomationLine::is_first_point (ControlPoint& cp)
{
	// If the list is not empty, and the point is the first point in the list

	if (!alist->empty() && cp.model() == alist->begin()) {
		return true;
	}

	return false;
}

// This is copied into AudioRegionGainLine
void
AutomationLine::remove_point (ControlPoint& cp)
{
	trackview.editor().begin_reversible_command (_("remove control point"));
	XMLNode &before = alist->get_state();

	trackview.editor ().get_selection ().clear_points ();
	alist->erase (cp.model());

	trackview.editor().session()->add_command(
		new MementoCommand<AutomationList> (memento_command_binder (), &before, &alist->get_state()));

	trackview.editor().commit_reversible_command ();
	trackview.editor().session()->set_dirty ();
}

/** Get selectable points within an area.
 *  @param start Start position in session samples.
 *  @param end End position in session samples.
 *  @param bot Bottom y range, as a fraction of line height, where 0 is the bottom of the line.
 *  @param top Top y range, as a fraction of line height, where 0 is the bottom of the line.
 *  @param result Filled in with selectable things; in this case, ControlPoints.
 */
void
AutomationLine::get_selectables (timepos_t const & start, timepos_t const & end, double botfrac, double topfrac, list<Selectable*>& results)
{
	/* convert fractions to display coordinates with 0 at the top of the track */
	double const bot_track = (1 - topfrac) * trackview.current_height ();
	double const top_track = (1 - botfrac) * trackview.current_height ();

	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {

		timepos_t const session_samples_when = timepos_t (session_sample_position ((*i)->model()));

		if (session_samples_when >= start && session_samples_when <= end && (*i)->get_y() >= bot_track && (*i)->get_y() <= top_track) {
			results.push_back (*i);
		}
	}
}

void
AutomationLine::get_inverted_selectables (Selection&, list<Selectable*>& /*results*/)
{
	// hmmm ....
}

void
AutomationLine::set_selected_points (PointSelection const & points)
{
	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		(*i)->set_selected (false);
	}

	for (PointSelection::const_iterator i = points.begin(); i != points.end(); ++i) {
		(*i)->set_selected (true);
	}

	if (points.empty()) {
		remove_visibility (SelectedControlPoints);
	} else {
		add_visibility (SelectedControlPoints);
	}

	set_colors ();
}

void
AutomationLine::set_colors ()
{
	set_line_color (UIConfiguration::instance().color ("automation line"));
	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		(*i)->set_color ();
	}
}

void
AutomationLine::list_changed ()
{
	DEBUG_TRACE (DEBUG::Automation, string_compose ("\tline changed, existing update pending? %1\n", update_pending));

	if (!update_pending) {
		update_pending = true;
		Gtkmm2ext::UI::instance()->call_slot (invalidator (*this), boost::bind (&AutomationLine::queue_reset, this));
	}
}

void
AutomationLine::reset_callback (const Evoral::ControlList& events)
{
	uint32_t vp = 0;
	uint32_t pi = 0;
	uint32_t np;

	if (events.empty()) {
		for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
			delete *i;
		}
		control_points.clear ();
		line->hide();
		return;
	}

	/* hide all existing points, and the line */

	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		(*i)->hide();
	}

	line->hide ();
	np = events.size();

	Evoral::ControlList& e = const_cast<Evoral::ControlList&> (events);

	for (AutomationList::iterator ai = e.begin(); ai != e.end(); ++ai, ++pi) {

		double ty = (*ai)->value;

		/* convert from model coordinates to canonical view coordinates */

		timepos_t tx = model_to_view_coord (**ai, ty);

		if (isnan_local (ty)) {
			warning << string_compose (_("Ignoring illegal points on AutomationLine \"%1\""),
			                           _name) << endmsg;
			continue;
		}

		if (tx >= timepos_t::max (tx.time_domain()) || tx.negative() || tx >= _maximum_time) {
			continue;
		}

		/* convert x-coordinate to a canvas unit coordinate (this takes
		 * zoom and scroll into account).
		 */

		double px = trackview.editor().time_to_pixel_unrounded (tx);

		/* convert from canonical view height (0..1.0) to actual
		 * height coordinates (using X11's top-left rooted system)
		 */

		ty = _height - (ty * _height);

		add_visible_control_point (vp, pi, px, ty, ai, np);
		vp++;
	}

	/* discard extra CP's to avoid confusing ourselves */

	while (control_points.size() > vp) {
		ControlPoint* cp = control_points.back();
		control_points.pop_back ();
		delete cp;
	}

	if (!terminal_points_can_slide) {
		control_points.back()->set_can_slide(false);
	}

	if (vp > 1) {

		/* reset the line coordinates given to the CanvasLine */

		while (line_points.size() < vp) {
			line_points.push_back (ArdourCanvas::Duple (0,0));
		}

		while (line_points.size() > vp) {
			line_points.pop_back ();
		}

		for (uint32_t n = 0; n < vp; ++n) {
			line_points[n].x = control_points[n]->get_x();
			line_points[n].y = control_points[n]->get_y();
		}

		line->set_steps (line_points, is_stepped());

		update_visibility ();
	}

	set_selected_points (trackview.editor().get_selection().points);
}

void
AutomationLine::reset ()
{
	DEBUG_TRACE (DEBUG::Automation, "\t\tLINE RESET\n");
	update_pending = false;
	have_timeout = false;

	if (no_draw) {
		return;
	}

	/* TODO: abort any drags in progress, e.g. draging points while writing automation
	 * (the control-point model, used by AutomationLine::drag_motion, will be invalid).
	 *
	 * Note: reset() may also be called from an aborted drag (LineDrag::aborted)
	 * maybe abort in list_changed(), interpolation_changed() and ... ?
	 * XXX
	 */

	alist->apply_to_points (*this, &AutomationLine::reset_callback);
}

void
AutomationLine::queue_reset ()
{
	/* this must be called from the GUI thread */

	if (trackview.editor().session()->transport_rolling() && alist->automation_write()) {
		/* automation write pass ... defer to a timeout */
		/* redraw in 1/4 second */
		if (!have_timeout) {
			DEBUG_TRACE (DEBUG::Automation, "\tqueue timeout\n");
			Glib::signal_timeout().connect (sigc::bind_return (sigc::mem_fun (*this, &AutomationLine::reset), false), 250);
			have_timeout = true;
		} else {
			DEBUG_TRACE (DEBUG::Automation, "\ttimeout already queued, change ignored\n");
		}
	} else {
		reset ();
	}
}

void
AutomationLine::clear ()
{
	/* parent must create and commit command */
	XMLNode &before = alist->get_state();
	alist->clear();

	trackview.editor().session()->add_command (
		new MementoCommand<AutomationList> (memento_command_binder (), &before, &alist->get_state()));
}

void
AutomationLine::set_list (boost::shared_ptr<ARDOUR::AutomationList> list)
{
	alist = list;
	queue_reset ();
	connect_to_list ();
}

void
AutomationLine::add_visibility (VisibleAspects va)
{
	VisibleAspects old = _visible;

	_visible = VisibleAspects (_visible | va);

	if (old != _visible) {
		update_visibility ();
	}
}

void
AutomationLine::set_visibility (VisibleAspects va)
{
	if (_visible != va) {
		_visible = va;
		update_visibility ();
	}
}

void
AutomationLine::remove_visibility (VisibleAspects va)
{
	VisibleAspects old = _visible;

	_visible = VisibleAspects (_visible & ~va);

	if (old != _visible) {
		update_visibility ();
	}
}

void
AutomationLine::track_entered()
{
	add_visibility (ControlPoints);
}

void
AutomationLine::track_exited()
{
	remove_visibility (ControlPoints);
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

Temporal::timepos_t
AutomationLine::view_to_model_coord (double x, double& y) const
{
	assert (alist->time_domain() != Temporal::BarTime);

	view_to_model_coord_y (y);

	Temporal::timepos_t w;

#warning NUTEMPO FIX ME ... this accepts view coordinate as double and things it can infer beats etc

	switch (alist->time_domain()) {
	case Temporal::AudioTime:
		return timepos_t (samplepos_t (x));
		break;
	case Temporal::BeatTime:
		return timepos_t (Temporal::Beats::from_double (x));
		break;
	default:
		/*NOTREACHED*/
		break;
	}

	/*NOTREACHED*/
	return timepos_t();
}

void
AutomationLine::view_to_model_coord_y (double& y) const
{
	if (alist->default_interpolation () != alist->interpolation()) {
		switch (alist->interpolation()) {
			case AutomationList::Discrete:
				/* toggles and MIDI only -- see is_stepped() */
				assert (alist->default_interpolation () == AutomationList::Linear);
				break;
			case AutomationList::Linear:
				y = y * (_desc.upper - _desc.lower) + _desc.lower;
				return;
			default:
				/* types that default to linear, can't be use
				 * Logarithmic or Exponential interpolation.
				 * "Curved" is invalid for automation (only x-fads)
				 */
				assert (0);
				break;
		}
	}
	y = _desc.from_interface (y);
}

double
AutomationLine::compute_delta (double from, double to) const
{
	return _desc.compute_delta (from, to);
}

void
AutomationLine::apply_delta (double& val, double delta) const
{
	if (val == 0 && !_desc.is_linear () && delta >= 1.0) {
		/* recover from -inf */
		val = 1.0 / _height;
		view_to_model_coord_y (val);
		return;
	}
	val = _desc.apply_delta (val, delta);
}

void
AutomationLine::model_to_view_coord_y (double& y) const
{
	if (alist->default_interpolation () != alist->interpolation()) {
		switch (alist->interpolation()) {
			case AutomationList::Discrete:
				/* toggles and MIDI only -- see is_stepped */
				assert (alist->default_interpolation () == AutomationList::Linear);
				break;
			case AutomationList::Linear:
				y = (y - _desc.lower) / (_desc.upper - _desc.lower);
				return;
			default:
				/* types that default to linear, can't be use
				 * Logarithmic or Exponential interpolation.
				 * "Curved" is invalid for automation (only x-fads)
				 */
				assert (0);
				break;
		}
	}
	y = _desc.to_interface (y);
}

timepos_t
AutomationLine::model_to_view_coord (Evoral::ControlEvent const & ev, double& y) const
{
	Temporal::timepos_t w (ev.when);
	model_to_view_coord_y (y);
	return (w).earlier (_offset);
}

/** Called when our list has announced that its interpolation style has changed */
void
AutomationLine::interpolation_changed (AutomationList::InterpolationStyle style)
{
	if (line_points.size() > 1) {
		reset ();
		line->set_steps(line_points, is_stepped());
	}
}

void
AutomationLine::add_visible_control_point (uint32_t view_index, uint32_t pi, double tx, double ty,
                                           AutomationList::iterator model, uint32_t npoints)
{
	ControlPoint::ShapeType shape;

	if (view_index >= control_points.size()) {

		/* make sure we have enough control points */

		ControlPoint* ncp = new ControlPoint (*this);
		ncp->set_size (control_point_box_size ());

		control_points.push_back (ncp);
	}

	if (!terminal_points_can_slide) {
		if (pi == 0) {
			control_points[view_index]->set_can_slide (false);
			if (tx == 0) {
				shape = ControlPoint::Start;
			} else {
				shape = ControlPoint::Full;
			}
		} else if (pi == npoints - 1) {
			control_points[view_index]->set_can_slide (false);
			shape = ControlPoint::End;
		} else {
			control_points[view_index]->set_can_slide (true);
			shape = ControlPoint::Full;
		}
	} else {
		control_points[view_index]->set_can_slide (true);
		shape = ControlPoint::Full;
	}

	control_points[view_index]->reset (tx, ty, model, view_index, shape);

	/* finally, control visibility */

	if (_visible & ControlPoints) {
		control_points[view_index]->show ();
	} else {
		control_points[view_index]->hide ();
	}
}

void
AutomationLine::connect_to_list ()
{
	_list_connections.drop_connections ();

	alist->StateChanged.connect (_list_connections, invalidator (*this), boost::bind (&AutomationLine::list_changed, this), gui_context());

	alist->InterpolationChanged.connect (
		_list_connections, invalidator (*this), boost::bind (&AutomationLine::interpolation_changed, this, _1), gui_context());
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
AutomationLine::set_maximum_time (Temporal::timepos_t const & t)
{
	if (_maximum_time == t) {
		return;
	}

	_maximum_time = t;
	reset ();
}


/** @return min and max x positions of points that are in the list, in session samples */
pair<timepos_t, timepos_t>
AutomationLine::get_point_x_range () const
{
	pair<timepos_t, timepos_t> r (timepos_t::max (the_list()->time_domain()), timepos_t::zero (the_list()->time_domain()));

	for (AutomationList::const_iterator i = the_list()->begin(); i != the_list()->end(); ++i) {
		r.first = min (r.first, session_position (i));
		r.second = max (r.second, session_position (i));
	}

	return r;
}

samplepos_t
AutomationLine::session_sample_position (AutomationList::const_iterator p) const
{
	return (*p)->when.samples() + _offset.samples() + _distance_measure.origin().samples();
}

timepos_t
AutomationLine::session_position (AutomationList::const_iterator p) const
{
	return (*p)->when + _offset + _distance_measure.origin();
}

void
AutomationLine::set_offset (timecnt_t const & off)
{
	if (_offset == off) {
		return;
	}

	_offset = off;
	reset ();
}

void
AutomationLine::set_distance_measure_origin (timepos_t const & pos)
{
	_distance_measure.set_origin (pos);
}
