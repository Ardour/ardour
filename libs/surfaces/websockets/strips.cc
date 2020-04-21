/*
 * Copyright (C) 2020 Luciano Iam <lucianito@gmail.com>
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

#include "ardour/dB.h"
#include "ardour/plugin_insert.h"
#include "ardour/session.h"
#include "pbd/controllable.h"

#include "strips.h"

using namespace ARDOUR;

int
ArdourStrips::start ()
{
	/* take an indexed snapshot of current strips */
	StripableList strips;
	session ().get_stripables (strips, PresentationInfo::AllStripables);

	for (StripableList::iterator strip = strips.begin (); strip != strips.end (); ++strip) {
		_strips.push_back (*strip);
	}

	return 0;
}

int
ArdourStrips::stop ()
{
	_strips.clear ();
	return 0;
}

double
ArdourStrips::to_db (double k)
{
	if (k == 0) {
		return -std::numeric_limits<double>::infinity ();
	}

	float db = accurate_coefficient_to_dB (static_cast<float> (k));

	return static_cast<double> (db);
}

double
ArdourStrips::from_db (double db)
{
	if (db < -192) {
		return 0;
	}

	float k = dB_to_coefficient (static_cast<float> (db));

	return static_cast<double> (k);
}

double
ArdourStrips::strip_gain (uint32_t strip_n) const
{
	return to_db (nth_strip (strip_n)->gain_control ()->get_value ());
}

void
ArdourStrips::set_strip_gain (uint32_t strip_n, double db)
{
	nth_strip (strip_n)->gain_control ()->set_value (from_db (db), PBD::Controllable::NoGroup);
}

double
ArdourStrips::strip_pan (uint32_t strip_n) const
{
	boost::shared_ptr<AutomationControl> ac = nth_strip (strip_n)->pan_azimuth_control ();
	if (!ac) {
		/* TODO: inform GUI that strip has no panner */
		return 0;
	}
	/* scale from [0.0 ; 1.0] to [-1.0 ; 1.0] */
	return 2.0 * ac->get_value () - 1.0; //TODO: prefer ac->internal_to_interface (c->get_value ());
}

void
ArdourStrips::set_strip_pan (uint32_t strip_n, double value)
{
	boost::shared_ptr<AutomationControl> ac = nth_strip (strip_n)->pan_azimuth_control ();
	if (!ac) {
		return;
	}
	/* TODO: prefer ac->set_value (ac->interface_to_internal (value), NoGroup); */
	value = (value + 1.0) / 2.0;
	ac->set_value (value, PBD::Controllable::NoGroup);
}

bool
ArdourStrips::strip_mute (uint32_t strip_n) const
{
	return nth_strip (strip_n)->mute_control ()->muted ();
}

void
ArdourStrips::set_strip_mute (uint32_t strip_n, bool mute)
{
	nth_strip (strip_n)->mute_control ()->set_value (mute ? 1.0 : 0.0, PBD::Controllable::NoGroup);
}

bool
ArdourStrips::strip_plugin_enabled (uint32_t strip_n, uint32_t plugin_n) const
{
	return strip_plugin_insert (strip_n, plugin_n)->enabled ();
}

void
ArdourStrips::set_strip_plugin_enabled (uint32_t strip_n, uint32_t plugin_n, bool enabled)
{
	strip_plugin_insert (strip_n, plugin_n)->enable (enabled);
}

TypedValue
ArdourStrips::strip_plugin_param_value (uint32_t strip_n, uint32_t plugin_n,
                                        uint32_t param_n) const
{
	return plugin_param_value (strip_plugin_param_control (strip_n, plugin_n, param_n));
}

void
ArdourStrips::set_strip_plugin_param_value (uint32_t strip_n, uint32_t plugin_n,
                                            uint32_t param_n, TypedValue value)
{
	boost::shared_ptr<AutomationControl> control = strip_plugin_param_control (
	    strip_n, plugin_n, param_n);

	if (control) {
		ParameterDescriptor pd = control->desc ();
		double              dbl_val;

		if (pd.toggled) {
			dbl_val = static_cast<double> (static_cast<bool> (value));
		} else if (pd.enumeration || pd.integer_step) {
			dbl_val = static_cast<double> (static_cast<int> (value));
		} else {
			dbl_val = static_cast<double> (value);
		}

		control->set_value (dbl_val, PBD::Controllable::NoGroup);
	}
}

uint32_t
ArdourStrips::strip_count () const
{
	return _strips.size ();
}

boost::shared_ptr<Stripable>
ArdourStrips::nth_strip (uint32_t strip_n) const
{
	if (strip_n < _strips.size ()) {
		return _strips[strip_n];
	}

	return boost::shared_ptr<Stripable> ();
}

TypedValue
ArdourStrips::plugin_param_value (boost::shared_ptr<ARDOUR::AutomationControl> control)
{
	TypedValue value = TypedValue ();

	if (control) {
		ParameterDescriptor pd = control->desc ();

		if (pd.toggled) {
			value = TypedValue (static_cast<bool> (control->get_value ()));
		} else if (pd.enumeration || pd.integer_step) {
			value = TypedValue (static_cast<int> (control->get_value ()));
		} else {
			value = TypedValue (control->get_value ());
		}
	}

	return value;
}

boost::shared_ptr<PluginInsert>
ArdourStrips::strip_plugin_insert (uint32_t strip_n, uint32_t plugin_n) const
{
	boost::shared_ptr<Stripable> strip = nth_strip (strip_n);

	if ((strip->presentation_info ().flags () & ARDOUR::PresentationInfo::VCA) == 0) {
		boost::shared_ptr<Route> route = boost::dynamic_pointer_cast<Route> (strip);

		if (route) {
			boost::shared_ptr<Processor> processor = route->nth_plugin (plugin_n);

			if (processor) {
				boost::shared_ptr<PluginInsert> insert =
				    boost::static_pointer_cast<PluginInsert> (processor);

				if (insert) {
					return insert;
				}
			}
		}
	}

	return boost::shared_ptr<PluginInsert> ();
}

boost::shared_ptr<AutomationControl>
ArdourStrips::strip_plugin_param_control (uint32_t strip_n, uint32_t plugin_n,
                                          uint32_t param_n) const
{
	boost::shared_ptr<PluginInsert> insert = strip_plugin_insert (strip_n, plugin_n);

	if (insert) {
		bool                      ok         = false;
		boost::shared_ptr<Plugin> plugin     = insert->plugin ();
		uint32_t                  control_id = plugin->nth_parameter (param_n, ok);

		if (ok && plugin->parameter_is_input (control_id)) {
			boost::shared_ptr<AutomationControl> control =
			    insert->automation_control (Evoral::Parameter (PluginAutomation, 0, control_id));
			return control;
		}
	}

	return boost::shared_ptr<AutomationControl> ();
}
