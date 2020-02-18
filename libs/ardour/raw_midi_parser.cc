/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#include "ardour/raw_midi_parser.h"

using namespace ARDOUR;

RawMidiParser::RawMidiParser ()
{
	reset ();
}

/* based on AlsaRawMidiIn, some code-dup */
bool
RawMidiParser::process_byte (const uint8_t byte)
{
	if (byte >= 0xf8) {
		// Realtime
		if (byte == 0xfd) {
			return false;
		}
		prepare_byte_event (byte);
		return true;
	}
	if (byte == 0xf7) {
		// Sysex end
		if (_status_byte == 0xf0) {
			record_byte (byte);
			return prepare_buffered_event ();
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
						prepare_byte_event (byte);
						_expected_bytes = 0;
						_status_byte = 0;
						return true;
				}
		}
		record_byte (byte);
		return false;
	}
	// Data byte
	if (!_status_byte) {
		// Data bytes without a status will be discarded.
		_total_bytes++;
		_unbuffered_bytes++;
		return false;
	}
	if (!_total_bytes) {
		record_byte (_status_byte);
	}
	record_byte (byte);
	return (_total_bytes == _expected_bytes) ? prepare_buffered_event () : false;
}
