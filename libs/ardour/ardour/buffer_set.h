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

#ifndef __ardour_buffer_set_h__
#define __ardour_buffer_set_h__

#include <vector>
#include <ardour/buffer.h>
#include <ardour/chan_count.h>
#include <ardour/data_type.h>
#include <ardour/port_set.h>

namespace ARDOUR {


/** A set of buffers of various types.
 *
 * These are mainly accessed from Session and passed around as scratch buffers
 * (eg as parameters to run() methods) to do in-place signal processing.
 *
 * There are two types of counts associated with a BufferSet - available,
 * and the 'use count'.  Available is the actual number of allocated buffers
 * (and so is the maximum acceptable value for the use counts).
 *
 * The use counts are how things determine the form of their input and inform
 * others the form of their output (eg what they did to the BufferSet).
 * Setting the use counts is realtime safe.
 */
class BufferSet
{
public:
	BufferSet(const PortSet& ports);
	BufferSet();
	~BufferSet();

	void clear();
	
	void ensure_buffers(const ChanCount& chan_count, size_t buffer_capacity);
	void ensure_buffers(size_t num_buffers, DataType type, size_t buffer_capacity);

	// FIXME: add these
	//const ChanCount& available() const { return _count; }
	//ChanCount&       available()       { return _count; }

	const ChanCount& count() const { return _count; }
	ChanCount&       count()       { return _count; }

	size_t available_buffers(DataType type) const;
	size_t buffer_capacity(DataType type) const;

	Buffer& buffer(DataType type, size_t i)
	{
		assert(i <= _count.get(type));
		return *_buffers[type.to_index()][i];
	}

	AudioBuffer& audio_buffer(size_t i)
	{
		return (AudioBuffer&)buffer(DataType::AUDIO, i);
	}
	#if 0
	/** See PortInsert::run for an example of usage */
	class IndexSet {
	public:
		IndexSet() { reset(); }
		
		void reset() { _is[0] = 0; _is[1] = 0; }

		size_t index(DataType type)     { return _is[type.to_index()]; }
		void   increment(DataType type) { _is[type.to_index()] += 1; }

	private:
		int _is[2];
	};
	#endif
	
	const ChanCount& chan_count() const { return _count; }
	
private:
	typedef std::vector<Buffer*> BufferVec;

	/// Vector of vectors, indexed by DataType::to_index()
	std::vector<BufferVec> _buffers;

	/// Use counts (there may be more actual buffers than this)
	ChanCount _count;

	/// Whether we (don't) 'own' the contained buffers (are a mirror of a PortSet)
	bool _is_mirror;
};


} // namespace ARDOUR

#endif // __ardour_buffer_set_h__
