/*
    Copyright (C) 2016 Paul Davis

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

#include "pbd/convert.h"

#include "ardour/automation_control.h"
#include "ardour/gain_control.h"
#include "ardour/route.h"
#include "ardour/vca.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using std::string;

gint VCA::next_number = 0;
string VCA::xml_node_name (X_("VCA"));

string
VCA::default_name_template ()
{
	return _("VCA %n");
}

int
VCA::next_vca_number ()
{
	/* recall that atomic_int_add() returns the value before the add */
	return g_atomic_int_add (&next_number, 1) + 1;
}

VCA::VCA (Session& s, const string& name, uint32_t num)
	: SessionHandleRef (s)
	, Automatable (s)
	, _number (num)
	, _name (name)
	, _control (new GainControl (s, Evoral::Parameter (GainAutomation), boost::shared_ptr<AutomationList> ()))
{
	add_control (_control);
}

VCA::VCA (Session& s, XMLNode const & node, int version)
	: SessionHandleRef (s)
	, Automatable (s)
	, _number (0)
	, _control (new GainControl (s, Evoral::Parameter (GainAutomation), boost::shared_ptr<AutomationList> ()))
{
	add_control (_control);

	set_state (node, version);
}

void
VCA::set_value (double val, Controllable::GroupControlDisposition gcd)
{
	_control->set_value (val, gcd);
}

double
VCA::get_value() const
{
	return _control->get_value();
}

void
VCA::add (boost::shared_ptr<Route> r)
{
	boost::dynamic_pointer_cast<GainControl>(r->gain_control())->add_master (_control);
	std::cerr << name() << " now controlling " << r->name() << std::endl;
}

void
VCA::remove (boost::shared_ptr<Route> r)
{
	r->gain_control()->remove_master (_control);
}

void
VCA::set_name (string const& str)
{
	_name = str;
}

XMLNode&
VCA::get_state ()
{
	XMLNode* node = new XMLNode (xml_node_name);
	node->add_property (X_("name"), _name);
	node->add_property (X_("number"), _number);

	node->add_child_nocopy (_control->get_state());
	node->add_child_nocopy (get_automation_xml_state());

	return *node;
}

int
VCA::set_state (XMLNode const& node, int version)
{
	XMLProperty const* prop;

	if ((prop = node.property ("name")) != 0) {
		set_name (prop->value());
	}

	if ((prop = node.property ("number")) != 0) {
		_number = atoi (prop->value());
	}

	XMLNodeList const &children (node.children());
	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
		if ((*i)->name() == Controllable::xml_node_name) {
			XMLProperty* prop = (*i)->property ("name");
			if (prop && prop->value() == X_("gaincontrol")) {
				_control->set_state (**i, version);
			}
		}
	}

	return 0;
}
