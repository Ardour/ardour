/*
    Copyright (C) 2013 Waves Audio Ltd.

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
#include "waves_midi_buffer.h"
#include "waves_midi_event.h"

using namespace ARDOUR;

WavesMidiBuffer::WavesMidiBuffer (std::string name)
    : std::vector<WavesMidiEvent*> ()
    , _name (name)
{
}

WavesMidiBuffer::~WavesMidiBuffer ()
{
    clear ();
}

void WavesMidiBuffer::clear ()
{
    for (WavesMidiBufferIterator it = begin (); it !=  end (); ++it)
        delete *it;

    std::vector<WavesMidiEvent*>::clear ();
}

WavesMidiBuffer& WavesMidiBuffer::operator += (const WavesMidiBuffer& source)
{
    for (WavesMidiBufferConstIterator it = source.begin (); it !=  source.end (); ++it) {
        push_back (new WavesMidiEvent (**it));
    }
    return *this;
}
