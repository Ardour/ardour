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

#include "ardour/presentation_info.h"
#include "ardour/types.h"
#include "control_protocol/visibility.h"
#include "temporal/timeline.h"
#include "pbd/controllable.h"
#include "pbd/signals.h"
#include "temporal/time.h"

#include <boost/smart_ptr.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace ARDOUR {

class Amp;
class Locations;
class RouteGroup;
class Session;
class SessionConfiguration;
class Stripable;

class LIBCONTROLCP_API BasicUI
{
public:
	BasicUI (Session&);
	virtual ~BasicUI ();

	void add_marker (const std::string& = std::string ());
	void remove_marker_at_playhead ();

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
	bool   transport_rolling () const;
	bool   transport_stopped_or_stopping () const;
	bool   get_play_loop () const;

	void jump_by_seconds (double sec, LocateTransportDisposition ltd = RollIfAppropriate);
	void jump_by_bars (int bars, LocateTransportDisposition ltd = RollIfAppropriate);
	void jump_by_beats (int beats, LocateTransportDisposition ltd = RollIfAppropriate);

	samplepos_t transport_sample () const;
	void        locate (samplepos_t                sample,
	                    LocateTransportDisposition ltd = RollIfAppropriate);
	void        locate (samplepos_t sample, bool);
	bool        locating ();
	bool        locked ();

	samplepos_t engine_sample_time ();

	void save_state ();
	void prev_marker ();
	void next_marker ();
	void undo ();
	void redo ();
	void toggle_punch_in ();
	void toggle_punch_out ();

	void mark_in ();
	void mark_out ();

	void toggle_click ();
	void midi_panic ();

	void toggle_monitor_mute ();
	void toggle_monitor_dim ();
	void toggle_monitor_mono ();

	std::vector<boost::weak_ptr<AutomationControl>> cancel_all_mute ();

	void cancel_all_solo ();

	void quick_snapshot_stay ();
	void quick_snapshot_switch ();

	void toggle_roll (bool roll_out_of_bounded_mode =
	                    true); // this provides the same operation as the
	                           // "spacebar", it's a lot smarter than "play".

	void stop_forget ();

	void set_punch_range ();
	void set_loop_range ();
	void set_session_range ();

	void        set_record_enable (bool yn);
	bool        get_record_enabled ();
	RecordState record_status () const;

	bool have_rec_enabled_track () const;

	// editor visibility stuff  (why do we have to make explicit numbers here?
	// because "gui actions" don't accept args
	void fit_1_track ();
	void fit_2_tracks ();
	void fit_4_tracks ();
	void fit_8_tracks ();
	void fit_16_tracks ();
	void fit_32_tracks ();
	void fit_all_tracks ();
	void zoom_10_ms ();
	void zoom_100_ms ();
	void zoom_1_sec ();
	void zoom_10_sec ();
	void zoom_1_min ();
	void zoom_5_min ();
	void zoom_10_min ();
	void zoom_to_session ();
	void temporal_zoom_in ();
	void temporal_zoom_out ();

	void scroll_up_1_track ();
	void scroll_dn_1_track ();
	void scroll_up_1_page ();
	void scroll_dn_1_page ();

	void rec_enable_toggle ();
	void toggle_all_rec_enables ();

	void all_tracks_rec_in ();
	void all_tracks_rec_out ();

	void goto_nth_marker (int n);

	samplecnt_t timecode_frames_per_hour ();

	void timecode_time (samplepos_t where, Timecode::Time&);
	void timecode_to_sample (Timecode::Time& timecode,
	                         samplepos_t&    sample,
	                         bool            use_offset,
	                         bool            use_subframes) const;
	void sample_to_timecode (samplepos_t     sample,
	                         Timecode::Time& timecode,
	                         bool            use_offset,
	                         bool            use_subframes) const;

	bool stop_button_onoff () const;
	bool play_button_onoff () const;
	bool ffwd_button_onoff () const;
	bool rewind_button_onoff () const;
	bool loop_button_onoff () const;

	/* Naming */

	std::string make_port_name_non_relative (const std::string& name) const;

	/* Configuration */

	const SessionConfiguration& config () const;
	SessionConfiguration&       config ();

	/* Control-based methods */

	void set_controls (boost::shared_ptr<ControlList>,
	                   double val,
	                   PBD::Controllable::GroupControlDisposition);

	void set_control (boost::shared_ptr<AutomationControl>,
	                  double val,
	                  PBD::Controllable::GroupControlDisposition);

	/* Monitor/Master Out */

	boost::shared_ptr<Stripable> monitor_out () const;
	boost::shared_ptr<Stripable> master_out () const;

	/* Clicking */

	boost::shared_ptr<Amp> click_gain();

	/* Locations */

	const Locations* locations () const;
	Locations*       locations ();

	/* Signals */

	PBD::Signal0<void>&                           BundleAddedOrRemoved ();
	PBD::Signal0<void>&                           DirtyChanged ();
	PBD::Signal2<void, std::string, std::string>& Exported ();
	PBD::Signal1<void, bool>&                     SoloActive ();
	PBD::Signal0<void>&                           SoloChanged ();
	PBD::Signal0<void>&                           MuteChanged ();
	PBD::Signal0<void>&                           RecordStateChanged ();
	PBD::Signal1<void, RouteList&>&               RouteAdded ();
	PBD::Signal1<void, RouteGroup*>&              RouteGroupPropertyChanged ();
	PBD::Signal1<void, std::string>&              StateSaved ();
	PBD::Signal0<void>&                           TransportLooped ();
	PBD::Signal0<void>&                           TransportStateChange ();

protected:
	Session* _session;
};

} // namespace ARDOUR

#endif /* __ardour_basic_ui_h__ */
