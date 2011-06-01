/*
    Copyright (C) 2009 Paul Davis

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

#include "midi_cut_buffer.h"

using namespace ARDOUR;

MidiCutBuffer::MidiCutBuffer (Session* s)
	: AutomatableSequence<MidiModel::TimeType> (*s)
	, _origin (0)
{

}

MidiCutBuffer::~MidiCutBuffer ()
{
}

void
MidiCutBuffer::set_origin (MidiCutBuffer::TimeType when)
{
	_origin = when;
}

void
MidiCutBuffer::set (const Evoral::Sequence<MidiCutBuffer::TimeType>::Notes& notes)
{
	set_notes (notes);
}
