/*
 * Copyright (C) 2008-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2010-2013 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef EVORAL_CONTROL_HPP
#define EVORAL_CONTROL_HPP

#include <set>
#include <map>
#include <memory>

#include "pbd/signals.h"

#include "temporal/timeline.h"

#include "evoral/visibility.h"
#include "evoral/ControlList.h"
#include "evoral/Parameter.h"
#include "evoral/ParameterDescriptor.h"

namespace Evoral {

struct ParameterDescriptor;
class Transport;
class TypeMap;

/** Base class representing some kind of (automatable) control; a fader's gain,
 *  for example, or a compressor plugin's threshold.
 *
 *  The class knows the Evoral::Parameter that it is controlling, and has
 *  a list of values for automation.
 */
class LIBEVORAL_API Control
{
public:
	Control (const Parameter&               parameter,
	         const ParameterDescriptor&     desc,
	         std::shared_ptr<ControlList> list);

	virtual ~Control() {}

	virtual void   set_double (double val, Temporal::timepos_t const & when = Temporal::timepos_t (), bool to_list = false);
	virtual double get_double () const { return _user_value; }

	void set_list(std::shared_ptr<ControlList>);

	std::shared_ptr<ControlList>       list()       { return _list; }
	std::shared_ptr<const ControlList> list() const { return _list; }

	inline const Parameter& parameter() const { return _parameter; }

	/** Emitted when the our ControlList is marked dirty */
	PBD::Signal<void()> ListMarkedDirty;

protected:
	Parameter                      _parameter;
	std::shared_ptr<ControlList> _list;
	double                         _user_value;
	PBD::ScopedConnection          _list_marked_dirty_connection;

private:
	void list_marked_dirty ();
};

} // namespace Evoral

#endif // EVORAL_CONTROL_HPP
