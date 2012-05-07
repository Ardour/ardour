/*
    Copyright (C) 2000 Paul Davis 

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

*/

#include <string>

#include <sigc++/bind.h>

#include <pbd/failed_constructor.h>
#include <pbd/xml++.h>
#include <pbd/stacktrace.h>

#include <ardour/insert.h>
#include <ardour/mtdm.h>
#include <ardour/plugin.h>
#include <ardour/port.h>
#include <ardour/route.h>
#include <ardour/ladspa_plugin.h>

#ifdef HAVE_LILV
#include <ardour/lv2_plugin.h>
#endif

#ifdef VST_SUPPORT
#include <ardour/vst_plugin.h>
#endif

#ifdef HAVE_AUDIOUNITS
#include <ardour/audio_unit.h>
#endif

#include <ardour/audioengine.h>
#include <ardour/session.h>
#include <ardour/types.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

Insert::Insert(Session& s, string name, Placement p)
	: Redirect (s, name, p)
{
}

Insert::Insert(Session& s, string name, Placement p, int imin, int imax, int omin, int omax)
	: Redirect (s, name, p, imin, imax, omin, omax)
{
}

/***************************************************************
 Plugin inserts: send data through a plugin
 ***************************************************************/

const string PluginInsert::port_automation_node_name = "PortAutomation";

PluginInsert::PluginInsert (Session& s, boost::shared_ptr<Plugin> plug, Placement placement)
	: Insert (s, plug->name(), placement)
{
	/* the first is the master */

	_plugins.push_back (plug);

	_plugins[0]->ParameterChanged.connect (mem_fun (*this, &PluginInsert::parameter_changed));
	
	init ();

	RedirectCreated (this); /* EMIT SIGNAL */
}

PluginInsert::PluginInsert (Session& s, const XMLNode& node)
	: Insert (s, "will change", PreFader)
{
	if (set_state (node)) {
		throw failed_constructor();
	}

	_plugins[0]->ParameterChanged.connect (mem_fun (*this, &PluginInsert::parameter_changed));
}

PluginInsert::PluginInsert (const PluginInsert& other)
	: Insert (other._session, other.plugin()->name(), other.placement())
{
	uint32_t count = other._plugins.size();

	/* make as many copies as requested */
	for (uint32_t n = 0; n < count; ++n) {
		_plugins.push_back (plugin_factory (other.plugin (n)));
	}


	_plugins[0]->ParameterChanged.connect (mem_fun (*this, &PluginInsert::parameter_changed));

	init ();

	RedirectCreated (this); /* EMIT SIGNAL */
}

int
PluginInsert::set_count (uint32_t num)
{
	bool require_state = !_plugins.empty();

	/* this is a bad idea.... we shouldn't do this while active.
	   only a route holding their redirect_lock should be calling this 
	*/

	if (num == 0) { 
		return -1;
	} else if (num > _plugins.size()) {
		uint32_t diff = num - _plugins.size();

		for (uint32_t n = 0; n < diff; ++n) {
			_plugins.push_back (plugin_factory (_plugins[0]));

			if (require_state) {
				/* XXX do something */
			}
		}

	} else if (num < _plugins.size()) {
		uint32_t diff = _plugins.size() - num;
		for (uint32_t n= 0; n < diff; ++n) {
			_plugins.pop_back();
		}
	}

	return 0;
}

void
PluginInsert::init ()
{
	set_automatable ();
}

PluginInsert::~PluginInsert ()
{
	GoingAway (); /* EMIT SIGNAL */
}

void
PluginInsert::automation_list_creation_callback (uint32_t which, AutomationList& alist)
{
	alist.automation_state_changed.connect (sigc::bind (mem_fun (*this, &PluginInsert::auto_state_changed), (which)));
}

void
PluginInsert::auto_state_changed (uint32_t which)
{
	AutomationList& alist (automation_list (which));

	/* don't reset automation if we're moving to Off or Write mode;
	   if we're moving to Write, the user may have manually set up automation
	   that they don't want to lose */		
	if (alist.automation_state() != Auto_Off && alist.automation_state() != Auto_Write) {
		_plugins[0]->set_parameter (which, alist.eval (_session.transport_frame()));
	}
}

uint32_t
PluginInsert::output_streams() const
{
	int32_t out = _plugins[0]->get_info()->n_outputs;

	if (out < 0) {
		return _plugins[0]->output_streams ();
	} else {
		return out * _plugins.size();
	}
}

uint32_t
PluginInsert::input_streams() const
{
	int32_t in = _plugins[0]->get_info()->n_inputs;

	if (in < 0) {
		return _plugins[0]->input_streams ();
	} else {
		return in * _plugins.size();
	}
}

uint32_t
PluginInsert::natural_output_streams() const
{
	return _plugins[0]->get_info()->n_outputs;
}

uint32_t
PluginInsert::natural_input_streams() const
{
	return _plugins[0]->get_info()->n_inputs;
}

bool
PluginInsert::is_generator() const
{
	/* XXX more finesse is possible here. VST plugins have a
	   a specific "instrument" flag, for example.
	 */

	return _plugins[0]->get_info()->n_inputs == 0;
}

void
PluginInsert::set_automatable ()
{
	/* fill the parameter automation list with null AutomationLists */

	parameter_automation.assign (_plugins.front()->parameter_count(), (AutomationList*) 0);

	set<uint32_t> a;
	
	a = _plugins.front()->automatable ();

	for (set<uint32_t>::iterator i = a.begin(); i != a.end(); ++i) {
		can_automate (*i);
	}
}

void
PluginInsert::parameter_changed (uint32_t which, float val)
{
	vector<boost::shared_ptr<Plugin> >::iterator i = _plugins.begin();

	/* don't set the first plugin, just all the slaves */

	if (i != _plugins.end()) {
		++i;
		for (; i != _plugins.end(); ++i) {
			(*i)->set_parameter (which, val);
		}
	}
}

int
PluginInsert::set_block_size (nframes_t nframes)
{
	int ret = 0;

	for (vector<boost::shared_ptr<Plugin> >::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		if ((*i)->set_block_size (nframes)) {
			ret = -1;
		}
	}

	return ret;
}

void
PluginInsert::activate ()
{
	for (vector<boost::shared_ptr<Plugin> >::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->activate ();
	}
}

void
PluginInsert::deactivate ()
{
	for (vector<boost::shared_ptr<Plugin> >::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->deactivate ();
	}
}

void
PluginInsert::flush ()
{
	for (vector<boost::shared_ptr<Plugin> >::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->flush ();
	}
}

void
PluginInsert::connect_and_run (vector<Sample*>& bufs, uint32_t nbufs, nframes_t nframes, nframes_t offset, bool with_auto, nframes_t now)
{
	int32_t in_index = 0;
	int32_t out_index = 0;

	/* Note that we've already required that plugins
	   be able to handle in-place processing.
	*/
        
	// cerr << "Connect and run for " << _plugins[0]->name() << " auto ? " << with_auto << " nf = " << nframes << " off = " << offset 
        // << " nbufs = " << nbufs << " of " << bufs.size() << " with ninputs = " << input_streams() 
        // << endl;
	
	if (with_auto) {

		vector<AutomationList*>::iterator li;
		uint32_t n;
		
		for (n = 0, li = parameter_automation.begin(); li != parameter_automation.end(); ++li, ++n) {
			
			AutomationList* alist = *li;

			if (alist && alist->automation_playback()) {
				bool valid;
				
				float val = alist->rt_safe_eval (now, valid);				

				if (valid) {
					/* set the first plugin, the others will be set via signals */
					// cerr << "\t@ " << now << " param[" << n << "] = " << val << endl;
					_plugins[0]->set_parameter (n, val);
				}

			} 
		}
	} 

	for (vector<boost::shared_ptr<Plugin> >::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
		(*i)->connect_and_run (bufs, nbufs, in_index, out_index, nframes, offset);
	}
}

void
PluginInsert::automation_snapshot (nframes_t now, bool force)
{
	vector<AutomationList*>::iterator li;
	uint32_t n;

	for (n = 0, li = parameter_automation.begin(); li != parameter_automation.end(); ++li, ++n) {
		
		AutomationList *alist = *li;

		if (alist && alist->automation_write () && _session.transport_rolling()) {
			
			float val = _plugins[0]->get_parameter (n);
			alist->rt_add (now, val);
			last_automation_snapshot = now;
		}
	}
}

void
PluginInsert::transport_stopped (nframes_t now)
{
	vector<AutomationList*>::iterator li;
	uint32_t n;

	for (n = 0, li = parameter_automation.begin(); li != parameter_automation.end(); ++li, ++n) {

		AutomationList* alist = *li;

		if (alist) {
			alist->write_pass_finished (now);
			if (alist->automation_state() == Auto_Touch || alist->automation_state() == Auto_Play) {
				_plugins[0]->set_parameter (n, alist->eval (now));
			}
		}
	}
}

void
PluginInsert::silence (nframes_t nframes)
{
	int32_t in_index = 0;
	int32_t out_index = 0;
	int32_t n;

	if (active()) {
		for (vector<boost::shared_ptr<Plugin> >::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
			n = input_streams();
			(*i)->connect_and_run (_session.get_silent_buffers (n), n, in_index, out_index, nframes, 0);
		}
	}
}
	
void
PluginInsert::run (vector<Sample *>& bufs, uint32_t nbufs, nframes_t nframes)
{
	if (active()) {

		if (_session.transport_rolling()) {
			automation_run (bufs, nbufs, nframes);
		} else {
			connect_and_run (bufs, nbufs, nframes, 0, false);
		}

	} else {

		uint32_t in = input_streams ();
		uint32_t out = output_streams ();

		if (out > in) {
			
			/* not active, but something has make up for any channel count increase,
			   so copy the last buffer to the extras.
			*/
			
			for (uint32_t n = out - in; n < out && n < nbufs; ++n) {
				memcpy (bufs[n], bufs[in - 1], sizeof (Sample) * nframes);
			}
		}
	}
}

void
PluginInsert::set_parameter (uint32_t port, float val)
{
	/* the others will be set from the event triggered by this */

	float last_val = _plugins[0]->get_parameter (port);
	Plugin::ParameterDescriptor desc;
	_plugins[0]->get_parameter_descriptor(port, desc);
	
	_plugins[0]->set_parameter (port, val);
	
	if (automation_list (port).automation_write() && _session.transport_rolling()) {
		if ( desc.toggled )  //store the previous value just before this so any interpolation works right 
			automation_list (port).add (_session.audible_frame()-1, last_val);
		automation_list (port).add (_session.audible_frame(), val);
	}

	_session.set_dirty();
}

void
PluginInsert::automation_run (vector<Sample *>& bufs, uint32_t nbufs, nframes_t nframes)
{
	ControlEvent next_event (0, 0.0f);
	nframes_t now = _session.transport_frame ();
	nframes_t end = now + nframes;
	nframes_t offset = 0;

	Glib::Mutex::Lock lm (_automation_lock, Glib::TRY_LOCK);

	if (!lm.locked()) {
		connect_and_run (bufs, nbufs, nframes, 0, false, now);
		return;
	}

	if (!find_next_event (now, end, next_event) || requires_fixed_size_buffers()) {
 		/* no events have a time within the relevant range */
 		connect_and_run (bufs, nbufs, nframes, 0, true, now);
 		return;
	}
	
	while (nframes) {
		nframes_t cnt = min (((nframes_t) ceil (next_event.when) - now), nframes);
		
		connect_and_run (bufs, nbufs, cnt, offset, true, now);
		
		nframes -= cnt;
		now += cnt;
		offset += cnt;
		
		if (!find_next_event (now, end, next_event)) {
			break;
		}
	}

 	/* cleanup anything that is left to do */
  
 	if (nframes) {
 		connect_and_run (bufs, nbufs, nframes, offset, true, now);
  	}

}	

float
PluginInsert::default_parameter_value (uint32_t port)
{
	if (_plugins.empty()) {
		fatal << _("programming error: ") << X_("PluginInsert::default_parameter_value() called with no plugin")
		      << endmsg;
		/*NOTREACHED*/
	}

	return _plugins[0]->default_value (port);
}
	
void
PluginInsert::set_port_automation_state (uint32_t port, AutoState s)
{
	if (port < _plugins[0]->parameter_count()) {
		
		AutomationList& al = automation_list (port);

		if (s != al.automation_state()) {
			al.set_automation_state (s);
			_session.set_dirty ();
		}
	}
}

AutoState
PluginInsert::get_port_automation_state (uint32_t port)
{
	if (port < _plugins[0]->parameter_count()) {
		return automation_list (port).automation_state();
	} else {
		return Auto_Off;
	}
}

void
PluginInsert::protect_automation ()
{
	set<uint32_t> automated_params;

	what_has_automation (automated_params);

	for (set<uint32_t>::iterator i = automated_params.begin(); i != automated_params.end(); ++i) {

		AutomationList& al = automation_list (*i);

		switch (al.automation_state()) {
		case Auto_Write:
			al.set_automation_state (Auto_Off);
			break;
		case Auto_Touch:
			al.set_automation_state (Auto_Play);
			break;
		default:
			break;
		}
	}
}

boost::shared_ptr<Plugin>
PluginInsert::plugin_factory (boost::shared_ptr<Plugin> other)
{
	boost::shared_ptr<LadspaPlugin> lp;
#ifdef HAVE_LILV
	boost::shared_ptr<LV2Plugin> lv2p;
#endif
#ifdef VST_SUPPORT
	boost::shared_ptr<VSTPlugin> vp;
#endif
#ifdef HAVE_AUDIOUNITS
	boost::shared_ptr<AUPlugin> ap;
#endif

	if ((lp = boost::dynamic_pointer_cast<LadspaPlugin> (other)) != 0) {
		return boost::shared_ptr<Plugin> (new LadspaPlugin (*lp));
#ifdef HAVE_LILV
	} else if ((lv2p = boost::dynamic_pointer_cast<LV2Plugin> (other)) != 0) {
		return boost::shared_ptr<Plugin> (new LV2Plugin (*lv2p));
#endif
#ifdef VST_SUPPORT
	} else if ((vp = boost::dynamic_pointer_cast<VSTPlugin> (other)) != 0) {
		return boost::shared_ptr<Plugin> (new VSTPlugin (*vp));
#endif
#ifdef HAVE_AUDIOUNITS
	} else if ((ap = boost::dynamic_pointer_cast<AUPlugin> (other)) != 0) {
		return boost::shared_ptr<Plugin> (new AUPlugin (*ap));
#endif
	}

	fatal << string_compose (_("programming error: %1"),
			  X_("unknown plugin type in PluginInsert::plugin_factory"))
	      << endmsg;
	/*NOTREACHED*/
	return boost::shared_ptr<Plugin> ((Plugin*) 0);
}

int32_t
PluginInsert::configure_io (int32_t magic, int32_t in, int32_t out)
{
	int32_t ret;

	if ((ret = set_count (magic)) < 0) {
		return ret;
	}

	/* if we're running replicated plugins, each plugin has
	   the same i/o configuration and we may need to announce how many
	   output streams there are.

	   if we running a single plugin, we need to configure it.
	*/

	return _plugins[0]->configure_io (in, out);
}

int32_t 
PluginInsert::can_do (int32_t in, int32_t& out)
{
	return _plugins[0]->can_do (in, out);
}

XMLNode&
PluginInsert::get_state(void)
{
	return state (true);
}

XMLNode&
PluginInsert::state (bool full)
{
	char buf[256];
	XMLNode *node = new XMLNode("Insert");

	node->add_child_nocopy (Redirect::state (full));

	node->add_property ("type", _plugins[0]->state_node_name());
	node->add_property("unique-id", _plugins[0]->unique_id());
	node->add_property("count", string_compose("%1", _plugins.size()));
	node->add_child_nocopy (_plugins[0]->get_state());

	/* add controllables */

	XMLNode* control_node = new XMLNode (X_("controls"));

	for (uint32_t x = 0; x < _plugins[0]->parameter_count(); ++x) {
		Controllable* c = _plugins[0]->get_nth_control (x, true);
		if (c) {
			XMLNode& controllable_state (c->get_state());
			controllable_state.add_property ("parameter", to_string (x, std::dec));
			control_node->add_child_nocopy (controllable_state);
		}
	}
	node->add_child_nocopy (*control_node);

	/* add port automation state */
	XMLNode *autonode = new XMLNode(port_automation_node_name);
	set<uint32_t> automatable = _plugins[0]->automatable();

	for (set<uint32_t>::iterator x =  automatable.begin(); x != automatable.end(); ++x) {

		XMLNode* child = new XMLNode("port");
		snprintf(buf, sizeof(buf), "%" PRIu32, *x);
		child->add_property("number", string(buf));

#ifdef HAVE_LILV
		LV2Plugin* lv2p = dynamic_cast<LV2Plugin*>(_plugins[0].get());
		if (lv2p) {
			child->add_property("symbol", string(lv2p->port_symbol(*x)));
		}
#endif

		child->add_child_nocopy (automation_list (*x).state (full));
		autonode->add_child_nocopy (*child);
	}

	node->add_child_nocopy (*autonode);
	
	return *node;
}

int
PluginInsert::set_state(const XMLNode& node)
{
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	XMLPropertyList plist;
	const XMLProperty *prop;
	ARDOUR::PluginType type;

	if ((prop = node.property ("type")) == 0) {
		error << _("XML node describing insert is missing the `type' field") << endmsg;
		return -1;
	}

	if (prop->value() == X_("ladspa") || prop->value() == X_("Ladspa")) { /* handle old school sessions */
		type = ARDOUR::LADSPA;
	} else if (prop->value() == X_("lv2")) {
		type = ARDOUR::LV2;
	} else if (prop->value() == X_("vst")) {
		type = ARDOUR::VST;
	} else if (prop->value() == X_("audiounit")) {
		type = ARDOUR::AudioUnit;
	} else {
		error << string_compose (_("unknown plugin type %1 in plugin insert state"),
				  prop->value())
		      << endmsg;
		return -1;
	}

	prop = node.property ("unique-id");
	if (prop == 0) {
#ifdef VST_SUPPORT
		/* older sessions contain VST plugins with only an "id" field.
		 */
		
		if (type == ARDOUR::VST) {
			prop = node.property ("id");
		}
#endif		
		/* recheck  */

		if (prop == 0) {
			error << _("Plugin has no unique ID field") << endmsg;
			return -1;
		}
	}

	boost::shared_ptr<Plugin> plugin;

	plugin = find_plugin (_session, prop->value(), type);	

	if (plugin == 0) {
		error << string_compose(_("Found a reference to a plugin (\"%1\") that is unknown.\n"
				   "Perhaps it was removed or moved since it was last used."), prop->value()) 
		      << endmsg;
		return -1;
	}

	uint32_t count = 1;

	if ((prop = node.property ("count")) != 0) {
		sscanf (prop->value().c_str(), "%u", &count);
	}

	if (_plugins.size() != count) {
		
		_plugins.push_back (plugin);
		
		for (uint32_t n=1; n < count; ++n) {
			_plugins.push_back (plugin_factory (plugin));
		}
	}

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == plugin->state_node_name()) {
			for (vector<boost::shared_ptr<Plugin> >::iterator i = _plugins.begin(); i != _plugins.end(); ++i) {
				(*i)->set_state (**niter);
			}
			break;
		}
	} 

	if (niter == nlist.end()) {
		error << string_compose(_("XML node describing a plugin insert is missing the `%1' information"), plugin->state_node_name()) << endmsg;
		return -1;
	}

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == Redirect::state_node_name) {
			Redirect::set_state (**niter);
			break;
		}
	}

	if (niter == nlist.end()) {
		error << _("XML node describing insert is missing a Redirect node") << endmsg;
		return -1;
	}

	/* look for controllables node */
	
	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		if ((*niter)->name() != X_("controls")) {
			continue;
		}
		
		XMLNodeList grandchildren ((*niter)->children());
		XMLProperty* prop;
		XMLNodeIterator gciter;
		uint32_t param;
		
		for (gciter = grandchildren.begin(); gciter != grandchildren.end(); ++gciter) {
			if ((prop = (*gciter)->property (X_("parameter"))) != 0) {
				param = atoi (prop->value());
				/* force creation of controllable for this parameter */
				_plugins[0]->make_nth_control (param, **gciter);
			} 
		}

		break;
	}
		
	set_automatable ();
	
	/* look for port automation node */
	
	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		if ((*niter)->name() != port_automation_node_name) {
			continue;
		}

		XMLNodeList cnodes;
		XMLProperty *cprop;
		XMLNodeConstIterator iter;
		XMLNode *child;
		const char *port;
		uint32_t port_id;
		
		cnodes = (*niter)->children ("port");
		
		for(iter = cnodes.begin(); iter != cnodes.end(); ++iter){
			
			child = *iter;
			
			if ((cprop = child->property("number")) != 0) {
				port = cprop->value().c_str();
			} else {
				warning << _("PluginInsert: Auto: no ladspa port number") << endmsg;
				continue;
			}
			
			sscanf (port, "%" PRIu32, &port_id);
			
			if (port_id >= _plugins[0]->parameter_count()) {
				warning << _("PluginInsert: Auto: port id out of range") << endmsg;
				continue;
			}

			if (!child->children().empty()) {
				automation_list (port_id).set_state (*child->children().front());
			} else {
				if ((cprop = child->property("auto")) != 0) {
					
					/* old school */

					int x;
					sscanf (cprop->value().c_str(), "0x%x", &x);
					automation_list (port_id).set_automation_state (AutoState (x));

				} else {
					
					/* missing */
					
					automation_list (port_id).set_automation_state (Auto_Off);
				}
			}

		}

		/* done */

		break;
	} 

	if (niter == nlist.end()) {
		warning << string_compose(_("XML node describing a port automation is missing the `%1' information"), port_automation_node_name) << endmsg;
	}
	
	// The name of the PluginInsert comes from the plugin, nothing else
	set_name(plugin->get_info()->name,this);
	
	return 0;
}

string
PluginInsert::describe_parameter (uint32_t what)
{
	return _plugins[0]->describe_parameter (what);
}

nframes_t 
PluginInsert::latency() 
{
	return _plugins[0]->latency ();
}
	
ARDOUR::PluginType
PluginInsert::type ()
{
	return plugin()->get_info()->type;
}

/***************************************************************
 Port inserts: send output to a port, pick up input at a port
 ***************************************************************/

PortInsert::PortInsert (Session& s, Placement p)
	: Insert (s, string_compose (_("insert %1"), (bitslot = s.next_insert_id()) + 1), p, 1, -1, 1, -1)
{
	init ();
	RedirectCreated (this); /* EMIT SIGNAL */

}

PortInsert::PortInsert (const PortInsert& other)
	: Insert (other._session, string_compose (_("insert %1"), (bitslot = other._session.next_insert_id()) + 1), other.placement(), 1, -1, 1, -1)
{
	init ();
	RedirectCreated (this); /* EMIT SIGNAL */
}

void
PortInsert::init ()
{
	_mtdm = 0;
	_latency_detect = false;
	_latency_flush_frames = false;
	_measured_latency = 0;
}

PortInsert::PortInsert (Session& s, const XMLNode& node)
	: Insert (s, "will change", PreFader)
{
	init ();

	bitslot = 0xffffffff;

	if (set_state (node)) {
		throw failed_constructor();
	}

	RedirectCreated (this); /* EMIT SIGNAL */
}

PortInsert::~PortInsert ()
{
	delete _mtdm;
	GoingAway ();
}

void
PortInsert::start_latency_detection ()
{
	if (_mtdm != 0) {
		delete _mtdm;
	}

	_mtdm = new MTDM;
	_latency_flush_frames = false;
	_latency_detect = true;
	_measured_latency = 0;
}

void
PortInsert::stop_latency_detection ()
{
	_latency_flush_frames = latency() + _session.engine().frames_per_cycle();
	_latency_detect = false;
}

void
PortInsert::set_measured_latency (nframes_t n)
{
	_measured_latency = n;
}

void
PortInsert::run (vector<Sample *>& bufs, uint32_t nbufs, nframes_t nframes)
{
	if (n_outputs() == 0) {
		return;
	}

	vector<Port*>::iterator o;

	if (_latency_detect) {

		if (n_inputs() != 0) {
			Sample* in = get_input_buffer (0, nframes);
			Sample* out = get_output_buffer (0, nframes);

			_mtdm->process (nframes, in, out);
			
			for (o = _outputs.begin(); o != _outputs.end(); ++o) {
				(*o)->mark_silence (false);
			}
		}

		return;

	} else if (_latency_flush_frames) {

		/* wait for the entire input buffer to drain before picking up input again so that we can't
		   hear the remnants of whatever MTDM pumped into the pipeline.
		*/

		silence (nframes);

		if (_latency_flush_frames > nframes) {
			_latency_flush_frames -= nframes;
		} else {
			_latency_flush_frames = 0;
		}

		return;
	}

	if (!active()) {
		/* deliver silence */
		silence (nframes);
		return;
	}

	/* deliver output */

	uint32_t n;

	for (o = _outputs.begin(), n = 0; o != _outputs.end(); ++o, ++n) {
		memcpy (get_output_buffer (n, nframes), bufs[min(nbufs,n)], sizeof (Sample) * nframes);
		(*o)->mark_silence (false);
	}
	
	vector<Port*>::iterator i;

	/* collect input */
	
	for (i = _inputs.begin(), n = 0; i != _inputs.end(); ++i, ++n) {
		memcpy (bufs[min(nbufs,n)], get_input_buffer (n, nframes), sizeof (Sample) * nframes);
	}
}

XMLNode&
PortInsert::get_state(void)
{
	return state (true);
}

XMLNode&
PortInsert::state (bool full)
{
	XMLNode *node = new XMLNode("Insert");
	char buf[32];
	node->add_child_nocopy (Redirect::state(full));	
	node->add_property ("type", "port");
	snprintf (buf, sizeof (buf), "%" PRIu32, bitslot);
	node->add_property ("bitslot", buf);
	snprintf (buf, sizeof (buf), "%u", _measured_latency);
	node->add_property("latency", buf);
	snprintf (buf, sizeof (buf), "%u", _session.get_block_size());
	node->add_property("block_size", buf);

	return *node;
}

int
PortInsert::set_state(const XMLNode& node)
{
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	XMLPropertyList plist;
	const XMLProperty *prop;

	if ((prop = node.property ("type")) == 0) {
		error << _("XML node describing insert is missing the `type' field") << endmsg;
		return -1;
	}
	
	if (prop->value() != "port") {
		error << _("non-port insert XML used for port plugin insert") << endmsg;
		return -1;
	}

	uint32_t blocksize = 0;
	if ((prop = node.property ("block_size")) != 0) {
		sscanf (prop->value().c_str(), "%u", &blocksize);
	}
		
	//if the jack period is the same as when the value was saved, we can recall our latency..
	if ( (_session.get_block_size() == blocksize) && (prop = node.property ("latency")) != 0) {
		uint32_t latency = 0;
		sscanf (prop->value().c_str(), "%u", &latency);
		_measured_latency = latency;
	}

	if ((prop = node.property ("bitslot")) == 0) {
		bitslot = _session.next_insert_id();
	} else {
		uint32_t old_bitslot = bitslot;
		sscanf (prop->value().c_str(), "%" PRIu32, &bitslot);

		if (old_bitslot != bitslot) {
			_session.mark_insert_id (bitslot);
		}
	}

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == Redirect::state_node_name) {
			Redirect::set_state (**niter);
			break;
		}
	}

	if (niter == nlist.end()) {
		error << _("XML node describing insert is missing a Redirect node") << endmsg;
		return -1;
	}

	return 0;
}

nframes_t 
PortInsert::latency() 
{
	/* because we deliver and collect within the same cycle,
	   all I/O is necessarily delayed by at least frames_per_cycle().

	   if the return port for insert has its own latency, we
	   need to take that into account too.
	*/

	if (_measured_latency == 0) {
		return _session.engine().frames_per_cycle() + input_latency();
	} else {
		return _measured_latency;
	}
}

int32_t
PortInsert::can_do (int32_t in, int32_t& out) 
{
	if (input_maximum() == -1 && output_maximum() == -1) {

		/* not configured yet */

		out = in;
		return 1;

	} else {

		/* the "input" config for a port insert corresponds to how
		   many output ports it will have.
		*/

		if (output_maximum() == in) {
			out = in;
			return 1;
		} 
	}

	return -1;
}

int32_t
PortInsert::configure_io (int32_t ignored_magic, int32_t in, int32_t out)
{
	/* do not allow configuration to be changed outside the range of
	   the last request config. or something like that.
	*/

	set_output_maximum (in);
	set_output_minimum (in);
	set_input_maximum (out);
	set_input_minimum (out);

	/* this can be momentarily confusing: 

	   the number of inputs we are required to handle corresponds 
	   to the number of output ports we need.

	   the number of outputs we are required to have corresponds
	   to the number of input ports we need.
	*/

	if (in < 0) {
		in = n_outputs ();
	} 

	if (out < 0) {
		out = n_inputs ();
	}

	return ensure_io (out, in, false, this);
}

uint32_t
PortInsert::output_streams() const
{
	return n_inputs ();
}

uint32_t
PortInsert::input_streams() const
{
	return n_outputs ();
}

