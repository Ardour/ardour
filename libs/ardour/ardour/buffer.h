/*
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2014 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_buffer_h__
#define __ardour_buffer_h__

#include <stddef.h>

#include <boost/utility.hpp>

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/data_type.h"

namespace ARDOUR {


/** A buffer of recordable/playable data.
 *
 * This is a datatype-agnostic base class for all buffers (there are no
 * methods to actually access the data).  This provides a way for code that
 * doesn't care about the data type to still deal with buffers (which is
 * why the base class can't be a template).
 *
 * To actually read/write buffer contents, use the appropriate derived class.
 */
class LIBARDOUR_API Buffer : public boost::noncopyable
{
public:
	virtual ~Buffer() {}

	/** Factory function */
	static Buffer* create(DataType type, size_t capacity);

	/** Maximum capacity of buffer. */
	size_t capacity() const { return _capacity; }

	/** Type of this buffer.
	 * Based on this you can static cast a Buffer* to the desired type. */
	DataType type() const { return _type; }

	bool silent() const { return _silent; }

	/** Reallocate the buffer used internally to handle at least @a size_t units of data.
	 *
	 * The buffer is not silent after this operation. the @a capacity argument
	 * passed to the constructor must have been non-zero.
	 */
	virtual void resize (size_t) = 0;

	/** Clear (eg zero, or empty) buffer */
	virtual void silence (samplecnt_t len, samplecnt_t offset = 0) = 0;

	/** Clear the entire buffer */
	virtual void clear() { silence(_capacity, 0); }

	virtual void read_from (const Buffer& src, samplecnt_t len, sampleoffset_t dst_offset = 0, sampleoffset_t src_offset = 0) = 0;
	virtual void merge_from (const Buffer& src, samplecnt_t len, sampleoffset_t dst_offset = 0, sampleoffset_t src_offset = 0) = 0;

  protected:
	Buffer(DataType type)
		: _type(type), _capacity(0), _silent (true)
	{}

	DataType  _type;
	pframes_t _capacity;
	bool      _silent;
};


} // namespace ARDOUR

#endif // __ardour_buffer_h__
