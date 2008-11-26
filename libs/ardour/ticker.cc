/*
    Copyright (C) 2008 Hans Baier 

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

    $Id$
*/
#include "ardour/ticker.h"
#include "ardour/session.h"

namespace ARDOUR
{


void Ticker::set_session(Session& s) 
{
	 _session = &s;
	 
	 if(_session) {
		 _session->tick.connect(mem_fun (*this, &Ticker::tick));
		 _session->GoingAway.connect(mem_fun (*this, &Ticker::going_away));
	 }
}


void MidiClockTicker::tick(const nframes_t& transport_frames, const BBT_Time& transport_bbt, const SMPTE::Time& transport_smpt)
{
	
}

}

