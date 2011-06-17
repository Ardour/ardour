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
				    bool linear,
				    bool dB)

	: MotionFeedback (p, MotionFeedback::Rotary, c, def, step, page, "", with_numeric, subw, subh)
	, _linear (linear)
	, _controllable_uses_dB (dB)
{
	set_print_func (VolumeController::_dB_printer, this);

	if (step < 1.0) {
		value->set_width_chars (6 + abs ((int) ceil (log10 (step))));
	} else {
		value->set_width_chars (5); // -NNdB
	}

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
			/* controllable units are in dB so just show the value */
			if (step_inc < 1.0) {
				snprintf (buf, 32, "%.2f dB", c->get_value());
			} else {
				snprintf (buf, 32, "%ld dB", lrint (c->get_value()));
			}
		} else {
			
			double gain_coefficient;

			if (!_controllable_uses_dB) {
				gain_coefficient = c->get_value();
			} else {
				double fract = (c->get_value() - c->lower()) / (c->upper() - c->lower());
				gain_coefficient = slider_position_to_gain (fract);
			}

			if (step_inc < 1.0) {
				snprintf (buf, 32, "%.2f dB", accurate_coefficient_to_dB (gain_coefficient));
			} else {
				snprintf (buf, 32, "%ld dB", lrint (accurate_coefficient_to_dB (gain_coefficient)));
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
		
		v = slider_position_to_gain (display_value);
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
		v = gain_to_slider_position (control_value);
	}

	return v;
}
