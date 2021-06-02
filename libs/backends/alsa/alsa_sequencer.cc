/*
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2018 Paul Davis <paul@linuxaudiosystems.com>
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
#include "alsa_sequencer.h"

#include "pbd/error.h"
#include "pbd/i18n.h"

using namespace ARDOUR;

#ifndef NDEBUG
#define _DEBUGPRINT(STR) fprintf(stderr, STR);
#else
#define _DEBUGPRINT(STR) ;
#endif

AlsaSeqMidiIO::AlsaSeqMidiIO (const std::string &name, const char *device, const bool input)
	: AlsaMidiIO()
	, _seq (0)
{
	_name = name;
	init (device, input);
}

AlsaSeqMidiIO::~AlsaSeqMidiIO ()
{
	if (_seq) {
		snd_seq_close (_seq);
		_seq = 0;
	}
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
			_DEBUGPRINT("AlsaSeqMidiIO: cannot connect input port.\n");
			goto initerr;
		}
	} else {
		if (snd_seq_connect_to (_seq, _port, port.client, port.port) < 0) {
			_DEBUGPRINT("AlsaSeqMidiIO: cannot connect output port.\n");
			goto initerr;
		}
	}

	snd_seq_nonblock(_seq, 1);

	_state = 0;
	return;

initerr:
	PBD::error << _("AlsaSeqMidiIO: Device initialization failed.") << endmsg;
	snd_seq_close (_seq);
	_seq = 0;
	return;
}

///////////////////////////////////////////////////////////////////////////////

AlsaSeqMidiOut::AlsaSeqMidiOut (const std::string &name, const char *device)
		: AlsaSeqMidiIO (name, device, false)
		, AlsaMidiOut ()
{
}

void *
AlsaSeqMidiOut::main_process_thread ()
{
	_running = true;
	bool need_drain = false;
	snd_midi_event_t *alsa_codec = NULL;
	snd_midi_event_new (MaxAlsaMidiEventSize, &alsa_codec);
	pthread_mutex_lock (&_notify_mutex);
	while (_running) {
		bool have_data = false;
		struct MidiEventHeader h(0,0);
		uint8_t data[MaxAlsaMidiEventSize];

		const uint32_t read_space = _rb->read_space();

		if (read_space > sizeof(MidiEventHeader)) {
			if (_rb->read ((uint8_t*)&h, sizeof(MidiEventHeader)) != sizeof(MidiEventHeader)) {
				_DEBUGPRINT("AlsaSeqMidiOut: Garbled MIDI EVENT HEADER!!\n");
				break;
			}
			assert (read_space >= h.size);
			if (h.size > MaxAlsaMidiEventSize) {
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

		if (err == -EAGAIN) {
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

AlsaSeqMidiIn::AlsaSeqMidiIn (const std::string &name, const char *device)
		: AlsaSeqMidiIO (name, device, true)
		, AlsaMidiIn ()
{
}

void *
AlsaSeqMidiIn::main_process_thread ()
{
	_running = true;
	bool do_poll = true;
	snd_midi_event_t *alsa_codec = NULL;
	snd_midi_event_new (MaxAlsaMidiEventSize, &alsa_codec);

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

#if EAGAIN == EWOULDBLOCK
		if (err == -EAGAIN) {
#else
		if ((err == -EAGAIN) || (err == -EWOULDBLOCK)) {
#endif
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

		uint8_t data[MaxAlsaMidiEventSize];
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
