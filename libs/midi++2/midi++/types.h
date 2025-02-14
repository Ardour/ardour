/*
 * Copyright (C) 2000-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2007 David Robillard <d@drobilla.net>
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

#pragma once

#include <inttypes.h>

#include "midi++/libmidi_visibility.h"

namespace MIDI {

	typedef char           channel_t;
	typedef float          controller_value_t;
	typedef unsigned char  byte;
	typedef unsigned short pitchbend_t;
	typedef uint32_t       timestamp_t;

	/** XXX: dupes from libardour */
	typedef int64_t  samplecnt_t;
	typedef uint32_t pframes_t;

	enum eventType {
	    none = 0x0,
	    raw = 0xF4, /* undefined in MIDI spec */
	    any = 0xF5, /* undefined in MIDI spec */
	    off = 0x80,
	    on = 0x90,
	    controller = 0xB0,
	    program = 0xC0,
	    chanpress = 0xD0,
	    polypress = 0xA0,
	    pitchbend = 0xE0,
	    sysex = 0xF0,
	    mtc_quarter = 0xF1,
	    position = 0xF2,
	    song = 0xF3,
	    tune = 0xF6,
	    eox = 0xF7,
	    timing = 0xF8,
	    tick = 0xF9,
	    start = 0xFA,
	    contineu = 0xFB, /* note spelling */
	    stop = 0xFC,
	    active = 0xFE,
	    reset = 0xFF
    };

    LIBMIDIPP_API extern const char *controller_names[];
	byte decode_controller_name (const char *name);

    struct LIBMIDIPP_API EventTwoBytes {
	union {
	    byte note_number;
	    byte controller_number;
	};
	union {
	    byte velocity;
	    byte value;
	};
    };

    enum LIBMIDIPP_API MTC_FPS {
	    MTC_24_FPS = 0,
	    MTC_25_FPS = 1,
	    MTC_30_FPS_DROP = 2,
	    MTC_30_FPS = 3
    };

    enum LIBMIDIPP_API MTC_Status {
	    MTC_Stopped = 0,
	    MTC_Forward,
	    MTC_Backward
    };

} // namespace MIDI





