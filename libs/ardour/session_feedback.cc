/*
    Copyright (C) 2004 Paul Davis 

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

#include <string>
#include <cmath>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <midi++/types.h>
#include <midi++/port.h>
#include <midi++/manager.h>
#include <pbd/error.h>
#include <pbd/lockmonitor.h>
#include <pbd/pthread_utils.h>

#include <ardour/configuration.h>
#include <ardour/audioengine.h>
#include <ardour/session.h>
#include <ardour/audio_track.h>
#include <ardour/diskstream.h>
#include <ardour/control_protocol.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
//using namespace sigc;

int
Session::init_feedback ()
{
	if (pipe (feedback_request_pipe) != 0) {
		error << string_compose (_("cannot create feedback request pipe (%1)"),
				  strerror (errno))
		      << endmsg;
		return -1;
	}

	if (fcntl (feedback_request_pipe[0], F_SETFL, O_NONBLOCK)) {
		error << string_compose(_("UI: cannot set O_NONBLOCK on "    "signal read pipe (%1)"), strerror (errno)) << endmsg;
		return -1;
	}

	if (fcntl (feedback_request_pipe[1], F_SETFL, O_NONBLOCK)) {
		error << string_compose(_("UI: cannot set O_NONBLOCK on "    "signal write pipe (%1)"), strerror (errno)) << endmsg;
		return -1;
	}

	active_feedback = 0;

	if (pthread_create_and_store ("feedback", &feedback_thread, 0, _feedback_thread_work, this)) {
		error << _("Session: could not create feedback thread") << endmsg;
		return -1;
	}

	return 0;
}	

int
Session::poke_feedback (FeedbackRequest::Type why)
{
	char c = (char) why;
	return !(write (feedback_request_pipe[1], &c, 1) == 1);
}

int
Session::start_feedback ()
{
	return poke_feedback (FeedbackRequest::Start);
}

int
Session::stop_feedback ()
{
	return poke_feedback (FeedbackRequest::Stop);
}

void
Session::set_feedback (bool yn)
{
	set_dirty();

	if (yn) {
		/* make sure the feedback thread is alive */
		start_feedback ();
	} else {
		/* maybe put the feedback thread to sleep */
		stop_feedback ();
	}

	ControlChanged (Feedback); /* EMIT SIGNAL */
}

bool
Session::get_feedback() const
{
	return active_feedback > 0;
}

void
Session::terminate_feedback ()
{
	void* status;
	poke_feedback (FeedbackRequest::Quit);
	pthread_join (feedback_thread, &status);
}

void*
Session::_feedback_thread_work (void* arg)
{
	return static_cast<Session*> (arg)->feedback_thread_work ();
}

void*
Session::feedback_thread_work ()
{
	PBD::ThreadCreated (pthread_self(), X_("Feedback"));
	struct pollfd pfd[1];
	int timeout;

	pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, 0);
	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	if (active_feedback) {
		timeout = max (5, (int) Config->get_feedback_interval_ms());
	} else {
		timeout = -1;
	}

	while (1) {

		pfd[0].fd = feedback_request_pipe[0];
		pfd[0].events = POLLIN|POLLHUP|POLLERR;

		if (poll (pfd, 1, timeout) < 0) {
			if (errno == EINTR) {
				continue;
			}
			error << string_compose (_("Feedback thread poll failed (%1)"),
					  strerror (errno))
			      << endmsg;
			break;
		}

		if (pfd[0].revents & ~POLLIN) {
			error << _("Error on feedback thread request pipe") << endmsg;
			break;
		}

		if (pfd[0].revents & POLLIN) {

			char req;
			
			/* empty the pipe of all current requests */

			while (1) {
				size_t nread = read (feedback_request_pipe[0], &req, sizeof (req));

				if (nread == 1) {
					switch ((FeedbackRequest::Type) req) {
					
					case FeedbackRequest::Start:
						timeout = max (5, (int) Config->get_feedback_interval_ms());
						active_feedback++;
						break;
						
					case FeedbackRequest::Stop:
						timeout = -1;
						if (active_feedback) {
							active_feedback--;
						} 
						break;
						
					case FeedbackRequest::Quit:
						pthread_exit_pbd (0);
						/*NOTREACHED*/
						break;
						
					default:
						break;
					}

				} else if (nread == 0) {
					break;
				} else if (errno == EAGAIN) {
					break;
				} else {
					fatal << _("Error reading from feedback request pipe") << endmsg;
					/*NOTREACHED*/
				}
			}
		}
		
		if (!active_feedback || transport_stopped()) {
			continue;
		}

		bool send = false;

		for (vector<ControlProtocol*>::iterator i = control_protocols.begin(); i != control_protocols.end(); ++i) {
			if ((*i)->send()) {
				send = true;
				break;
			}
		}
		
		if (send) {

			RouteList routes = get_routes(); /* copies the routes */

			for (vector<ControlProtocol*>::iterator i = control_protocols.begin(); i != control_protocols.end(); ++i) {
				if ((*i)->send_route_feedback ()) {
					(*i)->send_route_feedback (routes);
				}
				(*i)->send_global_feedback ();
			} 
		}
	}
	
	return 0;
}

	
