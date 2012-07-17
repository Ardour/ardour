/* This file is part of Evoral.
 * Copyright (C) 2008 David Robillard <http://drobilla.net>
 * Copyright (C) 2000-2008 Paul Davis
 *
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Evoral is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef EVORAL_CONTROL_HPP
#define EVORAL_CONTROL_HPP

#include <set>
#include <map>
#include <boost/shared_ptr.hpp>
#include "pbd/signals.h"
#include "evoral/types.hpp"
#include "evoral/Parameter.hpp"

namespace Evoral {

class ControlList;
class Transport;

/** Base class representing some kind of (automatable) control; a fader's gain,
 *  for example, or a compressor plugin's threshold.
 *
 *  The class knows the Evoral::Parameter that it is controlling, and has
 *  a list of values for automation.
 */

class Control
{
public:
	Control(const Parameter& parameter, boost::shared_ptr<ControlList>);
	virtual ~Control() {}

        virtual void   set_double (double val, double frame=0, bool to_list=false);
	virtual double get_double (bool from_list=false, double frame=0) const;

	/** Get the latest user-set value
	 * (which may not equal get_value() when automation is playing back).
	 *
	 * Automation write/touch works by periodically sampling this value
	 * and adding it to the ControlList.
	 */
	double user_double() const { return _user_value; }

	void set_list(boost::shared_ptr<ControlList>);

	boost::shared_ptr<ControlList>       list()       { return _list; }
	boost::shared_ptr<const ControlList> list() const { return _list; }

	inline const Parameter& parameter() const { return _parameter; }

	/** Emitted when the our ControlList is marked dirty */
	PBD::Signal0<void> ListMarkedDirty;

protected:
	Parameter                      _parameter;
	boost::shared_ptr<ControlList> _list;
	double                         _user_value;
	PBD::ScopedConnection          _list_marked_dirty_connection;

private:
	void list_marked_dirty ();
};

} // namespace Evoral

#endif // EVORAL_CONTROL_HPP
