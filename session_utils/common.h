/*
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#ifndef _session_utils_common_h_
#define _session_utils_common_h_

#include "ardour/ardour.h"
#include "ardour/session.h"

namespace SessionUtils {

	/** initialize libardour */
	void init (bool print_log = true);

	/** clean up, stop Processing Engine
	 * @param s Session to close (may me NULL)
	 */
	void cleanup ();

	/** @param dir Session directory.
	 *  @param state Session state file, without .ardour suffix.
	 *  @returns an ardour session object (free with \ref unload_session) or NULL
	 */
	ARDOUR::Session* load_session (std::string dir, std::string state, bool exit_at_failure = true);

	/** @param dir Session directory.
	 *  @param state Session state file, without .ardour suffix.
	 *  @returns an ardour session object (free with \ref unload_session) or NULL on error
	 */
	ARDOUR::Session* create_session (std::string dir, std::string state, float sample_rate);

	/** close session and stop engine
	 * @param s Session to close (may me NULL)
	 */
	void unload_session (ARDOUR::Session *s);

};

#endif /* _session_utils_misc_h_ */
