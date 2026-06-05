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
#ifndef midi_byte_array_h
#define midi_byte_array_h

#include <iostream>
#include <vector>

#include "control_protocol/visibility.h"

//#include <midi++/types.h>
namespace MIDI {
	typedef unsigned char byte;
}

/**
	To make building arrays of bytes easier. Thusly:

	MidiByteArray mba;
	mba << 0xf0 << 0x00 << 0xf7;

	MidiByteArray buf;
	buf << mba;

	MidiByteArray direct({0xf0, 0x00, 0xf7});

	cout << mba << endl;
	cout << buf << endl;
	cout << direct << endl;

	will all result in "f0 00 f7" being output to stdout
*/
class ARDOURSURFACE_API MidiByteArray : public std::vector<MIDI::byte>
{
public:
	MidiByteArray () : std::vector<MIDI::byte>() {};

	MidiByteArray (const std::vector<int>& array);

	bool compare_n (const MidiByteArray& other, MidiByteArray::size_type len) const;

	/// copy the given number of bytes from the given array
	static MidiByteArray copy (size_t count, const MIDI::byte* arr);
};

/// append the given byte to the end of the array
MidiByteArray & operator << ( MidiByteArray & mba, const MIDI::byte & b );

/// append the given string to the end of the array
MidiByteArray & operator << ( MidiByteArray & mba, const std::string & );

/// append the given array to the end of this array
MidiByteArray & operator << ( MidiByteArray & mba, const MidiByteArray & barr );

/// output the bytes as hex to the given stream
std::ostream & operator << ( std::ostream & os, const MidiByteArray & mba );

#endif
