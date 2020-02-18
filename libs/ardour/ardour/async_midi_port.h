/*
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef  __libardour_async_midiport_h__
#define  __libardour_async_midiport_h__

#include <string>
#include <iostream>

#include <boost/function.hpp>

#include "pbd/xml++.h"
#include "pbd/crossthread.h"
#include "pbd/signals.h"
#include "pbd/ringbuffer.h"

#include "evoral/Event.h"

#include "midi++/types.h"
#include "midi++/parser.h"
#include "midi++/port.h"

#include "ardour/event_ring_buffer.h"
#include "ardour/libardour_visibility.h"
#include "ardour/midi_port.h"

namespace ARDOUR {

class LIBARDOUR_API AsyncMIDIPort : public ARDOUR::MidiPort, public MIDI::Port {

	public:
		AsyncMIDIPort (std::string const &, PortFlags);
		~AsyncMIDIPort ();

		bool flush_at_cycle_start () const { return _flush_at_cycle_start; }
		void set_flush_at_cycle_start (bool en) { _flush_at_cycle_start = en; }

		/* called from an RT context */
		void cycle_start (pframes_t nframes);
		void cycle_end (pframes_t nframes);

		/* called from non-RT context */
		void parse (samplecnt_t timestamp);
		int write (const MIDI::byte *msg, size_t msglen, MIDI::timestamp_t timestamp);
		int read (MIDI::byte *buf, size_t bufsize);
		/* waits for output to be cleared */
		void drain (int check_interval_usecs, int total_usecs_to_wait);

		/* clears async request communication channel */
		void clear () {
			_xthread.drain ();
		}

		CrossThreadChannel& xthread() {
			return _xthread;
		}

		/* Not selectable; use ios() */
		int selectable() const { return -1; }
		void set_timer (boost::function<samplecnt_t (void)>&);

		static void set_process_thread (pthread_t);
		static pthread_t get_process_thread () { return _process_thread; }
		static bool is_process_thread();

	private:
		bool                    _currently_in_cycle;
		MIDI::timestamp_t       _last_write_timestamp;
		bool                    _flush_at_cycle_start;
		bool                    have_timer;
		boost::function<samplecnt_t (void)> timer;
		PBD::RingBuffer< Evoral::Event<double> > output_fifo;
		EventRingBuffer<MIDI::timestamp_t> input_fifo;
		Glib::Threads::Mutex output_fifo_lock;
		CrossThreadChannel _xthread;

		int create_port ();

		/** Channel used to signal to the MidiControlUI that input has arrived */

		std::string _connections;
		PBD::ScopedConnection connect_connection;
		PBD::ScopedConnection halt_connection;
		void jack_halted ();
		void make_connections ();
		void init (std::string const &, Flags);

		void flush_output_fifo (pframes_t);

		static pthread_t _process_thread;
};

} // namespace ARDOUR

#endif /* __libardour_async_midiport_h__ */
