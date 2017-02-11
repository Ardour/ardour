/*
    Copyright (C) 2006-2016 Paul Davis

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <cmath>

#include "pbd/convert.h"
#include "pbd/strsplit.h"

#include "ardour/dB.h"
#include "ardour/gain_control.h"
#include "ardour/session.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace std;

GainControl::GainControl (Session& session, const Evoral::Parameter &param, boost::shared_ptr<AutomationList> al)
	: SlavableAutomationControl (session, param, ParameterDescriptor(param),
	                             al ? al : boost::shared_ptr<AutomationList> (new AutomationList (param)),
	                             param.type() == GainAutomation ? X_("gaincontrol") : X_("trimcontrol"),
	                             Controllable::GainLike)
{
	alist()->reset_default (1.0);

	lower_db = accurate_coefficient_to_dB (_desc.lower);
	range_db = accurate_coefficient_to_dB (_desc.upper) - lower_db;
}

double
GainControl::internal_to_interface (double v) const
{
	if (_desc.type == GainAutomation) {
		return gain_to_slider_position (v);
	} else {
		return (accurate_coefficient_to_dB (v) - lower_db) / range_db;
	}
}

double
GainControl::interface_to_internal (double v) const
{
	if (_desc.type == GainAutomation) {
		return slider_position_to_gain (v);
	} else {
		return dB_to_coefficient (lower_db + v * range_db);
	}
}

double
GainControl::internal_to_user (double v) const
{
	return accurate_coefficient_to_dB (v);
}

double
GainControl::user_to_internal (double u) const
{
	return dB_to_coefficient (u);
}

std::string
GainControl::get_user_string () const
{
	char theBuf[32]; sprintf( theBuf, _("%3.1f dB"), accurate_coefficient_to_dB (get_value()));
	return std::string(theBuf);
}

void
GainControl::inc_gain (gain_t factor)
{
	/* To be used ONLY when doing group-relative gain adjustment, from
	 * ControlGroup::set_group_values().
	 */

	const float desired_gain = user_double();

	if (fabsf (desired_gain) < GAIN_COEFF_SMALL) {
		// really?! what's the idea here?
		actually_set_value (0.000001f + (0.000001f * factor), Controllable::ForGroup);
	} else {
		actually_set_value (desired_gain + (desired_gain * factor), Controllable::ForGroup);
	}
}

void
GainControl::recompute_masters_ratios (double val)
{
	/* Master WRITE lock must be held */

	/* V' is the new gain value for this

	   Mv(n) is the return value of ::get_value() for the n-th master
	   Mr(n) is the return value of ::ratio() for the n-th master record

	   the slave should return V' on the next call to ::get_value().

	   but the value is determined by the masters, so we know:

	   V' = (Mv(1) * Mr(1)) * (Mv(2) * Mr(2)) * ... * (Mv(n) * Mr(n))

	   hence:

	   Mr(1) * Mr(2) * ... * (Mr(n) = V' / (Mv(1) * Mv(2) * ... * Mv(n))

	   if we make all ratios equal (i.e. each master contributes the same
	   fraction of its own gain level to make the final slave gain), then we
	   have:

	   pow (Mr(n), n) = V' / (Mv(1) * Mv(2) * ... * Mv(n))

	   which gives

	   Mr(n) = pow ((V' / (Mv(1) * Mv(2) * ... * Mv(n))), 1/n)

	   Mr(n) is the new ratio number for the slaves
	*/

	const double nmasters = _masters.size();
	double masters_total_gain_coefficient = 1.0;

	for (Masters::iterator mr = _masters.begin(); mr != _masters.end(); ++mr) {
		masters_total_gain_coefficient *= mr->second.master()->get_value();
	}

	const double new_universal_ratio = pow ((val / masters_total_gain_coefficient), (1.0/nmasters));

	for (Masters::iterator mr = _masters.begin(); mr != _masters.end(); ++mr) {
		mr->second.reset_ratio (new_universal_ratio);
	}
}

