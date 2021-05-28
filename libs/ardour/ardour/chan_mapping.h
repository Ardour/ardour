/*
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_chan_mapping_h__
#define __ardour_chan_mapping_h__

#include <map>
#include <cassert>
#include <ostream>
#include <utility>

#include "pbd/stack_allocator.h"
#include "pbd/xml++.h"

#include "ardour/data_type.h"
#include "ardour/chan_count.h"

namespace ARDOUR {

/** A mapping from one set of channels to another.
 * The general form is  1 source (from), many sinks (to).
 * numeric IDs are used to identify sources and sinks.
 *
 * for plugins this is used to map "plugin-pin" to "audio-buffer"
 */
class LIBARDOUR_API ChanMapping {
public:
	ChanMapping() {}
	ChanMapping(ARDOUR::ChanCount identity);
	ChanMapping(const ChanMapping&);
	ChanMapping(const XMLNode& node);

	ChanMapping operator=(const ChanMapping&);

	uint32_t get(DataType t, uint32_t from, bool* valid) const;

	/** reverse lookup
	 * @param type data type
	 * @param to pin
	 * @param valid pointer to a boolean. If not NULL it is set to true if the mapping is found, and false otherwise.
	 * @returns first "from" that matches given "to"
	 */
	uint32_t get_src(DataType type, uint32_t to, bool* valid) const;

	/** get buffer mapping for given data type and pin
	 * @param type data type
	 * @param from numeric source id
	 * @returns mapped buffer number (or ChanMapping::Invalid)
	 */
	uint32_t get (DataType type, uint32_t from) const { return get (type, from, NULL); }

	/** set buffer mapping for given data type
	 * @param type data type
	 * @param from numeric source id
	 * @param to buffer
	 */
	void     set (DataType type, uint32_t from, uint32_t to);

	void     offset_from (DataType t, int32_t delta);
	void     offset_to (DataType t, int32_t delta);

	/** remove mapping
	 * @param type data type
	 * @param from numeric source to remove from mapping
	 */
	void     unset(DataType type, uint32_t from);

	/** Test mapping matrix for identity
	 * @param offset per data-type offset to take into account
	 * @returns true if the mapping is a channel identity map
	 */
	bool     is_identity (ARDOUR::ChanCount offset = ARDOUR::ChanCount()) const;

	/** Test if this mapping is monotonic (useful to see if inplace processing is feasible)
	 * @returns true if the map is a strict monotonic set
	 */
	bool     is_monotonic () const;

	uint32_t n_total () const;

	ChanCount count () const;

	XMLNode* state(const std::string& name) const;

	/** Test if this mapping is a subset
	 * @param superset to test against
	 * @returns true if all mapping are also present in the superset
	 */
	bool     is_subset (const ChanMapping& superset) const;

#if defined(_MSC_VER) /* && (_MSC_VER < 1900)
	                   * Regarding the note (below) it was initially
	                   * thought that this got fixed in VS2015 - but
	                   * in fact it's still faulty (JE - Feb 2021) */
	/* Use the older (heap based) mapping for early versions of MSVC.
	 * In fact it might be safer to use this for all MSVC builds - as
	 * our StackAllocator class depends on 'boost::aligned_storage'
	 * which is known to be troublesome with Visual C++ :-
	 * https://www.boost.org/doc/libs/1_65_0/libs/type_traits/doc/html/boost_typetraits/reference/aligned_storage.html
	 */
	typedef std::map<uint32_t, uint32_t>    TypeMapping;
	typedef std::map<DataType, TypeMapping> Mappings;
#else
	typedef std::map<uint32_t, uint32_t, std::less<uint32_t>, PBD::StackAllocator<std::pair<const uint32_t, uint32_t>, 16> > TypeMapping;
	typedef std::map<DataType, TypeMapping, std::less<DataType>, PBD::StackAllocator<std::pair<const DataType, TypeMapping>, 2> > Mappings;
#endif

	Mappings        mappings()       { return _mappings; }
	const Mappings& mappings() const { return _mappings; }

	bool operator==(const ChanMapping& other) const {
		return (_mappings == other._mappings);
	}

	bool operator!=(const ChanMapping& other) const {
		return ! (*this == other);
	}

private:
	Mappings _mappings;
};

} // namespace ARDOUR

std::ostream& operator<<(std::ostream& o, const ARDOUR::ChanMapping& m);

#endif // __ardour_chan_mapping_h__

