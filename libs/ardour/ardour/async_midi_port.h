/*
    Copyright (C) 1998-2010 Paul Barton-Davis 
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

#ifndef  __libardour_async_midiport_h__
#define  __libardour_async_midiport_h__

#include <string>
#include <iostream>

#include "pbd/xml++.h"
#include "pbd/crossthread.h"
#include "pbd/signals.h"
#include "pbd/ringbuffer.h"

#include "evoral/Event.hpp"
#include "evoral/EventRingBuffer.hpp"

#include "midi++/types.h"
#include "midi++/parser.h"
#include "midi++/port.h"

#include "ardour/libardour_visibility.h"
#include "ardour/midi_port.h"

namespace ARDOUR {

class LIBARDOUR_API AsyncMIDIPort : public ARDOUR::MidiPort, public MIDI::Port {

  public:
        AsyncMIDIPort (std::string const &, PortFlags);
	~AsyncMIDIPort ();

        /* called from an RT context */

	void cycle_start (pframes_t nframes);
	void cycle_end (pframes_t nframes);
    
        /* called from non-RT context */
    
	void parse (framecnt_t timestamp);
        int write (const MIDI::byte *msg, size_t msglen, MIDI::timestamp_t timestamp);
        int read (MIDI::byte *buf, size_t bufsize);
	void drain (int check_interval_usecs);
	int selectable () const { return xthread.selectable(); }

	static void set_process_thread (pthread_t);
	static pthread_t get_process_thread () { return _process_thread; }
	static bool is_process_thread();

  private:	
	bool                    _currently_in_cycle;
        MIDI::timestamp_t       _last_write_timestamp;
	RingBuffer< Evoral::Event<double> > output_fifo;
        Evoral::EventRingBuffer<MIDI::timestamp_t> input_fifo;
        Glib::Threads::Mutex    output_fifo_lock;
	CrossThreadChannel      xthread;

        void flush_output_fifo (pframes_t);

	static pthread_t _process_thread;
};

} // namespace ARDOUR

#endif /* __libardour_async_midiport_h__ */
