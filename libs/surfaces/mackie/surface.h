#ifndef mackie_surface_h
#define mackie_surface_h

#include <stdint.h>

#include "midi++/types.h"

#include "control_protocol/types.h"

#include "controls.h"
#include "types.h"
#include "mackie_jog_wheel.h"

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
	Surface (MackieControlProtocol&, jack_client_t* jack, const std::string& device_name, uint32_t number, surface_type_t stype);
	virtual ~Surface();

	surface_type_t type() const { return _stype; }
	uint32_t number() const { return _number; }

	MackieControlProtocol& mcp() const { return _mcp; }

	bool active() const { return _active; }
	void drop_routes ();

	typedef std::vector<Control*> Controls;
	Controls controls;

	std::map<int,Fader*> faders;
	std::map<int,Pot*> pots;
	std::map<int,Button*> buttons;
	std::map<int,Led*> leds;
	std::map<int,Meter*> meters;

	/// no strip controls in here because they usually
	/// have the same names.
	std::map<std::string,Control*> controls_by_name;
	
	Mackie::JogWheel* jog_wheel() const { return _jog_wheel; }

	/// The collection of all numbered strips. No master
	/// strip in here.
	typedef std::vector<Strip*> Strips;
	Strips strips;

	uint32_t n_strips () const;
	Strip* nth_strip (uint32_t n) const;

	/// This collection owns the groups
	typedef std::map<std::string,Group*> Groups;
	Groups groups;

	SurfacePort& port() const { return *_port; }

	void map_routes (const std::vector<boost::shared_ptr<ARDOUR::Route> >& routes);

	const MidiByteArray& sysex_hdr() const;

	void periodic ();

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

	/// turn off leds around the jog wheel. This is for surfaces that use a pot
	/// pretending to be a jog wheel.
	void blank_jog_ring ();

	bool has_timecode_display() const;
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
	MidiByteArray two_char_display (const std::string & msg, const std::string & dots = "  ");
	MidiByteArray two_char_display (unsigned int value, const std::string & dots = "  ");
	
	/**
		Timecode display. Only the difference between timecode and last_timecode will
		be encoded, to save midi bandwidth. If they're the same, an empty array will
		be returned
	*/
	MidiByteArray timecode_display (const std::string & timecode, const std::string & last_timecode = "");

	void update_view_mode_display ();
	void update_flip_mode_display ();

	void gui_selection_changed (ARDOUR::RouteNotificationListPtr);

  protected:
	void init_controls();
	void init_strips ();

  private:
	MackieControlProtocol& _mcp;
	SurfacePort* _port;
	surface_type_t _stype;
	uint32_t _number;
	bool _active;
	bool _connected;
	Mackie::JogWheel* _jog_wheel;

	void jog_wheel_state_display (Mackie::JogWheel::State state);
};

}

#endif
