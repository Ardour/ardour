/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_monitor_control_h__
#define __ardour_monitor_control_h__

#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/dynamic_bitset.hpp>

#include "ardour/slavable_automation_control.h"
#include "ardour/monitorable.h"

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class Session;

class LIBARDOUR_API MonitorControl : public SlavableAutomationControl
{
  public:
	MonitorControl (Session& session, std::string const & name, Monitorable& m, Temporal::TimeDomain);
	~MonitorControl() {}

	MonitorChoice monitoring_choice() const { return static_cast<MonitorChoice> ((int)get_value()); }
	MonitorState monitoring_state () const { return _monitorable.monitoring_state(); }

	int set_state (XMLNode const&, int);
	XMLNode& get_state ();

  protected:
	void actually_set_value (double, PBD::Controllable::GroupControlDisposition group_override);

  private:
	Monitorable& _monitorable;
	MonitorChoice _monitoring;
};

} /* namespace */

#endif /* __libardour_monitor_control_h__ */
