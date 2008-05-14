/*
 * Copyright (C) 2006 Paul Davis
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *  
 */

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <algorithm>

#include <sys/poll.h>
#include <unistd.h>
#include <fcntl.h>

#include <glibmm/miscutils.h>

#include <pbd/pthread_utils.h>

#include <ardour/osc.h>
#include <ardour/session.h>
#include <ardour/route.h>
#include <ardour/audio_track.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace sigc;
using namespace std;

static void error_callback(int num, const char *m, const char *path)
{
#ifdef DEBUG
	fprintf(stderr, "liblo server error %d in path %s: %s\n", num, path, m);
#endif
}

OSC::OSC (uint32_t port)
	: _port(port)
{
	_shutdown = false;
	_osc_server = 0;
	_osc_unix_server = 0;
	_osc_thread = 0;
}

int
OSC::start ()
{
	char tmpstr[255];

	if (_osc_server) {
		/* already started */
		return 0;
	}
	
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

	_osc_url_file = Glib::build_filename (get_user_ardour_path (), "osc_url");

	ofstream urlfile;
	urlfile.open(_osc_url_file.c_str(),ios::trunc);

	if (urlfile) {
		urlfile << get_server_url () << endl;
		urlfile.close();
	} else {  
		cerr << "Couldn't write '" <<  _osc_url_file << "'" <<endl;
	}
	
	register_callbacks();
	
	// lo_server_thread_add_method(_sthread, NULL, NULL, OSC::_dummy_handler, this);
		
	if (!init_osc_thread()) {
		return -1;
	}
	return 0;
}

int
OSC::stop ()
{	
	if (_osc_server == 0) {
		/* already stopped */
		return 0;
	}

	// stop server thread
	terminate_osc_thread();

	lo_server_free (_osc_server);
	_osc_server = 0;
	
	if (!_osc_unix_socket_path.empty()) {
		// unlink it
		unlink(_osc_unix_socket_path.c_str());
	}
	
   if (!  _osc_url_file.empty() ) {
      unlink(_osc_url_file.c_str() );
   }
	return 0;
}

OSC::~OSC()
{
	stop ();
}

void
OSC::register_callbacks()
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

#define REGISTER_CALLBACK(serv,path,types, function) lo_server_add_method (serv, path, types, OSC::_ ## function, this)
		
		REGISTER_CALLBACK (serv, "/ardour/add_marker", "", add_marker);
		REGISTER_CALLBACK (serv, "/ardour/access_action", "s", access_action);
		REGISTER_CALLBACK (serv, "/ardour/loop_toggle", "", loop_toggle);
		REGISTER_CALLBACK (serv, "/ardour/goto_start", "", goto_start);
		REGISTER_CALLBACK (serv, "/ardour/goto_end", "", goto_end);
		REGISTER_CALLBACK (serv, "/ardour/rewind", "", rewind);
		REGISTER_CALLBACK (serv, "/ardour/ffwd", "", ffwd);
		REGISTER_CALLBACK (serv, "/ardour/transport_stop", "", transport_stop);
		REGISTER_CALLBACK (serv, "/ardour/transport_play", "", transport_play);
		REGISTER_CALLBACK (serv, "/ardour/set_transport_speed", "f", set_transport_speed);
		REGISTER_CALLBACK (serv, "/ardour/save_state", "", save_state);
		REGISTER_CALLBACK (serv, "/ardour/prev_marker", "", prev_marker);
		REGISTER_CALLBACK (serv, "/ardour/next_marker", "", next_marker);
		REGISTER_CALLBACK (serv, "/ardour/undo", "", undo);
		REGISTER_CALLBACK (serv, "/ardour/redo", "", redo);
		REGISTER_CALLBACK (serv, "/ardour/toggle_punch_in", "", toggle_punch_in);
		REGISTER_CALLBACK (serv, "/ardour/toggle_punch_out", "", toggle_punch_out);
		REGISTER_CALLBACK (serv, "/ardour/rec_enable_toggle", "", rec_enable_toggle);
		REGISTER_CALLBACK (serv, "/ardour/toggle_all_rec_enables", "", toggle_all_rec_enables);

#if 0
		REGISTER_CALLBACK (serv, "/ardour/*/#current_value", "", current_value);
		REGISTER_CALLBACK (serv, "/ardour/set", "", set);
#endif

#if 0
		// un/register_update args= s:ctrl s:returl s:retpath
		lo_server_add_method(serv, "/register_update", "sss", OSC::global_register_update_handler, this);
		lo_server_add_method(serv, "/unregister_update", "sss", OSC::global_unregister_update_handler, this);
		lo_server_add_method(serv, "/register_auto_update", "siss", OSC::global_register_auto_update_handler, this);
		lo_server_add_method(serv, "/unregister_auto_update", "sss", OSC::_global_unregister_auto_update_handler, this);
#endif
	}
}

bool
OSC::init_osc_thread ()
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
	
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 500000);

	pthread_create (&_osc_thread, &attr, &OSC::_osc_receiver, this);
	if (!_osc_thread) {
		return false;
	}
	pthread_attr_destroy(&attr);

	//pthread_detach (_osc_thread);
	return true;
}

void
OSC::terminate_osc_thread ()
{
	void* status;

	_shutdown = true;
	
	poke_osc_thread ();

	pthread_join (_osc_thread, &status);
}

void
OSC::poke_osc_thread ()
{
	char c;

	if (write (_request_pipe[1], &c, 1) != 1) {
		cerr << "cannot send signal to osc thread! " <<  strerror (errno) << endl;
	}
}

std::string
OSC::get_server_url()
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
OSC::get_unix_server_url()
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
OSC::_osc_receiver(void * arg)
{
	PBD::ThreadCreated (pthread_self(), X_("OSC"));
	static_cast<OSC*> (arg)->osc_receiver();
	return 0;
}

void
OSC::osc_receiver()
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
OSC::set_session (Session& s)
{
	session = &s;
	session->GoingAway.connect (mem_fun (*this, &OSC::session_going_away));

	// "Application Hooks"
	session_loaded( s );
	session->Exported.connect( mem_fun( *this, &OSC::session_exported ) );
}

void
OSC::session_going_away ()
{
	session = 0;
}

// "Application Hook" Handlers //
void
OSC::session_loaded( Session& s ) {
	lo_address listener = lo_address_new( NULL, "7770" );
	lo_send( listener, "/session/loaded", "ss", s.path().c_str(), s.name().c_str() );
}

void
OSC::session_exported( std::string path, std::string name ) {
	lo_address listener = lo_address_new( NULL, "7770" );
	lo_send( listener, "/session/exported", "ss", path.c_str(), name.c_str() );
}

// end "Application Hook" Handlers //

/* path callbacks */

int 
OSC::current_value (const char *path, const char *types, lo_arg **argv, int argc, void *data, void* user_data) 
{ 
#if 0
	const char* returl;

	if (argc < 3 || types == 0 || strlen (types) < 3 || types[0] != 's' || types[1] != 's' || types[2] != s) {
		return 1;
	}

	const char *returl = argv[1]->s;
	lo_address addr = find_or_cache_addr (returl);

	const char *retpath = argv[2]->s;

	
	if (strcmp (argv[0]->s, "transport_frame")) {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_frame());
		}

	} else if (strcmp (argv[0]->s, "transport_speed")) {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_frame());
		}

	} else if (strcmp (argv[0]->s, "transport_locked")) {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_frame());
		}

	} else if (strcmp (argv[0]->s, "punch_in") {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_frame());
		}

	} else if (strcmp (argv[0]->s, "punch_out") {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_frame());
		}

	} else if (strcmp (argv[0]->s, "rec_enable") {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_frame());
		}

	} else {

		/* error */
	}
#endif
	return 0;
}
