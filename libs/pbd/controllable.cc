#include <pbd/controllable.h>
#include <pbd/xml++.h>
#include <pbd/error.h>

#include "i18n.h"

using namespace PBD;

sigc::signal<void,Controllable*> Controllable::Destroyed;
sigc::signal<bool,Controllable*> Controllable::StartLearning;
sigc::signal<void,Controllable*> Controllable::StopLearning;

Controllable::Controllable (std::string name)
	: _name (name)
{
}

XMLNode&
Controllable::get_state ()
{
	XMLNode* node = new XMLNode (X_("controllable"));
	char buf[64];

	node->add_property (X_("name"), _name); // not reloaded from XML state, just there to look at
	_id.print (buf, sizeof (buf));
	node->add_property (X_("id"), buf);
	return *node;
}

int
Controllable::set_state (const XMLNode& node)
{
	const XMLProperty* prop = node.property (X_("id"));

	if (prop) {
		_id = prop->value();
		return 0;
	} else {
		error << _("Controllable state node has no ID property") << endmsg;
		return -1;
	}
}
