/*
 * Copyright (C) 2006-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2015 John Emmas <john@creativepost.co.uk>
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

	/** Convenience constructor for making single-typed streams (mono, stereo, midi, etc)
	 * @param type data type
	 * @param count number of channels
	 */
	ChanCount(DataType type, uint32_t count) {
		reset();
		set(type, count);
	}

	/** zero count of all data types */
	void reset() {
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			_counts[*t] = 0;
		}
	}

	/** set channel count for given type
	 * @param t data type
	 * @param count number of channels
	 */
	void     set(DataType t, uint32_t count) { assert(t != DataType::NIL); _counts[t] = count; }

	/** query channel count for given type
	 * @param t data type
	 * @returns channel count for given type
	 */
	uint32_t get(DataType t) const { assert(t != DataType::NIL); return _counts[t]; }

	inline uint32_t n (DataType t) const { return _counts[t]; }

	/** query number of audio channels
	 * @returns number of audio channels
	 */
	inline uint32_t n_audio() const { return _counts[DataType::AUDIO]; }

	/** set number of audio channels
	 * @param a number of audio channels
	 */
	inline void set_audio(uint32_t a) { _counts[DataType::AUDIO] = a; }

	/** query number of midi channels
	 * @returns number of midi channels
	 */
	inline uint32_t n_midi()  const { return _counts[DataType::MIDI]; }

	/** set number of audio channels
	 * @param m number of midi channels
	 */
	inline void set_midi(uint32_t m) { _counts[DataType::MIDI] = m; }

	/** query total channel count of all data types
	 * @returns total channel count (audio + midi)
	 */
	uint32_t n_total() const {
		uint32_t ret = 0;
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t)
			ret += _counts[*t];

		return ret;
	}

	bool operator==(const ChanCount& other) const {
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t)
			if (_counts[*t] != other._counts[*t])
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

	/** underflow safe subtraction */
	ChanCount operator-(const ChanCount& other) const {
		ChanCount ret;
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			if (get(*t) < other.get(*t)) {
				ret.set(*t, 0);
			} else {
				ret.set(*t, get(*t) - other.get(*t));
			}
		}
		return ret;
	}

	ChanCount operator*(const unsigned int factor) const {
		ChanCount ret;
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			ret.set(*t, get(*t) * factor );
		}
		return ret;
	}

	/** underflow safe subtraction */
	ChanCount& operator-=(const ChanCount& other) {
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			if (_counts[*t] < other._counts[*t]) {
				_counts[*t] = 0;
			} else {
				_counts[*t] -= other._counts[*t];
			}
		}
		return *this;
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

	static const ChanCount ZERO;

private:
	uint32_t _counts[DataType::num_types];
};

} // namespace ARDOUR

LIBARDOUR_API std::ostream& operator<<(std::ostream& o, const ARDOUR::ChanCount& c);

#endif // __ardour_chan_count_h__

