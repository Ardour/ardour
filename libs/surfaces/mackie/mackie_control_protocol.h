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

namespace ArdourSurface { namespace MACKIE_NAMESPACE {

class Surface;
class Control;
class SurfacePort;
class Button;
class Strip;

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
		MidiTracks,
		Inputs,
		AudioTracks,
		AudioInstr,
		Auxes,
		Busses,
		Outputs,
		Selected,
	};

	enum FlipMode {
		Normal, /* fader controls primary, vpot controls secondary */
		Mirror, /* fader + vpot control secondary */
		Swap,   /* fader controls secondary, vpot controls primary */
		Zero,   /* fader controls primary, but doesn't move, vpot controls secondary */
	};

	MackieControlProtocol(ARDOUR::Session &, const char* name);
	virtual ~MackieControlProtocol();

	static MackieControlProtocol* instance() { return _instance; }

	const MACKIE_NAMESPACE::DeviceInfo& device_info() const { return _device_info; }
	MACKIE_NAMESPACE::DeviceProfile& device_profile() { return _device_profile; }

	PBD::Signal0<void> DeviceChanged;
	PBD::Signal1<void,std::shared_ptr<MACKIE_NAMESPACE::Surface> > ConnectionChange;

        void device_ready ();

	int set_active (bool yn);
	int  set_device (const std::string&, bool force);
	void set_profile (const std::string&);

	FlipMode flip_mode () const { return _flip_mode; }
	ViewMode view_mode () const { return _view_mode; }
	std::shared_ptr<MACKIE_NAMESPACE::Subview> subview() { return _subview; }
	bool zoom_mode () const { return modifier_state() & MODIFIER_ZOOM; }
	bool     metering_active () const { return _metering_active; }

	bool is_track (std::shared_ptr<ARDOUR::Stripable>) const;
	bool is_audio_track (std::shared_ptr<ARDOUR::Stripable>) const;
	bool is_midi_track (std::shared_ptr<ARDOUR::Stripable>) const;
	bool is_trigger_track (std::shared_ptr<ARDOUR::Stripable>) const;
	bool is_foldback_bus (std::shared_ptr<ARDOUR::Stripable>) const;
	bool is_vca (std::shared_ptr<ARDOUR::Stripable>) const;
	bool has_instrument (std::shared_ptr<ARDOUR::Stripable>) const;
	bool is_mapped (std::shared_ptr<ARDOUR::Stripable>) const;
	std::shared_ptr<ARDOUR::Stripable> first_selected_stripable () const;

	void check_fader_automation_state ();
	void update_fader_automation_state ();
	void set_automation_state (ARDOUR::AutoState);

	void set_view_mode (ViewMode);
	bool set_subview_mode (MACKIE_NAMESPACE::Subview::Mode, std::shared_ptr<ARDOUR::Stripable>);
	bool redisplay_subview_mode ();
	void set_flip_mode (FlipMode);
	void display_view_mode ();

	XMLNode& get_state () const;
	int set_state (const XMLNode&, int version);

	/* Note: because Mackie control is inherently a duplex protocol,
	   we do not implement get/set_feedback() since this aspect of
	   support for the protocol is not optional.
	*/

	mutable Glib::Threads::Mutex surfaces_lock;
	typedef std::list<std::shared_ptr<MACKIE_NAMESPACE::Surface> > Surfaces;
	Surfaces surfaces;

	std::shared_ptr<MACKIE_NAMESPACE::Surface> get_surface_by_raw_pointer (void*) const;
	std::shared_ptr<MACKIE_NAMESPACE::Surface> nth_surface (uint32_t) const;

	uint32_t global_index (MACKIE_NAMESPACE::Strip&);
	uint32_t global_index_locked (MACKIE_NAMESPACE::Strip&);

	std::list<std::shared_ptr<ARDOUR::Bundle> > bundles ();

	void set_master_on_surface_strip (uint32_t surface, uint32_t strip);
	void set_monitor_on_surface_strip (uint32_t surface, uint32_t strip);

	uint32_t n_strips (bool with_locked_strips = true) const;

	bool has_editor () const { return true; }
	void* get_gui () const;
	void tear_down_gui ();

	void handle_button_event (MACKIE_NAMESPACE::Surface&, MACKIE_NAMESPACE::Button& button, MACKIE_NAMESPACE::ButtonState);

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
	void update_led(MACKIE_NAMESPACE::Surface&, MACKIE_NAMESPACE::Button & button, MACKIE_NAMESPACE::LedState);

	void update_global_button (int id, MACKIE_NAMESPACE::LedState);
	void update_global_led (int id, MACKIE_NAMESPACE::LedState);

	ARDOUR::Session & get_session() { return *session; }
	samplepos_t transport_sample() const;

	int modifier_state() const { return _modifier_state; }
	int main_modifier_state() const { return _modifier_state & MAIN_MODIFIER_MASK; }

	typedef std::list<std::shared_ptr<ARDOUR::AutomationControl> > ControlList;

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
	typedef std::vector<std::shared_ptr<ARDOUR::Stripable> > Sorted;
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

	bool stripable_is_locked_to_strip (std::shared_ptr<ARDOUR::Stripable>) const;

	CONTROL_PROTOCOL_THREADS_NEED_TEMPO_MAP_DECL();

  private:

	struct ButtonHandlers {
		MACKIE_NAMESPACE::LedState (MackieControlProtocol::*press) (MACKIE_NAMESPACE::Button&);
		MACKIE_NAMESPACE::LedState (MackieControlProtocol::*release) (MACKIE_NAMESPACE::Button&);

	    ButtonHandlers (MACKIE_NAMESPACE::LedState (MackieControlProtocol::*p) (MACKIE_NAMESPACE::Button&),
			    MACKIE_NAMESPACE::LedState (MackieControlProtocol::*r) (MACKIE_NAMESPACE::Button&))
	    : press (p)
	    , release (r) {}
	};

	typedef std::map<MACKIE_NAMESPACE::Button::ID,ButtonHandlers> ButtonMap;

	static MackieControlProtocol* _instance;

	bool profile_exists (std::string const&) const;

	MACKIE_NAMESPACE::DeviceInfo       _device_info;
	MACKIE_NAMESPACE::DeviceProfile    _device_profile;
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
	MACKIE_NAMESPACE::Timer            _frm_left_last;
	// last written timecode string
	std::string              _timecode_last;
	samplepos_t				 _sample_last;
	// Which timecode are we displaying? BBT or Timecode
	ARDOUR::AnyTime::Type    _timecode_type;
	// Bundle to represent our input ports
	std::shared_ptr<ARDOUR::Bundle> _input_bundle;
	// Bundle to represent our output ports
	std::shared_ptr<ARDOUR::Bundle> _output_bundle;
	void*                    _gui;
	bool                     _scrub_mode;
	FlipMode                 _flip_mode;
	ViewMode                 _view_mode;
	std::shared_ptr<MACKIE_NAMESPACE::Subview> _subview;
	int                      _current_selected_track;
	int                      _modifier_state;
	ButtonMap                 button_map;
	int16_t                  _ipmidi_base;
	bool                      needs_ipmidi_restart;
	bool                     _metering_active;
	bool                     _initialized;
	mutable XMLNode*         configuration_state;
	int                      state_version;
	int                      _last_bank[9];
	bool                     marker_modifier_consumed_by_button;
	bool                     nudge_modifier_consumed_by_button;

	std::shared_ptr<ArdourSurface::MACKIE_NAMESPACE::Surface>	_master_surface;

	struct ipMIDIHandler {
		MackieControlProtocol* mcp;
		MIDI::Port* port;
	};
	friend struct ipMIDIHandler; /* is this necessary */
	friend gboolean ArdourSurface::MACKIE_NAMESPACE::ipmidi_input_handler (GIOChannel*, GIOCondition condition, void *data);

	int create_surfaces ();
	bool periodic();
	bool redisplay();
	bool hui_heartbeat ();
	void build_gui ();
	bool midi_input_handler (Glib::IOCondition ioc, MIDI::Port* port);
	void clear_ports ();
	void clear_surfaces ();
	void force_special_stripable_to_strip (std::shared_ptr<ARDOUR::Stripable> r, uint32_t surface, uint32_t strip_number);
	void build_button_map ();
	void build_device_specific_button_map ();
	void stripable_selection_changed ();
	int ipmidi_restart ();
        void initialize ();
        int set_device_info (const std::string& device_name);
	void update_configuration_state () const;

	/* MIDI port connection management */

	PBD::ScopedConnection port_connection;
	void connection_handler (std::weak_ptr<ARDOUR::Port>, std::string name1, std::weak_ptr<ARDOUR::Port>, std::string name2, bool);

	/* BUTTON HANDLING */

	typedef std::set<uint32_t> DownButtonList;
	typedef std::map<ARDOUR::AutomationType,DownButtonList> DownButtonMap;
	DownButtonMap  _down_buttons;
	DownButtonList _down_select_buttons;

	void pull_stripable_range (DownButtonList&, ARDOUR::StripableList&, uint32_t pressed);

	/* implemented button handlers */
	MACKIE_NAMESPACE::LedState stop_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState stop_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState play_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState play_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState record_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState record_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState loop_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState loop_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState rewind_press(MACKIE_NAMESPACE::Button & button);
	MACKIE_NAMESPACE::LedState rewind_release(MACKIE_NAMESPACE::Button & button);
	MACKIE_NAMESPACE::LedState ffwd_press(MACKIE_NAMESPACE::Button & button);
	MACKIE_NAMESPACE::LedState ffwd_release(MACKIE_NAMESPACE::Button & button);
	MACKIE_NAMESPACE::LedState cursor_up_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState cursor_up_release (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState cursor_down_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState cursor_down_release (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState cursor_left_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState cursor_left_release (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState cursor_right_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState cursor_right_release (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState left_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState left_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState right_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState right_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState channel_left_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState channel_left_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState channel_right_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState channel_right_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState marker_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState marker_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState save_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState save_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState timecode_beats_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState timecode_beats_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState zoom_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState zoom_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState scrub_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState scrub_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState undo_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState undo_release (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState shift_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState shift_release (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState option_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState option_release (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState control_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState control_release (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState cmd_alt_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState cmd_alt_release (MACKIE_NAMESPACE::Button &);

	MACKIE_NAMESPACE::LedState pan_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState pan_release (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState plugin_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState plugin_release (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState eq_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState eq_release (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState dyn_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState dyn_release (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState flip_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState flip_release (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState name_value_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState name_value_release (MACKIE_NAMESPACE::Button &);
//	MACKIE_NAMESPACE::LedState F1_press (MACKIE_NAMESPACE::Button &);
//	MACKIE_NAMESPACE::LedState F1_release (MACKIE_NAMESPACE::Button &);
//	MACKIE_NAMESPACE::LedState F2_press (MACKIE_NAMESPACE::Button &);
//	MACKIE_NAMESPACE::LedState F2_release (MACKIE_NAMESPACE::Button &);
//	MACKIE_NAMESPACE::LedState F3_press (MACKIE_NAMESPACE::Button &);
//	MACKIE_NAMESPACE::LedState F3_release (MACKIE_NAMESPACE::Button &);
//	MACKIE_NAMESPACE::LedState F4_press (MACKIE_NAMESPACE::Button &);
//	MACKIE_NAMESPACE::LedState F4_release (MACKIE_NAMESPACE::Button &);
//	MACKIE_NAMESPACE::LedState F5_press (MACKIE_NAMESPACE::Button &);
//	MACKIE_NAMESPACE::LedState F5_release (MACKIE_NAMESPACE::Button &);
//	MACKIE_NAMESPACE::LedState F6_press (MACKIE_NAMESPACE::Button &);
//	MACKIE_NAMESPACE::LedState F6_release (MACKIE_NAMESPACE::Button &);
//	MACKIE_NAMESPACE::LedState F7_press (MACKIE_NAMESPACE::Button &);
//	MACKIE_NAMESPACE::LedState F7_release (MACKIE_NAMESPACE::Button &);
//	MACKIE_NAMESPACE::LedState F8_press (MACKIE_NAMESPACE::Button &);
//	MACKIE_NAMESPACE::LedState F8_release (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState touch_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState touch_release (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState enter_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState enter_release (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState cancel_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState cancel_release (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState user_a_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState user_a_release (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState user_b_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState user_b_release (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState fader_touch_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState fader_touch_release (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState master_fader_touch_press (MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState master_fader_touch_release (MACKIE_NAMESPACE::Button &);

	MACKIE_NAMESPACE::LedState read_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState read_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState write_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState write_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState clearsolo_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState clearsolo_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState track_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState track_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState send_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState send_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState miditracks_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState miditracks_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState inputs_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState inputs_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState audiotracks_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState audiotracks_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState audioinstruments_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState audioinstruments_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState aux_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState aux_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState busses_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState busses_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState outputs_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState outputs_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState user_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState user_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState trim_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState trim_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState latch_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState latch_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState grp_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState grp_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState nudge_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState nudge_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState drop_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState drop_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState replace_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState replace_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState click_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState click_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState view_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState view_release (MACKIE_NAMESPACE::Button&);

	MACKIE_NAMESPACE::LedState bank_release (MACKIE_NAMESPACE::Button&, uint32_t bank_num);
	MACKIE_NAMESPACE::LedState master_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState master_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState redo_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState redo_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState prev_marker_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState prev_marker_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState next_marker_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState next_marker_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState flip_window_press (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState flip_window_release (MACKIE_NAMESPACE::Button&);
	MACKIE_NAMESPACE::LedState open_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState open_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState prog2_clear_solo_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState prog2_clear_solo_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState prog2_save_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState prog2_save_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState prog2_vst_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState prog2_vst_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState prog2_left_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState prog2_left_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState prog2_right_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState prog2_right_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState prog2_marker_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState prog2_marker_release(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState prog2_undo_press(MACKIE_NAMESPACE::Button &);
	MACKIE_NAMESPACE::LedState prog2_undo_release(MACKIE_NAMESPACE::Button &);
};

} // namespace
} // namespace

#endif // ardour_mackie_control_protocol_h
