/*
    Copyright (C) 2009 John Emmas

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

#ifdef COMPILER_MSVC

//#include <glib/gtimer.h>
#include "pbd/msvc_pbd.h"

#ifndef _DWORD_DEFINED
#define _DWORD_DEFINED
typedef unsigned long DWORD;
#endif  // !_DWORD_DEFINED

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                               *
 * Note that this entire strategy failed to work, at least for pipes. It turned  *
 * out that Windows 'tell()' always returns 0 when used on a pipe. This strategy *
 * is now deprecated, having been replaced by a new pipe-like object, which I've *
 * called 'PBD::pipex'. This polling functionality is included here mostly so    *
 * that Ardour will build and launch under Windows. However, any module that     *
 * relies on polling a pipe will eventually need to use the new pipex object.    *
 * This code will allow it to compile and link successfully, although it won't   *
 * poll successfully at run time. Having said that, these functions might well   *
 * work for ports and/or other machanisms that get represented by a file handle. *
 *                                                                               *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int poll_input (struct pollfd *fds, nfds_t nfds, int& elapsed_time, int timeout)
{
DWORD dwOldTickCount,
      dwNewTickCount = GetTickCount();
bool  input = false,
      error = false;
int   ret = 0;

	if (NULL != fds)
	{
		nfds_t loop;
		short ev_mask = (POLLOUT|POLLWRNORM|POLLWRBAND);

		errno = NO_ERROR;

		do
		{
			dwOldTickCount = dwNewTickCount;

			for (loop=0; loop<nfds; loop++)
				fds[loop].revents = 0;

			for (loop=0; (loop<nfds && !error); loop++)
			{
				if (!(fds[loop].events & ev_mask))
				{
					long pos = _tell(fds[loop].fd);

					if (0 > pos)
					{
						// An error occured ('errno' should have been set by '_tell()')
						ret = (-1);
						fds[loop].revents = POLLERR;
						if (fds[loop].events & POLLRDNORM)
							fds[loop].revents |= POLLRDNORM;
						if (fds[loop].events & POLLRDBAND)
							fds[loop].revents |= POLLRDBAND;
						if (fds[loop].events & POLLPRI)
							fds[loop].revents |= POLLPRI;

						// Do we want to abort on error?
						if (fds[loop].events & POLLERR)
							error = true;
					}
					else if (pos > 0)
					{
						// Input characters were found for this fd
						ret += 1;
						if (fds[loop].events & POLLRDNORM)
							fds[loop].revents |= POLLRDNORM;
						if (fds[loop].events & POLLRDBAND)
							fds[loop].revents |= POLLRDBAND;
						if (fds[loop].events & POLLPRI)
							fds[loop].revents |= POLLPRI;

						// Do we want to abort on input?
						if ((fds[loop].events & POLLIN)     ||
						    (fds[loop].events & POLLPRI)    ||
						    (fds[loop].events & POLLRDNORM) ||
						    (fds[loop].events & POLLRDBAND))
							input = true;
					}
				}
			}

			if (input)
				break;

			dwNewTickCount = GetTickCount();
			elapsed_time += (dwNewTickCount-dwOldTickCount);
			// Note that the above will wrap round if the user leaves
			// his computer powered up for more than about 50 days!

			// Sleep briefly because GetTickCount() only has an accuracy of 10mS
			Sleep(10); // For some reason 'g_usleep()' craps over everything here. Different 'C' runtimes???

		} while ((!error) && ((timeout == (-1)) || (elapsed_time < timeout)));
	}
	else
	{
		errno = ERROR_BAD_ARGUMENTS;
		ret = (-1);
	}

	return (ret);
}

int poll_output (struct pollfd *fds, nfds_t nfds, int& elapsed_time, int timeout)
{
int ret = 0; // This functionality is not yet implemented

	if (NULL != fds)
	{
		// Just flag whichever pollfd was specified for writing
		short ev_mask = (POLLOUT|POLLWRNORM|POLLWRBAND);

		errno = NO_ERROR;
		elapsed_time = 0;

		for (nfds_t loop=0; loop<nfds; loop++)
		{
			if (fds[loop].events & ev_mask)
			{
				fds[loop].revents = POLLNVAL;
				errno = ERROR_INVALID_ACCESS;
				ret = (-1);
			}
		}
	}
	else
	{
		errno = ERROR_BAD_ARGUMENTS;
		ret = (-1);
	}

	return (ret);
}

//***************************************************************
//
//	poll()
//
// Emulates POSIX poll() using Win32 _tell().
//
//	Returns:
//
//    On Success: A positive integer indicating the total number
//                of file descriptors that were available for
//                writing or had data available for reading.
//    On Failure: -1 (the actual error is saved in 'errno').
//
LIBPBD_API int PBD_APICALLTYPE
poll (struct pollfd *fds, nfds_t nfds, int timeout)
{
int elapsed_time = 0;
int ret = (-1);

	// Note that this functionality is not fully implemented.
	// At the time of writing, Ardour seems only to poll on
	// read pipes. Therefore return an error if any write
	// pipe seems to have been specified or if too many file
	// descriptors were passed.
	short ev_mask = (POLLOUT|POLLWRNORM|POLLWRBAND);

	if ((nfds > OPEN_MAX) || (nfds > NPOLLFILE))
	{
		errno = ERROR_TOO_MANY_OPEN_FILES;
	}
	else
	{
		ret = 0;

		for (nfds_t loop=0; loop<nfds; loop++)
		{
			if (fds[loop].events & ev_mask)
			{
				ret = poll_output(fds, nfds, elapsed_time, timeout);
				break;
			}
		}

		if (0 == ret)
		{
			// Poll for input
			ret = poll_input(fds, nfds, elapsed_time, timeout);
		}
	}

	return (ret);
}

#endif  //COMPILER_MSVC
