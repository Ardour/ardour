/*
 * Copyright (C) 2006-2021 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef __ardour_session_controller_h__
#define __ardour_session_controller_h__

#include "ardour/types.h"
#include "control_protocol/visibility.h"
#include "temporal/time.h"

#include <cstdint>
#include <string>

namespace ARDOUR {

class Session;

/** A controller for a session used by UIs and control surfaces.
 *
 * This implements operations that manipulate a session which are common to any
 * kind of UI.  Application logic that isn't specific to any particular UI
 * should go here and be reused, so UIs do things consistently and correctly.
 *
 * This only interacts with Session (and the objects it contains) directly, not
 * with any UI facilities like actions or event loops.
 */
class LIBCONTROLCP_API SessionController
{
public:
	SessionController (Session* s)
	  : _session (s)
	{}

	/* Transport Control */

	void loop_toggle ();
	void loop_location (samplepos_t start, samplepos_t end);

	void button_varispeed (bool fwd);
	void rewind ();
	void ffwd ();
	void transport_stop ();
	void transport_play (bool jump_back = false);
	void set_transport_speed (double speed);

	void toggle_roll (bool roll_out_of_bounded_mode = true);
	void stop_forget ();

	double      get_transport_speed () const;
	bool        transport_rolling () const;
	samplepos_t transport_sample () const;

	/* Markers */

	void add_marker (const std::string& = std::string ());
	void remove_marker_at_playhead ();

	/* Locating */

	void goto_zero ();
	void goto_start (bool and_roll = false);
	void goto_end ();
	void goto_nth_marker (int n);

	void jump_by_seconds (double                     sec,
	                      LocateTransportDisposition ltd = RollIfAppropriate);

	void jump_by_bars (double                     bars,
	                   LocateTransportDisposition ltd = RollIfAppropriate);

	void jump_by_beats (double                     beats,
	                    LocateTransportDisposition ltd = RollIfAppropriate);

	void locate (samplepos_t sample, LocateTransportDisposition ltd);
	void locate (samplepos_t sample, bool);

	void prev_marker ();
	void next_marker ();

	bool locating () const;
	bool locked () const;

	/* State */

	void save_state ();

	/* Monitoring */

	void toggle_click ();
	void midi_panic ();

	void toggle_monitor_mute ();
	void toggle_monitor_dim ();
	void toggle_monitor_mono ();

	void cancel_all_solo ();

	/* Recording */

	void toggle_punch_in ();
	void toggle_punch_out ();

	void set_record_enable (bool yn);
	void rec_enable_toggle ();
	void toggle_all_rec_enables ();

	void all_tracks_rec_in ();
	void all_tracks_rec_out ();

	bool get_record_enabled () const;

	/* Time */

	void timecode_time (samplepos_t where, Timecode::Time&);

private:
	Session* _session;
};

} // namespace ARDOUR

#endif /* __ardour_session_controller_h__ */
