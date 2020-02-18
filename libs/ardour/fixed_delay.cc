/*
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#include "ardour/audio_buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/fixed_delay.h"
#include "ardour/midi_buffer.h"

using namespace ARDOUR;

FixedDelay::FixedDelay ()
	: _max_delay (0)
	, _buf_size (0)
	, _delay (0)
{
	for (size_t i = 0; i < DataType::num_types; ++i) {
		_buffers.push_back (BufferVec ());
	}
	_count.reset ();
}

FixedDelay::~FixedDelay ()
{
	clear ();
}

void
FixedDelay::ensure_buffers (DataType type, size_t num_buffers, size_t buffer_capacity)
{
	assert (type != DataType::NIL);
	assert (type < _buffers.size ());
	if (num_buffers == 0) {
		return;
	}
	BufferVec& bufs = _buffers[type];
	if (bufs.size () < num_buffers || (bufs.size () > 0 && bufs[0]->buf->capacity () < buffer_capacity)) {
		for (BufferVec::iterator i = bufs.begin (); i != bufs.end (); ++i) {
			delete (*i);
		}
		bufs.clear ();
		for (size_t i = 0; i < num_buffers; ++i) {
			bufs.push_back (new DelayBuffer (type, buffer_capacity));
		}
		_count.set (type, num_buffers);
	}
}

void
FixedDelay::clear ()
{
	for (std::vector<BufferVec>::iterator i = _buffers.begin (); i != _buffers.end (); ++i) {
		for (BufferVec::iterator j = (*i).begin (); j != (*i).end (); ++j) {
			delete *j;
		}
		(*i).clear ();
	}
	_buffers.clear ();
	_count.reset ();
}

void
FixedDelay::flush()
{
	for (std::vector<BufferVec>::iterator i = _buffers.begin (); i != _buffers.end (); ++i) {
		for (BufferVec::iterator j = (*i).begin (); j != (*i).end (); ++j) {
			(*j)->buf->silence (_buf_size);
		}
	}
}

void
FixedDelay::configure (const ChanCount& count, samplecnt_t max_delay, bool shrink)
{
	if (shrink) {
		if (max_delay == _max_delay && count == _count) {
			return;
		}
		_max_delay = max_delay;
	} else if (max_delay <= _max_delay && count <= _count) {
		return;
	} else {
		_max_delay = std::max (_max_delay, max_delay);
	}

	// max possible (with all engines and during export)
	static const samplecnt_t max_block_length = 8192;
	_buf_size = _max_delay + max_block_length;
	for (DataType::iterator i = DataType::begin (); i != DataType::end (); ++i) {
		ensure_buffers (*i, count.get (*i), _buf_size);
	}
}

void
FixedDelay::set (const ChanCount& count, samplecnt_t delay)
{
	configure (count, delay, false);
	if (_delay != delay) {
		flush ();
	}
	_delay = delay;
}

void
FixedDelay::delay (
		ARDOUR::DataType dt, uint32_t id,
		Buffer& out, const Buffer& in,
		pframes_t n_samples,
		samplecnt_t dst_offset, samplecnt_t src_offset)
{
	if (_delay == 0) {
		out.read_from (in, n_samples, dst_offset, src_offset);
		return;
	}

	assert (dt < _buffers.size ());
	assert (id < _buffers[dt].size ());
	DelayBuffer *db = _buffers[dt][id];

	if (db->pos + n_samples > _buf_size) {
		uint32_t w0 = _buf_size - db->pos;
		uint32_t w1 = db->pos + n_samples - _buf_size;
		db->buf->read_from (in, w0, db->pos, src_offset);
		db->buf->read_from (in, w1, 0, src_offset + w0);
	} else {
		db->buf->read_from (in, n_samples, db->pos, src_offset);
	}

	uint32_t rp = (db->pos + _buf_size - _delay) % _buf_size;

	if (rp + n_samples > _buf_size) {
		uint32_t r0 = _buf_size - rp;
		uint32_t r1 = rp + n_samples - _buf_size;
		out.read_from (*db->buf, r0, dst_offset, rp);
		out.read_from (*db->buf, r1, dst_offset + r0, 0);
	} else {
		out.read_from (*db->buf, n_samples, dst_offset, rp);
	}

	db->pos = (db->pos + n_samples) % _buf_size;
}
