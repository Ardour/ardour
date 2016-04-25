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
#include "ardour/monitor_control.h"
#include "ardour/rc_configuration.h"
#include "ardour/record_enable_control.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/vca.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using std::string;

gint VCA::next_number = 1;
string VCA::xml_node_name (X_("VCA"));

string
VCA::default_name_template ()
{
	return _("VCA %n");
}

int
VCA::next_vca_number ()
{
	/* recall that atomic_int_add() returns the value before the add. We
	 * start at one, then next one will be two etc.
	 */
	return g_atomic_int_add (&next_number, 1);
}

void
VCA::set_next_vca_number (uint32_t n)
{
	g_atomic_int_set (&next_number, n);
}

uint32_t
VCA::get_next_vca_number ()
{
	return g_atomic_int_get (&next_number);
}

VCA::VCA (Session& s,  uint32_t num, const string& name)
	: Stripable (s, name)
	, Muteable (s, name)
	, Automatable (s)
	, _number (num)
	, _gain_control (new GainControl (s, Evoral::Parameter (GainAutomation), boost::shared_ptr<AutomationList> ()))
{
}

int
VCA::init ()
{
	_solo_control.reset (new SoloControl (_session, X_("solo"), *this, *this));
	_mute_control.reset (new MuteControl (_session, X_("mute"), *this));
	_recenable_control.reset (new RecordEnableControl (_session, X_("recenable"), *this));
	_monitor_control.reset (new MonitorControl (_session, X_("monitoring"), *this));

	add_control (_gain_control);
	add_control (_solo_control);
	add_control (_mute_control);

	return 0;
}

VCA::~VCA ()
{
	DropReferences (); /* emit signal */
}

uint32_t
VCA::remote_control_id () const
{
	return 9999999 + _number;
}

XMLNode&
VCA::get_state ()
{
	XMLNode* node = new XMLNode (xml_node_name);
	node->add_property (X_("name"), _name);
	node->add_property (X_("number"), _number);

	node->add_child_nocopy (_gain_control->get_state());
	node->add_child_nocopy (_solo_control->get_state());
	node->add_child_nocopy (_mute_control->get_state());
	node->add_child_nocopy (get_automation_xml_state());

	node->add_child_nocopy (Slavable::get_state());

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

			if (!prop) {
				continue;
			}

			if (prop->value() == _gain_control->name()) {
				_gain_control->set_state (**i, version);
			}
			if (prop->value() == _solo_control->name()) {
				_solo_control->set_state (**i, version);
			}
			if (prop->value() == _mute_control->name()) {
				_mute_control->set_state (**i, version);
			}
		} else if ((*i)->name() == Slavable::xml_node_name) {
			Slavable::set_state (**i, version);
		}
	}

	return 0;
}

void
VCA::clear_all_solo_state ()
{
	_solo_control->clear_all_solo_state ();
}

int
VCA::assign_controls (boost::shared_ptr<VCA> vca)
{
	_gain_control->add_master (vca->gain_control());
	_solo_control->add_master (vca->solo_control());
	_mute_control->add_master (vca->mute_control());

	return 0;
}

int
VCA::unassign_controls (boost::shared_ptr<VCA> vca)
{
	if (!vca) {
		/* unassign from all */
		_gain_control->clear_masters ();
		_solo_control->clear_masters ();
		_mute_control->clear_masters ();
	} else {
		_gain_control->remove_master (vca->gain_control());
		_solo_control->remove_master (vca->solo_control());
		_mute_control->remove_master (vca->mute_control());
	}

	return 0;
}

MonitorState
VCA::monitoring_state () const
{
	/* XXX this has to get more complex but not clear how */
	return MonitoringInput;
}
