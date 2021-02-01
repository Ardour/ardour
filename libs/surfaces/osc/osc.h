/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2013-2016 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Johannes Mueller <github@johannes-mueller.org>
 * Copyright (C) 2016-2018 Len Ovens <len@ovenwerks.net>
 * Copyright (C) 2016 Ben Loftis <ben@harrisonconsoles.com>
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
#include "ardour/send.h"
#include "ardour/plugin.h"
#include "control_protocol/control_protocol.h"

#include "pbd/i18n.h"

class OSCControllable;
class OSCRouteObserver;
class OSCGlobalObserver;
class OSCSelectObserver;
class OSCCueObserver;

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

	void stripable_selection_changed () {}

	bool has_editor () const { return true; }
	void* get_gui () const;
	void  tear_down_gui ();

	int set_active (bool yn);
	bool get_active () const;

	// generic osc send
	Glib::Threads::Mutex _lo_lock;
	int float_message (std::string, float value, lo_address addr);
	int int_message (std::string, int value, lo_address addr);
	int text_message (std::string path, std::string val, lo_address addr);
	int float_message_with_id (std::string, uint32_t ssid, float value, bool in_line, lo_address addr);
	int int_message_with_id (std::string, uint32_t ssid, int value, bool in_line, lo_address addr);
	int text_message_with_id (std::string path, uint32_t ssid, std::string val, bool in_line, lo_address addr);

	int send_group_list (lo_address addr);

	int start ();
	int stop ();

	static void* request_factory (uint32_t);

	enum OSCDebugMode {
		Off,
		Unhandled,
		All
	};

	enum OSCTempMode {
		TempOff = 0,
		GroupOnly = 1,
		VCAOnly = 2,
		BusOnly = 3
	};

	typedef std::vector<boost::shared_ptr<ARDOUR::Stripable> > Sorted;
	Sorted get_sorted_stripables(std::bitset<32> types, bool cue, uint32_t custom, Sorted my_list);
	typedef std::map<boost::shared_ptr<ARDOUR::AutomationControl>, uint32_t> FakeTouchMap;
	FakeTouchMap _touch_timeout;

// keep a surface's global setup by remote server url
	struct OSCSurface {
	public:
		//global
		std::string remote_url;		// the url these setting belong to
		bool no_clear;				// don't send osc clear messages on strip change
		uint32_t jogmode;			// current jogmode
		OSCGlobalObserver* global_obs;	// pointer to this surface's global observer
		uint32_t nstrips;			// how many strips are there for strip_types
		std::bitset<32> feedback;	// What is fed back? strips/meters/timecode/bar_beat/global
		int gainmode;				// what kind of faders do we have Gain db or position 0 to 1?
		PBD::Controllable::GroupControlDisposition usegroup;	// current group disposition
		Sorted custom_strips;		// a sorted list of user selected strips
		uint32_t custom_mode;		// use custom strip list
		OSCTempMode temp_mode;		// use temp strip list
		Sorted temp_strips;			// temp strip list for grouponly, vcaonly, auxonly
		boost::shared_ptr<ARDOUR::Stripable> temp_master; // stripable this surface uses as temp master
		Sorted strips;				// list of stripables for this surface
		// strips
		uint32_t bank;				// current bank
		uint32_t bank_size;			// size of banks for this surface
		std::vector<OSCRouteObserver*> observers;	// route observers for this surface
		std::bitset<32> strip_types;// what strip types are a part of this bank
		//select
		OSCSelectObserver* sel_obs;	// So we can sync select feedback with selected channel
		uint32_t expand;			// Used by /select/select
		bool expand_enable;			// use expand instead of select
		boost::shared_ptr<ARDOUR::Stripable> expand_strip; // stripable this surface uses for expand
		boost::shared_ptr<ARDOUR::Stripable> select; // stripable this surface uses as selected
		int plug_page;				// current plugin page
		uint32_t plug_page_size;	// plugin page size (number of controls)
		int plugin_id;			// id of current plugin
		std::vector<int> plug_params; // vector to store ports that are controls
		std::vector<int> plugins;	// stores allowable plugins with index (work around MB strip PIs)
		int send_page;				// current send page
		uint32_t send_page_size;	// send page size in channels
		uint32_t nsends;			// number of sends select has
		PBD::ScopedConnection proc_connection; // for processor signal monitoring
		// cue
		bool cue;					// is this a cue surface
		uint32_t aux;				// aux index for this cue surface
		Sorted sends;				// list of sends for cue aux
		OSCCueObserver* cue_obs;	// pointer to this surface's cue observer
		uint32_t linkset;			// ID of a set of surfaces used as one
		uint32_t linkid;			// ID of this surface within a linkset
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
		 * [14] - use OSC 1.0 only (#reply -> /reply)
		 *
		 * Strip_type bits:
		 * [0] - Audio Tracks
		 * [1] - Midi Tracks
		 * [2] - Audio Bus
		 * [3] - Midi Bus
		 * [4] - VCAs
		 * [5] - master
		 * [6] - Monitor
		 * [7] - Listen Bus
		 * [8] - Selected
		 * [9] - Hidden
		 * [10] - Use Groups
		 * [11] - Global Expand
		 */

// storage for  each surface's settings
	mutable Glib::Threads::Mutex surfaces_lock;
	typedef std::vector<OSCSurface> Surface;
	Surface _surface;

// linked surfaces
	struct LinkSet {
	public:
		std::vector<std::string> urls;	//urls of linked surfaces
		uint32_t banksize;				// linkset banksize
		uint32_t bank;					// linkset current bank
		bool autobank;					// banksize is derived from total
		uint32_t not_ready;				// number of 1st device, 0 = ready
		Sorted custom_strips;			// a sorted list of user selected strips
		uint32_t custom_mode;			// use custom strip list
		OSCTempMode temp_mode;			// use custom strip list
		Sorted temp_strips;				// temp strip list for grouponly, vcaonly, auxonly
		boost::shared_ptr<ARDOUR::Stripable> temp_master; // temp master stripable
		std::bitset<32> strip_types;	// strip_types for this linkset
		Sorted strips;					// list of valid strips in order for this set
	};

	std::map<uint32_t, LinkSet> link_sets;
	 // list of linksets

	struct PortAdd {
	public:
		std::string host;
		std::string port;
	};

	std::vector<PortAdd> _ports;

// GUI calls
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
	int get_send_size() { return default_send_size; }
	void set_send_size (int ss) { default_send_size = ss; }
	int get_plugin_size() { return default_plugin_size; }
	void set_plugin_size (int ps) { default_plugin_size = ps; }
	void clear_devices ();
	void gui_changed ();
	void get_surfaces ();
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
	OSCDebugMode _debugmode;
	bool address_only;
	std::string remote_port;
	uint32_t default_banksize;
	uint32_t default_strip;
	uint32_t default_feedback;
	uint32_t default_gainmode;
	uint32_t default_send_size;
	uint32_t default_plugin_size;
	bool tick;
	bool bank_dirty;
	bool observer_busy;
	float scrub_speed;		// Current scrub speed
	double scrub_place;		// place of play head at latest jog/scrub wheel tick
	int64_t scrub_time;		// when did the wheel move last?
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
	std::string get_port (std::string host);
	OSCSurface * get_surface (lo_address addr, bool quiet = false);
	int check_surface (lo_message msg);
	uint32_t get_sid (boost::shared_ptr<ARDOUR::Stripable> strip, lo_address addr);
	boost::shared_ptr<ARDOUR::Stripable> get_strip (uint32_t ssid, lo_address addr);
	void global_feedback (OSCSurface* sur);
	void strip_feedback (OSCSurface* sur, bool new_bank_size);
	void surface_destroy (OSCSurface* sur);
	uint32_t bank_limits_check (uint32_t bank, uint32_t size, uint32_t total);
	void bank_leds (OSCSurface* sur);

	void send_current_value (const char* path, lo_arg** argv, int argc, lo_message msg);
	void current_value_query (const char* path, size_t len, lo_arg **argv, int argc, lo_message msg);

	int current_value (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);

	int catchall (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
	static int _catchall (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);

	int set_automation (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg);
	int touch_detect (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg);
	int fake_touch (boost::shared_ptr<ARDOUR::AutomationControl> ctrl);

	int spill (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg);

	int route_get_sends (lo_message msg);
	int route_get_receives(lo_message msg);
	void routes_list (lo_message msg);
	int group_list (lo_message msg);
	void surface_list (lo_message msg);
	void transport_sample (lo_message msg);
	void transport_speed (lo_message msg);
	void record_enabled (lo_message msg);

	void add_marker_name(const std::string &markername) {
		add_marker(markername);
	}

	// cue
	Sorted cue_get_sorted_stripables(boost::shared_ptr<ARDOUR::Stripable> aux, uint32_t id, lo_address);
	int cue_parse (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg);
	int cue_set (uint32_t aux, lo_message msg);
	int _cue_set (uint32_t aux, lo_address addr);
	int cue_new_aux (std::string name, std::string dest_1, std::string dest_2, uint32_t count, lo_message msg);
	int cue_new_send (std::string rt_name, lo_message msg);
	int cue_connect_aux (std::string dest, lo_message msg);
	int cue_next (lo_message msg);
	int cue_previous (lo_message msg);
	int cue_send_fader (uint32_t id, float position, lo_message msg);
	int cue_send_enable (uint32_t id, float state, lo_message msg);
	int cue_aux_fader (float position, lo_message msg);
	int cue_aux_mute (float state, lo_message msg);
	void cue_set_aux (uint32_t aux, lo_message msg);
	boost::shared_ptr<ARDOUR::Send> cue_get_send (uint32_t id, lo_address addr);
	// end cue

	// link
	LinkSet * get_linkset (uint32_t set, lo_address addr);
	int parse_link (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg);
	int link_check (uint32_t linkset);
	int set_link (uint32_t set, uint32_t id, lo_address addr);
	void surface_link_state (LinkSet * set);
	void link_strip_types (uint32_t linkset, uint32_t striptypes);

#define OSC_DEBUG \
	if (_debugmode == All) { \
		debugmsg (dgettext(PACKAGE, "OSC"), path, types, argv, argc); \
	}

#define PATH_CALLBACK_MSG(name) \
	static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data) { \
		return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, msg); \
	} \
	int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg) { \
		OSC_DEBUG; \
		if (argc > 0 && !strcmp (types, "f") && argv[0]->f != 1.0) { return 0; } \
		name (msg); \
		return 0; \
	}

	PATH_CALLBACK_MSG(route_get_sends);
	PATH_CALLBACK_MSG(route_get_receives);
	PATH_CALLBACK_MSG(routes_list);
	PATH_CALLBACK_MSG(group_list);
	PATH_CALLBACK_MSG(sel_previous);
	PATH_CALLBACK_MSG(sel_next);
	PATH_CALLBACK_MSG(surface_list);
	PATH_CALLBACK_MSG(transport_sample);
	PATH_CALLBACK_MSG(transport_speed);
	PATH_CALLBACK_MSG(record_enabled);
	PATH_CALLBACK_MSG(refresh_surface);
	PATH_CALLBACK_MSG(bank_up);
	PATH_CALLBACK_MSG(bank_down);
	PATH_CALLBACK_MSG(custom_clear);

#define PATH_CALLBACK(name) \
	static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data) { \
		return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, msg); \
	} \
	int cb_ ## name (const char *path, const char *types, lo_arg ** argv, int argc, lo_message msg) { \
		OSC_DEBUG; \
		check_surface (msg); \
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

#define PATH_CALLBACK1(name,type,optional) \
	static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data) { \
		return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, msg); \
	} \
	int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg) { \
		OSC_DEBUG; \
		check_surface (msg); \
		if (argc > 0) { \
			name (optional argv[0]->type); \
		} \
		return 0; \
	}

	PATH_CALLBACK1(set_transport_speed,f,);
	PATH_CALLBACK1(add_marker_name,s,&);
	PATH_CALLBACK1(access_action,s,&);

	PATH_CALLBACK1(jump_by_bars,f,);
	PATH_CALLBACK1(jump_by_seconds,f,);
	PATH_CALLBACK1(click_level,f,);

#define PATH_CALLBACK1_MSG(name,arg1type) \
	static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data) { \
		return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, msg); \
	} \
	int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg) { \
		OSC_DEBUG; \
		if (argc > 0) { \
			name (argv[0]->arg1type, msg); \
		} \
		return 0; \
	}

#define PATH_CALLBACK1_MSG_s(name,arg1type) \
	static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data) { \
		return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, msg); \
	} \
	int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg) { \
		OSC_DEBUG; \
		if (argc > 0) { \
			name (&argv[0]->arg1type, msg); \
		} \
		return 0; \
	}

	PATH_CALLBACK1_MSG(scrub,f);
	PATH_CALLBACK1_MSG(jog,f);
	PATH_CALLBACK1_MSG(jog_mode,f);
	PATH_CALLBACK1_MSG(bank_delta,f);
	PATH_CALLBACK1_MSG(use_group,f);
	PATH_CALLBACK1_MSG_s(name_session,s);
	PATH_CALLBACK1_MSG_s(sel_new_personal_send,s);
	PATH_CALLBACK1_MSG(sel_master_send_enable,i);
	PATH_CALLBACK1_MSG(sel_pan_elevation,f);
	PATH_CALLBACK1_MSG(sel_pan_frontback,f);
	PATH_CALLBACK1_MSG(sel_pan_lfe,f);
	PATH_CALLBACK1_MSG(sel_send_page,f);
	PATH_CALLBACK1_MSG(sel_plug_page,f);
	PATH_CALLBACK1_MSG(sel_plugin,f);
	PATH_CALLBACK1_MSG(sel_plugin_activate,f);
	PATH_CALLBACK1_MSG(sel_comp_enable,f);
	PATH_CALLBACK1_MSG(sel_comp_threshold,f);
	PATH_CALLBACK1_MSG(sel_comp_speed,f);
	PATH_CALLBACK1_MSG(sel_comp_mode,f);
	PATH_CALLBACK1_MSG(sel_comp_makeup,f);
	PATH_CALLBACK1_MSG(sel_eq_enable,f);
	PATH_CALLBACK1_MSG(sel_eq_hpf_freq,f);
	PATH_CALLBACK1_MSG(sel_eq_hpf_enable,f);
	PATH_CALLBACK1_MSG(sel_eq_hpf_slope,f);
	PATH_CALLBACK1_MSG(sel_eq_lpf_freq,f);
	PATH_CALLBACK1_MSG(sel_eq_lpf_enable,f);
	PATH_CALLBACK1_MSG(sel_eq_lpf_slope,f);
	PATH_CALLBACK1_MSG(sel_expand,i);
	PATH_CALLBACK1_MSG(custom_mode,f);

#define PATH_CALLBACK2(name,arg1type,arg2type) \
	static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data) { \
		return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, msg); \
	} \
	int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg) { \
		OSC_DEBUG; \
		check_surface (msg); \
		if (argc > 1) { \
			name (argv[0]->arg1type, argv[1]->arg2type); \
		} \
		return 0; \
	}

#define PATH_CALLBACK2_MSG(name,arg1type,arg2type) \
	static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data) { \
		return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, msg); \
	} \
	int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg) { \
		OSC_DEBUG; \
		if (argc > 1) {	\
			name (argv[0]->arg1type, argv[1]->arg2type, msg); \
		} \
		return 0; \
	}

#define PATH_CALLBACK2_MSG_s(name,arg1type,arg2type) \
	static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data) { \
		return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, msg); \
	} \
	int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg) { \
		OSC_DEBUG; \
		if (argc > 1) {	\
			name (argv[0]->arg1type, &argv[1]->arg2type, msg); \
		} \
		return 0; \
	}

#define PATH_CALLBACK3(name,arg1type,arg2type,arg3type) \
	static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data) { \
		return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, msg); \
	} \
	int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg) { \
		OSC_DEBUG; \
		if (argc > 1) { \
			name (argv[0]->arg1type, argv[1]->arg2type,argv[2]->arg3type, msg); \
		} \
		return 0; \
	}

#define PATH_CALLBACK4(name,arg1type,arg2type,arg3type,arg4type) \
	static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data) { \
		return static_cast<OSC*>(user_data)->cb_ ## name (path, types, argv, argc, msg); \
	} \
	int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg) { \
		OSC_DEBUG; \
		if (argc > 1) { \
			name (argv[0]->arg1type, argv[1]->arg2type,argv[2]->arg3type,argv[3]->arg4type, msg); \
		} \
		return 0; \
	}

	PATH_CALLBACK2_MSG(sel_sendgain,i,f);
	PATH_CALLBACK2_MSG(sel_sendfader,i,f);
	PATH_CALLBACK2_MSG(sel_sendenable,i,f);
	PATH_CALLBACK2_MSG(sel_eq_gain,i,f);
	PATH_CALLBACK2_MSG(sel_eq_freq,i,f);
	PATH_CALLBACK2_MSG(sel_eq_q,i,f);
	PATH_CALLBACK2_MSG(sel_eq_shape,i,f);

	PATH_CALLBACK2(locate,i,i);
	PATH_CALLBACK2(loop_location,i,i);
	PATH_CALLBACK3(route_set_send_gain_dB,i,i,f);
	PATH_CALLBACK3(route_set_send_fader,i,i,f);
	PATH_CALLBACK3(route_set_send_enable,i,i,f);
	PATH_CALLBACK4(route_plugin_parameter,i,i,i,f);
	PATH_CALLBACK3(route_plugin_parameter_print,i,i,i);
	PATH_CALLBACK2_MSG(route_plugin_activate,i,i);
	PATH_CALLBACK2_MSG(route_plugin_deactivate,i,i);
	PATH_CALLBACK1_MSG(route_plugin_list,i);
	PATH_CALLBACK2_MSG(route_plugin_descriptor,i,i);
	PATH_CALLBACK2_MSG(route_plugin_reset,i,i);

	int strip_parse (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg);
	int master_parse (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg);
	int monitor_parse (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg);
	int _strip_parse (const char *path, const char *sub_path, const char* types, lo_arg **argv, int argc, boost::shared_ptr<ARDOUR::Stripable> s, int param_1, bool strp, lo_message msg);
	int strip_state (const char *path, boost::shared_ptr<ARDOUR::Stripable> s, int ssid, lo_message msg);
	int strip_list (lo_message msg);
	int _strip_select (boost::shared_ptr<ARDOUR::Stripable> s, lo_address addr);
	int _strip_select2 (boost::shared_ptr<ARDOUR::Stripable> s, OSCSurface *sur, lo_address addr);

	void loop_location (int start, int end);

	int route_set_send_gain_dB (int rid, int sid, float val, lo_message msg);
	int route_set_send_fader (int rid, int sid, float val, lo_message msg);
	int route_set_send_enable (int rid, int sid, float val, lo_message msg);
	int route_plugin_parameter (int rid, int piid,int par, float val, lo_message msg);
	int route_plugin_parameter_print (int rid, int piid,int par, lo_message msg);
	int route_plugin_activate (int rid, int piid, lo_message msg);
	int route_plugin_deactivate (int rid, int piid, lo_message msg);
	int route_plugin_list(int ssid, lo_message msg);
	int route_plugin_descriptor(int ssid, int piid, lo_message msg);
	int route_plugin_reset(int ssid, int piid, lo_message msg);

	//banking functions
	int set_bank (uint32_t bank_start, lo_message msg);
	int _set_bank (uint32_t bank_start, lo_address addr);
	int bank_up (lo_message msg);
	int bank_delta (float delta, lo_message msg);
	int use_group (float value, lo_message msg);
	int bank_down (lo_message msg);
	// surface set up
	int surface_parse (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg);
	int set_surface (uint32_t b_size, uint32_t strips, uint32_t fb, uint32_t gmode, uint32_t se_size, uint32_t pi_size, lo_message msg);
	int set_surface_bank_size (uint32_t bs, lo_message msg);
	int set_surface_strip_types (uint32_t st, lo_message msg);
	int set_surface_feedback (uint32_t fb, lo_message msg);
	int set_surface_gainmode (uint32_t gm, lo_message msg);
	int set_surface_port (uint32_t po, lo_message msg);
	int refresh_surface (lo_message msg);
	int custom_clear (lo_message msg);
	int custom_mode (float state, lo_message msg);
	int _custom_mode (uint32_t state, lo_address addr);
	int name_session (char *n, lo_message msg);
	// select
	int select_parse (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg);
	int sel_send_pagesize (uint32_t size, lo_message msg);
	int sel_send_page (int page, lo_message msg);
	int sel_plug_pagesize (uint32_t size, lo_message msg);
	int sel_plug_page (int page, lo_message msg);
	int sel_plugin (int delta, lo_message msg);
	int _sel_plugin (int id, lo_address addr);
	int sel_plugin_activate (float state, lo_message msg);
	int select_plugin_parameter (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg);
	void processor_changed (std::string remote_url);

	int scrub (float delta, lo_message msg);
	int jog (float delta, lo_message msg);
	int jog_mode (float mode, lo_message msg);
	int set_marker (const char* types, lo_arg **argv, int argc, lo_message msg);
	int click_level (float position);
	int sel_previous (lo_message msg);
	int sel_next (lo_message msg);
	int sel_delta (int delta, lo_message msg);
	boost::shared_ptr<ARDOUR::Send> get_send (boost::shared_ptr<ARDOUR::Stripable> st, lo_address addr);
	int sel_sendgain (int id, float dB, lo_message msg);
	int sel_sendfader (int id, float pos, lo_message msg);
	int sel_sendenable (int id, float pos, lo_message msg);
	int sel_master_send_enable (int state, lo_message msg);
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
	int sel_eq_hpf_freq (float val, lo_message msg);
	int sel_eq_hpf_enable (float val, lo_message msg);
	int sel_eq_hpf_slope (float val, lo_message msg);
	int sel_eq_lpf_freq (float val, lo_message msg);
	int sel_eq_lpf_enable (float val, lo_message msg);
	int sel_eq_lpf_slope (float val, lo_message msg);
	int sel_eq_gain (int id, float val, lo_message msg);
	int sel_eq_freq (int id, float val, lo_message msg);
	int sel_eq_q (int id, float val, lo_message msg);
	int sel_eq_shape (int id, float val, lo_message msg);
	int sel_new_personal_send (char *n, lo_message msg);
	int set_temp_mode (lo_address addr);
	int parse_sel_group (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg);
	boost::shared_ptr<ARDOUR::VCA> get_vca_by_name (std::string vname);

	void listen_to_route (boost::shared_ptr<ARDOUR::Stripable>, lo_address);

	void route_name_changed (const PBD::PropertyChange&, boost::weak_ptr<ARDOUR::Route> r, lo_address addr);
	void recalcbanks ();
	void _recalcbanks ();
	void notify_routes_added (ARDOUR::RouteList &);
	void notify_vca_added (ARDOUR::VCAList &);

	int cancel_all_solos ();
	int osc_toggle_roll (bool ret2strt);
	bool periodic (void);
	sigc::connection periodic_connection;
	PBD::ScopedConnectionList session_connections;

	void debugmsg (const char *prefix, const char *path, const char* types, lo_arg **argv, int argc);

	static OSC* _instance;

	mutable void *gui;
	void build_gui ();
};

} // namespace

#endif // ardour_osc_h
