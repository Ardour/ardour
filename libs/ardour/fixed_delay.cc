/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "ardour/audio_buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/fixed_delay.h"
#include "ardour/midi_buffer.h"

using namespace ARDOUR;

FixedDelay::FixedDelay ()
	: _max_delay (0)
	, _delay (0)
	, _pending_flush (false)
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
FixedDelay::configure (const ChanCount& count, framecnt_t max_delay)
{
	if (max_delay <= _max_delay || count <= _count) {
		return;
	}
	_max_delay = std::max (_max_delay, max_delay);
	for (DataType::iterator i = DataType::begin (); i != DataType::end (); ++i) {
		ensure_buffers (*i, count.get (*i), _max_delay + 1);
	}
}

void
FixedDelay::set (const ChanCount& count, framecnt_t delay)
{
	configure (count, delay);
	_delay = delay;
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
FixedDelay::delay (
		ARDOUR::DataType dt, uint32_t id,
		Buffer& out, const Buffer& in,
		pframes_t n_frames,
		framecnt_t dst_offset, framecnt_t src_offset)
{
	if (_delay == 0) {
		out.read_from (in, n_frames, dst_offset, src_offset);
		return;
	}

	assert (dt < _buffers.size ());
	assert (id < _buffers[dt].size ());
	DelayBuffer *db = _buffers[dt][id];

	if (db->pos + n_frames > _max_delay) {
		uint32_t w0 = _max_delay - db->pos;
		uint32_t w1 = db->pos + n_frames - _max_delay;
		db->buf->read_from (in, w0, db->pos, src_offset);
		db->buf->read_from (in, w1, 0, src_offset + w0);
	} else {
		db->buf->read_from (in, n_frames, db->pos, src_offset);
	}

	uint32_t rp = (db->pos + _max_delay - _delay) % _max_delay;

	if (rp + n_frames > _max_delay) {
		uint32_t r0 = _max_delay - rp;
		uint32_t r1 = rp + n_frames - _max_delay;
		out.read_from (*db->buf, r0, dst_offset, rp);
		out.read_from (*db->buf, r1, dst_offset + r0, 0);
	} else {
		out.read_from (*db->buf, n_frames, dst_offset, rp);
	}

	db->pos = (db->pos + n_frames) % _max_delay;
}
