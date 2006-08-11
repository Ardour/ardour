/*
    Copyright (C) 2006 Paul Davis 
    
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

#include <algorithm>
#include <ardour/buffer_set.h>

namespace ARDOUR {


/** Create a BufferSet to mirror a PortSet */
BufferSet::BufferSet(const PortSet& ports)
	: _count(ports.chan_count())
	, _is_mirror(true)
{
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		BufferVec v;

		for (size_t j=0; j < ports.num_ports(*t); ++j) {
			v.push_back(&ports.nth_port_of_type(*t, j)->get_buffer());
		}

		_buffers.push_back(v);
	}
}

/** Create a new, empty BufferSet */
BufferSet::BufferSet()
	: _is_mirror(false)
{
	for (size_t i=0; i < DataType::num_types; ++i)
		_buffers.push_back( BufferVec() );

	_count.reset();
}


BufferSet::~BufferSet()
{
	if (!_is_mirror)
		clear();
}


/** Destroy all contained buffers.
 */
void
BufferSet::clear()
{
	for (std::vector<BufferVec>::iterator i = _buffers.begin(); i != _buffers.end(); ++i) {
		for (BufferVec::iterator j = (*i).begin(); j != (*i).end(); ++j) {
			delete *j;
		}
		(*i).clear();
	}
	_buffers.clear();
}


void
BufferSet::ensure_buffers(const ChanCount& chan_count, size_t buffer_capacity)
{
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		ensure_buffers(chan_count.get(*t), *t, buffer_capacity);
	}
}


/** Ensure that there are @a num_buffers buffers of type @a type available,
 * each of size at least @a buffer_size
 */
void
BufferSet::ensure_buffers(size_t num_buffers, DataType type, size_t buffer_capacity)
{
	assert(type != DataType::NIL);
	assert(type.to_index() < _buffers.size());

	if (num_buffers == 0)
		return;

	// The vector of buffers of the type we care about
	BufferVec& bufs = _buffers[type.to_index()];

	// If there's not enough or they're too small, just nuke the whole thing and
	// rebuild it (so I'm lazy..)
	if (bufs.size() < num_buffers
			|| (bufs.size() > 0 && bufs[0]->capacity() < buffer_capacity)) {

		// Nuke it
		for (BufferVec::iterator i = bufs.begin(); i != bufs.end(); ++i) {
			delete (*i);
		}
		bufs.clear();

		// Rebuild it
		for (size_t i=0; i < num_buffers; ++i) {
			bufs.push_back(Buffer::create(type, buffer_capacity));
		}
	}

	// Post-conditions
	assert(bufs.size() >= num_buffers);
	assert((bufs[0])->type() == type);
	assert(bufs[0]->capacity() >= buffer_capacity);
}

size_t
BufferSet::available_buffers(DataType type) const
{
	return _buffers[type.to_symbol()-1].size();
}


/** Get the capacity (size) of the available buffers of the given type.
 *
 * All buffers of a certain type always have the same capacity.
 */
size_t
BufferSet::buffer_capacity(DataType type) const
{
	assert(available_buffers(type) > 0);
	return _buffers[type.to_index()][0]->capacity();
}


} // namespace ARDOUR

