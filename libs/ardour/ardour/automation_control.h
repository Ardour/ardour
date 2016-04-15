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
	                  const std::string&                        name="");

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

  private:
	/* I am unclear on why we have to make ControlGroup a friend in order
	   to get access to the ::set_group() method when it is already
	   declared to be a friend in ControlGroupMember. Oh well.
	*/
	friend class ControlGroup;
	void set_group (boost::shared_ptr<ControlGroup>);
};

class SlavableAutomationControl : public AutomationControl
{
    public:
	SlavableAutomationControl(ARDOUR::Session&,
	                  const Evoral::Parameter&                  parameter,
	                  const ParameterDescriptor&                desc,
	                  boost::shared_ptr<ARDOUR::AutomationList> l=boost::shared_ptr<ARDOUR::AutomationList>(),
	                  const std::string&                        name="");

	double get_value () const;

	void add_master (boost::shared_ptr<AutomationControl>);
	void remove_master (boost::shared_ptr<AutomationControl>);
	void clear_masters ();
	bool slaved_to (boost::shared_ptr<AutomationControl>) const;
	bool slaved () const;
	double get_masters_value () const {
		Glib::Threads::RWLock::ReaderLock lm (master_lock);
		return get_masters_value_locked ();
	}

	std::vector<PBD::ID> masters () const;

	PBD::Signal0<void> MasterStatusChange;

    protected:

	class MasterRecord {
          public:
		MasterRecord (boost::shared_ptr<AutomationControl> gc, double r)
			: _master (gc)
			, _ratio (r)
		{}

		boost::shared_ptr<AutomationControl> master() const { return _master; }
		double ratio () const { return _ratio; }
		void reset_ratio (double r) { _ratio = r; }

		PBD::ScopedConnection connection;

         private:
		boost::shared_ptr<AutomationControl> _master;
		double _ratio;

	};

	mutable Glib::Threads::RWLock master_lock;
	typedef std::map<PBD::ID,MasterRecord> Masters;
	Masters _masters;
	PBD::ScopedConnectionList masters_connections;
	virtual void master_changed (bool from_self, GroupControlDisposition gcd);
	void master_going_away (boost::weak_ptr<AutomationControl>);
	virtual void recompute_masters_ratios (double val) { /* do nothing by default */}
	virtual double get_masters_value_locked () const;
	double get_value_locked() const;
	void actually_set_value (double val, PBD::Controllable::GroupControlDisposition group_override);
};


} // namespace ARDOUR

#endif /* __ardour_automation_control_h__ */
