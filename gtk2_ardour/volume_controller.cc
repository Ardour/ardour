/*
    Copyright (C) 1998-2007 Paul Davis
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

    $Id: volume_controller.cc,v 1.4 2000/05/03 15:54:21 pbd Exp $
*/

#include <algorithm>

#include <string.h>
#include <limits.h>

#include "pbd/controllable.h"
#include "pbd/stacktrace.h"

#include "gtkmm2ext/gui_thread.h"

#include "ardour/dB.h"
#include "ardour/rc_configuration.h"
#include "ardour/utils.h"

#include "volume_controller.h"

using namespace Gtk;

VolumeController::VolumeController (Glib::RefPtr<Gdk::Pixbuf> p,
				    boost::shared_ptr<PBD::Controllable> c,
				    double def,
				    double step,
				    double page,
				    bool with_numeric,
                                    int subw, 
				    int subh,
				    bool linear)

	: MotionFeedback (p, MotionFeedback::Rotary, c, def, step, page, "", with_numeric, subw, subh)
	, _linear (linear)
{
	set_print_func (VolumeController::_dB_printer, this);
	value->set_width_chars (8);
}

void
VolumeController::_dB_printer (char buf[32], const boost::shared_ptr<PBD::Controllable>& c, void* arg)
{
	VolumeController* vc = reinterpret_cast<VolumeController*>(arg);
	vc->dB_printer (buf, c);
}

void
VolumeController::dB_printer (char buf[32], const boost::shared_ptr<PBD::Controllable>& c) 
{
	if (c) {
		
		if (_linear) {

			double val = accurate_coefficient_to_dB (c->get_value());

			if (step_inc < 1.0) {
				if (val >= 0.0) {
					snprintf (buf, 32, "+%5.2f dB", val);
				} else {
					snprintf (buf, 32, "%5.2f dB", val);
				}
			} else {
				if (val >= 0.0) {
					snprintf (buf, 32, "+%2ld dB", lrint (val));
				} else {
					snprintf (buf, 32, "%2ld dB", lrint (val));
				}
			}

		} else {
			
			double dB = accurate_coefficient_to_dB (c->get_value());

			if (step_inc < 1.0) {
				if (dB >= 0.0) {
					snprintf (buf, 32, "+%5.2f dB", dB);
				} else {
					snprintf (buf, 32, "%5.2f dB", dB);
				}
			} else {
				if (dB >= 0.0) {
					snprintf (buf, 32, "+%2ld dB", lrint (dB));
				} else {
					snprintf (buf, 32, "%2ld dB", lrint (dB));
				}
			}
		}
	} else {
		snprintf (buf, 32, "--");
	}
}

double
VolumeController::to_control_value (double display_value)
{
	double v;

	/* display value is always clamped to 0.0 .. 1.0 */
	display_value = std::max (0.0, std::min (1.0, display_value));

	if (_linear) {
		v = _controllable->lower() + ((_controllable->upper() - _controllable->lower()) * display_value);
	} else {
		v = slider_position_to_gain_with_max (display_value, ARDOUR::Config->get_max_gain());
	}

	return v;
}

double
VolumeController::to_display_value (double control_value)
{
	double v;

	if (_linear) {
		v = (control_value - _controllable->lower ()) / (_controllable->upper() - _controllable->lower());
	} else {
		v = gain_to_slider_position_with_max (control_value, _controllable->upper());
	}

	return v;
}

double
VolumeController::adjust (double control_delta)
{
	double v;

	if (!_linear) {
		/* we map back into the linear/fractional slider position,
		 * because this kind of control goes all the way down
		 * to -inf dB, and we want this occur in a reasonable way in
		 * terms of user interaction. if we leave the adjustment in the
		 * gain coefficient domain (or dB domain), the lower end of the
		 * control range (getting close to -inf dB) takes forever.
		 */
#if 0
		/* convert to linear/fractional slider position domain */
		v = gain_to_slider_position_with_max (_controllable->get_value (), _controllable->upper());
		/* increment in this domain */
		v += control_delta;
		/* clamp to appropriate range for linear/fractional slider domain */
		v = std::max (0.0, std::min (1.0, v));
		/* convert back to gain coefficient domain */
		v = slider_position_to_gain_with_max (v, _controllable->upper());
		/* clamp in controller domain */
		v = std::max (_controllable->lower(), std::min (_controllable->upper(), v));
		/* convert to dB domain */
		v = accurate_coefficient_to_dB (v);
		/* round up/down to nearest 0.1dB */
		if (control_delta > 0.0) {
			v = ceil (v * 10.0) / 10.0;
		} else {
			v = floor (v * 10.0) / 10.0;
		}
		/* and return it */
		return dB_to_coefficient (v);
#else
		/* ^^ Above algorithm is not symmetric. Scroll up to steps, scoll down two steps, -> different gain.
		 *
		 * see ./libs/gtkmm2ext/gtkmm2ext/motionfeedback.h and gtk2_ardour/monitor_section.cc:
		 * min-delta (corr) = MIN(0.01 * page inc, 1 * size_inc) // (gain_control uses size_inc=0.01, page_inc=0.1)
		 * range corr: 0..2   -> -inf..+6dB
		 * step sizes  [0.01, 0.10, 0.20] * page_inc,   [1,2,10,100] * step_inc. [1,2,10,100] * page_inc
		 *
		 * 0.001, 0.01, 0.02, 0.1, .2,  1, 10
		 * -> 1k steps between -inf..0dB
		 * -> 1k steps between 0..+dB
		 *
		 *  IOW:
		 *  the range is from *0  (-inf dB)  to  *2.0  ( +6dB)
		 *  the knob is configured to to go in steps of 0.001  - that's 2000 steps between 0 and 2.
		 *  or 1000 steps between 0 and 1.
		 *
		 *  we cannot round to .01dB steps because
		 *  There are only 600 possible values between  +0db and +6dB when going in steps of .01dB
		 *  1000/600 = 1.66666...
		 *
		 ******
		 * idea: make the 'controllable use a fixed range of dB.
		 * do a 1:1 mapping between values.  :et's stick with the range of 0..2 in 0.001 steps
		 *
		 * "-80" becomes 0 and "+6" becomes 2000. (NB +6dB is actually 1995, but we clamp that to the top)
		 *
		 * This approach is better (more consistet) but not good. At least the dial does not annoy me as much
		 * anymore as it did before.
		 *
		 * const double stretchfactor = rint((_controllable->upper() - _controllable->lower()) / 0.001); // 2000;
		 * const double logfactor =  stretchfactor / ((20.0 * log10( _controllable->upper())) + 80.0); // = 23.250244732
		 */
		v = _controllable->get_value ();
		/* assume everything below -60dB is silent (.001 ^= -60dB)
		 * but map range -80db..+6dB to a scale of 0..2000
		 * 80db was motivated because 2000/((20.0 * log(1)) + 80.0) is an integer value. "0dB" is included on the scale.
		 * but this leaves a dead area at the bottom of the meter..
		 */
		double arange = (v >= 0.001) ? ( ((20.0 * log10(v)) + 80.0) * 23.250244732 ) : ( 0 );
		/* add the delta */
		v = rint(arange) + rint(control_delta * 1000.0); // (min steps is 1.0/0.001 == 1000.0)
		/* catch bottom -80..-60 db in one step */
		if (v < 466) v = (control_delta > 0) ? 0.001 : 0;
		/* reverse operation  (pow(10, .05 * ((v / 23.250244732) - 80.0)))
		 * can be simplified to :*/
		else v = pow(10, (v * 0.00215051499) - 4.0);
		/* clamp value in coefficient domain */
		v = std::max (_controllable->lower(), std::min (_controllable->upper(), v));
		return v;
#endif
	} else {
		double mult;

		if (control_delta < 0.0) {
			mult = -1.0;
		} else {
			mult = 1.0;
		}

		if (fabs (control_delta) < 0.05) {
			control_delta = mult * 0.05;
		} else  {
			control_delta = mult * 0.1;
		}

		v = _controllable->get_value();

		if (v == 0.0) {
			/* if we don't special case this, we can't escape from
			   the -infinity dB black hole.
			*/
			if (control_delta > 0.0) {
				v = dB_to_coefficient (-100 + control_delta);
			}
		} else {
			static const double dB_minus_200 = dB_to_coefficient (-200.0);
			static const double dB_minus_100 = dB_to_coefficient (-100.0);
			static const double dB_minus_50 = dB_to_coefficient (-50.0);
			static const double dB_minus_20 = dB_to_coefficient (-20.0);

			if (control_delta < 0 && v < dB_minus_200) {
				v = 0.0;
			} else {

				/* non-linear scaling as the dB level gets low 
				   so that we can hit -inf and get back out of
				   it appropriately.
				*/

				if (v < dB_minus_100) {
					control_delta *= 1000.0;
				} else if (v < dB_minus_50) {
					control_delta *= 100.0;
				} else if (v < dB_minus_20) {
					control_delta *= 10.0;
				}

				v = accurate_coefficient_to_dB (v);
				v += control_delta;
				v = dB_to_coefficient (v);
			}
		}

		return std::max (_controllable->lower(), std::min (_controllable->upper(), v));
	}

}
