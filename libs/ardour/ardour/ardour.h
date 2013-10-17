/*
    Copyright (C) 1999-2009 Paul Davis

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

#ifndef __ardour_ardour_h__
#define __ardour_ardour_h__

#include <map>
#include <string>
#include <vector>

#include <limits.h>
#include <signal.h>

#include "pbd/signals.h"

#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/locale_guard.h"
#include "pbd/stateful.h"

#include "ardour/types.h"
#include "ardour/libardour_visibility.h"

#include <jack/jack.h>

namespace MIDI {
	class MachineControl;
	class Port;
}

namespace ARDOUR {

	class AudioEngine;

	extern PBD::Signal1<void,std::string> BootMessage;
	extern PBD::Signal0<void> GUIIdle;

	/**
	 * @param with_vst true to enable VST Support
	 * @param try_optimization true to enable hardware optimized routines
	 * for mixing, finding peak values etc.
	 * @param localedir Directory to look for localisation files
	 *
	 * @return true if Ardour library was successfully initialized
	 */
	bool init (bool with_vst, bool try_optimization, const char* localedir);
	void init_post_engine ();
	void cleanup ();
	bool no_auto_connect ();
	void make_property_quarks ();

	extern PBD::PropertyChange bounds_change;

	extern const char* const ardour_config_info;

	void find_bindings_files (std::map<std::string,std::string>&);

	/* these only impact bundled installations */
	std::string translation_enable_path ();
	bool translations_are_enabled ();
	bool set_translations_enabled (bool);

	microseconds_t get_microseconds ();

	void setup_fpu ();
	std::vector<SyncSource> get_available_sync_options();
}

#endif /* __ardour_ardour_h__ */

