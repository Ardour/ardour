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

#include <glibmm/threads.h>

#include "evoral/Event.h"
#include "evoral/EventSink.h"
#include "ardour/types.h"

namespace ARDOUR {

class MidiBuffer;
class MidiStateTracker;

/**  */
class LIBARDOUR_API RTMidiBuffer : public Evoral::EventSink<samplepos_t>
{
  public:
	typedef samplepos_t TimeType;

	RTMidiBuffer ();
	~RTMidiBuffer();

	void clear();
	void resize(size_t);
	size_t size() const { return _size; }

	uint32_t write (TimeType time, Evoral::EventType type, uint32_t size, const uint8_t* buf);
	uint32_t read (MidiBuffer& dst, samplepos_t start, samplepos_t end, MidiStateTracker& tracker, samplecnt_t offset = 0);

	void dump (uint32_t);
	void reverse ();
	bool reversed() const;

	struct Item {
		samplepos_t timestamp;
		union {
			uint8_t  bytes[4];
			uint32_t offset;
		};
	};

  private:
	friend struct WriteProtectRender;

	struct Blob {
		uint32_t size;
		uint8_t data[0];
	};

	/* The main store. Holds Items (timestamp+up to 3 bytes of data OR
	 * offset into secondary storage below)
	 */

	size_t _size;
	size_t _capacity;
	Item*  _data;
	bool   _reversed;
	/* secondary blob storage. Holds Blobs (arbitrary size + data) */

	uint32_t alloc_blob (uint32_t size);
	uint32_t store_blob (uint32_t size, uint8_t const * data);
	uint32_t _pool_size;
	uint32_t _pool_capacity;
	uint8_t* _pool;

	Glib::Threads::RWLock _lock;

  public:
	class WriteProtectRender {
          public:
		WriteProtectRender (RTMidiBuffer& rtm) : lm (rtm._lock, Glib::Threads::NOT_LOCK) {}
		void acquire () { lm.acquire(); }

          private:
		Glib::Threads::RWLock::WriterLock lm;
	};
};

} // namespace ARDOUR

#endif // __ardour_rt_midi_buffer_h__
