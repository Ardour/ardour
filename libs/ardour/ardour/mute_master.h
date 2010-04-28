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

namespace ARDOUR {

class Session;

class MuteMaster : public PBD::Stateful
{
  public:
	enum MutePoint {
		PreFader  = 0x1,
		PostFader = 0x2,
		Listen    = 0x4,
		Main      = 0x8
	};

	static const MutePoint AllPoints;

	MuteMaster (Session& s, const std::string& name);
	~MuteMaster() {}

	bool self_muted() const { return _self_muted && (_mute_point != MutePoint (0)); }
	bool muted_by_others() const { return _muted_by_others && (_mute_point != MutePoint (0)); }
	bool muted() const { return (_self_muted || (_muted_by_others > 0)) && (_mute_point != MutePoint (0)); }
        bool muted_at (MutePoint mp) const { return (_self_muted || (_muted_by_others > 0)) && (_mute_point & mp); }
        bool self_muted_at (MutePoint mp) const { return _self_muted && (_mute_point & mp); }
        bool muted_by_others_at (MutePoint mp) const { return (_muted_by_others > 0) && (_mute_point & mp); }

	bool muted_pre_fader() const  { return muted_at (PreFader); }
	bool muted_post_fader() const { return muted_at (PostFader); }
	bool muted_listen() const     { return muted_at (Listen); }
        bool muted_main () const      { return muted_at (Main); }

	gain_t mute_gain_at (MutePoint) const;

        void set_self_muted (bool yn) { _self_muted = yn; }
        void mod_muted_by_others (int delta);
        void clear_muted_by_others ();

	void mute_at (MutePoint);
	void unmute_at (MutePoint);

	void set_mute_points (const std::string& mute_point);
        void set_mute_points (MutePoint);
        MutePoint mute_points() const { return _mute_point; }

        void set_solo_level (int32_t);

	PBD::Signal0<void> MutePointChanged;

	XMLNode& get_state();
	int set_state(const XMLNode&, int version);

  private:
	volatile MutePoint _mute_point;
        volatile bool      _self_muted;
        volatile uint32_t  _muted_by_others;
        volatile int32_t   _solo_level;
};

} // namespace ARDOUR

#endif /*__ardour_mute_master_h__ */
