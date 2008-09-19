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
#include <glibmm/thread.h>
#include <evoral/types.hpp>
#include <evoral/Parameter.hpp>

namespace Evoral {

class Control;
class ControlList;
class ControlEvent;

class ControlSet {
public:
	ControlSet();
	virtual ~ControlSet() {}

	virtual boost::shared_ptr<Control> control(Evoral::Parameter id, bool create_if_missing=false);
	virtual boost::shared_ptr<const Control> control(Evoral::Parameter id) const;
	
	virtual boost::shared_ptr<Control> control_factory(boost::shared_ptr<ControlList> list) const;
	virtual boost::shared_ptr<ControlList> control_list_factory(const Parameter& param) const;
	
	typedef std::map< Parameter, boost::shared_ptr<Control> > Controls;
	Controls&       controls()       { return _controls; }
	const Controls& controls() const { return _controls; }

	virtual void add_control(boost::shared_ptr<Control>);

	virtual bool find_next_event(nframes_t start, nframes_t end, ControlEvent& ev) const;
	
	virtual float default_parameter_value(Parameter param) { return 1.0f; }

	virtual void clear();

	void what_has_data(std::set<Parameter>&) const;
	
	Glib::Mutex& control_lock() const { return _control_lock; }

protected:
	mutable Glib::Mutex _control_lock;
	Controls            _controls;
};

} // namespace Evoral

#endif // EVORAL_CONTROLLABLE_HPP
