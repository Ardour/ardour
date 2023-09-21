/*
 * Copyright (C) 2008-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <set>
#include <climits>
#include <float.h>
#include <cmath>
#include <sstream>
#include <algorithm>

#include "temporal/types_convert.h"

#include "ardour/automation_list.h"
#include "ardour/event_type_map.h"
#include "ardour/parameter_descriptor.h"
#include "ardour/parameter_types.h"
#include "ardour/evoral_types_convert.h"
#include "ardour/types_convert.h"

#include "evoral/Curve.h"

#include "pbd/memento_command.h"
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
AutomationList::AutomationList (const Evoral::Parameter& id, const Evoral::ParameterDescriptor& desc, Temporal::TimeDomainProvider const & tdp)
	: ControlList(id, desc, tdp)
	, _before (0)
{
	_state = Off;
	_touching.store (0);
	_interpolation = default_interpolation ();

	create_curve_if_necessary();

	assert(_parameter.type() != NullAutomation);
	AutomationListCreated(this);
}

AutomationList::AutomationList (const Evoral::Parameter& id, Temporal::TimeDomainProvider const & tdp)
	: ControlList(id, ARDOUR::ParameterDescriptor(id), tdp)
	, _before (0)
{
	_state = Off;
	_touching.store (0);
	_interpolation = default_interpolation ();

	create_curve_if_necessary();

	assert(_parameter.type() != NullAutomation);
	AutomationListCreated(this);
}

AutomationList::AutomationList (const AutomationList& other)
	: ControlList(other)
	, StatefulDestructible()
	, _before (0)
{
	_state = other._state;
	_touching.store (other.touching());

	create_curve_if_necessary();

	assert(_parameter.type() != NullAutomation);
	AutomationListCreated(this);
}

AutomationList::AutomationList (const AutomationList& other, timepos_t const & start, timepos_t const & end)
	: ControlList(other, start, end)
	, _before (0)
{
	_state = other._state;
	_touching.store (other.touching());

	create_curve_if_necessary();

	assert(_parameter.type() != NullAutomation);
	AutomationListCreated(this);
}

/** @param id is used for legacy sessions where the type is not present
 * in or below the AutomationList node.  It is used if @p id is non-null.
 */
AutomationList::AutomationList (const XMLNode& node, Evoral::Parameter id)
	: ControlList(id, ARDOUR::ParameterDescriptor(id), Temporal::TimeDomainProvider (Temporal::AudioTime)) /* domain may change in ::set_state */
	, _before (0)
{
	_touching.store (0);
	_interpolation = default_interpolation ();
	_state = Off;

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

std::shared_ptr<Evoral::ControlList>
AutomationList::create(const Evoral::Parameter&           id,
                       const Evoral::ParameterDescriptor& desc,
                       Temporal::TimeDomainProvider const & tdp)
{
	return std::shared_ptr<Evoral::ControlList>(new AutomationList(id, desc, tdp));
}

void
AutomationList::create_curve_if_necessary()
{
	switch (_parameter.type()) {
	case GainAutomation:
	case BusSendLevel:
	case InsertReturnLevel:
	case TrimAutomation:
	case PanAzimuthAutomation:
	case PanElevationAutomation:
	case PanWidthAutomation:
	case FadeInAutomation:
	case FadeOutAutomation:
	case EnvelopeAutomation:
	case MidiVelocityAutomation:
		create_curve();
		break;
	default:
		break;
	}

	WritePassStarted.connect_same_thread (_writepass_connection, boost::bind (&AutomationList::snapshot_history, this, false));
}

AutomationList&
AutomationList::operator= (const AutomationList& other)
{
	if (this != &other) {
		ControlList::freeze ();
		/* ControlList::operator= calls copy_events() which calls
		 * mark_dirty() and maybe_signal_changed()
		 */
		ControlList::operator= (other);
		_state = other._state;
		_touching.store (other._touching);
		ControlList::thaw ();
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

AutoState
AutomationList::automation_state() const
{
	Glib::Threads::RWLock::ReaderLock lm (Evoral::ControlList::_lock);
	return _state;
}

void
AutomationList::set_automation_state (AutoState s)
{
	{
		Glib::Threads::RWLock::ReaderLock lm (Evoral::ControlList::_lock);

		if (s == _state) {
			return;
		}
		_state = s;
		if (s == Write && _desc.toggled) {
			snapshot_history (true);
		}
	}

	automation_state_changed (s); /* EMIT SIGNAL */
}

Evoral::ControlList::InterpolationStyle
AutomationList::default_interpolation () const
{
	switch (_parameter.type()) {
		case GainAutomation:
		case BusSendLevel:
		case InsertReturnLevel:
		case EnvelopeAutomation:
			return ControlList::Exponential;
			break;
		case MainOutVolume:
		case TrimAutomation:
			return ControlList::Logarithmic;
			break;
		default:
			break;
	}
	/* based on Evoral::ParameterDescriptor log,toggle,.. */
	return ControlList::default_interpolation ();
}

void
AutomationList::start_write_pass (timepos_t const & when)
{
	snapshot_history (true);
	ControlList::start_write_pass (when);
}

void
AutomationList::write_pass_finished (timepos_t const & when, double thinning_factor)
{
	ControlList::write_pass_finished (when, thinning_factor);
}

void
AutomationList::start_touch (timepos_t const & when)
{
	_touching.store (1);
}

void
AutomationList::stop_touch (timepos_t const & /* not used */)
{
	if (_touching.load () == 0) {
		/* this touch has already been stopped (probably by Automatable::transport_stopped),
		   so we've nothing to do.
		*/
		return;
	}

	_touching.store (0);
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
AutomationList::snapshot_history (bool need_lock)
{
	if (!in_new_write_pass ()) {
		return;
	}
	delete _before;
	_before = &state (true, need_lock);
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

PBD::Command*
AutomationList::memento_command (XMLNode* before, XMLNode* after)
{
	return new MementoCommand<AutomationList> (*this, before, after);
}

XMLNode&
AutomationList::get_state () const
{
	return state (true, true);
}

XMLNode&
AutomationList::state (bool save_auto_state, bool need_lock) const
{
	XMLNode* root = new XMLNode (X_("AutomationList"));

	root->set_property ("automation-id", EventTypeMap::instance().to_symbol(_parameter));
	root->set_property ("id", id());
	root->set_property ("interpolation-style", _interpolation);
	root->set_property ("time-domain", enum_2_string (time_domain()));

	if (save_auto_state) {
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

	if (!_events.empty()) {
		root->add_child_nocopy (serialize_events (need_lock));
	}

	return *root;
}

XMLNode&
AutomationList::serialize_events (bool need_lock) const
{
	XMLNode* node = new XMLNode (X_("events"));
	stringstream str;

	Glib::Threads::RWLock::ReaderLock lm (Evoral::ControlList::_lock, Glib::Threads::NOT_LOCK);
	if (need_lock) {
		lm.acquire ();
	}
	for (const_iterator xx = _events.begin(); xx != _events.end(); ++xx) {
		str << PBD::to_string ((*xx)->when);
		str << ' ';
		str << PBD::to_string ((*xx)->value);
		str << '\n';
	}

	/* XML is a bit weird */

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
	timepos_t x;
	double y;
	bool ok = true;

	while (str) {
		str >> x_str;
		if (!str || !PBD::string_to<timepos_t> (x_str, x)) {
			break;
		}
		str >> y_str;
		if (!str || !PBD::string_to<double> (y_str, y)) {
			ok = false;
			break;
		}
		y = std::min ((double)_desc.upper, std::max ((double)_desc.lower, y));
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
	XMLNodeList nlist = node.children();
	XMLNode* nsos;
	XMLNodeIterator niter;
	Temporal::TimeDomain time_domain;

	if (node.get_property ("time-domain", time_domain)) {
		set_time_domain (time_domain);
	}

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

			y = std::min ((double)_desc.upper, std::max ((double)_desc.lower, y));
			fast_simple_add (timepos_t (x), y);
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
		_interpolation = default_interpolation ();
	}

	if (node.get_property (X_("state"), _state)) {
		if (_state == Write) {
			_state = Off;
		}
		automation_state_changed (_state);
	} else {
		_state = Off;
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
		_touching != other._touching
		);
}

PBD::PropertyBase *
AutomationListProperty::clone () const
{
	return new AutomationListProperty (
		this->property_id(),
		std::shared_ptr<AutomationList> (new AutomationList (*this->_old.get())),
		std::shared_ptr<AutomationList> (new AutomationList (*this->_current.get()))
		);
}
