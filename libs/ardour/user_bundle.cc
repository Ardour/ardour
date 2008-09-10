#include <cassert>
#include <pbd/failed_constructor.h>
#include <pbd/compose.h>
#include <pbd/xml++.h>
#include "ardour/user_bundle.h"
#include "ardour/port_set.h"
#include "ardour/io.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "i18n.h"

ARDOUR::UserBundle::UserBundle (std::string const & n)
	: Bundle (n)
{

}

ARDOUR::UserBundle::UserBundle (XMLNode const & x, bool i)
	: Bundle (i)
{
	if (set_state (x)) {
		throw failed_constructor ();
	}
}

ARDOUR::ChanCount
ARDOUR::UserBundle::nchannels () const
{
	Glib::Mutex::Lock lm (_ports_mutex);
	return ChanCount (type(), _ports.size ());
}

const ARDOUR::PortList&
ARDOUR::UserBundle::channel_ports (uint32_t n) const
{
	assert (n < nchannels ().get (type()));

	Glib::Mutex::Lock lm (_ports_mutex);
	return _ports[n];
}

void
ARDOUR::UserBundle::add_port_to_channel (uint32_t c, std::string const & p)
{
	assert (c < nchannels ().get (type()));
	
	PortsWillChange (c);

	{
		Glib::Mutex::Lock lm (_ports_mutex);
		_ports[c].push_back (p);
	}
	
	PortsHaveChanged (c);
}

void
ARDOUR::UserBundle::remove_port_from_channel (uint32_t c, std::string const & p)
{
	assert (c < nchannels ().get (type()));

	PortsWillChange (c);

	{
		Glib::Mutex::Lock lm (_ports_mutex);
		PortList::iterator i = std::find (_ports[c].begin(), _ports[c].end(), p);
		if (i != _ports[c].end()) {
			_ports[c].erase (i);
		}
	}
	
	PortsHaveChanged (c);
}

bool
ARDOUR::UserBundle::port_attached_to_channel (uint32_t c, std::string const & p) const
{
	assert (c < nchannels ().get (type()));

	Glib::Mutex::Lock lm (_ports_mutex);
	return std::find (_ports[c].begin(), _ports[c].end(), p) != _ports[c].end();
}

void
ARDOUR::UserBundle::add_channel ()
{
	ConfigurationWillChange ();

	{
		Glib::Mutex::Lock lm (_ports_mutex);
		_ports.resize (_ports.size() + 1);
	}
	
	ConfigurationHasChanged ();
}

void
ARDOUR::UserBundle::set_channels (uint32_t n)
{
	ConfigurationWillChange ();

	{
		Glib::Mutex::Lock lm (_ports_mutex);
		_ports.resize (n);
	}

	ConfigurationHasChanged ();
}

void
ARDOUR::UserBundle::remove_channel (uint32_t r)
{
	assert (r < nchannels ().get (type()));

	ConfigurationWillChange ();

	{
		Glib::Mutex::Lock lm (_ports_mutex);
		_ports.erase (_ports.begin() + r, _ports.begin() + r + 1);
	}

	ConfigurationHasChanged ();
}

int
ARDOUR::UserBundle::set_state (XMLNode const & node)
{
	XMLProperty const * name;
	
	if ((name = node.property ("name")) == 0) {
		PBD::error << _("Node for Bundle has no \"name\" property") << endmsg;
		return -1;
	}

	set_name (name->value ());

	XMLNodeList const channels = node.children ();

	int n = 0;
	for (XMLNodeConstIterator i = channels.begin(); i != channels.end(); ++i) {

		if ((*i)->name() != "Channel") {
			PBD::error << string_compose (_("Unknown node \"%s\" in Bundle"), (*i)->name()) << endmsg;
			return -1;
		}

		add_channel ();

		XMLNodeList const ports = (*i)->children ();

		for (XMLNodeConstIterator j = ports.begin(); j != ports.end(); ++j) {
			if ((*j)->name() != "Port") {
				PBD::error << string_compose (_("Unknown node \"%s\" in Bundle"), (*j)->name()) << endmsg;
				return -1;
			}

			if ((name = (*j)->property ("name")) == 0) {
				PBD::error << _("Node for Port has no \"name\" property") << endmsg;
				return -1;
			} 
			
			add_port_to_channel (n, name->value ());
		}

		++n;
	}

	return 0;
}

XMLNode&
ARDOUR::UserBundle::get_state ()
{
	XMLNode *node;
	
	if (ports_are_inputs ()) {
		node = new XMLNode ("InputBundle");
	} else {
		node = new XMLNode ("OutputBundle");
	}

	node->add_property ("name", name ());

	for (std::vector<PortList>::iterator i = _ports.begin(); i != _ports.end(); ++i) {

		XMLNode* c = new XMLNode ("Channel");

		for (PortList::iterator j = i->begin(); j != i->end(); ++j) {
			XMLNode* p = new XMLNode ("Port");
			p->add_property ("name", *j);
			c->add_child_nocopy (*p);
		}

		node->add_child_nocopy (*c);
	}

	return *node;
}
