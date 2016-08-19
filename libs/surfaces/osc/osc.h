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
#include <vector>
#include <bitset>

#include <sys/time.h>
#include <pthread.h>

#include <boost/shared_ptr.hpp>

#include <lo/lo.h>

#include <glibmm/main.h>

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"

#include "ardour/types.h"
#include "control_protocol/control_protocol.h"

#include "pbd/i18n.h"

class OSCControllable;
class OSCRouteObserver;
class OSCGlobalObserver;
class OSCSelectObserver;

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

	typedef std::vector<boost::shared_ptr<ARDOUR::Stripable> > Sorted;
	Sorted get_sorted_stripables(std::bitset<32> types);

// keep a surface's global setup by remote server url
	struct OSCSurface {
	public:
		std::string remote_url;		// the url these setting belong to
		uint32_t bank;				// current bank
		uint32_t bank_size;			// size of banks for this surface
		std::bitset<32> strip_types;// what strip types are a part of this bank
		uint32_t nstrips;			// how many strips are there for strip_types
		std::bitset<32> feedback;	// What is fed back? strips/meters/timecode/bar_beat/global
		int gainmode;				// what kind of faders do we have Gain db or position 0 to 1?
		uint32_t expand;			// Used by /select/select
		bool expand_enable;			// use expand instead of select
		OSCSelectObserver* sel_obs;	// So we can sync select feedback with selected channel
		Sorted strips;				// list of stripables for this surface
	};
		/*
		 * feedback bits:
		 * [0] - Strips - buttons
		 * [1] - Strips - variables (pots/faders)
		 * [2] - Strips - Send SSID as path extension
		 * [3] - Send heart beat to surface
		 * [4] - Send feedback for master/global buttons/variables
		 * [5] - Send Bar and Beat
		 * [6] - Send Time code
		 * [7] - Send metering as dB or positional depending on gainmode
		 * [8] - Send metering as 16 bits (led strip)
		 * [9] - Send signal present (signal greater than -20dB)
		 * [10] - Send Playhead position as samples
		 * [11] - Send Playhead position as minutes seconds
		 * [12]	- Send Playhead position like primary/secondary GUI clocks
		 * [13] - Send well known feedback (for /select/command
		 */


// storage for  each surface's settings
	typedef std::vector<OSCSurface> Surface;
	Surface _surface;

	std::string get_server_url ();
	void set_debug_mode (OSCDebugMode m) { _debugmode = m; }
	OSCDebugMode get_debug_mode () { return _debugmode; }
	int get_portmode() { return address_only; }
	void set_portmode (int pm) { address_only = pm; }
	int get_banksize () { return default_banksize; }
	void set_banksize (int bs) {default_banksize = bs; }
	int get_gainmode() { return default_gainmode; }
	void set_gainmode (int gm) { default_gainmode = gm; }
	int get_defaultstrip() { return default_strip; }
	void set_defaultstrip (int st) { default_strip = st; }
	int get_defaultfeedback() { return default_feedback; }
	void set_defaultfeedback (int fb) { default_feedback = fb; }
	void clear_devices ();
	std::string get_remote_port () { return remote_port; }
	void set_remote_port (std::string pt) { remote_port = pt; }

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
	bool address_only;
	std::string remote_port;
	uint32_t default_banksize;
	uint32_t default_strip;
	uint32_t default_feedback;
	uint32_t default_gainmode;
	bool tick;
	bool bank_dirty;
	bool global_init;
	boost::shared_ptr<ARDOUR::Stripable> _select;	// which stripable out of /surface/stripables is gui selected

	void register_callbacks ();

	void route_added (ARDOUR::RouteList&);

	// Handlers for "Application Hook" signals
	void session_loaded (ARDOUR::Session&);
	void session_exported (std::string, std::string);

	// end "Application Hook" handles

	std::string get_unix_server_url ();
	lo_address get_address (lo_message msg);
	OSCSurface * get_surface (lo_address addr);
	uint32_t get_sid (boost::shared_ptr<ARDOUR::Stripable> strip, lo_address addr);
	boost::shared_ptr<ARDOUR::Stripable> get_strip (uint32_t ssid, lo_address addr);
	void global_feedback (std::bitset<32> feedback, lo_address addr, uint32_t gainmode);

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
		if (argc > 0 && !strcmp (types, "f") && argv[0]->f != 1.0) { return 0; } \
		name (data);		\
		return 0;		\
	}

	PATH_CALLBACK_MSG(routes_list);
	PATH_CALLBACK_MSG(transport_frame);
	PATH_CALLBACK_MSG(transport_speed);
	PATH_CALLBACK_MSG(record_enabled);
	PATH_CALLBACK_MSG(refresh_surface);
	PATH_CALLBACK_MSG(bank_up);
	PATH_CALLBACK_MSG(bank_down);

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
	PATH_CALLBACK(cancel_all_solos);
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
	PATH_CALLBACK1(master_set_gain,f,);
	PATH_CALLBACK1(master_set_fader,f,);
	PATH_CALLBACK1(master_set_trim,f,);
	PATH_CALLBACK1(master_set_mute,i,);
	PATH_CALLBACK1(monitor_set_gain,f,);
	PATH_CALLBACK1(monitor_set_fader,f,);

#define PATH_CALLBACK1_MSG(name,arg1type)			\
        static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data) { \
		return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, data); \
        } \
        int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data) { \
		OSC_DEBUG;              \
                if (argc > 0) {						\
			name (argv[0]->arg1type, data); \
                }							\
		return 0;						\
	}

	// pan position needs message info to send feedback
	PATH_CALLBACK1_MSG(master_set_pan_stereo_position,f);

	PATH_CALLBACK1_MSG(set_surface_bank_size,i);
	PATH_CALLBACK1_MSG(set_surface_strip_types,i);
	PATH_CALLBACK1_MSG(set_surface_feedback,i);
	PATH_CALLBACK1_MSG(set_surface_gainmode,i);
	PATH_CALLBACK1_MSG(sel_recenable,i);
	PATH_CALLBACK1_MSG(sel_recsafe,i);
	PATH_CALLBACK1_MSG(sel_mute,i);
	PATH_CALLBACK1_MSG(sel_solo,i);
	PATH_CALLBACK1_MSG(sel_solo_iso,i);
	PATH_CALLBACK1_MSG(sel_solo_safe,i);
	PATH_CALLBACK1_MSG(sel_monitor_input,i);
	PATH_CALLBACK1_MSG(sel_monitor_disk,i);
	PATH_CALLBACK1_MSG(sel_phase,i);
	PATH_CALLBACK1_MSG(sel_gain,f);
	PATH_CALLBACK1_MSG(sel_fader,f);
	PATH_CALLBACK1_MSG(sel_trim,f);
	PATH_CALLBACK1_MSG(sel_pan_position,f);
	PATH_CALLBACK1_MSG(sel_pan_width,f);
	PATH_CALLBACK1_MSG(sel_pan_elevation,f);
	PATH_CALLBACK1_MSG(sel_pan_frontback,f);
	PATH_CALLBACK1_MSG(sel_pan_lfe,f);
	PATH_CALLBACK1_MSG(sel_comp_enable,f);
	PATH_CALLBACK1_MSG(sel_comp_threshold,f);
	PATH_CALLBACK1_MSG(sel_comp_speed,f);
	PATH_CALLBACK1_MSG(sel_comp_mode,f);
	PATH_CALLBACK1_MSG(sel_comp_makeup,f);
	PATH_CALLBACK1_MSG(sel_eq_enable,f);
	PATH_CALLBACK1_MSG(sel_eq_hpf,f);
	PATH_CALLBACK1_MSG(sel_expand,i);

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

#define PATH_CALLBACK2_MSG(name,arg1type,arg2type)			\
        static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data) { \
		return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, data); \
        } \
        int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data) { \
		OSC_DEBUG;              \
                if (argc > 1) {						\
			name (argv[0]->arg1type, argv[1]->arg2type, data); \
                }							\
		return 0;						\
	}

#define PATH_CALLBACK3(name,arg1type,arg2type,arg3type)                \
        static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data) { \
               return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, data); \
        } \
        int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data) { \
		OSC_DEBUG;              \
                if (argc > 1) {                                                \
                 name (argv[0]->arg1type, argv[1]->arg2type,argv[2]->arg3type, data); \
                }                                                      \
               return 0;                                               \
       }

#define PATH_CALLBACK4(name,arg1type,arg2type,arg3type,arg4type)               \
        static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data) { \
               return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, data); \
        } \
        int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data) { \
		OSC_DEBUG;              \
                if (argc > 1) {                                                \
                 name (argv[0]->arg1type, argv[1]->arg2type,argv[2]->arg3type,argv[3]->arg4type, data); \
                }                                                      \
               return 0;                                               \
       }

	PATH_CALLBACK2_MSG(sel_sendgain,i,f);
	PATH_CALLBACK2_MSG(sel_sendfader,i,f);
	PATH_CALLBACK2_MSG(sel_sendenable,i,f);
	PATH_CALLBACK2_MSG(sel_eq_gain,i,f);
	PATH_CALLBACK2_MSG(sel_eq_freq,i,f);
	PATH_CALLBACK2_MSG(sel_eq_q,i,f);
	PATH_CALLBACK2_MSG(sel_eq_shape,i,f);

	PATH_CALLBACK4(set_surface,i,i,i,i);
	PATH_CALLBACK2(locate,i,i);
	PATH_CALLBACK2(loop_location,i,i);
	PATH_CALLBACK2_MSG(route_mute,i,i);
	PATH_CALLBACK2_MSG(route_solo,i,i);
	PATH_CALLBACK2_MSG(route_solo_iso,i,i);
	PATH_CALLBACK2_MSG(route_solo_safe,i,i);
	PATH_CALLBACK2_MSG(route_recenable,i,i);
	PATH_CALLBACK2_MSG(route_recsafe,i,i);
	PATH_CALLBACK2_MSG(route_monitor_input,i,i);
	PATH_CALLBACK2_MSG(route_monitor_disk,i,i);
	PATH_CALLBACK2_MSG(strip_phase,i,i);
	PATH_CALLBACK2_MSG(strip_expand,i,i);
	PATH_CALLBACK2_MSG(strip_gui_select,i,i);
	PATH_CALLBACK2_MSG(route_set_gain_dB,i,f);
	PATH_CALLBACK2_MSG(route_set_gain_fader,i,f);
	PATH_CALLBACK2_MSG(route_set_trim_dB,i,f);
	PATH_CALLBACK2_MSG(route_set_pan_stereo_position,i,f);
	PATH_CALLBACK2_MSG(route_set_pan_stereo_width,i,f);
	PATH_CALLBACK3(route_set_send_gain_dB,i,i,f);
	PATH_CALLBACK3(route_set_send_fader,i,i,f);
	PATH_CALLBACK3(route_set_send_enable,i,i,f);
	PATH_CALLBACK4(route_plugin_parameter,i,i,i,f);
	PATH_CALLBACK3(route_plugin_parameter_print,i,i,i);

	int route_mute (int rid, int yn, lo_message msg);
	int route_solo (int rid, int yn, lo_message msg);
	int route_solo_iso (int rid, int yn, lo_message msg);
	int route_solo_safe (int rid, int yn, lo_message msg);
	int route_recenable (int rid, int yn, lo_message msg);
	int route_recsafe (int ssid, int yn, lo_message msg);
	int route_monitor_input (int rid, int yn, lo_message msg);
	int route_monitor_disk (int rid, int yn, lo_message msg);
	int strip_phase (int rid, int yn, lo_message msg);
	int strip_expand (int rid, int yn, lo_message msg);
	int _strip_select (boost::shared_ptr<ARDOUR::Stripable> s, lo_address addr);
	int strip_gui_select (int rid, int yn, lo_message msg);
	int route_set_gain_abs (int rid, float level, lo_message msg);
	int route_set_gain_dB (int rid, float dB, lo_message msg);
	int route_set_gain_fader (int rid, float pos, lo_message msg);
	int route_set_trim_abs (int rid, float level, lo_message msg);
	int route_set_trim_dB (int rid, float dB, lo_message msg);
	int route_set_pan_stereo_position (int rid, float left_right_fraction, lo_message msg);
	int route_set_pan_stereo_width (int rid, float percent, lo_message msg);
	int route_set_send_gain_dB (int rid, int sid, float val, lo_message msg);
	int route_set_send_fader (int rid, int sid, float val, lo_message msg);
	int route_set_send_enable (int rid, int sid, float val, lo_message msg);
	int route_plugin_parameter (int rid, int piid,int par, float val, lo_message msg);
	int route_plugin_parameter_print (int rid, int piid,int par, lo_message msg);

	//banking functions
	int set_bank (uint32_t bank_start, lo_message msg);
	int _set_bank (uint32_t bank_start, lo_address addr);
	int bank_up (lo_message msg);
	int bank_down (lo_message msg);
	int set_surface (uint32_t b_size, uint32_t strips, uint32_t fb, uint32_t gmode, lo_message msg);
	int set_surface_bank_size (uint32_t bs, lo_message msg);
	int set_surface_strip_types (uint32_t st, lo_message msg);
	int set_surface_feedback (uint32_t fb, lo_message msg);
	int set_surface_gainmode (uint32_t gm, lo_message msg);
	int refresh_surface (lo_message msg);

	int master_set_gain (float dB);
	int master_set_fader (float position);
	int master_set_trim (float dB);
	int master_set_pan_stereo_position (float position, lo_message msg);
	int master_set_mute (uint32_t state);
	int monitor_set_gain (float dB);
	int monitor_set_fader (float position);
	int sel_recenable (uint32_t state, lo_message msg);
	int sel_recsafe (uint32_t state, lo_message msg);
	int sel_mute (uint32_t state, lo_message msg);
	int sel_solo (uint32_t state, lo_message msg);
	int sel_solo_iso (uint32_t state, lo_message msg);
	int sel_solo_safe (uint32_t state, lo_message msg);
	int sel_monitor_input (uint32_t state, lo_message msg);
	int sel_monitor_disk (uint32_t state, lo_message msg);
	int sel_phase (uint32_t state, lo_message msg);
	int sel_gain (float state, lo_message msg);
	int sel_fader (float state, lo_message msg);
	int sel_trim (float val, lo_message msg);
	int sel_pan_position (float val, lo_message msg);
	int sel_pan_width (float val, lo_message msg);
	int sel_sendgain (int id, float dB, lo_message msg);
	int sel_sendfader (int id, float pos, lo_message msg);
	int sel_sendenable (int id, float pos, lo_message msg);
	int sel_expand (uint32_t state, lo_message msg);
	int sel_pan_elevation (float val, lo_message msg);
	int sel_pan_frontback (float val, lo_message msg);
	int sel_pan_lfe (float val, lo_message msg);
	int sel_comp_enable (float val, lo_message msg);
	int sel_comp_threshold (float val, lo_message msg);
	int sel_comp_speed (float val, lo_message msg);
	int sel_comp_mode (float val, lo_message msg);
	int sel_comp_makeup (float val, lo_message msg);
	int sel_eq_enable (float val, lo_message msg);
	int sel_eq_hpf (float val, lo_message msg);
	int sel_eq_gain (int id, float val, lo_message msg);
	int sel_eq_freq (int id, float val, lo_message msg);
	int sel_eq_q (int id, float val, lo_message msg);
	int sel_eq_shape (int id, float val, lo_message msg);

	void listen_to_route (boost::shared_ptr<ARDOUR::Stripable>, lo_address);
	void end_listen (boost::shared_ptr<ARDOUR::Stripable>, lo_address);
	void drop_route (boost::weak_ptr<ARDOUR::Stripable>);
	void route_lost (boost::weak_ptr<ARDOUR::Stripable>);
	void gui_selection_changed (void);

	void route_name_changed (const PBD::PropertyChange&, boost::weak_ptr<ARDOUR::Route> r, lo_address addr);
	void recalcbanks ();
	void _recalcbanks ();
	void notify_routes_added (ARDOUR::RouteList &);
	void notify_vca_added (ARDOUR::VCAList &);

	void update_clock ();
	int cancel_all_solos ();
	bool periodic (void);
	sigc::connection periodic_connection;
	PBD::ScopedConnectionList session_connections;

	int route_send_fail (std::string path, uint32_t ssid, float val, lo_address addr);
	int sel_send_fail (std::string path, uint32_t id, float val, lo_address addr);
	int sel_fail (std::string path, float val, lo_address addr);

	typedef std::list<OSCRouteObserver*> RouteObservers;

	RouteObservers route_observers;

	typedef std::list<OSCGlobalObserver*> GlobalObservers;
	GlobalObservers global_observers;

	void debugmsg (const char *prefix, const char *path, const char* types, lo_arg **argv, int argc);

	static OSC* _instance;

	mutable void *gui;
	void build_gui ();
};

} // namespace

#endif // ardour_osc_h
