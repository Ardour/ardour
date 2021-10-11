/*
 * Copyright (C) 2006-2007 John Anderson
 * Copyright (C) 2007-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2016 Len Ovens <len@ovenwerks.net>
 * Copyright (C) 2016-2018 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef ardour_mackie_control_protocol_h
#define ardour_mackie_control_protocol_h

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
#include "subview.h"

namespace ARDOUR {
	class AutomationControl;
	class Port;
}

namespace MIDI {
	class Port;
}

namespace ArdourSurface {

namespace Mackie {
	class Surface;
	class Control;
	class SurfacePort;
	class Button;
	class Strip;
}

gboolean ipmidi_input_handler (GIOChannel*, GIOCondition condition, void *data);

struct MackieControlUIRequest : public BaseUI::BaseRequestObject {
public:
	MackieControlUIRequest () {}
	~MackieControlUIRequest () {}
};

class MackieControlProtocol
	: public ARDOUR::ControlProtocol
	, public AbstractUI<MackieControlUIRequest>
{
  public:
	static const int MODIFIER_OPTION;
	static const int MODIFIER_CONTROL;
	static const int MODIFIER_SHIFT;
	static const int MODIFIER_CMDALT;
	static const int MODIFIER_ZOOM;
	static const int MODIFIER_SCRUB;
	static const int MODIFIER_MARKER;
	static const int MODIFIER_NUDGE;
	static const int MAIN_MODIFIER_MASK;

	enum ViewMode {
		Mixer,
		AudioTracks,
		MidiTracks,
		Busses,
		Auxes,
		Selected,
		Hidden,
		Plugins,
	};

	enum FlipMode {
		Normal, /* fader controls primary, vpot controls secondary */
		Mirror, /* fader + vpot control secondary */
		Swap,   /* fader controls secondary, vpot controls primary */
		Zero,   /* fader controls primary, but doesn't move, vpot controls secondary */
	};

	MackieControlProtocol(ARDOUR::Session &);
	virtual ~MackieControlProtocol();

	static MackieControlProtocol* instance() { return _instance; }

	const Mackie::DeviceInfo& device_info() const { return _device_info; }
	Mackie::DeviceProfile& device_profile() { return _device_profile; }

	PBD::Signal0<void> DeviceChanged;
	PBD::Signal1<void,boost::shared_ptr<Mackie::Surface> > ConnectionChange;

        void device_ready ();

	int set_active (bool yn);
	int  set_device (const std::string&, bool force);
        void set_profile (const std::string&);

	FlipMode flip_mode () const { return _flip_mode; }
	ViewMode view_mode () const { return _view_mode; }
	boost::shared_ptr<Mackie::Subview> subview() { return _subview; }
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
	bool set_subview_mode (Mackie::Subview::Mode, boost::shared_ptr<ARDOUR::Stripable>);
	bool redisplay_subview_mode ();
	void set_flip_mode (FlipMode);
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
	typedef std::list<boost::shared_ptr<Mackie::Surface> > Surfaces;
	Surfaces surfaces;

	boost::shared_ptr<Mackie::Surface> get_surface_by_raw_pointer (void*) const;
	boost::shared_ptr<Mackie::Surface> nth_surface (uint32_t) const;

	uint32_t global_index (Mackie::Strip&);
	uint32_t global_index_locked (Mackie::Strip&);

	std::list<boost::shared_ptr<ARDOUR::Bundle> > bundles ();

	void set_master_on_surface_strip (uint32_t surface, uint32_t strip);
	void set_monitor_on_surface_strip (uint32_t surface, uint32_t strip);

	uint32_t n_strips (bool with_locked_strips = true) const;

	bool has_editor () const { return true; }
	void* get_gui () const;
	void tear_down_gui ();

	void handle_button_event (Mackie::Surface&, Mackie::Button& button, Mackie::ButtonState);

	void notify_subview_stripable_deleted ();
	void notify_routes_added (ARDOUR::RouteList &);
	void notify_vca_added (ARDOUR::VCAList &);
	void notify_monitor_added_or_removed ();

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
	void update_led(Mackie::Surface&, Mackie::Button & button, Mackie::LedState);

	void update_global_button (int id, Mackie::LedState);
	void update_global_led (int id, Mackie::LedState);

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

	int16_t ipmidi_base() const { return _ipmidi_base; }
	void    set_ipmidi_base (int16_t);

	void ping_devices ();

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

	// also called from poll_automation to update timecode display
	void update_timecode_display();

	std::string format_bbt_timecode (ARDOUR::samplepos_t now_sample);
	std::string format_timecode_timecode (ARDOUR::samplepos_t now_sample);

	void do_request (MackieControlUIRequest*);
	int stop ();

	void thread_init ();

	bool stripable_is_locked_to_strip (boost::shared_ptr<ARDOUR::Stripable>) const;

  private:

	struct ButtonHandlers {
	    Mackie::LedState (MackieControlProtocol::*press) (Mackie::Button&);
	    Mackie::LedState (MackieControlProtocol::*release) (Mackie::Button&);

	    ButtonHandlers (Mackie::LedState (MackieControlProtocol::*p) (Mackie::Button&),
			    Mackie::LedState (MackieControlProtocol::*r) (Mackie::Button&))
	    : press (p)
	    , release (r) {}
	};

	typedef std::map<Mackie::Button::ID,ButtonHandlers> ButtonMap;

	static MackieControlProtocol* _instance;

	bool profile_exists (std::string const&) const;

	Mackie::DeviceInfo       _device_info;
	Mackie::DeviceProfile    _device_profile;
	sigc::connection          periodic_connection;
	sigc::connection          redisplay_connection;
	sigc::connection          hui_connection;
	uint32_t                 _current_initial_bank;
	PBD::ScopedConnectionList audio_engine_connections;
	PBD::ScopedConnectionList session_connections;
	PBD::ScopedConnectionList stripable_connections;
	PBD::ScopedConnectionList gui_connections;
	PBD::ScopedConnectionList fader_automation_connections;
	// timer for two quick marker left presses
	Mackie::Timer            _frm_left_last;
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
	FlipMode                 _flip_mode;
	ViewMode                 _view_mode;
	boost::shared_ptr<Mackie::Subview> _subview;
	int                      _current_selected_track;
	int                      _modifier_state;
	ButtonMap                 button_map;
	int16_t                  _ipmidi_base;
	bool                      needs_ipmidi_restart;
	bool                     _metering_active;
	bool                     _initialized;
	XMLNode*                 configuration_state;
	int                      state_version;
	int                      _last_bank[9];
	bool                     marker_modifier_consumed_by_button;
	bool                     nudge_modifier_consumed_by_button;

	boost::shared_ptr<ArdourSurface::Mackie::Surface>	_master_surface;

        struct ipMIDIHandler {
                MackieControlProtocol* mcp;
                MIDI::Port* port;
        };
        friend struct ipMIDIHandler; /* is this necessary */
	friend gboolean ArdourSurface::ipmidi_input_handler (GIOChannel*, GIOCondition condition, void *data);

	int create_surfaces ();
	bool periodic();
	bool redisplay();
	bool hui_heartbeat ();
	void build_gui ();
	bool midi_input_handler (Glib::IOCondition ioc, MIDI::Port* port);
	void clear_ports ();
	void clear_surfaces ();
	void force_special_stripable_to_strip (boost::shared_ptr<ARDOUR::Stripable> r, uint32_t surface, uint32_t strip_number);
	void build_button_map ();
	void stripable_selection_changed ();
	int ipmidi_restart ();
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
	Mackie::LedState stop_press(Mackie::Button &);
	Mackie::LedState stop_release(Mackie::Button &);
	Mackie::LedState play_press(Mackie::Button &);
	Mackie::LedState play_release(Mackie::Button &);
	Mackie::LedState record_press(Mackie::Button &);
	Mackie::LedState record_release(Mackie::Button &);
	Mackie::LedState loop_press(Mackie::Button &);
	Mackie::LedState loop_release(Mackie::Button &);
	Mackie::LedState rewind_press(Mackie::Button & button);
	Mackie::LedState rewind_release(Mackie::Button & button);
	Mackie::LedState ffwd_press(Mackie::Button & button);
	Mackie::LedState ffwd_release(Mackie::Button & button);
	Mackie::LedState cursor_up_press (Mackie::Button &);
	Mackie::LedState cursor_up_release (Mackie::Button &);
	Mackie::LedState cursor_down_press (Mackie::Button &);
	Mackie::LedState cursor_down_release (Mackie::Button &);
	Mackie::LedState cursor_left_press (Mackie::Button &);
	Mackie::LedState cursor_left_release (Mackie::Button &);
	Mackie::LedState cursor_right_press (Mackie::Button &);
	Mackie::LedState cursor_right_release (Mackie::Button &);
	Mackie::LedState left_press(Mackie::Button &);
	Mackie::LedState left_release(Mackie::Button &);
	Mackie::LedState right_press(Mackie::Button &);
	Mackie::LedState right_release(Mackie::Button &);
	Mackie::LedState channel_left_press(Mackie::Button &);
	Mackie::LedState channel_left_release(Mackie::Button &);
	Mackie::LedState channel_right_press(Mackie::Button &);
	Mackie::LedState channel_right_release(Mackie::Button &);
	Mackie::LedState marker_press(Mackie::Button &);
	Mackie::LedState marker_release(Mackie::Button &);
	Mackie::LedState save_press(Mackie::Button &);
	Mackie::LedState save_release(Mackie::Button &);
	Mackie::LedState timecode_beats_press(Mackie::Button &);
	Mackie::LedState timecode_beats_release(Mackie::Button &);
	Mackie::LedState zoom_press(Mackie::Button &);
	Mackie::LedState zoom_release(Mackie::Button &);
	Mackie::LedState scrub_press(Mackie::Button &);
	Mackie::LedState scrub_release(Mackie::Button &);
	Mackie::LedState undo_press (Mackie::Button &);
	Mackie::LedState undo_release (Mackie::Button &);
	Mackie::LedState shift_press (Mackie::Button &);
	Mackie::LedState shift_release (Mackie::Button &);
	Mackie::LedState option_press (Mackie::Button &);
	Mackie::LedState option_release (Mackie::Button &);
	Mackie::LedState control_press (Mackie::Button &);
	Mackie::LedState control_release (Mackie::Button &);
	Mackie::LedState cmd_alt_press (Mackie::Button &);
	Mackie::LedState cmd_alt_release (Mackie::Button &);

	Mackie::LedState pan_press (Mackie::Button &);
	Mackie::LedState pan_release (Mackie::Button &);
	Mackie::LedState plugin_press (Mackie::Button &);
	Mackie::LedState plugin_release (Mackie::Button &);
	Mackie::LedState eq_press (Mackie::Button &);
	Mackie::LedState eq_release (Mackie::Button &);
	Mackie::LedState dyn_press (Mackie::Button &);
	Mackie::LedState dyn_release (Mackie::Button &);
	Mackie::LedState flip_press (Mackie::Button &);
	Mackie::LedState flip_release (Mackie::Button &);
	Mackie::LedState name_value_press (Mackie::Button &);
	Mackie::LedState name_value_release (Mackie::Button &);
//	Mackie::LedState F1_press (Mackie::Button &);
//	Mackie::LedState F1_release (Mackie::Button &);
//	Mackie::LedState F2_press (Mackie::Button &);
//	Mackie::LedState F2_release (Mackie::Button &);
//	Mackie::LedState F3_press (Mackie::Button &);
//	Mackie::LedState F3_release (Mackie::Button &);
//	Mackie::LedState F4_press (Mackie::Button &);
//	Mackie::LedState F4_release (Mackie::Button &);
//	Mackie::LedState F5_press (Mackie::Button &);
//	Mackie::LedState F5_release (Mackie::Button &);
//	Mackie::LedState F6_press (Mackie::Button &);
//	Mackie::LedState F6_release (Mackie::Button &);
//	Mackie::LedState F7_press (Mackie::Button &);
//	Mackie::LedState F7_release (Mackie::Button &);
//	Mackie::LedState F8_press (Mackie::Button &);
//	Mackie::LedState F8_release (Mackie::Button &);
	Mackie::LedState touch_press (Mackie::Button &);
	Mackie::LedState touch_release (Mackie::Button &);
	Mackie::LedState enter_press (Mackie::Button &);
	Mackie::LedState enter_release (Mackie::Button &);
	Mackie::LedState cancel_press (Mackie::Button &);
	Mackie::LedState cancel_release (Mackie::Button &);
	Mackie::LedState user_a_press (Mackie::Button &);
	Mackie::LedState user_a_release (Mackie::Button &);
	Mackie::LedState user_b_press (Mackie::Button &);
	Mackie::LedState user_b_release (Mackie::Button &);
	Mackie::LedState fader_touch_press (Mackie::Button &);
	Mackie::LedState fader_touch_release (Mackie::Button &);
	Mackie::LedState master_fader_touch_press (Mackie::Button &);
	Mackie::LedState master_fader_touch_release (Mackie::Button &);

	Mackie::LedState read_press (Mackie::Button&);
	Mackie::LedState read_release (Mackie::Button&);
	Mackie::LedState write_press (Mackie::Button&);
	Mackie::LedState write_release (Mackie::Button&);
	Mackie::LedState clearsolo_press (Mackie::Button&);
	Mackie::LedState clearsolo_release (Mackie::Button&);
	Mackie::LedState track_press (Mackie::Button&);
	Mackie::LedState track_release (Mackie::Button&);
	Mackie::LedState send_press (Mackie::Button&);
	Mackie::LedState send_release (Mackie::Button&);
	Mackie::LedState miditracks_press (Mackie::Button&);
	Mackie::LedState miditracks_release (Mackie::Button&);
	Mackie::LedState inputs_press (Mackie::Button&);
	Mackie::LedState inputs_release (Mackie::Button&);
	Mackie::LedState audiotracks_press (Mackie::Button&);
	Mackie::LedState audiotracks_release (Mackie::Button&);
	Mackie::LedState audioinstruments_press (Mackie::Button&);
	Mackie::LedState audioinstruments_release (Mackie::Button&);
	Mackie::LedState aux_press (Mackie::Button&);
	Mackie::LedState aux_release (Mackie::Button&);
	Mackie::LedState busses_press (Mackie::Button&);
	Mackie::LedState busses_release (Mackie::Button&);
	Mackie::LedState outputs_press (Mackie::Button&);
	Mackie::LedState outputs_release (Mackie::Button&);
	Mackie::LedState user_press (Mackie::Button&);
	Mackie::LedState user_release (Mackie::Button&);
	Mackie::LedState trim_press (Mackie::Button&);
	Mackie::LedState trim_release (Mackie::Button&);
	Mackie::LedState latch_press (Mackie::Button&);
	Mackie::LedState latch_release (Mackie::Button&);
	Mackie::LedState grp_press (Mackie::Button&);
	Mackie::LedState grp_release (Mackie::Button&);
	Mackie::LedState nudge_press (Mackie::Button&);
	Mackie::LedState nudge_release (Mackie::Button&);
	Mackie::LedState drop_press (Mackie::Button&);
	Mackie::LedState drop_release (Mackie::Button&);
	Mackie::LedState replace_press (Mackie::Button&);
	Mackie::LedState replace_release (Mackie::Button&);
	Mackie::LedState click_press (Mackie::Button&);
	Mackie::LedState click_release (Mackie::Button&);
	Mackie::LedState view_press (Mackie::Button&);
	Mackie::LedState view_release (Mackie::Button&);

	Mackie::LedState bank_release (Mackie::Button&, uint32_t bank_num);
};

} // namespace

#endif // ardour_mackie_control_protocol_h
