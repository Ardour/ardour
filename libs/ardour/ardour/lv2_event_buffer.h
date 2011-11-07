/*
    Copyright (C) 2009 Paul Davis
    Author: David Robillard

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_lv2_event_buffer_h__
#define __ardour_lv2_event_buffer_h__

#include "lv2/lv2plug.in/ns/ext/event/event.h"
#include "lv2/lv2plug.in/ns/ext/event/event-helpers.h"

namespace ARDOUR {


class LV2EventBuffer {
public:
	LV2EventBuffer(size_t capacity);
	~LV2EventBuffer();

	inline LV2_Event_Buffer*       data()       { return _data; }
	inline const LV2_Event_Buffer* data() const { return _data; }

	inline void rewind() const { lv2_event_begin(&_iter, _data); }

	inline void reset() {
		_latest_frames = 0;
		_latest_subframes = 0;
		_data->event_count = 0;
		_data->size = 0;
		rewind();
	}

	inline size_t   event_count()      const { return _data->event_count; }
	inline uint32_t capacity()         const { return _data->capacity; }
	inline uint32_t size()             const { return _data->size; }
	inline uint32_t latest_frames()    const { return _latest_frames; }
	inline uint32_t latest_subframes() const { return _latest_subframes; }

	bool increment() const;

	bool is_valid() const;

	bool get_event(uint32_t* frames,
	               uint32_t* subframes,
	               uint16_t* type,
	               uint16_t* size,
	               uint8_t** data) const;

	bool append(uint32_t       frames,
	            uint32_t       subframes,
	            uint16_t       type,
	            uint16_t       size,
	            const uint8_t* data);

	bool append(const LV2_Event_Buffer* buf);

private:
	LV2_Event_Buffer*          _data;             ///< Contents
	mutable LV2_Event_Iterator _iter;             ///< Iterator into _data
	uint32_t                   _latest_frames;    ///< Latest time of all events (frames)
	uint32_t                   _latest_subframes; ///< Latest time of all events (subframes)
};


} // namespace ARDOUR

#endif // __ardour_lv2_event_buffer_h__
