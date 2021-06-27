/*
 * Copyright (C) 2016 W.P. van Paass
 * Copyright (C) 2017-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2018 Robin Gareus <robin@gareus.org>
 *
 * Thanks to Rolf Meyerhoff for reverse engineering the CC121 protocol.
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

#ifndef ardour_surface_cc121_h
#define ardour_surface_cc121_h

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

struct CC121Request : public BaseUI::BaseRequestObject {
public:
	CC121Request () {}
	~CC121Request () {}
};

class CC121 : public ARDOUR::ControlProtocol, public AbstractUI<CC121Request> {
  public:
	CC121 (ARDOUR::Session&);
	virtual ~CC121();

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


	/* Note: because the CC121 speaks an inherently duplex protocol,
	   we do not implement get/set_feedback() since this aspect of
	   support for the protocol is not optional.
	*/

	void do_request (CC121Request*);
	int stop ();

	void thread_init ();

	PBD::Signal0<void> ConnectionChange;

	boost::shared_ptr<ARDOUR::Port> input_port();
	boost::shared_ptr<ARDOUR::Port> output_port();

	enum ButtonID {
		Rec = 0x00,
		Solo = 0x08,
		Mute = 0x10,
		Left = 0x30,
		Right = 0x31,
		EButton = 0x33,
		Function1 = 0x36,
		Function2 = 0x37,
		Function3 = 0x38,
		Function4 = 0x39,
		Value = 0x3A,
		Footswitch = 0x3B,
		FP_Read = 0x4A,
		FP_Write = 0x4B,
		Loop = 0x56,
		ToStart = 0x58,
		ToEnd = 0x5A,
		Rewind = 0x5B,
		Ffwd = 0x5C,
		Stop = 0x5D,
		Play = 0x5E,
		RecEnable = 0x5F,
		FaderTouch = 0x68,
		EQ1Enable = 0x70,
		EQ2Enable = 0x71,
		EQ3Enable = 0x72,
		EQ4Enable = 0x73,
		EQType = 0x74,
		AllBypass = 0x75,
		Jog = 0x76,
		Lock = 0x77,
		InputMonitor = 0x78,
		OpenVST = 0x79,
		Output = 22
	};

	enum ButtonState {
		ShiftDown = 0x1,
		RewindDown = 0x2,
		StopDown = 0x4,
		UserDown = 0x8,
		LongPress = 0x10
	};

	void set_action (ButtonID, std::string const& action_name, bool on_press, CC121::ButtonState = ButtonState (0));
	std::string get_action (ButtonID, bool on_press, CC121::ButtonState = ButtonState (0));

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

	bool midi_input_handler (Glib::IOCondition ioc, boost::shared_ptr<ARDOUR::AsyncMIDIPort> port);

	mutable void *gui;
	void build_gui ();

	bool connection_handler (boost::weak_ptr<ARDOUR::Port>, std::string name1, boost::weak_ptr<ARDOUR::Port>, std::string name2, bool yn);
	PBD::ScopedConnection port_connection;

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
	enum JogMode { scroll=1, zoom=2 };
	JogMode _jogmode;

	PBD::microseconds_t last_encoder_time;
	int last_good_encoder_delta;
	int last_encoder_delta, last_last_encoder_delta;

	void button_press_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb);
	void button_release_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb);
	void fader_handler (MIDI::Parser &, MIDI::pitchbend_t pb);
	void encoder_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb);
	/*	void fader_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb);*/

	ButtonState button_state;

	friend class Button;

	class Button {
	  public:

		enum ActionType {
			NamedAction,
			InternalFunction,
		};

		Button (CC121& f, std::string const& str, ButtonID i)
			: fp (f)
			, name (str)
			, id (i)
			, flash (false)
		{}

		void set_action (std::string const& action_name, bool on_press, CC121::ButtonState = ButtonState (0));
		void set_action (boost::function<void()> function, bool on_press, CC121::ButtonState = ButtonState (0));
		std::string get_action (bool press, CC121::ButtonState bs = ButtonState (0));

		void set_led_state (boost::shared_ptr<MIDI::Port>, bool onoff);
		void invoke (ButtonState bs, bool press);
		bool uses_flash () const { return flash; }
		void set_flash (bool yn) { flash = yn; }

		XMLNode& get_state () const;
		int set_state (XMLNode const&);

		sigc::connection timeout_connection;

	  private:
		CC121& fp;
		std::string name;
		ButtonID id;
		bool flash;

		struct ToDo {
			ActionType type;
			/* could be a union if boost::function didn't require a
			 * constructor
			 */
			std::string action_name;
			boost::function<void()> function;
		};

		typedef std::map<CC121::ButtonState,ToDo> ToDoMap;
		ToDoMap on_press;
		ToDoMap on_release;
	};

	typedef std::map<ButtonID,Button> ButtonMap;

	ButtonMap buttons;
	Button& get_button (ButtonID) const;

	std::set<ButtonID> buttons_down;
	std::set<ButtonID> consumed;

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

	sigc::connection heartbeat_connection;
	sigc::connection blink_connection;
	typedef std::list<ButtonID> Blinkers;
	Blinkers blinkers;
	bool blink_state;
	bool blink ();
	bool beat ();
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
	void map_monitoring ();

	/* operations (defined in operations.cc) */

	void read ();
	void write ();

	void input_monitor ();
	void left ();
	void right ();

	void touch ();
	void off ();

	void undo ();
	void redo ();
	void solo ();
	void mute ();
	void jog ();
	void rec_enable ();

	void set_controllable (boost::shared_ptr<ARDOUR::AutomationControl>, float);

	void punch ();
};

}

#endif /* ardour_surface_cc121_h */
