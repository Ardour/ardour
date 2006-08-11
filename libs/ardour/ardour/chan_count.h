/*
    Copyright (C) 2006 Paul Davis 

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

    $Id: insert.cc 712 2006-07-28 01:08:57Z drobilla $
*/

#ifndef __ardour_chan_count_h__
#define __ardour_chan_count_h__

#include <ardour/data_type.h>

namespace ARDOUR {


class ChanCount {
public:
	ChanCount() { reset(); }
	
	// Convenience constructor for making single-typed streams (stereo, mono, etc)
	ChanCount(DataType type, size_t channels)
	{
		reset();
		set(type, channels);
	}

	void reset()
	{
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			_counts[(*t).to_index()] = 0;
		}
	}

	void   set(DataType type, size_t count) { _counts[type.to_index()] = count; }
	size_t get(DataType type) const { return _counts[type.to_index()]; }
	
	size_t get_total() const
	{
		size_t ret = 0;
		for (size_t i=0; i < DataType::num_types; ++i)
			ret += _counts[i];

		return ret;
	}

	bool operator==(const ChanCount& other) const
	{
		for (size_t i=0; i < DataType::num_types; ++i)
			if (_counts[i] != other._counts[i])
				return false;

		return true;
	}
	
	bool operator!=(const ChanCount& other) const
	{
		return ! (*this == other);
	}

	bool operator<(const ChanCount& other) const
	{
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			if (_counts[(*t).to_index()] >= other._counts[(*t).to_index()]) {
				return false;
			}
		}
		return true;
	}
	
	bool operator<=(const ChanCount& other) const
	{
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			if (_counts[(*t).to_index()] > other._counts[(*t).to_index()]) {
				return false;
			}
		}
		return true;
	}
	
	bool operator>(const ChanCount& other) const
	{
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			if (_counts[(*t).to_index()] <= other._counts[(*t).to_index()]) {
				return false;
			}
		}
		return true;
	}
	
	bool operator>=(const ChanCount& other) const
	{
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			if (_counts[(*t).to_index()] < other._counts[(*t).to_index()]) {
				return false;
			}
		}
		return true;
	}

	static const ChanCount INFINITE;
	static const ChanCount ZERO;

private:
	
	size_t _counts[DataType::num_types];
};


} // namespace ARDOUR

#endif // __ardour_chan_count_h__

