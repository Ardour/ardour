/*
    Copyright (C) 2006 Paul Davis

    This program is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser
    General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your
    option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __ardour_basic_ui_h__
#define __ardour_basic_ui_h__

#include <string>
#include <stdint.h>

#include "pbd/signals.h"

#include "ardour/types.h"
#include "ardour/presentation_info.h"

#include "timecode/time.h"

#include "control_protocol/visibility.h"

namespace ARDOUR {
	class Session;
	class SessionEvent;
	class Stripable;
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
	void loop_location (framepos_t start, framepos_t end);
	void access_action ( std::string action_path );
	static PBD::Signal2<void,std::string,std::string> AccessAction;
	void goto_zero ();
	void goto_start (bool and_roll = false);
	void goto_end ();
	void rewind ();
	void ffwd ();
	void transport_stop ();
	void transport_play (bool jump_back = false);
	void set_transport_speed (double speed);
	double get_transport_speed ();

	void jump_by_seconds( double sec );
	void jump_by_bars(double bars);

	ARDOUR::framepos_t transport_frame ();
	void locate (ARDOUR::framepos_t frame, bool play = false);
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

	void toggle_monitor_mute();
	void toggle_monitor_dim();
	void toggle_monitor_mono();

	void cancel_all_solo ();

	void quick_snapshot_stay ();
	void quick_snapshot_switch ();

	void toggle_roll();  //this provides the same operation as the "spacebar", it's a lot smarter than "play".

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

	ARDOUR::framecnt_t timecode_frames_per_hour ();

	void timecode_time (framepos_t where, Timecode::Time&);
	void timecode_to_sample (Timecode::Time& timecode, framepos_t & sample, bool use_offset, bool use_subframes) const;
	void sample_to_timecode (framepos_t sample, Timecode::Time& timecode, bool use_offset, bool use_subframes) const;

  protected:
	BasicUI ();
	ARDOUR::Session* session;
};

#endif /* __ardour_basic_ui_h__ */
