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
		snprintf (buf, sizeof (buf), "--");
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
		v = gain_to_slider_position_with_max (control_value, ARDOUR::Config->get_max_gain());
	}

	return v;
}

double
VolumeController::adjust (double control_delta)
{
	double v = _controllable->get_value ();
	double abs_delta = fabs (control_delta);

	/* convert to linear/fractional slider position domain */
	v = gain_to_slider_position_with_max (v, ARDOUR::Config->get_max_gain());
	/* adjust in this domain */
	v += control_delta;
	/* clamp in this domain */
	v = std::max (0.0, std::min (1.0, v));
	/* convert back to gain coefficient domain */
	v = slider_position_to_gain_with_max (v, ARDOUR::Config->get_max_gain());
	/* clamp in this domain */
	v = std::max (_controllable->lower(), std::min (_controllable->upper(), v));

	/* now round to some precision in the dB domain */
	v = accurate_coefficient_to_dB (v);

	if (abs_delta <= 0.01) {
		v -= fmod (v, 0.05);
	} else {
		v -= fmod (v, 0.1);
	} 

	/* and return it */
	return dB_to_coefficient (v);
}
