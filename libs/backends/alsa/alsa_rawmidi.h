/*
 * Copyright (C) 2014 Robin Gareus <robin@gareus.org>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __libbackend_alsa_rawmidi_h__
#define __libbackend_alsa_rawmidi_h__

#include <stdint.h>
#include <poll.h>
#include <pthread.h>

#include <alsa/asoundlib.h>

#include "pbd/ringbuffer.h"
#include "ardour/types.h"

namespace ARDOUR {

class AlsaRawMidiIO {
public:
	AlsaRawMidiIO (const char *device, const bool input);
	virtual ~AlsaRawMidiIO ();

	int state (void) const { return _state; }
	int start ();
	int stop ();

	void setup_timing (const size_t samples_per_period, const float samplerate);
	void sync_time(uint64_t);

	virtual void* main_process_thread () = 0;

protected:
	pthread_t _main_thread;
	pthread_mutex_t _notify_mutex;
	pthread_cond_t _notify_ready;

	int  _state;
	bool  _running;

	snd_rawmidi_t *_device;
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

	RingBuffer<uint8_t>* _rb;

private:
	void init (const char *device_name, const bool input);

};

class AlsaRawMidiOut : public AlsaRawMidiIO
{
public:
	AlsaRawMidiOut (const char *device);

	void* main_process_thread ();
	int send_event (const pframes_t, const uint8_t *, const size_t);
};

class AlsaRawMidiIn : public AlsaRawMidiIO
{
public:
	AlsaRawMidiIn (const char *device);

	void* main_process_thread ();

	size_t recv_event (pframes_t &, uint8_t *, size_t &);

private:
	int queue_event (const uint64_t, const uint8_t *, const size_t);
	void parse_events (const uint64_t, const uint8_t *, const size_t);
	bool process_byte (const uint64_t, const uint8_t);

	void record_byte(uint8_t byte) {
		if (_total_bytes < sizeof(_parser_buffer)) {
			_parser_buffer[_total_bytes] = byte;
		} else {
			++_unbuffered_bytes;
		}
		++_total_bytes;
	}

	void prepare_byte_event(const uint64_t time, const uint8_t byte) {
		_parser_buffer[0] = byte;
		_event.prepare(time, 1);
	}

	bool prepare_buffered_event(const uint64_t time) {
		const bool result = !_unbuffered_bytes;
		if (result) {
			_event.prepare(time, _total_bytes);
		}
		_total_bytes = 0;
		_unbuffered_bytes = 0;
		if (_status_byte >= 0xf0) {
			_expected_bytes = 0;
			_status_byte = 0;
		}
		return result;
	}

	struct ParserEvent {
		uint64_t _time;
		size_t _size;
		bool _pending;
		ParserEvent (const uint64_t time, const size_t size)
			: _time(time)
			, _size(size)
			, _pending(false) {}

		void prepare(const uint64_t time, const size_t size) {
			_time = time;
			_size = size;
			_pending = true;
		}
	} _event;

	size_t  _unbuffered_bytes;
	size_t  _total_bytes;
	size_t  _expected_bytes;
	uint8_t _status_byte;
	uint8_t _parser_buffer[1024];
};

} // namespace

#endif
