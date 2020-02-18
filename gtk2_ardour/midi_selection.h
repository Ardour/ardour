/*
 * Copyright (C) 2009-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_gtk_midi_selection_h__
#define __ardour_gtk_midi_selection_h__

#include "region_selection.h"

class MidiRegionView;
class MidiCutBuffer;
class RegionView;

class MidiRegionSelection : public RegionSelection
{
public:
	MidiRegionSelection ();
	MidiRegionSelection (const MidiRegionSelection&);

	MidiRegionSelection& operator= (const MidiRegionSelection&);
};

struct MidiNoteSelection : std::list<MidiCutBuffer*> {
public:
	const_iterator
	get_nth(size_t nth) const {
		size_t count = 0;
		for (const_iterator m = begin(); m != end(); ++m) {
			if (count++ == nth) {
				return m;
			}
		}
		return end();
	}
};

#endif /* __ardour_gtk_midi_selection_h__ */
