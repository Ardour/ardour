#ifndef mackie_surface_h
#define mackie_surface_h

#include <stdint.h>

#include "midi++/types.h"

#include "control_protocol/types.h"

#include "controls.h"
#include "types.h"
#include "jog_wheel.h"

namespace MIDI {
	class Parser;
}

namespace ARDOUR {
	class Route;
}

class MidiByteArray;
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

class Surface : public PBD::ScopedConnectionList
{
public:
	Surface (MackieControlProtocol&, const std::string& name, uint32_t number, surface_type_t stype);
	virtual ~Surface();

	surface_type_t type() const { return _stype; }
	uint32_t number() const { return _number; }
	const std::string& name() { return _name; }

	void say_hello ();

	bool active() const { return _active; }
	void drop_routes ();

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

	bool route_is_locked_to_strip (boost::shared_ptr<ARDOUR::Route>) const;

	/// This collection owns the groups
	typedef std::map<std::string,Group*> Groups;
	Groups groups;

	SurfacePort& port() const { return *_port; }

	void map_routes (const std::vector<boost::shared_ptr<ARDOUR::Route> >& routes);

	const MidiByteArray& sysex_hdr() const;

	void periodic (uint64_t now_usecs);

	void handle_midi_pitchbend_message (MIDI::Parser&, MIDI::pitchbend_t, uint32_t channel_id);
	void handle_midi_controller_message (MIDI::Parser&, MIDI::EventTwoBytes*);
	void handle_midi_note_on_message (MIDI::Parser&, MIDI::EventTwoBytes*);

	/// Connect the any signal from the parser to handle_midi_any
	/// unless it's already connected
	void connect_to_signals ();

	/// notification from a MackiePort that it's now inactive
	void handle_port_inactive(Mackie::SurfacePort *);

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
	
	void update_view_mode_display ();
	void update_flip_mode_display ();

	void gui_selection_changed (const ARDOUR::StrongRouteNotificationList&);

	MackieControlProtocol& mcp() const { return _mcp; }

	void next_jog_mode ();
	void set_jog_mode (Mackie::JogWheel::Mode);

  protected:
	
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

	void handle_midi_sysex (MIDI::Parser&, MIDI::byte *, size_t count);
	MidiByteArray host_connection_query (MidiByteArray& bytes);
	MidiByteArray host_connection_confirmation (const MidiByteArray& bytes);

	void init_controls();
	void init_strips (uint32_t n);
	void setup_master ();
	void master_gain_changed ();
	void turn_it_on ();
};

}

#endif
