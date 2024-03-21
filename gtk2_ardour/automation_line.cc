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

#define SAMPLES_TO_TIME(x) (get_origin().distance (x))

/** @param converter A TimeConverter whose origin_b is the start time of the AutomationList in session samples.
 *  This will not be deleted by AutomationLine.
 */
AutomationLine::AutomationLine (const string&                              name,
                                TimeAxisView&                              tv,
                                ArdourCanvas::Item&                        parent,
                                std::shared_ptr<AutomationList>          al,
                                const ParameterDescriptor&                 desc)
	: trackview (tv)
	, _name (name)
	, _height (0)
	, _view_index_offset (0)
	, alist (al)
	, _visible (Line)
	, terminal_points_can_slide (true)
	, update_pending (false)
	, have_reset_timeout (false)
	, no_draw (false)
	, _is_boolean (false)
	, _parent_group (parent)
	, _offset (0)
	, _maximum_time (timepos_t::max (al->time_domain()))
	, _fill (false)
	, _desc (desc)
{
	group = new ArdourCanvas::Container (&parent, ArdourCanvas::Duple(0, 1.5));
	CANVAS_DEBUG_NAME (group, "region gain envelope group");

	line = new ArdourCanvas::PolyLine (group);
	CANVAS_DEBUG_NAME (line, "region gain envelope line");
	line->set_data ("line", this);
	line->set_data ("trackview", &trackview);
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

timepos_t
AutomationLine::get_origin() const
{
	/* this is the default for all non-derived AutomationLine classes: the
	   origin is zero, in whatever time domain the list we represent uses.
	*/
	return timepos_t (the_list()->time_domain());
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
		if (line_points.size() >= 2) {
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
		case SurroundSendLevel:
		case InsertReturnLevel:
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
	float uiscale = UIConfiguration::instance().get_ui_scale();
	uiscale = std::max<float> (1.f, powf (uiscale, 1.71));

	if (_height > TimeAxisView::preset_height (HeightLarger)) {
		return rint (8.0 * uiscale);
	} else if (_height > (guint32) TimeAxisView::preset_height (HeightNormal)) {
		return rint (6.0 * uiscale);
	}
	return rint (4.0 * uiscale);
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
AutomationLine::modify_points_y (std::vector<ControlPoint*> const& cps, double y)
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

	alist->freeze ();
	for (auto const& cp : cps) {
		cp->move_to (cp->get_x(), y, ControlPoint::Full);
		sync_model_with_view_point (*cp);
	}
	alist->thaw ();

	for (auto const& cp : cps) {
		reset_line_coords (*cp);
	}

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
		line_points[cp.view_index() + _view_index_offset].x = cp.get_x ();
		line_points[cp.view_index() + _view_index_offset].y = cp.get_y ();
	}
}

bool
AutomationLine::sync_model_with_view_points (list<ControlPoint*> cp)
{
	update_pending = true;

	bool moved = false;
	for (auto const & vp : cp) {
		moved = sync_model_with_view_point (*vp) || moved;
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
		return u8"\u0394 " + ARDOUR::value_as_string (_desc, delta);
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
		case SurroundSendLevel:
		case InsertReturnLevel:
			if (s == "-inf") { /* translation */
				v = 0;
			} else {
				v = dB_to_coefficient (v);
			}
			break;
		default:
			break;
	}
	return model_to_view_coord_y (v);
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
	: line (al), before_x (timepos_t (line.the_list()->time_domain())), after_x (timepos_t::max (line.the_list()->time_domain()))
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
			before_x = (*line.nth (front()->view_index() - 1)->model())->when;
			before_x += timepos_t (64);
		}

		/* if our last point has a point after it in the line,
		   we have an "after" bound
		*/

		if (back()->view_index() < (line.npoints() - 1)) {
			after_x = (*line.nth (back()->view_index() + 1)->model())->when;
			after_x.shift_earlier (timepos_t (64));
		}
	}
}

Temporal::timecnt_t
AutomationLine::ContiguousControlPoints::clamp_dt (timecnt_t const & dt, timepos_t const & line_limit)
{
	if (empty()) {
		return dt;
	}

	/* get the maximum distance we can move any of these points along the x-axis
	 */

	ControlPoint* reference_point;

	if (dt.magnitude() > 0) {
		/* check the last point, since we're moving later in time */
		reference_point = back();
	} else {
		/* check the first point, since we're moving earlier in time */
		reference_point = front();
	}

	/* possible position the "reference" point would move to, given dx */
	Temporal::timepos_t possible_pos = (*reference_point->model())->when + dt; // new possible position if we just add the motion

	/* Now clamp that position so that:
	 *
	 * - it is not before the origin (zero)
	 * - it is not beyond the line's own limit (e.g. for region automation)
	 * - it is not before the preceding point
	 * - it is not after the following point
	 */

	possible_pos = max (possible_pos, Temporal::timepos_t (possible_pos.time_domain()));
	possible_pos = min (possible_pos, line_limit);

	possible_pos = max (possible_pos, before_x); // can't move later than following point
	possible_pos = min (possible_pos, after_x);  // can't move earlier than preceding point

	return (*reference_point->model())->when.distance (possible_pos);
}

void
AutomationLine::ContiguousControlPoints::move (timecnt_t const & dt, double dvalue)
{
	for (auto & cp : *this) {
		// compute y-axis delta
		double view_y = 1.0 - cp->get_y() / line.height();
		line.view_to_model_coord_y (view_y);
		line.apply_delta (view_y, dvalue);
		view_y = line.model_to_view_coord_y (view_y);
		view_y = (1.0 - view_y) * line.height();

		cp->move_to (line.dt_to_dx ((*cp->model())->when, dt), view_y, ControlPoint::Full);
		line.reset_line_coords (*cp);
	}
}

/** Common parts of starting a drag.
 *  @param x Starting x position in units, or 0 if x is being ignored.
 *  @param fraction Starting y position (as a fraction of the track height, where 0 is the bottom and 1 the top)
 */
void
AutomationLine::start_drag_common (double x, float fraction)
{
	_last_drag_fraction = fraction;
	_drag_had_movement = false;
	did_push = false;

	/* they are probably ordered already, but we have to make sure */

	_drag_points.sort (ControlPointSorter());
}

/** Takes a relative-to-origin position, moves it by dt, and returns a
 *  relative-to-origin pixel count.
 */
double
AutomationLine::dt_to_dx (timepos_t const & pos, timecnt_t const & dt)
{
	/* convert a shift of pos by dt into an absolute timepos */
	timepos_t const new_pos ((pos + dt + get_origin()).shift_earlier (offset()));
	/* convert to pixels */
	double px = trackview.editor().time_to_pixel_unrounded (new_pos);
	/* convert back to pixels-relative-to-origin */
	px -= trackview.editor().time_to_pixel_unrounded (get_origin());
	return px;
}

/** Should be called to indicate motion during a drag.
 *  @param x New x position of the drag in canvas units relative to origin, or undefined if ignore_x == true.
 *  @param fraction New y fraction.
 *  @return x position and y fraction that were actually used (once clamped).
 */
pair<float, float>
AutomationLine::drag_motion (timecnt_t const & pdt, float fraction, bool ignore_x, bool with_push, uint32_t& final_index)
{
	if (_drag_points.empty()) {
		return pair<double,float> (fraction, _desc.is_linear () ? 0 : 1);
	}

	timecnt_t dt (pdt);

	if (ignore_x) {
		dt = timecnt_t (pdt.time_domain());
	}

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

		for (auto const & ccp : contiguous_points) {
			ccp->compute_x_bounds (trackview.editor());
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

	if (dt.is_negative() || (dt.is_positive() && !with_push)) {
		const timepos_t line_limit = get_origin() + maximum_time() + _offset;
		for (auto const & ccp : contiguous_points){
			dt = ccp->clamp_dt (dt, line_limit);
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

	if (!dt.is_zero() || dy) {
		/* and now move each section */


		for (vector<CCP>::iterator ccp = contiguous_points.begin(); ccp != contiguous_points.end(); ++ccp) {
			(*ccp)->move (dt, delta_value);
		}

		if (with_push) {
			final_index = contiguous_points.back()->back()->view_index () + 1;
			ControlPoint* p;
			uint32_t i = final_index;

			while ((p = nth (i)) != 0 && p->can_slide()) {

				p->move_to (dt_to_dx ((*p->model())->when, dt), p->get_y(), ControlPoint::Full);
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

/**
 *
 * get model coordinates synced with (possibly changed) view coordinates.
 *
 * For example, we call this in ::end_drag(), when we have probably moved a
 * point in the view, and now want to "push" that change back into the
 * corresponding model point.
 */
bool
AutomationLine::sync_model_with_view_point (ControlPoint& cp)
{
	/* find out where the visual control point is.
	 * ControlPoint uses canvas-units. The origin
	 * is the RegionView's top-left corner.
	 */
	double view_x = cp.get_x();

	/* model time is relative to the Region (regardless of region->start offset) */
	timepos_t model_time = (*cp.model())->when;

	const timepos_t origin (get_origin());

	/* convert to absolute time on timeline */
	const timepos_t absolute_time = model_time + origin;

	/* now convert to pixels relative to start of region, which matches view_x */
	const double model_x = trackview.editor().time_to_pixel_unrounded (absolute_time) - trackview.editor().time_to_pixel_unrounded (origin);

	if (view_x != model_x) {

		/* convert the current position in the view (units:
		 * region-relative pixels) into samples, then use that to
		 * create a timecnt_t that measures the distance from the
		 * origin for this line.
		 *
		 * Note that the offset and origin is irrelevant here,
		 * pixel_to_sample() islinear only depending on zoom level.
		 */

		const timepos_t view_samples (trackview.editor().pixel_to_sample (view_x));

		/* measure distance from RegionView origin (this preserves time domain) */

		if (model_time.time_domain() == Temporal::AudioTime) {
			model_time = timepos_t (timecnt_t (view_samples, origin).samples());
		} else {
			model_time = timepos_t (timecnt_t (view_samples, origin).beats());
		}

		/* convert RegionView to Region position (account for region->start() _offset) */
		model_time += _offset;
	}

	update_pending = true;

	double view_y = 1.0 - cp.get_y() / (double)_height;
	view_to_model_coord_y (view_y);

	alist->modify (cp.model(), model_time, view_y);

	/* convert back from model to view y for clamping position (for integer/boolean/etc) */
	view_y = model_to_view_coord_y (view_y);
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
	double const bot_track = (1 - topfrac) * trackview.current_height (); // this should StreamView::child_height () for RegionGain
	double const top_track = (1 - botfrac) * trackview.current_height (); //  --"--

	for (auto const & cp : control_points) {

		const timepos_t w = session_position ((*cp->model())->when);

		if (w >= start && w <= end && cp->get_y() >= bot_track && cp->get_y() <= top_track) {
			results.push_back (cp);
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
AutomationLine::tempo_map_changed ()
{
	if (alist->time_domain() != Temporal::BeatTime) {
		return;
	}

	reset ();
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
		line_points.clear ();
		return;
	}

	/* hide all existing points, and the line */

	for (vector<ControlPoint*>::iterator i = control_points.begin(); i != control_points.end(); ++i) {
		(*i)->hide();
	}

	line->hide ();
	np = events.size();

	Evoral::ControlList& e (const_cast<Evoral::ControlList&> (events));
	AutomationList::iterator preceding (e.end());
	AutomationList::iterator following (e.end());

	for (AutomationList::iterator ai = e.begin(); ai != e.end(); ++ai, ++pi) {

		/* drop points outside our range */

		if (((*ai)->when < _offset)) {
			preceding = ai;
			continue;
		}

		if ((*ai)->when >= _offset + _maximum_time) {
			following = ai;
			break;
		}

		double ty = model_to_view_coord_y ((*ai)->value);

		if (isnan_local (ty)) {
			warning << string_compose (_("Ignoring illegal points on AutomationLine \"%1\""), _name) << endmsg;
			continue;
		}

		/* convert from canonical view height (0..1.0) to actual
		 * height coordinates (using X11's top-left rooted system)
		 */

		ty = _height - (ty * _height);

		/* tx is currently the distance of this point from
		 * _offset, which may be either:
		 *
		 * a) zero, for an automation line not connected to a
		 * region
		 *
		 * b) some non-zero value, corresponding to the start
		 * of the region within its source(s). Remember that
		 * this start is an offset within the source, not a
		 * position on the timeline.
		 *
		 * We need to convert tx to a global position, and to
		 * do that we need to measure the distance from the
		 * result of get_origin(), which tells ut the timeline
		 * position of _offset
		 */

		timecnt_t tx = model_to_view_coord_x ((*ai)->when);

		/* convert x-coordinate to a canvas unit coordinate (this takes
		 * zoom and scroll into account).
		 */

		double px = trackview.editor().duration_to_pixels_unrounded (tx);
		add_visible_control_point (vp, pi, px, ty, ai, np);
		vp++;
	}

	/* discard extra CP's to avoid confusing ourselves */

	while (control_points.size() > vp) {
		ControlPoint* cp = control_points.back();
		control_points.pop_back ();
		delete cp;
	}

	if (!terminal_points_can_slide && !control_points.empty()) {
		control_points.back()->set_can_slide(false);
	}

	if (vp) {

		/* reset the line coordinates given to the CanvasLine */

		/* 2 extra in case we need hidden points for line start and end */

		line_points.resize (vp + 2, ArdourCanvas::Duple (0, 0));

		ArdourCanvas::Points::size_type n = 0;

		/* potentially insert front hidden (line) point to make the line draw from
		 * zero to the first actual point
		 */

		_view_index_offset = 0;

		if (control_points[0]->get_x() != 0 && preceding != e.end()) {
			double ty = model_to_view_coord_y (e.unlocked_eval (_offset));

			if (isnan_local (ty)) {
				warning << string_compose (_("Ignoring illegal points on AutomationLine \"%1\""), _name) << endmsg;


			} else {
				line_points[n].y = _height - (ty * _height);
				line_points[n].x = 0;
				_view_index_offset = 1;
				++n;
			}
		}

		for (auto const & cp : control_points) {
			line_points[n].x = cp->get_x();
			line_points[n].y = cp->get_y();
			++n;
		}

		/* potentially insert final hidden (line) point to make the line draw
		 * from the last point to the very end
		 */

		double px = trackview.editor().duration_to_pixels_unrounded (model_to_view_coord_x (_offset + _maximum_time));

		if (control_points[control_points.size() - 1]->get_x() != px && following != e.end()) {
			double ty = model_to_view_coord_y (e.unlocked_eval (_offset + _maximum_time));

			if (isnan_local (ty)) {
				warning << string_compose (_("Ignoring illegal points on AutomationLine \"%1\""), _name) << endmsg;


			} else {
				line_points[n].y = _height - (ty * _height);
				line_points[n].x = px;
				++n;
			}
		}

		line_points.resize (n);
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
	have_reset_timeout = false;

	if (no_draw) {
		return;
	}

	/* TODO: abort any drags in progress, e.g. dragging points while writing automation
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
		if (!have_reset_timeout) {
			DEBUG_TRACE (DEBUG::Automation, "\tqueue timeout\n");
			Glib::signal_timeout().connect (sigc::bind_return (sigc::mem_fun (*this, &AutomationLine::reset), false), 250);
			have_reset_timeout = true;
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
AutomationLine::set_list (std::shared_ptr<ARDOUR::AutomationList> list)
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
AutomationLine::get_state () const
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

double
AutomationLine::model_to_view_coord_y (double y) const
{
	if (alist->default_interpolation () != alist->interpolation()) {
		switch (alist->interpolation()) {
			case AutomationList::Discrete:
				/* toggles and MIDI only -- see is_stepped */
				assert (alist->default_interpolation () == AutomationList::Linear);
				break;
			case AutomationList::Linear:
				return (y - _desc.lower) / (_desc.upper - _desc.lower);
			default:
				/* types that default to linear, can't be use
				 * Logarithmic or Exponential interpolation.
				 * "Curved" is invalid for automation (only x-fads)
				 */
				assert (0);
				break;
		}
	}
	return _desc.to_interface (y);
}

timecnt_t
AutomationLine::model_to_view_coord_x (timepos_t const & when) const
{
	/* @param when is a distance (with implicit origin) from the start of the
	 * source. So we subtract the offset (from the region if this is
	 * related to a region; zero otherwise) to get the distance (again,
	 * implicit origin) from the start of the line.
	 *
	 * Then we construct a timecnt_t from this duration, and the origin of
	 * the line on the timeline.
	 */

	return timecnt_t (when.earlier (_offset), get_origin());
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
AutomationLine::dump (std::ostream& ostr) const
{
	for (auto const & cp : control_points) {
		if (cp->model() != alist->end()) {
			ostr << '#' << cp->view_index() << " @ " << cp->get_x() << ", " << cp->get_y() << " for " << (*cp->model())->value << " @ " << (*(cp->model()))->when << std::endl;
		} else {
			ostr << "dead point\n";
		}
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

	for (auto const & cp : *the_list()) {
		const timepos_t w (session_position (cp->when));
		r.first = min (r.first, w);
		r.second = max (r.second, w);
	}

	return r;
}

timepos_t
AutomationLine::session_position (timepos_t const & when) const
{
	return when + get_origin();
}

void
AutomationLine::set_offset (timepos_t const & off)
{
	_offset = off;
	reset ();
}
