/*
    Copyright (C) 2000-2007 Paul Davis

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

#ifndef __ardour_automatable_h__
#define __ardour_automatable_h__

#include <map>
#include <set>
#include <string>
#include <boost/shared_ptr.hpp>
#include "pbd/signals.h"
#include "evoral/ControlSet.hpp"
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

class XMLNode;

namespace ARDOUR {

class Session;
class AutomationControl;

/* The inherited ControlSet is virtual because AutomatableSequence inherits
 * from this AND EvoralSequence, which is also a ControlSet
 */
class LIBARDOUR_API Automatable : virtual public Evoral::ControlSet
{
public:
	Automatable(Session&);
	Automatable (const Automatable& other);

        virtual ~Automatable();

	boost::shared_ptr<Evoral::Control>
	control_factory(const Evoral::Parameter& id);

	boost::shared_ptr<AutomationControl>
	automation_control (const Evoral::Parameter& id, bool create_if_missing=false);

	boost::shared_ptr<const AutomationControl>
	automation_control (const Evoral::Parameter& id) const;

	virtual void add_control(boost::shared_ptr<Evoral::Control>);
	void clear_controls ();

        virtual void transport_located (framepos_t now);
	virtual void transport_stopped (framepos_t now);

	virtual std::string describe_parameter(Evoral::Parameter param);
	virtual std::string value_as_string (boost::shared_ptr<AutomationControl>) const;

	AutoState get_parameter_automation_state (Evoral::Parameter param);
	virtual void set_parameter_automation_state (Evoral::Parameter param, AutoState);

	AutoStyle get_parameter_automation_style (Evoral::Parameter param);
	void set_parameter_automation_style (Evoral::Parameter param, AutoStyle);

	void protect_automation ();

	const std::set<Evoral::Parameter>& what_can_be_automated() const { return _can_automate_list; }
	void what_has_existing_automation (std::set<Evoral::Parameter>&) const;

	static const std::string xml_node_name;

	int set_automation_xml_state (const XMLNode&, Evoral::Parameter default_param);
	XMLNode& get_automation_xml_state();

  protected:
	Session& _a_session;

	void can_automate(Evoral::Parameter);

	virtual void automation_list_automation_state_changed (Evoral::Parameter, AutoState) {}

	int load_automation (const std::string& path);
	int old_set_automation_state(const XMLNode&);

	std::set<Evoral::Parameter> _can_automate_list;

	framepos_t _last_automation_snapshot;

private:
	PBD::ScopedConnectionList _control_connections; ///< connections to our controls' signals
};


} // namespace ARDOUR

#endif /* __ardour_automatable_h__ */
