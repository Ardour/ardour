/*
 * Copyright (C) 2014-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2016 Robin Gareus <robin@gareus.org>
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

#include <poll.h>

CrossThreadChannel::CrossThreadChannel (bool non_blocking)
        : receive_channel (0)
        , receive_source (0)
{
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

        receive_channel = g_io_channel_unix_new (fds[0]);
}

CrossThreadChannel::~CrossThreadChannel ()
{
	if (receive_source) {
		/* this disconnects it from any main context it was attached in
		   in ::attach(), this prevent its callback from being invoked
		   after the destructor has finished.
		*/
		g_source_destroy (receive_source);
	}

	if (receive_channel) {
                g_io_channel_unref (receive_channel);
                receive_channel = 0;
        }

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

void
CrossThreadChannel::drain ()
{
	char buf[64];
	while (::read (fds[0], buf, sizeof (buf)) > 0) {};
}

int
CrossThreadChannel::deliver (char msg)
{
        return ::write (fds[1], &msg, 1);
}

bool
CrossThreadChannel::poll_for_request()
{
	struct pollfd pfd[1];
	pfd[0].fd = fds[0];
	pfd[0].events = POLLIN|POLLERR|POLLHUP;
	while(true) {
		if (poll (pfd, 1, -1) < 0) {
			if (errno == EINTR) {
				continue;
			}
			break;
		}
		if (pfd[0].revents & ~POLLIN) {
			break;
		}

		if (pfd[0].revents & POLLIN) {
			return true;
		}
	}
	return false;
}

int
CrossThreadChannel::receive (char& msg, bool wait)
{
	if (wait) {
		if (!poll_for_request ()) {
			return -1;
		}
	}
        return ::read (fds[0], &msg, 1);
}
