/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_slavable_automation_control_h__
#define __ardour_slavable_automation_control_h__

#include "ardour/automation_control.h"
#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class LIBARDOUR_API SlavableAutomationControl : public AutomationControl
{
public:
	SlavableAutomationControl(ARDOUR::Session&,
	                          const Evoral::Parameter&                  parameter,
	                          const ParameterDescriptor&                desc,
	                          boost::shared_ptr<ARDOUR::AutomationList> l=boost::shared_ptr<ARDOUR::AutomationList>(),
	                          const std::string&                        name="",
	                          PBD::Controllable::Flag                   flags=PBD::Controllable::Flag (0)
		);

	virtual ~SlavableAutomationControl ();

	double get_value () const;

	void add_master (boost::shared_ptr<AutomationControl>);
	void remove_master (boost::shared_ptr<AutomationControl>);
	void clear_masters ();
	bool slaved_to (boost::shared_ptr<AutomationControl>) const;
	bool slaved () const;

	virtual void automation_run (samplepos_t start, pframes_t nframes);

	double get_masters_value () const {
		Glib::Threads::RWLock::ReaderLock lm (master_lock);
		return get_masters_value_locked ();
	}

	/* factor out get_masters_value() */
	double reduce_by_masters (double val, bool ignore_automation_state = false) const {
		Glib::Threads::RWLock::ReaderLock lm (master_lock);
		return reduce_by_masters_locked (val, ignore_automation_state);
	}

	bool get_masters_curve (samplepos_t s, samplepos_t e, float* v, samplecnt_t l) const {
		Glib::Threads::RWLock::ReaderLock lm (master_lock);
		return get_masters_curve_locked (s, e, v, l);
	}

	/* for toggled/boolean controls, returns a count of the number of
	   masters currently enabled. For other controls, returns zero.
	*/
	int32_t   get_boolean_masters () const;

	PBD::Signal0<void> MasterStatusChange;

	void use_saved_master_ratios ();

	int set_state (XMLNode const&, int);
	XMLNode& get_state();

	bool find_next_event (Temporal::timepos_t const & n, Temporal::timepos_t const & e, Evoral::ControlEvent& ev) const
	{
		Glib::Threads::RWLock::ReaderLock lm (master_lock);
		return find_next_event_locked (n, e, ev);
	}

	bool find_next_event_locked (Temporal::timepos_t const & now, Temporal::timepos_t const & end, Evoral::ControlEvent& next_event) const;

protected:

	class MasterRecord {
	public:
		MasterRecord (boost::weak_ptr<AutomationControl> gc, double vc, double vm)
			: _master (gc)
			, _yn (false)
			, _val_ctrl (vc)
			, _val_master (vm)
		{}

		boost::shared_ptr<AutomationControl> master() const { assert(_master.lock()); return _master.lock(); }

		double val_ctrl () const { return _val_ctrl; }
		double val_master () const { return _val_master; }

		double val_master_inv () const { return _val_master == 0 ? 0 : 1.0 / _val_master; }
		double master_ratio () const { return _val_master == 0 ? 0 : master()->get_value() / _val_master; }

		int set_state (XMLNode const&, int);

		/* for boolean/toggled controls, we store a boolean value to
		 * indicate if this master returned true/false (1.0/0.0) from
		 * ::get_value() after its most recent change.
		 */

		bool yn() const { return _yn; }
		void set_yn (bool yn) { _yn = yn; }

		PBD::ScopedConnection changed_connection;
		PBD::ScopedConnection dropped_connection;

  private:
		boost::weak_ptr<AutomationControl> _master;
		/* holds most recently seen master value for boolean/toggle controls */
		bool   _yn;

		/* values at time of assignment */
		double _val_ctrl;
		double _val_master;
	};

	mutable Glib::Threads::RWLock master_lock;
	typedef std::map<PBD::ID,MasterRecord> Masters;
	Masters _masters;

	void   master_going_away (boost::weak_ptr<AutomationControl>);
	double get_value_locked() const;
	void   actually_set_value (double value, PBD::Controllable::GroupControlDisposition);
	void   update_boolean_masters_records (boost::shared_ptr<AutomationControl>);

	virtual bool get_masters_curve_locked (samplepos_t, samplepos_t, float*, samplecnt_t) const;
	bool masters_curve_multiply (timepos_t const &, timepos_t const &, float*, samplecnt_t) const;

	virtual double reduce_by_masters_locked (double val, bool) const;
	virtual double scale_automation_callback (double val, double ratio) const;

	virtual bool handle_master_change (boost::shared_ptr<AutomationControl>);
	virtual bool boolean_automation_run_locked (samplepos_t start, pframes_t len);
	bool boolean_automation_run (samplepos_t start, pframes_t len);

	virtual void   master_changed (bool from_self, GroupControlDisposition gcd, boost::weak_ptr<AutomationControl>);
	virtual double get_masters_value_locked () const;
	virtual void   pre_remove_master (boost::shared_ptr<AutomationControl>) {}
	virtual void   post_add_master (boost::shared_ptr<AutomationControl>) {}

	XMLNode* _masters_node; /* used to store master ratios in ::set_state() for later use */
};

} // namespace ARDOUR

#endif /* __ardour_slavable_automation_control_h__ */
