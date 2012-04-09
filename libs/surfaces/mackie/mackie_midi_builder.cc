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
#include "mackie_midi_builder.h"

#include <typeinfo>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

#include "pbd/compose.h"

#include "ardour/debug.h"
#include "controls.h"
#include "midi_byte_array.h"
#include "mackie_port.h"

using namespace PBD;
using namespace Mackie;
using namespace std;

#define NUCLEUS_DEBUG 1

MIDI::byte MackieMidiBuilder::calculate_pot_value( midi_pot_mode mode, const ControlState & state )
{
	// TODO do an exact calc for 0.50? To allow manually re-centering the port.
	
	// center on or off
	MIDI::byte retval = ( state.pos > 0.45 && state.pos < 0.55 ? 1 : 0 ) << 6;
	
	// mode
	retval |= ( mode << 4 );
	
	// value, but only if off hasn't explicitly been set
	if ( state.led_state != off )
		retval += ( int(state.pos * 10.0) + 1 ) & 0x0f; // 0b00001111
	
	return retval;
}

MidiByteArray MackieMidiBuilder::build_led_ring( const Pot & pot, const ControlState & state, midi_pot_mode mode  )
{
	return build_led_ring( pot.led_ring(), state, mode );
}

MidiByteArray MackieMidiBuilder::build_led_ring( const LedRing & led_ring, const ControlState & state, midi_pot_mode mode )
{
	// The other way of doing this:
	// 0x30 + pot/ring number (0-7)
	//, 0x30 + led_ring.ordinal() - 1
	return MidiByteArray ( 3
		// the control type
		, midi_pot_id
		// the id
		, 0x20 + led_ring.raw_id()
		// the value
		, calculate_pot_value( mode, state )
	);
}

MidiByteArray MackieMidiBuilder::build_led( const Button & button, LedState ls )
{
	return build_led( button.led(), ls );
}

MidiByteArray MackieMidiBuilder::build_led( const Led & led, LedState ls )
{
	MIDI::byte state = 0;
	switch ( ls.state() )
	{
		case LedState::on:			state = 0x7f; break;
		case LedState::off:			state = 0x00; break;
		case LedState::none:			state = 0x00; break; // actually, this should never happen.
		case LedState::flashing:	state = 0x01; break;
	}
	
	return MidiByteArray ( 3
		, midi_button_id
		, led.raw_id()
		, state
	);
}

MidiByteArray MackieMidiBuilder::build_fader( const Fader & fader, float pos )
{
	int posi = int( 0x3fff * pos );
	
	return MidiByteArray ( 3
		, midi_fader_id | fader.raw_id()
		// lower-order bits
		, posi & 0x7f
		// higher-order bits
		, ( posi >> 7 )
	);
}

MidiByteArray MackieMidiBuilder::build_meter (const Meter & meter, float val)
{
	MIDI::byte segment = lrintf (val*16.0);
	
	return MidiByteArray (2,
			     0xD0,
			     (meter.raw_id()<<3) | segment);
}

MidiByteArray MackieMidiBuilder::zero_strip( SurfacePort & port, const Strip & strip )
{
	Group::Controls::const_iterator it = strip.controls().begin();
	MidiByteArray retval;
	for (; it != strip.controls().end(); ++it )
	{
		Control & control = **it;
		if ( control.accepts_feedback() )
			retval << zero_control( control );
	}
	
	// These must have sysex headers

	/* XXX: not sure about this check to only display stuff for strips of index < 8 */
	if (strip.index() < 8) {
		retval << strip_display_blank( port, strip, 0 );
		retval << strip_display_blank( port, strip, 1 );
	}
	
	return retval;
}

MidiByteArray MackieMidiBuilder::zero_control( const Control & control )
{
	switch( control.type() ) {
	case Control::type_button:
		return build_led( (Button&)control, off );
		
	case Control::type_led:
		return build_led( (Led&)control, off );
		
	case Control::type_fader:
		return build_fader( (Fader&)control, 0.0 );
		
	case Control::type_pot:
		return build_led_ring( dynamic_cast<const Pot&>( control ), off );
		
	case Control::type_led_ring:
		return build_led_ring( dynamic_cast<const LedRing&>( control ), off );
		
	case Control::type_meter:
		return build_meter (dynamic_cast<const Meter&>(control), 0.0);
		
	default:
		ostringstream os;
		os << "Unknown control type " << control << " in Strip::zero_control";
		throw MackieControlException( os.str() );
	}
}

char translate_seven_segment( char achar )
{
	achar = toupper( achar );
	if ( achar >= 0x40 && achar <= 0x60 )
		return achar - 0x40;
	else if ( achar >= 0x21 && achar <= 0x3f )
      return achar;
	else
      return 0x00;
}

MidiByteArray MackieMidiBuilder::two_char_display( const std::string & msg, const std::string & dots )
{
	if ( msg.length() != 2 ) throw MackieControlException( "MackieMidiBuilder::two_char_display: msg must be exactly 2 characters" );
	if ( dots.length() != 2 ) throw MackieControlException( "MackieMidiBuilder::two_char_display: dots must be exactly 2 characters" );
	
	MidiByteArray bytes( 5, 0xb0, 0x4a, 0x00, 0x4b, 0x00 );
	
	// chars are understood by the surface in right-to-left order
	// could also exchange the 0x4a and 0x4b, above
	bytes[4] = translate_seven_segment( msg[0] ) + ( dots[0] == '.' ? 0x40 : 0x00 );
	bytes[2] = translate_seven_segment( msg[1] ) + ( dots[1] == '.' ? 0x40 : 0x00 );
	
	return bytes;
}

MidiByteArray MackieMidiBuilder::two_char_display (unsigned int value, const std::string & /*dots*/)
{
	ostringstream os;
	os << setfill('0') << setw(2) << value % 100;
	return two_char_display( os.str() );
}

MidiByteArray MackieMidiBuilder::strip_display_blank( SurfacePort & port, const Strip & strip, unsigned int line_number )
{
	// 6 spaces, not 7 because strip_display adds a space where appropriate
	return strip_display( port, strip, line_number, "      " );
}

MidiByteArray MackieMidiBuilder::strip_display (SurfacePort & port, const Strip & strip, unsigned int line_number, const std::string & line )
{
	assert (line_number <= 1);

	MidiByteArray retval;
	uint32_t index = strip.index() % port.strips();

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("MackieMidiBuilder::strip_display index: %1, line %2 = %3\n", strip.index(), line_number, line));

	// sysex header
	retval << port.sysex_hdr();
	
	// code for display
	retval << 0x12;
	// offset (0 to 0x37 first line, 0x38 to 0x6f for second line )
	retval << (index * 7 + (line_number * 0x38));
	
	// ascii data to display
	retval << line;
	// pad with " " out to 6 chars
	for (int i = line.length(); i < 6; ++i) {
		retval << ' ';
	}
	
	// column spacer, unless it's the right-hand column
	if (strip.index() < 7) {
		retval << ' ';
	}

	// sysex trailer
	retval << MIDI::eox;
	
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("MackieMidiBuilder::strip_display midi: %1\n", retval));

	return retval;
}
	
MidiByteArray MackieMidiBuilder::all_strips_display (SurfacePort & /*port*/, std::vector<std::string> & /*lines1*/, std::vector<std::string> & /*lines2*/)
{
	MidiByteArray retval;
	retval << 0x12 << 0;
	// NOTE remember max 112 bytes per message, including sysex headers
	retval << "Not working yet";
	return retval;
}

MidiByteArray MackieMidiBuilder::timecode_display( SurfacePort & port, const std::string & timecode, const std::string & last_timecode )
{
	// if there's no change, send nothing, not even sysex header
	if ( timecode == last_timecode ) return MidiByteArray();
	
	// length sanity checking
	string local_timecode = timecode;
	// truncate to 10 characters
	if ( local_timecode.length() > 10 ) local_timecode = local_timecode.substr( 0, 10 );
	// pad to 10 characters
	while ( local_timecode.length() < 10 ) local_timecode += " ";
		
	// find the suffix of local_timecode that differs from last_timecode
	std::pair<string::const_iterator,string::iterator> pp = mismatch( last_timecode.begin(), last_timecode.end(), local_timecode.begin() );
	
	MidiByteArray retval;
	
	// sysex header
	retval << port.sysex_hdr();
	
	// code for timecode display
	retval << 0x10;
	
	// translate characters. These are sent in reverse order of display
	// hence the reverse iterators
	string::reverse_iterator rend = reverse_iterator<string::iterator>( pp.second );
	for ( string::reverse_iterator it = local_timecode.rbegin(); it != rend; ++it )
	{
		retval << translate_seven_segment( *it );
	}
	
	// sysex trailer
	retval << MIDI::eox;
	
	return retval;
}
