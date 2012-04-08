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

#include <sys/time.h>
#include <pthread.h>

#include <glibmm/thread.h>

#include "pbd/abstract_ui.h"

#include "ardour/types.h"
#include "ardour/midi_ui.h"
#include "midi++/types.h"

#include "control_protocol/control_protocol.h"
#include "midi_byte_array.h"
#include "controls.h"
#include "dummy_port.h"
#include "route_signal.h"
#include "mackie_button_handler.h"
#include "mackie_port.h"
#include "mackie_jog_wheel.h"
#include "timer.h"

namespace MIDI {
	class Port;
}

namespace Mackie {
	class Surface;
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
	, public Mackie::MackieButtonHandler
{
  public:
	MackieControlProtocol(ARDOUR::Session &);
	virtual ~MackieControlProtocol();

	int set_active (bool yn);

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);
  
	static bool probe();
	
	Mackie::Surface & surface();

	std::list<boost::shared_ptr<ARDOUR::Bundle> > bundles ();

	bool has_editor () const { return true; }
	void* get_gui () const;
	void tear_down_gui ();
	
	// control events
	void handle_control_event (Mackie::SurfacePort & port, Mackie::Control & control, const Mackie::ControlState & state);

	// strip/route related stuff
  public:	
	void notify_solo_changed (Mackie::RouteSignal *);
	void notify_mute_changed (Mackie::RouteSignal *);
	void notify_record_enable_changed (Mackie::RouteSignal *);
	void notify_gain_changed (Mackie::RouteSignal *, bool force_update = true);
	void notify_property_changed (const PBD::PropertyChange&, Mackie::RouteSignal *);
	void notify_panner_changed (Mackie::RouteSignal *, bool force_update = true);
	void notify_route_added (ARDOUR::RouteList &);
	void notify_active_changed (Mackie::RouteSignal *);
 
	void notify_remote_id_changed();

	/// rebuild the current bank. Called on route added/removed and
	/// remote id changed.
	void refresh_current_bank();

  public:
	// button-related signals
	void notify_record_state_changed();
	void notify_transport_state_changed();
	// mainly to pick up punch-in and punch-out
	void notify_parameter_changed(std::string const &);
	void notify_solo_active_changed(bool);

	/// Turn timecode on and beats off, or vice versa, depending
	/// on state of _timecode_type
	void update_timecode_beats_led();
  
	/// this is called to generate the midi to send in response to a button press.
	void update_led(Mackie::Button & button, Mackie::LedState);
  
	void update_global_button(const std::string & name, Mackie::LedState);
	void update_global_led(const std::string & name, Mackie::LedState);
  
	// transport button handler methods from MackieButtonHandler
	virtual Mackie::LedState frm_left_press(Mackie::Button &);
	virtual Mackie::LedState frm_left_release(Mackie::Button &);

	virtual Mackie::LedState frm_right_press(Mackie::Button &);
	virtual Mackie::LedState frm_right_release(Mackie::Button &);

	virtual Mackie::LedState stop_press(Mackie::Button &);
	virtual Mackie::LedState stop_release(Mackie::Button &);

	virtual Mackie::LedState play_press(Mackie::Button &);
	virtual Mackie::LedState play_release(Mackie::Button &);

	virtual Mackie::LedState record_press(Mackie::Button &);
	virtual Mackie::LedState record_release(Mackie::Button &);

	virtual Mackie::LedState loop_press(Mackie::Button &);
	virtual Mackie::LedState loop_release(Mackie::Button &);

	virtual Mackie::LedState punch_in_press(Mackie::Button &);
	virtual Mackie::LedState punch_in_release(Mackie::Button &);

	virtual Mackie::LedState punch_out_press(Mackie::Button &);
	virtual Mackie::LedState punch_out_release(Mackie::Button &);

	virtual Mackie::LedState home_press(Mackie::Button &);
	virtual Mackie::LedState home_release(Mackie::Button &);

	virtual Mackie::LedState end_press(Mackie::Button &);
	virtual Mackie::LedState end_release(Mackie::Button &);
	
	virtual Mackie::LedState rewind_press(Mackie::Button & button);
	virtual Mackie::LedState rewind_release(Mackie::Button & button);

	virtual Mackie::LedState ffwd_press(Mackie::Button & button);
	virtual Mackie::LedState ffwd_release(Mackie::Button & button);

	// bank switching button handler methods from MackieButtonHandler
	virtual Mackie::LedState left_press(Mackie::Button &);
	virtual Mackie::LedState left_release(Mackie::Button &);

	virtual Mackie::LedState right_press(Mackie::Button &);
	virtual Mackie::LedState right_release(Mackie::Button &);

	virtual Mackie::LedState channel_left_press(Mackie::Button &);
	virtual Mackie::LedState channel_left_release(Mackie::Button &);

	virtual Mackie::LedState channel_right_press(Mackie::Button &);
	virtual Mackie::LedState channel_right_release(Mackie::Button &);
	
	virtual Mackie::LedState clicking_press(Mackie::Button &);
	virtual Mackie::LedState clicking_release(Mackie::Button &);
	
	virtual Mackie::LedState global_solo_press(Mackie::Button &);
	virtual Mackie::LedState global_solo_release(Mackie::Button &);
	
	// function buttons
	virtual Mackie::LedState marker_press(Mackie::Button &);
	virtual Mackie::LedState marker_release(Mackie::Button &);

	virtual Mackie::LedState drop_press(Mackie::Button &);
	virtual Mackie::LedState drop_release(Mackie::Button &);

	virtual Mackie::LedState save_press(Mackie::Button &);
	virtual Mackie::LedState save_release(Mackie::Button &);

	virtual Mackie::LedState timecode_beats_press(Mackie::Button &);
	virtual Mackie::LedState timecode_beats_release(Mackie::Button &);

	// jog wheel states
	virtual Mackie::LedState zoom_press(Mackie::Button &);
	virtual Mackie::LedState zoom_release(Mackie::Button &);

	virtual Mackie::LedState scrub_press(Mackie::Button &);
	virtual Mackie::LedState scrub_release(Mackie::Button &);
	
	/// This is the main MCU port, ie not an extender port
	/// Only for use by JogWheel
	const Mackie::SurfacePort & mcu_port() const;
	Mackie::SurfacePort & mcu_port();
	ARDOUR::Session & get_session() { return *session; }
 
  protected:
	// create instances of MackiePort, depending on what's found in ardour.rc
	void create_ports();
  
	// shut down the surface
	void close();
  
	// create the Surface object, with the correct number
	// of strips for the currently connected ports and 
	// hook up the control event notification
	void initialize_surface();
  
	// This sets up the notifications and sets the
	// controls to the correct values
	void update_surface();
	
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
	void switch_banks(int initial);
	void prev_track();
	void next_track();
  
	// delete all RouteSignal objects connecting Routes to Strips
	void clear_route_signals();
	
	typedef std::vector<Mackie::RouteSignal*> RouteSignals;
	RouteSignals route_signals;
	
	// return which of the ports a particular route_table
	// index belongs to
	Mackie::MackiePort & port_for_id(uint32_t index);

	/**
	   Handle a button press for the control and return whether
	   the corresponding light should be on or off.
	*/
	bool handle_strip_button (Mackie::SurfacePort &, Mackie::Control &, Mackie::ButtonState, boost::shared_ptr<ARDOUR::Route>);

	void add_port (MIDI::Port &, MIDI::Port &, int number, Mackie::MackiePort::port_type_t);

	/**
	   Read session data and send to surface. Includes
	   automation from the currently active routes and
	   timecode displays.
	*/
	void poll_session_data();
	
	// called from poll_automation to figure out which automations need to be sent
	void update_automation(Mackie::RouteSignal &);
	
	// also called from poll_automation to update timecode display
	void update_timecode_display();

	std::string format_bbt_timecode (ARDOUR::framepos_t now_frame);
	std::string format_timecode_timecode (ARDOUR::framepos_t now_frame);
	
	/**
	   notification that the port is about to start it's init sequence.
	   We must make sure that before this exits, the port is being polled
	   for new data.
	*/
	void handle_port_init(Mackie::SurfacePort *);

	/// notification from a MackiePort that it's now active
	void handle_port_active(Mackie::SurfacePort *);
	
	/// notification from a MackiePort that it's now inactive
	void handle_port_inactive(Mackie::SurfacePort *);
	
	boost::shared_ptr<ARDOUR::Route> master_route();
	Mackie::Strip & master_strip();

	void do_request (MackieControlUIRequest*);
	int stop ();
	
  private:

	void port_connected_or_disconnected (std::string, std::string, bool);
	
	boost::shared_ptr<Mackie::RouteSignal> master_route_signal;
	
	static const char * default_port_name;
	
	/// The Midi port(s) connected to the units
	typedef std::vector<Mackie::MackiePort*> MackiePorts;
	MackiePorts _ports;
  
	/// Sometimes the real port goes away, and we want to contain the breakage
	Mackie::DummyPort _dummy_port;
  
	/// The initial remote_id of the currently switched in bank.
	uint32_t _current_initial_bank;
	
	/// protects the port list
	Glib::Mutex update_mutex;

	PBD::ScopedConnectionList audio_engine_connections;
	PBD::ScopedConnectionList session_connections;
	PBD::ScopedConnectionList port_connections;
	PBD::ScopedConnectionList route_connections;
	
	/// The representation of the physical controls on the surface.
  	Mackie::Surface * _surface;
	
	bool _transport_previously_rolling;
	
	// timer for two quick marker left presses
	Mackie::Timer _frm_left_last;
	
	Mackie::JogWheel _jog_wheel;
	
	// last written timecode string
	std::string _timecode_last;
	
	// Which timecode are we displaying? BBT or Timecode
	ARDOUR::AnyTime::Type _timecode_type;

	// Bundle to represent our input ports
	boost::shared_ptr<ARDOUR::Bundle> _input_bundle;
	// Bundle to represent our output ports
	boost::shared_ptr<ARDOUR::Bundle> _output_bundle;

	void build_gui ();
	void* _gui;
};

#endif // ardour_mackie_control_protocol_h
