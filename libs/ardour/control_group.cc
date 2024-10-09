/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include <vector>

#include "pbd/unwind.h"

#include "ardour/control_group.h"
#include "ardour/gain_control.h"
#include "ardour/selection.h"
#include "ardour/stripable.h"

using namespace ARDOUR;
using namespace PBD;

ControlGroup::ControlGroup (Evoral::Parameter p)
	: _parameter (p)
	, _active (true)
	, _mode (Mode (0))
	, propagating (false)
{
}


ControlGroup::~ControlGroup ()
{
	clear ();
}

void
ControlGroup::set_active (bool yn)
{
	_active = yn;
}

void
ControlGroup::set_mode (Mode m)
{
	_mode = m;
}

void
ControlGroup::clear (bool pop)
{
	/* we're giving up on all members, so we don't care about their
	 * DropReferences signals anymore
	 */

	member_connections.drop_connections ();

	/* make a copy so that when the control calls ::remove_control(), we
	 * don't deadlock.
	 */

	std::vector<std::shared_ptr<AutomationControl> > controls;
	{
		Glib::Threads::RWLock::WriterLock lm (controls_lock);
		for (ControlMap::const_iterator i = _controls.begin(); i != _controls.end(); ++i) {
			controls.push_back (i->second);
		}
	}

	_controls.clear ();

	for (auto & c : controls) {
		if (pop) {
			c->pop_group ();
		} else {
			c->set_group (std::shared_ptr<ControlGroup>());
		}
	}
}

AutomationControlList
ControlGroup::controls () const
{
	AutomationControlList c;

	if (_active) {
		Glib::Threads::RWLock::WriterLock lm (controls_lock);
		for (ControlMap::const_iterator i = _controls.begin(); i != _controls.end(); ++i) {
			c.push_back (i->second);
		}
	}

	return c;
}

void
ControlGroup::control_going_away (std::weak_ptr<AutomationControl> wac)
{
	std::shared_ptr<AutomationControl> ac (wac.lock());
	if (!ac) {
		return;
	}

	remove_control (ac);
}

int
ControlGroup::remove_control (std::shared_ptr<AutomationControl> ac, bool pop)
{
	int erased;

	{
		Glib::Threads::RWLock::WriterLock lm (controls_lock);
		erased = _controls.erase (ac->id());
	}

	if (erased) {
		if (pop) {
			ac->pop_group ();
		} else {
			ac->set_group (std::shared_ptr<ControlGroup>());
		}
	}

	/* return zero if erased, non-zero otherwise */
	return !erased;
}

int
ControlGroup::add_control (std::shared_ptr<AutomationControl> ac, bool push)
{
	if (ac->parameter() != _parameter) {
		if (_parameter.type () != PluginAutomation) {
			return -1;
		}
		/* allow plugin-automation - first control sets Evoral::parameter */
		Glib::Threads::RWLock::ReaderLock lm (controls_lock);
		if (!_controls.empty () && _controls.begin()->second->parameter() != ac->parameter()) {
			return -1;
		}
	}

	std::pair<ControlMap::iterator,bool> res;

	{
		Glib::Threads::RWLock::WriterLock lm (controls_lock);
		res = _controls.insert (std::make_pair (ac->id(), ac));
	}

	if (!res.second) {
		/* already in ControlMap */
		return -1;
	}

	/* Inserted */


	if (push) {
		ac->push_group (shared_from_this());
	} else {
		ac->set_group (shared_from_this());
	}

	ac->DropReferences.connect_same_thread (member_connections, boost::bind (&ControlGroup::control_going_away, this, std::weak_ptr<AutomationControl>(ac)));

	return 0;
}

void
ControlGroup::pre_realtime_queue_stuff (double val)
{
	Glib::Threads::RWLock::ReaderLock lm (controls_lock);

	for (ControlMap::iterator c = _controls.begin(); c != _controls.end(); ++c) {
		c->second->do_pre_realtime_queue_stuff (val);
	}
}

void
ControlGroup::set_group_value (std::shared_ptr<AutomationControl> control, double val)
{
	double old = control->get_value ();

	/* set the primary control */

	control->set_value (val, Controllable::ForGroup);

	/* now propagate across the group */

	Glib::Threads::RWLock::ReaderLock lm (controls_lock);

	if (_mode & Relative) {

		const double factor = old / control->get_value ();

		for (ControlMap::iterator c = _controls.begin(); c != _controls.end(); ++c) {
			if (c->second != control) {
				c->second->set_value (factor * c->second->get_value(), Controllable::ForGroup);
			}
		}

	} else {

		for (ControlMap::iterator c = _controls.begin(); c != _controls.end(); ++c) {
			if (c->second != control) {
				c->second->set_value (val, Controllable::ForGroup);
			}
		}
	}
}

void
ControlGroup::fill_from_stripable_list (StripableList& sl, Evoral::Parameter const & p)
{
	/* Very unfortunate that gain control is special cased. Routes do not
	 * call ::add_control() for their gain control, but instead pass it to
	 * their Amp processor which takes a certain kind of ownership of it.
	 */

	switch (p.type()) {
	case GainAutomation:
		for (auto & s : sl) {
			std::shared_ptr<AutomationControl> ac = s->gain_control ();
			if (ac) {
				add_control (ac, true);
			}
		}
		break;
	case TrimAutomation:
		for (auto & s : sl) {
			std::shared_ptr<AutomationControl> ac = s->trim_control ();
			if (ac) {
				add_control (ac, true);
			}
		}
		break;
	default:
		for (auto & s : sl) {
			std::shared_ptr<AutomationControl> ac = s->automation_control (p, false);
			if (ac) {
				add_control (ac, true);
			}
		}
	}
}

void
ControlGroup::pop_all ()
{
	clear (true);
}

/*---- GAIN CONTROL GROUP -----------*/

GainControlGroup::GainControlGroup (ARDOUR::AutomationType t)
	: ControlGroup (t)
{
}

gain_t
GainControlGroup::get_min_factor (gain_t factor)
{
	/* CALLER MUST HOLD READER LOCK */

	const gain_t min_factor = _controls.begin()->second->desc().from_interface (0.0);

	for (auto const & c : _controls) {
		gain_t const g = c.second->get_value();

		if ((g + g * factor) >= min_factor) {
			continue;
		}

		if (g <= 0.0000003f) {
			return 0.0f;
		}

		factor = 0.0000003f / g - 1.0f;
	}

	return factor;
}

gain_t
GainControlGroup::get_max_factor (gain_t factor)
{
	/* CALLER MUST HOLD READER LOCK */

	const gain_t max_factor = _controls.begin()->second->desc().from_interface (1.0);

	for (auto const & c : _controls) {
		gain_t const g = c.second->get_value();

		// if the current factor woulnd't raise this route above maximum
		if ((g + g * factor) <= max_factor) {
			continue;
		}

		// if route gain is already at peak, return 0.0f factor
		if (g >= max_factor) {
			return 0.0f;
		}

		// factor is calculated so that it would raise current route to max
		factor = max_factor / g - 1.0f;
	}

	return factor;
}

void
GainControlGroup::set_group_value (std::shared_ptr<AutomationControl> control, double val)
{
	Glib::Threads::RWLock::ReaderLock lm (controls_lock);

	if (_mode & Relative) {

		gain_t usable_gain = control->get_value();

		if (usable_gain < 0.000001f) {
			usable_gain = 0.000001f;
		}

		gain_t delta = val;
		if (delta < 0.000001f) {
			delta = 0.000001f;
		}

		delta -= usable_gain;

		if (delta == 0.0f) {
			return;
		}

		gain_t factor = delta / usable_gain;

		if (factor > 0.0f) {
			factor = get_max_factor (factor);
			if (factor == 0.0f) {
				control->Changed (true, Controllable::ForGroup, val); /* EMIT SIGNAL */
				return;
			}
		} else {
			factor = get_min_factor (factor);
			if (factor == 0.0f) {
				control->Changed (true, Controllable::ForGroup, val); /* EMIT SIGNAL */
				return;
			}
		}

		/* set the primary control */

		control->set_value (val, Controllable::ForGroup);

		/* now propagate across the group */

		for (ControlMap::iterator c = _controls.begin(); c != _controls.end(); ++c) {
			if (c->second == control) {
				continue;
			}

			std::shared_ptr<GainControl> gc = std::dynamic_pointer_cast<GainControl> (c->second);

			if (gc) {
				gc->inc_gain (factor);
			}
		}

	} else {

		/* just set entire group */

		for (auto & c : _controls) {
			c.second->set_value (val, Controllable::ForGroup);
		}
	}
}
