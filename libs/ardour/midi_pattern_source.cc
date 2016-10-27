/*
    Copyright (C) 2016 Paul Davis

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

#include "ardour/midi_pattern_source.h"
#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

MidiPatternSource::MidiPatternSource (Session& s, std::string const & name)
	: Source (s, DataType::MIDI, name, Source::Pattern)
	, MidiSource (s, name)
	, PatternSource (s, DataType::MIDI, name)
{
}

MidiPatternSource::MidiPatternSource (Session& s, const XMLNode& node)
	: Source (s, node)
	, MidiSource (s, node)
	, PatternSource (s, node)
{
	// set_state (node Stateful::loading_state_version);
}

MidiPatternSource::~MidiPatternSource ()
{
}

framecnt_t
MidiPatternSource::length (framepos_t) const
{
	return max_framepos - 1;
}

bool
MidiPatternSource::empty () const
{
	return false;
}

void
MidiPatternSource::update_length (framecnt_t)
{
	return;
}

XMLNode&
MidiPatternSource::get_state ()
{
	XMLNode* node = new XMLNode ("midi-pattern-source");
	return *node;
}

int
MidiPatternSource::set_state (XMLNode const& node, int)
{
	return 0;
}

framecnt_t
MidiPatternSource::write_unlocked (Lock const& lock,
                                   MidiRingBuffer<framepos_t>& source,
                                   framepos_t position,
                                   framecnt_t cnt)
{
	return 0;
}

framecnt_t
MidiPatternSource::read_unlocked (const Lock&                    lock,
	                          Evoral::EventSink<framepos_t>& dst,
	                          framepos_t                     position,
	                          framepos_t                     start,
	                          framecnt_t                     cnt,
	                          Evoral::Range<framepos_t>*     loop_range,
	                          MidiStateTracker*              tracker,
	                          MidiChannelFilter*             filter) const
{
	return 0;
}
