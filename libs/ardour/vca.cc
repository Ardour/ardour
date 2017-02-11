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
#include "ardour/debug.h"
#include "ardour/gain_control.h"
#include "ardour/monitor_control.h"
#include "ardour/rc_configuration.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/vca.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using std::string;

Glib::Threads::Mutex VCA::number_lock;
int32_t VCA::next_number = 1;
string VCA::xml_node_name (X_("VCA"));

string
VCA::default_name_template ()
{
	return _("VCA %n");
}

int32_t
VCA::next_vca_number ()
{
	/* we could use atomic inc here, but elsewhere we need more complete
	   mutex semantics, so we have to do it here also.
	*/
	Glib::Threads::Mutex::Lock lm (number_lock);
	return next_number++;
}

void
VCA::set_next_vca_number (int32_t n)
{
	Glib::Threads::Mutex::Lock lm (number_lock);
	next_number = n;
}

int32_t
VCA::get_next_vca_number ()
{
	Glib::Threads::Mutex::Lock lm (number_lock);
	return next_number;
}

VCA::VCA (Session& s, int32_t num, const string& name)
	: Stripable (s, name, PresentationInfo (num, PresentationInfo::VCA))
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

	add_control (_gain_control);
	add_control (_solo_control);
	add_control (_mute_control);

	return 0;
}

VCA::~VCA ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("delete VCA %1\n", number()));
	{
		Glib::Threads::Mutex::Lock lm (number_lock);
		if (_number == next_number - 1) {
			/* this was the "last" VCA added, so rewind the next number so
			 * that future VCAs get numbered as intended
			 */
			next_number--;
		}
	}
}

string
VCA::full_name() const
{
	/* name() is never empty - default is VCA %n */
	return string_compose (_("VCA %1 : %2"), _number, name());
}

XMLNode&
VCA::get_state ()
{
	XMLNode* node = new XMLNode (xml_node_name);
	node->add_property (X_("name"), _name);
	node->add_property (X_("number"), _number);

	node->add_child_nocopy (_presentation_info.get_state());

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

	Stripable::set_state (node, version);

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

MonitorState
VCA::monitoring_state () const
{
	/* XXX this has to get more complex but not clear how */
	return MonitoringInput;
}

bool
VCA::slaved () const
{
	if (!_gain_control) {
		return false;
	}
	/* just test one particular control, not all of them */
	return _gain_control->slaved ();
}

bool
VCA::slaved_to (boost::shared_ptr<VCA> vca) const
{
	if (!vca || !_gain_control) {
		return false;
	}

	/* just test one particular control, not all of them */

	return _gain_control->slaved_to (vca->gain_control());
}
