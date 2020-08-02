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

#include "ardour/meter.h"
#include "ardour/plugin_insert.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "feedback.h"
#include "transport.h"
#include "server.h"
#include "state.h"
#include "mixer.h"

// TO DO: make this configurable
#define POLL_INTERVAL_MS 100

using namespace ARDOUR;

struct TransportObserver {
	void operator() (ArdourFeedback* p)
	{
		p->update_all (Node::transport_roll, p->transport ().roll ());
	}
};

struct RecordStateObserver {
	void operator() (ArdourFeedback* p)
	{
		p->update_all (Node::transport_record, p->transport ().record ());
	}
};

struct TempoObserver {
	void operator() (ArdourFeedback* p)
	{
		p->update_all (Node::transport_tempo, p->transport ().tempo ());
	}
};

struct StripGainObserver {
	void operator() (ArdourFeedback* p, uint32_t strip_n)
	{
		// fires multiple times (4x as of ardour 6.0)
		p->update_all (Node::strip_gain, strip_n, p->mixer ().strip_gain (strip_n));
	}
};

struct StripPanObserver {
	void operator() (ArdourFeedback* p, uint32_t strip_n)
	{
		p->update_all (Node::strip_pan, strip_n, p->mixer ().strip_pan (strip_n));
	}
};

struct StripMuteObserver {
	void operator() (ArdourFeedback* p, uint32_t strip_n)
	{
		p->update_all (Node::strip_mute, strip_n, p->mixer ().strip_mute (strip_n));
	}
};

struct PluginBypassObserver {
	void operator() (ArdourFeedback* p, uint32_t strip_n, uint32_t plugin_n)
	{
		p->update_all (Node::strip_plugin_enable, strip_n, plugin_n,
		               p->mixer ().strip_plugin_enabled (strip_n, plugin_n));
	}
};

struct PluginParamValueObserver {
	void operator() (ArdourFeedback* p, uint32_t strip_n, uint32_t plugin_n,
	                 uint32_t param_n, boost::weak_ptr<AutomationControl> ctrl)
	{
		boost::shared_ptr<AutomationControl> control = ctrl.lock ();
		if (!control) {
			return;
		}
		p->update_all (Node::strip_plugin_param_value, strip_n, plugin_n, param_n,
		               ArdourMixer::plugin_param_value (control));
	}
};

int
ArdourFeedback::start ()
{
	observe_transport ();
	observe_mixer ();

	// some things need polling like the strip meters
	Glib::RefPtr<Glib::TimeoutSource> periodic_timeout = Glib::TimeoutSource::create (POLL_INTERVAL_MS);
	_periodic_connection                               = periodic_timeout->connect (sigc::mem_fun (*this,
                                                                         &ArdourFeedback::poll));
	periodic_timeout->attach (main_loop ()->get_context ());

	return 0;
}

int
ArdourFeedback::stop ()
{
	_periodic_connection.disconnect ();
	_transport_connections.drop_connections ();

	for (StripConnectionMap::iterator it = _strip_connections.begin (); it != _strip_connections.end(); ++it) {
		it->second->drop_connections ();
	}
	
	_strip_connections.clear();

	for (PluginConnectionMap::iterator it = _plugin_connections.begin (); it != _plugin_connections.end(); ++it) {
		it->second->drop_connections ();
	}

	_plugin_connections.clear();

	return 0;
}

void
ArdourFeedback::update_all (std::string node, TypedValue value) const
{
	update_all (node, ADDR_NONE, ADDR_NONE, ADDR_NONE, value);
}

void
ArdourFeedback::update_all (std::string node, uint32_t strip_n, TypedValue value) const
{
	update_all (node, strip_n, ADDR_NONE, ADDR_NONE, value);
}

void
ArdourFeedback::update_all (std::string node, uint32_t strip_n, uint32_t plugin_n,
                            TypedValue value) const
{
	update_all (node, strip_n, plugin_n, ADDR_NONE, value);
}

void
ArdourFeedback::update_all (std::string node, uint32_t strip_n, uint32_t plugin_n, uint32_t param_n,
                            TypedValue value) const
{
	AddressVector addr = AddressVector ();

	if (strip_n != ADDR_NONE) {
		addr.push_back (strip_n);
	}

	if (plugin_n != ADDR_NONE) {
		addr.push_back (plugin_n);
	}

	if (param_n != ADDR_NONE) {
		addr.push_back (param_n);
	}

	ValueVector val = ValueVector ();
	val.push_back (value);

	server ().update_all_clients (NodeState (node, addr, val), false);
}

bool
ArdourFeedback::poll () const
{
	update_all (Node::transport_time, transport ().time ());

	for (uint32_t strip_n = 0; strip_n < mixer ().strip_count (); ++strip_n) {
		// meters
		boost::shared_ptr<Stripable> strip = mixer ().nth_strip (strip_n);
		boost::shared_ptr<PeakMeter> meter = strip->peak_meter ();
		float                        db    = meter ? meter->meter_level (0, MeterMCP) : -193;
		update_all (Node::strip_meter, strip_n, static_cast<double> (db));
	}

	return true;
}

void
ArdourFeedback::observe_transport ()
{
	ARDOUR::Session& sess = session ();
	sess.TransportStateChange.connect (_transport_connections, MISSING_INVALIDATOR,
	                                   boost::bind<void> (TransportObserver (), this), event_loop ());
	sess.RecordStateChanged.connect (_transport_connections, MISSING_INVALIDATOR,
	                                 boost::bind<void> (RecordStateObserver (), this), event_loop ());
	sess.tempo_map ().PropertyChanged.connect (_transport_connections, MISSING_INVALIDATOR,
	                                 boost::bind<void> (TempoObserver (), this), event_loop ());
}

void
ArdourFeedback::observe_mixer ()
{
	for (uint32_t strip_n = 0; strip_n < mixer ().strip_count (); ++strip_n) {
		boost::shared_ptr<Stripable> strip = mixer ().nth_strip (strip_n);

		std::unique_ptr<PBD::ScopedConnectionList> connections (new PBD::ScopedConnectionList());

		strip->gain_control ()->Changed.connect (*connections, MISSING_INVALIDATOR,
		                                         boost::bind<void> (StripGainObserver (), this, strip_n), event_loop ());

		if (strip->pan_azimuth_control ()) {
			strip->pan_azimuth_control ()->Changed.connect (*connections, MISSING_INVALIDATOR,
			                                                boost::bind<void> (StripPanObserver (), this, strip_n), event_loop ());
		}

		strip->mute_control ()->Changed.connect (*connections, MISSING_INVALIDATOR,
		                                         boost::bind<void> (StripMuteObserver (), this, strip_n), event_loop ());
	
		strip->DropReferences.connect (*connections, MISSING_INVALIDATOR,
									   boost::bind (&ArdourFeedback::on_drop_strip, this, strip_n), event_loop ());

		_strip_connections[strip_n] = std::move (connections);

		observe_strip_plugins (strip_n, strip);
	}
}

void
ArdourFeedback::observe_strip_plugins (uint32_t strip_n, boost::shared_ptr<ARDOUR::Stripable> strip)
{
	for (uint32_t plugin_n = 0;; ++plugin_n) {
		boost::shared_ptr<PluginInsert> insert = mixer ().strip_plugin_insert (strip_n, plugin_n);
		if (!insert) {
			break;
		}

		uint32_t                             	   bypass  = insert->plugin ()->designated_bypass_port ();
		Evoral::Parameter                   	   param   = Evoral::Parameter (PluginAutomation, 0, bypass);
		boost::shared_ptr<AutomationControl>	   control = insert->automation_control (param);
		std::unique_ptr<PBD::ScopedConnectionList> connections (new PBD::ScopedConnectionList());

		if (control) {
			control->Changed.connect (*connections, MISSING_INVALIDATOR,
			                          boost::bind<void> (PluginBypassObserver (), this, strip_n, plugin_n), event_loop ());
		}

		insert->DropReferences.connect (*connections, MISSING_INVALIDATOR,
										boost::bind (&ArdourFeedback::on_drop_plugin, this, strip_n, plugin_n), event_loop ());

		// assume each strip can hold up to 65535 plugins
		_plugin_connections[(strip_n << 16) | plugin_n] = std::move (connections);

		observe_strip_plugin_param_values (strip_n, plugin_n, insert);
	}
}

void
ArdourFeedback::observe_strip_plugin_param_values (uint32_t strip_n,
                                                   uint32_t plugin_n, boost::shared_ptr<ARDOUR::PluginInsert> insert)
{
	boost::shared_ptr<Plugin> plugin = insert->plugin ();

	for (uint32_t param_n = 0; param_n < plugin->parameter_count (); ++param_n) {
		boost::shared_ptr<AutomationControl> control = mixer ().strip_plugin_param_control (
		    strip_n, plugin_n, param_n);

		if (!control) {
			continue;
		}

		PBD::ScopedConnectionList *connections = _plugin_connections[(strip_n << 16) | plugin_n].get();

		control->Changed.connect (*connections, MISSING_INVALIDATOR,
		                          boost::bind<void> (PluginParamValueObserver (), this, strip_n, plugin_n, param_n,
		                                             boost::weak_ptr<AutomationControl>(control)),
		                          event_loop ());
	}
}

void
ArdourFeedback::on_drop_strip (uint32_t strip_n)
{
	for (uint32_t plugin_n = 0;; ++plugin_n) {
		boost::shared_ptr<PluginInsert> insert = mixer ().strip_plugin_insert (strip_n, plugin_n);
		if (!insert) {
			break;
		}

		on_drop_plugin (strip_n, plugin_n);
	}

	_strip_connections[strip_n]->drop_connections ();
	_strip_connections.erase (strip_n);
}

void
ArdourFeedback::on_drop_plugin (uint32_t strip_n, uint32_t plugin_n)
{
	uint32_t key = (strip_n << 16) | plugin_n;	
	_plugin_connections[key]->drop_connections ();
	_plugin_connections.erase (key);
}
