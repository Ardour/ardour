/*
 *   Copyright (C) 2006 Paul Davis 
 *   Copyright (C) 2007 Michael Taht
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *  
 */

#include <iostream>
#include <algorithm>
#include <cmath>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <float.h>
#include <sys/time.h>
#include <errno.h>
#include "ardour/route.h"
#include "ardour/audio_track.h"
#include "ardour/session.h"
#include "ardour/location.h"
#include "ardour/dB.h"

using namespace ARDOUR;
using namespace std;
using namespace sigc;
using namespace PBD;

#include "i18n.h"

#include "pbd/abstract_ui.cc"

#include "tranzport_control_protocol.h"


// FIXME: How to handle multiple tranzports in a system?

XMLNode&
TranzportControlProtocol::get_state () 
{
	return ControlProtocol::get_state();
}

int
TranzportControlProtocol::set_state (const XMLNode& node)
{
	cout << "TranzportControlProtocol::set_state: active " << _active << endl;
	int retval = 0;

// I think I want to make these strings rather than numbers
#if 0		
	// fetch current display mode
	if ( node.property( X_("display_mode") ) != 0 )
	{
		string display = node.property( X_("display_mode") )->value();
		try
		{
			set_active( true );
			int32_t new_display = atoi( display.c_str() );
			if ( display_mode != new_display ) display_mode = (DisplayMode)new_display;
		}
		catch ( exception & e )
		{
			cout << "exception in TranzportControlProtocol::set_state: " << e.what() << endl;
			return -1;
		}
	}

	if ( node.property( X_("wheel_mode") ) != 0 )
	{
		string wheel = node.property( X_("wheel_mode") )->value();
		try
		{
			int32_t new_wheel = atoi( wheel.c_str() );
			if ( wheel_mode != new_wheel ) wheel_mode = (WheelMode) new_wheel;
		}
		catch ( exception & e )
		{
			cout << "exception in TranzportControlProtocol::set_state: " << e.what() << endl;
			return -1;
		}
	}

	// fetch current bling mode
	if ( node.property( X_("bling") ) != 0 )
	{
		string bling = node.property( X_("bling_mode") )->value();
		try
		{
			int32_t new_bling = atoi( bling.c_str() );
			if ( bling_mode != new_bling ) bling_mode = (BlingMode) new_bling;
		}
		catch ( exception & e )
		{
			cout << "exception in TranzportControlProtocol::set_state: " << e.what() << endl;
			return -1;
		}
	}
#endif 

	return retval;

}

// These are intended for the day we have more options for tranzport modes
// And perhaps we could load up sessions this way, too

int
TranzportControlProtocol::save (char *name) 
{
	// Presently unimplemented
	return 0;
}

int
TranzportControlProtocol::load (char *name) 
{
	// Presently unimplemented
	return 0;
}

int
TranzportControlProtocol::save_config (char *name) 
{
	// Presently unimplemented
	return 0;
}

int
TranzportControlProtocol::load_config (char *name) 
{
	// Presently unimplemented
	return 0;
}
