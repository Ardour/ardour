#include <pbd/controllable.h>
#include <pbd/xml++.h>

#include "i18n.h"

using namespace PBD;

sigc::signal<void,Controllable*> Controllable::Created;
sigc::signal<void,Controllable*> Controllable::GoingAway;
sigc::signal<bool,Controllable*> Controllable::StartLearning;
sigc::signal<void,Controllable*> Controllable::StopLearning;

Controllable::Controllable ()
{
	Created (this);
}

XMLNode&
Controllable::get_state ()
{
	XMLNode* node = new XMLNode (X_("Controllable"));
	char buf[64];
	_id.print (buf);
	node->add_property (X_("id"), buf);
	return *node;
}
