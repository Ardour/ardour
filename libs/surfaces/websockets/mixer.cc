/*
 * Copyright (C) 2020 Luciano Iam <oss@lucianoiam.com>
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

#include <boost/lexical_cast.hpp>

#include "ardour/dB.h"
#include "ardour/meter.h"
#include "ardour/plugin_insert.h"
#include "ardour/session.h"
#include "pbd/controllable.h"

#include "mixer.h"

using namespace ARDOUR;
using namespace ArdourSurface;

ArdourMixerPlugin::ArdourMixerPlugin (boost::shared_ptr<ARDOUR::PluginInsert> insert)
	: _insert (insert)
{}

ArdourMixerPlugin::~ArdourMixerPlugin ()
{
	drop_connections ();
}

boost::shared_ptr<ARDOUR::PluginInsert>
ArdourMixerPlugin::insert () const
{
	return _insert;
}

bool
ArdourMixerPlugin::enabled () const
{
	return insert ()->enabled ();
}

void
ArdourMixerPlugin::set_enabled (bool enabled)
{
	insert ()->enable (enabled);
}

uint32_t
ArdourMixerPlugin::param_count () const
{
	return _insert->plugin ()->parameter_count ();
}

TypedValue
ArdourMixerPlugin::param_value (uint32_t param_id)
{
	return param_value (param_control (param_id));
}

void
ArdourMixerPlugin::set_param_value (uint32_t param_id, TypedValue value)
{
	boost::shared_ptr<AutomationControl> control = param_control (param_id);
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

boost::shared_ptr<ARDOUR::AutomationControl>
ArdourMixerPlugin::param_control (uint32_t param_id) const
{
	bool                      ok         = false;
	boost::shared_ptr<Plugin> plugin     = _insert->plugin ();
	uint32_t                  control_id = plugin->nth_parameter (param_id, ok);

	if (!ok || !plugin->parameter_is_input (control_id)) {
		throw ArdourMixerNotFoundException ("invalid automation control for param id = "
			+ boost::lexical_cast<std::string>(param_id));
	}

	return _insert->automation_control (Evoral::Parameter (PluginAutomation, 0, control_id));
}

TypedValue
ArdourMixerPlugin::param_value (boost::shared_ptr<ARDOUR::AutomationControl> control)
{
	ParameterDescriptor pd    = control->desc ();
	TypedValue          value = TypedValue ();

	if (pd.toggled) {
		value = TypedValue (static_cast<bool> (control->get_value ()));
	} else if (pd.enumeration || pd.integer_step) {
		value = TypedValue (static_cast<int> (control->get_value ()));
	} else {
		value = TypedValue (control->get_value ());
	}

	return value;
}

ArdourMixerStrip::ArdourMixerStrip (boost::shared_ptr<ARDOUR::Stripable> stripable, PBD::EventLoop* event_loop)
	: _stripable (stripable)
{
	boost::shared_ptr<Route> route = boost::dynamic_pointer_cast<Route> (_stripable);

	if (!route) {
		return;
	}

	for (uint32_t plugin_id = 0;; ++plugin_id) {
		boost::shared_ptr<Processor> processor = route->nth_plugin (plugin_id);

		if (!processor) {
			break;
		}

		boost::shared_ptr<PluginInsert> insert = boost::static_pointer_cast<PluginInsert> (processor);

		if (insert) {
			_plugins[plugin_id] = boost::shared_ptr<ArdourMixerPlugin> (new ArdourMixerPlugin (insert));
			insert->DropReferences.connect (*_plugins[plugin_id], MISSING_INVALIDATOR,
			                                boost::bind (&ArdourMixerStrip::on_drop_plugin, this, plugin_id), event_loop);
		}
	}
}

ArdourMixerStrip::~ArdourMixerStrip ()
{
	drop_connections ();
}

boost::shared_ptr<ARDOUR::Stripable>
ArdourMixerStrip::stripable () const
{
	return _stripable;
}

ArdourMixerPlugin&
ArdourMixerStrip::plugin (uint32_t plugin_id)
{
	if (_plugins.find (plugin_id) == _plugins.end ()) {
		throw ArdourMixerNotFoundException ("plugin id = " + boost::lexical_cast<std::string>(plugin_id) + " not found");
	}

	return *_plugins[plugin_id];
}

ArdourMixerStrip::PluginMap&
ArdourMixerStrip::plugins ()
{
	return _plugins;
}

double
ArdourMixerStrip::gain () const
{
	double val = _stripable->gain_control ()->get_value ();
	return is_midi () ? static_cast<double> (to_velocity (val)) : to_db (val);
}

void
ArdourMixerStrip::set_gain (double gain)
{
	double val = is_midi () ? from_velocity (static_cast<int> (gain)) : from_db (gain);
	_stripable->gain_control ()->set_value (val, PBD::Controllable::NoGroup);
}

bool
ArdourMixerStrip::has_pan () const
{
	return _stripable->pan_azimuth_control () != 0;
}

double
ArdourMixerStrip::pan () const
{
	boost::shared_ptr<AutomationControl> ac = _stripable->pan_azimuth_control ();
	
	if (!ac) {
		throw ArdourMixerNotFoundException ("strip has no panner");
	}

	return ac->internal_to_interface (ac->get_value ());
}

void
ArdourMixerStrip::set_pan (double value)
{
	boost::shared_ptr<AutomationControl> ac = _stripable->pan_azimuth_control ();

	if (!ac) {
		return;
	}

	ac->set_value (ac->interface_to_internal (value), PBD::Controllable::NoGroup);
}

bool
ArdourMixerStrip::mute () const
{
	return _stripable->mute_control ()->muted ();
}

void
ArdourMixerStrip::set_mute (bool mute)
{
	_stripable->mute_control ()->set_value (mute ? 1.0 : 0.0, PBD::Controllable::NoGroup);
}

float
ArdourMixerStrip::meter_level_db () const
{
	boost::shared_ptr<PeakMeter> meter = _stripable->peak_meter ();
	return meter ? meter->meter_level (0, MeterMCP) : -193;
}

std::string
ArdourMixerStrip::name () const
{
	return _stripable->name ();
}

bool
ArdourMixerStrip::is_midi () const
{
	return _stripable->presentation_info ().flags () & PresentationInfo::MidiTrack;
}

void
ArdourMixerStrip::on_drop_plugin (uint32_t plugin_id)
{
	_plugins.erase (plugin_id);
}

double
ArdourMixerStrip::to_db (double k)
{
	if (k == 0) {
		return -std::numeric_limits<double>::infinity ();
	}

	float db = accurate_coefficient_to_dB (static_cast<float> (k));

	return static_cast<double> (db);
}

double
ArdourMixerStrip::from_db (double db)
{
	if (db < -192) {
		return 0;
	}

	float k = dB_to_coefficient (static_cast<float> (db));

	return static_cast<double> (k);
}
	
int
ArdourMixerStrip::to_velocity (double k)
{
	return static_cast<int> (127.0 * k / 2.0);
}

double
ArdourMixerStrip::from_velocity (int k)
{
	return 2.0 * static_cast<double> (k) / 127.0;
}

int
ArdourMixer::start ()
{
	/* take a snapshot of current strips */
	StripableList strips;
	session ().get_stripables (strips, PresentationInfo::AllStripables);
	uint32_t strip_id = 0;

	for (StripableList::iterator it = strips.begin (); it != strips.end (); ++it) {
		_strips[strip_id] = boost::shared_ptr<ArdourMixerStrip> (new ArdourMixerStrip (*it, event_loop ()));
		(*it)->DropReferences.connect (*_strips[strip_id], MISSING_INVALIDATOR,
		                               boost::bind (&ArdourMixer::on_drop_strip, this, strip_id), event_loop ());
		strip_id++;
	}

	return 0;
}

int
ArdourMixer::stop ()
{
	Glib::Threads::Mutex::Lock lock (mixer ().mutex ());
	_strips.clear ();
	return 0;
}

ArdourMixer::StripMap&
ArdourMixer::strips ()
{
	return _strips;
}


ArdourMixerStrip&
ArdourMixer::strip (uint32_t strip_id)
{
	if (_strips.find (strip_id) == _strips.end ()) {
		throw ArdourMixerNotFoundException ("strip id = " + boost::lexical_cast<std::string>(strip_id) + " not found");
	}

	return *_strips[strip_id];
}

void
ArdourMixer::on_drop_strip (uint32_t strip_id)
{
	Glib::Threads::Mutex::Lock lock (_mutex);
	_strips.erase (strip_id);
}

Glib::Threads::Mutex&
ArdourMixer::mutex ()
{
	return _mutex;
}
