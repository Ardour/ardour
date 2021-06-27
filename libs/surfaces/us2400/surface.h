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

#ifndef us2400_surface_h
#define us2400_surface_h

#include <stdint.h>

#include <sigc++/trackable.h>

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

class US2400Protocol;

namespace US2400
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
	Surface (US2400Protocol&, const std::string& name, uint32_t number, surface_type_t stype);
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

	US2400::JogWheel* jog_wheel() const { return _jog_wheel; }
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

	/// called from US2400Protocol::zero_all to turn things off
	void zero_all ();
	void zero_controls ();

	/// turn off leds around the jog wheel. This is for surfaces that use a pot
	/// pretending to be a jog wheel.
	void blank_jog_ring ();

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

	void subview_mode_changed ();

	US2400Protocol& mcp() const { return _mcp; }

	void next_jog_mode ();
	void set_jog_mode (US2400::JogWheel::Mode);

        void notify_metering_state_changed();
	void turn_it_on ();

	bool connection_handler (boost::weak_ptr<ARDOUR::Port>, std::string name1, boost::weak_ptr<ARDOUR::Port>, std::string name2, bool);

	void master_monitor_may_have_changed ();

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

  private:
	US2400Protocol& _mcp;
	SurfacePort*           _port;
	surface_type_t         _stype;
	uint32_t               _number;
	std::string            _name;
	bool                   _active;
	bool                   _connected;
	US2400::JogWheel*      _jog_wheel;
	Fader*                 _master_fader;
	float                  _last_master_gain_written;
	PBD::ScopedConnection   master_connection;
	bool                   _joystick_active;

	void handle_midi_sysex (MIDI::Parser&, MIDI::byte *, size_t count);
	MidiByteArray host_connection_query (MidiByteArray& bytes);
	MidiByteArray host_connection_confirmation (const MidiByteArray& bytes);

	void say_hello ();
	void init_controls ();
	void init_strips (uint32_t n);
	void setup_master ();
	void master_gain_changed ();

	enum ConnectionState {
		InputConnected = 0x1,
		OutputConnected = 0x2
	};

	int connection_state;

  public:
	/* IP MIDI devices need to keep a handle on this and destroy it */
	GSource*    input_source;
};

}
}

#endif
