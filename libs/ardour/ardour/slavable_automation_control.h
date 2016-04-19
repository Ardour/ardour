/*
    Copyright (C) 2016 Paul Davis

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

#ifndef __ardour_slavable_automation_control_h__
#define __ardour_slavable_automation_control_h__

#include "ardour/automation_control.h"

namespace ARDOUR {

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

		/* for boolean/toggled controls, we store a boolean value to
		 * indicate if this master returned true/false (1.0/0.0) from
		 * ::get_value() after its most recent change.
		 */

		bool yn() const { return _yn; }
		void set_yn (bool yn) { _yn = yn; }

		/* for non-boolean/non-toggled controls, we store a ratio that
		 * connects the value of the master with the value of this
		 * slave. See comments in the source for more details on how
		 * this is computed and used.
		 */

		double ratio () const { return _ratio; }
		void reset_ratio (double r) { _ratio = r; }

		PBD::ScopedConnection connection;

         private:
		boost::shared_ptr<AutomationControl> _master;
		union {
			double _ratio;
			bool   _yn;
		};
	};

	mutable Glib::Threads::RWLock master_lock;
	typedef std::map<PBD::ID,MasterRecord> Masters;
	Masters _masters;
	PBD::ScopedConnectionList masters_connections;

	void   master_going_away (boost::weak_ptr<AutomationControl>);
	double get_value_locked() const;
	void   actually_set_value (double val, PBD::Controllable::GroupControlDisposition group_override);
	void   update_boolean_masters_records (boost::shared_ptr<AutomationControl>);
	int32_t   get_boolean_masters () const;

	virtual void   master_changed (bool from_self, GroupControlDisposition gcd, boost::shared_ptr<AutomationControl>);
	virtual void   recompute_masters_ratios (double val) { /* do nothing by default */}
	virtual double get_masters_value_locked () const;
	virtual void   pre_remove_master (boost::shared_ptr<AutomationControl>) {}
	virtual void   post_add_master (boost::shared_ptr<AutomationControl>) {}


};

} // namespace ARDOUR

#endif /* __ardour_slavable_automation_control_h__ */
