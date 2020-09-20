/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#include "pbd/convert.h"
#include "pbd/strsplit.h"

#include "evoral/Curve.h"

#include "ardour/dB.h"
#include "ardour/gain_control.h"
#include "ardour/session.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace std;

static std::string gain_control_name (Evoral::Parameter const& param)
{
	switch (param.type()) {
		case GainAutomation:
			/* fallthrough */
		case BusSendLevel:
			return X_("gaincontrol");
		case TrimAutomation:
			return X_("trimcontrol");
		case MainOutVolume:
			return X_("mastervolume");
		default:
			break;
	}
	/* default in AutomationControl c'tor uses
	 * EventTypeMap::instance().to_symbol(parameter)
	 */
	return "";
}

static boost::shared_ptr<AutomationList> automation_list_new (Evoral::Parameter const& param)
{
	switch (param.type()) {
		case GainAutomation:
			/* fallthrough */
		case BusSendLevel:
			/* fallthrough */
		case TrimAutomation:
			return boost::shared_ptr<AutomationList> (new AutomationList (param, Temporal::AudioTime));
		case MainOutVolume:
			/* not automatable */
			break;
		default:
			assert (0);
			break;
	}
	return boost::shared_ptr<AutomationList> ();
}

GainControl::GainControl (Session& session, const Evoral::Parameter &param, boost::shared_ptr<AutomationList> al)
	: SlavableAutomationControl (session, param, ParameterDescriptor(param),
	                             al ? al : automation_list_new (param),
	                             gain_control_name (param),
	                             Controllable::GainLike)
{
}

void
GainControl::inc_gain (gain_t factor)
{
	/* To be used ONLY when doing group-relative gain adjustment, from
	 * ControlGroup::set_group_values().
	 */

	const float desired_gain = get_value ();

	if (fabsf (desired_gain) < GAIN_COEFF_SMALL) {
		// really?! what's the idea here?
		actually_set_value (0.000001f + (0.000001f * factor), Controllable::ForGroup);
	} else {
		actually_set_value (desired_gain + (desired_gain * factor), Controllable::ForGroup);
	}
}

void
GainControl::post_add_master (boost::shared_ptr<AutomationControl> m)
{
	if (m->get_value() == 0) {
		/* master is at -inf, which forces this ctrl to -inf on assignment */
		Changed (false, Controllable::NoGroup); /* EMIT SIGNAL */
	}
}

bool
GainControl::get_masters_curve_locked (samplepos_t start, samplepos_t end, float* vec, samplecnt_t veclen) const
{
	if (_masters.empty()) {
		return list()->curve().rt_safe_get_vector (timepos_t (start), timepos_t (end), vec, veclen);
	}
	for (samplecnt_t i = 0; i < veclen; ++i) {
		vec[i] = 1.f;
	}
	return SlavableAutomationControl::masters_curve_multiply (timepos_t (start), timepos_t (end), vec, veclen);
}
