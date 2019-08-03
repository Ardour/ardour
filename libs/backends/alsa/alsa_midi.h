/*
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

#ifndef __libbackend_alsa_midi_h__
#define __libbackend_alsa_midi_h__

#include <stdint.h>
#include <poll.h>
#include <pthread.h>

#include "pbd/ringbuffer.h"
#include "ardour/types.h"

/* max bytes per individual midi-event
 * events larger than this are ignored */
#define MaxAlsaMidiEventSize (256)

namespace ARDOUR {

class AlsaMidiIO {
public:
	AlsaMidiIO ();
	virtual ~AlsaMidiIO ();

	int state (void) const { return _state; }
	int start ();
	int stop ();

	void setup_timing (const size_t samples_per_period, const float samplerate);
	void sync_time(uint64_t);

	virtual void* main_process_thread () = 0;

	const std::string & name () const { return _name; }

protected:
	pthread_t _main_thread;
	pthread_mutex_t _notify_mutex;
	pthread_cond_t _notify_ready;

	int  _state;
	bool  _running;

	int _npfds;
	struct pollfd *_pfds;

	double _sample_length_us;
	double _period_length_us;
	size_t _samples_per_period;
	uint64_t _clock_monotonic;

	struct MidiEventHeader {
		uint64_t time;
		size_t size;
		MidiEventHeader(const uint64_t t, const size_t s)
			: time(t)
			, size(s) {}
	};

	PBD::RingBuffer<uint8_t>* _rb;

	std::string _name;

	virtual void init (const char *device_name, const bool input) = 0;

};

class AlsaMidiOut : virtual public AlsaMidiIO
{
public:
	AlsaMidiOut ();

	int send_event (const pframes_t, const uint8_t *, const size_t);
};

class AlsaMidiIn : virtual public AlsaMidiIO
{
public:
	AlsaMidiIn ();

	size_t recv_event (pframes_t &, uint8_t *, size_t &);

protected:
	int queue_event (const uint64_t, const uint8_t *, const size_t);
};

} // namespace

#endif
