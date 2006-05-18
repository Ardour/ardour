/*
** Copyright (C) 2004 Jesse Chappell <jesse@essej.net>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**  
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**  
*/

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <algorithm>

#include <sys/poll.h>
#include <unistd.h>
#include <fcntl.h>

#include <sigc++/sigc++.h>

#include <lo/lo.h>

#include "osc_server.h"

#include "i18n.h"

using namespace sigc;
using namespace std;

#include <pbd/abstract_ui.cc>

static void error_callback(int num, const char *m, const char *path)
{
#ifdef DEBUG
	fprintf(stderr, "liblo server error %d in path %s: %s\n", num, path, m);
#endif
}

ControlOSC::ControlOSC (ARDOUR::Session& s, uint32_t port)
	: ControlProtocol(s, X_("OSC")), 
	  AbstractUI<OSCRequest> (X_("OSC"), false),
	  _port(port)
{
	_shutdown = false;
	_osc_server = 0;
	_osc_unix_server = 0;
	_osc_thread = 0;
}

int
ControlOSC::set_active (bool yn)
{
	if (yn) {

		char tmpstr[255];
		
		for (int j=0; j < 20; ++j) {
			snprintf(tmpstr, sizeof(tmpstr), "%d", _port);
			
			if ((_osc_server = lo_server_new (tmpstr, error_callback))) {
				break;
			}
#ifdef DEBUG		
			cerr << "can't get osc at port: " << _port << endl;
#endif
			_port++;
			continue;
		}
		
#ifdef ARDOUR_OSC_UNIX_SERVER
		
		// APPEARS sluggish for now
		     
		// attempt to create unix socket server too

		snprintf(tmpstr, sizeof(tmpstr), "/tmp/sooperlooper_XXXXXX");
		int fd = mkstemp(tmpstr);

		if (fd >= 0 ) {
			unlink (tmpstr);
			close (fd);

			_osc_unix_server = lo_server_new (tmpstr, error_callback);

			if (_osc_unix_server) {
				_osc_unix_socket_path = tmpstr;
			}
		}
#endif

		cerr << "OSC @ " << get_server_url () << endl;

		register_callbacks();
	
		on_session_load ();
		
		// lo_server_thread_add_method(_sthread, NULL, NULL, ControlOSC::_dummy_handler, this);
		
		if (!init_osc_thread()) {
			return -1;
		}
		
	} else {
		
		/* need to stop the OSC UDP server */

		if (!_osc_unix_socket_path.empty()) {
			// unlink it
			unlink(_osc_unix_socket_path.c_str());
		}
		
		// stop server thread
		terminate_osc_thread();
	}

	return 0;
}

bool
ControlOSC::caller_is_ui_thread ()
{
	return false;
}

ControlOSC::~ControlOSC()
{
	set_active (false);
}

void
ControlOSC::register_callbacks()
{
	lo_server srvs[2];
	lo_server serv;

	srvs[0] = _osc_server;
	srvs[1] = _osc_unix_server;
	
	for (size_t i = 0; i < 2; ++i) {

		if (!srvs[i]) {
			continue;
		}

		serv = srvs[i];

#define REGISTER_CALLBACK(serv,path,types, function) lo_server_add_method (serv, path, types, ControlOSC::_ ## function, this)
		
		REGISTER_CALLBACK (serv, "/session/add_marker", "", add_marker);
		REGISTER_CALLBACK (serv, "/session/loop_toggle", "", loop_toggle);
		REGISTER_CALLBACK (serv, "/session/goto_start", "", goto_start);
		REGISTER_CALLBACK (serv, "/session/goto_end", "", goto_end);
		REGISTER_CALLBACK (serv, "/session/rewind", "", rewind);
		REGISTER_CALLBACK (serv, "/session/ffwd", "", ffwd);
		REGISTER_CALLBACK (serv, "/session/transport_stop", "", transport_stop);
		REGISTER_CALLBACK (serv, "/session/transport_play", "", transport_play);
		REGISTER_CALLBACK (serv, "/session/set_transport_speed", "f", set_transport_speed);
		REGISTER_CALLBACK (serv, "/session/save_state", "", save_state);
		REGISTER_CALLBACK (serv, "/session/prev_marker", "", prev_marker);
		REGISTER_CALLBACK (serv, "/session/next_marker", "", next_marker);
		REGISTER_CALLBACK (serv, "/session/undo", "", undo);
		REGISTER_CALLBACK (serv, "/session/redo", "", redo);
		REGISTER_CALLBACK (serv, "/session/toggle_punch_in", "", toggle_punch_in);
		REGISTER_CALLBACK (serv, "/session/toggle_punch_out", "", toggle_punch_out);
		REGISTER_CALLBACK (serv, "/session/rec_enable_toggle", "", rec_enable_toggle);
		REGISTER_CALLBACK (serv, "/session/toggle_all_rec_enables", "", toggle_all_rec_enables);

#if 0

		lo_server_add_method(serv, "/session/set", "ss", ControlOSC::global_set_handler, this);
		lo_server_add_method(serv, "/session/get", "ss", ControlOSC::global_get_handler, this);

		// un/register_update args= s:ctrl s:returl s:retpath
		lo_server_add_method(serv, "/register_update", "sss", ControlOSC::global_register_update_handler, this);
		lo_server_add_method(serv, "/unregister_update", "sss", ControlOSC::global_unregister_update_handler, this);
		lo_server_add_method(serv, "/register_auto_update", "siss", ControlOSC::global_register_auto_update_handler, this);
		lo_server_add_method(serv, "/unregister_auto_update", "sss", ControlOSC::_global_unregister_auto_update_handler, this);
#endif
	}
}

bool
ControlOSC::init_osc_thread ()
{
	// create new thread to run server
	if (pipe (_request_pipe)) {
		cerr << "Cannot create osc request signal pipe" <<  strerror (errno) << endl;
		return false;
	}

	if (fcntl (_request_pipe[0], F_SETFL, O_NONBLOCK)) {
		cerr << "osc: cannot set O_NONBLOCK on signal read pipe " << strerror (errno) << endl;
		return false;
	}

	if (fcntl (_request_pipe[1], F_SETFL, O_NONBLOCK)) {
		cerr << "osc: cannot set O_NONBLOCK on signal write pipe " << strerror (errno) << endl;
		return false;
	}
	
	pthread_create (&_osc_thread, NULL, &ControlOSC::_osc_receiver, this);
	if (!_osc_thread) {
		return false;
	}

	//pthread_detach (_osc_thread);
	return true;
}

void
ControlOSC::terminate_osc_thread ()
{
	void* status;

	_shutdown = true;
	
	poke_osc_thread ();

	pthread_join (_osc_thread, &status);
}

void
ControlOSC::poke_osc_thread ()
{
	char c;

	if (write (_request_pipe[1], &c, 1) != 1) {
		cerr << "cannot send signal to osc thread! " <<  strerror (errno) << endl;
	}
}

void
ControlOSC::on_session_load ()
{
}

void
ControlOSC::on_session_unload ()
{
	// will be called from main event loop
}


std::string
ControlOSC::get_server_url()
{
	string url;
	char * urlstr;

	if (_osc_server) {
		urlstr = lo_server_get_url (_osc_server);
		url = urlstr;
		free (urlstr);
	}
	
	return url;
}

std::string
ControlOSC::get_unix_server_url()
{
	string url;
	char * urlstr;

	if (_osc_unix_server) {
		urlstr = lo_server_get_url (_osc_unix_server);
		url = urlstr;
		free (urlstr);
	}
	
	return url;
}


/* server thread */

void *
ControlOSC::_osc_receiver(void * arg)
{
	static_cast<ControlOSC*> (arg)->osc_receiver();
	return 0;
}

void
ControlOSC::osc_receiver()
{
	struct pollfd pfd[3];
	int fds[3];
	lo_server srvs[3];
	int nfds = 0;
	int timeout = -1;
	int ret;
	
	fds[0] = _request_pipe[0];
	nfds++;
	
	if (_osc_server && lo_server_get_socket_fd(_osc_server) >= 0) {
		fds[nfds] = lo_server_get_socket_fd(_osc_server);
		srvs[nfds] = _osc_server;
		nfds++;
	}

	if (_osc_unix_server && lo_server_get_socket_fd(_osc_unix_server) >= 0) {
		fds[nfds] = lo_server_get_socket_fd(_osc_unix_server);
		srvs[nfds] = _osc_unix_server;
		nfds++;
	}
	
	
	while (!_shutdown) {

		for (int i=0; i < nfds; ++i) {
			pfd[i].fd = fds[i];
			pfd[i].events = POLLIN|POLLPRI|POLLHUP|POLLERR;
			pfd[i].revents = 0;
		}
		
	again:
		//cerr << "poll on " << nfds << " for " << timeout << endl;
		if ((ret = poll (pfd, nfds, timeout)) < 0) {
			if (errno == EINTR) {
				/* gdb at work, perhaps */
				cerr << "EINTR hit " << endl;
				goto again;
			}
			
			cerr << "OSC thread poll failed: " <<  strerror (errno) << endl;
			
			break;
		}

		//cerr << "poll returned " << ret << "  pfd[0].revents = " << pfd[0].revents << "  pfd[1].revents = " << pfd[1].revents << endl;
		
		if (_shutdown) {
			break;
		}
		
		if ((pfd[0].revents & ~POLLIN)) {
			cerr << "OSC: error polling extra port" << endl;
			break;
		}
		
		for (int i=1; i < nfds; ++i) {
			if (pfd[i].revents & POLLIN)
			{
				// this invokes callbacks
				//cerr << "invoking recv on " << pfd[i].fd << endl;
				lo_server_recv(srvs[i]);
			}
		}

	}

	//cerr << "SL engine shutdown" << endl;
	
	if (_osc_server) {
		int fd = lo_server_get_socket_fd(_osc_server);
		if (fd >=0) {
				// hack around
			close(fd);
		}
		lo_server_free (_osc_server);
		_osc_server = 0;
	}

	if (_osc_unix_server) {
		cerr << "freeing unix server" << endl;
		lo_server_free (_osc_unix_server);
		_osc_unix_server = 0;
	}
	
	close(_request_pipe[0]);
	close(_request_pipe[1]);
}

void
ControlOSC::do_request (OSCRequest* req)
{
}

/* path callbacks */


