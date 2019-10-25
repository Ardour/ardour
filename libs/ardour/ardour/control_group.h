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

#ifndef __libardour_control_group_h__
#define __libardour_control_group_h__

#include <map>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <glibmm/threads.h>

#include "pbd/controllable.h"

#include "evoral/Parameter.h"

#include "ardour/automation_control.h"
#include "ardour/types.h"

namespace ARDOUR {

class LIBARDOUR_API ControlGroup : public boost::enable_shared_from_this<ControlGroup>
{
  public:
	ControlGroup (Evoral::Parameter p);
	virtual ~ControlGroup ();

	enum Mode {
		Relative = 0x1,
		Inverted = 0x2,
	};

	int add_control (boost::shared_ptr<AutomationControl>);
	int remove_control (boost::shared_ptr<AutomationControl>);

	ControlList controls () const;

	void clear ();

	void set_active (bool);
	bool active() const { return _active; }

	void set_mode (Mode m);
	Mode mode () const { return _mode; }

	Evoral::Parameter parameter() const { return _parameter; }

	virtual void set_group_value (boost::shared_ptr<AutomationControl>, double val);
	virtual void pre_realtime_queue_stuff (double val);

	bool use_me (PBD::Controllable::GroupControlDisposition gcd) const {
		switch (gcd) {
		case PBD::Controllable::ForGroup:
			return false;
		case PBD::Controllable::NoGroup:
			return false;
		case PBD::Controllable::InverseGroup:
			return !_active;
		default:
			return _active;
		}
	}

  protected:
	typedef std::map<PBD::ID,boost::shared_ptr<AutomationControl> > ControlMap;
	Evoral::Parameter _parameter;
	mutable Glib::Threads::RWLock controls_lock;
	ControlMap _controls;
	bool _active;
	Mode _mode;
	PBD::ScopedConnectionList member_connections;
	bool propagating;

	void control_going_away (boost::weak_ptr<AutomationControl>);
};


class LIBARDOUR_API GainControlGroup : public ControlGroup
{
  public:
	GainControlGroup();

	void set_group_value (boost::shared_ptr<AutomationControl>, double val);

  private:
	gain_t get_max_factor (gain_t);
	gain_t get_min_factor (gain_t);
};

} /* namespace */

#endif /* __libardour_control_group_h__ */
