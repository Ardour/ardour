/*
 * Copyright (C) 2006-2009 Paul Davis
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

#include <boost/shared_ptr.hpp>

#include <lo/lo.h>

#include <glibmm/main.h>

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"

#include "ardour/types.h"
#include "control_protocol/control_protocol.h"

#include "i18n.h"

class OSCControllable;
class OSCRouteObserver;

namespace ARDOUR {
class Session;
class Route;
}

/* this is mostly a placeholder because I suspect that at some
   point we will want to add more members to accomodate
   certain types of requests to the OSC UI
*/

namespace ArdourSurface {

struct OSCUIRequest : public BaseUI::BaseRequestObject {
  public:
	OSCUIRequest () {}
	~OSCUIRequest() {}
};

class OSC : public ARDOUR::ControlProtocol, public AbstractUI<OSCUIRequest>
{
  public:
	OSC (ARDOUR::Session&, uint32_t port);
	virtual ~OSC();

	static OSC* instance() { return _instance; }

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	bool has_editor () const { return true; }
	void* get_gui () const;
	void  tear_down_gui ();

	int set_active (bool yn);
	bool get_active () const;
	int set_feedback (bool yn);
	bool get_feedback () const;


	int start ();
	int stop ();

	static void* request_factory (uint32_t);

	enum OSCDebugMode {
		Off,
		Unhandled,
		All
	};

	std::string get_server_url ();
	void set_debug_mode (OSCDebugMode m) { _debugmode = m; }
	OSCDebugMode get_debug_mode () { return _debugmode; }

  protected:
        void thread_init ();
	void do_request (OSCUIRequest*);

	GSource* local_server;
	GSource* remote_server;

	bool osc_input_handler (Glib::IOCondition, lo_server);

  private:
	uint32_t _port;
	volatile bool _ok;
	volatile bool _shutdown;
	lo_server _osc_server;
	lo_server _osc_unix_server;
	std::string _osc_unix_socket_path;
	std::string _osc_url_file;
	bool _send_route_changes;
	OSCDebugMode _debugmode;

	void register_callbacks ();

	void route_added (ARDOUR::RouteList&);

	// Handlers for "Application Hook" signals
	void session_loaded (ARDOUR::Session&);
	void session_exported (std::string, std::string);

	// end "Application Hook" handles

	std::string get_unix_server_url ();

	void send_current_value (const char* path, lo_arg** argv, int argc, lo_message msg);
	void current_value_query (const char* path, size_t len, lo_arg **argv, int argc, lo_message msg);

	int current_value (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);

	int catchall (const char *path, const char *types, lo_arg **argv, int argc, void *data);
	static int _catchall (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);

	void routes_list (lo_message msg);
	void transport_frame (lo_message msg);
	void transport_speed (lo_message msg);
	void record_enabled (lo_message msg);

#define OSC_DEBUG \
	if (_debugmode == All) { \
		debugmsg (dgettext(PACKAGE, "OSC"), path, types, argv, argc); \
	}

#define PATH_CALLBACK_MSG(name)					\
        static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data) { \
		return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, data); \
        } \
        int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data) { \
		OSC_DEBUG;              \
		name (data);		\
		return 0;		\
	}

	PATH_CALLBACK_MSG(routes_list);
	PATH_CALLBACK_MSG(transport_frame);
	PATH_CALLBACK_MSG(transport_speed);
	PATH_CALLBACK_MSG(record_enabled);

#define PATH_CALLBACK(name) \
        static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data) { \
		return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, data); \
        } \
        int cb_ ## name (const char *path, const char *types, lo_arg ** argv, int argc, void *) { \
		OSC_DEBUG;              \
		if (argc > 0 && !strcmp (types, "f") && argv[0]->f != 1.0) { return 0; } \
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
	PATH_CALLBACK(all_tracks_rec_in);
	PATH_CALLBACK(all_tracks_rec_out);
	PATH_CALLBACK(remove_marker_at_playhead);
	PATH_CALLBACK(mark_in);
	PATH_CALLBACK(mark_out);
	PATH_CALLBACK(toggle_click);
	PATH_CALLBACK(midi_panic);
	PATH_CALLBACK(toggle_roll);
	PATH_CALLBACK(stop_forget);
	PATH_CALLBACK(set_punch_range);
	PATH_CALLBACK(set_loop_range);
	PATH_CALLBACK(set_session_range);
	PATH_CALLBACK(toggle_monitor_mute);
	PATH_CALLBACK(toggle_monitor_dim);
	PATH_CALLBACK(toggle_monitor_mono);
	PATH_CALLBACK(quick_snapshot_stay);
	PATH_CALLBACK(quick_snapshot_switch);
	PATH_CALLBACK(fit_1_track);
	PATH_CALLBACK(fit_2_tracks);
	PATH_CALLBACK(fit_4_tracks);
	PATH_CALLBACK(fit_8_tracks);
	PATH_CALLBACK(fit_16_tracks);
	PATH_CALLBACK(fit_32_tracks);
	PATH_CALLBACK(fit_all_tracks);
	PATH_CALLBACK(zoom_100_ms);
	PATH_CALLBACK(zoom_1_sec);
	PATH_CALLBACK(zoom_10_sec);
	PATH_CALLBACK(zoom_1_min);
	PATH_CALLBACK(zoom_5_min);
	PATH_CALLBACK(zoom_10_min);
	PATH_CALLBACK(zoom_to_session);
	PATH_CALLBACK(temporal_zoom_in);
	PATH_CALLBACK(temporal_zoom_out);
	PATH_CALLBACK(scroll_up_1_track);
	PATH_CALLBACK(scroll_dn_1_track);
	PATH_CALLBACK(scroll_up_1_page);
	PATH_CALLBACK(scroll_dn_1_page);

#define PATH_CALLBACK1(name,type,optional)					\
        static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data) { \
		return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, data); \
        } \
        int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *) { \
		OSC_DEBUG;              \
                if (argc > 0) {						\
			name (optional argv[0]->type);		\
                }							\
		return 0;						\
	}

	PATH_CALLBACK1(set_transport_speed,f,);
	PATH_CALLBACK1(access_action,s,&);

	PATH_CALLBACK1(jump_by_bars,f,);
	PATH_CALLBACK1(jump_by_seconds,f,);

#define PATH_CALLBACK2(name,arg1type,arg2type)			\
        static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data) { \
		return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, data); \
        } \
        int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *) { \
		OSC_DEBUG;              \
                if (argc > 1) {						\
			name (argv[0]->arg1type, argv[1]->arg2type); \
                }							\
		return 0;						\
	}

#define PATH_CALLBACK3(name,arg1type,arg2type,arg3type)                \
        static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data) { \
               return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, data); \
        } \
        int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *) { \
		OSC_DEBUG;              \
                if (argc > 1) {                                                \
                 name (argv[0]->arg1type, argv[1]->arg2type,argv[2]->arg3type); \
                }                                                      \
               return 0;                                               \
       }

#define PATH_CALLBACK4(name,arg1type,arg2type,arg3type,arg4type)               \
        static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data) { \
               return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, data); \
        } \
        int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *) { \
		OSC_DEBUG;              \
                if (argc > 1) {                                                \
                 name (argv[0]->arg1type, argv[1]->arg2type,argv[2]->arg3type,argv[3]->arg4type); \
                }                                                      \
               return 0;                                               \
       }

        PATH_CALLBACK2(locate,i,i);
        PATH_CALLBACK2(loop_location,i,i);
	PATH_CALLBACK2(route_mute,i,i);
	PATH_CALLBACK2(route_solo,i,i);
	PATH_CALLBACK2(route_recenable,i,i);
	PATH_CALLBACK2(route_set_gain_abs,i,f);
	PATH_CALLBACK2(route_set_gain_dB,i,f);
	PATH_CALLBACK2(route_set_gain_fader,i,f);
	PATH_CALLBACK2(route_set_trim_abs,i,f);
	PATH_CALLBACK2(route_set_trim_dB,i,f);
	PATH_CALLBACK2(route_set_pan_stereo_position,i,f);
	PATH_CALLBACK2(route_set_pan_stereo_width,i,f);
        PATH_CALLBACK3(route_set_send_gain_abs,i,i,f);
        PATH_CALLBACK3(route_set_send_gain_dB,i,i,f);
        PATH_CALLBACK4(route_plugin_parameter,i,i,i,f);
        PATH_CALLBACK3(route_plugin_parameter_print,i,i,i);

	int route_mute (int rid, int yn);
	int route_solo (int rid, int yn);
	int route_recenable (int rid, int yn);
	int route_set_gain_abs (int rid, float level);
	int route_set_gain_dB (int rid, float dB);
	int route_set_gain_fader (int rid, float pos);
	int route_set_trim_abs (int rid, float level);
	int route_set_trim_dB (int rid, float dB);
	int route_set_pan_stereo_position (int rid, float left_right_fraction);
	int route_set_pan_stereo_width (int rid, float percent);
	int route_set_send_gain_abs (int rid, int sid, float val);
	int route_set_send_gain_dB (int rid, int sid, float val);
	int route_plugin_parameter (int rid, int piid,int par, float val);
	int route_plugin_parameter_print (int rid, int piid,int par);

	void listen_to_route (boost::shared_ptr<ARDOUR::Route>, lo_address);
	void end_listen (boost::shared_ptr<ARDOUR::Route>, lo_address);
	void drop_route (boost::weak_ptr<ARDOUR::Route>);

	void route_name_changed (const PBD::PropertyChange&, boost::weak_ptr<ARDOUR::Route> r, lo_address addr);

	void update_clock ();

	typedef std::list<OSCRouteObserver*> RouteObservers;

	RouteObservers route_observers;
	void debugmsg (const char *prefix, const char *path, const char* types, lo_arg **argv, int argc);

	static OSC* _instance;

	mutable void *gui;
	void build_gui ();
};

} // namespace

#endif // ardour_osc_h
