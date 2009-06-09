/*
    Copyright (C) 2009 Paul Davis 

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

#ifndef __ardour_mute_master_h__
#define __ardour_mute_master_h__

#include "evoral/Parameter.hpp"
#include "ardour/automation_control.h"
#include "ardour/automation_list.h"

namespace ARDOUR {

class Session;

class MuteMaster : public AutomationControl
{
  public:
	enum MutePoint {
		PreFader  = 0x1,
		PostFader = 0x2,
		Listen    = 0x4,
		Main      = 0x8
	};
	
	MuteMaster (Session& s, const std::string& name);
	~MuteMaster() {}

	bool muted_pre_fader() const  { return _mute_point & PreFader; }
	bool muted_post_fader() const { return _mute_point & PostFader; }
	bool muted_listen() const     { return _mute_point & Listen; }
	bool muted_main () const      { return _mute_point & Main; }

	bool muted_at (MutePoint mp) const { return _mute_point & mp; }
	bool muted() const { return _mute_point != MutePoint (0) && get_value() != 0.0; }
	
	gain_t mute_gain_at (MutePoint) const;

	void clear_mute ();
	void mute_at (MutePoint);
	void unmute_at (MutePoint);

	void mute (bool yn);

	/* Controllable interface */

	void set_value (float); /* note: float is used as a bitfield of MutePoints */
	float get_value () const;

	sigc::signal<void> MutePointChanged;

	XMLNode& get_state();
	int set_state(const XMLNode& node);

  private:
	AutomationList* _automation;
	MutePoint _mute_point;
};

} // namespace ARDOUR

#endif /*__ardour_mute_master_h__ */
