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

#include "ardour/circular_buffer.h"
#include "ardour/runtime_functions.h"

using namespace ARDOUR;

CircularSampleBuffer::CircularSampleBuffer (samplecnt_t size)
	: _rb (size)
{
}

void
CircularSampleBuffer::write (Sample const* buf, samplecnt_t n_samples)
{
	guint ws = _rb.write_space ();
	if (ws < n_samples) {
		/* overwrite old data (consider a spinlock wrt ::read) */
		_rb.increment_read_idx (n_samples - ws);
	}
	_rb.write (buf, n_samples);
}

void
CircularSampleBuffer::silence (samplecnt_t n_samples)
{
	guint ws = _rb.write_space ();
	if (ws < n_samples) {
		/* overwrite old data (consider a spinlock wrt ::read) */
		_rb.increment_read_idx (n_samples - ws);
	}
	PBD::RingBuffer<Sample>::rw_vector vec;
	_rb.get_write_vector (&vec);
	if (vec.len[0] >= n_samples) {
		memset (vec.buf[0], 0, sizeof (Sample) * n_samples);
	} else {
		assert (vec.len[0] > 0 && vec.len[0] + vec.len[1] >= n_samples);
		memset (vec.buf[0], 0, sizeof (Sample) * vec.len[0]);
		memset (vec.buf[1], 0, sizeof (Sample) * (n_samples - vec.len[0]));
	}
	_rb.increment_write_idx (n_samples);
}

bool
CircularSampleBuffer::read (Sample& s_min, Sample& s_max, samplecnt_t spp)
{
	s_min = s_max = 0;

	PBD::RingBuffer<Sample>::rw_vector vec;
	_rb.get_read_vector (&vec);

	if (vec.len[0] + vec.len[1] < spp) {
		return false;
	}

	/* immediately mark as read, allow writer to overwrite data if needed */
	_rb.increment_read_idx (spp);

	samplecnt_t to_proc = std::min (spp, (samplecnt_t)vec.len[0]);
	ARDOUR::find_peaks (vec.buf[0], to_proc, &s_min, &s_max);

	to_proc = std::min (spp - to_proc, (samplecnt_t)vec.len[1]);
	if (to_proc > 0) { // XXX is this check needed?
		ARDOUR::find_peaks (vec.buf[1], to_proc, &s_min, &s_max);
	}

	return true;
}

CircularEventBuffer::Event::Event (uint8_t const* buf, size_t size)
{
	switch (size) {
		case 0:
			data[0] = 0;
			data[1] = 0;
			data[2] = 0;
			break;
		case 1:
			data[0] = buf[0];
			data[1] = 0;
			data[2] = 0;
			break;
		case 2:
			data[0] = buf[0];
			data[1] = buf[1];
			data[2] = 0;
			break;
		default:
		case 3:
			data[0] = buf[0];
			data[1] = buf[1];
			data[2] = buf[2];
			break;
	}
	pad = 0;
}

CircularEventBuffer::CircularEventBuffer (samplecnt_t size)
{
	guint power_of_two;
	for (power_of_two = 1; 1U << power_of_two < size; ++power_of_two) {}
	_size = 1 << power_of_two;
	_size_mask = _size;
	_size_mask -= 1;
	_buf = new Event[size];
	reset ();
}

CircularEventBuffer::~CircularEventBuffer ()
{
	delete [] _buf;
}

void
CircularEventBuffer::reset () {
	g_atomic_int_set (&_idx, 0);
	g_atomic_int_set (&_ack, 0);
	memset ((void*)_buf, 0, _size * sizeof (Event));
}

void
CircularEventBuffer::write (uint8_t const* buf, size_t size)
{
	Event e (buf, size);

	guint write_idx = g_atomic_int_get (&_idx);
	memcpy (&_buf[write_idx], &e, sizeof (Event));
	write_idx = (write_idx + 1) & _size_mask;
	g_atomic_int_set (&_idx, write_idx);
	g_atomic_int_set (&_ack, 1);
}

bool
CircularEventBuffer::read (EventList& l)
{
	guint to_read = _size_mask;
	if (!g_atomic_int_compare_and_exchange (&_ack, 1, 0)) {
		return false;
	}

	l.clear ();
	guint priv_idx = g_atomic_int_get (&_idx);
	while (priv_idx > 0) {
		--priv_idx;
		--to_read;
		l.push_back (_buf[priv_idx]);
	}
	priv_idx += _size_mask;
	while (to_read > 0) {
		l.push_back (_buf[priv_idx]);
		--priv_idx;
		--to_read;
	}
	return true;
}
