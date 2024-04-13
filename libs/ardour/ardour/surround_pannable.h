/*
 * Copyright (C) 2023 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_surround_pannable_h_
#define _ardour_surround_pannable_h_

#include "pbd/stateful.h"

#include "ardour/automatable.h"
#include "ardour/automation_control.h"
#include "ardour/session_handle.h"

namespace ARDOUR
{

class LIBARDOUR_API SurroundControllable : public AutomationControl
{
public:
	SurroundControllable (Session&, Evoral::Parameter, Temporal::TimeDomainProvider const&);
	std::string get_user_string () const;

private:
};

class LIBARDOUR_API SurroundPannable : public Automatable, public PBD::Stateful, public SessionHandleRef
{
public:
	SurroundPannable (Session& s, uint32_t chn, Temporal::TimeDomainProvider const &);
	~SurroundPannable ();

	std::shared_ptr<AutomationControl> pan_pos_x;
	std::shared_ptr<AutomationControl> pan_pos_y;
	std::shared_ptr<AutomationControl> pan_pos_z;
	std::shared_ptr<AutomationControl> pan_size;
	std::shared_ptr<AutomationControl> pan_snap;
	std::shared_ptr<AutomationControl> binaural_render_mode;
	std::shared_ptr<AutomationControl> sur_elevation_enable;
	std::shared_ptr<AutomationControl> sur_zones;
	std::shared_ptr<AutomationControl> sur_ramp;

	void set_automation_state (AutoState);
	AutoState automation_state() const { return _auto_state; }
	PBD::Signal1<void, AutoState> automation_state_changed;

	bool automation_playback() const {
		return (_auto_state & Play) || ((_auto_state & (Touch | Latch)) && !touching());
	}

	void foreach_pan_control (boost::function<void(std::shared_ptr<AutomationControl>)>) const;

	void setup_visual_links ();
	void sync_visual_link_to (std::shared_ptr<SurroundPannable>);
	void sync_auto_state_with (std::shared_ptr<SurroundPannable>);

	bool touching() const;

	XMLNode& get_state () const;
	int set_state (const XMLNode&, int version);

protected:
	void control_auto_state_changed (AutoState);
	virtual XMLNode& state () const;

	AutoState _auto_state;
	uint32_t  _responding_to_control_auto_state_change;

private:
	void value_changed ();
};

}

#endif
