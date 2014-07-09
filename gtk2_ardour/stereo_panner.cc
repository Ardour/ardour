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

#include <iostream>
#include <iomanip>
#include <cstring>
#include <cmath>

#include <gtkmm/window.h>
#include <pangomm/layout.h>

#include "pbd/controllable.h"
#include "pbd/compose.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/persistent_tooltip.h"

#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"

#include "ardour_ui.h"
#include "global_signals.h"
#include "stereo_panner.h"
#include "stereo_panner_editor.h"
#include "rgb_macros.h"
#include "utils.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

using namespace ARDOUR;

StereoPanner::StereoPanner (boost::shared_ptr<PannerShell> p)
	: PannerInterface (p->panner())
	, _panner_shell (p)
	, position_control (_panner->pannable()->pan_azimuth_control)
	, width_control (_panner->pannable()->pan_width_control)
	, dragging_position (false)
	, dragging_left (false)
	, dragging_right (false)
	, drag_start_x (0)
	, last_drag_x (0)
	, accumulated_delta (0)
	, detented (false)
	, position_binder (position_control)
	, width_binder (width_control)
	, _dragging (false)
{
	if (_knob_image[0] == 0) {
		for (size_t i=0; i < (sizeof(_knob_image)/sizeof(_knob_image[0])); i++) {
			_knob_image[i] = load_pixbuf (_knob_image_files[i]);
		}
	}

	position_control->Changed.connect (panvalue_connections, invalidator(*this), boost::bind (&StereoPanner::value_change, this), gui_context());
	width_control->Changed.connect (panvalue_connections, invalidator(*this), boost::bind (&StereoPanner::value_change, this), gui_context());

	_panner_shell->Changed.connect (panshell_connections, invalidator (*this), boost::bind (&StereoPanner::bypass_handler, this), gui_context());
	_panner_shell->PannableChanged.connect (panshell_connections, invalidator (*this), boost::bind (&StereoPanner::pannable_handler, this), gui_context());

	set_tooltip ();
}

StereoPanner::~StereoPanner ()
{

}

void
StereoPanner::set_tooltip ()
{
	if (_panner_shell->bypassed()) {
		_tooltip.set_tip (_("bypassed"));
		return;
	}
	double pos = position_control->get_value(); // 0..1

	/* We show the position of the center of the image relative to the left & right.
	   This is expressed as a pair of percentage values that ranges from (100,0)
	   (hard left) through (50,50) (hard center) to (0,100) (hard right).

	   This is pretty wierd, but its the way audio engineers expect it. Just remember that
	   the center of the USA isn't Kansas, its (50LA, 50NY) and it will all make sense.
	*/

	char buf[64];
	snprintf (buf, sizeof (buf), _("L:%3d R:%3d Width:%d%%"), (int) rint (100.0 * (1.0 - pos)),
	          (int) rint (100.0 * pos),
	          (int) floor (100.0 * width_control->get_value()));
	_tooltip.set_tip (buf);
}

bool
StereoPanner::on_expose_event (GdkEventExpose*)
{
	Cairo::RefPtr<Cairo::Context> context = get_window()->create_cairo_context();
	unsigned pos = (unsigned)(rint (100.0 * position_control->get_value ())); /* 0..100 */

	double x = (get_width() - _knob_image[pos]->get_width())/2.0;
	double y = (get_height() - _knob_image[pos]->get_height())/2.0;

	cairo_rectangle (context->cobj(), x, y, _knob_image[pos]->get_width(), _knob_image[pos]->get_height());

	gdk_cairo_set_source_pixbuf (context->cobj(), _knob_image[pos]->gobj(), x, y);
	cairo_fill (context->cobj());
	return true;
}

bool
StereoPanner::on_button_press_event (GdkEventButton* ev)
{
	if (PannerInterface::on_button_press_event (ev)) {
		return true;
	}

	if (_panner_shell->bypassed()) {
		return true;
	}
	
	drag_start_x = ev->x;
	last_drag_x = ev->x;

	dragging_position = false;
	dragging_left = false;
	dragging_right = false;
	_dragging = false;
	_tooltip.target_stop_drag ();
	accumulated_delta = 0;
	detented = false;

	/* Let the binding proxies get first crack at the press event
	 */

	if (ev->y < 20) {
		if (position_binder.button_press_handler (ev)) {
			return true;
		}
	} else {
		if (width_binder.button_press_handler (ev)) {
			return true;
		}
	}

	if (ev->button != 1) {
		return false;
	}

	if (ev->type == GDK_2BUTTON_PRESS) {
		int width = get_width();

		if (Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier)) {
			/* handled by button release */
			return true;
		}

		if (ev->y < 20) {

			/* upper section: adjusts position, constrained by width */

			const double w = fabs (width_control->get_value ());
			const double max_pos = 1.0 - (w/2.0);
			const double min_pos = w/2.0;

			if (ev->x <= width/3) {
				/* left side dbl click */
				if (Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier)) {
					/* 2ndary-double click on left, collapse to hard left */
					width_control->set_value (0);
					position_control->set_value (0);
				} else {
					position_control->set_value (min_pos);
				}
			} else if (ev->x > 2*width/3) {
				if (Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier)) {
					/* 2ndary-double click on right, collapse to hard right */
					width_control->set_value (0);
					position_control->set_value (1.0);
				} else {
					position_control->set_value (max_pos);
				}
			} else {
				position_control->set_value (0.5);
			}

		} else {

			/* lower section: adjusts width, constrained by position */

			const double p = position_control->get_value ();
			const double max_width = 2.0 * min ((1.0 - p), p);

			if (ev->x <= width/3) {
				/* left side dbl click */
				width_control->set_value (max_width); // reset width to 100%
			} else if (ev->x > 2*width/3) {
				/* right side dbl click */
				width_control->set_value (-max_width); // reset width to inverted 100%
			} else {
				/* center dbl click */
				width_control->set_value (0); // collapse width to 0%
			}
		}

		_dragging = false;
		_tooltip.target_stop_drag ();

	} else if (ev->type == GDK_BUTTON_PRESS) {

		if (Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier)) {
			/* handled by button release */
			return true;
		}

		if (ev->y < 20) {
			/* top section of widget is for position drags */
			dragging_position = true;
			StartPositionGesture ();
		} else {
			/* lower section is for dragging width */

			double pos = position_control->get_value (); /* 0..1 */
			double swidth = width_control->get_value (); /* -1..+1 */
			double fswidth = fabs (swidth);
			int usable_width = get_width();
			double center = usable_width * pos;
			int left = lrint (center - (fswidth * usable_width / 2.0)); // center of leftmost box
			int right = lrint (center +  (fswidth * usable_width / 2.0)); // center of rightmost box

			if (ev->x >= left && ev->x < left) {
				if (swidth < 0.0) {
					dragging_right = true;
				} else {
					dragging_left = true;
				}
			} else if (ev->x >= right && ev->x < right) {
				if (swidth < 0.0) {
					dragging_left = true;
				} else {
					dragging_right = true;
				}
			}
			StartWidthGesture ();
		}

		_dragging = true;
		_tooltip.target_start_drag ();
	}

	return true;
}

bool
StereoPanner::on_button_release_event (GdkEventButton* ev)
{
	if (PannerInterface::on_button_release_event (ev)) {
		return true;
	}
	
	if (ev->button != 1) {
		return false;
	}

	if (_panner_shell->bypassed()) {
		return false;
	}

	bool const dp = dragging_position;

	_dragging = false;
	_tooltip.target_stop_drag ();
	dragging_position = false;
	dragging_left = false;
	dragging_right = false;
	accumulated_delta = 0;
	detented = false;

	if (Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier)) {
		_panner->reset ();
	} else {
		if (dp) {
			StopPositionGesture ();
		} else {
			StopWidthGesture ();
		}
	}

	return true;
}

bool
StereoPanner::on_scroll_event (GdkEventScroll* ev)
{
	double one_degree = 1.0/180.0; // one degree as a number from 0..1, since 180 degrees is the full L/R axis
	double pv = position_control->get_value(); // 0..1.0 ; 0 = left
	double wv = width_control->get_value(); // 0..1.0 ; 0 = left
	double step;

	if (_panner_shell->bypassed()) {
		return false;
	}

	if (Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier)) {
		step = one_degree;
	} else {
		step = one_degree * 5.0;
	}

	switch (ev->direction) {
	case GDK_SCROLL_LEFT:
		wv += step;
		width_control->set_value (wv);
		break;
	case GDK_SCROLL_UP:
		pv -= step;
		position_control->set_value (pv);
		break;
	case GDK_SCROLL_RIGHT:
		wv -= step;
		width_control->set_value (wv);
		break;
	case GDK_SCROLL_DOWN:
		pv += step;
		position_control->set_value (pv);
		break;
	}

	return true;
}

bool
StereoPanner::on_motion_notify_event (GdkEventMotion* ev)
{
	if (_panner_shell->bypassed()) {
		_dragging = false;
	}
	if (!_dragging) {
		return false;
	}

	int usable_width = get_width();
	double delta = (ev->x - last_drag_x) / (double) usable_width;
	double current_width = width_control->get_value ();

	if (dragging_left) {
		delta = -delta;
	}
	
	if (dragging_left || dragging_right) {

		if (Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier)) {

			/* change width and position in a way that keeps the
			 * other side in the same place
			 */

			_panner->freeze ();
			
			double pv = position_control->get_value();

			if (dragging_left) {
				position_control->set_value (pv - delta);
			} else {
				position_control->set_value (pv + delta);
			}

			if (delta > 0.0) {
				/* delta is positive, so we're about to
				   increase the width. But we need to increase it
				   by twice the required value so that the
				   other side remains in place when we set
				   the position as well.
				*/
				width_control->set_value (current_width + (delta * 2.0));
			} else {
				width_control->set_value (current_width + delta);
			}

			_panner->thaw ();

		} else {

			/* maintain position as invariant as we change the width */
			
			/* create a detent close to the center */
			
			if (!detented && fabs (current_width) < 0.02) {
				detented = true;
				/* snap to zero */
				width_control->set_value (0);
			}
			
			if (detented) {
				
				accumulated_delta += delta;
				
				/* have we pulled far enough to escape ? */
				
				if (fabs (accumulated_delta) >= 0.025) {
					width_control->set_value (current_width + accumulated_delta);
					detented = false;
					accumulated_delta = false;
				}
				
			} else {
				/* width needs to change by 2 * delta because both L & R move */
				width_control->set_value (current_width + (delta * 2.0));
			}
		}

	} else if (dragging_position) {

		double pv = position_control->get_value(); // 0..1.0 ; 0 = left
		position_control->set_value (pv + delta);
	}

	last_drag_x = ev->x;
	return true;
}

bool
StereoPanner::on_key_press_event (GdkEventKey* ev)
{
	double one_degree = 1.0/180.0;
	double pv = position_control->get_value(); // 0..1.0 ; 0 = left
	double wv = width_control->get_value(); // 0..1.0 ; 0 = left
	double step;

	if (_panner_shell->bypassed()) {
		return false;
	}

	if (Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier)) {
		step = one_degree;
	} else {
		step = one_degree * 5.0;
	}

	/* up/down control width because we consider pan position more "important"
	   (and thus having higher "sense" priority) than width.
	*/

	switch (ev->keyval) {
	case GDK_Up:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {
			width_control->set_value (1.0);
		} else {
			width_control->set_value (wv + step);
		}
		break;
	case GDK_Down:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {
			width_control->set_value (-1.0);
		} else {
			width_control->set_value (wv - step);
		}
		break;

	case GDK_Left:
		pv -= step;
		position_control->set_value (pv);
		break;
	case GDK_Right:
		pv += step;
		position_control->set_value (pv);
		break;
	case GDK_0:
	case GDK_KP_0:
		width_control->set_value (0.0);
		break;

	default:
		return false;
	}

	return true;
}

void
StereoPanner::bypass_handler ()
{
	queue_draw ();
}

void
StereoPanner::pannable_handler ()
{
	panvalue_connections.drop_connections();
	position_control = _panner->pannable()->pan_azimuth_control;
	width_control = _panner->pannable()->pan_width_control;
	position_binder.set_controllable(position_control);
	width_binder.set_controllable(width_control);

	position_control->Changed.connect (panvalue_connections, invalidator(*this), boost::bind (&StereoPanner::value_change, this), gui_context());
	width_control->Changed.connect (panvalue_connections, invalidator(*this), boost::bind (&StereoPanner::value_change, this), gui_context());
	queue_draw ();
}

PannerEditor*
StereoPanner::editor ()
{
	return new StereoPannerEditor (this);
}
