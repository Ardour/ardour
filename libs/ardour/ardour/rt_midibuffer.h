/*
 * Copyright (C) 2007-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2014-2016 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_rt_midi_buffer_h__
#define __ardour_rt_midi_buffer_h__

#include <map>

#include "evoral/Event.hpp"
#include "evoral/EventSink.hpp"
#include "ardour/types.h"

namespace ARDOUR {

class MidiBuffer;
class MidiStateTracker;

/**  */
class LIBARDOUR_API RTMidiBuffer : public Evoral::EventSink<samplepos_t>
{
  public:
	typedef samplepos_t TimeType;

	RTMidiBuffer (size_t capacity);
	~RTMidiBuffer();

	void clear() { _size = 0; }
	void resize(size_t);
	size_t size() const { return _size; }

	uint32_t write (TimeType time, Evoral::EventType type, uint32_t size, const uint8_t* buf);
	uint32_t read (MidiBuffer& dst, samplepos_t start, samplepos_t end, MidiStateTracker& tracker, samplecnt_t offset = 0);

	void dump (uint32_t);

  private:
	size_t _size;
	size_t _capacity;
	uint8_t* _data; ///< event data
	typedef std::multimap<TimeType,size_t> Map;
	Map _map;
};

} // namespace ARDOUR

#endif // __ardour_rt_midi_buffer_h__
