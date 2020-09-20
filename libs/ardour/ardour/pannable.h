/*
 * Copyright (C) 2011-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libardour_pannable_h__
#define __libardour_pannable_h__

#include <string>

#include <boost/shared_ptr.hpp>

#include "pbd/stateful.h"
#include "evoral/Parameter.h"

#include "ardour/automatable.h"
#include "ardour/session_handle.h"

namespace ARDOUR {

class Session;
class AutomationControl;
class Panner;

class LIBARDOUR_API Pannable : public PBD::Stateful, public Automatable, public SessionHandleRef
{
public:
	Pannable (Session& s);
	~Pannable ();

	boost::shared_ptr<AutomationControl> pan_azimuth_control;
	boost::shared_ptr<AutomationControl> pan_elevation_control;
	boost::shared_ptr<AutomationControl> pan_width_control;
	boost::shared_ptr<AutomationControl> pan_frontback_control;
	boost::shared_ptr<AutomationControl> pan_lfe_control;

	boost::shared_ptr<Panner> panner() const { return _panner.lock(); }
	void set_panner(boost::shared_ptr<Panner>);

	const std::set<Evoral::Parameter>& what_can_be_automated() const;

	void set_automation_state (AutoState);
	AutoState automation_state() const { return _auto_state; }
	PBD::Signal1<void, AutoState> automation_state_changed;

	bool automation_playback() const {
		return (_auto_state & Play) || ((_auto_state & (Touch | Latch)) && !touching());
	}
	bool automation_write () const {
		return ((_auto_state & Write) || ((_auto_state & (Touch | Latch)) && touching()));
	}

	void start_touch (timepos_t const & when);
	void stop_touch (timepos_t const & when);

	bool touching() const { return g_atomic_int_get (&_touching); }

	bool writing() const { return _auto_state == Write; }
	bool touch_enabled() const { return _auto_state & (Touch | Latch); }

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	bool has_state() const { return _has_state; }

protected:
	virtual XMLNode& state ();

	boost::weak_ptr<Panner> _panner;
	AutoState _auto_state;
	bool      _has_state;
	uint32_t  _responding_to_control_auto_state_change;

	GATOMIC_QUAL gint _touching;

	void control_auto_state_changed (AutoState);

private:
	void value_changed ();
};

} // namespace

#endif /* __libardour_pannable_h__ */
