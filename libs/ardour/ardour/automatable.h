/*
 * Copyright (C) 2007-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2018 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_automatable_h__
#define __ardour_automatable_h__

#include <map>
#include <set>
#include <string>

#include <boost/shared_ptr.hpp>

#include "pbd/rcu.h"
#include "pbd/signals.h"

#include "evoral/ControlSet.h"

#include "ardour/libardour_visibility.h"
#include "ardour/slavable.h"
#include "ardour/types.h"

class XMLNode;

namespace ARDOUR {

class Session;
class AutomationControl;

/* The inherited ControlSet is virtual because AutomatableSequence inherits
 * from this AND EvoralSequence, which is also a ControlSet
 */
class LIBARDOUR_API Automatable : virtual public Evoral::ControlSet, public Slavable
{
public:
	Automatable(Session&);
	Automatable (const Automatable& other);

	virtual ~Automatable();

	static bool skip_saving_automation; // to be used only by session-state

	boost::shared_ptr<Evoral::Control> control_factory(const Evoral::Parameter& id);

	boost::shared_ptr<AutomationControl> automation_control (PBD::ID const & id) const;
	/* derived classes need to provide some way to search their own child
	   automatable's for a control. normally, we'd just make the method
	   above virtual, and let them override it. But that wouldn't
	   differentiate the "check children" and "just your own" cases.

	   We could theoretically just overload the above method with an extra
	   "bool recurse = default", but the rules of name hiding for C++ mean
	   that making a method virtual will hide other overloaded versions of
	   the same name. This means that virtual automation_control (PBD::ID
	   const &) would hide automation_control (Evoral::Parameter const &
	   id).

	   So, skip around all that with a different name.
	*/
	virtual boost::shared_ptr<AutomationControl> automation_control_recurse (PBD::ID const & id) const {
		return automation_control (id);
	}

	boost::shared_ptr<AutomationControl> automation_control (const Evoral::Parameter& id) {
		return automation_control (id, false);
	}
	boost::shared_ptr<AutomationControl> automation_control (const Evoral::Parameter& id, bool create_if_missing);
	boost::shared_ptr<const AutomationControl> automation_control (const Evoral::Parameter& id) const;

	virtual void add_control(boost::shared_ptr<Evoral::Control>);
	virtual bool find_next_event (Temporal::timepos_t const & start, Temporal::timepos_t const & end, Evoral::ControlEvent& ev, bool only_active = true) const;
	void clear_controls ();

	virtual void non_realtime_locate (samplepos_t now);
	virtual void non_realtime_transport_stop (samplepos_t now, bool flush);

	virtual void automation_run (samplepos_t, pframes_t, bool only_active = false);

	virtual std::string describe_parameter(Evoral::Parameter param);

	AutoState get_parameter_automation_state (Evoral::Parameter param);
	virtual void set_parameter_automation_state (Evoral::Parameter param, AutoState);

	void protect_automation ();

	const std::set<Evoral::Parameter>& what_can_be_automated() const { return _can_automate_list; }

	/** API for Lua binding */
	std::vector<Evoral::Parameter> all_automatable_params () const;

	void what_has_existing_automation (std::set<Evoral::Parameter>&) const;

	static const std::string xml_node_name;

	int set_automation_xml_state (const XMLNode&, Evoral::Parameter default_param);
	XMLNode& get_automation_xml_state();

	PBD::Signal0<void> AutomationStateChanged;

protected:
	Session& _a_session;

	void can_automate(Evoral::Parameter);

	virtual void automation_list_automation_state_changed (Evoral::Parameter, AutoState);
	SerializedRCUManager<ControlList> _automated_controls;

	int load_automation (const std::string& path);
	int old_set_automation_state(const XMLNode&);

	std::set<Evoral::Parameter> _can_automate_list;

	samplepos_t _last_automation_snapshot;

	SlavableControlList slavables () const { return SlavableControlList(); }

	void find_next_ac_event (boost::shared_ptr<AutomationControl>, Temporal::timepos_t const & start, Temporal::timepos_t const & end, Evoral::ControlEvent& ev) const;
	void find_prev_ac_event (boost::shared_ptr<AutomationControl>, Temporal::timepos_t const & start, Temporal::timepos_t const & end, Evoral::ControlEvent& ev) const;

private:
	PBD::ScopedConnectionList _control_connections; ///< connections to our controls' signals
};


} // namespace ARDOUR

#endif /* __ardour_automatable_h__ */
