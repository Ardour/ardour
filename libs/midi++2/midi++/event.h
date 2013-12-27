/*
    Copyright (C) 2007 Paul Davis 

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

#ifndef __libmidipp_midi_event_h__
#define __libmidipp_midi_event_h__

#include <stdint.h>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <assert.h>

#include "midi++/libmidi_visibility.h"
#include "midi++/types.h"
#include "midi++/events.h"
#include "pbd/xml++.h"

/** If this is not defined, all methods of MidiEvent are RT safe
 * but MidiEvent will never deep copy and (depending on the scenario)
 * may not be usable in STL containers, signals, etc. 
 */
#define EVORAL_EVENT_ALLOC 1

/** Support serialisation of MIDI events to/from XML */
#define EVORAL_MIDI_XML 1

#include "evoral/Event.hpp"
#include "evoral/MIDIEvent.hpp"

#endif /* __libmidipp_midi_event_h__ */
