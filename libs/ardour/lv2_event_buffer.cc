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

#include <stdint.h>
#include <iostream>
#include "lv2/lv2plug.in/ns/ext/event/event.h"
#include "lv2/lv2plug.in/ns/ext/event/event-helpers.h"
#include "ardour/lv2_event_buffer.h"

using namespace std;

namespace ARDOUR {


/** Allocate a new event buffer.
 * \a capacity is in bytes (not number of events).
 */
LV2EventBuffer::LV2EventBuffer(size_t capacity)
	: _latest_frames(0)
	, _latest_subframes(0)
{
	if (capacity > UINT32_MAX) {
		cerr << "Event buffer size " << capacity << " too large, aborting." << endl;
		throw std::bad_alloc();
	}

	if (capacity == 0) {
		cerr << "ERROR: LV2 event buffer of size 0 created." << endl;
		capacity = 1024;
	}

#ifdef NO_POSIX_MEMALIGN
	_data = (LV2_Event_Buffer*)malloc(sizeof(LV2_Event_Buffer) + capacity);
	int ret = (_data != NULL) ? 0 : -1;
#else
	int ret = posix_memalign((void**)&_data, 16, sizeof(LV2_Event_Buffer) + capacity);
#endif

	if (ret != 0) {
		cerr << "Failed to allocate event buffer.  Aborting." << endl;
		exit(EXIT_FAILURE);
	}

	_data->event_count = 0;
	_data->capacity = (uint32_t)capacity;
	_data->size = 0;
	_data->data = reinterpret_cast<uint8_t*>(_data + 1);

	reset();
}


LV2EventBuffer::~LV2EventBuffer()
{
	free(_data);
}


/** Increment the read position by one event.
 *
 * \return true if increment was successful, or false if end of buffer reached.
 */
bool
LV2EventBuffer::increment() const
{
	if (lv2_event_is_valid(&_iter)) {
		lv2_event_increment(&_iter);
		return true;
	} else {
		return false;
	}
}


/** \return true iff the cursor is valid (ie get_event is safe)
 */
bool
LV2EventBuffer::is_valid() const
{
	return lv2_event_is_valid(&_iter);
}


/** Read an event from the current position in the buffer
 *
 * \return true if read was successful, or false if end of buffer reached
 */
bool
LV2EventBuffer::get_event(uint32_t* frames,
                          uint32_t* subframes,
                          uint16_t* type,
                          uint16_t* size,
                          uint8_t** data) const
{
	if (lv2_event_is_valid(&_iter)) {
		LV2_Event* ev = lv2_event_get(&_iter, data);
		*frames = ev->frames;
		*subframes = ev->subframes;
		*type = ev->type;
		*size = ev->size;
		return true;
	} else {
		return false;
	}
}


/** Append an event to the buffer.
 *
 * \a timestamp must be >= the latest event in the buffer.
 *
 * \return true on success
 */
bool
LV2EventBuffer::append(uint32_t       frames,
                       uint32_t       subframes,
                       uint16_t       type,
                       uint16_t       size,
                       const uint8_t* data)
{
#ifndef NDEBUG
	if (lv2_event_is_valid(&_iter)) {
		LV2_Event* last_event = lv2_event_get(&_iter, NULL);
		assert(last_event->frames < frames
				|| (last_event->frames == frames && last_event->subframes <= subframes));
	}
#endif

	/*cout << "Appending event type " << type << ", size " << size
		<< " @ " << frames << "." << subframes << endl;
	cout << "Buffer capacity " << _data->capacity << ", size " << _data->size << endl;*/

	if (!lv2_event_write(&_iter, frames, subframes, type, size, data)) {
		cerr << "ERROR: Failed to write event." << endl;
		return false;
	} else {
		_latest_frames = frames;
		_latest_subframes = subframes;
		return true;
	}
}


/** Append a buffer of events to the buffer.
 *
 * \a timestamp must be >= the latest event in the buffer.
 *
 * \return true on success
 */
bool
LV2EventBuffer::append(const LV2_Event_Buffer* /*buf*/)
{
	uint8_t** data = NULL;
	bool      ret  = true;

	LV2_Event_Iterator iter;
	for (lv2_event_begin(&iter, _data); lv2_event_is_valid(&iter); lv2_event_increment(&iter)) {
		LV2_Event* ev = lv2_event_get(&iter, data);

#ifndef NDEBUG
		assert((ev->frames > _latest_frames)
				|| (ev->frames == _latest_frames
					&& ev->subframes >= _latest_subframes));
#endif

		if (!(ret = append(ev->frames, ev->subframes, ev->type, ev->size, *data))) {
			cerr << "ERROR: Failed to write event." << endl;
			break;
		}

		_latest_frames = ev->frames;
		_latest_subframes = ev->subframes;
	}

	return ret;
}


} // namespace ARDOUR

