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

	{
		Glib::Mutex::Lock lm (_ports_mutex);

		for (std::vector<PortList>::iterator i = _ports.begin(); i != _ports.end(); ++i) {
			
			XMLNode* c = new XMLNode ("Channel");
			
			for (PortList::iterator j = i->begin(); j != i->end(); ++j) {
				XMLNode* p = new XMLNode ("Port");
				p->add_property ("name", *j);
				c->add_child_nocopy (*p);
			}
			
			node->add_child_nocopy (*c);
		}
	}

	return *node;
}
