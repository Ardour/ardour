/*
    Copyright (C) 1999 Paul Davis 

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

    $Id$
*/

#ifndef __ardour_ardour_h__
#define __ardour_ardour_h__

#include <limits.h>
#include <string>
#include <signal.h>

#include <pbd/error.h>
#include <pbd/lockmonitor.h>
#include <pbd/failed_constructor.h>

#include <ardour/configuration.h>
#include <ardour/types.h>

using namespace PBD;

namespace MIDI {
	class MachineControl;
	class Port;
}

namespace ARDOUR {

	class AudioEngine;

	static const jack_nframes_t max_frames = JACK_MAX_FRAMES;

	int init (AudioEngine&, bool with_vst, bool try_optimization, void (*sighandler)(int,siginfo_t*,void*) = 0);
	int cleanup ();
	std::string find_config_file (std::string name);
	std::string find_data_file (std::string name);

	const layer_t max_layer = UCHAR_MAX;

	id_t new_id();

	Change new_change ();

	extern Change StartChanged;
	extern Change LengthChanged;
	extern Change PositionChanged;
	extern Change NameChanged;
	extern Change BoundsChanged;

	struct LocaleGuard {
	    LocaleGuard (const char*);
	    ~LocaleGuard ();
	    const char* old;
	};

};

/* how do we make these be within the Ardour namespace? */

extern MIDI::Port* default_mmc_port;
extern MIDI::Port* default_mtc_port;
extern MIDI::Port* default_midi_port;

#endif /* __ardour_ardour_h__ */

