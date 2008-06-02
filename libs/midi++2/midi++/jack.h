/*
    Copyright (C) 2006 Paul Davis 
	Written by Dave Robillard
 
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
 
    $Id: jack.h 4 2005-05-13 20:47:18Z taybin $
*/

#ifndef __jack_midiport_h__
#define __jack_midiport_h__

#include <vector>
#include <string>

#include <fcntl.h>
#include <unistd.h>

#include <glibmm/thread.h>

#include <pbd/ringbuffer.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <midi++/port.h>
#include <midi++/event.h>

namespace MIDI
{


class JACK_MidiPort : public Port
{
public:
	JACK_MidiPort (const XMLNode& node, jack_client_t* jack_client);
	virtual ~JACK_MidiPort ();

	int write(byte *msg, size_t msglen, timestamp_t timestamp);
	int read(byte *buf, size_t max);

	/* No select(2)/poll(2)-based I/O */
	virtual int selectable() const { return -1; }
	
	virtual void cycle_start(nframes_t nframes);

	static std::string typestring;

	virtual XMLNode& get_state () const;
	virtual void set_state (const XMLNode&);

	static void set_process_thread (pthread_t);

  protected:
	std::string get_typestring () const {
		return typestring;
	}

private:
	int create_ports(const XMLNode&);

	jack_client_t* _jack_client;
	jack_port_t*   _jack_input_port;
	jack_port_t*   _jack_output_port;
	nframes_t      _last_read_index;
	timestamp_t    _last_write_timestamp;

	void flush (void* jack_port_buffer);

	static pthread_t _process_thread;
	static bool is_process_thread();

	RingBuffer<MIDI::Event> non_process_thread_fifo;
	Glib::Mutex non_process_thread_fifo_lock;
};


} /* namespace MIDI */

#endif // __jack_midiport_h__

