/*
 * Copyright (C) 2006-2007 John Anderson
 * Copyright (C) 2012-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef mackie_surface_h
#define mackie_surface_h

#include <stdint.h>

#include <sigc++/trackable.h>

#include "pbd/property_basics.h"
#include "pbd/signals.h"
#include "pbd/xml++.h"
#include "midi++/types.h"

#include "ardour/types.h"

#include "control_protocol/types.h"

#include "controls.h"
#include "types.h"
#include "jog_wheel.h"

namespace MIDI {
	class Parser;
}

namespace ARDOUR {
	class Stripable;
	class Port;
}

class MidiByteArray;

namespace ArdourSurface {

class MackieControlProtocol;

namespace Mackie
{

class MackieButtonHandler;
class SurfacePort;
class MackieMidiBuilder;
class Button;
class Meter;
class Fader;
class Jog;
class Pot;
class Led;

class Surface : public PBD::ScopedConnectionList, public sigc::trackable
{
public:
	Surface (MackieControlProtocol&, const std::string& name, uint32_t number, surface_type_t stype);
	virtual ~Surface();

	surface_type_t type() const { return _stype; }
	uint32_t number() const { return _number; }
	const std::string& name() { return _name; }

	void connected ();

	bool active() const { return _active; }

	typedef std::vector<Control*> Controls;
	Controls controls;

	std::map<int,Fader*> faders;
	std::map<int,Pot*> pots;
	std::map<int,Button*> buttons; // index is device-DEPENDENT
	std::map<int,Led*> leds;
	std::map<int,Meter*> meters;
	std::map<int,Control*> controls_by_device_independent_id;

	Mackie::JogWheel* jog_wheel() const { return _jog_wheel; }
	Fader* master_fader() const { return _master_fader; }

	/// The collection of all numbered strips.
	typedef std::vector<Strip*> Strips;
	Strips strips;

	uint32_t n_strips (bool with_locked_strips = true) const;
	Strip* nth_strip (uint32_t n) const;

	bool stripable_is_locked_to_strip (boost::shared_ptr<ARDOUR::Stripable>) const;
	bool stripable_is_mapped (boost::shared_ptr<ARDOUR::Stripable>) const;

	/// This collection owns the groups
	typedef std::map<std::string,Group*> Groups;
	Groups groups;

	SurfacePort& port() const { return *_port; }

	void map_stripables (const std::vector<boost::shared_ptr<ARDOUR::Stripable> >&);

	void update_strip_selection ();

	const MidiByteArray& sysex_hdr() const;

	void periodic (PBD::microseconds_t now_usecs);
	void redisplay (PBD::microseconds_t now_usecs, bool force);
	void hui_heartbeat ();

	void handle_midi_pitchbend_message (MIDI::Parser&, MIDI::pitchbend_t, uint32_t channel_id);
	void handle_midi_controller_message (MIDI::Parser&, MIDI::EventTwoBytes*);
	void handle_midi_note_on_message (MIDI::Parser&, MIDI::EventTwoBytes*);

	/// Connect the any signal from the parser to handle_midi_any
	/// unless it's already connected
	void connect_to_signals ();

	/// write a sysex message
	void write_sysex (const MidiByteArray& mba);
	void write_sysex (MIDI::byte msg);
	/// proxy write for port
	void write (const MidiByteArray&);

	/// display an indicator of the first switched-in Route. Do nothing by default.
	void display_bank_start (uint32_t /*current_bank*/);

	/// called from MackieControlProtocol::zero_all to turn things off
	void zero_all ();
	void zero_controls ();

	/// turn off leds around the jog wheel. This is for surfaces that use a pot
	/// pretending to be a jog wheel.
	void blank_jog_ring ();

	void display_timecode (const std::string & /*timecode*/, const std::string & /*timecode_last*/);

	/// sends MCP "reset" message to surface
	void reset ();

	void recalibrate_faders ();
	void toggle_backlight ();
	void set_touch_sensitivity (int);

	/**
		This is used to calculate the clicks per second that define
		a transport speed of 1.0 for the jog wheel. 100.0 is 10 clicks
		per second, 50.5 is 5 clicks per second.
	*/
	float scrub_scaling_factor() const;

	/**
		The scaling factor function for speed increase and decrease. At
		low transport speeds this should return a small value, for high transport
		speeds, this should return an exponentially larger value. This provides
		high definition control at low speeds and quick speed changes to/from
		higher speeds.
	*/
	float scaled_delta (float delta, float current_speed);

	// display the first 2 chars of the msg in the 2 char display
	// . is appended to the previous character, so A.B. would
	// be two characters
	void show_two_char_display (const std::string & msg, const std::string & dots = "  ");
	void show_two_char_display (unsigned int value, const std::string & dots = "  ");

	void update_view_mode_display (bool with_helpful_text);
	void update_flip_mode_display ();

	void subview_mode_changed ();

	MackieControlProtocol& mcp() const { return _mcp; }

	void next_jog_mode ();
	void set_jog_mode (Mackie::JogWheel::Mode);

        void notify_metering_state_changed();
	void turn_it_on ();

	void display_message_for (std::string const& msg, uint64_t msecs);

	bool connection_handler (boost::weak_ptr<ARDOUR::Port>, std::string name1, boost::weak_ptr<ARDOUR::Port>, std::string name2, bool);

	void master_monitor_may_have_changed ();

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	bool get_qcon_flag() { return is_qcon; }

  private:
	MackieControlProtocol& _mcp;
	SurfacePort*           _port;
	surface_type_t         _stype;
	uint32_t               _number;
	std::string            _name;
	bool                   _active;
	bool                   _connected;
	Mackie::JogWheel*      _jog_wheel;
	Fader*                 _master_fader;
	float                  _last_master_gain_written;
	PBD::ScopedConnection   master_connection;
	bool                   _has_master_display;
	bool                   _has_master_meter;
	boost::shared_ptr<ARDOUR::Stripable> _master_stripable;
	std::string pending_display[2];
	std::string current_display[2];

	void handle_midi_sysex (MIDI::Parser&, MIDI::byte *, size_t count);
	MidiByteArray host_connection_query (MidiByteArray& bytes);
	MidiByteArray host_connection_confirmation (const MidiByteArray& bytes);

	void say_hello ();
	void init_controls ();
	void init_strips (uint32_t n);
	void setup_master ();
	void master_gain_changed ();
	void master_property_changed (const PBD::PropertyChange&);
	void master_meter_changed ();
	void show_master_name();
	MidiByteArray master_display (uint32_t line_number, const std::string&);		// QCon ProX 2nd LCD master label
	MidiByteArray blank_master_display (uint32_t line_number);

	enum ConnectionState {
		InputConnected = 0x1,
		OutputConnected = 0x2
	};

	int connection_state;

	// QCon Flag
	bool is_qcon;

	MidiByteArray display_line (std::string const& msg, int line_num);

  public:
	/* IP MIDI devices need to keep a handle on this and destroy it */
	GSource*    input_source;
};

}
}

#endif
