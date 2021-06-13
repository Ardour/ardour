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

#include "ardour/plugin_insert.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "pbd/abstract_ui.cc" // instantiate template

#include "feedback.h"
#include "transport.h"
#include "server.h"
#include "state.h"

// TO DO: make this configurable
#define POLL_INTERVAL_MS 100

#define OPTIONAL_CONNECT_HELPER(s,c) if (server ().should_request_write ()) \
											s.connect (c, MISSING_INVALIDATOR, boost::bind<void> \
											(ServerWriteObserver (), &server ()), &_helper);

using namespace ARDOUR;
using namespace ArdourSurface;

struct ServerWriteObserver {
	void operator() (WebsocketsServer *server)
	{
		server->request_write ();
	}
};

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
	void operator() (ArdourFeedback* p, uint32_t strip_id)
	{
		// fires multiple times (4x as of ardour 6.0)
		p->update_all (Node::strip_gain, strip_id, p->mixer ().strip (strip_id).gain ());
	}
};

struct StripPanObserver {
	void operator() (ArdourFeedback* p, uint32_t strip_id)
	{
		p->update_all (Node::strip_pan, strip_id, p->mixer ().strip (strip_id).pan ());
	}
};

struct StripMuteObserver {
	void operator() (ArdourFeedback* p, uint32_t strip_id)
	{
		p->update_all (Node::strip_mute, strip_id, p->mixer ().strip (strip_id).mute ());
	}
};

struct PluginBypassObserver {
	void operator() (ArdourFeedback* p, uint32_t strip_id, uint32_t plugin_id)
	{
		p->update_all (Node::strip_plugin_enable, strip_id, plugin_id,
		               p->mixer ().strip (strip_id).plugin (plugin_id).enabled ());
	}
};

struct PluginParamValueObserver {
	void operator() (ArdourFeedback* p, uint32_t strip_id, uint32_t plugin_id,
	                 uint32_t param_id, boost::weak_ptr<AutomationControl> ctrl)
	{
		boost::shared_ptr<AutomationControl> control = ctrl.lock ();

		if (!control) {
			return;
		}
		
		p->update_all (Node::strip_plugin_param_value, strip_id, plugin_id, param_id,
		               ArdourMixerPlugin::param_value (control));
	}
};

FeedbackHelperUI::FeedbackHelperUI()
	: AbstractUI<BaseUI::BaseRequestObject> ("feedback_helper")
{
	pthread_set_name ("test_ui_thread"); // FIXME - needed?
	run_loop_thread = Glib::Threads::Thread::self ();
	set_event_loop_for_thread (this);
	ARDOUR::SessionEvent::create_per_thread_pool ("test", 512); // FIXME - needed?
}

void
FeedbackHelperUI::do_request (BaseUI::BaseRequestObject* req) {
	if (req->type == CallSlot) {
		call_slot (MISSING_INVALIDATOR, req->the_slot);
	} else if (req->type == Quit) {
		quit ();
	}
};

int
ArdourFeedback::start ()
{
	observe_transport ();
	observe_mixer ();

	// some values need polling like the strip meters
	Glib::RefPtr<Glib::TimeoutSource> periodic_timeout = Glib::TimeoutSource::create (POLL_INTERVAL_MS);
	_periodic_connection                               = periodic_timeout->connect (sigc::mem_fun (*this,
                                                                         &ArdourFeedback::poll));

	// server must be started before feedback otherwise
	// should_request_write() will always return false
	if (!server ().should_request_write ()) {
		periodic_timeout->attach (main_loop ()->get_context ());
	} else {
		_helper.run();
		periodic_timeout->attach (_helper.main_loop()->get_context ());
	}

	return 0;
}

int
ArdourFeedback::stop ()
{
	if (server ().should_request_write ()) {
		_helper.quit();
	}

	_periodic_connection.disconnect ();
	_transport_connections.drop_connections ();
	
	return 0;
}

void
ArdourFeedback::update_all (std::string node, TypedValue value) const
{
	update_all (node, ADDR_NONE, ADDR_NONE, ADDR_NONE, value);
}

void
ArdourFeedback::update_all (std::string node, uint32_t strip_id, TypedValue value) const
{
	update_all (node, strip_id, ADDR_NONE, ADDR_NONE, value);
}

void
ArdourFeedback::update_all (std::string node, uint32_t strip_id, uint32_t plugin_id,
                            TypedValue value) const
{
	update_all (node, strip_id, plugin_id, ADDR_NONE, value);
}

void
ArdourFeedback::update_all (std::string node, uint32_t strip_id, uint32_t plugin_id, uint32_t param_id,
                            TypedValue value) const
{
	AddressVector addr = AddressVector ();

	if (strip_id != ADDR_NONE) {
		addr.push_back (strip_id);
	}

	if (plugin_id != ADDR_NONE) {
		addr.push_back (plugin_id);
	}

	if (param_id != ADDR_NONE) {
		addr.push_back (param_id);
	}

	ValueVector val = ValueVector ();
	val.push_back (value);

	server ().update_all_clients (NodeState (node, addr, val), false);
}

bool
ArdourFeedback::poll () const
{
	update_all (Node::transport_time, transport ().time ());

	Glib::Threads::Mutex::Lock lock (mixer ().mutex ());

	for (ArdourMixer::StripMap::iterator it = mixer ().strips ().begin (); it != mixer ().strips ().end (); ++it) {
		double db = it->second->meter_level_db ();
		update_all (Node::strip_meter, it->first, db);
	}

	if (server ().should_request_write ()) {
		server ().request_write ();
	}

	return true;
}

void
ArdourFeedback::observe_transport ()
{
	ARDOUR::Session& sess = session ();
	sess.TransportStateChange.connect (_transport_connections, MISSING_INVALIDATOR,
	                                   boost::bind<void> (TransportObserver (), this), event_loop ());
	OPTIONAL_CONNECT_HELPER(sess.TransportStateChange, _transport_connections);

	sess.RecordStateChanged.connect (_transport_connections, MISSING_INVALIDATOR,
	                                 boost::bind<void> (RecordStateObserver (), this), event_loop ());
	OPTIONAL_CONNECT_HELPER(sess.RecordStateChanged, _transport_connections);

	sess.tempo_map ().PropertyChanged.connect (_transport_connections, MISSING_INVALIDATOR,
	                                 boost::bind<void> (TempoObserver (), this), event_loop ());
	OPTIONAL_CONNECT_HELPER(sess.tempo_map ().PropertyChanged, _transport_connections);
}

void
ArdourFeedback::observe_mixer ()
{
	for (ArdourMixer::StripMap::iterator it = mixer().strips().begin(); it != mixer().strips().end(); ++it) {
		uint32_t strip_id                         = it->first;
		boost::shared_ptr<ArdourMixerStrip> strip = it->second;

		boost::shared_ptr<Stripable> stripable = strip->stripable ();

		stripable->gain_control ()->Changed.connect (*it->second, MISSING_INVALIDATOR,
		                                         boost::bind<void> (StripGainObserver (), this, strip_id), event_loop ());
		OPTIONAL_CONNECT_HELPER(stripable->gain_control ()->Changed, *it->second);

		if (stripable->pan_azimuth_control ()) {
			stripable->pan_azimuth_control ()->Changed.connect (*it->second, MISSING_INVALIDATOR,
			                                                boost::bind<void> (StripPanObserver (), this, strip_id), event_loop ());
			OPTIONAL_CONNECT_HELPER(stripable->pan_azimuth_control ()->Changed, *it->second);
		}

		stripable->mute_control ()->Changed.connect (*it->second, MISSING_INVALIDATOR,
		                                         boost::bind<void> (StripMuteObserver (), this, strip_id), event_loop ());
		OPTIONAL_CONNECT_HELPER(stripable->mute_control ()->Changed, *it->second);
	
		observe_strip_plugins (strip_id, strip->plugins ());
	}
}

void
ArdourFeedback::observe_strip_plugins (uint32_t strip_id, ArdourMixerStrip::PluginMap& plugins)
{
	for (ArdourMixerStrip::PluginMap::iterator it = plugins.begin(); it != plugins.end(); ++it) {
		uint32_t                             plugin_id = it->first;
		boost::shared_ptr<ArdourMixerPlugin> plugin    = it->second;
		boost::shared_ptr<PluginInsert>      insert    = plugin->insert ();
		uint32_t                             bypass    = insert->plugin ()->designated_bypass_port ();
		Evoral::Parameter                    param     = Evoral::Parameter (PluginAutomation, 0, bypass);
		boost::shared_ptr<AutomationControl> control   = insert->automation_control (param);

		if (control) {
			control->Changed.connect (*plugin, MISSING_INVALIDATOR,
			                          boost::bind<void> (PluginBypassObserver (), this, strip_id, plugin_id), event_loop ());
			OPTIONAL_CONNECT_HELPER(control->Changed, *plugin);
		}

		for (uint32_t param_id = 0; param_id < plugin->param_count (); ++param_id) {
			try {
				boost::shared_ptr<AutomationControl> control = plugin->param_control (param_id);

				control->Changed.connect (*plugin, MISSING_INVALIDATOR,
				                          boost::bind<void> (PluginParamValueObserver (), this, strip_id, plugin_id, param_id,
				                                             boost::weak_ptr<AutomationControl>(control)),
				                          event_loop ());
				OPTIONAL_CONNECT_HELPER(control->Changed, *plugin);
			} catch (ArdourMixerNotFoundException& e) {
				/* ignore */
			}
		}
	}
}
