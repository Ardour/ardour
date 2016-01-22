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

#include "ardour/dB.h"
#include "ardour/gain_control.h"
#include "ardour/session.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace std;

GainControl::GainControl (Session& session, const Evoral::Parameter &param, boost::shared_ptr<AutomationList> al)
	: AutomationControl (session, param, ParameterDescriptor(param),
	                     al ? al : boost::shared_ptr<AutomationList> (new AutomationList (param)),
	                     param.type() == GainAutomation ? X_("gaincontrol") : X_("trimcontrol")) {

	alist()->reset_default (1.0);

	lower_db = accurate_coefficient_to_dB (_desc.lower);
	range_db = accurate_coefficient_to_dB (_desc.upper) - lower_db;
}

double
GainControl::get_value() const
{
	if (!_master) {
		return AutomationControl::get_value();
	}
	return AutomationControl::get_value() * _master->get_value();
}

void
GainControl::set_value (double val, PBD::Controllable::GroupControlDisposition group_override)
{
	if (writable()) {
		_set_value (val, group_override);
	}
}

void
GainControl::set_value_unchecked (double val)
{
	/* used only automation playback */
	_set_value (val, Controllable::NoGroup);
}

void
GainControl::_set_value (double val, Controllable::GroupControlDisposition group_override)
{
	AutomationControl::set_value (std::max (std::min (val, (double)_desc.upper), (double)_desc.lower), group_override);
	_session.set_dirty ();
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
GainControl::set_master (boost::shared_ptr<GainControl> m)
{
	double old_master_val;

	if (_master) {
		old_master_val = _master->get_value();
	} else {
		old_master_val = 1.0;
	}

	_master = m;

	double new_master_val;

	if (_master) {
		new_master_val = _master->get_value();
	} else {
		new_master_val = 1.0;
	}

	if (old_master_val != new_master_val) {
		Changed(); /* EMIT SIGNAL */
	}
}

