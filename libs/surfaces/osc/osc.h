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

#ifndef ardour_osc_h
#define ardour_osc_h

#include <string>

#include <sys/time.h>
#include <pthread.h>

#include <lo/lo.h>

#include <sigc++/sigc++.h>

#include <ardour/types.h>
#include <ardour/session.h>
#include <control_protocol/control_protocol.h>

namespace ARDOUR {

class Session;
class Route;
	
class OSC : public ControlProtocol
{
  public:
	OSC (Session&, uint32_t port);
	virtual ~OSC();

	XMLNode& get_state ();
	int set_state (const XMLNode&);

	int set_active (bool yn);
	bool get_active () const;
	int set_feedback (bool yn);
	bool get_feedback () const;

	void set_namespace_root (std::string);
	bool send_route_changes () const { return _send_route_changes; }
	void set_send_route_changes (bool yn);

	int start ();
	int stop ();

  private:
	uint32_t _port;
	volatile bool _ok;
	volatile bool _shutdown;
	lo_server _osc_server;
	lo_server _osc_unix_server;
	std::string _osc_unix_socket_path;
	std::string _osc_url_file;
	std::string _namespace_root;
	bool _send_route_changes;
	pthread_t _osc_thread;
	int _request_pipe[2];

	static void * _osc_receiver(void * arg);
	void osc_receiver();
	void send(); // This should accept an OSC payload

	bool init_osc_thread ();
	void terminate_osc_thread ();
	void poke_osc_thread ();

	void register_callbacks ();

	void route_added (ARDOUR::Session::RouteList&);
		
	// Handlers for "Application Hook" signals
	void session_loaded (ARDOUR::Session&);
	void session_exported (std::string, std::string);

	enum RouteChangeType {
		RouteSolo,
		RouteMute,
		RouteGain
	};

	void route_changed (void* ignored, RouteChangeType, ARDOUR::Route*, lo_address);
	void route_changed_deux (RouteChangeType, ARDOUR::Route*, lo_address);

	// end "Application Hook" handles

	std::string get_server_url ();
	std::string get_unix_server_url ();

	void send_current_value (const char* path, lo_arg** argv, int argc, lo_message msg);
	void current_value_query (const char* path, size_t len, lo_arg **argv, int argc, lo_message msg);
	int catchall (const char *path, const char *types, lo_arg **argv, int argc, void *data);
	static int _catchall (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);

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

#define PATH_CALLBACK1(name,type,optional)					\
        static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data) { \
		return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, data); \
        } \
        int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data) { \
                if (argc > 0) {						\
			name (optional argv[0]->type);		\
                }							\
		return 0;						\
	}

	PATH_CALLBACK1(set_transport_speed,f,);
	PATH_CALLBACK1(access_action,s,&);

#define PATH_CALLBACK2(name,arg1type,arg2type)			\
        static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data) { \
		return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, data); \
        } \
        int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data) { \
                if (argc > 1) {						\
			name (argv[0]->arg1type, argv[1]->arg2type); \
                }							\
		return 0;						\
	}

	PATH_CALLBACK2(route_mute,i,i);
	PATH_CALLBACK2(route_solo,i,i);
	PATH_CALLBACK2(route_recenable,i,i);
	PATH_CALLBACK2(route_set_gain_abs,i,f);
	PATH_CALLBACK2(route_set_gain_dB,i,f);

	int route_mute (int rid, int yn);
	int route_solo (int rid, int yn);
	int route_recenable (int rid, int yn);
	int route_set_gain_abs (int rid, float level);
	int route_set_gain_dB (int rid, float dB);

	struct Listener {
	    Route* route;
	    lo_address addr;
	    std::vector<sigc::connection> connections;

	    Listener (Route* r, lo_address a) : route (r), addr (a) {}
	};

	typedef std::pair<Route*, lo_address> ListenerPair;
	typedef std::list<Listener*> Listeners;

	Listeners listeners;

	void listen_to_route (const ListenerPair&);
	void drop_listener_pair (const ListenerPair&);
	void drop_listeners_by_route (Route*);
};

}

#endif // ardour_osc_h
