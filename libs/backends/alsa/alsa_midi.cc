/*
 * Copyright (C) 2014-2018 Robin Gareus <robin@gareus.org>
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

#include <unistd.h>

#include <glibmm.h>

#include "alsa_midi.h"

#include "pbd/error.h"
#include "pbd/pthread_utils.h"
#include "pbd/i18n.h"

using namespace ARDOUR;

#ifndef NDEBUG
#define _DEBUGPRINT(STR) fprintf(stderr, STR);
#else
#define _DEBUGPRINT(STR) ;
#endif

AlsaMidiIO::AlsaMidiIO ()
	: _state (-1)
	, _running (false)
	, _pfds (0)
	, _sample_length_us (1e6 / 48000.0)
	, _period_length_us (1.024e6 / 48000.0)
	, _samples_per_period (1024)
	, _rb (0)
{
	pthread_mutex_init (&_notify_mutex, 0);
	pthread_cond_init (&_notify_ready, 0);

	// MIDI (hw port) 31.25 kbaud
	// worst case here is  8192 SPP and 8KSPS for which we'd need
	// 4000 bytes sans MidiEventHeader.
	// since we're not always in sync, let's use 4096.
	_rb = new PBD::RingBuffer<uint8_t>(4096 + 4096 * sizeof(MidiEventHeader));
}

AlsaMidiIO::~AlsaMidiIO ()
{
	delete _rb;
	pthread_mutex_destroy (&_notify_mutex);
	pthread_cond_destroy (&_notify_ready);
	free (_pfds);
}

static void * pthread_process (void *arg)
{
	AlsaMidiIO *d = static_cast<AlsaMidiIO *>(arg);
	pthread_set_name ("AlsaMidiIO");
	d->main_process_thread ();
	pthread_exit (0);
	return 0;
}

int
AlsaMidiIO::start ()
{
	if (pbd_realtime_pthread_create (PBD_SCHED_FIFO, PBD_RT_PRI_MIDI, PBD_RT_STACKSIZE_HELP,
				&_main_thread, pthread_process, this))
	{
		if (pbd_pthread_create (PBD_RT_STACKSIZE_HELP, &_main_thread, pthread_process, this)) {
			PBD::error << _("AlsaMidiIO: Failed to create process thread.") << endmsg;
			return -1;
		} else {
			PBD::warning << _("AlsaMidiIO: Cannot acquire realtime permissions.") << endmsg;
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
AlsaMidiIO::stop ()
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
		PBD::error << _("AlsaMidiIO: Failed to terminate.") << endmsg;
		return -1;
	}
	return 0;
}

void
AlsaMidiIO::setup_timing (const size_t samples_per_period, const float samplerate)
{
	_period_length_us = (double) samples_per_period * 1e6 / samplerate;
	_sample_length_us = 1e6 / samplerate;
	_samples_per_period = samples_per_period;
}

void
AlsaMidiIO::sync_time (const uint64_t tme)
{
	// TODO consider a PLL, if this turns out to be the bottleneck for jitter
	// also think about using
	// snd_pcm_status_get_tstamp() and snd_rawmidi_status_get_tstamp()
	// instead of monotonic clock.
#ifdef DEBUG_TIMING
	double tdiff = (_clock_monotonic + _period_length_us - tme) / 1000.0;
	if (abs(tdiff) >= .05) {
		printf("AlsaMidiIO MJ: %.1f ms\n", tdiff);
	}
#endif
	_clock_monotonic = tme;
}

///////////////////////////////////////////////////////////////////////////////

AlsaMidiOut::AlsaMidiOut ()
	: AlsaMidiIO ()
{
}

int
AlsaMidiOut::send_event (const pframes_t time, const uint8_t *data, const size_t size)
{
	const uint32_t  buf_size = sizeof (MidiEventHeader) + size;
	if (_rb->write_space() < buf_size) {
		_DEBUGPRINT("AlsaMidiOut: ring buffer overflow\n");
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

///////////////////////////////////////////////////////////////////////////////

AlsaMidiIn::AlsaMidiIn ()
	: AlsaMidiIO ()
{
}

size_t
AlsaMidiIn::recv_event (pframes_t &time, uint8_t *data, size_t &size)
{
	const uint32_t read_space = _rb->read_space();
	struct MidiEventHeader h(0,0);

	if (read_space <= sizeof(MidiEventHeader)) {
		return 0;
	}

	PBD::RingBuffer<uint8_t>::rw_vector vector;
	_rb->get_read_vector(&vector);
	if (vector.len[0] >= sizeof(MidiEventHeader)) {
		memcpy((uint8_t*)&h, vector.buf[0], sizeof(MidiEventHeader));
	} else {
		if (vector.len[0] > 0) {
			memcpy ((uint8_t*)&h, vector.buf[0], vector.len[0]);
		}
		assert(vector.buf[1]);
		memcpy (((uint8_t*)&h) + vector.len[0], vector.buf[1], sizeof(MidiEventHeader) - vector.len[0]);
	}

	if (h.time >= _clock_monotonic + _period_length_us ) {
#ifdef DEBUG_TIMING
		printf("AlsaMidiIn DEBUG: POSTPONE EVENT TO NEXT CYCLE: %.1f spl\n", ((h.time - _clock_monotonic) / _sample_length_us));
#endif
		return 0;
	}
	_rb->increment_read_idx (sizeof(MidiEventHeader));

	assert (h.size > 0);
	if (h.size > size) {
		_DEBUGPRINT("AlsaMidiIn::recv_event MIDI event too large!\n");
		_rb->increment_read_idx (h.size);
		return 0;
	}
	if (_rb->read (&data[0], h.size) != h.size) {
		_DEBUGPRINT("AlsaMidiIn::recv_event Garbled MIDI EVENT DATA!!\n");
		return 0;
	}
	if (h.time < _clock_monotonic) {
#ifdef DEBUG_TIMING
		printf("AlsaMidiIn DEBUG: MIDI TIME < 0 %.1f spl\n", ((_clock_monotonic - h.time) / -_sample_length_us));
#endif
		time = 0;
	} else if (h.time >= _clock_monotonic + _period_length_us ) {
#ifdef DEBUG_TIMING
		printf("AlsaMidiIn DEBUG: MIDI TIME > PERIOD %.1f spl\n", ((h.time - _clock_monotonic) / _sample_length_us));
#endif
		time = _samples_per_period - 1;
	} else {
		time = floor ((h.time - _clock_monotonic) / _sample_length_us);
	}
	assert(time < _samples_per_period);
	size = h.size;
	return h.size;
}

int
AlsaMidiIn::queue_event (const uint64_t time, const uint8_t *data, const size_t size) {
	const uint32_t  buf_size = sizeof(MidiEventHeader) + size;

	if (size == 0) {
		return -1;
	}
	if (_rb->write_space() < buf_size) {
		_DEBUGPRINT("AlsaMidiIn: ring buffer overflow\n");
		return -1;
	}
	struct MidiEventHeader h (time, size);
	_rb->write ((uint8_t*) &h, sizeof(MidiEventHeader));
	_rb->write (data, size);
	return 0;
}
