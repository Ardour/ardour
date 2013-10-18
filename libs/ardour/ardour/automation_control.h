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

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "pbd/controllable.h"
#include "evoral/Control.hpp"

#include "ardour/libardour_visibility.h"
#include "ardour/automation_list.h"

namespace ARDOUR {

class Session;
class Automatable;


/** A PBD::Controllable with associated automation data (AutomationList)
 */
class LIBARDOUR_API AutomationControl : public PBD::Controllable, public Evoral::Control, public boost::enable_shared_from_this<AutomationControl>
{
public:
	AutomationControl(ARDOUR::Session&,
			  const Evoral::Parameter& parameter,
			  boost::shared_ptr<ARDOUR::AutomationList> l=boost::shared_ptr<ARDOUR::AutomationList>(),
			  const std::string& name="");

        ~AutomationControl ();

	boost::shared_ptr<AutomationList> alist() const {
		return boost::dynamic_pointer_cast<AutomationList>(_list);
	}

	void set_list (boost::shared_ptr<Evoral::ControlList>);

	inline bool automation_playback() const {
		return alist()->automation_playback();
	}

	inline bool automation_write() const {
		return alist()->automation_write();
	}

	inline AutoState automation_state() const {
		return alist()->automation_state();
	}

	inline AutoStyle automation_style() const {
		return alist()->automation_style();
	}

        void set_automation_state(AutoState as);
        void set_automation_style(AutoStyle as);
        void start_touch (double when);
        void stop_touch (bool mark, double when);

	void set_value (double);
	double get_value () const;

	virtual double internal_to_interface (double v) const {
		return v;
	}
	
	virtual double interface_to_internal (double v) const {
		return v;
	}

	virtual double internal_to_user (double v) const {
		return v;
	}

	double lower() const { return parameter().min(); }
	double upper() const { return parameter().max(); }
	double normal() const { return parameter().normal(); }
	bool toggled() const { return parameter().toggled(); }

	const ARDOUR::Session& session() const { return _session; }

protected:

	ARDOUR::Session& _session;
};


} // namespace ARDOUR

#endif /* __ardour_automation_control_h__ */
