/*
 * Copyright (C) 2006-2008 Doug McLain <doug@nostar.net>
 * Copyright (C) 2006-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2006-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2016 Robin Gareus <robin@gareus.org>
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
#include "pbd/stateful.h"

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/libardour_visibility.h"

namespace MIDI {
	class MachineControl;
	class Port;
}

namespace ARDOUR {

	class AudioEngine;
	class Session;

	extern LIBARDOUR_API PBD::Signal1<void,std::string> BootMessage;
	extern LIBARDOUR_API PBD::Signal3<void,std::string,std::string,bool> PluginScanMessage;
	extern LIBARDOUR_API PBD::Signal1<void,int> PluginScanTimeout;
	extern LIBARDOUR_API PBD::Signal0<void> GUIIdle;
	extern LIBARDOUR_API PBD::Signal3<bool,std::string,std::string,int> CopyConfigurationFiles;
	extern LIBARDOUR_API std::map<std::string, bool> reserved_io_names;
	extern LIBARDOUR_API float ui_scale_factor;

	/**
	 * @param try_optimization true to enable hardware optimized routines
	 * for mixing, finding peak values etc.
	 * @param localedir Directory to look for localisation files
	 * @param with_gui set to true if running from a GUI (expected to take
	 *                care of certain initialization itself)
	 *
	 * @return true if Ardour library was successfully initialized
	 */
	LIBARDOUR_API bool init (bool try_optimization, const char* localedir, bool with_gui = false);
	LIBARDOUR_API void init_post_engine (uint32_t);
	LIBARDOUR_API void cleanup ();
	LIBARDOUR_API bool no_auto_connect ();
	LIBARDOUR_API void make_property_quarks ();

	extern LIBARDOUR_API PBD::PropertyChange bounds_change;

	extern LIBARDOUR_API const char* const ardour_config_info;

	/* these only impact bundled installations */
	LIBARDOUR_API std::string translation_enable_path ();
	LIBARDOUR_API bool translations_are_enabled ();
	LIBARDOUR_API bool set_translations_enabled (bool);

	LIBARDOUR_API void setup_fpu ();
	LIBARDOUR_API std::vector<SyncSource> get_available_sync_options();

	LIBARDOUR_API void set_global_ui_scale_factor (float s);

	/* the @param ui_handler will be called if there are old configuration
	 * files to be copied. It should (probably) ask the user about the
	 * action, and return true or false depending on whether or not the
	 * copy should take place.
	 */
	LIBARDOUR_API void check_for_old_configuration_files ();
	LIBARDOUR_API int handle_old_configuration_files (boost::function<bool (std::string const&, std::string const&, int)> ui_handler);

	LIBARDOUR_API void reset_performance_meters (Session*);
}

#endif /* __ardour_ardour_h__ */

