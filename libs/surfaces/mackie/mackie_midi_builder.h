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
#ifndef mackie_midi_builder_h
#define mackie_midi_builder_h

#include "midi_byte_array.h"
#include "types.h"
#include "controls.h"

namespace Mackie
{

class SurfacePort;

/**
	This knows how to build midi messages given a control and
	a state.
*/
class MackieMidiBuilder
{
public:
	/**
		The first byte of a midi message from the surface
		will contain one of these, sometimes bitmasked
		with the control id
	*/
	enum midi_types {
		midi_fader_id = Control::type_fader
		, midi_button_id = Control::type_button
		, midi_pot_id = Control::type_pot
	};

	/**
		The LED rings have these modes.
	*/
	enum midi_pot_mode {
		midi_pot_mode_dot = 0
		, midi_pot_mode_boost_cut = 1
		, midi_pot_mode_wrap = 2
		, midi_pot_mode_spread = 3
	};

	MidiByteArray build_led_ring( const Pot & pot, const ControlState &, midi_pot_mode mode = midi_pot_mode_dot );
	MidiByteArray build_led_ring( const LedRing & led_ring, const ControlState &, midi_pot_mode mode = midi_pot_mode_dot );

  	MidiByteArray build_led( const Led & led, LedState ls );
  	MidiByteArray build_led( const Button & button, LedState ls );
	
	MidiByteArray build_fader( const Fader & fader, float pos );

	MidiByteArray build_meter (const Meter& meter, float val);
	
	/// return bytes that will reset all controls to their zero positions
	/// And blank the display for the strip. Pass SurfacePort so we know which sysex header to use.
	MidiByteArray zero_strip( SurfacePort &, const Strip & strip );
	
	// provide bytes to zero the given control
	MidiByteArray zero_control( const Control & control );
	
	// display the first 2 chars of the msg in the 2 char display
	// . is appended to the previous character, so A.B. would
	// be two characters
	MidiByteArray two_char_display( const std::string & msg, const std::string & dots = "  " );
	MidiByteArray two_char_display( unsigned int value, const std::string & dots = "  " );
	
	/**
		Timecode display. Only the difference between timecode and last_timecode will
		be encoded, to save midi bandwidth. If they're the same, an empty array will
		be returned
	*/
	MidiByteArray timecode_display( SurfacePort &, const std::string & timecode, const std::string & last_timecode = "" );
	
	/**
		for displaying characters on the strip LCD
		pass SurfacePort so we know which sysex header to use
	*/
	MidiByteArray strip_display( SurfacePort &, const Strip & strip, unsigned int line_number, const std::string & line );
	
	/// blank the strip LCD, ie write all spaces. Pass SurfacePort so we know which sysex header to use.
	MidiByteArray strip_display_blank( SurfacePort &, const Strip & strip, unsigned int line_number );
	
	/// for generating all strip names. Pass SurfacePort so we know which sysex header to use.
	MidiByteArray all_strips_display( SurfacePort &, std::vector<std::string> & lines1, std::vector<std::string> & lines2 );
	
protected:
	static MIDI::byte calculate_pot_value( midi_pot_mode mode, const ControlState & );
};

}

#endif
