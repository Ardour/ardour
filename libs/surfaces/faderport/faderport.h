/*
    Copyright (C) 2006 Paul Davis

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

#ifndef ardour_surface_faderport_h
#define ardour_surface_faderport_h

#include <list>
#include <map>
#include <glibmm/threads.h>

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"

#include "ardour/types.h"

#include "control_protocol/control_protocol.h"

namespace PBD {
	class Controllable;
	class ControllableDescriptor;
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

  private:
	boost::shared_ptr<ARDOUR::Route> _current_route;
	boost::weak_ptr<ARDOUR::Route> pre_master_route;
	boost::weak_ptr<ARDOUR::Route> pre_monitor_route;

	boost::shared_ptr<ARDOUR::AsyncMIDIPort> _input_port;
	boost::shared_ptr<ARDOUR::AsyncMIDIPort> _output_port;

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

	ARDOUR::microseconds_t last_encoder_time;
	int last_good_encoder_delta;
	int last_encoder_delta, last_last_encoder_delta;

	void sysex_handler (MIDI::Parser &p, MIDI::byte *, size_t);
	void switch_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb);
	void encoder_handler (MIDI::Parser &, MIDI::pitchbend_t pb);
	void fader_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb);

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
		FaderTouch = 127,
	};

	enum ButtonState {
		ShiftDown = 0x1,
		RewindDown = 0x2,
		StopDown = 0x4,
		UserDown = 0x8,
	};

	ButtonState button_state;

	friend class ButtonInfo;

	class ButtonInfo {
	  public:

		enum ActionType {
			NamedAction,
			InternalFunction,
		};

		ButtonInfo (FaderPort& f, std::string const& str, ButtonID i, int o)
			: fp (f)
			, name (str)
			, id (i)
			, out (o)
			, type (NamedAction)
			, led_on (false)
			, flash (false)
		{}

		void set_action (std::string const& action_name, bool on_press, FaderPort::ButtonState = ButtonState (0));
		void set_action (boost::function<void()> function, bool on_press, FaderPort::ButtonState = ButtonState (0));
		void set_led_state (boost::shared_ptr<MIDI::Port>, int onoff, bool force = false);
		void invoke (ButtonState bs, bool press);
		bool uses_flash () const { return flash; }
		void set_flash (bool yn) { flash = yn; }

	  private:
		FaderPort& fp;
		std::string name;
		ButtonID id;
		int out;
		ActionType type;
		bool led_on;
		bool flash;

		/* could be a union if boost::function didn't require a
		 * constructor
		 */
		struct ToDo {
			std::string action_name;
			boost::function<void()> function;
		};

		typedef std::map<FaderPort::ButtonState,ToDo> ToDoMap;
		ToDoMap on_press;
		ToDoMap on_release;
	};

	typedef std::map<ButtonID,ButtonInfo> ButtonMap;

	ButtonMap buttons;
	ButtonInfo& button_info (ButtonID) const;

	void all_lights_out ();
	void close ();
	void start_midi_handling ();
	void stop_midi_handling ();

	PBD::ScopedConnectionList session_connections;
	void connect_session_signals ();
	void notify_record_state_changed ();
	void notify_transport_state_changed ();

	sigc::connection blink_connection;
	typedef std::list<ButtonID> Blinkers;
	Blinkers blinkers;
	bool blink_state;
	bool blink ();

	void set_current_route (boost::shared_ptr<ARDOUR::Route>);
	void drop_current_route ();
	void use_master ();
	void use_monitor ();
	void gui_track_selection_changed (ARDOUR::RouteNotificationListPtr);
	PBD::ScopedConnection selection_connection;
	PBD::ScopedConnectionList route_connections;

	void map_route_state ();
	void map_solo (bool,void*,bool);
	void map_listen (void*,bool);
	void map_mute (void*);
	void map_recenable ();
	void map_gain ();
	void map_cut ();

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

	void ardour_pan_azimuth (int);
	void ardour_pan_width (int);
	void mixbus_pan (int);
};

}

#endif /* ardour_surface_faderport_h */
