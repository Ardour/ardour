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
#include <sigc++/sigc++.h>

#include "ardour/types.h"
#include "midi++/jack.h"

#ifndef TICKER_H_
#define TICKER_H_

namespace ARDOUR
{

class Session;

class Ticker : public sigc::trackable
{
public:
	Ticker() : _session(0) {};
	virtual ~Ticker() {};
	
	virtual void tick(
		const nframes_t& transport_frames, 
		const BBT_Time& transport_bbt, 
		const SMPTE::Time& transport_smpte) = 0;
	
	virtual void set_session(Session& s);
	virtual void going_away() { _session = 0; }

private:
	Session* _session;
};

class MidiClockTicker : public Ticker
{
	MidiClockTicker() : _jack_port(0) {};
	virtual ~MidiClockTicker() {};
	
	
	void tick(
		const nframes_t& transport_frames, 
		const BBT_Time& transport_bbt, 
		const SMPTE::Time& transport_smpte);
	
	
	void going_away() { Ticker::going_away(); _jack_port = 0;}
	
	void set_midi_port(MIDI::JACK_MidiPort &port);

private:	
	MIDI::JACK_MidiPort* _jack_port;
	
};

}

#endif /* TICKER_H_ */
