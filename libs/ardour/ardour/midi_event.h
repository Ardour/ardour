/*
    Copyright (C) 2007 Paul Davis 
    Author: Dave Robillard

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __ardour_midi_event_h__
#define __ardour_midi_event_h__

namespace ARDOUR {


/** Identical to jack_midi_event_t, but with double timestamp
 *
 * time is either a frame time (from/to Jack) or a beat time (internal
 * tempo time, used in MidiModel) depending on context.
 */
struct MidiEvent {
	MidiEvent(bool owns_buffer=false, double t=0, size_t s=0, Byte* b=NULL)
		: _owns_buffer(owns_buffer)
		, _time(t)
		, _size(s)
		, _buffer(b)
	{
		if (owns_buffer) {
			_buffer = (Byte*)malloc(_size);
			if (b)
				memcpy(_buffer, b, _size);
			else
				memset(_buffer, 0, _size);
		}
	}
	
	MidiEvent(const MidiEvent& copy, bool owns_buffer)
		: _owns_buffer(owns_buffer)
		, _time(copy._time)
		, _size(copy._size)
		, _buffer(copy._buffer)
	{
		if (owns_buffer) {
			_buffer = (Byte*)malloc(_size);
			if (copy._buffer)
				memcpy(_buffer, copy._buffer, _size);
			else
				memset(_buffer, 0, _size);
		}
	}
	
	~MidiEvent() {
		if (_owns_buffer)
			free(_buffer);
	}

	inline const MidiEvent& operator=(const MidiEvent& copy) {
		_time = copy._time;
		if (!_owns_buffer) {
			_buffer = copy._buffer;
		} else if (copy._buffer) {
			if (!_buffer || _size < copy._size)
				_buffer = (Byte*)realloc(_buffer, copy._size);
			memcpy(_buffer, copy._buffer, copy._size);
		} else {
			free(_buffer);
			_buffer = NULL;
		}
		_size = copy._size;
		return *this;
	}

	inline bool        owns_buffer() const { return _owns_buffer; }
	inline double      time()        const { return _time; }
	inline double&     time()              { return _time; }
	inline size_t      size()        const { return _size; }
	inline size_t&     size()              { return _size; }
	inline uint8_t     type()        const { return (_buffer[0] & 0xF0); }
	inline uint8_t     note()        const { return (_buffer[1]); }
	inline uint8_t     velocity()    const { return (_buffer[2]); }
	inline const Byte* buffer()      const { return _buffer; }
	inline Byte*       buffer()            { return _buffer; }

	void set_buffer(Byte* buf) { assert(!_owns_buffer); _buffer = buf; }

private:
	bool   _owns_buffer; /**< Whether buffer is locally allocated */
	double _time;         /**< Sample index (or beat time) at which event is valid */
	size_t _size;         /**< Number of bytes of data in \a buffer */
	Byte*  _buffer;       /**< Raw MIDI data */
};


} // namespace ARDOUR

#endif /* __ardour_midi_event_h__ */
