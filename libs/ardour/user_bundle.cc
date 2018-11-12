/*
    Copyright (C) 2012 Paul Davis

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

#include "ardour/user_bundle.h"
#include "ardour/types_convert.h"
#include "pbd/i18n.h"
#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/xml++.h"

ARDOUR::UserBundle::UserBundle (std::string const & n)
	: Bundle (n)
{

}

ARDOUR::UserBundle::UserBundle (XMLNode const & node, bool i)
	: Bundle (i)
{
	if (set_state (node, Stateful::loading_state_version)) {
		throw failed_constructor ();
	}
}

int
ARDOUR::UserBundle::set_state (XMLNode const & node, int /*version*/)
{
	std::string str;
	if (!node.get_property ("name", str)) {
		PBD::error << _("Node for Bundle has no \"name\" property") << endmsg;
		return -1;
	}

	set_name (str);

	XMLNodeList const channels = node.children ();

	int n = 0;
	for (XMLNodeConstIterator i = channels.begin(); i != channels.end(); ++i) {

		if ((*i)->name() != "Channel") {
			PBD::error << string_compose (_("Unknown node \"%s\" in Bundle"), (*i)->name()) << endmsg;
			return -1;
		}

		if (!(*i)->get_property ("name", str)) {
			PBD::error << _("Node for Channel has no \"name\" property") << endmsg;
			return -1;
		}

		DataType type(DataType::NIL);
		if (!(*i)->get_property ("type", type)) {
			PBD::error << _("Node for Channel has no \"type\" property") << endmsg;
			return -1;
		}

		add_channel (str, type);

		XMLNodeList const ports = (*i)->children ();

		for (XMLNodeConstIterator j = ports.begin(); j != ports.end(); ++j) {
			if ((*j)->name() != "Port") {
				PBD::error << string_compose (_("Unknown node \"%s\" in Bundle"), (*j)->name()) << endmsg;
				return -1;
			}

			if (!(*j)->get_property ("name", str)) {
				PBD::error << _("Node for Port has no \"name\" property") << endmsg;
				return -1;
			}

			add_port_to_channel (n, str);
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

	node->set_property ("name", name ());

	{
		Glib::Threads::Mutex::Lock lm (_channel_mutex);

		for (std::vector<Channel>::iterator i = _channel.begin(); i != _channel.end(); ++i) {
			XMLNode* c = new XMLNode ("Channel");
			c->set_property ("name", i->name);
			c->set_property ("type", i->type);

			for (PortList::iterator j = i->ports.begin(); j != i->ports.end(); ++j) {
				XMLNode* p = new XMLNode ("Port");
				p->set_property ("name", *j);
				c->add_child_nocopy (*p);
			}

			node->add_child_nocopy (*c);
		}
	}

	return *node;
}
