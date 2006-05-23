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
 * $Id$
 */

#ifndef ardour_osc_h
#define ardour_osc_h

#include <string>

#include <sys/time.h>
#include <pthread.h>

#include <lo/lo.h>

#include <sigc++/sigc++.h>

#include <ardour/types.h>

#include "basic_ui.h"

namespace ARDOUR {
	class Session;
	class Route;

class OSC : public BasicUI, public sigc::trackable
{
  public:
	OSC (uint32_t port);
	virtual ~OSC();
	
	void set_session (ARDOUR::Session&);
	int start ();
	int stop ();

  private:
	uint32_t _port;
	volatile bool _ok;
	volatile bool _shutdown;
	lo_server _osc_server;
	lo_server _osc_unix_server;
	std::string _osc_unix_socket_path;
	pthread_t _osc_thread;
	int _request_pipe[2];

	static void * _osc_receiver(void * arg);
	void osc_receiver();

	bool init_osc_thread ();
	void terminate_osc_thread ();
	void poke_osc_thread ();

	void register_callbacks ();

	void session_going_away ();

	std::string get_server_url ();
	std::string get_unix_server_url ();

	int current_value (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);

#define PATH_CALLBACK(name) \
        static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data) { \
		return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, data); \
        } \
        int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data) { \
		name (); \
		return 0; \
	}

	PATH_CALLBACK(add_marker);
	PATH_CALLBACK(loop_toggle);
	PATH_CALLBACK(goto_start);
	PATH_CALLBACK(goto_end);
	PATH_CALLBACK(rewind);
	PATH_CALLBACK(ffwd);
	PATH_CALLBACK(transport_stop);
	PATH_CALLBACK(transport_play);
	PATH_CALLBACK(save_state);
	PATH_CALLBACK(prev_marker);
	PATH_CALLBACK(next_marker);
	PATH_CALLBACK(undo);
	PATH_CALLBACK(redo);
	PATH_CALLBACK(toggle_punch_in);
	PATH_CALLBACK(toggle_punch_out);
	PATH_CALLBACK(rec_enable_toggle);
	PATH_CALLBACK(toggle_all_rec_enables);

#define PATH_CALLBACK1(name,type) \
        static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data) { \
		return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, data); \
        } \
        int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data) { \
                if (argc > 0) { \
			name (argv[0]->type); \
                }\
		return 0; \
	}

	PATH_CALLBACK1(set_transport_speed,f);
};

}

#endif // ardour_osc_h
