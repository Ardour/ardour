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

#include <unistd.h>

#include <glibmm.h>

#include "alsa_sequencer.h"
#include "rt_thread.h"

#include "pbd/error.h"
#include "i18n.h"

using namespace ARDOUR;

#ifndef NDEBUG
#define _DEBUGPRINT(STR) fprintf(stderr, STR);
#else
#define _DEBUGPRINT(STR) ;
#endif

AlsaSeqMidiIO::AlsaSeqMidiIO (const char *device, const bool input)
	: _state (-1)
	, _running (false)
	, _seq (0)
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

AlsaSeqMidiIO::~AlsaSeqMidiIO ()
{
	if (_seq) {
		snd_seq_close (_seq);
		_seq = 0;
	}
	delete _rb;
	pthread_mutex_destroy (&_notify_mutex);
	pthread_cond_destroy (&_notify_ready);
	free (_pfds);
}

void
AlsaSeqMidiIO::init (const char *device_name, const bool input)
{
	if (snd_seq_open (&_seq, "hw",
				input ? SND_SEQ_OPEN_INPUT : SND_SEQ_OPEN_OUTPUT, 0) < 0)
	{
		_seq = 0;
		return;
	}

	if (snd_seq_set_client_name (_seq, "Ardour")) {
		_DEBUGPRINT("AlsaSeqMidiIO: cannot set client name.\n");
		goto initerr;
	}

	_port = snd_seq_create_simple_port (_seq, "port", SND_SEQ_PORT_CAP_NO_EXPORT |
			(input ? SND_SEQ_PORT_CAP_WRITE : SND_SEQ_PORT_CAP_READ),
			SND_SEQ_PORT_TYPE_APPLICATION);

	if (_port < 0) {
		_DEBUGPRINT("AlsaSeqMidiIO: cannot create port.\n");
		goto initerr;
	}

	_npfds = snd_seq_poll_descriptors_count (_seq, input ? POLLIN : POLLOUT);
	if (_npfds < 1) {
		_DEBUGPRINT("AlsaSeqMidiIO: no poll descriptor(s).\n");
		goto initerr;
	}
	_pfds = (struct pollfd*) malloc (_npfds * sizeof(struct pollfd));
	snd_seq_poll_descriptors (_seq, _pfds, _npfds, input ? POLLIN : POLLOUT);


	snd_seq_addr_t port;
	if (snd_seq_parse_address (_seq, &port, device_name) < 0) {
		_DEBUGPRINT("AlsaSeqMidiIO: cannot resolve hardware port.\n");
		goto initerr;
	}

	if (input) {
		if (snd_seq_connect_from (_seq, _port, port.client, port.port) < 0) {
			_DEBUGPRINT("AlsaSeqMidiIO: cannot connect port.\n");
			goto initerr;
		}
	} else {
		if (snd_seq_connect_to (_seq, _port, port.client, port.port) < 0) {
			_DEBUGPRINT("AlsaSeqMidiIO: cannot connect port.\n");
			goto initerr;
		}
	}

	snd_seq_nonblock(_seq, 1);

	// MIDI (hw port) 31.25 kbaud
	// worst case here is  8192 SPP and 8KSPS for which we'd need
	// 4000 bytes sans MidiEventHeader.
	// since we're not always in sync, let's use 4096.
	_rb = new RingBuffer<uint8_t>(4096 + 4096 * sizeof(MidiEventHeader));

	_state = 0;
	return;

initerr:
	PBD::error << _("AlsaSeqMidiIO: Device initialization failed.") << endmsg;
	snd_seq_close (_seq);
	_seq = 0;
	return;
}

static void * pthread_process (void *arg)
{
	AlsaSeqMidiIO *d = static_cast<AlsaSeqMidiIO *>(arg);
	d->main_process_thread ();
	pthread_exit (0);
	return 0;
}

int
AlsaSeqMidiIO::start ()
{
	if (_realtime_pthread_create (SCHED_FIFO, -21, 100000,
				&_main_thread, pthread_process, this))
	{
		if (pthread_create (&_main_thread, NULL, pthread_process, this)) {
			PBD::error << _("AlsaSeqMidiIO: Failed to create process thread.") << endmsg;
			return -1;
		} else {
			PBD::warning << _("AlsaSeqMidiIO: Cannot acquire realtime permissions.") << endmsg;
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
AlsaSeqMidiIO::stop ()
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
		PBD::error << _("AlsaSeqMidiIO: Failed to terminate.") << endmsg;
		return -1;
	}
	return 0;
}

void
AlsaSeqMidiIO::setup_timing (const size_t samples_per_period, const float samplerate)
{
	_period_length_us = (double) samples_per_period * 1e6 / samplerate;
	_sample_length_us = 1e6 / samplerate;
	_samples_per_period = samples_per_period;
}

void
AlsaSeqMidiIO::sync_time (const uint64_t tme)
{
	// TODO consider a PLL, if this turns out to be the bottleneck for jitter
	// also think about using
	// snd_pcm_status_get_tstamp() and snd_rawmidi_status_get_tstamp()
	// instead of monotonic clock.
#ifdef DEBUG_TIMING
	double tdiff = (_clock_monotonic + _period_length_us - tme) / 1000.0;
	if (abs(tdiff) >= .05) {
		printf("AlsaSeqMidiIO MJ: %.1f ms\n", tdiff);
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

AlsaSeqMidiOut::AlsaSeqMidiOut (const char *device)
		: AlsaSeqMidiIO (device, false)
{
}


int
AlsaSeqMidiOut::send_event (const pframes_t time, const uint8_t *data, const size_t size)
{
	const uint32_t  buf_size = sizeof (MidiEventHeader) + size;
	if (_rb->write_space() < buf_size) {
		_DEBUGPRINT("AlsaSeqMidiOut: ring buffer overflow\n");
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

#define MaxAlsaSeqEventSize 64

void *
AlsaSeqMidiOut::main_process_thread ()
{
	_running = true;
	bool need_drain = false;
	snd_midi_event_t *alsa_codec = NULL;
	snd_midi_event_new (MaxAlsaSeqEventSize, &alsa_codec);
	pthread_mutex_lock (&_notify_mutex);
	while (_running) {
		bool have_data = false;
		struct MidiEventHeader h(0,0);
		uint8_t data[MaxAlsaSeqEventSize];

		const uint32_t read_space = _rb->read_space();

		if (read_space > sizeof(MidiEventHeader)) {
			if (_rb->read ((uint8_t*)&h, sizeof(MidiEventHeader)) != sizeof(MidiEventHeader)) {
				_DEBUGPRINT("AlsaSeqMidiOut: Garbled MIDI EVENT HEADER!!\n");
				break;
			}
			assert (read_space >= h.size);
			if (h.size > MaxAlsaSeqEventSize) {
				_rb->increment_read_idx (h.size);
				_DEBUGPRINT("AlsaSeqMidiOut: MIDI event too large!\n");
				continue;
			}
			if (_rb->read (&data[0], h.size) != h.size) {
				_DEBUGPRINT("AlsaSeqMidiOut: Garbled MIDI EVENT DATA!!\n");
				break;
			}
			have_data = true;
		}

		if (!have_data) {
			if (need_drain) {
				snd_seq_drain_output (_seq);
				need_drain = false;
			}
			pthread_cond_wait (&_notify_ready, &_notify_mutex);
			continue;
		}

		snd_seq_event_t alsa_event;
		snd_seq_ev_clear (&alsa_event);
		snd_midi_event_reset_encode (alsa_codec);
		if (!snd_midi_event_encode (alsa_codec, data, h.size, &alsa_event)) {
			PBD::error << _("AlsaSeqMidiOut: Invalid Midi Event.") << endmsg;
			continue;
		}

		snd_seq_ev_set_source (&alsa_event, _port);
		snd_seq_ev_set_subs (&alsa_event);
		snd_seq_ev_set_direct (&alsa_event);

		uint64_t now = g_get_monotonic_time();
		while (h.time > now + 500) {
			if (need_drain) {
				snd_seq_drain_output (_seq);
				need_drain = false;
			} else {
				select_sleep(h.time - now);
			}
			now = g_get_monotonic_time();
		}

retry:
		int perr = poll (_pfds, _npfds, 10 /* ms */);
		if (perr < 0) {
			PBD::error << _("AlsaSeqMidiOut: Error polling device. Terminating Midi Thread.") << endmsg;
			break;
		}
		if (perr == 0) {
			_DEBUGPRINT("AlsaSeqMidiOut: poll() timed out.\n");
			goto retry;
		}

		ssize_t err = snd_seq_event_output(_seq, &alsa_event);

		if ((err == -EAGAIN)) {
			snd_seq_drain_output (_seq);
			goto retry;
		}
		if (err == -EWOULDBLOCK) {
			select_sleep (1000);
			goto retry;
		}
		if (err < 0) {
			PBD::error << _("AlsaSeqMidiOut: write failed. Terminating Midi Thread.") << endmsg;
			break;
		}
		need_drain = true;
	}

	pthread_mutex_unlock (&_notify_mutex);

	if (alsa_codec) {
		snd_midi_event_free(alsa_codec);
	}
	_DEBUGPRINT("AlsaSeqMidiOut: MIDI OUT THREAD STOPPED\n");
	return 0;
}

///////////////////////////////////////////////////////////////////////////////

AlsaSeqMidiIn::AlsaSeqMidiIn (const char *device)
		: AlsaSeqMidiIO (device, true)
{
}

size_t
AlsaSeqMidiIn::recv_event (pframes_t &time, uint8_t *data, size_t &size)
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
		printf("AlsaSeqMidiIn DEBUG: POSTPONE EVENT TO NEXT CYCLE: %.1f spl\n", ((h.time - _clock_monotonic) / _sample_length_us));
#endif
		return 0;
	}
	_rb->increment_read_idx (sizeof(MidiEventHeader));
#else
	if (_rb->read ((uint8_t*)&h, sizeof(MidiEventHeader)) != sizeof(MidiEventHeader)) {
		_DEBUGPRINT("AlsaSeqMidiIn::recv_event Garbled MIDI EVENT HEADER!!\n");
		return 0;
	}
#endif
	assert (h.size > 0);
	if (h.size > size) {
		_DEBUGPRINT("AlsaSeqMidiIn::recv_event MIDI event too large!\n");
		_rb->increment_read_idx (h.size);
		return 0;
	}
	if (_rb->read (&data[0], h.size) != h.size) {
		_DEBUGPRINT("AlsaSeqMidiIn::recv_event Garbled MIDI EVENT DATA!!\n");
		return 0;
	}
	if (h.time < _clock_monotonic) {
#ifdef DEBUG_TIMING
		printf("AlsaSeqMidiIn DEBUG: MIDI TIME < 0 %.1f spl\n", ((_clock_monotonic - h.time) / -_sample_length_us));
#endif
		time = 0;
	} else if (h.time >= _clock_monotonic + _period_length_us ) {
#ifdef DEBUG_TIMING
		printf("AlsaSeqMidiIn DEBUG: MIDI TIME > PERIOD %.1f spl\n", ((h.time - _clock_monotonic) / _sample_length_us));
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
AlsaSeqMidiIn::queue_event (const uint64_t time, const uint8_t *data, const size_t size) {
	const uint32_t  buf_size = sizeof(MidiEventHeader) + size;

	if (size == 0) {
		return -1;
	}
	if (_rb->write_space() < buf_size) {
		_DEBUGPRINT("AlsaSeqMidiIn: ring buffer overflow\n");
		return -1;
	}
	struct MidiEventHeader h (time, size);
	_rb->write ((uint8_t*) &h, sizeof(MidiEventHeader));
	_rb->write (data, size);
	return 0;
}

void *
AlsaSeqMidiIn::main_process_thread ()
{
	_running = true;
	bool do_poll = true;
	snd_midi_event_t *alsa_codec = NULL;
	snd_midi_event_new (MaxAlsaSeqEventSize, &alsa_codec);

	while (_running) {

		if (do_poll) {
			snd_seq_poll_descriptors (_seq, _pfds, _npfds, POLLIN);
			int perr = poll (_pfds, _npfds, 100 /* ms */);

			if (perr < 0) {
				PBD::error << _("AlsaSeqMidiIn: Error polling device. Terminating Midi Thread.") << endmsg;
				break;
			}
			if (perr == 0) {
				continue;
			}
		}

		snd_seq_event_t *event;
		uint64_t time = g_get_monotonic_time();
		ssize_t err = snd_seq_event_input (_seq, &event);

		if ((err == -EAGAIN) || (err == -EWOULDBLOCK)) {
			do_poll = true;
			continue;
		}
		if (err == -ENOSPC) {
			PBD::error << _("AlsaSeqMidiIn: FIFO overrun.") << endmsg;
			do_poll = true;
			continue;
		}
		if (err < 0) {
			PBD::error << _("AlsaSeqMidiIn: read error. Terminating Midi") << endmsg;
			break;
		}

		uint8_t data[MaxAlsaSeqEventSize];
		snd_midi_event_reset_decode (alsa_codec);
		ssize_t size = snd_midi_event_decode (alsa_codec, data, sizeof(data), event);

		if (size > 0) {
			queue_event (time, data, size);
		}
		do_poll = (0 == err);
	}

	if (alsa_codec) {
		snd_midi_event_free(alsa_codec);
	}
	_DEBUGPRINT("AlsaSeqMidiIn: MIDI IN THREAD STOPPED\n");
	return 0;
}
