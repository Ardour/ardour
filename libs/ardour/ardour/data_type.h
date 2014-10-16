/*
    Copyright (C) 2006 Paul Davis
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

#ifndef __ardour_data_type_h__
#define __ardour_data_type_h__

#include <string>
#include <stdint.h>
#include <glib.h>

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

/** A type of Data Ardour is capable of processing.
 *
 * The majority of this class is dedicated to conversion to and from various
 * other type representations, simple comparison between then, etc.  This code
 * is deliberately 'ugly' so other code doesn't have to be.
 */
class LIBARDOUR_API DataType
{
public:
	/** Numeric symbol for this DataType.
	 *
	 * Castable to int for use as an array index (e.g. by ChanCount).
	 * Note this means NIL is (ntypes-1) and guaranteed to change when
	 * types are added, so this number is NOT suitable for serialization,
	 * network, or binary anything.
	 *
	 * WARNING: The number of non-NIL entries here must match num_types.
	 */
	enum Symbol {
		AUDIO = 0,
		MIDI = 1,
		NIL = 2,
	};

	/** Number of types (not including NIL).
	 * WARNING: make sure this matches Symbol!
	 */
	static const uint32_t num_types = 2;

	DataType(const Symbol& symbol)
	: _symbol(symbol)
	{}

	/** Construct from a string (Used for loading from XML and Ports)
	 * The string can be as in an XML file (eg "audio" or "midi"), or a
	 */
	DataType(const std::string& str)
	: _symbol(NIL) {
		if (!g_ascii_strncasecmp(str.c_str(), "audio", str.length())) {
			_symbol = AUDIO;
		} else if (!g_ascii_strncasecmp(str.c_str(), "midi", str.length())) {
			_symbol = MIDI;
		}
	}

	/** Inverse of the from-string constructor */
	const char* to_string() const {
		switch (_symbol) {
		case AUDIO: return "audio";
		case MIDI:  return "midi";
		default:    return "unknown"; // reeeally shouldn't ever happen
		}
	}
    
	const char* to_i18n_string () const;

	inline operator uint32_t() const { return (uint32_t)_symbol; }

	/** DataType iterator, for writing generic loops that iterate over all
	 * available types.
	 */
	class iterator {
	public:

		iterator(uint32_t index) : _index(index) {}

		DataType  operator*()  { return DataType((Symbol)_index); }
		iterator& operator++() { ++_index; return *this; } // yes, prefix only
		bool operator==(const iterator& other) { return (_index == other._index); }
		bool operator!=(const iterator& other) { return (_index != other._index); }

	private:
		friend class DataType;

		uint32_t _index;
	};

	static iterator begin() { return iterator(0); }
	static iterator end()   { return iterator(num_types); }

	bool operator==(const Symbol symbol) { return (_symbol == symbol); }
	bool operator!=(const Symbol symbol) { return (_symbol != symbol); }

	bool operator==(const DataType other) { return (_symbol == other._symbol); }
	bool operator!=(const DataType other) { return (_symbol != other._symbol); }

private:
	Symbol _symbol; // could be const if not for the string constructor
};


} // namespace ARDOUR

#endif // __ardour_data_type_h__

