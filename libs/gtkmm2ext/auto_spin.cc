/*
    Copyright (C) 1999 Paul Barton-Davis 

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

#include <gtkmm2ext/auto_spin.h>
#include <cmath>

using namespace Gtkmm2ext;
using namespace std;

#define upper          adjustment.get_upper()
#define lower          adjustment.get_lower()
#define step_increment adjustment.get_step_increment()
#define page_increment adjustment.get_page_increment()

const unsigned int AutoSpin::initial_timer_interval = 500;   /* msecs */
const unsigned int AutoSpin::timer_interval = 20;            /* msecs */
const unsigned int AutoSpin::climb_timer_calls = 5;    /* between climbing */

AutoSpin::AutoSpin (Gtk::Adjustment &adjr, gfloat cr, bool round_to_steps_yn) 
	: adjustment (adjr),
	  climb_rate (cr)

{
	initial = adjustment.get_value();
	left_is_decrement = true;
	wrap = false;
	have_timer = false;
	need_timer = false;
	timer_calls = 0;
	round_to_steps = round_to_steps_yn;
}

void
AutoSpin::stop_timer ()
{
	if (have_timer) {
		g_source_remove (timeout_tag);
		have_timer = false;	
	}
}

gint
AutoSpin::stop_spinning (GdkEventButton *ev)
{
	need_timer = false;
	stop_timer ();
	return FALSE;
}

gint
AutoSpin::button_press (GdkEventButton *ev)
{
	bool shifted = false;
	bool control = false;
	bool with_decrement = false;

	stop_spinning (0);

	if (ev->state & GDK_SHIFT_MASK) {
		/* use page shift */

		shifted = true;
	}

	if (ev->state & GDK_CONTROL_MASK) {
		/* go to upper/lower bound on button1/button2 */

		control = true;
	}

	/* XXX should figure out which button is left/right */

	switch (ev->button) {
	case 1:
		if (control) {
			set_value (left_is_decrement ? lower : upper);
			return TRUE;
		} else {
			if (left_is_decrement) {
				with_decrement = true;
			} else {
				with_decrement = false;
			}
		}
		break;

	case 2:
		if (!control) {
			set_value (initial);
		}
		return TRUE;
		break;

	case 3:
		if (control) {
			set_value (left_is_decrement ? upper : lower);
			return TRUE;
		}
		break;

	case 4:
		if (!control) {
			adjust_value (shifted ? page_increment : step_increment);
		} else {
			set_value (upper);
		}
		return TRUE;
		break;

	case 5:
		if (!control) {
			adjust_value (shifted ? -page_increment : -step_increment);
		} else {
			set_value (lower);
		}
		return TRUE;
		break;
	} 

	start_spinning (with_decrement, shifted);
	return TRUE;
}

void
AutoSpin::start_spinning (bool decrement, bool page)
{
	timer_increment = page ? page_increment : step_increment;

	if (decrement) { 
		timer_increment = -timer_increment;
	}

	adjust_value (timer_increment);
	
	have_timer = true;
	timer_calls = 0;
	timeout_tag = g_timeout_add (initial_timer_interval,
				       AutoSpin::_timer,
				       this);
}

gint
AutoSpin::_timer (void *arg)
{
	return ((AutoSpin *) arg)->timer ();
}

void
AutoSpin::set_value (gfloat value)
{
	if (round_to_steps)
		adjustment.set_value (floor((value / step_increment) + 0.5f) * step_increment);
	else
		adjustment.set_value (value);
}

bool
AutoSpin::adjust_value (gfloat increment)
{
	gfloat val;
	bool done = false;

	val = adjustment.get_value();

	val += increment;

	if (val > upper) {
		if (wrap) {
			val = lower;
		} else {
			val = upper;
			done = true;
		}
	} else if (val < lower) {
		if (wrap) {
			val = upper;
		} else {
			val = lower;
			done = true;
		}
	}

	set_value(val);
	return done;
}

gint
AutoSpin::timer ()
{
	bool done;
	int retval = FALSE;

	done = adjust_value (timer_increment);

	if (need_timer) {

		/* we're in the initial call, which happened
		   after initial_timer_interval msecs. Now
		   request a much more frequent update.
		*/
		
		timeout_tag = g_timeout_add (timer_interval,
					       _timer,
					       this);
		have_timer = true;
		need_timer = false;

		/* cancel this initial timeout */
		
		retval = FALSE;

	} else { 
		/* this is the regular "fast" call after each
		   timer_interval msecs. 
		*/

		if (timer_calls < climb_timer_calls) {
			timer_calls++;
		} else {
			if (climb_rate > 0.0) {
				if (timer_increment > 0) {
					timer_increment += climb_rate;
				} else {
					timer_increment -= climb_rate;
				}
			}
			timer_calls = 0;
		}

		if (!done) {
			retval = TRUE;
		}
	}

	return retval;
}	

void
AutoSpin::set_bounds (gfloat init, gfloat up, gfloat down, bool with_reset)
{
	adjustment.set_upper(up);
	adjustment.set_lower(down);

	initial = init;
	
	adjustment.changed ();
	
	if (with_reset) {
		adjustment.set_value (init);
	}
} 
