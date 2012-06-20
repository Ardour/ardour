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
#include "ardour/automation_list.h"
#include "ardour/event_type_map.h"
#include "evoral/Curve.hpp"
#include "pbd/stacktrace.h"
#include "pbd/enumwriter.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PBD::Signal1<void,AutomationList *> AutomationList::AutomationListCreated;

#if 0
static void dumpit (const AutomationList& al, string prefix = "")
{
	cerr << prefix << &al << endl;
	for (AutomationList::const_iterator i = al.begin(); i != al.end(); ++i) {
		cerr << prefix << '\t' << (*i)->when << ',' << (*i)->value << endl;
	}
	cerr << "\n";
}
#endif
AutomationList::AutomationList (Evoral::Parameter id)
	: ControlList(id)
{
	_state = Off;
	_style = Absolute;
	g_atomic_int_set (&_touching, 0);

	create_curve_if_necessary();

	assert(_parameter.type() != NullAutomation);
	AutomationListCreated(this);
}

AutomationList::AutomationList (const AutomationList& other)
	: StatefulDestructible()
	, ControlList(other)
{
	_style = other._style;
	_state = other._state;
	g_atomic_int_set (&_touching, other.touching());

	create_curve_if_necessary();

	assert(_parameter.type() != NullAutomation);
	AutomationListCreated(this);
}

AutomationList::AutomationList (const AutomationList& other, double start, double end)
	: ControlList(other, start, end)
{
	_style = other._style;
	_state = other._state;
	g_atomic_int_set (&_touching, other.touching());

	create_curve_if_necessary();

	assert(_parameter.type() != NullAutomation);
	AutomationListCreated(this);
}

/** @param id is used for legacy sessions where the type is not present
 * in or below the AutomationList node.  It is used if @param id is non-null.
 */
AutomationList::AutomationList (const XMLNode& node, Evoral::Parameter id)
	: ControlList(id)
{
	g_atomic_int_set (&_touching, 0);
	_state = Off;
	_style = Absolute;

	set_state (node, Stateful::loading_state_version);

	if (id) {
		_parameter = id;
	}

	create_curve_if_necessary();

	assert(_parameter.type() != NullAutomation);
	AutomationListCreated(this);
}

AutomationList::~AutomationList()
{
}

boost::shared_ptr<Evoral::ControlList>
AutomationList::create(Evoral::Parameter id)
{
	return boost::shared_ptr<Evoral::ControlList>(new AutomationList(id));
}

void
AutomationList::create_curve_if_necessary()
{
	switch (_parameter.type()) {
	case GainAutomation:
	case PanAzimuthAutomation:
	case PanElevationAutomation:
	case PanWidthAutomation:
	case FadeInAutomation:
	case FadeOutAutomation:
	case EnvelopeAutomation:
		create_curve();
		break;
	default:
		break;
	}
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
			_events.push_back (new Evoral::ControlEvent (**i));
		}

		_min_yval = other._min_yval;
		_max_yval = other._max_yval;
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

	if (!ControlList::frozen()) {
		StateChanged (); /* EMIT SIGNAL */
	}
}

void
AutomationList::set_automation_state (AutoState s)
{
	if (s != _state) {
		_state = s;

                if (_state == Write) {
                        Glib::Mutex::Lock lm (ControlList::_lock);
                        nascent.push_back (new NascentInfo ());
                }
		automation_state_changed (s); /* EMIT SIGNAL */
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
AutomationList::start_touch (double when)
{
        if (_state == Touch) {
                Glib::Mutex::Lock lm (ControlList::_lock);
                nascent.push_back (new NascentInfo (when));
        }

	g_atomic_int_set (&_touching, 1);
}

void
AutomationList::stop_touch (bool mark, double when)
{
	if (g_atomic_int_get (&_touching) == 0) {
		/* this touch has already been stopped (probably by Automatable::transport_stopped),
		   so we've nothing to do.
		*/
		return;
	}

	g_atomic_int_set (&_touching, 0);

        if (_state == Touch) {

		assert (!nascent.empty ());

                Glib::Mutex::Lock lm (ControlList::_lock);

                if (mark) {

			nascent.back()->end_time = when;

                } else {

                        /* nascent info created in start touch but never used. just get rid of it.
                         */

                        NascentInfo* ninfo = nascent.back ();
                        nascent.erase (nascent.begin());
                        delete ninfo;
                }
        }
}

void
AutomationList::thaw ()
{
	ControlList::thaw();

	if (_changed_when_thawed) {
		_changed_when_thawed = false;
		StateChanged(); /* EMIT SIGNAL */
	}
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

	root->add_property ("automation-id", EventTypeMap::instance().to_symbol(_parameter));

	root->add_property ("id", id().to_s());

	snprintf (buf, sizeof (buf), "%.12g", _default_value);
	root->add_property ("default", buf);
	snprintf (buf, sizeof (buf), "%.12g", _min_yval);
	root->add_property ("min-yval", buf);
	snprintf (buf, sizeof (buf), "%.12g", _max_yval);
	root->add_property ("max-yval", buf);

	root->add_property ("interpolation-style", enum_2_string (_interpolation));

	if (full) {
                /* never serialize state with Write enabled - too dangerous
                   for the user's data
                */
                if (_state != Write) {
                        root->add_property ("state", auto_state_to_string (_state));
                } else {
                        root->add_property ("state", auto_state_to_string (Off));
                }
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

	str.precision(15);  //10 digits is enough digits for 24 hours at 96kHz

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

        ControlList::freeze ();
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

	thin ();

	if (!ok) {
		clear ();
		error << _("automation list: cannot load coordinates from XML, all points ignored") << endmsg;
	} else {
		mark_dirty ();
		maybe_signal_changed ();
	}

        thaw ();

	return 0;
}

int
AutomationList::set_state (const XMLNode& node, int version)
{
	LocaleGuard lg (X_("POSIX"));
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
			return set_state (*nsos, version);
		}

		/* old school */

		const XMLNodeList& elist = node.children();
		XMLNodeConstIterator i;
		XMLProperty* prop;
		pframes_t x;
		double y;

                ControlList::freeze ();
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

		thin ();

                thaw ();

		return 0;
	}

	if (node.name() != X_("AutomationList") ) {
		error << string_compose (_("AutomationList: passed XML node called %1, not \"AutomationList\" - ignored"), node.name()) << endmsg;
		return -1;
	}

	if (set_id (node)) {
		/* update session AL list */
		AutomationListCreated(this);
	}

	if ((prop = node.property (X_("automation-id"))) != 0){
		_parameter = EventTypeMap::instance().new_parameter(prop->value());
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
                if (_state == Write) {
                        _state = Off;
                }
	} else {
		_state = Off;
	}

	if ((prop = node.property (X_("min-yval"))) != 0) {
		_min_yval = atof (prop->value ().c_str());
	} else {
		_min_yval = FLT_MIN;
	}

	if ((prop = node.property (X_("max-yval"))) != 0) {
		_max_yval = atof (prop->value ().c_str());
	} else {
		_max_yval = FLT_MAX;
	}

	bool have_events = false;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == X_("events")) {
			deserialize_events (*(*niter));
			have_events = true;
		}
	}

	if (!have_events) {
		/* there was no Events child node; clear any current events */
		freeze ();
		clear ();
		mark_dirty ();
		maybe_signal_changed ();
		thaw ();
	}

	return 0;
}

bool
AutomationList::operator!= (AutomationList const & other) const
{
	return (
		static_cast<ControlList const &> (*this) != static_cast<ControlList const &> (other) ||
		_state != other._state ||
		_style != other._style ||
		_touching != other._touching
		);
}

PBD::PropertyBase *
AutomationListProperty::clone () const
{
	return new AutomationListProperty (
		this->property_id(),
		boost::shared_ptr<AutomationList> (new AutomationList (*this->_old.get())),
		boost::shared_ptr<AutomationList> (new AutomationList (*this->_current.get()))
		);
}
	
