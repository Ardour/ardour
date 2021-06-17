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

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
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
class LIBARDOUR_API SessionController
{
public:
	SessionController (Session* s)
	  : _session (s)
	{}

	/* Transport Control */

	/** Toggle looping.
	 *
	 * If the transport is currently looping, then looping is disabled so that
	 * playback will continue past the end of the loop.
	 */
	void loop_toggle ();

	/** Start looping a specific range.
	 *
	 * @param start The position of the first sample in the loop.
	 * @param end The position one past the last sample in the loop.
	 */
	void loop_location (samplepos_t start, samplepos_t end);

	void button_varispeed (bool fwd);

	/** Rewind the transport.
	 *
	 * If the transport is already rewinding, then the speed will be further
	 * increased.
	 */
	void rewind ();

	/** Fast-forward the transport.
	 *
	 * If the transport is already fast-forwarding, then the speed will be
	 * further increased.
	 */
	void ffwd ();

	/** Stop the transport if possible.
	 *
	 * This may not succeed if some other transport master is being followed.
	 */
	void transport_stop ();

	void transport_play (bool jump_back = false);

	/** Set a new transport speed.
	 *
	 * This should be used by callers who are varying transport speed but don't
	 * ever want to stop it.  If the speed is zero, then it will be adjusted to
	 * a very small positive value to prevent the transport from actually
	 * stopping.
	 */
	void set_transport_speed (double speed);

	void toggle_roll (bool with_abort               = true,
	                  bool roll_out_of_bounded_mode = true);

	void stop_forget ();

	/// Return the transport speed as a factor (so 1.0 = normal speed forward)
	double get_transport_speed () const;

	/// Return true if the transport is currently rolling
	bool transport_rolling () const;

	/// Return the current transport sample position
	samplepos_t transport_sample () const;

	/* Markers */

	/** Add a marker at the current audible sample with the given name.
	 *
	 * Creates one undo step.
	 */
	void add_marker (const std::string& name = std::string ());

	/** Remove any marker at the current audible sample.
	 *
	 * Creates one undo step if a marker was removed.
	 */
	bool remove_marker_at_playhead ();

	/* Locating */

	/** Move the transport to position zero.
	 *
	 * This moves to absolute time 0 regardless of the session range.
	 */
	void goto_zero ();

	/** Move the transport to the start of the session.
	 *
	 * Moves to absolute time 0 if the session range is empty.
	 */
	void goto_start (bool and_roll = false);

	/** Move the transport to the end of the session.
	 *
	 * Moves to absolute time 0 if the session range is empty.
	 */
	void goto_end ();

	/** Move the transport to a particular marker.
	 *
	 * Moves to the start of the session if no such marker is found.
	 */
	void goto_nth_marker (int n);

	void jump_by_seconds (double                     sec,
	                      LocateTransportDisposition ltd = RollIfAppropriate);

	void jump_by_bars (double                     bars,
	                   LocateTransportDisposition ltd = RollIfAppropriate);

	void jump_by_beats (double                     beats,
	                    LocateTransportDisposition ltd = RollIfAppropriate);

	void locate (samplepos_t sample, LocateTransportDisposition ltd);
	void locate (samplepos_t sample, bool);

	/** Move the transport to the first marker before the current position.
	 *
	 * Moves to the start of the session if no such marker is found.
	 */
	void prev_marker ();

	/** Move the transport to the first marker after the current position.
	 *
	 * Moves to the end of the session if no such marker is found.
	 */
	void next_marker ();

	/// Return true if the transport is in the process of locating
	bool locating () const;

	/// Return true if the transport is synced to an external time source
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

	/// Return true if recording is currently enabled
	bool get_record_enabled () const;

	/* Time */

	void timecode_time (samplepos_t where, Timecode::Time&);

private:
	Session* _session;
};

} // namespace ARDOUR

#endif /* __ardour_session_controller_h__ */
