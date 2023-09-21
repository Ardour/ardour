/*
 * Copyright (C) 2008-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2010-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
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

#ifndef EVORAL_CONTROLLABLE_HPP
#define EVORAL_CONTROLLABLE_HPP

#include <set>
#include <map>
#include <memory>

#include <boost/utility.hpp>
#include <glibmm/threads.h>
#include "pbd/signals.h"

#include "temporal/domain_swap.h"
#include "temporal/types.h"

#include "evoral/visibility.h"
#include "evoral/Parameter.h"
#include "evoral/ControlList.h"

namespace Evoral {

class Control;
class ControlEvent;

class LIBEVORAL_API ControlSet : public boost::noncopyable, public Temporal::TimeDomainSwapper
{
public:
	ControlSet ();
	ControlSet (const ControlSet&);
        virtual ~ControlSet() {}

	virtual std::shared_ptr<Evoral::Control> control_factory(const Evoral::Parameter& id) = 0;

	std::shared_ptr<Control>
	control (const Parameter& id, bool create_if_missing=false);

	inline std::shared_ptr<const Control>
	control (const Parameter& id) const {
		const Controls::const_iterator i = _controls.find(id);
		return (i != _controls.end() ? i->second : std::shared_ptr<Control>());
	}

	typedef std::map< Parameter, std::shared_ptr<Control> > Controls;
	inline Controls&       controls()       { return _controls; }
	inline const Controls& controls() const { return _controls; }

	virtual void add_control(std::shared_ptr<Control>);

	virtual bool controls_empty() const { return _controls.size() == 0; }
	virtual void clear_controls();

	void what_has_data(std::set<Parameter>&) const;

	Glib::Threads::Mutex& control_lock() const { return _control_lock; }

	void start_domain_bounce (Temporal::DomainBounceInfo&);
	void finish_domain_bounce (Temporal::DomainBounceInfo&);

protected:
	virtual void control_list_marked_dirty () {}
	virtual void control_list_interpolation_changed (Parameter const&, ControlList::InterpolationStyle) {}

	mutable Glib::Threads::Mutex _control_lock;
	Controls            _controls;

	PBD::ScopedConnectionList _list_connections;

private:

	PBD::ScopedConnectionList _control_connections;
};


} // namespace Evoral

#endif // EVORAL_CONTROLLABLE_HPP
