/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include "midi_byte_array.h"

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstdarg>
#include <iomanip>
#include <stdexcept>

MidiByteArray::MidiByteArray (std::initializer_list<int> bytes)
{
	for (size_t i = 0; i < bytes.size (); ++i) {
		const int byte = bytes.begin ()[i];
		if (byte >= 0 && byte <= 255) {
			push_back (static_cast<MIDI::byte> (byte));
		} else {
			throw std::out_of_range ("MIDI byte");
		}
	}
}

MidiByteArray MidiByteArray::copy (size_t count, const MIDI::byte* arr)
{
	MidiByteArray result;
	for (size_t i = 0; i < count; ++i) {
		result.push_back (arr[i]);
	}
	return result;
}

MidiByteArray & operator <<  (MidiByteArray & mba, const MIDI::byte & b)
{
	mba.push_back (b);
	return mba;
}

MidiByteArray & operator <<  (MidiByteArray & mba, const MidiByteArray & barr)
{
	std::back_insert_iterator<MidiByteArray> bit (mba);
	copy (barr.begin(), barr.end(), bit);
	return mba;
}

std::ostream & operator <<  (std::ostream & os, const MidiByteArray & mba)
{
	os << "[";
	char fill = os.fill('0');
	for (MidiByteArray::const_iterator it = mba.begin(); it != mba.end(); ++it) {
		if  (it != mba.begin()) os << " ";
		os << std::hex << std::setw(2) << (int)*it;
	}
	os.fill (fill);
	os << std::dec;
	os << "]";
	return os;
}

MidiByteArray & operator <<  (MidiByteArray & mba, const std::string & st)
{
	/* note that this assumes that "st" is ASCII encoded
	 */

	mba.insert (mba.end(), st.begin(), st.end());
	return mba;
}

bool
MidiByteArray::compare_n (const MidiByteArray& other, MidiByteArray::size_type n) const
{
	MidiByteArray::const_iterator us = begin();
	MidiByteArray::const_iterator them = other.begin();

	while (n && us != end() && them != other.end()) {
		if ((*us) != (*them)) {
			return false;
		}
		--n;
		++us;
		++them;
	}

	return true;
}
