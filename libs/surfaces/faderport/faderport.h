/*
 * Copyright (C) 2015-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef ardour_surface_faderport_h
#define ardour_surface_faderport_h

#include <list>
#include <map>
#include <set>
#include <glibmm/threads.h>

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"

#include "ardour/types.h"

#include "control_protocol/control_protocol.h"

namespace PBD {
	class Controllable;
}

#include <midi++/types.h>

//#include "pbd/signals.h"


//#include "midi_byte_array.h"
#include "types.h"

#include "glibmm/main.h"

namespace MIDI {
	class Parser;
	class Port;
}


namespace ARDOUR {
	class AsyncMIDIPort;
	class Bundle;
	class Port;
	class Session;
	class MidiPort;
}


class MIDIControllable;
class MIDIFunction;
class MIDIAction;

namespace ArdourSurface {

struct FaderPortRequest : public BaseUI::BaseRequestObject {
public:
	FaderPortRequest () {}
	~FaderPortRequest () {}
};

class FaderPort : public ARDOUR::ControlProtocol, public AbstractUI<FaderPortRequest> {
  public:
	FaderPort (ARDOUR::Session&);
	virtual ~FaderPort();

	int set_active (bool yn);

	/* we probe for a device when our ports are connected. Before that,
	   there's no way to know if the device exists or not.
	 */
	static bool probe() { return true; }
	static void* request_factory (uint32_t);

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	bool has_editor () const { return true; }
	void* get_gui () const;
	void  tear_down_gui ();

	/* Note: because the FaderPort speaks an inherently duplex protocol,
	   we do not implement get/set_feedback() since this aspect of
	   support for the protocol is not optional.
	*/

	void do_request (FaderPortRequest*);
	int stop ();

	void thread_init ();

	PBD::Signal0<void> ConnectionChange;

	boost::shared_ptr<ARDOUR::Port> input_port();
	boost::shared_ptr<ARDOUR::Port> output_port();

	/* In a feat of engineering brilliance, the Presonus Faderport sends
	 * one button identifier when the button is pressed/released, but
	 * responds to another button identifier as a command to light the LED
	 * corresponding to the button. These ID's define what is sent
	 * for press/release; a separate data structure contains information
	 * on what to send to turn the LED on/off.
	 *
	 * One can only conclude that Presonus just didn't want to fix this
	 * issue because it contradicts their own documentation and is more or
	 * less the first thing you discover when programming the device.
	 */

	enum ButtonID {
		Mute = 18,
		Solo = 17,
		Rec = 16,
		Left = 19,
		Bank = 20,
		Right = 21,
		Output = 22,
		FP_Read = 10,
		FP_Write = 9,
		FP_Touch = 8,
		FP_Off = 23,
		Mix = 11,
		Proj = 12,
		Trns = 13,
		Undo = 14,
		Shift = 2,
		Punch = 1,
		User = 0,
		Loop = 15,
		Rewind = 3,
		Ffwd = 4,
		Stop = 5,
		Play = 6,
		RecEnable = 7,
		Footswitch = 126,
		FaderTouch = 127,
	};

	enum ButtonState {
		ShiftDown = 0x1,
		RewindDown = 0x2,
		StopDown = 0x4,
		UserDown = 0x8,
		LongPress = 0x10
	};

	void set_action (ButtonID, std::string const& action_name, bool on_press, FaderPort::ButtonState = ButtonState (0));
	std::string get_action (ButtonID, bool on_press, FaderPort::ButtonState = ButtonState (0));

	std::list<boost::shared_ptr<ARDOUR::Bundle> > bundles ();

  private:
	boost::shared_ptr<ARDOUR::Stripable> _current_stripable;
	boost::weak_ptr<ARDOUR::Stripable> pre_master_stripable;
	boost::weak_ptr<ARDOUR::Stripable> pre_monitor_stripable;

	boost::shared_ptr<ARDOUR::AsyncMIDIPort> _input_port;
	boost::shared_ptr<ARDOUR::AsyncMIDIPort> _output_port;

	// Bundle to represent our input ports
	boost::shared_ptr<ARDOUR::Bundle> _input_bundle;
	// Bundle to represent our output ports
	boost::shared_ptr<ARDOUR::Bundle> _output_bundle;

	PBD::ScopedConnectionList midi_connections;

	bool midi_input_handler (Glib::IOCondition ioc, boost::weak_ptr<ARDOUR::AsyncMIDIPort> port);

	mutable void *gui;
	void build_gui ();

	bool connection_handler (boost::weak_ptr<ARDOUR::Port>, std::string name1, boost::weak_ptr<ARDOUR::Port>, std::string name2, bool yn);
	PBD::ScopedConnection _port_connection;

	enum ConnectionState {
		InputConnected = 0x1,
		OutputConnected = 0x2
	};

	int connection_state;
	void connected ();
	bool _device_active;
	int fader_msb;
	int fader_lsb;
	bool fader_is_touched;

	PBD::microseconds_t last_encoder_time;
	int last_good_encoder_delta;
	int last_encoder_delta, last_last_encoder_delta;

	void sysex_handler (MIDI::Parser &p, MIDI::byte *, size_t);
	void button_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb);
	void encoder_handler (MIDI::Parser &, MIDI::pitchbend_t pb);
	void fader_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb);

	ButtonState button_state;

	friend class Button;

	class Button {
	  public:

		enum ActionType {
			NamedAction,
			InternalFunction,
		};

		Button (FaderPort& f, std::string const& str, ButtonID i, int o)
			: fp (f)
			, name (str)
			, id (i)
			, out (o)
			, flash (false)
		{}

		void set_action (std::string const& action_name, bool on_press, FaderPort::ButtonState = ButtonState (0));
		void set_action (boost::function<void()> function, bool on_press, FaderPort::ButtonState = ButtonState (0));
		std::string get_action (bool press, FaderPort::ButtonState bs = ButtonState (0));

		void set_led_state (boost::shared_ptr<MIDI::Port>, bool onoff);
		bool invoke (ButtonState bs, bool press);
		bool uses_flash () const { return flash; }
		void set_flash (bool yn) { flash = yn; }

		XMLNode& get_state () const;
		int set_state (XMLNode const&);

		sigc::connection timeout_connection;

	  private:
		FaderPort& fp;
		std::string name;
		ButtonID id;
		int out;
		bool flash;

		struct ToDo {
			ActionType type;
			/* could be a union if boost::function didn't require a
			 * constructor
			 */
			std::string action_name;
			boost::function<void()> function;
		};

		typedef std::map<FaderPort::ButtonState,ToDo> ToDoMap;
		ToDoMap on_press;
		ToDoMap on_release;
	};

	typedef std::map<ButtonID,Button> ButtonMap;

	ButtonMap buttons;
	Button& get_button (ButtonID) const;

	std::set<ButtonID> buttons_down;
	std::set<ButtonID> consumed;

	bool button_long_press_timeout (ButtonID id);
	void start_press_timeout (Button&, ButtonID);

	void all_lights_out ();
	void close ();
	void start_midi_handling ();
	void stop_midi_handling ();

	PBD::ScopedConnectionList session_connections;
	void connect_session_signals ();
	void map_recenable_state ();
	void map_transport_state ();

	sigc::connection periodic_connection;
	bool periodic ();

	sigc::connection blink_connection;
	typedef std::list<ButtonID> Blinkers;
	Blinkers blinkers;
	bool blink_state;
	bool blink ();
	void start_blinking (ButtonID);
	void stop_blinking (ButtonID);

	void set_current_stripable (boost::shared_ptr<ARDOUR::Stripable>);
	void drop_current_stripable ();
	void use_master ();
	void use_monitor ();
	void stripable_selection_changed ();
	PBD::ScopedConnection selection_connection;
	PBD::ScopedConnectionList stripable_connections;

	void map_stripable_state ();
	void map_solo ();
	void map_mute ();
	bool rec_enable_state;
	void map_recenable ();
	void map_gain ();
	void map_cut ();
	void map_auto ();
	void parameter_changed (std::string);

	/* operations (defined in operations.cc) */

	void read ();
	void write ();

	void left ();
	void right ();

	void touch ();
	void off ();

	void undo ();
	void redo ();
	void solo ();
	void mute ();
	void rec_enable ();

	void pan_azimuth (int);
	void pan_width (int);

	void punch ();
};

}

#endif /* ardour_surface_faderport_h */
