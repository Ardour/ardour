/*
 * Copyright (C) 2022 Paul Davis <paul@linuxaudiosystems.com>
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

#include <glib/gstdio.h>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/inflater.h"
#include "pbd/pthread_utils.h"

using namespace PBD;

Inflater::Inflater (std::string const & ap, std::string const & dd)
	: FileArchive (ap)
	, thread (0)
	, _status (-1) /* means "unknown" */
	, archive_path (ap)
	, destdir (dd)
{
}

Inflater::~Inflater ()
{
	if (thread) {
		thread->join ();
	}
}

int
Inflater::start ()
{
	return 0 != (thread = PBD::Thread::create (boost::bind (&Inflater::threaded_inflate, this)));
}

void
Inflater::threaded_inflate ()
{
	require_progress ();

	try {
		std::string pwd (Glib::get_current_dir ());

		if (inflate (destdir)) {
			/* cleanup ? */
			_status = 1; /* failure */
		}

		_status = 0; /* success */

	} catch (...) {
		_status = 1; /* failure */
	}


	/* final progress signal, values do not matter much because status is
	 * set to be >= 0
	 */

	progress (1, 1);
}

