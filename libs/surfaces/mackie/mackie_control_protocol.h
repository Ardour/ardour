/*
    Copyright (C) 2006,2007 John Anderson
    
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

#ifndef ardour_mackie_control_protocol_h
#define ardour_mackie_control_protocol_h

#include <vector>
#include <map>

#include <sys/time.h>
#include <pthread.h>
#include <boost/smart_ptr.hpp>

#include <glibmm/thread.h>

#include "pbd/abstract_ui.h"

#include "midi++/types.h"

#include "ardour/types.h"
#include "control_protocol/control_protocol.h"

#include "types.h"
#include "midi_byte_array.h"
#include "controls.h"
#include "mackie_jog_wheel.h"
#include "timer.h"
#include "device_info.h"

namespace ARDOUR {
	class AutomationControl;
}

namespace MIDI {
	class Port;
}

namespace Mackie {
	class Surface;
	class Control;
	class SurfacePort;
	class Button;
}

/**
	This handles the plugin duties, and the midi encoding and decoding,
	and the signal callbacks, mostly from ARDOUR::Route.

	The model of the control surface is handled by classes in controls.h

	What happens is that each strip on the control surface has
	a corresponding route in ControlProtocol::route_table. When
	an incoming midi message is signaled, the correct route
	is looked up, and the relevant changes made to it.

	For each route currently in route_table, there's a RouteSignal object
	which encapsulates the signals that indicate that there are changes
	to be sent to the surface. The signals are handled by this class.

	Calls to signal handlers pass a Route object which is used to look
	up the relevant Strip in Surface. Then the state is retrieved from
	the Route and encoded as the correct midi message.
*/

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

	enum ViewMode {
		Mixer,
		Dynamics,
		EQ,
		Loop,
		AudioTracks,
		MidiTracks,
		Busses,
		Sends,
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

	int set_active (bool yn);
	void set_device (const std::string&);

	bool     flip_mode () const { return _flip_mode; }
	ViewMode view_mode () const { return _view_mode; }

	void set_view_mode (ViewMode);
	void set_flip_mode (bool);

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);
  
	static bool probe();
	
	typedef std::list<boost::shared_ptr<Mackie::Surface> > Surfaces;
	Surfaces surfaces;

	std::list<boost::shared_ptr<ARDOUR::Bundle> > bundles ();

	void set_master_on_surface_strip (uint32_t surface, uint32_t strip);
	void set_monitor_on_surface_strip (uint32_t surface, uint32_t strip);
	
	uint32_t n_strips () const;
	
	bool has_editor () const { return true; }
	void* get_gui () const;
	void tear_down_gui ();

	void select_track (boost::shared_ptr<ARDOUR::Route> r);
	
	void handle_button_event (Mackie::Surface&, Mackie::Button& button, Mackie::ButtonState);

	void notify_route_added (ARDOUR::RouteList &);
	void notify_remote_id_changed();

	/// rebuild the current bank. Called on route added/removed and
	/// remote id changed.
	void refresh_current_bank();

	// button-related signals
	void notify_record_state_changed();
	void notify_transport_state_changed();
	void notify_loop_state_changed();
	// mainly to pick up punch-in and punch-out
	void notify_parameter_changed(std::string const &);
	void notify_solo_active_changed(bool);

	/// Turn timecode on and beats off, or vice versa, depending
	/// on state of _timecode_type
	void update_timecode_beats_led();
  
	/// this is called to generate the midi to send in response to a button press.
	void update_led(Mackie::Surface&, Mackie::Button & button, Mackie::LedState);
  
	void update_global_button(const std::string & name, Mackie::LedState);
	void update_global_led(const std::string & name, Mackie::LedState);

	ARDOUR::Session & get_session() { return *session; }
	framepos_t transport_frame() const;

	void add_in_use_timeout (Mackie::Surface& surface, Mackie::Control& in_use_control, boost::weak_ptr<ARDOUR::AutomationControl> touched);

	int modifier_state() const { return _modifier_state; }
	
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
	   Fetch the set of routes to be considered for control by the
	   surface. Excluding master, hidden and control routes, and inactive routes
	*/
	typedef std::vector<boost::shared_ptr<ARDOUR::Route> > Sorted;
	Sorted get_sorted_routes();
  
	// bank switching
	void switch_banks (uint32_t first_remote_id, bool force = false);
	void prev_track ();
	void next_track ();
  
	// also called from poll_automation to update timecode display
	void update_timecode_display();

	std::string format_bbt_timecode (ARDOUR::framepos_t now_frame);
	std::string format_timecode_timecode (ARDOUR::framepos_t now_frame);
	
	void do_request (MackieControlUIRequest*);
	int stop ();

	void thread_init ();

	/* handling function key presses */

	std::string f_action (uint32_t fn);
	void f_press (uint32_t fn);

  private:

	struct ButtonHandlers {
	    Mackie::LedState (MackieControlProtocol::*press) (Mackie::Button&);
	    Mackie::LedState (MackieControlProtocol::*release) (Mackie::Button&);
	    
	    ButtonHandlers (Mackie::LedState (MackieControlProtocol::*p) (Mackie::Button&),
			    Mackie::LedState (MackieControlProtocol::*r) (Mackie::Button&)) 
	    : press (p)
	    , release (r) {}
	};

	typedef std::map<int,ButtonHandlers> ButtonMap;
	typedef std::list<GSource*> PortSources;

	static MackieControlProtocol* _instance;
	
	Mackie::DeviceInfo       _device_info;
	sigc::connection          periodic_connection;
	uint32_t                 _current_initial_bank;
	PBD::ScopedConnectionList audio_engine_connections;
	PBD::ScopedConnectionList session_connections;
	PBD::ScopedConnectionList port_connections;
	PBD::ScopedConnectionList route_connections;
	PBD::ScopedConnectionList gui_connections;
	bool _transport_previously_rolling;
	// timer for two quick marker left presses
	Mackie::Timer            _frm_left_last;
	// last written timecode string
	std::string              _timecode_last;
	// Which timecode are we displaying? BBT or Timecode
	ARDOUR::AnyTime::Type    _timecode_type;
	// Bundle to represent our input ports
	boost::shared_ptr<ARDOUR::Bundle> _input_bundle;
	// Bundle to represent our output ports
	boost::shared_ptr<ARDOUR::Bundle> _output_bundle;
	void*                    _gui;
	bool                     _zoom_mode;
	bool                     _scrub_mode;
	bool                     _flip_mode;
	ViewMode                 _view_mode;
	int                      _current_selected_track;
	int                      _modifier_state;
	PortSources               port_sources;
	std::vector<std::string> _f_actions;
	ButtonMap                 button_map;

	void create_surfaces ();
	void port_connected_or_disconnected (std::string, std::string, bool);
	bool control_in_use_timeout (Mackie::Surface*, Mackie::Control *, boost::weak_ptr<ARDOUR::AutomationControl>);
	bool periodic();
	void build_gui ();
	bool midi_input_handler (Glib::IOCondition ioc, MIDI::Port* port);
	void clear_ports ();
	void force_special_route_to_strip (boost::shared_ptr<ARDOUR::Route> r, uint32_t surface, uint32_t strip_number);
	void build_button_map ();

	void gui_track_selection_changed (ARDOUR::RouteNotificationListPtr);

	/* BUTTON HANDLING */

	/* implemented button handlers */
	Mackie::LedState frm_left_press(Mackie::Button &);
	Mackie::LedState frm_left_release(Mackie::Button &);
	Mackie::LedState frm_right_press(Mackie::Button &);
	Mackie::LedState frm_right_release(Mackie::Button &);
	Mackie::LedState stop_press(Mackie::Button &);
	Mackie::LedState stop_release(Mackie::Button &);
	Mackie::LedState play_press(Mackie::Button &);
	Mackie::LedState play_release(Mackie::Button &);
	Mackie::LedState record_press(Mackie::Button &);
	Mackie::LedState record_release(Mackie::Button &);
	Mackie::LedState loop_press(Mackie::Button &);
	Mackie::LedState loop_release(Mackie::Button &);
	Mackie::LedState punch_in_press(Mackie::Button &);
	Mackie::LedState punch_in_release(Mackie::Button &);
	Mackie::LedState punch_out_press(Mackie::Button &);
	Mackie::LedState punch_out_release(Mackie::Button &);
	Mackie::LedState home_press(Mackie::Button &);
	Mackie::LedState home_release(Mackie::Button &);
	Mackie::LedState end_press(Mackie::Button &);
	Mackie::LedState end_release(Mackie::Button &);
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
	Mackie::LedState clicking_press(Mackie::Button &);
	Mackie::LedState clicking_release(Mackie::Button &);
	Mackie::LedState global_solo_press(Mackie::Button &);
	Mackie::LedState global_solo_release(Mackie::Button &);
	Mackie::LedState marker_press(Mackie::Button &);
	Mackie::LedState marker_release(Mackie::Button &);
	Mackie::LedState drop_press(Mackie::Button &);
	Mackie::LedState drop_release(Mackie::Button &);
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
	Mackie::LedState redo_press (Mackie::Button &);
	Mackie::LedState redo_release (Mackie::Button &);
	Mackie::LedState shift_press (Mackie::Button &);
	Mackie::LedState shift_release (Mackie::Button &);
	Mackie::LedState option_press (Mackie::Button &);
	Mackie::LedState option_release (Mackie::Button &);
	Mackie::LedState control_press (Mackie::Button &);
	Mackie::LedState control_release (Mackie::Button &);
	Mackie::LedState cmd_alt_press (Mackie::Button &);
	Mackie::LedState cmd_alt_release (Mackie::Button &);

	Mackie::LedState io_press (Mackie::Button &);
	Mackie::LedState io_release (Mackie::Button &);
	Mackie::LedState sends_press (Mackie::Button &);
	Mackie::LedState sends_release (Mackie::Button &);
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
	Mackie::LedState edit_press (Mackie::Button &);
	Mackie::LedState edit_release (Mackie::Button &);
	Mackie::LedState name_value_press (Mackie::Button &);
	Mackie::LedState name_value_release (Mackie::Button &);
	Mackie::LedState F1_press (Mackie::Button &);
	Mackie::LedState F1_release (Mackie::Button &);
	Mackie::LedState F2_press (Mackie::Button &);
	Mackie::LedState F2_release (Mackie::Button &);
	Mackie::LedState F3_press (Mackie::Button &);
	Mackie::LedState F3_release (Mackie::Button &);
	Mackie::LedState F4_press (Mackie::Button &);
	Mackie::LedState F4_release (Mackie::Button &);
	Mackie::LedState F5_press (Mackie::Button &);
	Mackie::LedState F5_release (Mackie::Button &);
	Mackie::LedState F6_press (Mackie::Button &);
	Mackie::LedState F6_release (Mackie::Button &);
	Mackie::LedState F7_press (Mackie::Button &);
	Mackie::LedState F7_release (Mackie::Button &);
	Mackie::LedState F8_press (Mackie::Button &);
	Mackie::LedState F8_release (Mackie::Button &);
	Mackie::LedState F9_press (Mackie::Button &);
	Mackie::LedState F9_release (Mackie::Button &);
	Mackie::LedState F10_press (Mackie::Button &);
	Mackie::LedState F10_release (Mackie::Button &);
	Mackie::LedState F11_press (Mackie::Button &);
	Mackie::LedState F11_release (Mackie::Button &);
	Mackie::LedState F12_press (Mackie::Button &);
	Mackie::LedState F12_release (Mackie::Button &);
	Mackie::LedState F13_press (Mackie::Button &);
	Mackie::LedState F13_release (Mackie::Button &);
	Mackie::LedState F14_press (Mackie::Button &);
	Mackie::LedState F14_release (Mackie::Button &);
	Mackie::LedState F15_press (Mackie::Button &);
	Mackie::LedState F15_release (Mackie::Button &);
	Mackie::LedState F16_press (Mackie::Button &);
	Mackie::LedState F16_release (Mackie::Button &);
	Mackie::LedState on_press (Mackie::Button &);
	Mackie::LedState on_release (Mackie::Button &);
	Mackie::LedState rec_ready_press (Mackie::Button &);
	Mackie::LedState rec_ready_release (Mackie::Button &);
	Mackie::LedState snapshot_press (Mackie::Button &);
	Mackie::LedState snapshot_release (Mackie::Button &);
	Mackie::LedState touch_press (Mackie::Button &);
	Mackie::LedState touch_release (Mackie::Button &);
	Mackie::LedState enter_press (Mackie::Button &);
	Mackie::LedState enter_release (Mackie::Button &);
	Mackie::LedState cancel_press (Mackie::Button &);
	Mackie::LedState cancel_release (Mackie::Button &);
	Mackie::LedState mixer_press (Mackie::Button &);
	Mackie::LedState mixer_release (Mackie::Button &);
	Mackie::LedState user_a_press (Mackie::Button &);
	Mackie::LedState user_a_release (Mackie::Button &);
	Mackie::LedState user_b_press (Mackie::Button &);
	Mackie::LedState user_b_release (Mackie::Button &);
	Mackie::LedState fader_touch_press (Mackie::Button &);
	Mackie::LedState fader_touch_release (Mackie::Button &);
};



#endif // ardour_mackie_control_protocol_h
