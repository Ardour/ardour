/*
 * Copyright (C) 2010 Devin Anderson <surfacepatterns@gmail.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include "select_sleep.h"
#include "alsa_rawmidi.h"

#include "pbd/error.h"
#include "pbd/i18n.h"

using namespace ARDOUR;

#ifndef NDEBUG
#define _DEBUGPRINT(STR) fprintf(stderr, STR);
#else
#define _DEBUGPRINT(STR) ;
#endif

AlsaRawMidiIO::AlsaRawMidiIO (const std::string &name, const char *device, const bool input)
	: AlsaMidiIO()
	, _device (0)
{
	_name = name;
	init (device, input);
}

AlsaRawMidiIO::~AlsaRawMidiIO ()
{
	if (_device) {
		snd_rawmidi_drain (_device);
		snd_rawmidi_close (_device);
		_device = 0;
	}
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
	if (snd_rawmidi_params_set_buffer_size (_device, params, 64)) {
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

///////////////////////////////////////////////////////////////////////////////

AlsaRawMidiOut::AlsaRawMidiOut (const std::string &name, const char *device)
		: AlsaRawMidiIO (name, device, false)
		, AlsaMidiOut ()
{
}

void *
AlsaRawMidiOut::main_process_thread ()
{
	_running = true;
	pthread_mutex_lock (&_notify_mutex);
	unsigned int need_drain = 0;
	while (_running) {
		bool have_data = false;
		struct MidiEventHeader h(0,0);
		uint8_t data[MaxAlsaMidiEventSize];

		const uint32_t read_space = _rb->read_space();

		if (read_space > sizeof(MidiEventHeader)) {
			if (_rb->read ((uint8_t*)&h, sizeof(MidiEventHeader)) != sizeof(MidiEventHeader)) {
				_DEBUGPRINT("AlsaRawMidiOut: Garbled MIDI EVENT HEADER!!\n");
				break;
			}
			assert (read_space >= h.size);
			if (h.size > MaxAlsaMidiEventSize) {
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
			if (need_drain > 0) {
				snd_rawmidi_drain (_device);
				need_drain = 0;
			}
			pthread_cond_wait (&_notify_ready, &_notify_mutex);
			continue;
		}

		uint64_t now = g_get_monotonic_time();
		while (h.time > now + 500) {
			if (need_drain > 0) {
				snd_rawmidi_drain (_device);
				need_drain = 0;
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

#if 0 // DEBUG -- not rt-safe
		printf("TX [%ld | %ld]", h.size, err);
		for (size_t i = 0; i < h.size; ++i) {
			printf (" %02x", data[i]);
		}
		printf ("\n");
#endif

		if (err == -EAGAIN) {
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

		if ((need_drain += h.size) >= 64) {
			snd_rawmidi_drain (_device);
			need_drain = 0;
		}
	}

	pthread_mutex_unlock (&_notify_mutex);
	_DEBUGPRINT("AlsaRawMidiOut: MIDI OUT THREAD STOPPED\n");
	return 0;
}


///////////////////////////////////////////////////////////////////////////////

AlsaRawMidiIn::AlsaRawMidiIn (const std::string &name, const char *device)
		: AlsaRawMidiIO (name, device, true)
		, AlsaMidiIn ()
		, _event(0,0)
		, _first_time(true)
		, _unbuffered_bytes(0)
		, _total_bytes(0)
		, _expected_bytes(0)
		, _status_byte(0)
{
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

		uint8_t data[MaxAlsaMidiEventSize];
		uint64_t time = g_get_monotonic_time();
		ssize_t err = snd_rawmidi_read (_device, data, sizeof(data));

#if EAGAIN != EWOULDBLOCK
		if ((err == -EAGAIN) || (err == -EWOULDBLOCK))  {
#else
		if (err == -EAGAIN) {
#endif
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
	_event._pending = false;
	return AlsaMidiIn::queue_event(time, data, size);
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
