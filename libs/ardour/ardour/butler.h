/*
    Copyright (C) 2000-2009 Paul Davis

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

#ifndef __ardour_butler_h__
#define __ardour_butler_h__

#include <glibmm/thread.h>
#include "ardour/types.h"

namespace ARDOUR {

class Butler {
public:
	Butler();
	~Butler();

	pthread_t    thread;
	Glib::Mutex  request_lock;
	Glib::Cond   paused;
	bool         should_run;
	mutable gint should_do_transport_work;
	int          request_pipe[2];
};

} // namespace ARDOUR

#endif // __ardour_butler_h__
