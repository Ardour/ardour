/*
	Copyright (C) 2006,2007 John Anderson

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
#include "midi_byte_array.h"

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstdarg>
#include <iomanip>
#include <stdexcept>

using namespace std;

MidiByteArray::MidiByteArray (size_t size, MIDI::byte array[])
  : std::vector<MIDI::byte>()
{
	for  (size_t i = 0; i < size; ++i)
	{
		push_back (array[i]);
	}			
}

MidiByteArray::MidiByteArray (size_t count, MIDI::byte first, ...)
  : vector<MIDI::byte>()
{
	push_back (first);
	va_list var_args;
	va_start (var_args, first);
	for  (size_t i = 1; i < count; ++i)
	{
		MIDI::byte b = va_arg (var_args, int);
		push_back (b);
	}
	va_end (var_args);
}


void MidiByteArray::copy (size_t count, MIDI::byte * arr)
{
	for (size_t i = 0; i < count; ++i) {
		push_back (arr[i]);
	}
}

MidiByteArray & operator <<  (MidiByteArray & mba, const MIDI::byte & b)
{
	mba.push_back (b);
	return mba;
}

MidiByteArray & operator <<  (MidiByteArray & mba, const MidiByteArray & barr)
{
	back_insert_iterator<MidiByteArray> bit (mba);
	copy (barr.begin(), barr.end(), bit);
	return mba;
}

ostream & operator <<  (ostream & os, const MidiByteArray & mba)
{
	os << "[";
	char fill = os.fill('0');
	for (MidiByteArray::const_iterator it = mba.begin(); it != mba.end(); ++it) {
		if  (it != mba.begin()) os << " ";
		os << hex << setw(2) << (int)*it;
	}
	os.fill (fill);
	os << dec;
	os << "]";
	return os;
}

MidiByteArray & operator <<  (MidiByteArray & mba, const std::string & st)
{
	for  (string::const_iterator it = st.begin(); it != st.end(); ++it) {
		mba << *it;
	}
	return mba;
}
