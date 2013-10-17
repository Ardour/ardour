/*
    Copyright (C) 2006 Paul Davis
    Author: David Robillard

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_chan_count_h__
#define __ardour_chan_count_h__

#include <cassert>
#include <ostream>

#include "pbd/xml++.h"
#include "ardour/data_type.h"

namespace ARDOUR {


/** A count of channels, possibly with many types.
 *
 * Operators are defined so this may safely be used as if it were a simple
 * (single-typed) integer count of channels.
 */
class LIBARDOUR_API ChanCount {
public:
	ChanCount(const XMLNode& node);
	ChanCount() { reset(); }

	// Convenience constructor for making single-typed streams (stereo, mono, etc)
	ChanCount(DataType type, uint32_t channels) {
		reset();
		set(type, channels);
	}

	void reset() {
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			_counts[*t] = 0;
		}
	}

	void     set(DataType t, uint32_t count) { assert(t != DataType::NIL); _counts[t] = count; }
	uint32_t get(DataType t) const { assert(t != DataType::NIL); return _counts[t]; }

	inline uint32_t n (DataType t) const { return _counts[t]; }

	inline uint32_t n_audio() const { return _counts[DataType::AUDIO]; }
	inline void set_audio(uint32_t a) { _counts[DataType::AUDIO] = a; }

	inline uint32_t n_midi()  const { return _counts[DataType::MIDI]; }
	inline void set_midi(uint32_t m) { _counts[DataType::MIDI] = m; }

	uint32_t n_total() const {
		uint32_t ret = 0;
		for (uint32_t i=0; i < DataType::num_types; ++i)
			ret += _counts[i];

		return ret;
	}

	bool operator==(const ChanCount& other) const {
		for (uint32_t i=0; i < DataType::num_types; ++i)
			if (_counts[i] != other._counts[i])
				return false;

		return true;
	}

	bool operator!=(const ChanCount& other) const {
		return ! (*this == other);
	}

	bool operator<(const ChanCount& other) const {
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			if (_counts[*t] > other._counts[*t]) {
				return false;
			}
		}
		return (*this != other);
	}

	bool operator<=(const ChanCount& other) const {
		return ( (*this < other) || (*this == other) );
	}

	bool operator>(const ChanCount& other) const {
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			if (_counts[*t] < other._counts[*t]) {
				return false;
			}
		}
		return (*this != other);
	}

	bool operator>=(const ChanCount& other) const {
		return ( (*this > other) || (*this == other) );
	}

	ChanCount operator+(const ChanCount& other) const {
		ChanCount ret;
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			ret.set(*t, get(*t) + other.get(*t));
		}
		return ret;
	}

	ChanCount& operator+=(const ChanCount& other) {
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			_counts[*t] += other._counts[*t];
		}
		return *this;
	}

	static ChanCount min(const ChanCount& a, const ChanCount& b) {
		ChanCount ret;
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			ret.set(*t, std::min(a.get(*t), b.get(*t)));
		}
		return ret;
	}

	static ChanCount max(const ChanCount& a, const ChanCount& b) {
		ChanCount ret;
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			ret.set(*t, std::max(a.get(*t), b.get(*t)));
		}
		return ret;
	}

	XMLNode* state(const std::string& name) const;

	static const ChanCount INFINITE;
	static const ChanCount ZERO;

private:
	uint32_t _counts[DataType::num_types];
};

} // namespace ARDOUR

std::ostream& operator<<(std::ostream& o, const ARDOUR::ChanCount& c);

#endif // __ardour_chan_count_h__

