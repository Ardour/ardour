/*
 * Copyright (C) 2017 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef ardour_us2400_control_protocol_h
#define ardour_us2400_control_protocol_h

#include <vector>
#include <map>
#include <list>
#include <set>

#include <sys/time.h>
#include <pthread.h>
#include <boost/smart_ptr.hpp>

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"
#include "midi++/types.h"
#include "ardour/types.h"
#include "control_protocol/control_protocol.h"

#include "types.h"
#include "midi_byte_array.h"
#include "controls.h"
#include "jog_wheel.h"
#include "timer.h"
#include "device_info.h"
#include "device_profile.h"

namespace ARDOUR {
	class AutomationControl;
	class Port;
}

namespace MIDI {
	class Port;
}

namespace ArdourSurface {

namespace US2400 {
	class Surface;
	class Control;
	class SurfacePort;
	class Button;
	class Strip;
}

struct US2400ControlUIRequest : public BaseUI::BaseRequestObject {
public:
	US2400ControlUIRequest () {}
	~US2400ControlUIRequest () {}
};

class US2400Protocol
	: public ARDOUR::ControlProtocol
	, public AbstractUI<US2400ControlUIRequest>
{
  public:
	static const int MODIFIER_OPTION;
	static const int MODIFIER_CONTROL;
	static const int MODIFIER_SHIFT;
	static const int MODIFIER_CMDALT;
	static const int MODIFIER_ZOOM;
	static const int MODIFIER_SCRUB;
	static const int MODIFIER_MARKER;
	static const int MODIFIER_DROP;  //US2400 replaces MODIFIER_NUDGE which is unused
	static const int MAIN_MODIFIER_MASK;

	enum ViewMode {
		Mixer,
		Busses,
	};

	enum SubViewMode {
		None,
		TrackView,
	};

	US2400Protocol(ARDOUR::Session &);
	virtual ~US2400Protocol();

	static US2400Protocol* instance() { return _instance; }

	const US2400::DeviceInfo& device_info() const { return _device_info; }
	US2400::DeviceProfile& device_profile() { return _device_profile; }

	PBD::Signal0<void> DeviceChanged;
	PBD::Signal1<void,boost::shared_ptr<US2400::Surface> > ConnectionChange;

        void device_ready ();

	int set_active (bool yn);
	int  set_device (const std::string&, bool force);
        void set_profile (const std::string&);

	ViewMode view_mode () const { return _view_mode; }
	SubViewMode subview_mode () const { return _subview_mode; }
	static bool subview_mode_would_be_ok (SubViewMode, boost::shared_ptr<ARDOUR::Stripable>);
	boost::shared_ptr<ARDOUR::Stripable> subview_stripable() const;
	bool zoom_mode () const { return modifier_state() & MODIFIER_ZOOM; }
	bool     metering_active () const { return _metering_active; }

	bool is_track (boost::shared_ptr<ARDOUR::Stripable>) const;
	bool is_audio_track (boost::shared_ptr<ARDOUR::Stripable>) const;
	bool is_midi_track (boost::shared_ptr<ARDOUR::Stripable>) const;
	bool is_mapped (boost::shared_ptr<ARDOUR::Stripable>) const;
	boost::shared_ptr<ARDOUR::Stripable> first_selected_stripable () const;

	void check_fader_automation_state ();
	void update_fader_automation_state ();
	void set_automation_state (ARDOUR::AutoState);

	void set_view_mode (ViewMode);
	int set_subview_mode (SubViewMode, boost::shared_ptr<ARDOUR::Stripable>);
	void display_view_mode ();

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	/* Note: because Mackie control is inherently a duplex protocol,
	   we do not implement get/set_feedback() since this aspect of
	   support for the protocol is not optional.
	*/

	static bool probe();
	static void* request_factory (uint32_t);

	mutable Glib::Threads::Mutex surfaces_lock;
	typedef std::list<boost::shared_ptr<US2400::Surface> > Surfaces;
	Surfaces surfaces;

	boost::shared_ptr<US2400::Surface> get_surface_by_raw_pointer (void*) const;
	boost::shared_ptr<US2400::Surface> nth_surface (uint32_t) const;

	uint32_t global_index (US2400::Strip&);
	uint32_t global_index_locked (US2400::Strip&);

	std::list<boost::shared_ptr<ARDOUR::Bundle> > bundles ();

	void set_master_on_surface_strip (uint32_t surface, uint32_t strip);
	void set_monitor_on_surface_strip (uint32_t surface, uint32_t strip);

	uint32_t n_strips (bool with_locked_strips = true) const;

	bool has_editor () const { return true; }
	void* get_gui () const;
	void tear_down_gui ();

	void handle_button_event (US2400::Surface&, US2400::Button& button, US2400::ButtonState);

	void notify_subview_stripable_deleted ();
	void notify_stripable_removed ();
	void notify_routes_added (ARDOUR::RouteList &);
	void notify_vca_added (ARDOUR::VCAList &);

	void notify_presentation_info_changed(PBD::PropertyChange const &);

	void recalibrate_faders ();
	void toggle_backlight ();
	void set_touch_sensitivity (int);

	/// rebuild the current bank. Called on route or vca added/removed and
	/// presentation info changed.
	void refresh_current_bank();

	// button-related signals
	void notify_record_state_changed();
	void notify_transport_state_changed();
	void notify_loop_state_changed();
	void notify_metering_state_changed();
	// mainly to pick up punch-in and punch-out
	void notify_parameter_changed(std::string const &);
	void notify_solo_active_changed(bool);

	/// Turn timecode on and beats off, or vice versa, depending
	/// on state of _timecode_type
	void update_timecode_beats_led();

	/// this is called to generate the midi to send in response to a button press.
	void update_led(US2400::Surface&, US2400::Button & button, US2400::LedState);

	void update_global_button (int id, US2400::LedState);
	void update_global_led (int id, US2400::LedState);

	ARDOUR::Session & get_session() { return *session; }
	samplepos_t transport_sample() const;

	int modifier_state() const { return _modifier_state; }
	int main_modifier_state() const { return _modifier_state & MAIN_MODIFIER_MASK; }

	typedef std::list<boost::shared_ptr<ARDOUR::AutomationControl> > ControlList;

	void add_down_button (ARDOUR::AutomationType, int surface, int strip);
	void remove_down_button (ARDOUR::AutomationType, int surface, int strip);
	ControlList down_controls (ARDOUR::AutomationType, uint32_t pressed);

	void add_down_select_button (int surface, int strip);
	void remove_down_select_button (int surface, int strip);
	void select_range (uint32_t pressed);

  protected:
	// shut down the surface
	void close();

	// This sets up the notifications and sets the
	// controls to the correct values
	void update_surfaces();

	// connects global (not strip) signals from the Session to here
	// so the surface can be notified of changes from the other UIs.
	void connect_session_signals();

	// set all controls to their zero position
	void zero_all();

	/**
	   Fetch the set of Stripables to be considered for control by the
	   surface. Excluding master, hidden and control routes, and inactive routes
	*/
	typedef std::vector<boost::shared_ptr<ARDOUR::Stripable> > Sorted;
	Sorted get_sorted_stripables();

	// bank switching
	int switch_banks (uint32_t first_remote_id, bool force = false);
	void prev_track ();
	void next_track ();

	void do_request (US2400ControlUIRequest*);
	int stop ();

	void thread_init ();

	bool stripable_is_locked_to_strip (boost::shared_ptr<ARDOUR::Stripable>) const;

  private:

	struct ButtonHandlers {
	    US2400::LedState (US2400Protocol::*press) (US2400::Button&);
	    US2400::LedState (US2400Protocol::*release) (US2400::Button&);

	    ButtonHandlers (US2400::LedState (US2400Protocol::*p) (US2400::Button&),
			    US2400::LedState (US2400Protocol::*r) (US2400::Button&))
	    : press (p)
	    , release (r) {}
	};

	typedef std::map<US2400::Button::ID,ButtonHandlers> ButtonMap;

	static US2400Protocol* _instance;

	bool profile_exists (std::string const&) const;

	US2400::DeviceInfo       _device_info;
	US2400::DeviceProfile    _device_profile;
	sigc::connection          periodic_connection;
	sigc::connection          redisplay_connection;
	sigc::connection          hui_connection;
	uint32_t                 _current_initial_bank;
	PBD::ScopedConnectionList audio_engine_connections;
	PBD::ScopedConnectionList session_connections;
	PBD::ScopedConnectionList stripable_connections;
	PBD::ScopedConnectionList subview_stripable_connections;
	PBD::ScopedConnectionList gui_connections;
	// timer for two quick marker left presses
	US2400::Timer            _frm_left_last;
	// last written timecode string
	std::string              _timecode_last;
	samplepos_t				 _sample_last;
	// Which timecode are we displaying? BBT or Timecode
	ARDOUR::AnyTime::Type    _timecode_type;
	// Bundle to represent our input ports
	boost::shared_ptr<ARDOUR::Bundle> _input_bundle;
	// Bundle to represent our output ports
	boost::shared_ptr<ARDOUR::Bundle> _output_bundle;
	void*                    _gui;
	bool                     _scrub_mode;
	ViewMode                 _view_mode;
	SubViewMode              _subview_mode;
	boost::shared_ptr<ARDOUR::Stripable> _subview_stripable;
	int                      _modifier_state;
	ButtonMap                 button_map;
	bool                     _metering_active;
	bool                     _initialized;
	XMLNode*                 configuration_state;
	int                      state_version;
	int                      _last_bank[9];
	bool                     marker_modifier_consumed_by_button;
	bool                     nudge_modifier_consumed_by_button;

	boost::shared_ptr<ArdourSurface::US2400::Surface>	_master_surface;

	int create_surfaces ();
	bool periodic();
	bool redisplay();
	bool redisplay_subview_mode ();
	bool hui_heartbeat ();
	void build_gui ();
	bool midi_input_handler (Glib::IOCondition ioc, MIDI::Port* port);
	void clear_ports ();
	void clear_surfaces ();
	void force_special_stripable_to_strip (boost::shared_ptr<ARDOUR::Stripable> r, uint32_t surface, uint32_t strip_number);
	void build_button_map ();
	void stripable_selection_changed ();
       void initialize ();
        int set_device_info (const std::string& device_name);
	void update_configuration_state ();

	/* MIDI port connection management */

	PBD::ScopedConnection port_connection;
	void connection_handler (boost::weak_ptr<ARDOUR::Port>, std::string name1, boost::weak_ptr<ARDOUR::Port>, std::string name2, bool);

	/* BUTTON HANDLING */

	typedef std::set<uint32_t> DownButtonList;
	typedef std::map<ARDOUR::AutomationType,DownButtonList> DownButtonMap;
	DownButtonMap  _down_buttons;
	DownButtonList _down_select_buttons;

	void pull_stripable_range (DownButtonList&, ARDOUR::StripableList&, uint32_t pressed);

	/* implemented button handlers */
	US2400::LedState stop_press(US2400::Button &);
	US2400::LedState stop_release(US2400::Button &);
	US2400::LedState play_press(US2400::Button &);
	US2400::LedState play_release(US2400::Button &);
	US2400::LedState record_press(US2400::Button &);
	US2400::LedState record_release(US2400::Button &);
	US2400::LedState loop_press(US2400::Button &);
	US2400::LedState loop_release(US2400::Button &);
	US2400::LedState rewind_press(US2400::Button & button);
	US2400::LedState rewind_release(US2400::Button & button);
	US2400::LedState ffwd_press(US2400::Button & button);
	US2400::LedState ffwd_release(US2400::Button & button);
	US2400::LedState cursor_up_press (US2400::Button &);
	US2400::LedState cursor_up_release (US2400::Button &);
	US2400::LedState cursor_down_press (US2400::Button &);
	US2400::LedState cursor_down_release (US2400::Button &);
	US2400::LedState cursor_left_press (US2400::Button &);
	US2400::LedState cursor_left_release (US2400::Button &);
	US2400::LedState cursor_right_press (US2400::Button &);
	US2400::LedState cursor_right_release (US2400::Button &);
	US2400::LedState left_press(US2400::Button &);
	US2400::LedState left_release(US2400::Button &);
	US2400::LedState right_press(US2400::Button &);
	US2400::LedState right_release(US2400::Button &);
	US2400::LedState channel_left_press(US2400::Button &);
	US2400::LedState channel_left_release(US2400::Button &);
	US2400::LedState channel_right_press(US2400::Button &);
	US2400::LedState channel_right_release(US2400::Button &);
	US2400::LedState marker_press(US2400::Button &);
	US2400::LedState marker_release(US2400::Button &);
	US2400::LedState save_press(US2400::Button &);
	US2400::LedState save_release(US2400::Button &);
	US2400::LedState timecode_beats_press(US2400::Button &);
	US2400::LedState timecode_beats_release(US2400::Button &);
	US2400::LedState zoom_press(US2400::Button &);
	US2400::LedState zoom_release(US2400::Button &);
	US2400::LedState scrub_press(US2400::Button &);
	US2400::LedState scrub_release(US2400::Button &);
	US2400::LedState undo_press (US2400::Button &);
	US2400::LedState undo_release (US2400::Button &);
	US2400::LedState shift_press (US2400::Button &);
	US2400::LedState shift_release (US2400::Button &);
	US2400::LedState option_press (US2400::Button &);
	US2400::LedState option_release (US2400::Button &);
	US2400::LedState control_press (US2400::Button &);
	US2400::LedState control_release (US2400::Button &);
	US2400::LedState cmd_alt_press (US2400::Button &);
	US2400::LedState cmd_alt_release (US2400::Button &);

	US2400::LedState pan_press (US2400::Button &);
	US2400::LedState pan_release (US2400::Button &);
	US2400::LedState plugin_press (US2400::Button &);
	US2400::LedState plugin_release (US2400::Button &);
	US2400::LedState eq_press (US2400::Button &);
	US2400::LedState eq_release (US2400::Button &);
	US2400::LedState dyn_press (US2400::Button &);
	US2400::LedState dyn_release (US2400::Button &);
	US2400::LedState flip_press (US2400::Button &);
	US2400::LedState flip_release (US2400::Button &);
	US2400::LedState mstr_press (US2400::Button &);
	US2400::LedState mstr_release (US2400::Button &);
	US2400::LedState name_value_press (US2400::Button &);
	US2400::LedState name_value_release (US2400::Button &);
//	US2400::LedState F1_press (US2400::Button &);
//	US2400::LedState F1_release (US2400::Button &);
//	US2400::LedState F2_press (US2400::Button &);
//	US2400::LedState F2_release (US2400::Button &);
//	US2400::LedState F3_press (US2400::Button &);
//	US2400::LedState F3_release (US2400::Button &);
//	US2400::LedState F4_press (US2400::Button &);
//	US2400::LedState F4_release (US2400::Button &);
//	US2400::LedState F5_press (US2400::Button &);
//	US2400::LedState F5_release (US2400::Button &);
//	US2400::LedState F6_press (US2400::Button &);
//	US2400::LedState F6_release (US2400::Button &);
//	US2400::LedState F7_press (US2400::Button &);
//	US2400::LedState F7_release (US2400::Button &);
//	US2400::LedState F8_press (US2400::Button &);
//	US2400::LedState F8_release (US2400::Button &);
	US2400::LedState touch_press (US2400::Button &);
	US2400::LedState touch_release (US2400::Button &);
	US2400::LedState enter_press (US2400::Button &);
	US2400::LedState enter_release (US2400::Button &);
	US2400::LedState cancel_press (US2400::Button &);
	US2400::LedState cancel_release (US2400::Button &);
	US2400::LedState user_a_press (US2400::Button &);
	US2400::LedState user_a_release (US2400::Button &);
	US2400::LedState user_b_press (US2400::Button &);
	US2400::LedState user_b_release (US2400::Button &);
	US2400::LedState fader_touch_press (US2400::Button &);
	US2400::LedState fader_touch_release (US2400::Button &);
	US2400::LedState master_fader_touch_press (US2400::Button &);
	US2400::LedState master_fader_touch_release (US2400::Button &);

	US2400::LedState read_press (US2400::Button&);
	US2400::LedState read_release (US2400::Button&);
	US2400::LedState write_press (US2400::Button&);
	US2400::LedState write_release (US2400::Button&);
	US2400::LedState clearsolo_press (US2400::Button&);
	US2400::LedState clearsolo_release (US2400::Button&);
	US2400::LedState track_press (US2400::Button&);
	US2400::LedState track_release (US2400::Button&);
	US2400::LedState send_press (US2400::Button&);
	US2400::LedState send_release (US2400::Button&);
	US2400::LedState miditracks_press (US2400::Button&);
	US2400::LedState miditracks_release (US2400::Button&);
	US2400::LedState inputs_press (US2400::Button&);
	US2400::LedState inputs_release (US2400::Button&);
	US2400::LedState audiotracks_press (US2400::Button&);
	US2400::LedState audiotracks_release (US2400::Button&);
	US2400::LedState audioinstruments_press (US2400::Button&);
	US2400::LedState audioinstruments_release (US2400::Button&);
	US2400::LedState aux_press (US2400::Button&);
	US2400::LedState aux_release (US2400::Button&);
	US2400::LedState busses_press (US2400::Button&);
	US2400::LedState busses_release (US2400::Button&);
	US2400::LedState outputs_press (US2400::Button&);
	US2400::LedState outputs_release (US2400::Button&);
	US2400::LedState user_press (US2400::Button&);
	US2400::LedState user_release (US2400::Button&);
	US2400::LedState trim_press (US2400::Button&);
	US2400::LedState trim_release (US2400::Button&);
	US2400::LedState latch_press (US2400::Button&);
	US2400::LedState latch_release (US2400::Button&);
	US2400::LedState grp_press (US2400::Button&);
	US2400::LedState grp_release (US2400::Button&);
	US2400::LedState nudge_press (US2400::Button&);
	US2400::LedState nudge_release (US2400::Button&);
	US2400::LedState drop_press (US2400::Button&);
	US2400::LedState drop_release (US2400::Button&);
	US2400::LedState replace_press (US2400::Button&);
	US2400::LedState replace_release (US2400::Button&);
	US2400::LedState click_press (US2400::Button&);
	US2400::LedState click_release (US2400::Button&);
	US2400::LedState view_press (US2400::Button&);
	US2400::LedState view_release (US2400::Button&);

	US2400::LedState bank_release (US2400::Button&, uint32_t bank_num);
};

} // namespace

#endif // ardour_us2400_control_protocol_h
