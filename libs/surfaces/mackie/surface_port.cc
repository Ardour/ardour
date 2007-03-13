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
#include "surface_port.h"

#include "mackie_control_exception.h"
#include "controls.h"

#include <midi++/types.h>
#include <midi++/port.h>
#include <sigc++/sigc++.h>
#include <boost/shared_array.hpp>

#include "i18n.h"

#define _XOPEN_SOURCE 600  // force XSI for non-GNU strerror_r()

#include <sstream>
#include <cstring>
#include <cerrno>


using namespace std;
using namespace Mackie;

SurfacePort::SurfacePort( MIDI::Port & port, int number )
: _port( port ), _number( number ), _active( false )
{
}

SurfacePort::~SurfacePort()
{
	//cout << "~SurfacePort::SurfacePort()" << endl;
	// make sure another thread isn't reading or writing as we close the port
	Glib::RecMutex::Lock lock( _rwlock );
	_active = false;
	//cout << "~SurfacePort::SurfacePort() finished" << endl;
}

MidiByteArray SurfacePort::read()
{
	const int max_buf_size = 512;
	MIDI::byte buf[max_buf_size];
	MidiByteArray retval;

	// check active. Mainly so that the destructor
	// doesn't destroy the mutex while it's still locked
	if ( !active() ) return retval;
	
	// return nothing read if the lock isn't acquired
	Glib::RecMutex::Lock lock( _rwlock, Glib::TRY_LOCK );
		
	if ( !lock.locked() )
	{
		//cout << "SurfacePort::read not locked" << endl;
		return retval;
	}
	
	// check active again - destructor sequence
	if ( !active() ) return retval;
	
	// read port and copy to return value
	int nread = port().read( buf, sizeof (buf) );

	if (nread >= 0) {
		retval.copy( nread, buf );
		if ((size_t) nread == sizeof (buf))
		{
			retval << read();
		}
	}
	else
	{
		if ( errno != EAGAIN )
		{
			ostringstream os;
			os << "Surface: error reading from port: " << port().name() << ": " << errno;

			char buf[512];
			int result = strerror_r( errno, buf, 512 );
			if (!result) {
				os << " " << buf;
			} 
			
			cout << os.str() << endl;
			inactive_event();
			throw MackieControlException( os.str() );
		}
	}
	return retval;
}

void SurfacePort::write( const MidiByteArray & mba )
{
	//if ( mba[0] == 0xf0 ) cout << "SurfacePort::write: " << mba << endl;
	//cout << "SurfacePort::write: " << mba << endl;
	
	// check active before and after lock - to make sure
	// that the destructor doesn't destroy the mutex while
	// it's still in use
	if ( !active() ) return;
	Glib::RecMutex::Lock lock( _rwlock );
	if ( !active() ) return;

	int count = port().write( mba.bytes().get(), mba.size() );
	if ( count != (int)mba.size() )
	{
		if ( errno != EAGAIN )
		{
			ostringstream os;
			os << "Surface: couldn't write to port " << port().name() << ": " << errno;
			char buf[512];
			int result = strerror_r( errno, buf, 512 );

			if (!result) {
				os << " " << buf;
			}

			cout << os.str();
			inactive_event();
			throw MackieControlException( os.str() );
		}
	}
	//if ( mba[0] == 0xf0 ) cout << "SurfacePort::write " << count << endl;
}

void SurfacePort::write_sysex( const MidiByteArray & mba )
{
	MidiByteArray buf;
	buf << sysex_hdr() << mba << MIDI::eox;
	write( buf );
}

void SurfacePort::write_sysex( MIDI::byte msg )
{
	MidiByteArray buf;
	buf << sysex_hdr() << msg << MIDI::eox;
	write( buf );
}

// This should be moved to midi++ at some point
ostream & operator << ( ostream & os, const MIDI::Port & port )
{
	os << "device: " << port.device();
	os << "; ";
	os << "name: " << port.name();
	os << "; ";
	os << "type: " << port.type();
	os << "; ";
	os << "mode: " << port.mode();
	os << "; ";
	os << "ok: " << port.ok();
	os << "; ";
	os << "number: " << port.number();
	os << "; ";
	return os;
}

ostream & Mackie::operator << ( ostream & os, const SurfacePort & port )
{
	os << "{ ";
	os << "device: " << port.port().device();
	os << "; ";
	os << "name: " << port.port().name();
	os << "; ";
	os << "number: " << port.number();
	os << " }";
	return os;
}
