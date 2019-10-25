/*
 * Copyright (C) 2008-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2008 Hans Baier <hansfbaier@googlemail.com>
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

#include "evoral/Event.h"

#endif /* __libmidipp_midi_event_h__ */
