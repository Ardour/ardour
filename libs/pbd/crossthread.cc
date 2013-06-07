/*
    Copyright (C) 2009 Paul Davis 

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

#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include "pbd/error.h"
#include "pbd/crossthread.h"

using namespace std;
using namespace PBD;
using namespace Glib;

CrossThreadChannel::CrossThreadChannel (bool non_blocking)
{
	_ios = 0;
	fds[0] = -1;
	fds[1] = -1;

	if (pipe (fds)) {
		error << "cannot create x-thread pipe for read (%2)" << ::strerror (errno) << endmsg;
		return;
	}

	if (non_blocking) {
		if (fcntl (fds[0], F_SETFL, O_NONBLOCK)) {
			error << "cannot set non-blocking mode for x-thread pipe (read) (" << ::strerror (errno) << ')' << endmsg;
			return;
		}
		
		if (fcntl (fds[1], F_SETFL, O_NONBLOCK)) {
			error << "cannot set non-blocking mode for x-thread pipe (write) (%2)" << ::strerror (errno) << ')' << endmsg;
			return;
		}
	}
	fcntl(fds[0], F_SETFD, fcntl(fds[0], F_GETFD) | FD_CLOEXEC);
	fcntl(fds[1], F_SETFD, fcntl(fds[1], F_GETFD) | FD_CLOEXEC);
}

CrossThreadChannel::~CrossThreadChannel ()
{
	/* glibmm hack */
	drop_ios ();

	if (fds[0] >= 0) {
		close (fds[0]);
		fds[0] = -1;
	} 

	if (fds[1] >= 0) {
		close (fds[1]);
		fds[1] = -1;
	} 
}

void
CrossThreadChannel::wakeup ()
{
	char c = 0;
	(void) ::write (fds[1], &c, 1);
}

RefPtr<IOSource>
CrossThreadChannel::ios () 
{
	if (!_ios) {
		_ios = new RefPtr<IOSource> (IOSource::create (fds[0], IOCondition(IO_IN|IO_PRI|IO_ERR|IO_HUP|IO_NVAL)));
	}
	return *_ios;
}

void
CrossThreadChannel::drop_ios ()
{
	delete _ios;
	_ios = 0;
}

void
CrossThreadChannel::drain ()
{
	drain (fds[0]);
}

void
CrossThreadChannel::drain (int fd)
{
	/* drain selectable fd */
	char buf[64];
	while (::read (fd, buf, sizeof (buf)) > 0) {};
}

int
CrossThreadChannel::deliver (char msg)
{
        return ::write (fds[1], &msg, 1);
}

int 
CrossThreadChannel::receive (char& msg)
{
        return ::read (fds[0], &msg, 1);
}
