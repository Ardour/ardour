/*
 * Copyright (C) 2014 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2010 Devin Anderson
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

#include <unistd.h>

#include <glibmm.h>

#include "alsa_rawmidi.h"
#include "rt_thread.h"

#include "pbd/error.h"
#include "i18n.h"

using namespace ARDOUR;

/* max bytes per individual midi-event
 * events larger than this are ignored */
#define MaxAlsaRawEventSize (64)

#ifndef NDEBUG
#define _DEBUGPRINT(STR) fprintf(stderr, STR);
#else
#define _DEBUGPRINT(STR) ;
#endif

AlsaRawMidiIO::AlsaRawMidiIO (const char *device, const bool input)
	: _state (-1)
	, _running (false)
	, _device (0)
	, _pfds (0)
	, _sample_length_us (1e6 / 48000.0)
	, _period_length_us (1.024e6 / 48000.0)
	, _samples_per_period (1024)
	, _rb (0)
{
	pthread_mutex_init (&_notify_mutex, 0);
	pthread_cond_init (&_notify_ready, 0);
	init (device, input);
}

AlsaRawMidiIO::~AlsaRawMidiIO ()
{
	if (_device) {
		snd_rawmidi_drain (_device);
		snd_rawmidi_close (_device);
		_device = 0;
	}
	delete _rb;
	pthread_mutex_destroy (&_notify_mutex);
	pthread_cond_destroy (&_notify_ready);
	free (_pfds);
}

void
AlsaRawMidiIO::init (const char *device_name, const bool input)
{
	if (snd_rawmidi_open (
				input ? &_device : NULL,
				input ? NULL : &_device,
				device_name, SND_RAWMIDI_NONBLOCK) < 0) {
		return;
	}

	_npfds = snd_rawmidi_poll_descriptors_count (_device);
	if (_npfds < 1) {
		_DEBUGPRINT("AlsaRawMidiIO: no poll descriptor(s).\n");
		snd_rawmidi_close (_device);
		_device = 0;
		return;
	}
	_pfds = (struct pollfd*) malloc (_npfds * sizeof(struct pollfd));
	snd_rawmidi_poll_descriptors (_device, _pfds, _npfds);

	// MIDI (hw port) 31.25 kbaud
	// worst case here is  8192 SPP and 8KSPS for which we'd need
	// 4000 bytes sans MidiEventHeader.
	// since we're not always in sync, let's use 4096.
	_rb = new RingBuffer<uint8_t>(4096 + 4096 * sizeof(MidiEventHeader));

#if 0
	_state = 0;
#else
	snd_rawmidi_params_t *params;
	if (snd_rawmidi_params_malloc (&params)) {
		goto initerr;
	}
	if (snd_rawmidi_params_current (_device, params)) {
		goto initerr;
	}
	if (snd_rawmidi_params_set_avail_min (_device, params, 1)) {
		goto initerr;
	}
	if ( snd_rawmidi_params_set_buffer_size (_device, params, 64)) {
		goto initerr;
	}
	if (snd_rawmidi_params_set_no_active_sensing (_device, params, 1)) {
		goto initerr;
	}

	_state = 0;
	return;

initerr:
	_DEBUGPRINT("AlsaRawMidiIO: parameter setup error\n");
	snd_rawmidi_close (_device);
	_device = 0;
#endif
	return;
}

static void * pthread_process (void *arg)
{
	AlsaRawMidiIO *d = static_cast<AlsaRawMidiIO *>(arg);
	d->main_process_thread ();
	pthread_exit (0);
	return 0;
}

int
AlsaRawMidiIO::start ()
{
	if (_realtime_pthread_create (SCHED_FIFO, -21, 100000,
				&_main_thread, pthread_process, this))
	{
		if (pthread_create (&_main_thread, NULL, pthread_process, this)) {
			PBD::error << _("AlsaRawMidiIO: Failed to create process thread.") << endmsg;
			return -1;
		} else {
			PBD::warning << _("AlsaRawMidiIO: Cannot acquire realtime permissions.") << endmsg;
		}
	}
	int timeout = 5000;
	while (!_running && --timeout > 0) { Glib::usleep (1000); }
	if (timeout == 0 || !_running) {
		return -1;
	}
	return 0;
}

int
AlsaRawMidiIO::stop ()
{
	void *status;
	if (!_running) {
		return 0;
	}

	_running = false;

	pthread_mutex_lock (&_notify_mutex);
	pthread_cond_signal (&_notify_ready);
	pthread_mutex_unlock (&_notify_mutex);

	if (pthread_join (_main_thread, &status)) {
		PBD::error << _("AlsaRawMidiIO: Failed to terminate.") << endmsg;
		return -1;
	}
	return 0;
}

void
AlsaRawMidiIO::setup_timing (const size_t samples_per_period, const float samplerate)
{
	_period_length_us = (double) samples_per_period * 1e6 / samplerate;
	_sample_length_us = 1e6 / samplerate;
	_samples_per_period = samples_per_period;
}

void
AlsaRawMidiIO::sync_time (const uint64_t tme)
{
	// TODO consider a PLL, if this turns out to be the bottleneck for jitter
	// also think about using
	// snd_pcm_status_get_tstamp() and snd_rawmidi_status_get_tstamp()
	// instead of monotonic clock.
#ifdef DEBUG_TIMING
	double tdiff = (_clock_monotonic + _period_length_us - tme) / 1000.0;
	if (abs(tdiff) >= .05) {
		printf("AlsaRawMidiIO MJ: %.1f ms\n", tdiff);
	}
#endif
	_clock_monotonic = tme;
}

///////////////////////////////////////////////////////////////////////////////

// select sleeps _at most_ (compared to usleep() which sleeps at least)
static void select_sleep (uint32_t usec) {
	if (usec <= 10) return;
	fd_set fd;
	int max_fd=0;
	struct timeval tv;
	tv.tv_sec = usec / 1000000;
	tv.tv_usec = usec % 1000000;
	FD_ZERO (&fd);
	select (max_fd, &fd, NULL, NULL, &tv);
}

///////////////////////////////////////////////////////////////////////////////

AlsaRawMidiOut::AlsaRawMidiOut (const char *device)
		: AlsaRawMidiIO (device, false)
{
}


int
AlsaRawMidiOut::send_event (const pframes_t time, const uint8_t *data, const size_t size)
{
	const uint32_t  buf_size = sizeof (MidiEventHeader) + size;
	if (_rb->write_space() < buf_size) {
		_DEBUGPRINT("AlsaRawMidiOut: ring buffer overflow\n");
		return -1;
	}
	struct MidiEventHeader h (_clock_monotonic + time * _sample_length_us, size);
	_rb->write ((uint8_t*) &h, sizeof(MidiEventHeader));
	_rb->write (data, size);

	if (pthread_mutex_trylock (&_notify_mutex) == 0) {
		pthread_cond_signal (&_notify_ready);
		pthread_mutex_unlock (&_notify_mutex);
	}
	return 0;
}

void *
AlsaRawMidiOut::main_process_thread ()
{
	_running = true;
	pthread_mutex_lock (&_notify_mutex);
	bool need_drain = false;
	while (_running) {
		bool have_data = false;
		struct MidiEventHeader h(0,0);
		uint8_t data[MaxAlsaRawEventSize];

		const uint32_t read_space = _rb->read_space();

		if (read_space > sizeof(MidiEventHeader)) {
			if (_rb->read ((uint8_t*)&h, sizeof(MidiEventHeader)) != sizeof(MidiEventHeader)) {
				_DEBUGPRINT("AlsaRawMidiOut: Garbled MIDI EVENT HEADER!!\n");
				break;
			}
			assert (read_space >= h.size);
			if (h.size > MaxAlsaRawEventSize) {
				_rb->increment_read_idx (h.size);
				_DEBUGPRINT("AlsaRawMidiOut: MIDI event too large!\n");
				continue;
			}
			if (_rb->read (&data[0], h.size) != h.size) {
				_DEBUGPRINT("AlsaRawMidiOut: Garbled MIDI EVENT DATA!!\n");
				break;
			}
			have_data = true;
		}

		if (!have_data) {
			if (need_drain) {
				snd_rawmidi_drain (_device);
				need_drain = false;
			}
			pthread_cond_wait (&_notify_ready, &_notify_mutex);
			continue;
		}

		uint64_t now = g_get_monotonic_time();
		while (h.time > now + 500) {
			if (need_drain) {
				snd_rawmidi_drain (_device);
				need_drain = false;
			} else {
				select_sleep(h.time - now);
			}
			now = g_get_monotonic_time();
		}

retry:
		int perr = poll (_pfds, _npfds, 10 /* ms */);
		if (perr < 0) {
			PBD::error << _("AlsaRawMidiOut: Error polling device. Terminating Midi Thread.") << endmsg;
			break;
		}
		if (perr == 0) {
			_DEBUGPRINT("AlsaRawMidiOut: poll() timed out.\n");
			goto retry;
		}

		unsigned short revents = 0;
		if (snd_rawmidi_poll_descriptors_revents (_device, _pfds, _npfds, &revents)) {
			PBD::error << _("AlsaRawMidiOut: Failed to poll device. Terminating Midi Thread.") << endmsg;
			break;
		}

		if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
			PBD::error << _("AlsaRawMidiOut: poll error. Terminating Midi Thread.") << endmsg;
			break;
		}

		if (!(revents & POLLOUT)) {
			_DEBUGPRINT("AlsaRawMidiOut: POLLOUT not ready.\n");
			select_sleep (1000);
			goto retry;
		}

		ssize_t err = snd_rawmidi_write (_device, data, h.size);

		if ((err == -EAGAIN)) {
			snd_rawmidi_drain (_device);
			goto retry;
		}
		if (err == -EWOULDBLOCK) {
			select_sleep (1000);
			goto retry;
		}
		if (err < 0) {
			PBD::error << _("AlsaRawMidiOut: write failed. Terminating Midi Thread.") << endmsg;
			break;
		}
		if ((size_t) err < h.size) {
			_DEBUGPRINT("AlsaRawMidiOut: short write\n");
			memmove(&data[0], &data[err], err);
			h.size -= err;
			goto retry;
		}
		need_drain = true;
	}

	pthread_mutex_unlock (&_notify_mutex);
	_DEBUGPRINT("AlsaRawMidiOut: MIDI OUT THREAD STOPPED\n");
	return 0;
}


///////////////////////////////////////////////////////////////////////////////

AlsaRawMidiIn::AlsaRawMidiIn (const char *device)
		: AlsaRawMidiIO (device, true)
		, _event(0,0)
		, _first_time(true)
		, _unbuffered_bytes(0)
		, _total_bytes(0)
		, _expected_bytes(0)
		, _status_byte(0)
{
}

size_t
AlsaRawMidiIn::recv_event (pframes_t &time, uint8_t *data, size_t &size)
{
	const uint32_t read_space = _rb->read_space();
	struct MidiEventHeader h(0,0);

	if (read_space <= sizeof(MidiEventHeader)) {
		return 0;
	}

#if 1
	// check if event is in current cycle
	RingBuffer<uint8_t>::rw_vector vector;
	_rb->get_read_vector(&vector);
	if (vector.len[0] >= sizeof(MidiEventHeader)) {
		memcpy((uint8_t*)&h, vector.buf[0], sizeof(MidiEventHeader));
	} else {
		if (vector.len[0] > 0) {
			memcpy ((uint8_t*)&h, vector.buf[0], vector.len[0]);
		}
		memcpy (((uint8_t*)&h) + vector.len[0], vector.buf[1], sizeof(MidiEventHeader) - vector.len[0]);
	}

	if (h.time >= _clock_monotonic + _period_length_us ) {
#ifdef DEBUG_TIMING
		printf("AlsaRawMidiIn DEBUG: POSTPONE EVENT TO NEXT CYCLE: %.1f spl\n", ((h.time - _clock_monotonic) / _sample_length_us));
#endif
		return 0;
	}
	_rb->increment_read_idx (sizeof(MidiEventHeader));
#else
	if (_rb->read ((uint8_t*)&h, sizeof(MidiEventHeader)) != sizeof(MidiEventHeader)) {
		_DEBUGPRINT("AlsaRawMidiIn::recv_event Garbled MIDI EVENT HEADER!!\n");
		return 0;
	}
#endif
	assert (h.size > 0);
	if (h.size > size) {
		_DEBUGPRINT("AlsaRawMidiIn::recv_event MIDI event too large!\n");
		_rb->increment_read_idx (h.size);
		return 0;
	}
	if (_rb->read (&data[0], h.size) != h.size) {
		_DEBUGPRINT("AlsaRawMidiIn::recv_event Garbled MIDI EVENT DATA!!\n");
		return 0;
	}
	if (h.time < _clock_monotonic) {
#ifdef DEBUG_TIMING
		printf("AlsaRawMidiIn DEBUG: MIDI TIME < 0 %.1f spl\n", ((_clock_monotonic - h.time) / -_sample_length_us));
#endif
		time = 0;
	} else if (h.time >= _clock_monotonic + _period_length_us ) {
#ifdef DEBUG_TIMING
		printf("AlsaRawMidiIn DEBUG: MIDI TIME > PERIOD %.1f spl\n", ((h.time - _clock_monotonic) / _sample_length_us));
#endif
		time = _samples_per_period - 1;
	} else {
		time = floor ((h.time - _clock_monotonic) / _sample_length_us);
	}
	assert(time < _samples_per_period);
	size = h.size;
	return h.size;
}

void *
AlsaRawMidiIn::main_process_thread ()
{
	_running = true;
	while (_running) {
		unsigned short revents = 0;

		int perr = poll (_pfds, _npfds, 100 /* ms */);
		if (perr < 0) {
			PBD::error << _("AlsaRawMidiIn: Error polling device. Terminating Midi Thread.") << endmsg;
			break;
		}
		if (perr == 0) {
			continue;
		}

		if (snd_rawmidi_poll_descriptors_revents (_device, _pfds, _npfds, &revents)) {
			PBD::error << _("AlsaRawMidiIn: Failed to poll device. Terminating Midi Thread.") << endmsg;
			break;
		}

		if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
			PBD::error << _("AlsaRawMidiIn: poll error. Terminating Midi Thread.") << endmsg;
			break;
		}

		if (!(revents & POLLIN)) {
			_DEBUGPRINT("AlsaRawMidiOut: POLLIN not ready.\n");
			select_sleep (1000);
			continue;
		}

		uint8_t data[MaxAlsaRawEventSize];
		uint64_t time = g_get_monotonic_time();
		ssize_t err = snd_rawmidi_read (_device, data, sizeof(data));

		if ((err == -EAGAIN) || (err == -EWOULDBLOCK)) {
			continue;
		}
		if (err < 0) {
			PBD::error << _("AlsaRawMidiIn: read error. Terminating Midi") << endmsg;
			break;
		}
		if (err == 0) {
			_DEBUGPRINT("AlsaRawMidiIn: zero read\n");
			continue;
		}

#if 0
		queue_event (time, data, err);
#else
		parse_events (time, data, err);
#endif
	}

	_DEBUGPRINT("AlsaRawMidiIn: MIDI IN THREAD STOPPED\n");
	return 0;
}

int
AlsaRawMidiIn::queue_event (const uint64_t time, const uint8_t *data, const size_t size) {
	const uint32_t  buf_size = sizeof(MidiEventHeader) + size;
	_event._pending = false;

	if (size == 0) {
		return -1;
	}
	if (_rb->write_space() < buf_size) {
		_DEBUGPRINT("AlsaRawMidiIn: ring buffer overflow\n");
		return -1;
	}
	struct MidiEventHeader h (time, size);
	_rb->write ((uint8_t*) &h, sizeof(MidiEventHeader));
	_rb->write (data, size);
	return 0;
}

void
AlsaRawMidiIn::parse_events (const uint64_t time, const uint8_t *data, const size_t size) {
	if (_event._pending) {
		_DEBUGPRINT("AlsaRawMidiIn: queue pending event\n");
		if (queue_event (_event._time, _parser_buffer, _event._size)) {
			return;
		}
	}
	for (size_t i = 0; i < size; ++i) {
		if (_first_time && !(data[i] & 0x80)) {
			continue;
		}
		_first_time = false; /// TODO optimize e.g. use fn pointer to different parse_events()
		if (process_byte(time, data[i])) {
			if (queue_event (_event._time, _parser_buffer, _event._size)) {
				return;
			}
		}
	}
}

// based on JackMidiRawInputWriteQueue by Devin Anderson //
bool
AlsaRawMidiIn::process_byte(const uint64_t time, const uint8_t byte)
{
	if (byte >= 0xf8) {
		// Realtime
		if (byte == 0xfd) {
			return false;
		}
		_parser_buffer[0] = byte;
		prepare_byte_event(time, byte);
		return true;
	}
	if (byte == 0xf7) {
		// Sysex end
		if (_status_byte == 0xf0) {
			record_byte(byte);
			return prepare_buffered_event(time);
		}
    _total_bytes = 0;
    _unbuffered_bytes = 0;
		_expected_bytes = 0;
		_status_byte = 0;
		return false;
	}
	if (byte >= 0x80) {
		// Non-realtime status byte
		if (_total_bytes) {
			_DEBUGPRINT("AlsaRawMidiIn: discarded bogus midi message\n");
#if 0
			for (size_t i=0; i < _total_bytes; ++i) {
				printf("%02x ", _parser_buffer[i]);
			}
			printf("\n");
#endif
			_total_bytes = 0;
			_unbuffered_bytes = 0;
		}
		_status_byte = byte;
		switch (byte & 0xf0) {
			case 0x80:
			case 0x90:
			case 0xa0:
			case 0xb0:
			case 0xe0:
				// Note On, Note Off, Aftertouch, Control Change, Pitch Wheel
				_expected_bytes = 3;
				break;
			case 0xc0:
			case 0xd0:
				// Program Change, Channel Pressure
				_expected_bytes = 2;
				break;
			case 0xf0:
				switch (byte) {
					case 0xf0:
						// Sysex
						_expected_bytes = 0;
						break;
					case 0xf1:
					case 0xf3:
						// MTC Quarter Frame, Song Select
						_expected_bytes = 2;
						break;
					case 0xf2:
						// Song Position
						_expected_bytes = 3;
						break;
					case 0xf4:
					case 0xf5:
						// Undefined
						_expected_bytes = 0;
						_status_byte = 0;
						return false;
					case 0xf6:
						// Tune Request
						prepare_byte_event(time, byte);
						_expected_bytes = 0;
						_status_byte = 0;
						return true;
				}
		}
		record_byte(byte);
		return false;
	}
	// Data byte
	if (! _status_byte) {
		// Data bytes without a status will be discarded.
		_total_bytes++;
		_unbuffered_bytes++;
		return false;
	}
	if (! _total_bytes) {
		_DEBUGPRINT("AlsaRawMidiIn: apply running status\n");
		record_byte(_status_byte);
	}
	record_byte(byte);
	return (_total_bytes == _expected_bytes) ? prepare_buffered_event(time) : false;
}
