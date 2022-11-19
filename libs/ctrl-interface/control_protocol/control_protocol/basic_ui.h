/*
 * Copyright (C) 2006-2010 David Robillard <d@drobilla.net>
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

#ifndef __ardour_basic_ui_h__
#define __ardour_basic_ui_h__

#include <string>
#include <stdint.h>

#include "pbd/signals.h"

#include "temporal/timeline.h"

#include "ardour/types.h"
#include "ardour/presentation_info.h"

#include "temporal/time.h"

#include "control_protocol/visibility.h"

namespace ARDOUR {
	class Session;
	class SessionEvent;
	class Stripable;
	class Trigger;
}

class LIBCONTROLCP_API BasicUI {
  public:
	BasicUI (ARDOUR::Session&);
	virtual ~BasicUI ();

	void add_marker (const std::string& = std::string());
	void remove_marker_at_playhead ();

//	void mark_in();
//	void mark_out();

	void register_thread (std::string name);

	/* transport control */

	void loop_toggle ();
	void loop_location (Temporal::timepos_t const & start, Temporal::timepos_t const & end);
	void access_action ( std::string action_path );
	static PBD::Signal2<void,std::string,std::string> AccessAction;
	void goto_zero ();
	void goto_start (bool and_roll = false);
	void goto_end ();
	void button_varispeed (bool fwd);
	void rewind ();
	void ffwd ();
	void transport_stop ();
	void transport_play (bool jump_back = false);
	void set_transport_speed (double speed);

	double get_transport_speed () const;
	double transport_rolling () const;

	void jump_by_seconds (double sec, ARDOUR::LocateTransportDisposition ltd = ARDOUR::RollIfAppropriate);
	void jump_by_bars (int bars, ARDOUR::LocateTransportDisposition ltd = ARDOUR::RollIfAppropriate);
	void jump_by_beats (int beats, ARDOUR::LocateTransportDisposition ltd = ARDOUR::RollIfAppropriate);

	ARDOUR::samplepos_t transport_sample ();
	void locate (ARDOUR::samplepos_t sample, ARDOUR::LocateTransportDisposition ltd);
	void locate (ARDOUR::samplepos_t sample, bool);
	bool locating ();
	bool locked ();

	void save_state ();
	void prev_marker ();
	void next_marker ();
	void undo ();
	void redo ();
	void toggle_punch_in ();
	void toggle_punch_out ();

	void mark_in();
	void mark_out();

	void toggle_click();
	void midi_panic();

	void trigger_cue_row (int cue);
	void trigger_stop_all (bool stop_now = false);

	void store_mixer_scene (int scn);
	void apply_mixer_scene (int scn);

	void toggle_monitor_mute();
	void toggle_monitor_dim();
	void toggle_monitor_mono();

	void cancel_all_solo ();

	void quick_snapshot_stay ();
	void quick_snapshot_switch ();

	void toggle_roll(bool with_abort, bool roll_out_of_bounded_mode);  //this provides the same operation as the "spacebar", it's a lot smarter than "play".

	void stop_forget();

	void set_punch_range();
	void set_loop_range();
	void set_session_range();

	void set_record_enable (bool yn);
	bool get_record_enabled ();

	//editor visibility stuff  (why do we have to make explicit numbers here?  because "gui actions" don't accept args
	void fit_1_track();
	void fit_2_tracks();
	void fit_4_tracks();
	void fit_8_tracks();
	void fit_16_tracks();
	void fit_32_tracks();
	void fit_all_tracks();
	void zoom_10_ms();
	void zoom_100_ms();
	void zoom_1_sec();
	void zoom_10_sec();
	void zoom_1_min();
	void zoom_5_min();
	void zoom_10_min();
	void zoom_to_session();
	void temporal_zoom_in();
	void temporal_zoom_out();

	void scroll_up_1_track();
	void scroll_dn_1_track();
	void scroll_up_1_page();
	void scroll_dn_1_page();

	void rec_enable_toggle ();
	void toggle_all_rec_enables ();

	void all_tracks_rec_in ();
	void all_tracks_rec_out ();

	void goto_nth_marker (int n);

	ARDOUR::samplecnt_t timecode_frames_per_hour ();

	void timecode_time (samplepos_t where, Timecode::Time&);
	void timecode_to_sample (Timecode::Time& timecode, samplepos_t & sample, bool use_offset, bool use_subframes) const;
	void sample_to_timecode (samplepos_t sample, Timecode::Time& timecode, bool use_offset, bool use_subframes) const;

	bool stop_button_onoff() const;
	bool play_button_onoff() const;
	bool ffwd_button_onoff() const;
	bool rewind_button_onoff() const;
	bool loop_button_onoff() const;

	/* These functions access Triggers in the order they are displayed on the Cue page, WITH an optional bank offset
	 Use this for a launchpad-style NxM (route x row) matrix that maps directly to the Cue page layout.
	 Trigger banking is separate from 'route' banking implemented by a fader surface.
	 To match a fader/mute/solo to the Trigger banking, the tentative plan is:
	    request trigger-tracks-only to be displayed on the surface
		bank the faders using the offset reported here
	 */
	void tbank_set_size (int route_width, int row_height);
	void tbank_step_routes (int step_size);
	void tbank_step_rows (int step_size);
	float trigger_progress_at (int x);  /* 0..1   or -1 for not playing; */
	struct TriggerDisplay {
		int state;
		TriggerDisplay () {
			state = -1;     /*  -1=empty;  0=stopped;  1=playing */  /*potentially extend to include */
			//potentially name, color, launch style, follow action(s) etc
		}
	};
	TriggerDisplay trigger_display_at (int x, int y);
	void bang_trigger_at (int x, int y);
	void unbang_trigger_at (int x, int y);

	/* it would be nice to use TriggerPtr here but that implies including ardour/triggerbox.h */
	boost::shared_ptr<ARDOUR::Trigger> find_trigger (int x, int y);

  protected:
	BasicUI ();
	ARDOUR::Session* session;

	int _tbank_route_width, _tbank_row_height;
	int _tbank_start_route, _tbank_start_row;
};

#endif /* __ardour_basic_ui_h__ */
