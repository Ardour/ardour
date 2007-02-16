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
#include "mackie_port.h"

#include "mackie_control_exception.h"
#include "mackie_control_protocol.h"
#include "mackie_midi_builder.h"
#include "controls.h"
#include "surface.h"

#include <midi++/types.h>
#include <midi++/port.h>
#include <sigc++/sigc++.h>
#include <boost/shared_array.hpp>
#include <ardour/configuration.h>

#include "i18n.h"

#include <sstream>

using namespace std;
using namespace Mackie;

// The MCU sysex header
MidiByteArray mackie_sysex_hdr ( 5, MIDI::sysex, 0x0, 0x0, 0x66, 0x10 );

// The MCU extender sysex header
MidiByteArray mackie_sysex_hdr_xt ( 5, MIDI::sysex, 0x0, 0x0, 0x66, 0x11 );

MackiePort::MackiePort( MackieControlProtocol & mcp, MIDI::Port & port, int number, port_type_t port_type )
: SurfacePort( port, number )
, _mcp( mcp )
, _port_type( port_type )
, _emulation( none )
, _initialising( true )
{
	cout << "MackiePort::MackiePort" <<endl;
}

MackiePort::~MackiePort()
{
	close();
}

int MackiePort::strips() const
{
	if ( _port_type == mcu )
	{
		switch ( _emulation )
		{
			// BCF2000 only has 8 faders, so reserve one for master
			case bcf2000: return 7;
			case mackie: return 8;
			case none:
			default:
				throw MackieControlException( "MackiePort::strips: don't know what emulation we're using" );
		}
	}
	else
	{
		// must be an extender, ie no master fader
		return 8;
	}
}

// should really be in MackiePort
void MackiePort::open()
{
	cout << "MackiePort::open " << *this << endl;
	_sysex = port().input()->sysex.connect( ( mem_fun (*this, &MackiePort::handle_midi_sysex) ) );
	
	// make sure the device is connected
	init();
}

void MackiePort::close()
{
	// disconnect signals
	_any.disconnect();
	_sysex.disconnect();
	
	// or emit a "closing" signal
}

const MidiByteArray & MackiePort::sysex_hdr() const
{
	switch ( _port_type )
	{
		case mcu: return mackie_sysex_hdr;
		case ext: return mackie_sysex_hdr_xt;
	}
	cout << "MackiePort::sysex_hdr _port_type not known" << endl;
	return mackie_sysex_hdr;
}

Control & MackiePort::lookup_control( const MidiByteArray & bytes )
{
	Control * control = 0;
	int midi_id = -1;
	MIDI::byte midi_type = bytes[0] & 0xf0; //0b11110000
	switch( midi_type )
	{
		// fader
		case MackieMidiBuilder::midi_fader_id:
			midi_id = bytes[0] & 0x0f;
			control = _mcp.surface().faders[midi_id];
			if ( control == 0 )
			{
				ostringstream os;
				os << "control for fader" << midi_id << " is null";
				throw MackieControlException( os.str() );
			}
			break;
			
		// button
		case MackieMidiBuilder::midi_button_id:
			midi_id = bytes[1];
			control = _mcp.surface().buttons[midi_id];
			if ( control == 0 )
			{
				ostringstream os;
				os << "control for button" << midi_id << " is null";
				throw MackieControlException( os.str() );
			}
			break;
			
		// pot (jog wheel, external control)
		case MackieMidiBuilder::midi_pot_id:
			midi_id = bytes[1] & 0x1f;
			control = _mcp.surface().pots[midi_id];
			if ( control == 0 )
			{
				ostringstream os;
				os << "control for button" << midi_id << " is null";
				throw MackieControlException( os.str() );
			}
			break;
		
		default:
			ostringstream os;
			os << "Cannot find control for " << bytes;
			throw MackieControlException( os.str() );
	}
	return *control;
}

MidiByteArray calculate_challenge_response( MidiByteArray::iterator begin, MidiByteArray::iterator end )
{
	MidiByteArray l;
	back_insert_iterator<MidiByteArray> back ( l );
	copy( begin, end, back );
	
	MidiByteArray retval;
	
	// this is how to calculate the response to the challenge.
	// from the Logic docs.
	retval << ( 0x7f & ( l[0] + ( l[1] ^ 0xa ) - l[3] ) );
	retval << ( 0x7f & ( ( l[2] >> l[3] ) ^ ( l[0] + l[3] ) ) );
	retval << ( 0x7f & ( l[3] - ( l[2] << 2 ) ^ ( l[0] | l[1] ) ) );
	retval << ( 0x7f & ( l[1] - l[2] + ( 0xf0 ^ ( l[3] << 4 ) ) ) );
	
	return retval;
}

// not used right now
MidiByteArray MackiePort::host_connection_query( MidiByteArray & bytes )
{
	// handle host connection query
	cout << "host connection query: " << bytes << endl;
	
	if ( bytes.size() != 18 )
	{
		finalise_init( false );
		ostringstream os;
		os << "expecting 18 bytes, read " << bytes << " from " << port().name();
		throw MackieControlException( os.str() );
	}

	// build and send host connection reply
	MidiByteArray response;
	response << 0x02;
	copy( bytes.begin() + 6, bytes.begin() + 6 + 7, back_inserter( response ) );
	response << calculate_challenge_response( bytes.begin() + 6 + 7, bytes.begin() + 6 + 7 + 4 );
	return response;
}

// not used right now
MidiByteArray MackiePort::host_connection_confirmation( const MidiByteArray & bytes )
{
	cout << "host_connection_confirmation: " << bytes << endl;
	
	// decode host connection confirmation
	if ( bytes.size() != 14 )
	{
		finalise_init( false );
		ostringstream os;
		os << "expecting 14 bytes, read " << bytes << " from " << port().name();
		throw MackieControlException( os.str() );
	}
	
	// send version request
	return MidiByteArray( 2, 0x13, 0x00 );
}

void MackiePort::probe_emulation( const MidiByteArray & bytes )
{
	cout << "MackiePort::probe_emulation: " << bytes.size() << ", " << bytes << endl;
	string version_string;
	for ( int i = 6; i < 11; ++i ) version_string.append( 1, (char)bytes[i] );
	cout << "version_string: " << version_string << endl;
	
	// TODO investigate using serial number. Also, possibly size of bytes might
	// give an indication. Also, apparently MCU sends non-documented messages
	// sometimes.
	if (!_initialising)
	{
		cout << "MackiePort::probe_emulation out of sequence." << endl;
		return;
	}

	// probing doesn't work very well, so just use a config variable
	// to set the emulation mode
	bool emulation_ok = false;
	if ( ARDOUR::Config->get_mackie_emulation() == "bcf" )
	{
		_emulation = bcf2000;
		emulation_ok = true;
	}
	else if ( ARDOUR::Config->get_mackie_emulation() == "mcu" )
	{
		_emulation = mackie;
		emulation_ok = true;
	}
	else
	{
		cout << "unknown mackie emulation: " << ARDOUR::Config->get_mackie_emulation() << endl;
		emulation_ok = false;
	}
	
	finalise_init( emulation_ok );
}

void MackiePort::init()
{
	cout << "MackiePort::init" << endl;
	init_mutex.lock();
	_initialising = true;
	
	cout << "MackiePort::lock acquired" << endl;
	// emit pre-init signal
	init_event();
	
	// kick off initialisation. See docs in header file for init()
	write_sysex ( MidiByteArray (2, 0x13, 0x00 ));
}

void MackiePort::finalise_init( bool yn )
{
	cout << "MackiePort::finalise_init" << endl;
	
	SurfacePort::active( yn );

	if ( yn )
	{
		active_event();
		
		// start handling messages from controls
		_any = port().input()->any.connect( ( mem_fun (*this, &MackiePort::handle_midi_any) ) );
	}
	_initialising = false;
	init_cond.signal();
	init_mutex.unlock();
}

bool MackiePort::wait_for_init()
{
	Glib::Mutex::Lock lock( init_mutex );
	while ( _initialising )
	{
		cout << "MackiePort::wait_for_active waiting" << endl;
		init_cond.wait( init_mutex );
		cout << "MackiePort::wait_for_active released" << endl;
	}
	cout << "MackiePort::wait_for_active returning" << endl;
	return SurfacePort::active();
}

void MackiePort::handle_midi_sysex (MIDI::Parser & parser, MIDI::byte * raw_bytes, size_t count )
{
	MidiByteArray bytes( count, raw_bytes );
	cout << "handle_midi_sysex: " << bytes << endl;
	switch( bytes[5] )
	{
		case 0x01:
			// not used right now
			write_sysex( host_connection_query( bytes ) );
			break;
		case 0x03:
			// not used right now
			write_sysex( host_connection_confirmation( bytes ) );
			break;
		case 0x04:
			inactive_event();
			cout << "host connection error" << bytes << endl;
			break;
		case 0x14:
			probe_emulation( bytes );
			break;
		default:
			cout << "unknown sysex: " << bytes << endl;
	}
}

// converts midi messages into control_event signals
void MackiePort::handle_midi_any (MIDI::Parser & parser, MIDI::byte * raw_bytes, size_t count )
{
	MidiByteArray bytes( count, raw_bytes );
	try
	{
		// ignore sysex messages
		if ( bytes[0] == MIDI::sysex ) return;

		Control & control = lookup_control( bytes );
		
		// This handles incoming bytes. Outgoing bytes
		// are sent by the signal handlers.
		switch ( control.type() )
		{
			// fader
			case Control::type_fader:
				{
					// for a BCF2000, max is 7f for high-order byte and 0x70 for low-order byte
					// According to the Logic docs, these should both be 0x7f.
					// Although it does mention something about only the top-order
					// 10 bits out of 14 being used
					int midi_pos = ( bytes[2] << 7 ) + bytes[1];
					control_event( *this, control, float(midi_pos) / float(0x3fff) );
				}
				break;
				
			// button
			case Control::type_button:
				control_event( *this, control, bytes[2] == 0x7f ? press : release );
				break;
				
			// pot (jog wheel, external control)
			case Control::type_pot:
				{
					ControlState state;
					
					// bytes[2] & 0b01000000 (0x40) give sign
					int sign = ( bytes[2] & 0x40 ) == 0 ? 1 : -1; 
					// bytes[2] & 0b00111111 (0x3f) gives delta
					state.ticks = ( bytes[2] & 0x3f) * sign;
					state.delta = float( state.ticks ) / float( 0x3f );
					
					control_event( *this, control, state );
				}
				break;
			default:
				cerr << "Do not understand control type " << control;
		}
	}
	catch( MackieControlException & e )
	{
		cout << bytes << ' ' << e.what() << endl;
	}
}
