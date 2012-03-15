/*
    Copyright (C) 2002 Paul Davis

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

#ifndef __libardour_interthread_h__
#define __libardour_interthread_h__

#include <pthread.h>

#include "ardour/types.h"
#include "ardour/process_thread.h"

namespace ARDOUR {

	struct InterThreadInfo {
		InterThreadInfo () : done (false), cancel (false), progress (0), thread (0) {}

		volatile bool  done;
		volatile bool  cancel;
		volatile float progress;
		pthread_t      thread;
	        ProcessThread  process_thread;
	};

} // namespace

#endif /* __libardour_interthread_h__ */
