/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_circular_buffer_h_
#define _ardour_circular_buffer_h_

#include "pbd/ringbuffer.h"
#include "pbd/g_atomic_compat.h"

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

/** Endless ringbuffer
 *
 * Writing never fails, and may flush out old data.
 * This is intended for an oscilloscope waveform view.
 */
class LIBARDOUR_API CircularSampleBuffer
{
public:
	CircularSampleBuffer (samplecnt_t size);

	void silence (samplecnt_t);
	void write (Sample const*, samplecnt_t);
	bool read (Sample& s_min, Sample& s_max, samplecnt_t n_samples);

private:
	PBD::RingBuffer<Sample> _rb;
	CircularSampleBuffer (CircularSampleBuffer const&);
};

class LIBARDOUR_API CircularEventBuffer
{
public:
	struct Event {
		/* up to 3 byte MIDI events, 32bit aligned */
		Event (uint8_t const* buf = 0, size_t sz = 0);
		uint8_t data[3];
		uint8_t pad;
	};

	typedef std::vector<Event> EventList;

	CircularEventBuffer (samplecnt_t size);
	~CircularEventBuffer ();

	void reset ();
	void write (uint8_t const*, size_t);
	bool read (EventList&);

private:
	CircularEventBuffer (CircularEventBuffer const&);

	Event* _buf;
	guint  _size;
	guint  _size_mask;

	GATOMIC_QUAL gint _idx;
	GATOMIC_QUAL gint _ack;
};

}
#endif
