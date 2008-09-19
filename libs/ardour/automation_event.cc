/*
    Copyright (C) 2002 Paul Davis 

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

#include <set>
#include <climits>
#include <float.h>
#include <cmath>
#include <sstream>
#include <algorithm>
#include <sigc++/bind.h>
#include <ardour/parameter.h>
#include <ardour/automation_event.h>
#include <ardour/curve.h>
#include <pbd/stacktrace.h>
#include <pbd/enumwriter.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace sigc;
using namespace PBD;

sigc::signal<void,AutomationList *> AutomationList::AutomationListCreated;

#if 0
static void dumpit (const AutomationList& al, string prefix = "")
{
	cerr << prefix << &al << endl;
	for (AutomationList::const_iterator i = al.const_begin(); i != al.const_end(); ++i) {
		cerr << prefix << '\t' << (*i)->when << ',' << (*i)->value << endl;
	}
	cerr << "\n";
}
#endif

/* XXX: min_val max_val redundant? (param.min() param.max()) */
AutomationList::AutomationList (Parameter id)
	: ControlList(id)
{
	_state = Off;
	_style = Absolute;
	_touching = false;

	assert(_parameter.type() != NullAutomation);
	AutomationListCreated(this);
}

AutomationList::AutomationList (const AutomationList& other)
	: ControlList(other)
{
	_state = other._state;
	_touching = other._touching;
	
	assert(_parameter.type() != NullAutomation);
	AutomationListCreated(this);
}

AutomationList::AutomationList (const AutomationList& other, double start, double end)
	: ControlList(other)
{
	_style = other._style;
	_state = other._state;
	_touching = other._touching;

	assert(_parameter.type() != NullAutomation);
	AutomationListCreated(this);
}

/** \a id is used for legacy sessions where the type is not present
 * in or below the <AutomationList> node.  It is used if \a id is non-null.
 */
AutomationList::AutomationList (const XMLNode& node, Parameter id)
	: ControlList(id)
{
	_touching = false;
	_state = Off;
	_style = Absolute;
	
	set_state (node);

	if (id)
		_parameter = id;

	assert(_parameter.type() != NullAutomation);
	AutomationListCreated(this);
}

AutomationList::~AutomationList()
{
	GoingAway ();
}

boost::shared_ptr<Evoral::ControlList>
AutomationList::create(Evoral::Parameter id)
{
	return boost::shared_ptr<Evoral::ControlList>(new AutomationList(id));
}

bool
AutomationList::operator== (const AutomationList& other)
{
	return _events == other._events;
}

AutomationList&
AutomationList::operator= (const AutomationList& other)
{
	if (this != &other) {
		
		_events.clear ();
		
		for (const_iterator i = other._events.begin(); i != other._events.end(); ++i) {
			_events.push_back (new ControlEvent (**i));
		}
		
		_min_yval = other._min_yval;
		_max_yval = other._max_yval;
		_max_xval = other._max_xval;
		_default_value = other._default_value;
		
		mark_dirty ();
		maybe_signal_changed ();
	}

	return *this;
}

void
AutomationList::maybe_signal_changed ()
{
	ControlList::maybe_signal_changed ();

	if (!_frozen) {
		StateChanged (); /* EMIT SIGNAL */
	}
}

void
AutomationList::set_automation_state (AutoState s)
{
	if (s != _state) {
		_state = s;
		automation_state_changed (); /* EMIT SIGNAL */
	}
}

void
AutomationList::set_automation_style (AutoStyle s)
{
	if (s != _style) {
		_style = s;
		automation_style_changed (); /* EMIT SIGNAL */
	}
}

void
AutomationList::start_touch ()
{
	_touching = true;
	_new_value = true;
}

void
AutomationList::stop_touch ()
{
	_touching = false;
	_new_value = false;
}

void
AutomationList::freeze ()
{
	_frozen++;
}

void
AutomationList::thaw ()
{
	ControlList::thaw();

	if (_changed_when_thawed) {
		StateChanged(); /* EMIT SIGNAL */
	}
}

void 
AutomationList::mark_dirty () const
{
	ControlList::mark_dirty ();
	Dirty (); /* EMIT SIGNAL */
}

XMLNode&
AutomationList::get_state ()
{
	return state (true);
}

XMLNode&
AutomationList::state (bool full)
{
	XMLNode* root = new XMLNode (X_("AutomationList"));
	char buf[64];
	LocaleGuard lg (X_("POSIX"));

	root->add_property ("automation-id", _parameter.symbol());

	root->add_property ("id", _id.to_s());

	snprintf (buf, sizeof (buf), "%.12g", _default_value);
	root->add_property ("default", buf);
	snprintf (buf, sizeof (buf), "%.12g", _min_yval);
	root->add_property ("min_yval", buf);
	snprintf (buf, sizeof (buf), "%.12g", _max_yval);
	root->add_property ("max_yval", buf);
	snprintf (buf, sizeof (buf), "%.12g", _max_xval);
	root->add_property ("max_xval", buf);
	
	root->add_property ("interpolation-style", enum_2_string (_interpolation));

	if (full) {
		root->add_property ("state", auto_state_to_string (_state));
	} else {
		/* never save anything but Off for automation state to a template */
		root->add_property ("state", auto_state_to_string (Off));
	}

	root->add_property ("style", auto_style_to_string (_style));

	if (!_events.empty()) {
		root->add_child_nocopy (serialize_events());
	}

	return *root;
}

XMLNode&
AutomationList::serialize_events ()
{
	XMLNode* node = new XMLNode (X_("events"));
	stringstream str;

	for (iterator xx = _events.begin(); xx != _events.end(); ++xx) {
		str << (double) (*xx)->when;
		str << ' ';
		str <<(double) (*xx)->value;
		str << '\n';
	}

	/* XML is a bit wierd */

	XMLNode* content_node = new XMLNode (X_("foo")); /* it gets renamed by libxml when we set content */
	content_node->set_content (str.str());

	node->add_child_nocopy (*content_node);

	return *node;
}

int
AutomationList::deserialize_events (const XMLNode& node)
{
	if (node.children().empty()) {
		return -1;
	}

	XMLNode* content_node = node.children().front();

	if (content_node->content().empty()) {
		return -1;
	}

	freeze ();
	clear ();
	
	stringstream str (content_node->content());
	
	double x;
	double y;
	bool ok = true;
	
	while (str) {
		str >> x;
		if (!str) {
			break;
		}
		str >> y;
		if (!str) {
			ok = false;
			break;
		}
		fast_simple_add (x, y);
	}
	
	if (!ok) {
		clear ();
		error << _("automation list: cannot load coordinates from XML, all points ignored") << endmsg;
	} else {
		mark_dirty ();
		reposition_for_rt_add (0);
		maybe_signal_changed ();
	}

	thaw ();

	return 0;
}

int
AutomationList::set_state (const XMLNode& node)
{
	XMLNodeList nlist = node.children();
	XMLNode* nsos;
	XMLNodeIterator niter;
	const XMLProperty* prop;

	if (node.name() == X_("events")) {
		/* partial state setting*/
		return deserialize_events (node);
	}
	
	if (node.name() == X_("Envelope") || node.name() == X_("FadeOut") || node.name() == X_("FadeIn")) {

		if ((nsos = node.child (X_("AutomationList")))) {
			/* new school in old school clothing */
			return set_state (*nsos);
		}

		/* old school */

		const XMLNodeList& elist = node.children();
		XMLNodeConstIterator i;
		XMLProperty* prop;
		nframes_t x;
		double y;
		
		freeze ();
		clear ();
		
		for (i = elist.begin(); i != elist.end(); ++i) {
			
			if ((prop = (*i)->property ("x")) == 0) {
				error << _("automation list: no x-coordinate stored for control point (point ignored)") << endmsg;
				continue;
			}
			x = atoi (prop->value().c_str());
			
			if ((prop = (*i)->property ("y")) == 0) {
				error << _("automation list: no y-coordinate stored for control point (point ignored)") << endmsg;
				continue;
			}
			y = atof (prop->value().c_str());
			
			fast_simple_add (x, y);
		}
		
		thaw ();

		return 0;
	}

	if (node.name() != X_("AutomationList") ) {
		error << string_compose (_("AutomationList: passed XML node called %1, not \"AutomationList\" - ignored"), node.name()) << endmsg;
		return -1;
	}

	if ((prop = node.property ("id")) != 0) {
		_id = prop->value ();
		/* update session AL list */
		AutomationListCreated(this);
	}
	
	if ((prop = node.property (X_("automation-id"))) != 0){ 
		_parameter = Parameter(prop->value());
	} else {
		warning << "Legacy session: automation list has no automation-id property.";
	}
	
	if ((prop = node.property (X_("interpolation-style"))) != 0) {
		_interpolation = (InterpolationStyle)string_2_enum(prop->value(), _interpolation);
	} else {
		_interpolation = Linear;
	}
	
	if ((prop = node.property (X_("default"))) != 0){ 
		_default_value = atof (prop->value().c_str());
	} else {
		_default_value = 0.0;
	}

	if ((prop = node.property (X_("style"))) != 0) {
		_style = string_to_auto_style (prop->value());
	} else {
		_style = Absolute;
	}

	if ((prop = node.property (X_("state"))) != 0) {
		_state = string_to_auto_state (prop->value());
	} else {
		_state = Off;
	}

	if ((prop = node.property (X_("min_yval"))) != 0) {
		_min_yval = atof (prop->value ().c_str());
	} else {
		_min_yval = FLT_MIN;
	}

	if ((prop = node.property (X_("max_yval"))) != 0) {
		_max_yval = atof (prop->value ().c_str());
	} else {
		_max_yval = FLT_MAX;
	}

	if ((prop = node.property (X_("max_xval"))) != 0) {
		_max_xval = atof (prop->value ().c_str());
	} else {
		_max_xval = 0; // means "no limit ;
	}

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == X_("events")) {
			deserialize_events (*(*niter));
		}
	}

	return 0;
}

