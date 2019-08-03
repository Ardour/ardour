/*
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
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

#ifndef __libbackend_alsa_rawmidi_h__
#define __libbackend_alsa_rawmidi_h__

#include <stdint.h>
#include <poll.h>
#include <pthread.h>

#include <alsa/asoundlib.h>

#include "pbd/ringbuffer.h"
#include "ardour/types.h"
#include "alsa_midi.h"

namespace ARDOUR {

class AlsaRawMidiIO : virtual public AlsaMidiIO {
public:
	AlsaRawMidiIO (const std::string &name, const char *device, const bool input);
	virtual ~AlsaRawMidiIO ();

protected:
	snd_rawmidi_t *_device;

private:
	void init (const char *device_name, const bool input);
};

class AlsaRawMidiOut : public AlsaRawMidiIO, public AlsaMidiOut
{
public:
	AlsaRawMidiOut (const std::string &name, const char *device);
	void* main_process_thread ();
};

class AlsaRawMidiIn : public AlsaRawMidiIO, public AlsaMidiIn
{
public:
	AlsaRawMidiIn (const std::string &name, const char *device);

	void* main_process_thread ();

protected:
	int queue_event (const uint64_t, const uint8_t *, const size_t);
private:
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
		const bool result = _unbuffered_bytes == 0;
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

	bool    _first_time;
	size_t  _unbuffered_bytes;
	size_t  _total_bytes;
	size_t  _expected_bytes;
	uint8_t _status_byte;
	uint8_t _parser_buffer[1024];
};

} // namespace

#endif
