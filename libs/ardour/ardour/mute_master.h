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
#include "pbd/signals.h"
#include "pbd/stateful.h"
#include <string>

#include "ardour/session_handle.h"

namespace ARDOUR {

class Session;

class LIBARDOUR_API MuteMaster : public SessionHandleRef, public PBD::Stateful
{
  public:
	/** deliveries to mute when the channel is "muted" */
	enum MutePoint {
		PreFader  = 0x1, ///< mute all pre-fader sends
		PostFader = 0x2, ///< mute all post-fader sends
		Listen    = 0x4, ///< mute listen out
		Main      = 0x8  ///< mute main out
	};

	static const MutePoint AllPoints;

	MuteMaster (Session& s, const std::string& name);
	~MuteMaster() {}

	bool muted_by_self () const { return _muted_by_self && (_mute_point != MutePoint (0)); }
	bool muted_by_self_at (MutePoint mp) const { return _muted_by_self && (_mute_point & mp); }
	bool muted_by_others_at (MutePoint mp) const;

	gain_t mute_gain_at (MutePoint) const;

	void set_muted_by_self (bool yn) { _muted_by_self = yn; }

	void mute_at (MutePoint);
	void unmute_at (MutePoint);

	void set_mute_points (const std::string& mute_point);
	void set_mute_points (MutePoint);
	MutePoint mute_points() const { return _mute_point; }

	void set_soloed (bool);
	void set_solo_ignore (bool yn) { _solo_ignore = yn; }

	PBD::Signal0<void> MutePointChanged;

	XMLNode& get_state();
	int set_state(const XMLNode&, int version);

  private:
	volatile MutePoint _mute_point;
	volatile bool      _muted_by_self;
	volatile bool      _soloed;
	volatile bool      _solo_ignore;
};

} // namespace ARDOUR

#endif /*__ardour_mute_master_h__ */
