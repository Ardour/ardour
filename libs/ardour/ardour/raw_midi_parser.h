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

#ifndef _ardour_raw_midi_parser_h_
#define _ardour_raw_midi_parser_h_

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

class LIBARDOUR_API RawMidiParser
{
public:
	RawMidiParser ();

	void reset () {
		_event_size = 0;
		_unbuffered_bytes = 0;
		_total_bytes = 0;
		_expected_bytes = 0;
		_status_byte = 0;
	}

	uint8_t const * midi_buffer () const { return  _parser_buffer; }
	size_t buffer_size () const { return _event_size; }

	/** parse a MIDI byte
	 * @return true if message is complete, false if more data is needed
	 */
	bool process_byte (const uint8_t byte);

private:

	void record_byte (uint8_t byte) {
		if (_total_bytes < sizeof (_parser_buffer)) {
			_parser_buffer[_total_bytes] = byte;
		} else {
			++_unbuffered_bytes;
		}
		++_total_bytes;
	}

	void prepare_byte_event (const uint8_t byte) {
		_parser_buffer[0] = byte;
		_event_size = 1;
	}

	bool prepare_buffered_event () {
		const bool result = _unbuffered_bytes == 0;
		if (result) {
			_event_size = _total_bytes;
		}
		_total_bytes = 0;
		_unbuffered_bytes = 0;
		if (_status_byte >= 0xf0) {
			_expected_bytes = 0;
			_status_byte = 0;
		}
		return result;
	}

	size_t  _event_size;
	size_t  _unbuffered_bytes;
	size_t  _total_bytes;
	size_t  _expected_bytes;
	uint8_t _status_byte;
	uint8_t _parser_buffer[1024];
};

} // namespace ARDOUR

#endif /* _ardour_raw_midi_parser_h_ */
