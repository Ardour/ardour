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
#include "ardour/parameter_descriptor.h"
#include "ardour/evoral_types_convert.h"
#include "ardour/types_convert.h"
#include "evoral/Curve.hpp"
#include "pbd/memento_command.h"
#include "pbd/stacktrace.h"
#include "pbd/enumwriter.h"
#include "pbd/types_convert.h"

#include "pbd/i18n.h"

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
AutomationList::AutomationList (const Evoral::Parameter& id, const Evoral::ParameterDescriptor& desc)
	: ControlList(id, desc)
	, _before (0)
{
	_state = Off;
	_style = Absolute;
	g_atomic_int_set (&_touching, 0);

	create_curve_if_necessary();

	assert(_parameter.type() != NullAutomation);
	AutomationListCreated(this);
}

AutomationList::AutomationList (const Evoral::Parameter& id)
	: ControlList(id, ARDOUR::ParameterDescriptor(id))
	, _before (0)
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
	, _before (0)
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
	, _before (0)
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
	: ControlList(id, ARDOUR::ParameterDescriptor(id))
	, _before (0)
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
	delete _before;
}

boost::shared_ptr<Evoral::ControlList>
AutomationList::create(const Evoral::Parameter&           id,
                       const Evoral::ParameterDescriptor& desc)
{
	return boost::shared_ptr<Evoral::ControlList>(new AutomationList(id, desc));
}

void
AutomationList::create_curve_if_necessary()
{
	switch (_parameter.type()) {
	case GainAutomation:
	case TrimAutomation:
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

AutomationList&
AutomationList::operator= (const AutomationList& other)
{
	if (this != &other) {


		ControlList::operator= (other);
		_state = other._state;
		_style = other._style;
		_touching = other._touching;

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
		delete _before;
		if (s == Write && _desc.toggled) {
			_before = &get_state ();
		} else {
			_before = 0;
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
AutomationList::start_write_pass (double when)
{
	delete _before;
	if (in_new_write_pass ()) {
		_before = &get_state ();
	} else {
		_before = 0;
	}
	ControlList::start_write_pass (when);
}

void
AutomationList::write_pass_finished (double when, double thinning_factor)
{
	ControlList::write_pass_finished (when, thinning_factor);
}

void
AutomationList::start_touch (double when)
{
        if (_state == Touch) {
		start_write_pass (when);
        }

	g_atomic_int_set (&_touching, 1);
}

void
AutomationList::stop_touch (bool mark, double)
{
	if (g_atomic_int_get (&_touching) == 0) {
		/* this touch has already been stopped (probably by Automatable::transport_stopped),
		   so we've nothing to do.
		*/
		return;
	}

	g_atomic_int_set (&_touching, 0);

        if (_state == Touch) {

                if (mark) {

			/* XXX need to mark the last added point with the
			 * current time
			 */
                }
        }
}

/* _before may be owned by the undo stack,
 * so we have to be careful about doing this.
 *
 * ::before () transfers ownership, setting _before to 0
 */
void
AutomationList::clear_history ()
{
	delete _before;
	_before = 0;
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

Command*
AutomationList::memento_command (XMLNode* before, XMLNode* after)
{
	return new MementoCommand<AutomationList> (*this, before, after);
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
	LocaleGuard lg;

	root->set_property ("automation-id", EventTypeMap::instance().to_symbol(_parameter));
	root->set_property ("id", id());
	root->set_property ("default", _default_value);
	root->set_property ("min-yval", _min_yval);
	root->set_property ("max-yval", _max_yval);
	root->set_property ("interpolation-style", _interpolation);

	if (full) {
		/* never serialize state with Write enabled - too dangerous
		   for the user's data
		*/
		if (_state != Write) {
			root->set_property ("state", _state);
		} else {
			if (_events.empty ()) {
				root->set_property ("state", Off);
			} else {
				root->set_property ("state", Touch);
			}
		}
	} else {
		/* never save anything but Off for automation state to a template */
		root->set_property ("state", Off);
	}

	root->set_property ("style", _style);

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
		str << PBD::to_string ((*xx)->when);
		str << ' ';
		str << PBD::to_string ((*xx)->value);
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

	std::string x_str;
	std::string y_str;
	double x;
	double y;
	bool ok = true;

	while (str) {
		str >> x_str;
		if (!str || !PBD::string_to<double> (x_str, x)) {
			break;
		}
		str >> y_str;
		if (!str || !PBD::string_to<double> (y_str, y)) {
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
		maybe_signal_changed ();
	}

        thaw ();

	return 0;
}

int
AutomationList::set_state (const XMLNode& node, int version)
{
	LocaleGuard lg;
	XMLNodeList nlist = node.children();
	XMLNode* nsos;
	XMLNodeIterator niter;

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

		ControlList::freeze ();
		clear ();

		for (i = elist.begin(); i != elist.end(); ++i) {

			pframes_t x;
			if (!(*i)->get_property ("x", x)) {
				error << _("automation list: no x-coordinate stored for control point (point ignored)") << endmsg;
				continue;
			}

			double y;
			if (!(*i)->get_property ("y", y)) {
				error << _("automation list: no y-coordinate stored for control point (point ignored)") << endmsg;
				continue;
			}

			fast_simple_add (x, y);
		}

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

	std::string value;
	if (node.get_property (X_("automation-id"), value)) {
		_parameter = EventTypeMap::instance().from_symbol(value);
	} else {
		warning << "Legacy session: automation list has no automation-id property." << endmsg;
	}

	if (!node.get_property (X_("interpolation-style"), _interpolation)) {
		_interpolation = Linear;
	}

	if (!node.get_property (X_("default"), _default_value)) {
		_default_value = 0.0;
	}

	if (!node.get_property (X_("style"), _style)) {
		_style = Absolute;
	}

	if (node.get_property (X_("state"), _state)) {
		if (_state == Write) {
			_state = Off;
		}
		automation_state_changed (_state);
	} else {
		_state = Off;
	}

	if (!node.get_property (X_("min-yval"), _min_yval)) {
		_min_yval = FLT_MIN;
	}

	if (!node.get_property (X_("max-yval"), _max_yval)) {
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

