/*
    Copyright (C) 2006 Paul Davis 

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

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

#include <pbd/pthread_utils.h>
#include <pbd/error.h>
#include <ardour/control_protocol.h>
#include <ardour/configuration.h>
#include <ardour/session.h>

using namespace ARDOUR;
using namespace std;

#include "i18n.h"

sigc::signal<void> ControlProtocol::ZoomToSession;
sigc::signal<void> ControlProtocol::ZoomOut;
sigc::signal<void> ControlProtocol::ZoomIn;
sigc::signal<void> ControlProtocol::Enter;
sigc::signal<void,float> ControlProtocol::ScrollTimeline;

ControlProtocol::ControlProtocol (Session& s, string str)
	: session (s), 
	  _name (str)
{
	active_thread = 1;
	thread_request_pipe[0] = -1;
	thread_request_pipe[1] = -1;
}

ControlProtocol::~ControlProtocol ()
{
	terminate_thread ();

	if (thread_request_pipe[0] >= 0) {
		close (thread_request_pipe[0]);
		close (thread_request_pipe[1]);
	}
}

void
ControlProtocol::set_send (SendWhat sw)
{
	_send = sw;
}

int
ControlProtocol::init_thread ()
{
	if (pipe (thread_request_pipe) != 0) {
		error << string_compose (_("%1: cannot create thread request pipe (%1)"), _name, strerror (errno))
		      << endmsg;
		return -1;
	}

	if (fcntl (thread_request_pipe[0], F_SETFL, O_NONBLOCK)) {
		error << string_compose(_("%1: cannot set O_NONBLOCK on read pipe (%2)"), _name, strerror (errno)) << endmsg;
		return -1;
	}

	if (fcntl (thread_request_pipe[1], F_SETFL, O_NONBLOCK)) {
		error << string_compose(_("%1: cannot set O_NONBLOCK on signal write pipe (%2)"), _name, strerror (errno)) << endmsg;
		return -1;
	}

	if (pthread_create_and_store ("tranzport delivery", &_thread, 0, _thread_work, this)) {
		error << string_compose (_("%1: could not create thread"), _name) << endmsg;
		return -1;
	}

	return 0;
}	

int
ControlProtocol::poke_thread (ThreadRequest::Type why)
{
	char c = (char) why;
	return !(write (thread_request_pipe[1], &c, 1) == 1);
}

int
ControlProtocol::start_thread ()
{
	return poke_thread (ThreadRequest::Start);
}

int
ControlProtocol::stop_thread ()
{
	return poke_thread (ThreadRequest::Stop);
}

void
ControlProtocol::set_active (bool yn)
{
	if (yn != active_thread) {

		if (yn) {
			/* make sure the feedback thread is alive */
			start_thread ();
		} else {
			/* maybe put the feedback thread to sleep */
			stop_thread ();
		}
		
		ActiveChanged ();
	}
}

void
ControlProtocol::terminate_thread ()
{
	void* status;
	poke_thread (ThreadRequest::Quit);
	pthread_join (_thread, &status);
}

void*
ControlProtocol::_thread_work (void* arg)
{
	return static_cast<ControlProtocol*> (arg)->thread_work ();
}

void*
ControlProtocol::thread_work ()
{
	PBD::ThreadCreated (pthread_self(), _name);

	struct pollfd pfd[1];
	int timeout;

	struct sched_param rtparam;
	int err;

	memset (&rtparam, 0, sizeof (rtparam));
	rtparam.sched_priority = 3; /* XXX should be relative to audio (JACK) thread */
	
	if ((err = pthread_setschedparam (pthread_self(), SCHED_FIFO, &rtparam)) != 0) {
		// do we care? not particularly.
		info << string_compose (_("%1: delivery thread not running with realtime scheduling (%2)"), _name, strerror (errno)) << endmsg;
	} 

	pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, 0);
	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);


	if (active_thread) {
		timeout = 10; // max (5, (int) Config->get_feedback_interval_ms());
	} else {
		timeout = -1;
	}

	while (1) {

		pfd[0].fd = thread_request_pipe[0];
		pfd[0].events = POLLIN|POLLHUP|POLLERR;

		if (poll (pfd, 1, timeout) < 0) {
			if (errno == EINTR) {
				continue;
			}
			error << string_compose (_("Protocol \"%1\" thread: poll failed (%2)"), _name, strerror (errno))
			      << endmsg;
			break;
		}

		if (pfd[0].revents & ~POLLIN) {
			error << string_compose (_("Error thread request pipe for protocol \"%1\""), _name) << endmsg;
			break;
		}

		if (pfd[0].revents & POLLIN) {

			char req;
			
			/* empty the pipe of all current requests */

			while (1) {
				size_t nread = read (thread_request_pipe[0], &req, sizeof (req));

				if (nread == 1) {
					switch ((ThreadRequest::Type) req) {
					
					case ThreadRequest::Start:
						timeout = 10; // max (5, (int) Config->get_feedback_interval_ms());
						active_thread++;
						break;
						
					case ThreadRequest::Stop:
						timeout = -1;
						if (active_thread) {
							active_thread--;
						} 
						break;
						
					case ThreadRequest::Quit:
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
					fatal << string_compose (_("Error reading from thread request pipe for protocol \"%1\""), _name) << endmsg;
					/*NOTREACHED*/
				}
			}
		}
		
		if (!active_thread) {
			continue;
		}

		if (send()) {
			
			if (send_route_feedback ()) {
				list<Route*> routes = session.get_routes(); /* copies the routes */
				send_route_feedback (routes);
			}

			send_global_feedback ();
		}
	}

	return 0;
}
