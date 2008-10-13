/* This file is part of Evoral.
 * Copyright (C) 2008 Dave Robillard <http://drobilla.net>
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

#ifndef EVORAL_CONTROLLABLE_HPP
#define EVORAL_CONTROLLABLE_HPP

#include <set>
#include <map>
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <glibmm/thread.h>
#include <evoral/types.hpp>
#include <evoral/Parameter.hpp>

namespace Evoral {

class Control;
class ControlList;
class ControlEvent;

class ControlSet : public boost::noncopyable {
public:
	ControlSet();
	virtual ~ControlSet() {}
	
	virtual boost::shared_ptr<Evoral::Control>
	control_factory(const Evoral::Parameter& id) = 0;
	
	boost::shared_ptr<Control>
	control (const Parameter& id, bool create_if_missing=false);

	inline boost::shared_ptr<const Control>
	control (const Parameter& id) const {
		const Controls::const_iterator i = _controls.find(id);
		return (i != _controls.end() ? i->second : boost::shared_ptr<Control>());
	}

	typedef std::map< Parameter, boost::shared_ptr<Control> > Controls;
	inline Controls&       controls()       { return _controls; }
	inline const Controls& controls() const { return _controls; }

	virtual void add_control(boost::shared_ptr<Control>);

	bool find_next_event(FrameTime start, FrameTime end, ControlEvent& ev) const;
	
	virtual bool empty() const { return _controls.size() == 0; }
	virtual void clear();

	void what_has_data(std::set<Parameter>&) const;
	
	Glib::Mutex& control_lock() const { return _control_lock; }

protected:
	mutable Glib::Mutex _control_lock;
	Controls            _controls;
};


} // namespace Evoral

#endif // EVORAL_CONTROLLABLE_HPP
