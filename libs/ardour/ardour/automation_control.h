/*
    Copyright (C) 2007 Paul Davis
    Author: David Robillard

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

#ifndef __ardour_automation_control_h__
#define __ardour_automation_control_h__

#include <map>

#include <glibmm/threads.h>

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "pbd/controllable.h"

#include "evoral/types.hpp"
#include "evoral/Control.hpp"

#include "ardour/automation_list.h"
#include "ardour/control_group_member.h"
#include "ardour/parameter_descriptor.h"

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class Session;
class Automatable;
class ControlGroup;

/** A PBD::Controllable with associated automation data (AutomationList)
 */
class LIBARDOUR_API AutomationControl
	: public PBD::Controllable
	, public Evoral::Control
	, public boost::enable_shared_from_this<AutomationControl>
	, public ControlGroupMember
{
    public:
	AutomationControl(ARDOUR::Session&,
	                  const Evoral::Parameter&                  parameter,
	                  const ParameterDescriptor&                desc,
	                  boost::shared_ptr<ARDOUR::AutomationList> l=boost::shared_ptr<ARDOUR::AutomationList>(),
	                  const std::string&                        name="",
	                  PBD::Controllable::Flag                   flags=PBD::Controllable::Flag (0)
		);

	~AutomationControl ();

	boost::shared_ptr<AutomationList> alist() const {
		return boost::dynamic_pointer_cast<AutomationList>(_list);
	}

	void set_list (boost::shared_ptr<Evoral::ControlList>);

	inline bool automation_playback() const {
		return alist() ? alist()->automation_playback() : false;
	}

	inline bool automation_write() const {
		return alist() ? alist()->automation_write() : false;
	}

	inline AutoState automation_state() const {
		return alist() ? alist()->automation_state() : Off;
	}

	inline AutoStyle automation_style() const {
		return alist() ? alist()->automation_style() : Absolute;
	}

	void set_automation_state(AutoState as);
	void set_automation_style(AutoStyle as);
	void start_touch(double when);
	void stop_touch(bool mark, double when);

	/* inherited from PBD::Controllable.
	 */
	double get_value () const;
	/* inherited from PBD::Controllable.
	 * Derived classes MUST call ::writable() to verify
	 * that writing to the parameter is legal at that time.
	 */
	void set_value (double value, PBD::Controllable::GroupControlDisposition group_override);
	/* automation related value setting */
	virtual bool writable () const;
	/* Call to ::set_value() with no test for writable() because
	 * this is only used by automation playback.
	 */
	void set_value_unchecked (double val) {
		actually_set_value (val, PBD::Controllable::NoGroup);
	}

	double lower()   const { return _desc.lower; }
	double upper()   const { return _desc.upper; }
	double normal()  const { return _desc.normal; }
	bool   toggled() const { return _desc.toggled; }

	double internal_to_interface (double i) const;
	double interface_to_internal (double i) const;

	const ParameterDescriptor& desc() const { return _desc; }

	const ARDOUR::Session& session() const { return _session; }
	void commit_transaction (bool did_write);

  protected:
	ARDOUR::Session& _session;
	boost::shared_ptr<ControlGroup> _group;

	const ParameterDescriptor _desc;

	bool check_rt (double val, Controllable::GroupControlDisposition gcd);

	/* derived classes may reimplement this, but should either
	   call this explicitly inside their version OR make sure that the
	   Controllable::Changed signal is emitted when necessary.
	*/

	virtual void actually_set_value (double value, PBD::Controllable::GroupControlDisposition);

	/* Session needs to call this method before it queues up the real
	   change for execution in a realtime context. C++ access control sucks.
	*/
	friend class Session;
	/* this is what the session invokes */
	void pre_realtime_queue_stuff (double new_value, PBD::Controllable::GroupControlDisposition);
	/* this will be invoked in turn on behalf of the group or the control by itself */
	virtual void do_pre_realtime_queue_stuff (double new_value) {}

  private:
	/* I am unclear on why we have to make ControlGroup a friend in order
	   to get access to the ::set_group() method when it is already
	   declared to be a friend in ControlGroupMember. Oh well.
	*/
	friend class ControlGroup;
	void set_group (boost::shared_ptr<ControlGroup>);
};


} // namespace ARDOUR

#endif /* __ardour_automation_control_h__ */
