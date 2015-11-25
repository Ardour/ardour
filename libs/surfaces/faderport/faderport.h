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

class FaderPort : public ARDOUR::ControlProtocol {
  public:
	FaderPort (ARDOUR::Session&);
	virtual ~FaderPort();

	int set_active (bool yn);

	/* It would be nice to send a device query message here to see if
	 * faderport is out there. But the probe() API doesn't provide
	 * a set of ports to be checked, so there's really no nice
	 * way to do this. We would have to fall back on the PortManager
	 * and get a list of all physical ports. Could be done ....
	 */
	static bool probe() { return true; }

	void set_feedback_interval (ARDOUR::microseconds_t);

	int set_feedback (bool yn);
	bool get_feedback () const;

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	bool has_editor () const { return true; }
	void* get_gui () const;
	void  tear_down_gui ();

	void set_current_bank (uint32_t);
	void next_bank ();
	void prev_bank ();

	void reset_controllables ();

	void set_motorised (bool);

	bool motorised () const {
		return _motorised;
	}

	void set_threshold (int);

	int threshold () const {
		return _threshold;
	}

	bool device_active() const { return _device_active; }

  private:
	boost::shared_ptr<ARDOUR::AsyncMIDIPort> _input_port;
	boost::shared_ptr<ARDOUR::AsyncMIDIPort> _output_port;

	ARDOUR::microseconds_t _feedback_interval;
	ARDOUR::microseconds_t last_feedback_time;
	int native_counter;

	bool  do_feedback;
	void  send_feedback ();

	PBD::ScopedConnectionList midi_connections;

	bool midi_input_handler (Glib::IOCondition ioc, boost::shared_ptr<ARDOUR::AsyncMIDIPort> port);

	std::string _current_binding;
	uint32_t _bank_size;
	uint32_t _current_bank;
	/** true if this surface is motorised.  If it is, we assume
	    that the surface's controls are never out of sync with
	    Ardour's state, so we don't have to take steps to avoid
	    values jumping around when things are not in sync.
	*/
	bool _motorised;
	int _threshold;

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
		Read = 10,
		Write = 9,
		Touch = 8,
		Off = 23,
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
		ShiftDown,
		RewindDown,
		StopDown,
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
		{}

		void set_action (std::string const& action_name, bool on_press);
		void set_action (boost::function<void()> function, bool on_press);

		void set_led_state (boost::shared_ptr<MIDI::Port>, int onoff);

		void invoke (ButtonState bs, bool press);

	  private:
		FaderPort& fp;
		std::string name;
		ButtonID id;
		int out;
		ActionType type;
		bool led_on;

		struct {
			std::string action_name;
			boost::function<void()> function;
		} on_press;

		struct {
			std::string action_name;
			boost::function<void()> function;
		} on_release;
	};

	typedef std::map<ButtonID,ButtonInfo> ButtonMap;

	ButtonMap buttons;
	ButtonInfo& button_info (ButtonID) const;

	void all_lights_out ();
	void party ();
};

}

#endif /* ardour_surface_faderport_h */
