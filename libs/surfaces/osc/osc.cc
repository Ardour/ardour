/*
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2016 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2018 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015 Johannes Mueller <github@johannes-mueller.org>
 * Copyright (C) 2016-2018 Len Ovens <len@ovenwerks.net>
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

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <algorithm>

#include <unistd.h>
#include <fcntl.h>

#include "pbd/gstdio_compat.h"
#include <glibmm.h>

#include "pbd/control_math.h"
#include "pbd/controllable.h"
#include <pbd/convert.h>
#include <pbd/pthread_utils.h>
#include <pbd/file_utils.h>
#include <pbd/failed_constructor.h>

#include "temporal/timeline.h"

#include "ardour/amp.h"
#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/route_group.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"
#include "ardour/vca.h"
#include "ardour/monitor_control.h"
#include "ardour/dB.h"
#include "ardour/filesystem_paths.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"
#include "ardour/pannable.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/presentation_info.h"
#include "ardour/profile.h"
#include "ardour/send.h"
#include "ardour/internal_send.h"
#include "ardour/phase_control.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/solo_safe_control.h"
#include "ardour/vca_manager.h"

#include "osc_select_observer.h"
#include "osc.h"
#include "osc_controllable.h"
#include "osc_route_observer.h"
#include "osc_global_observer.h"
#include "osc_cue_observer.h"
#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace Glib;
using namespace ArdourSurface;

#include "pbd/abstract_ui.cc" // instantiate template

OSC* OSC::_instance = 0;

#ifdef DEBUG
static void error_callback(int num, const char *m, const char *path)
{
	fprintf(stderr, "liblo server error %d in path %s: %s\n", num, path, m);
}
#else
static void error_callback(int, const char *, const char *)
{

}
#endif

OSC::OSC (Session& s, uint32_t port)
	: ControlProtocol (s, X_("Open Sound Control (OSC)"))
	, AbstractUI<OSCUIRequest> (name())
	, local_server (0)
	, remote_server (0)
	, _port(port)
	, _ok (true)
	, _shutdown (false)
	, _osc_server (0)
	, _osc_unix_server (0)
	, _debugmode (Off)
	, address_only (true)
	, remote_port ("8000")
	, default_banksize (0)
	, default_strip (31)
	, default_feedback (0)
	, default_gainmode (0)
	, default_send_size (0)
	, default_plugin_size (0)
	, tick (true)
	, bank_dirty (false)
	, observer_busy (true)
	, scrub_speed (0)
	, gui (0)
{
	_instance = this;

	session->Exported.connect (*this, MISSING_INVALIDATOR, boost::bind (&OSC::session_exported, this, _1, _2), this);
}

OSC::~OSC()
{
	tick = false;
	stop ();
	tear_down_gui ();
	_instance = 0;
}

void*
OSC::request_factory (uint32_t num_requests)
{
	/* AbstractUI<T>::request_buffer_factory() is a template method only
	   instantiated in this source module. To provide something visible for
	   use in the interface/descriptor, we have this static method that is
	   template-free.
	*/
	return request_buffer_factory (num_requests);
}

void
OSC::do_request (OSCUIRequest* req)
{
	if (req->type == CallSlot) {

		call_slot (MISSING_INVALIDATOR, req->the_slot);

	} else if (req->type == Quit) {

		stop ();
	}
}

int
OSC::set_active (bool yn)
{
	if (yn != active()) {

		if (yn) {
			if (start ()) {
				return -1;
			}
		} else {
			if (stop ()) {
				return -1;
			}
		}

	}

	return ControlProtocol::set_active (yn);
}

bool
OSC::get_active () const
{
	return _osc_server != 0;
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

		//if ((_osc_server = lo_server_new_with_proto (tmpstr, LO_TCP, error_callback))) {
		//	break;
		//}

		if ((_osc_server = lo_server_new (tmpstr, error_callback))) {
			break;
		}

#ifdef DEBUG
		cerr << "can't get osc at port: " << _port << endl;
#endif
		_port++;
		continue;
	}

	if (!_osc_server) {
		return 1;
	}

#ifdef ARDOUR_OSC_UNIX_SERVER

	// APPEARS sluggish for now

	// attempt to create unix socket server too

	snprintf(tmpstr, sizeof(tmpstr), "/tmp/sooperlooper_XXXXXX");
	int fd = mkstemp(tmpstr);

	if (fd >= 0 ) {
		::g_unlink (tmpstr);
		close (fd);

		_osc_unix_server = lo_server_new (tmpstr, error_callback);

		if (_osc_unix_server) {
			_osc_unix_socket_path = tmpstr;
		}
	}
#endif

	PBD::info << "OSC @ " << get_server_url () << endmsg;

	std::string url_file;

	if (find_file (ardour_config_search_path(), "osc_url", url_file)) {
		_osc_url_file = url_file;
		if (g_file_set_contents (_osc_url_file.c_str(), get_server_url().c_str(), -1, NULL)) {
			cerr << "Couldn't write '" <<  _osc_url_file << "'" <<endl;
		}
	}

	observer_busy = false;
	register_callbacks();

	session_loaded (*session);

	// lo_server_thread_add_method(_sthread, NULL, NULL, OSC::_dummy_handler, this);

	/* startup the event loop thread */

	BaseUI::run ();

	// start timers for metering, timecode and heartbeat.
	// timecode and metering run at 100
	Glib::RefPtr<Glib::TimeoutSource> periodic_timeout = Glib::TimeoutSource::create (100); // milliseconds
	periodic_connection = periodic_timeout->connect (sigc::mem_fun (*this, &OSC::periodic));
	periodic_timeout->attach (main_loop()->get_context());

	// catch track reordering
	// receive routes added
	session->RouteAdded.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&OSC::notify_routes_added, this, _1), this);
	// receive VCAs added
	session->vca_manager().VCAAdded.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&OSC::notify_vca_added, this, _1), this);
	// order changed
	PresentationInfo::Change.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&OSC::recalcbanks, this), this);

	_select = ControlProtocol::first_selected_stripable();
	if(!_select) {
		_select = session->master_out ();
	}

	return 0;
}

void
OSC::thread_init ()
{
	pthread_set_name (event_loop_name().c_str());

	if (_osc_unix_server) {
		Glib::RefPtr<IOSource> src = IOSource::create (lo_server_get_socket_fd (_osc_unix_server), IO_IN|IO_HUP|IO_ERR);
		src->connect (sigc::bind (sigc::mem_fun (*this, &OSC::osc_input_handler), _osc_unix_server));
		src->attach (_main_loop->get_context());
		local_server = src->gobj();
		g_source_ref (local_server);
	}

	if (_osc_server) {
#ifdef PLATFORM_WINDOWS
		Glib::RefPtr<IOChannel> chan = Glib::IOChannel::create_from_win32_socket (lo_server_get_socket_fd (_osc_server));
		Glib::RefPtr<IOSource> src  = IOSource::create (chan, IO_IN|IO_HUP|IO_ERR);
#else
		Glib::RefPtr<IOSource> src  = IOSource::create (lo_server_get_socket_fd (_osc_server), IO_IN|IO_HUP|IO_ERR);
#endif
		src->connect (sigc::bind (sigc::mem_fun (*this, &OSC::osc_input_handler), _osc_server));
		src->attach (_main_loop->get_context());
		remote_server = src->gobj();
		g_source_ref (remote_server);
	}

	PBD::notify_event_loops_about_thread_creation (pthread_self(), event_loop_name(), 2048);
	SessionEvent::create_per_thread_pool (event_loop_name(), 128);
}

int
OSC::stop ()
{
	periodic_connection.disconnect ();
	session_connections.drop_connections ();

	// clear surfaces
	observer_busy = true;
	for (uint32_t it = 0; it < _surface.size (); ++it) {
		OSCSurface* sur = &_surface[it];
		surface_destroy (sur);
	}
	_surface.clear();

	/* stop main loop */
	if (local_server) {
		g_source_destroy (local_server);
		g_source_unref (local_server);
		local_server = 0;
	}

	if (remote_server) {
		g_source_destroy (remote_server);
		g_source_unref (remote_server);
		remote_server = 0;
	}

	BaseUI::quit ();

	if (_osc_server) {
		lo_server_free (_osc_server);
		_osc_server = 0;
	}

	if (_osc_unix_server) {
		lo_server_free (_osc_unix_server);
		_osc_unix_server = 0;
	}

	if (!_osc_unix_socket_path.empty()) {
		::g_unlink (_osc_unix_socket_path.c_str());
	}

	if (!_osc_url_file.empty() ) {
		::g_unlink (_osc_url_file.c_str() );
	}

	return 0;
}

void
OSC::surface_destroy (OSCSurface* sur)
{
	OSCSelectObserver* so;
	if ((so = dynamic_cast<OSCSelectObserver*>(sur->sel_obs)) != 0) {
		so->clear_observer ();
		delete so;
		sur->sel_obs = 0;
		PBD::ScopedConnection pc = sur->proc_connection;
		pc.disconnect ();
	}

	OSCCueObserver* co;
	if ((co = dynamic_cast<OSCCueObserver*>(sur->cue_obs)) != 0) {
		delete co;
		sur->cue_obs = 0;
		sur->sends.clear ();
	}

	OSCGlobalObserver* go;
	if ((go = dynamic_cast<OSCGlobalObserver*>(sur->global_obs)) != 0) {
		go->clear_observer ();
		delete go;
		sur->global_obs = 0;
	}
	uint32_t st_end = sur->observers.size ();

	for (uint32_t i = 0; i < st_end; i++) {
		OSCRouteObserver* ro;
		if ((ro = dynamic_cast<OSCRouteObserver*>(sur->observers[i])) != 0) {
			ro->clear_strip ();
			delete ro;
			ro = 0;
		}
	}
	sur->observers.clear();
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

		// Some controls have optional "f" for feedback or touchosc
		// http://hexler.net/docs/touchosc-controls-reference

		REGISTER_CALLBACK (serv, X_("/refresh"), "", refresh_surface);
		REGISTER_CALLBACK (serv, X_("/refresh"), "f", refresh_surface);
		REGISTER_CALLBACK (serv, X_("/group/list"), "", group_list);
		REGISTER_CALLBACK (serv, X_("/group/list"), "f", group_list);
		REGISTER_CALLBACK (serv, X_("/surface/list"), "", surface_list);
		REGISTER_CALLBACK (serv, X_("/surface/list"), "f", surface_list);
		REGISTER_CALLBACK (serv, X_("/add_marker"), "", add_marker);
		REGISTER_CALLBACK (serv, X_("/add_marker"), "f", add_marker);
		REGISTER_CALLBACK (serv, X_("/add_marker"), "s", add_marker_name);
		REGISTER_CALLBACK (serv, X_("/access_action"), "s", access_action);
		REGISTER_CALLBACK (serv, X_("/loop_toggle"), "", loop_toggle);
		REGISTER_CALLBACK (serv, X_("/loop_toggle"), "f", loop_toggle);
		REGISTER_CALLBACK (serv, X_("/loop_location"), "ii", loop_location);
		REGISTER_CALLBACK (serv, X_("/goto_start"), "", goto_start);
		REGISTER_CALLBACK (serv, X_("/goto_start"), "f", goto_start);
		REGISTER_CALLBACK (serv, X_("/goto_end"), "", goto_end);
		REGISTER_CALLBACK (serv, X_("/goto_end"), "f", goto_end);
		REGISTER_CALLBACK (serv, X_("/scrub"), "f", scrub);
		REGISTER_CALLBACK (serv, X_("/jog"), "f", jog);
		REGISTER_CALLBACK (serv, X_("/jog/mode"), "f", jog_mode);
		REGISTER_CALLBACK (serv, X_("/rewind"), "", rewind);
		REGISTER_CALLBACK (serv, X_("/rewind"), "f", rewind);
		REGISTER_CALLBACK (serv, X_("/ffwd"), "", ffwd);
		REGISTER_CALLBACK (serv, X_("/ffwd"), "f", ffwd);
		REGISTER_CALLBACK (serv, X_("/transport_stop"), "", transport_stop);
		REGISTER_CALLBACK (serv, X_("/transport_stop"), "f", transport_stop);
		REGISTER_CALLBACK (serv, X_("/transport_play"), "", transport_play);
		REGISTER_CALLBACK (serv, X_("/transport_play"), "f", transport_play);
		REGISTER_CALLBACK (serv, X_("/transport_frame"), "", transport_sample);
		REGISTER_CALLBACK (serv, X_("/transport_speed"), "", transport_speed);
		REGISTER_CALLBACK (serv, X_("/record_enabled"), "", record_enabled);
		REGISTER_CALLBACK (serv, X_("/set_transport_speed"), "f", set_transport_speed);
		// locate ii is position and bool roll
		REGISTER_CALLBACK (serv, X_("/locate"), "ii", locate);
		REGISTER_CALLBACK (serv, X_("/save_state"), "", save_state);
		REGISTER_CALLBACK (serv, X_("/save_state"), "f", save_state);
		REGISTER_CALLBACK (serv, X_("/prev_marker"), "", prev_marker);
		REGISTER_CALLBACK (serv, X_("/prev_marker"), "f", prev_marker);
		REGISTER_CALLBACK (serv, X_("/next_marker"), "", next_marker);
		REGISTER_CALLBACK (serv, X_("/next_marker"), "f", next_marker);
		REGISTER_CALLBACK (serv, X_("/undo"), "", undo);
		REGISTER_CALLBACK (serv, X_("/undo"), "f", undo);
		REGISTER_CALLBACK (serv, X_("/redo"), "", redo);
		REGISTER_CALLBACK (serv, X_("/redo"), "f", redo);
		REGISTER_CALLBACK (serv, X_("/toggle_punch_in"), "", toggle_punch_in);
		REGISTER_CALLBACK (serv, X_("/toggle_punch_in"), "f", toggle_punch_in);
		REGISTER_CALLBACK (serv, X_("/toggle_punch_out"), "", toggle_punch_out);
		REGISTER_CALLBACK (serv, X_("/toggle_punch_out"), "f", toggle_punch_out);
		REGISTER_CALLBACK (serv, X_("/rec_enable_toggle"), "", rec_enable_toggle);
		REGISTER_CALLBACK (serv, X_("/rec_enable_toggle"), "f", rec_enable_toggle);
		REGISTER_CALLBACK (serv, X_("/toggle_all_rec_enables"), "", toggle_all_rec_enables);
		REGISTER_CALLBACK (serv, X_("/toggle_all_rec_enables"), "f", toggle_all_rec_enables);
		REGISTER_CALLBACK (serv, X_("/all_tracks_rec_in"), "f", all_tracks_rec_in);
		REGISTER_CALLBACK (serv, X_("/all_tracks_rec_out"), "f", all_tracks_rec_out);
		REGISTER_CALLBACK (serv, X_("/cancel_all_solos"), "f", cancel_all_solos);
		REGISTER_CALLBACK (serv, X_("/remove_marker"), "", remove_marker_at_playhead);
		REGISTER_CALLBACK (serv, X_("/remove_marker"), "f", remove_marker_at_playhead);
		REGISTER_CALLBACK (serv, X_("/jump_bars"), "f", jump_by_bars);
		REGISTER_CALLBACK (serv, X_("/jump_seconds"), "f", jump_by_seconds);
		REGISTER_CALLBACK (serv, X_("/mark_in"), "", mark_in);
		REGISTER_CALLBACK (serv, X_("/mark_in"), "f", mark_in);
		REGISTER_CALLBACK (serv, X_("/mark_out"), "", mark_out);
		REGISTER_CALLBACK (serv, X_("/mark_out"), "f", mark_out);
		REGISTER_CALLBACK (serv, X_("/toggle_click"), "", toggle_click);
		REGISTER_CALLBACK (serv, X_("/toggle_click"), "f", toggle_click);
		REGISTER_CALLBACK (serv, X_("/click/level"), "f", click_level);
		REGISTER_CALLBACK (serv, X_("/midi_panic"), "", midi_panic);
		REGISTER_CALLBACK (serv, X_("/midi_panic"), "f", midi_panic);
		REGISTER_CALLBACK (serv, X_("/stop_forget"), "", stop_forget);
		REGISTER_CALLBACK (serv, X_("/stop_forget"), "f", stop_forget);
		REGISTER_CALLBACK (serv, X_("/set_punch_range"), "", set_punch_range);
		REGISTER_CALLBACK (serv, X_("/set_punch_range"), "f", set_punch_range);
		REGISTER_CALLBACK (serv, X_("/set_loop_range"), "", set_loop_range);
		REGISTER_CALLBACK (serv, X_("/set_loop_range"), "f", set_loop_range);
		REGISTER_CALLBACK (serv, X_("/set_session_range"), "", set_session_range);
		REGISTER_CALLBACK (serv, X_("/set_session_range"), "f", set_session_range);
		REGISTER_CALLBACK (serv, X_("/toggle_monitor_mute"), "", toggle_monitor_mute);
		REGISTER_CALLBACK (serv, X_("/toggle_monitor_mute"), "f", toggle_monitor_mute);
		REGISTER_CALLBACK (serv, X_("/toggle_monitor_dim"), "", toggle_monitor_dim);
		REGISTER_CALLBACK (serv, X_("/toggle_monitor_dim"), "f", toggle_monitor_dim);
		REGISTER_CALLBACK (serv, X_("/toggle_monitor_mono"), "", toggle_monitor_mono);
		REGISTER_CALLBACK (serv, X_("/toggle_monitor_mono"), "f", toggle_monitor_mono);
		REGISTER_CALLBACK (serv, X_("/quick_snapshot_switch"), "", quick_snapshot_switch);
		REGISTER_CALLBACK (serv, X_("/quick_snapshot_switch"), "f", quick_snapshot_switch);
		REGISTER_CALLBACK (serv, X_("/quick_snapshot_stay"), "", quick_snapshot_stay);
		REGISTER_CALLBACK (serv, X_("/quick_snapshot_stay"), "f", quick_snapshot_stay);
		REGISTER_CALLBACK (serv, X_("/session_name"), "s", name_session);
		REGISTER_CALLBACK (serv, X_("/fit_1_track"), "", fit_1_track);
		REGISTER_CALLBACK (serv, X_("/fit_1_track"), "f", fit_1_track);
		REGISTER_CALLBACK (serv, X_("/fit_2_tracks"), "", fit_2_tracks);
		REGISTER_CALLBACK (serv, X_("/fit_2_tracks"), "f", fit_2_tracks);
		REGISTER_CALLBACK (serv, X_("/fit_4_tracks"), "", fit_4_tracks);
		REGISTER_CALLBACK (serv, X_("/fit_4_tracks"), "f", fit_4_tracks);
		REGISTER_CALLBACK (serv, X_("/fit_8_tracks"), "", fit_8_tracks);
		REGISTER_CALLBACK (serv, X_("/fit_8_tracks"), "f", fit_8_tracks);
		REGISTER_CALLBACK (serv, X_("/fit_16_tracks"), "", fit_16_tracks);
		REGISTER_CALLBACK (serv, X_("/fit_16_tracks"), "f", fit_16_tracks);
		REGISTER_CALLBACK (serv, X_("/fit_32_tracks"), "", fit_32_tracks);
		REGISTER_CALLBACK (serv, X_("/fit_32_tracks"), "f", fit_32_tracks);
		REGISTER_CALLBACK (serv, X_("/fit_all_tracks"), "", fit_all_tracks);
		REGISTER_CALLBACK (serv, X_("/fit_all_tracks"), "f", fit_all_tracks);
		REGISTER_CALLBACK (serv, X_("/zoom_100_ms"), "", zoom_100_ms);
		REGISTER_CALLBACK (serv, X_("/zoom_100_ms"), "f", zoom_100_ms);
		REGISTER_CALLBACK (serv, X_("/zoom_1_sec"), "", zoom_1_sec);
		REGISTER_CALLBACK (serv, X_("/zoom_1_sec"), "f", zoom_1_sec);
		REGISTER_CALLBACK (serv, X_("/zoom_10_sec"), "", zoom_10_sec);
		REGISTER_CALLBACK (serv, X_("/zoom_10_sec"), "f", zoom_10_sec);
		REGISTER_CALLBACK (serv, X_("/zoom_1_min"), "", zoom_1_min);
		REGISTER_CALLBACK (serv, X_("/zoom_1_min"), "f", zoom_1_min);
		REGISTER_CALLBACK (serv, X_("/zoom_5_min"), "", zoom_5_min);
		REGISTER_CALLBACK (serv, X_("/zoom_5_min"), "f", zoom_5_min);
		REGISTER_CALLBACK (serv, X_("/zoom_10_min"), "", zoom_10_min);
		REGISTER_CALLBACK (serv, X_("/zoom_10_min"), "f", zoom_10_min);
		REGISTER_CALLBACK (serv, X_("/zoom_to_session"), "", zoom_to_session);
		REGISTER_CALLBACK (serv, X_("/zoom_to_session"), "f", zoom_to_session);
		REGISTER_CALLBACK (serv, X_("/temporal_zoom_in"), "f", temporal_zoom_in);
		REGISTER_CALLBACK (serv, X_("/temporal_zoom_in"), "", temporal_zoom_in);
		REGISTER_CALLBACK (serv, X_("/temporal_zoom_out"), "", temporal_zoom_out);
		REGISTER_CALLBACK (serv, X_("/temporal_zoom_out"), "f", temporal_zoom_out);
		REGISTER_CALLBACK (serv, X_("/scroll_up_1_track"), "f", scroll_up_1_track);
		REGISTER_CALLBACK (serv, X_("/scroll_up_1_track"), "", scroll_up_1_track);
		REGISTER_CALLBACK (serv, X_("/scroll_dn_1_track"), "f", scroll_dn_1_track);
		REGISTER_CALLBACK (serv, X_("/scroll_dn_1_track"), "", scroll_dn_1_track);
		REGISTER_CALLBACK (serv, X_("/scroll_up_1_page"), "f", scroll_up_1_page);
		REGISTER_CALLBACK (serv, X_("/scroll_up_1_page"), "", scroll_up_1_page);
		REGISTER_CALLBACK (serv, X_("/scroll_dn_1_page"), "f", scroll_dn_1_page);
		REGISTER_CALLBACK (serv, X_("/scroll_dn_1_page"), "", scroll_dn_1_page);
		REGISTER_CALLBACK (serv, X_("/bank_up"), "", bank_up);
		REGISTER_CALLBACK (serv, X_("/bank_up"), "f", bank_delta);
		REGISTER_CALLBACK (serv, X_("/bank_down"), "", bank_down);
		REGISTER_CALLBACK (serv, X_("/bank_down"), "f", bank_down);
		REGISTER_CALLBACK (serv, X_("/use_group"), "f", use_group);

		// Controls for the Selected strip
		REGISTER_CALLBACK (serv, X_("/select/previous"), "f", sel_previous);
		REGISTER_CALLBACK (serv, X_("/select/previous"), "", sel_previous);
		REGISTER_CALLBACK (serv, X_("/select/next"), "f", sel_next);
		REGISTER_CALLBACK (serv, X_("/select/next"), "", sel_next);
		REGISTER_CALLBACK (serv, X_("/select/send_gain"), "if", sel_sendgain);
		REGISTER_CALLBACK (serv, X_("/select/send_fader"), "if", sel_sendfader);
		REGISTER_CALLBACK (serv, X_("/select/send_enable"), "if", sel_sendenable);
		REGISTER_CALLBACK (serv, X_("/select/master_send_enable"), "i", sel_master_send_enable);
		REGISTER_CALLBACK (serv, X_("/select/send_page"), "f", sel_send_page);
		REGISTER_CALLBACK (serv, X_("/select/plug_page"), "f", sel_plug_page);
		REGISTER_CALLBACK (serv, X_("/select/plugin"), "f", sel_plugin);
		REGISTER_CALLBACK (serv, X_("/select/plugin/activate"), "f", sel_plugin_activate);
		REGISTER_CALLBACK (serv, X_("/select/expand"), "i", sel_expand);
		REGISTER_CALLBACK (serv, X_("/select/pan_elevation_position"), "f", sel_pan_elevation);
		REGISTER_CALLBACK (serv, X_("/select/pan_frontback_position"), "f", sel_pan_frontback);
		REGISTER_CALLBACK (serv, X_("/select/pan_lfe_control"), "f", sel_pan_lfe);
		REGISTER_CALLBACK (serv, X_("/select/comp_enable"), "f", sel_comp_enable);
		REGISTER_CALLBACK (serv, X_("/select/comp_threshold"), "f", sel_comp_threshold);
		REGISTER_CALLBACK (serv, X_("/select/comp_speed"), "f", sel_comp_speed);
		REGISTER_CALLBACK (serv, X_("/select/comp_mode"), "f", sel_comp_mode);
		REGISTER_CALLBACK (serv, X_("/select/comp_makeup"), "f", sel_comp_makeup);
		REGISTER_CALLBACK (serv, X_("/select/eq_enable"), "f", sel_eq_enable);
		REGISTER_CALLBACK (serv, X_("/select/eq_hpf/freq"), "f", sel_eq_hpf_freq);
		REGISTER_CALLBACK (serv, X_("/select/eq_hpf/enable"), "f", sel_eq_hpf_enable);
		REGISTER_CALLBACK (serv, X_("/select/eq_hpf/slope"), "f", sel_eq_hpf_slope);
		REGISTER_CALLBACK (serv, X_("/select/eq_lpf/freq"), "f", sel_eq_lpf_freq);
		REGISTER_CALLBACK (serv, X_("/select/eq_lpf/enable"), "f", sel_eq_lpf_enable);
		REGISTER_CALLBACK (serv, X_("/select/eq_lpf/slope"), "f", sel_eq_lpf_slope);
		REGISTER_CALLBACK (serv, X_("/select/eq_gain"), "if", sel_eq_gain);
		REGISTER_CALLBACK (serv, X_("/select/eq_freq"), "if", sel_eq_freq);
		REGISTER_CALLBACK (serv, X_("/select/eq_q"), "if", sel_eq_q);
		REGISTER_CALLBACK (serv, X_("/select/eq_shape"), "if", sel_eq_shape);
		REGISTER_CALLBACK (serv, X_("/select/add_personal_send"), "s", sel_new_personal_send);
		REGISTER_CALLBACK (serv, X_("/select/add_fldbck_send"), "s", sel_new_personal_send);

		/* These commands require the route index in addition to the arg; TouchOSC (et al) can't use these  */
		REGISTER_CALLBACK (serv, X_("/strip/custom/mode"), "f", custom_mode);
		REGISTER_CALLBACK (serv, X_("/strip/custom/clear"), "f", custom_clear);
		REGISTER_CALLBACK (serv, X_("/strip/custom/clear"), "", custom_clear);

		REGISTER_CALLBACK (serv, X_("/strip/plugin/parameter"), "iiif", route_plugin_parameter);
		// prints to cerr only
		REGISTER_CALLBACK (serv, X_("/strip/plugin/parameter/print"), "iii", route_plugin_parameter_print);
		REGISTER_CALLBACK (serv, X_("/strip/plugin/activate"), "ii", route_plugin_activate);
		REGISTER_CALLBACK (serv, X_("/strip/plugin/deactivate"), "ii", route_plugin_deactivate);
		REGISTER_CALLBACK (serv, X_("/strip/send/gain"), "iif", route_set_send_gain_dB);
		REGISTER_CALLBACK (serv, X_("/strip/send/fader"), "iif", route_set_send_fader);
		REGISTER_CALLBACK (serv, X_("/strip/send/enable"), "iif", route_set_send_enable);
		REGISTER_CALLBACK (serv, X_("/strip/sends"), "i", route_get_sends);
		REGISTER_CALLBACK (serv, X_("/strip/receives"), "i", route_get_receives);
		REGISTER_CALLBACK (serv, X_("/strip/plugin/list"), "i", route_plugin_list);
		REGISTER_CALLBACK (serv, X_("/strip/plugin/descriptor"), "ii", route_plugin_descriptor);
		REGISTER_CALLBACK (serv, X_("/strip/plugin/reset"), "ii", route_plugin_reset);

		/* this is a special catchall handler,
		 * register at the end so this is only called if no
		 * other handler matches (also used for debug) */
		lo_server_add_method (serv, 0, 0, _catchall, this);
	}
}

bool
OSC::osc_input_handler (IOCondition ioc, lo_server srv)
{
	if (ioc & IO_IN) {
		lo_server_recv (srv);
	}

	if (ioc & ~(IO_IN|IO_PRI)) {
		return false;
	}

	return true;
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

void
OSC::gui_changed ()
{
	session->set_dirty();
}

void
OSC::current_value_query (const char* path, size_t len, lo_arg **argv, int argc, lo_message msg)
{
	char* subpath;

	subpath = (char*) malloc (len-15+1);
	memcpy (subpath, path, len-15);
	subpath[len-15] = '\0';

	send_current_value (subpath, argv, argc, msg);

	free (subpath);
}

void
OSC::send_current_value (const char* path, lo_arg** argv, int argc, lo_message msg)
{
	if (!session) {
		return;
	}

	lo_message reply = lo_message_new ();
	boost::shared_ptr<Route> r;
	int id;

	lo_message_add_string (reply, path);

	if (argc == 0) {
		lo_message_add_string (reply, "bad syntax");
	} else {
		id = argv[0]->i;
		r = session->get_remote_nth_route (id);

		if (!r) {
			lo_message_add_string (reply, "not found");
		} else {

			if (strcmp (path, X_("/strip/state")) == 0) {

				if (boost::dynamic_pointer_cast<AudioTrack>(r)) {
					lo_message_add_string (reply, "AT");
				} else if (boost::dynamic_pointer_cast<MidiTrack>(r)) {
					lo_message_add_string (reply, "MT");
				} else {
					lo_message_add_string (reply, "B");
				}

				lo_message_add_string (reply, r->name().c_str());
				lo_message_add_int32 (reply, r->n_inputs().n_audio());
				lo_message_add_int32 (reply, r->n_outputs().n_audio());
				lo_message_add_int32 (reply, r->muted());
				lo_message_add_int32 (reply, r->soloed());

			} else if (strcmp (path, X_("/strip/mute")) == 0) {

				lo_message_add_int32 (reply, (float) r->muted());

			} else if (strcmp (path, X_("/strip/solo")) == 0) {

				lo_message_add_int32 (reply, r->soloed());
			}
		}
	}
	OSCSurface *sur = get_surface(get_address (msg));

	if (sur->feedback[14]) {
		lo_send_message (get_address (msg), X_("/reply"), reply);
	} else {
		lo_send_message (get_address (msg), X_("#reply"), reply);
	}
	lo_message_free (reply);
}

int
OSC::_catchall (const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
	return ((OSC*)user_data)->catchall (path, types, argv, argc, msg);
}

int
OSC::catchall (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg)
{
	size_t len;
	int ret = 1; /* unhandled */

	len = strlen (path);
	OSCSurface *sur = get_surface(get_address (msg), true);
	LinkSet *set;
	uint32_t ls = sur->linkset;

	if (ls) {
		set = &(link_sets[ls]);
		sur->custom_mode = set->custom_mode;
		sur->custom_strips = set->custom_strips;
		sur->temp_mode = set->temp_mode;
		sur->temp_strips = set->temp_strips;
		sur->temp_master = set->temp_master;
	}

	if (strstr (path, X_("/automation"))) {
		ret = set_automation (path, types, argv, argc, msg);

	} else
	if (strstr (path, X_("/touch"))) {
		ret = touch_detect (path, types, argv, argc, msg);

	} else
	if (strstr (path, X_("/toggle_roll"))) {
		if (!argc) {
			ret = osc_toggle_roll (false);
		} else {
			if ((types[0] == 'f' && argv[0]->f == 1.0) || (types[0] == 'i' && argv[0]->i == 1)) {
				ret = osc_toggle_roll (true);
			} else if ((types[0] == 'f' && argv[0]->f == 0.0) || (types[0] == 'i' && argv[0]->i == 0)) {
				ret = osc_toggle_roll (false);
			}
		}
	} else
	if (strstr (path, X_("/spill"))) {
		ret = spill (path, types, argv, argc, msg);

	} else
	if (len >= 17 && !strcmp (&path[len-15], X_("/#current_value"))) {
		current_value_query (path, len, argv, argc, msg);
		ret = 0;

	} else
	if (!strncmp (path, X_("/cue/"), 5)) {

		ret = cue_parse (path, types, argv, argc, msg);

	} else
	if (!strncmp (path, X_("/select/plugin/parameter"), 24)) {

		ret = select_plugin_parameter (path, types, argv, argc, msg);

	} else
	if (!strncmp (path, X_("/access_action/"), 15)) {
		check_surface (msg);
		if (!(argc && !argv[0]->i)) {
			std::string action_path = path;

			access_action (action_path.substr(15));
		}

		ret = 0;
	} else
	if (strcmp (path, X_("/strip/listen")) == 0) {
		if (argc <= 0) {
			PBD::warning << "OSC: Wrong number of parameters." << endmsg;
		} else if (sur->custom_mode && !sur->temp_mode) {
			PBD::warning << "OSC: Can't add strips with custom enabled." << endmsg;
		} else {
			for (int n = 0; n < argc; ++n) {
				boost::shared_ptr<Stripable> s = boost::shared_ptr<Stripable>();
				if (types[n] == 'f') {
					s = get_strip ((uint32_t) argv[n]->f, get_address (msg));
				} else if (types[n] == 'i') {
					s = get_strip (argv[n]->i, get_address (msg));
				}
				if (s) {
					sur->custom_strips.push_back (s);
				}
			}
			if (ls) {
				set->custom_strips = sur->custom_strips;
			}
		}
		ret = 0;
	} else
	if (strcmp (path, X_("/strip/ignore")) == 0) {
		if (argc <= 0) {
			PBD::warning << "OSC: Wrong number of parameters." << endmsg;
		} else if (!sur->custom_mode || sur->temp_mode) {
			PBD::warning << "OSC: Can't remove strips without custom enabled." << endmsg;
		} else {
			for (int n = 0; n < argc; ++n) {
				uint32_t st_no = 0;
				if (types[n] == 'f') {
					st_no = (uint32_t) argv[n]->f;
				} else if (types[n] == 'i') {
					st_no = (uint32_t) argv[n]->i;
				}
				if (st_no && st_no <= sur->custom_strips.size ()) {
					sur->custom_strips[argv[n]->i - 1] = boost::shared_ptr<Stripable>();
				}
			}
			if (ls) {
				set->custom_strips = sur->custom_strips;
			}
			ret = set_bank (sur->bank, msg);
		}

		ret = 0;
	}
	else if (!strncmp (path, X_("/set_surface"), 12)) {
		ret = surface_parse (path, types, argv, argc, msg);
	}
	else if (strstr (path, X_("/strip"))) {
		ret = strip_parse (path, types, argv, argc, msg);
	}
	else if (strstr (path, X_("/master"))) {
		ret = master_parse (path, types, argv, argc, msg);
	}
	else if (strstr (path, X_("/monitor"))) {
		ret = monitor_parse (path, types, argv, argc, msg);
	}
	else if (strstr (path, X_("/select"))) {
		ret = select_parse (path, types, argv, argc, msg);
	}
	else if (!strncmp (path, X_("/marker"), 7)) {
		ret = set_marker (types, argv, argc, msg);
	}
	else if (strstr (path, X_("/link"))) {
		ret = parse_link (path, types, argv, argc, msg);
	}
	if (ret) {
		check_surface (msg);
	}

	if ((ret && _debugmode != Off)) {
		debugmsg (_("Unhandled OSC message"), path, types, argv, argc);
	} else if (!ret && _debugmode == All) {
		debugmsg (_("OSC"), path, types, argv, argc);
	}

	return ret;
}

void
OSC::debugmsg (const char *prefix, const char *path, const char* types, lo_arg **argv, int argc)
{
	std::stringstream ss;
	for (int i = 0; i < argc; ++i) {
		lo_type type = (lo_type)types[i];
			ss << " ";
		switch (type) {
			case LO_INT32:
				ss << "i:" << argv[i]->i;
				break;
			case LO_FLOAT:
				ss << "f:" << argv[i]->f;
				break;
			case LO_DOUBLE:
				ss << "d:" << argv[i]->d;
				break;
			case LO_STRING:
				ss << "s:" << &argv[i]->s;
				break;
			case LO_INT64:
				ss << "h:" << argv[i]->h;
				break;
			case LO_CHAR:
				ss << "c:" << argv[i]->s;
				break;
			case LO_TIMETAG:
				ss << "<Timetag>";
				break;
			case LO_BLOB:
				ss << "<BLOB>";
				break;
			case LO_TRUE:
				ss << "#T";
				break;
			case LO_FALSE:
				ss << "#F";
				break;
			case LO_NIL:
				ss << "NIL";
				break;
			case LO_INFINITUM:
				ss << "#inf";
				break;
			case LO_MIDI:
				ss << "<MIDI>";
				break;
			case LO_SYMBOL:
				ss << "<SYMBOL>";
				break;
			default:
				ss << "< ?? >";
				break;
		}
	}
	PBD::info << prefix << ": " << path << ss.str() << endmsg;
}

// "Application Hook" Handlers //
void
OSC::session_loaded (Session& s)
{
//	lo_address listener = lo_address_new (NULL, "7770");
//	lo_send (listener, "/session/loaded", "ss", s.path().c_str(), s.name().c_str());
}

void
OSC::session_exported (std::string path, std::string name)
{
	lo_address listener = lo_address_new (NULL, "7770");
	lo_send (listener, X_("/session/exported"), "ss", path.c_str(), name.c_str());
	lo_address_free (listener);
}

// end "Application Hook" Handlers //

/* path callbacks */

int
OSC::current_value (const char */*path*/, const char */*types*/, lo_arg **/*argv*/, int /*argc*/, lo_message /*msg*/, void* /*user_data*/)
{
#if 0
	const char* returl;

	if (argc < 3 || types == 0 || strlen (types) < 3 || types[0] != 's' || types[1] != 's' || types[2] != s) {
		return 1;
	}

	const char *returl = argv[1]->s;
	lo_address addr = find_or_cache_addr (returl);

	const char *retpath = argv[2]->s;
	/** this call back looks wrong. It appears to send the same information for all queries */


	if (strcmp (argv[0]->s, X_("transport_frame")) == 0) {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_sample());
		}

	} else if (strcmp (argv[0]->s, X_("transport_speed")) == 0) {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_sample());
		}

	} else if (strcmp (argv[0]->s, X_("transport_locked")) == 0) {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_sample());
		}

	} else if (strcmp (argv[0]->s, X_("punch_in")) == 0) {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_sample());
		}

	} else if (strcmp (argv[0]->s, X_("punch_out")) == 0) {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_sample());
		}

	} else if (strcmp (argv[0]->s, X_("rec_enable")) == 0) {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_sample());
		}

	} else {

		/* error */
	}
#endif
	return 0;
}

void
OSC::routes_list (lo_message msg)
{
	if (!session) {
		return;
	}
	OSCSurface *sur = get_surface(get_address (msg), true);

	for (int n = 0; n < (int) sur->nstrips; ++n) {

		boost::shared_ptr<Stripable> s = get_strip (n + 1, get_address (msg));

		if (s) {
			// some things need the route
			boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (s);

			lo_message reply = lo_message_new ();

			if (boost::dynamic_pointer_cast<AudioTrack>(s)) {
				lo_message_add_string (reply, "AT");
			} else if (boost::dynamic_pointer_cast<MidiTrack>(s)) {
				lo_message_add_string (reply, "MT");
			} else if (boost::dynamic_pointer_cast<VCA>(s)) {
				lo_message_add_string (reply, "V");
			} else if (s->is_master()) {
				lo_message_add_string (reply, "MA");
			} else if (s->is_monitor()) {
				lo_message_add_string (reply, "MO");
			} else if (boost::dynamic_pointer_cast<Route>(s) && !boost::dynamic_pointer_cast<Track>(s)) {
				if (!(s->presentation_info().flags() & PresentationInfo::MidiBus)) {
					if (s->is_foldbackbus()) {
						lo_message_add_string (reply, "FB");
					} else {
						lo_message_add_string (reply, "B");
					}
				} else {
					lo_message_add_string (reply, "MB");
				}
			}

			lo_message_add_string (reply, s->name().c_str());
			if (r) {
				// routes have inputs and outputs
				lo_message_add_int32 (reply, r->n_inputs().n_audio());
				lo_message_add_int32 (reply, r->n_outputs().n_audio());
			} else {
				// non-routes like VCAs don't
				lo_message_add_int32 (reply, 0);
				lo_message_add_int32 (reply, 0);
			}
			if (s->mute_control()) {
				lo_message_add_int32 (reply, s->mute_control()->get_value());
			} else {
				lo_message_add_int32 (reply, 0);
			}
			if (s->solo_control()) {
				lo_message_add_int32 (reply, s->solo_control()->get_value());
			} else {
				lo_message_add_int32 (reply, 0);
			}
			lo_message_add_int32 (reply, n + 1);
			if (s->rec_enable_control()) {
				lo_message_add_int32 (reply, s->rec_enable_control()->get_value());
			}
			if (sur->feedback[14]) {
				lo_send_message (get_address (msg), X_("/reply"), reply);
			} else {
				lo_send_message (get_address (msg), X_("#reply"), reply);
			}
			lo_message_free (reply);
		}
	}

	// Send end of listing message
	lo_message reply = lo_message_new ();

	lo_message_add_string (reply, X_("end_route_list"));
	lo_message_add_int64 (reply, session->sample_rate());
	lo_message_add_int64 (reply, session->current_end_sample());
	if (session->monitor_out()) {
		// this session has a monitor section
		lo_message_add_int32 (reply, 1);
	} else {
		lo_message_add_int32 (reply, 0);
	}

	if (sur->feedback[14]) {
		lo_send_message (get_address (msg), X_("/reply"), reply);
	} else {
		lo_send_message (get_address (msg), X_("#reply"), reply);
	}

	lo_message_free (reply);
	// send feedback for newly created control surface
	strip_feedback (sur, true);
	global_feedback (sur);
	_strip_select (boost::shared_ptr<ARDOUR::Stripable>(), get_address (msg));

}

void
OSC::surface_list (lo_message msg)
{
		get_surfaces ();
}

void
OSC::get_surfaces ()
{

	/* this function is for debugging and prints lots of
	 * information about what surfaces Ardour knows about and their
	 * internal parameters. It is best accessed by sending:
	 * /surface/list from oscsend. This command does not create
	 * a surface entry.
	 */

	PBD::info << string_compose ("\nList of known Surfaces (%1):\n", _surface.size());

	Glib::Threads::Mutex::Lock lm (surfaces_lock);
	for (uint32_t it = 0; it < _surface.size(); it++) {
		OSCSurface* sur = &_surface[it];
		char *chost = lo_url_get_hostname (sur->remote_url.c_str());
		string host = chost;
		free (chost);
		string port = get_port (host);
		if (port != "auto") {
			port = "Manual port";
		} else {
			port = "Auto port";
		}
		PBD::info << string_compose ("\n  Surface: %1 - URL: %2  %3\n", it, sur->remote_url, port);
		PBD::info << string_compose ("	Number of strips: %1   Bank size: %2   Current Bank %3\n", sur->nstrips, sur->bank_size, sur->bank);
		PBD::info << string_compose ("	Use Custom: %1   Custom Strips: %2\n", sur->custom_mode, sur->custom_strips.size ());
		PBD::info << string_compose ("	Temp Mode: %1   Temp Strips: %2\n", sur->temp_mode, sur->temp_strips.size ());
		bool ug = false;
		if (sur->usegroup == PBD::Controllable::UseGroup) {
			ug = true;
		}
		PBD::info << string_compose ("	Strip Types: %1   Feedback: %2   No_clear flag: %3   Gain mode: %4   Use groups flag %5\n", \
			sur->strip_types.to_ulong(), sur->feedback.to_ulong(), sur->no_clear, sur->gainmode, ug);
		PBD::info << string_compose ("	Using plugin: %1  of  %2 plugins, with %3 params.  Page size: %4  Page: %5\n", \
			sur->plugin_id, sur->plugins.size(), sur->plug_params.size(), sur->plug_page_size, sur->plug_page);
		PBD::info << string_compose ("	Send page size: %1  Page: %2\n", sur->send_page_size, sur->send_page);
		PBD::info << string_compose ("	Expanded flag %1   Track: %2   Jogmode: %3\n", sur->expand_enable, sur->expand, sur->jogmode);
		PBD::info << string_compose ("	Personal monitor flag %1,   Aux master: %2,   Number of sends: %3\n", sur->cue, sur->aux, sur->sends.size());
		PBD::info << string_compose ("	Linkset: %1   Device Id: %2\n", sur->linkset, sur->linkid);
	}
	PBD::info << string_compose ("\nList of LinkSets (%1):\n", link_sets.size());
	std::map<uint32_t, LinkSet>::iterator it;
	for (it = link_sets.begin(); it != link_sets.end(); it++) {
		if (!(*it).first) {
			continue;
		}
		uint32_t devices = 0;
		LinkSet* set = &(*it).second;
		if (set->urls.size()) {
			devices = set->urls.size() - 1;
		}
		PBD::info << string_compose ("\n  Linkset %1 has %2 devices and sees %3 strips\n", (*it).first, devices, set->strips.size());
		PBD::info << string_compose ("	Bank size: %1   Current bank: %2   Strip Types: %3\n", set->banksize, set->bank, set->strip_types.to_ulong());
		PBD::info << string_compose ("	Auto bank sizing: %1 Linkset not ready flag: %2\n", set->autobank, set->not_ready);
		PBD::info << string_compose ("	Use Custom: %1 Number of Custom Strips: %2\n", set->custom_mode, set->custom_strips.size ());
		PBD::info << string_compose ("	Temp Mode: %1 Number of Temp Strips: %2\n", set->temp_mode, set->temp_strips.size ());
	}
	PBD::info << endmsg;
}

int
OSC::custom_clear (lo_message msg)
{
	if (!session) {
		return 0;
	}
	OSCSurface *sur = get_surface(get_address (msg), true);
	sur->custom_mode = 0;
	sur->custom_strips.clear ();
	sur->strips = get_sorted_stripables(sur->strip_types, sur->cue, false, sur->custom_strips);
	sur->nstrips = sur->strips.size();
	LinkSet *set;
	uint32_t ls = sur->linkset;
	if (ls) {
		set = &(link_sets[ls]);
		set->custom_mode = 0;
		set->custom_strips.clear ();
		set->strips = sur->strips;
	}
	return set_bank (1, msg);
}

int
OSC::custom_mode (float state, lo_message msg)
{
	return _custom_mode ((uint32_t) state, get_address (msg));
}

int
OSC::_custom_mode (uint32_t state, lo_address addr)
{
	if (!session) {
		return 0;
	}
	OSCSurface *sur = get_surface(addr, true);
	LinkSet *set;
	uint32_t ls = sur->linkset;

	if (ls) {
		set = &(link_sets[ls]);
		sur->custom_mode = set->custom_mode;
		sur->custom_strips = set->custom_strips;
	}
	sur->temp_mode = TempOff;
	if (state > 0){
		if (sur->custom_strips.size () == 0) {
			PBD::warning << "No custom strips set to enable" << endmsg;
			sur->custom_mode = 0;
			if (ls) {
				set->custom_mode = 0;
			}
			return -1;
		} else {
			if (sur->bank_size) {
				sur->custom_mode = state | 0x4;
			} else {
				sur->custom_mode = state;
			}
			sur->strips = get_sorted_stripables(sur->strip_types, sur->cue, sur->custom_mode, sur->custom_strips);
			sur->nstrips = sur->custom_strips.size();
		}
	} else {
		sur->custom_mode = 0;
		sur->strips = get_sorted_stripables(sur->strip_types, sur->cue, 0, sur->custom_strips);
		sur->nstrips = sur->strips.size();
	}
	if (ls) {
		set->custom_mode = sur->custom_mode;
		set->strips = sur->strips;
		set->temp_mode = sur->temp_mode;
	}
	return _set_bank (1, addr);
}

int
OSC::cancel_all_solos ()
{
	session->cancel_all_solo ();
	return 0;
}

int
OSC::osc_toggle_roll (bool ret2strt)
{
	if (!session) {
		return 0;
	}

	if (session->is_auditioning()) {
		session->cancel_audition ();
		return 0;
	}

	bool rolling = transport_rolling();

	if (rolling) {
		session->request_stop (ret2strt, true);
	} else {

		if (session->get_play_loop() && Config->get_loop_is_mode()) {
			session->request_locate (session->locations()->auto_loop_location()->start().samples(), MustRoll);
		} else {
			session->request_roll (TRS_UI);
		}
	}
	return 0;
}

lo_address
OSC::get_address (lo_message msg)
{
	lo_address addr = lo_message_get_source (msg);
	string host = lo_address_get_hostname (addr);
	string port = lo_address_get_port (addr);
	int protocol = lo_address_get_protocol (addr);
	string saved_port = get_port (host);
	if (saved_port != "") {
		if (saved_port != "auto") {
			port = saved_port;
			return lo_address_new_with_proto (protocol, host.c_str(), port.c_str());
		} else {
			return lo_message_get_source (msg);
		}
	}

	// if we get here we need to add a new entry for this surface
	PortAdd new_port;
	new_port.host = host;
	if (address_only) {
		new_port.port = remote_port;
		_ports.push_back (new_port);
		return lo_address_new_with_proto (protocol, host.c_str(), remote_port.c_str());
	} else {
		new_port.port = "auto";
		_ports.push_back (new_port);
		return lo_message_get_source (msg);
	}
}

string
OSC::get_port (string host)
{
	for (uint32_t i = 0; i < _ports.size (); i++) {
		if (_ports[i].host == host) {
			return _ports[i].port;
		}
	}
	return "";
}

int
OSC::refresh_surface (lo_message msg)
{
	OSCSurface *s = get_surface(get_address (msg), true);
	uint32_t bs = s->bank_size;
	uint32_t st = (uint32_t) s->strip_types.to_ulong();
	uint32_t fb = (uint32_t) s->feedback.to_ulong();
	uint32_t gm = (uint32_t) s->gainmode;
	uint32_t sp = s->send_page_size;
	uint32_t pp = s->plug_page_size;

	surface_destroy (s);
	// restart all observers
	set_surface (bs, st, fb, gm, sp, pp, msg);
	return 0;
}

void
OSC::clear_devices ()
{
	tick = false;
	observer_busy = true;
	session_connections.drop_connections ();
	// clear out surfaces
	for (uint32_t it = 0; it < _surface.size(); ++it) {
		OSCSurface* sur = &_surface[it];
		surface_destroy (sur);
	}
	_surface.clear();
	link_sets.clear ();
	_ports.clear ();

	PresentationInfo::Change.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&OSC::recalcbanks, this), this);

	observer_busy = false;
	tick = true;
}

int
OSC::parse_link (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg)
{
	int ret = 1; /* unhandled */
	int set = 0;
	if (!argc) {
		PBD::warning << "OSC: /link/* needs at least one parameter" << endmsg;
		return ret;
	}
	float data = 0;
	if (types[argc - 1] == 'f') {
		data = argv[argc - 1]->f;
	} else {
		data = argv[argc - 1]->i;
	}
	if (isdigit(strrchr (path, '/')[1])) {
		set = atoi (&(strrchr (path, '/')[1]));
	} else if (argc == 2) {
		if (types[0] == 'f') {
			set = (int) argv[0]->f;
		} else {
			set = argv[0]->i;
		}
	} else {
		PBD::warning << "OSC: wrong number of parameters." << endmsg;
		return ret;
	}
	LinkSet *ls = get_linkset (set, get_address (msg));

	if (!set) {
		return 0;
	}
	if (!strncmp (path, X_("/link/bank_size"), 15)) {
		ls->banksize = (uint32_t) data;
		ls->autobank = false;
		ls->not_ready = link_check (set);
		if (ls->not_ready) {
			ls->bank = 1;
			surface_link_state (ls);
		} else {
			_set_bank (ls->bank, get_address (msg));
		}
		ret = 0;

	} else if (!strncmp (path, X_("/link/set"), 9)) {
		ret = set_link (set, (uint32_t) data, get_address (msg));
	}

	return ret;
}

OSC::LinkSet *
OSC::get_linkset (uint32_t set, lo_address addr)
{
	OSCSurface *sur = get_surface(addr);
	LinkSet *ls = 0;

	if (set) {
		// need to check if set is wanted
		std::map<uint32_t, LinkSet>::iterator it;
		it = link_sets.find(set);
		if (it == link_sets.end()) {
			// no such linkset make it
			LinkSet new_ls;
			new_ls.banksize = 0;
			new_ls.bank = 1;
			new_ls.autobank = true;
			new_ls.not_ready = true;
			new_ls.strip_types = sur->strip_types;
			new_ls.strips = sur->strips;
			new_ls.custom_strips = sur->custom_strips;
			new_ls.custom_mode = sur->custom_mode;
			new_ls.temp_mode = sur->temp_mode;
			new_ls.urls.resize (2);
			link_sets[set] = new_ls;
		}
		ls = &link_sets[set];

	} else {
		// User expects this surface to be removed from any sets
		uint32_t oldset = sur->linkset;
		if (oldset) {
			uint32_t oldid = sur->linkid;
			sur->linkid = 1;
			sur->linkset = 0;
			LinkSet *ols = &link_sets[oldset];
			if (ols) {
				ols->not_ready = oldid;
				ols->urls[oldid] = "";
				surface_link_state (ols);
			}
		}
	}
	return ls;
}

int
OSC::set_link (uint32_t set, uint32_t id, lo_address addr)
{
	OSCSurface *sur = get_surface(addr, true);
	sur->linkset = set;
	sur->linkid = id;
	LinkSet *ls = get_linkset (set, addr);
	if (ls->urls.size() <= (uint32_t) id) {
		ls->urls.resize ((int) id + 1);
	}
	ls->urls[(uint32_t) id] = sur->remote_url;
	ls->not_ready = link_check (set);
	if (ls->not_ready) {
		surface_link_state (ls);
	} else {
		_set_bank (1, addr);
	}
	return 0;
}

void
OSC::link_strip_types (uint32_t linkset, uint32_t striptypes)
{
	LinkSet *ls = 0;

	if (!linkset) {
		return;
	}
	std::map<uint32_t, LinkSet>::iterator it;
	it = link_sets.find(linkset);
	if (it == link_sets.end()) {
		// this should never happen... but
		return;
	}
	ls = &link_sets[linkset];
	ls->strip_types = striptypes;
	ls->temp_mode = TempOff;
	for (uint32_t dv = 1; dv < ls->urls.size(); dv++) {
		OSCSurface *su;

		if (ls->urls[dv] != "") {
			string url = ls->urls[dv];
			su = get_surface (lo_address_new_from_url (url.c_str()), true);
			if (su->linkset == linkset) {
				su->strip_types = striptypes;
				if (su->strip_types[10]) {
					su->usegroup = PBD::Controllable::UseGroup;
				} else {
					su->usegroup = PBD::Controllable::NoGroup;
				}
			} else {
				ls->urls[dv] = "";
			}
		}
	}
}

void
OSC::surface_link_state (LinkSet * set)
{
	for (uint32_t dv = 1; dv < set->urls.size(); dv++) {

		if (set->urls[dv] != "") {
			string url = set->urls[dv];
			OSCSurface *sur = get_surface (lo_address_new_from_url (url.c_str()), true);
			for (uint32_t i = 0; i < sur->observers.size(); i++) {
				sur->observers[i]->set_link_ready (set->not_ready);
			}
		}
	}
}

int
OSC::link_check (uint32_t set)
{
	LinkSet *ls = 0;

	if (!set) {
		return 1;
	}
	std::map<uint32_t, LinkSet>::iterator it;
	it = link_sets.find(set);
	if (it == link_sets.end()) {
		// this should never happen... but
		return 1;
	}
	ls = &link_sets[set];
	uint32_t bank_total = 0;
	for (uint32_t dv = 1; dv < ls->urls.size(); dv++) {
		OSCSurface *su;

		if (ls->urls[dv] != "") {
			string url = ls->urls[dv];
			su = get_surface (lo_address_new_from_url (url.c_str()), true);
		} else {
			return dv;
		}
		if (su->linkset == set) {
			bank_total = bank_total + su->bank_size;
		} else {
			ls->urls[dv] = "";
			return dv;
		}
		if (ls->autobank) {
			ls->banksize = bank_total;
		} else {
			if (bank_total != ls->banksize) {
				return ls->urls.size();
			}
		}
	}
	return 0;
}

int
OSC::surface_parse (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg)
{
	int ret = 1; /* unhandled */
	OSCSurface *sur = get_surface(get_address (msg), true);
	int pi_page = sur->plug_page_size;
	int se_page = sur->send_page_size;
	int fadermode = sur->gainmode;
	int feedback = sur->feedback.to_ulong();
	int strip_types = sur->strip_types.to_ulong();
	int bank_size = sur->bank_size;
	int linkset = sur->linkset;
	int linkid = sur->linkid;
	string host = lo_url_get_hostname(sur->remote_url.c_str());
	int port = atoi (get_port (host).c_str());
	int data = 0;

	if (argc) {
		if (types[0] == 'f') {
			data = (int)argv[0]->f;
		} else if (types[0] == 'i') {
			data = argv[0]->i;
		} else if (types[0] == 's') {
			if (isdigit(argv[0]->s)) {
				data = atoi (&(argv[0]->s));
			} else {
				PBD::warning << "OSC: Parameter is not numerical." << endmsg;
				return 1;
			}
		} else {
			PBD::warning << "OSC: Wrong parameter type." << endmsg;
			return 1;
		}
	}

	if (argc == 1 && !strncmp (path, X_("/set_surface/feedback"), 21)) {
		ret = set_surface_feedback (data, msg);
	}
	else if (argc == 1 && !strncmp (path, X_("/set_surface/bank_size"), 22)) {
		ret = set_surface_bank_size (data, msg);
	}
	else if (argc == 1 && !strncmp (path, X_("/set_surface/gainmode"), 21)) {
		ret = set_surface_gainmode (data, msg);
	}
	else if (argc == 1 && !strncmp (path, X_("/set_surface/strip_types"), 24)) {
		ret = set_surface_strip_types (data, msg);
	}
	else if (argc == 1 && !strncmp (path, X_("/set_surface/send_page_size"), 27)) {
		ret = sel_send_pagesize (data, msg);
	}
	else if (argc == 1 && !strncmp (path, X_("/set_surface/plugin_page_size"), 29)) {
		ret = sel_plug_pagesize (data, msg);
	}
	else if (argc == 1 && !strncmp (path, X_("/set_surface/port"), 17)) {
		ret = set_surface_port (data, msg);
	} else if (strlen(path) == 12) {

		// command is in /set_surface iii form
		switch (argc) {
			case 9:
				if (types[8] == 'f') {
					linkid = (int) argv[8]->f;
				} else {
					linkid = argv[8]->i;
				}
				/* fallthrough */
			case 8:
				if (types[7] == 'f') {
					linkset = (int) argv[7]->f;
				} else {
					linkset = argv[7]->i;
				}
				/* fallthrough */
			case 7:
				if (types[6] == 'f') {
					port = (int) argv[6]->f;
				} else {
					port = argv[6]->i;
				}
				/* fallthrough */
			case 6:
				if (types[5] == 'f') {
					pi_page = (int) argv[5]->f;
				} else {
					pi_page = argv[5]->i;
				}
				/* fallthrough */
			case 5:
				if (types[4] == 'f') {
					se_page = (int) argv[4]->f;
				} else {
					se_page = argv[4]->i;
				}
				/* fallthrough */
			case 4:
				if (types[3] == 'f') {
					fadermode = (int) argv[3]->f;
				} else {
					fadermode = argv[3]->i;
				}
				/* fallthrough */
			case 3:
				if (types[2] == 'f') {
					feedback = (int) argv[2]->f;
				} else {
					feedback = argv[2]->i;
				}
				/* fallthrough */
			case 2:
				if (types[1] == 'f') {
					strip_types = (int) argv[1]->f;
				} else {
					strip_types = argv[1]->i;
				}
				/* fallthrough */
			case 1:
				bank_size = data;
				set_surface_port (port, msg);
				ret = set_surface (bank_size, strip_types, feedback, fadermode, se_page, pi_page, msg);
				if ((uint32_t) linkset != sur->linkset) {
					set_link (linkset, linkid, get_address (msg));
				}
				break;
			case 0:
				// send current setup
				{
					lo_message reply = lo_message_new ();
					lo_message_add_int32 (reply, bank_size);
					lo_message_add_int32 (reply, strip_types);
					lo_message_add_int32 (reply, feedback);
					lo_message_add_int32 (reply, fadermode);
					lo_message_add_int32 (reply, se_page);
					lo_message_add_int32 (reply, pi_page);
					lo_message_add_int32 (reply, (int) linkset);
					lo_message_add_int32 (reply, (int) linkid);
					lo_message_add_int32 (reply, (int) port);
					lo_send_message (get_address (msg), X_("/set_surface"), reply);
					lo_message_free (reply);
					return 0;
				}
				break;

			default:
				PBD::warning << "OSC: Too many parameters." << endmsg;
				return 1;
				break;
		}
	} else if (isdigit(path[13])) {
		// some of our parameters must be "in-lined"
		bank_size = atoi (&path[13]);
		const char * par = strstr (&path[13], "/");
		if (par) {
			strip_types = atoi (&par[1]);
			const char * fb = strstr (&par[1], "/");
			if (fb) {
				feedback = atoi (&fb[1]);
				const char * fm = strstr (&fb[1], "/");
				if (fm) {
					fadermode = atoi (&fm[1]);
					const char * sp = strstr (&fm[1], "/");
					if (sp) {
						se_page = atoi (&sp[1]);
						const char * pp = strstr (&sp[1], "/");
						if (pp) {
							pi_page = atoi (&pp[1]);
							const char * po = strstr (&pp[1], "/");
							if (po) {
								port = atoi (&po[1]);
								const char * ls = strstr (&po[1], "/");
								if (ls) {
									linkset = atoi (&ls[1]);
									const char * li = strstr (&ls[1], "/");
									if (li) {
										linkid = atoi (&li[1]);
									} else {
										if (types[0] == 'f') {
											linkid = (int) argv[0]->f;
										} else if (types[0] == 'i') {
											linkid = argv[0]->i;
										}
									}
								} else {
									if (types[0] == 'f') {
										linkset = (int) argv[0]->f;
									} else if (types[0] == 'i') {
										linkset = argv[0]->i;
									}
								}
							} else {
								if (types[0] == 'f') {
									port = (int) argv[0]->f;
								} else if (types[0] == 'i') {
									port = argv[0]->i;
								}
							}
						} else {
							if (types[0] == 'f') {
								pi_page = (int) argv[0]->f;
							} else if (types[0] == 'i') {
								pi_page = argv[0]->i;
							}
						}
					} else {
						if (types[0] == 'f') {
							se_page = (int) argv[0]->f;
						} else if (types[0] == 'i') {
							se_page = argv[0]->i;
						}
					}
				} else {
					if (types[0] == 'f') {
						fadermode = (int) argv[0]->f;
					} else if (types[0] == 'i') {
						fadermode = argv[0]->i;
					}
				}
			} else {
				if (types[0] == 'f') {
					feedback = (int) argv[0]->f;
				} else if (types[0] == 'i') {
					feedback = argv[0]->i;
				}
			}
		} else {
			if (types[0] == 'f') {
				strip_types = (int) argv[0]->f;
			} else if (types[0] == 'i') {
				strip_types = argv[0]->i;
			}
		}
		set_surface_port (port, msg);
		ret = set_surface (bank_size, strip_types, feedback, fadermode, se_page, pi_page, msg);
		if ((uint32_t) linkset != sur->linkset) {
			set_link (linkset, linkid, get_address (msg));
		}
	}
	return ret;
}

int
OSC::set_surface (uint32_t b_size, uint32_t strips, uint32_t fb, uint32_t gm, uint32_t se_size, uint32_t pi_size, lo_message msg)
{
	if (observer_busy) {
		return -1;
	}
	OSCSurface *s = get_surface(get_address (msg), true);
	s->bank_size = b_size;
	s->strip_types = strips;
	s->feedback = fb;
	s->gainmode = gm;
	if (s->strip_types[10]) {
		s->usegroup = PBD::Controllable::UseGroup;
	} else {
		s->usegroup = PBD::Controllable::NoGroup;
	}
	s->send_page_size = se_size;
	s->plug_page_size = pi_size;
	if (s->temp_mode) {
		s->temp_mode = TempOff;
	}
	if (s->linkset) {
		set_link (s->linkset, s->linkid, get_address (msg));
		link_strip_types (s->linkset, s->strip_types.to_ulong());
	} else {
		// set bank and strip feedback
		strip_feedback(s, true);
		_set_bank (1, get_address (msg));
		_strip_select (boost::shared_ptr<Stripable> (), get_address (msg));
	}

	global_feedback (s);
	sel_send_pagesize (se_size, msg);
	sel_plug_pagesize (pi_size, msg);
	return 0;
}

int
OSC::set_surface_bank_size (uint32_t bs, lo_message msg)
{
	if (observer_busy) {
		return -1;
	}
	OSCSurface *s = get_surface(get_address (msg), true);
	s->bank_size = bs;
	if (s->custom_mode && bs) {
		s->custom_mode = s->custom_mode | 0x4;
	}
	if (s->linkset) {
		set_link (s->linkset, s->linkid, get_address (msg));
	} else {
		// set bank and strip feedback
		_set_bank (1, get_address (msg));
	}
	return 0;
}

int
OSC::set_surface_strip_types (uint32_t st, lo_message msg)
{
	if (observer_busy) {
		return -1;
	}
	OSCSurface *s = get_surface(get_address (msg), true);
	s->strip_types = st;
	s->temp_mode = TempOff;
	if (s->strip_types[10]) {
		s->usegroup = PBD::Controllable::UseGroup;
	} else {
		s->usegroup = PBD::Controllable::NoGroup;
	}
	if (s->linkset) {
		link_strip_types (s->linkset, st);
	}
	// set bank and strip feedback
	strip_feedback(s, false);
	set_bank (1, msg);
	_strip_select (boost::shared_ptr<Stripable> (), get_address (msg));
	return 0;
}


int
OSC::set_surface_feedback (uint32_t fb, lo_message msg)
{
	if (observer_busy) {
		return -1;
	}
	OSCSurface *s = get_surface(get_address (msg), true);
	s->feedback = fb;

	strip_feedback (s, true);
	global_feedback (s);
	_strip_select (boost::shared_ptr<ARDOUR::Stripable>(), get_address (msg));
	return 0;
}

int
OSC::set_surface_gainmode (uint32_t gm, lo_message msg)
{
	if (observer_busy) {
		return -1;
	}
	OSCSurface *s = get_surface(get_address (msg), true);
	s->gainmode = gm;

	strip_feedback (s, true);
	global_feedback (s);
	_strip_select (boost::shared_ptr<ARDOUR::Stripable>(), get_address (msg));
	return 0;
}

int
OSC::set_surface_port (uint32_t po, lo_message msg)
{
	string new_port;
	if (!po) {
		new_port = "auto";
	} else if (po > 1024) {
		new_port = string_compose ("%1", po);
	} else {
		PBD::warning << "Port value must be greater than 1024" << endmsg;
		return -1;
	}
	OSCSurface *sur = get_surface(get_address (msg), true);
	lo_address addr = lo_message_get_source (msg);
	string host = lo_address_get_hostname (addr);
	string port = lo_address_get_port (addr);
	int protocol = lo_address_get_protocol (addr);
	for (uint32_t i = 0; i < _ports.size (); i++) {
		if (_ports[i].host == host) {
			if (_ports[i].port == new_port) {
				// no change - do nothing
				return 0;
			} else {
				lo_address new_addr;
				_ports[i].port = new_port;
				if (new_port == "auto") {
					new_addr = addr;
				} else {
					new_addr = lo_address_new_with_proto (protocol, host.c_str(), new_port.c_str());
				}
				char * rurl;
				rurl = lo_address_get_url (new_addr);
				sur->remote_url = rurl;
				free (rurl);
				for (uint32_t it = 0; it < _surface.size();) {
					if (&_surface[it] == sur) {
						it++;
						continue;
					}
					char *sur_host = lo_url_get_hostname(_surface[it].remote_url.c_str());
					if (strstr (sur_host, host.c_str())) {
						surface_destroy (&_surface[it]);
						_surface.erase (_surface.begin() + it);
					} else {
						it++;
					}
				}
				if (sur->feedback.to_ulong()) {
					refresh_surface (msg);
				}
				return 0;
			}
		}
	}
	// should not get here
	return -1;
}

int
OSC::check_surface (lo_message msg)
{
	if (!session) {
		return -1;
	}
	get_surface (get_address (msg));
	return 0;
}

OSC::OSCSurface *
OSC::get_surface (lo_address addr , bool quiet)
{
	string r_url;
	char * rurl;
	rurl = lo_address_get_url (addr);
	r_url = rurl;
	free (rurl);
	for (uint32_t it = 0; it < _surface.size(); ++it) {
		//find setup for this surface
		if (!_surface[it].remote_url.find(r_url)){
			return &_surface[it];
		}
	}

	// No surface create one with default values
	OSCSurface s;
	s.remote_url = r_url;
	s.no_clear = false;
	s.jogmode = 0;
	s.bank = 1;
	s.bank_size = default_banksize;
	s.observers.clear();
	s.sel_obs = 0;
	s.global_obs = 0;
	s.strip_types = default_strip;
	s.feedback = default_feedback;
	s.gainmode = default_gainmode;
	s.usegroup = PBD::Controllable::NoGroup;
	s.custom_strips.clear ();
	s.custom_mode = 0;
	s.temp_mode = TempOff;
	s.sel_obs = 0;
	s.expand = 0;
	s.expand_enable = false;
	s.expand_strip = boost::shared_ptr<Stripable> ();
	s.cue = false;
	s.aux = 0;
	s.cue_obs = 0;
	s.strips = get_sorted_stripables(s.strip_types, s.cue, false, s.custom_strips);
	s.send_page = 1;
	s.send_page_size = default_send_size;
	s.plug_page = 1;
	s.plug_page_size = default_plugin_size;
	s.plugin_id = 1;
	s.linkset = 0;
	s.linkid = 1;

	s.nstrips = s.strips.size();
	{
		_surface.push_back (s);
	}

	if (!quiet) {
		strip_feedback (&s, true);
		global_feedback (&s);
	}
	_strip_select2 (boost::shared_ptr<ARDOUR::Stripable>(), &_surface[_surface.size() - 1], addr);

	return &_surface[_surface.size() - 1];
}

// setup global feedback for a surface
void
OSC::global_feedback (OSCSurface* sur)
{
	OSCGlobalObserver* o = sur->global_obs;
	if (o) {
		delete o;
		sur->global_obs = 0;
	}
	if (sur->feedback[4] || sur->feedback[3] || sur->feedback[5] || sur->feedback[6]) {

		// create a new Global Observer for this surface
		OSCGlobalObserver* o = new OSCGlobalObserver (*this, *session, sur);
		sur->global_obs = o;
		o->jog_mode (sur->jogmode);
	}
}

void
OSC::strip_feedback (OSCSurface* sur, bool new_bank_size)
{
	LinkSet *set;
	uint32_t ls = sur->linkset;

	if (ls) {
		set = &(link_sets[ls]);
		if (set->not_ready) {
			return;
		}
		sur->custom_mode = set->custom_mode;
		sur->custom_strips = set->custom_strips;
		sur->temp_mode = set->temp_mode;
		sur->temp_strips = set->temp_strips;
		sur->temp_master = set->temp_master;
	}
	if (!sur->temp_mode) {
		sur->strips = get_sorted_stripables(sur->strip_types, sur->cue, sur->custom_mode, sur->custom_strips);
	} else {
		sur->strips = get_sorted_stripables(sur->strip_types, sur->cue, 1, sur->temp_strips);
	}
	uint32_t old_size = sur->nstrips;
	sur->nstrips = sur->strips.size();
	if (old_size != sur->nstrips) {
		new_bank_size = true;
	}

	if (ls) {
		set->strips = sur->strips;
	}

	if (new_bank_size || (!sur->feedback[0] && !sur->feedback[1])) {
		// delete old observers
		for (uint32_t i = 0; i < sur->observers.size(); i++) {
			delete sur->observers[i];
		}
		sur->observers.clear();

		uint32_t bank_size = sur->bank_size;
		if (!bank_size) {
			bank_size = sur->nstrips;
		}

		if (sur->feedback[0] || sur->feedback[1]) {
			for (uint32_t i = 0; i < bank_size; i++) {
				OSCRouteObserver* o = new OSCRouteObserver (*this, i + 1, sur);
				sur->observers.push_back (o);
				if (sur->temp_mode == BusOnly) {
					boost::shared_ptr<ARDOUR::Stripable> str = get_strip (i + 1, lo_address_new_from_url (sur->remote_url.c_str()));
					boost::shared_ptr<ARDOUR::Send> send = get_send (str, lo_address_new_from_url (sur->remote_url.c_str()));
					if (send) {
						o->refresh_send (send, true);
					}
				}

			}
		}
	} else {
		if (sur->feedback[0] || sur->feedback[1]) {
			for (uint32_t i = 0; i < sur->observers.size(); i++) {
				boost::shared_ptr<ARDOUR::Stripable> str = get_strip (i + 1, lo_address_new_from_url (sur->remote_url.c_str()));
				sur->observers[i]->refresh_strip(str, true);
				if (sur->temp_mode == BusOnly) {
					boost::shared_ptr<ARDOUR::Send> send = get_send (str, lo_address_new_from_url (sur->remote_url.c_str()));
					if (send) {
						sur->observers[i]->refresh_send (send, true);
					}
				}
			}
		}
	}
	bank_leds (sur);
}

void
OSC::notify_routes_added (ARDOUR::RouteList &)
{
	// not sure if we need this PI change seems to cover
	//recalcbanks();
}

void
OSC::notify_vca_added (ARDOUR::VCAList &)
{
	// not sure if we need this PI change seems to cover
	//recalcbanks();
}

void
OSC::recalcbanks ()
{
	tick = false;
	bank_dirty = true;
}

void
OSC::_recalcbanks ()
{
	if (observer_busy) {
		return;
	}
	/*
	 * We have two different ways of working here:
	 * 1) banked: The controller has a bank of strips and only can deal
	 * with banksize strips. We start banksize observers which run until
	 * either banksize is changed or Ardour exits.
	 *
	 * 2) banksize is 0 or unlimited and so is the same size as the number
	 * of strips.
	 */

	// refresh each surface we know about.
	for (uint32_t it = 0; it < _surface.size(); ++it) {
		OSCSurface* sur = &_surface[it];
		// find lo_address
		lo_address addr = lo_address_new_from_url (sur->remote_url.c_str());
		if (sur->cue) {
			_cue_set (sur->aux, addr);
		} else if (!sur->bank_size) {
			strip_feedback (sur, false);
			// This surface uses /strip/list tell it routes have changed
			lo_message reply;
			reply = lo_message_new ();
			lo_send_message (addr, X_("/strip/list"), reply);
			lo_message_free (reply);
		} else {
			strip_feedback (sur, false);
		}
		_strip_select (boost::shared_ptr<ARDOUR::Stripable>(), addr);
	}
}

int
OSC::set_bank (uint32_t bank_start, lo_message msg)
{
	return _set_bank (bank_start, get_address (msg));
}

// set bank is callable with either message or address
int
OSC::_set_bank (uint32_t bank_start, lo_address addr)
{
	if (!session) {
		return -1;
	}
	if (!session->nroutes()) {
		return -1;
	}

	OSCSurface *s = get_surface (addr, true);

	Sorted striplist = s->strips;
	uint32_t nstrips = s->nstrips;

	LinkSet *set;
	uint32_t ls = s->linkset;

	if (ls) {
		//we have a linkset... deal with each surface
		set = &(link_sets[ls]);
		if (set->not_ready) {
			return 1;
		}
		uint32_t d_count = set->urls.size();
		set->strips = striplist;
		bank_start = bank_limits_check (bank_start, set->banksize, nstrips);
		set->bank = bank_start;
		uint32_t not_ready = 0;
		for (uint32_t dv = 1; dv < d_count; dv++) {
			if (set->urls[dv] != "") {
				string url = set->urls[dv];
				OSCSurface *sur = get_surface (lo_address_new_from_url (url.c_str()));
				if (sur->linkset != ls) {
					set->urls[dv] = "";
					not_ready = dv;
				} else {
					lo_address sur_addr = lo_address_new_from_url (sur->remote_url.c_str());

					sur->bank = bank_start;
					bank_start = bank_start + sur->bank_size;
					strip_feedback (sur, false);
					_strip_select (boost::shared_ptr<ARDOUR::Stripable>(), sur_addr);
					bank_leds (sur);
					lo_address_free (sur_addr);
				}
			} else {
				not_ready = dv;
			}
			if (not_ready) {
				if (!set->not_ready) {
					set->not_ready = not_ready;
				}
				set->bank = 1;
				break;
			}
		}
		if (not_ready) {
			surface_link_state (set);
		}

	} else {

		s->bank = bank_limits_check (bank_start, s->bank_size, nstrips);
		strip_feedback (s, true);
		_strip_select (boost::shared_ptr<ARDOUR::Stripable>(), addr);
		bank_leds (s);
	}


	bank_dirty = false;
	tick = true;
	return 0;
}

uint32_t
OSC::bank_limits_check (uint32_t bank, uint32_t size, uint32_t total)
{
	uint32_t b_size;
	if (!size) {
		// no banking - bank includes all stripables
		b_size = total;
	} else {
		b_size = size;
	}
	// Do limits checking
	if (bank < 1) bank = 1;
	if (b_size >= total)  {
		bank = 1;
	} else if (bank > ((total - b_size) + 1)) {
		// top bank is always filled if there are enough strips for at least one bank
		bank = (uint32_t)((total - b_size) + 1);
	}
	return bank;
}

void
OSC::bank_leds (OSCSurface* s)
{
	uint32_t bank = 0;
	uint32_t size = 0;
	uint32_t total = 0;
	// light bankup or bankdown buttons if it is possible to bank in that direction
	lo_address addr = lo_address_new_from_url (s->remote_url.c_str());
	if (s->linkset) {
		LinkSet *set;
		set = &(link_sets[s->linkset]);
		bank = set->bank;
		size = set->banksize;
		total = s->nstrips;
		if (set->not_ready) {
			total = 1;
		}
	} else {
		bank = s->bank;
		size = s->bank_size;
		total = s->nstrips;
	}
	if (size && (s->feedback[0] || s->feedback[1] || s->feedback[4])) {
		lo_message reply;
		reply = lo_message_new ();
		if ((total <= size) || (bank > (total - size))) {
			lo_message_add_int32 (reply, 0);
		} else {
			lo_message_add_int32 (reply, 1);
		}
		lo_send_message (addr, X_("/bank_up"), reply);
		lo_message_free (reply);
		reply = lo_message_new ();
		if (bank > 1) {
			lo_message_add_int32 (reply, 1);
		} else {
			lo_message_add_int32 (reply, 0);
		}
		lo_send_message (addr, X_("/bank_down"), reply);
		lo_message_free (reply);
	}
}

int
OSC::bank_up (lo_message msg)
{
	return bank_delta (1.0, msg);
}

int
OSC::bank_delta (float delta, lo_message msg)
{
	if (!session) {
		return -1;
	}
	// only do deltas of -1 0 or 1
	if (delta > 0) {
		delta = 1;
	} else if (delta < 0) {
		delta = -1;
	} else {
		// 0  key release ignore
		return 0;
	}
	OSCSurface *s = get_surface(get_address (msg));
	if (!s->bank_size) {
		// bank size of 0 means use all strips no banking
		return 0;
	}
	uint32_t old_bank = 0;
	uint32_t bank_size = 0;
	if (s->linkset) {
		old_bank = link_sets[s->linkset].bank;
		bank_size = link_sets[s->linkset].banksize;
	} else {
		old_bank = s->bank;
		bank_size = s->bank_size;
	}
	uint32_t new_bank = old_bank + (bank_size * (int) delta);
	if ((int)new_bank < 1) {
		new_bank = 1;
	}
	if (new_bank != old_bank) {
		set_bank (new_bank, msg);
	}
	return 0;
}

int
OSC::bank_down (lo_message msg)
{
	return bank_delta (-1.0, msg);
}

int
OSC::use_group (float value, lo_message msg)
{
	if (!session) {
		return -1;
	}
	OSCSurface *s = get_surface(get_address (msg));
	if (value) {
		s->usegroup = PBD::Controllable::UseGroup;
	} else {
		s->usegroup = PBD::Controllable::NoGroup;
	}
	return 0;
}

// this gets called for anything that starts with /select/group
int
OSC::parse_sel_group (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s = sur->select;
	int ret = 1; /* unhandled */
	/// these could be added to strip
	if (s) {
		boost::shared_ptr<Route> rt = boost::dynamic_pointer_cast<Route> (s);
		if (!rt) {
			PBD::warning << "OSC: VCAs can not be part of a group." << endmsg;
			return ret;
		}
		RouteGroup *rg = rt->route_group();
		if (!rg) {
			PBD::warning << "OSC: This strip is not part of a group." << endmsg;
		}
		float value = 0;
		if (argc == 1) {
			if (types[0] == 'f') {
				value = (uint32_t) argv[0]->f;
			} else if (types[0] == 'i') {
				value = (uint32_t) argv[0]->i;
			}
		}
		if (!strncmp (path, X_("/select/group/enable"), 20)) {
			if (rg) {
				if (argc == 1) {
					rg->set_active (value, this);
					ret = 0;
				}
			} else {
				int_message (X_("/select/group/enable"), 0, get_address (msg));
			}
		}
		else if (strcmp (path, X_("/select/group/gain")) == 0) {
			if (rg) {
				if (argc == 1) {
					rg->set_gain ((bool) value);
					ret = 0;
				}
			} else {
				int_message (X_("/select/group/gain"), 0, get_address (msg));
			}
		}
		else if (strcmp (path, X_("/select/group/relative")) == 0) {
			if (rg) {
				if (argc == 1) {
					rg->set_relative ((bool) value, this);
					ret = 0;
				}
			} else {
				int_message (X_("/select/group/relative"), 0, get_address (msg));
			}
		}
		else if (strcmp (path, X_("/select/group/mute")) == 0) {
			if (rg) {
				if (argc == 1) {
					rg->set_mute ((bool) value);
					ret = 0;
				}
			} else {
				int_message (X_("/select/group/mute"), 0, get_address (msg));
			}
		}
		else if (strcmp (path, X_("/select/group/solo")) == 0) {
			if (rg) {
				if (argc == 1) {
					rg->set_solo ((bool) value);
					ret = 0;
				}
			} else {
				int_message (X_("/select/group/solo"), 0, get_address (msg));
			}
		}
		else if (strcmp (path, X_("/select/group/recenable")) == 0) {
			if (rg) {
				if (argc == 1) {
					rg->set_recenable ((bool) value);
					ret = 0;
				}
			} else {
				int_message (X_("/select/group/recenable"), 0, get_address (msg));
			}
		}
		else if (strcmp (path, X_("/select/group/select")) == 0) {
			if (rg) {
				if (argc == 1) {
					rg->set_select ((bool) value);
					ret = 0;
				}
			} else {
				int_message (X_("/select/group/select"), 0, get_address (msg));
			}
		}
		else if (strcmp (path, X_("/select/group/active")) == 0) {
			if (rg) {
				if (argc == 1) {
					rg->set_route_active ((bool) value);
					ret = 0;
				}
			} else {
				int_message (X_("/select/group/active"), 0, get_address (msg));
			}
		}
		else if (strcmp (path, X_("/select/group/color")) == 0) {
			if (rg) {
				if (argc == 1) {
					rg->set_color ((bool) value);
					ret = 0;
				}
			} else {
				int_message (X_("/select/group/color"), 0, get_address (msg));
			}
		}
		else if (strcmp (path, X_("/select/group/monitoring")) == 0) {
			if (rg) {
				if (argc == 1) {
					rg->set_monitoring ((bool) value);
					ret = 0;
				}
			} else {
				int_message (X_("/select/group/monitoring"), 0, get_address (msg));
			}
		}
	}
	return ret;
 }

boost::shared_ptr<VCA>
OSC::get_vca_by_name (std::string vname)
{
	StripableList stripables;
	session->get_stripables (stripables);
	for (StripableList::iterator it = stripables.begin(); it != stripables.end(); ++it) {
		boost::shared_ptr<Stripable> s = *it;
		boost::shared_ptr<VCA> v = boost::dynamic_pointer_cast<VCA> (s);
		if (v) {
			if (vname == v->name()) {
				return v;
			}
		}
	}
	return boost::shared_ptr<VCA>();
}

int
OSC::set_temp_mode (lo_address addr)
{
	bool ret = 1;
	OSCSurface *sur = get_surface(addr);
	boost::shared_ptr<Stripable> s = sur->temp_master;
	if (s) {
		if (sur->temp_mode == GroupOnly) {
			boost::shared_ptr<Route> rt = boost::dynamic_pointer_cast<Route> (s);
			if (rt) {
				RouteGroup *rg = rt->route_group();
				if (rg) {
					sur->temp_strips.clear();
					boost::shared_ptr<RouteList> rl = rg->route_list();
					for (RouteList::iterator it = rl->begin(); it != rl->end(); ++it) {
						boost::shared_ptr<Route> r = *it;
						boost::shared_ptr<Stripable> st = boost::dynamic_pointer_cast<Stripable> (r);
						sur->temp_strips.push_back(st);
					}
					// check if this group feeds a bus or is slaved
					boost::shared_ptr<Stripable> mstr = boost::shared_ptr<Stripable> ();
					if (rg->has_control_master()) {
						boost::shared_ptr<VCA> vca = session->vca_manager().vca_by_number (rg->group_master_number());
						if (vca) {
							mstr = boost::dynamic_pointer_cast<Stripable> (vca);
						}
					} else if (rg->has_subgroup()) {
						boost::shared_ptr<Route> sgr = rg->subgroup_bus().lock();
						if (sgr) {
							mstr = boost::dynamic_pointer_cast<Stripable> (sgr);
						}
					}
					if (mstr) {
						sur->temp_strips.push_back(mstr);
					}
					sur->strips = get_sorted_stripables(sur->strip_types, sur->cue, 1, sur->temp_strips);
					sur->nstrips = sur->temp_strips.size();
					ret = 0;
				}
			}
		} else if (sur->temp_mode == VCAOnly) {
			boost::shared_ptr<VCA> vca = boost::dynamic_pointer_cast<VCA> (s);
			if (vca) {
				sur->temp_strips.clear();
				StripableList stripables;
				session->get_stripables (stripables);
				for (StripableList::iterator it = stripables.begin(); it != stripables.end(); ++it) {
					boost::shared_ptr<Stripable> st = *it;
					if (st->slaved_to (vca)) {
						sur->temp_strips.push_back(st);
					}
				}
				sur->temp_strips.push_back(s);
				sur->strips = get_sorted_stripables(sur->strip_types, sur->cue, 1, sur->temp_strips);
				sur->nstrips = sur->temp_strips.size();
				ret = 0;
			}
		} else if (sur->temp_mode == BusOnly) {
			boost::shared_ptr<Route> rt = boost::dynamic_pointer_cast<Route> (s);
			if (rt) {
				if (!rt->is_track () && rt->can_solo ()) {
					// this is a bus, but not master, monitor or audition
					sur->temp_strips.clear();
					StripableList stripables;
					session->get_stripables (stripables, PresentationInfo::AllStripables);
					for (StripableList::iterator it = stripables.begin(); it != stripables.end(); ++it) {
						boost::shared_ptr<Stripable> st = *it;
						boost::shared_ptr<Route> ri = boost::dynamic_pointer_cast<Route> (st);
						bool sends = true;
						if (ri && ri->direct_feeds_according_to_graph (rt, &sends)) {
							sur->temp_strips.push_back(st);
						}
					}
					sur->temp_strips.push_back(s);
					sur->strips = get_sorted_stripables(sur->strip_types, sur->cue, 1, sur->temp_strips);
					sur->nstrips = sur->temp_strips.size();
					ret = 0;
				}
			}
		} else if (sur->temp_mode == TempOff) {
			sur->temp_mode = TempOff;
			ret = 0;
		}
	}
	LinkSet *set;
	uint32_t ls = sur->linkset;
	if (ls) {
		set = &(link_sets[ls]);
		set->temp_mode = sur->temp_mode;
		set->temp_strips.clear ();
		set->temp_strips = sur->temp_strips;
		set->temp_master = sur->temp_master;
		set->strips = sur->strips;
	}
	if (ret) {
		sur->temp_mode = TempOff;
	}
	return ret;
}

boost::shared_ptr<Send>
OSC::get_send (boost::shared_ptr<Stripable> st, lo_address addr)
{
	OSCSurface *sur = get_surface(addr);
	boost::shared_ptr<Stripable> s = sur->temp_master;
	if (st && s && (st != s)) {
		boost::shared_ptr<Route> rt = boost::dynamic_pointer_cast<Route> (s);
		boost::shared_ptr<Route> rst = boost::dynamic_pointer_cast<Route> (st);
		//find what send number feeds s
		return rst->internal_send_for (rt);
	}
	return boost::shared_ptr<Send> ();
}

int
OSC::name_session (char *n, lo_message msg)
{
	if (!session) {
		return -1;
	}
	string new_name = n;
	std::string const& illegal = Session::session_name_is_legal (new_name);

	if (!illegal.empty()) {
		PBD::warning  << (string_compose (_("To ensure compatibility with various systems\n"
				    "session names may not contain a '%1' character"), illegal)) << endmsg;
		return -1;
	}
	switch (session->rename (new_name)) {
		case -1:
			PBD::warning  << (_("That name is already in use by another directory/folder. Please try again.")) << endmsg;
			break;
		case 0:
			return 0;
			break;
		default:
			PBD::warning  << (_("Renaming this session failed.\nThings could be seriously messed up at this point")) << endmsg;
			break;
	}
	return -1;
}

uint32_t
OSC::get_sid (boost::shared_ptr<ARDOUR::Stripable> strip, lo_address addr)
{
	if (!strip) {
		return 0;
	}

	OSCSurface *s = get_surface(addr);

	uint32_t b_size;
	if (!s->bank_size) {
		// no banking
		b_size = s->nstrips;
	} else {
		b_size = s->bank_size;
	}

	for (uint32_t n = s->bank; n < (min ((b_size + s->bank), s->nstrips + 1)); ++n) {
		if (n <= s->strips.size()) {
			if (strip == s->strips[n-1]) {
				return n - s->bank + 1;
			}
		}
	}
	// strip not in current bank
	return 0;
}

boost::shared_ptr<ARDOUR::Stripable>
OSC::get_strip (uint32_t ssid, lo_address addr)
{
	OSCSurface *s = get_surface(addr);
	if (ssid && ((ssid + s->bank - 2) < s->nstrips)) {
		return s->strips[ssid + s->bank - 2];
	}
	// guess it is out of range
	return boost::shared_ptr<ARDOUR::Stripable>();
}

// send and plugin paging commands
int
OSC::sel_send_pagesize (uint32_t size, lo_message msg)
{
	OSCSurface *s = get_surface(get_address (msg));
	if  (size != s->send_page_size) {
		s->send_page_size = size;
		s->sel_obs->set_send_size(size);
	}
	return 0;
}

int
OSC::sel_send_page (int page, lo_message msg)
{
	OSCSurface *s = get_surface(get_address (msg));
	uint32_t send_size = s->send_page_size;
	if (!send_size) {
		send_size = s->nsends;
	}
	uint32_t max_page = (uint32_t)(s->nsends / send_size) + 1;
	s->send_page = s->send_page + page;
	if (s->send_page < 1) {
		s->send_page = 1;
	} else if ((uint32_t)s->send_page > max_page) {
		s->send_page = max_page;
	}
	s->sel_obs->set_send_page (s->send_page);
	return 0;
}

int
OSC::sel_plug_pagesize (uint32_t size, lo_message msg)
{
	OSCSurface *s = get_surface(get_address (msg));
	if (size != s->plug_page_size) {
		s->plug_page_size = size;
		s->sel_obs->set_plugin_size(size);
	}
	return 0;
}

int
OSC::sel_plug_page (int page, lo_message msg)
{
	if (!page) {
		return 0;
	}
	int new_page = 0;
	OSCSurface *s = get_surface(get_address (msg));
	if (page > 0) {
		new_page = s->plug_page + s->plug_page_size;
		if ((uint32_t) new_page > s->plug_params.size ()) {
			new_page = s->plug_page;
		}
	} else {
		new_page = s->plug_page - s->plug_page_size;
		if (new_page < 1) {
			new_page = 1;
		}
	}
	if (new_page != s->plug_page) {
		s->plug_page = new_page;
		s->sel_obs->set_plugin_page(s->plug_page);
	}
	return 0;
}

int
OSC::sel_plugin (int delta, lo_message msg)
{
	if (!delta) {
		return 0;
	}
	OSCSurface *sur = get_surface(get_address (msg));
	return _sel_plugin (sur->plugin_id + delta, get_address (msg));
}

int
OSC::_sel_plugin (int id, lo_address addr)
{
	OSCSurface *sur = get_surface(addr);
	boost::shared_ptr<Stripable> s = sur->select;
	if (s) {
		boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route>(s);
		if (!r) {
			return 1;
		}

		/* find out how many plugins we have */
		sur->plugins.clear();
		for (int nplugs = 0; true; ++nplugs) {
			boost::shared_ptr<Processor> proc = r->nth_plugin (nplugs);
			if (!proc) {
				break;
			}
			if (!r->nth_plugin(nplugs)->display_to_user()) {
				continue;
			}
#ifdef MIXBUS
			/* need to check for mixbus channel strips (and exclude them) */
			boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert>(proc);
			if (pi && pi->is_channelstrip()) {
				continue;
			}
#endif
			sur->plugins.push_back (nplugs);
		}

		// limit plugin_id to actual plugins
		if (sur->plugins.size() < 1) {
			sur->plugin_id = 0;
			sur->plug_page = 1;
			if (sur->sel_obs) {
				sur->sel_obs->set_plugin_id(-1, 1);
			}
			return 0;
		} else if (id < 1) {
			sur->plugin_id = 1;
		} else if (sur->plugins.size() < (uint32_t) id) {
			sur->plugin_id = sur->plugins.size();
		} else {
			sur->plugin_id = id;
		}

		// we have a plugin number now get the processor
		boost::shared_ptr<Processor> proc = r->nth_plugin (sur->plugins[sur->plugin_id - 1]);
		boost::shared_ptr<PluginInsert> pi;
		if (!(pi = boost::dynamic_pointer_cast<PluginInsert>(proc))) {
			PBD::warning << "OSC: Plugin: " << sur->plugin_id << " does not seem to be a plugin" << endmsg;
			return 1;
		}
		boost::shared_ptr<ARDOUR::Plugin> pip = pi->plugin();
		bool ok = false;
		// put only input controls into a vector
		sur->plug_params.clear ();
		uint32_t nplug_params  = pip->parameter_count();
		for ( uint32_t ppi = 0;  ppi < nplug_params; ++ppi) {
			uint32_t controlid = pip->nth_parameter(ppi, ok);
			if (!ok) {
				continue;
			}
			if (pip->parameter_is_input(controlid)) {
				sur->plug_params.push_back (ppi);
			}
		}

		sur->plug_page = 1;

		if (sur->sel_obs) {
			sur->sel_obs->set_plugin_id(sur->plugins[sur->plugin_id - 1], sur->plug_page);
		}
		return 0;
	}
	return 1;
}

void
OSC::transport_sample (lo_message msg)
{
	if (!session) {
		return;
	}
	check_surface (msg);
	samplepos_t pos = session->transport_sample ();

	lo_message reply = lo_message_new ();
	lo_message_add_int64 (reply, pos);

	lo_send_message (get_address (msg), X_("/transport_frame"), reply);

	lo_message_free (reply);
}

void
OSC::transport_speed (lo_message msg)
{
	if (!session) {
		return;
	}
	check_surface (msg);
	double ts = get_transport_speed();

	lo_message reply = lo_message_new ();
	lo_message_add_double (reply, ts);

	lo_send_message (get_address (msg), X_("/transport_speed"), reply);

	lo_message_free (reply);
}

void
OSC::record_enabled (lo_message msg)
{
	if (!session) {
		return;
	}
	check_surface (msg);
	int re = (int)session->get_record_enabled ();

	lo_message reply = lo_message_new ();
	lo_message_add_int32 (reply, re);

	lo_send_message (get_address (msg), X_("/record_enabled"), reply);

	lo_message_free (reply);
}

int
OSC::scrub (float delta, lo_message msg)
{
	if (!session) return -1;
	check_surface (msg);

	scrub_place = session->transport_sample ();

	float speed;

	int64_t now = PBD::get_microseconds ();
	int64_t diff = now - scrub_time;
	if (diff > 35000) {
		// speed 1 (or 0 if jog wheel supports touch)
		speed = delta;
	} else if ((diff > 20000) && (fabs(scrub_speed) == 1)) {
		// add some hysteresis to stop excess speed jumps
		speed = delta;
	} else {
		speed = (int)(delta * 2);
	}
	scrub_time = now;
	if (scrub_speed == speed) {
		// Already at that speed no change
		return 0;
	}
	scrub_speed = speed;

	if (speed > 0) {
		if (speed == 1) {
			session->request_transport_speed (.5);
		} else {
			session->request_transport_speed (9.9);
		}
	} else if (speed < 0) {
		if (speed == -1) {
			session->request_transport_speed (-.5);
		} else {
			session->request_transport_speed (-1);
		}
	} else {
		session->request_stop ();
	}

	return 0;
}

int
OSC::jog (float delta, lo_message msg)
{
	if (!session) return -1;

	OSCSurface *s = get_surface(get_address (msg));

	switch(s->jogmode)
	{
		case 0:
			if (delta) {
				jump_by_seconds (delta / 5);
			}
			break;
		case 1:
			if (delta > 0) {
				access_action ("Common/nudge-playhead-forward");
			} else if (delta < 0) {
				access_action ("Common/nudge-playhead-backward");
			}
			break;
		case 2:
			scrub (delta, msg);
			break;
		case 3:
			if (delta) {
				double speed = get_transport_speed ();
				set_transport_speed (speed + (delta / 8.1));
			} else {
				set_transport_speed (0);
			}
			break;
		case 4:
			if (delta > 0) {
				next_marker ();
			} else if (delta < 0) {
				prev_marker ();
			}
			break;
		case 5:
			if (delta > 0) {
				access_action ("Editor/scroll-forward");
			} else if (delta < 0) {
				access_action ("Editor/scroll-backward");
			}
			break;
		case 6:
			if (delta > 0) {
				set_bank (s->bank + 1, msg);
			} else if (delta < 0) {
				set_bank (s->bank - 1, msg);
			}
			break;
		case 7:
			if (delta > 0) {
				bank_up (msg);
			} else if (delta < 0) {
				bank_down (msg);
			}
			break;
		default:
			break;

	}
	return 0;

}

int
OSC::jog_mode (float mode, lo_message msg)
{
	if (!session) return -1;

	OSCSurface *s = get_surface(get_address (msg));
	if (get_transport_speed () != 1.0) {
		set_transport_speed (0);
	}
	s->jogmode = (uint32_t) mode;
	s->global_obs->jog_mode (mode);
	return 0;
}

// two structs to help with going to markers
struct LocationMarker {
	LocationMarker (const std::string& l, samplepos_t w)
		: label (l), when (w) {}
	std::string label;
	samplepos_t  when;
};

struct LocationMarkerSort {
	bool operator() (const LocationMarker& a, const LocationMarker& b) {
		return (a.when < b.when);
	}
};

int
OSC::set_marker (const char* types, lo_arg **argv, int argc, lo_message msg)
{
	if (argc != 1) {
		PBD::warning << "Wrong number of parameters, one only." << endmsg;
		return -1;
	}
	const Locations::LocationList& ll (session->locations ()->list ());
	uint32_t marker = 0;

	switch (types[0]) {
		case 's':
			{
				Location *cur_mark = 0;
				for (Locations::LocationList::const_iterator l = ll.begin(); l != ll.end(); ++l) {
					if ((*l)->is_mark ()) {
						if (strcmp (&argv[0]->s, (*l)->name().c_str()) == 0) {
							session->request_locate ((*l)->start_sample (), MustStop);
							return 0;
						} else if ((*l)->start () == session->transport_sample()) {
							cur_mark = (*l);
						}
					}
				}
				if (cur_mark) {
					cur_mark->set_name (&argv[0]->s);
					return 0;
				}
				PBD::warning << string_compose ("Marker: \"%1\" - does not exist", &argv[0]->s) << endmsg;
				return -1;
			}
			break;
		case 'i':
			marker = (uint32_t) argv[0]->i - 1;
			break;
		case 'f':
			marker = (uint32_t) argv[0]->f - 1;
			break;
		default:
			return -1;
			break;
	}
	std::vector<LocationMarker> lm;
	// get Locations that are marks
	for (Locations::LocationList::const_iterator l = ll.begin(); l != ll.end(); ++l) {
		if ((*l)->is_mark ()) {
			lm.push_back (LocationMarker((*l)->name(), (*l)->start_sample ()));
		}
	}
	// sort them by position
	LocationMarkerSort location_marker_sort;
	std::sort (lm.begin(), lm.end(), location_marker_sort);
	// go there
	if (marker < lm.size()) {
		session->request_locate (lm[marker].when, MustStop);
		return 0;
	}
	// we were unable to deal with things
	return -1;
}

int
OSC::group_list (lo_message msg)
{
	return send_group_list (get_address (msg));
}

int
OSC::send_group_list (lo_address addr)
{
	lo_message reply;
	reply = lo_message_new ();

	lo_message_add_string (reply, X_("none"));

	std::list<RouteGroup*> groups = session->route_groups ();
	for (std::list<RouteGroup *>::iterator i = groups.begin(); i != groups.end(); ++i) {
		RouteGroup *rg = *i;
		lo_message_add_string (reply, rg->name().c_str());
	}
	lo_send_message (addr, X_("/group/list"), reply);
	lo_message_free (reply);
	return 0;
}

int
OSC::click_level (float position)
{
	if (!session) return -1;
	if (session->click_gain()->gain_control()) {
		session->click_gain()->gain_control()->set_value (session->click_gain()->gain_control()->interface_to_internal (position), PBD::Controllable::NoGroup);
	}
	return 0;
}

int
OSC::route_get_sends(lo_message msg) {
	if (!session) {
		return -1;
	}

	lo_arg **argv = lo_message_get_argv(msg);

	int rid = argv[0]->i;

	boost::shared_ptr<Stripable> strip = get_strip(rid, get_address(msg));
	if (!strip) {
		return -1;
	}

	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (strip);
	if (!r) {
		return -1;
	}

	lo_message reply = lo_message_new();
	lo_message_add_int32(reply, rid);

	int i = 0;
	for (;;) {
		boost::shared_ptr<Processor> p = r->nth_send(i++);

		if (!p) {
			break;
		}

		boost::shared_ptr<InternalSend> isend = boost::dynamic_pointer_cast<InternalSend> (p);
		if (isend) {
			lo_message_add_int32(reply, get_sid(isend->target_route(), get_address(msg)));
			lo_message_add_string(reply, isend->name().c_str());
			lo_message_add_int32(reply, i);
			boost::shared_ptr<Amp> a = isend->amp();
			lo_message_add_float(reply, a->gain_control()->internal_to_interface (a->gain_control()->get_value()));
			lo_message_add_int32(reply, p->active() ? 1 : 0);
		}
	}
	// if used dedicated message path to identify this reply in async operation.
	// Naming it #reply wont help the client to identify the content.
	lo_send_message(get_address (msg), X_("/strip/sends"), reply);

	lo_message_free(reply);

	return 0;
}

int
OSC::route_get_receives(lo_message msg) {
	if (!session) {
		return -1;
	}

	lo_arg **argv = lo_message_get_argv(msg);

	uint32_t rid = argv[0]->i;


	boost::shared_ptr<Stripable> strip = get_strip(rid, get_address(msg));
	if (!strip) {
		return -1;
	}

	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (strip);
	if (!r) {
		return -1;
	}

	boost::shared_ptr<RouteList> route_list = session->get_routes();

	lo_message reply = lo_message_new();
	lo_message_add_int32(reply, rid);

	for (RouteList::iterator i = route_list->begin(); i != route_list->end(); ++i) {
		boost::shared_ptr<Route> tr = boost::dynamic_pointer_cast<Route> (*i);
		if (!tr) {
			continue;
		}
		int j = 0;

		for (;;) {
			boost::shared_ptr<Processor> p = tr->nth_send(j++);

			if (!p) {
				break;
			}

			boost::shared_ptr<InternalSend> isend = boost::dynamic_pointer_cast<InternalSend> (p);
			if (isend) {
				if( isend->target_route()->id() == r->id()){
					boost::shared_ptr<Amp> a = isend->amp();

					lo_message_add_int32(reply, get_sid(tr, get_address(msg)));
					lo_message_add_string(reply, tr->name().c_str());
					lo_message_add_int32(reply, j);
					lo_message_add_float(reply, a->gain_control()->internal_to_interface (a->gain_control()->get_value()));
					lo_message_add_int32(reply, p->active() ? 1 : 0);
				}
			}
		}
	}

	// I have used a dedicated message path to identify this reply in async operation.
	// Naming it #reply wont help the client to identify the content.
	lo_send_message(get_address (msg), X_("/strip/receives"), reply);
	lo_message_free(reply);
	return 0;
}

// strip calls

int
OSC::master_parse (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg)
{
	if (!session) return -1;
	int ret = 1;
	// set sub_path to null string if path is /master
	const char* sub_path = &path[7];
	if (strlen(path) > 8) {
		// reset sub_path to char after /master/ if at least 1 char longer
		sub_path = &path[8];
	} else if (strlen(path) == 8) {
		PBD::warning << "OSC: trailing / not valid." << endmsg;
	}

	//OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s = session->master_out();
	if (s) {
		ret = _strip_parse (path, sub_path, types, argv, argc, s, 0, false, msg);
	} else {
		PBD::warning << "OSC: No Master strip" << endmsg;
	}
	return ret;
}

int
OSC::monitor_parse (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg)
{
	if (!session) return -1;
	int ret = 1;
	// set sub_path to null string if path is /monitor
	const char* sub_path = &path[8];
	if (strlen(path) > 9) {
		// reset sub_path to char after /monitor/ if at least 1 char longer
		sub_path = &path[9];
	} else if (strlen(path) == 9) {
		PBD::warning << "OSC: trailing / not valid." << endmsg;
	}

	//OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s = session->monitor_out();
	if (s) {
		boost::shared_ptr<MonitorProcessor> mon = session->monitor_out()->monitor_control();
		int state = 0;
		if (types[0] == 'f') {
			state = (uint32_t) argv[0]->f;
		} else if (types[0] == 'i') {
			state = argv[0]->i;
		}
		// these are only in the monitor section
		if (!strncmp (sub_path, X_("mute"), 4)) {
			if (argc) {
				mon->set_cut_all (state);
			} else {
				int_message (path, mon->cut_all (), get_address (msg));
			}
		} else if (!strncmp (sub_path, X_("dim"), 3)) {
			if (argc) {
				mon->set_dim_all (state);
			} else {
				int_message (path, mon->dim_all (), get_address (msg));
			}
		} else if (!strncmp (sub_path, X_("mono"), 4)) {
			if (argc) {
				mon->set_mono (state);
			} else {
				int_message (path, mon->mono (), get_address (msg));
			}
		} else {
			ret = _strip_parse (path, sub_path, types, argv, argc, s, 0, false, msg);
		}
	} else {
		PBD::warning << "OSC: No Monitor strip" << endmsg;
	}
	return ret;
}

int
OSC::select_parse (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg)
{
	if (!session) return -1;
	int ret = 1;
	// set sub_path to null string if path is /select
	const char* sub_path = &path[7];
	if (strlen(path) > 8) {
		// reset sub_path to char after /select/ if at least 1 char longer
		sub_path = &path[8];
	} else if (strlen(path) == 8) {
		PBD::warning << "OSC: trailing / not valid." << endmsg;
	}

	OSCSurface *sur = get_surface(get_address (msg));

	if (!strncmp (sub_path, X_("select"), 6)) {
		PBD::warning << "OSC: select is already selected." << endmsg;
		return 1;
	}
	if (!strncmp (path, X_("/select/group"), 13) && strlen (path) > 13) {
		/** this needs fixing as it blocks /group s name */
		PBD::info << "OSC: select_parse /select/group/." << endmsg;
		ret = parse_sel_group (path, types, argv, argc, msg);
	}
	else if (!strncmp (path, X_("/select/send_gain/"), 18) && strlen (path) > 18) {
		int ssid = atoi (&path[18]);
		ret = sel_sendgain (ssid, argv[0]->f, msg);
	}
	else if (!strncmp (path, X_("/select/send_fader/"), 19) && strlen (path) > 19) {
		int ssid = atoi (&path[19]);
		ret = sel_sendfader (ssid, argv[0]->f, msg);
	}
	else if (!strncmp (path, X_("/select/send_enable/"), 20) && strlen (path) > 20) {
		int ssid = atoi (&path[20]);
		ret = sel_sendenable (ssid, argv[0]->f, msg);
	}
	else if (!strncmp (path, X_("/select/eq_gain/"), 16) && strlen (path) > 16) {
		int ssid = atoi (&path[16]);
		ret = sel_eq_gain (ssid, argv[0]->f, msg);
	}
	else if (!strncmp (path, X_("/select/eq_freq/"), 16) && strlen (path) > 16) {
		int ssid = atoi (&path[16]);
		ret = sel_eq_freq (ssid, argv[0]->f , msg);
	}
	else if (!strncmp (path, X_("/select/eq_q/"), 13) && strlen (path) > 13) {
		int ssid = atoi (&path[13]);
		ret = sel_eq_q (ssid, argv[0]->f, msg);
	}
	else if (!strncmp (path, X_("/select/eq_shape/"), 17) && strlen (path) > 17) {
		int ssid = atoi (&path[17]);
		ret = sel_eq_shape (ssid, argv[0]->f, msg);
	}
	else {
		/// this is in both strip and select
		boost::shared_ptr<Stripable> s = sur->select;
		if (s) {
			if (!strncmp (sub_path, X_("expand"), 6)) {
				int yn = 0;
				if (types[0] == 'f') {
					yn = (int) argv[0]->f;
				} else if (types[0] == 'i') {
					yn = argv[0]->i;
				} else {
					return 1;
				}
				if (types[0] != 'f' && types[0] != 'i') {
					return 1;
				}
				sur->expand_strip = s;
				sur->expand_enable = (bool) yn;
				boost::shared_ptr<Stripable> sel;
				if (yn) {
					sel = s;
				} else {
					sel = boost::shared_ptr<Stripable> ();
				}

				return _strip_select (sel, get_address (msg));
			} else {
				ret = _strip_parse (path, sub_path, types, argv, argc, s, 0, false, msg);
			}
		} else {
			PBD::warning << "OSC: No selected strip" << endmsg;
		}
	}

	return ret;
}


int
OSC::strip_parse (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg)
{
	if (!session) return -1;
	int ret = 1;
	int ssid = 0;
	int param_1 = 1;
	uint32_t nparam = argc;
	const char* sub_path = &path[6];
	if (strlen(path) > 7) {
		// reset sub_path to char after /strip/ if at least 1 char longer
		sub_path = &path[7];
	} else if (strlen(path) == 7) {
		PBD::warning << "OSC: trailing / not valid." << endmsg;
		return 1;
	}

	OSCSurface *sur = get_surface(get_address (msg));

	// ssid may be in three places
	 if (atoi(sub_path)) {
		// test for /strip/<ssid>/subpath
		ssid = atoi(sub_path);
		nparam++;
		param_1 = 0;
		if (strchr(sub_path, (int) '/')) {
			sub_path = &(strchr(sub_path, (int) '/')[1]);
		} else {
			sub_path = &(strchr(sub_path, 0)[1]);
		}
	} else if (atoi (&(strrchr(path, (int) '/')[1]))) {
		// check for /path/<ssid>
		ssid = atoi (&(strrchr(path, (int) '/')[1]));
		nparam++;
		param_1 = 0;
	} else if (argc) {
		if (types[0] == 'i') {
			ssid = argv[0]->i;
		} else if (types[0] == 'f') {
			ssid = argv[0]->f;
		}
	}
	if (!nparam && !ssid) {
		// only list works here
		if (!strcmp (path, X_("/strip/list"))) {
			// /strip/list is legacy
			routes_list (msg);
			ret = 0;
		}
		else if (!strcmp (path, X_("/strip"))) {
			strip_list (msg);
			ret = 0;
		} else {
			PBD::warning << "OSC: missing parameters." << endmsg;
			return 1;
		}
	}
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
	if (s) {
		if (!strncmp (sub_path, X_("expand"), 6)) {
			/// this is in both strip and select should be in _parse_strip
			int yn = 0;
			if (types[param_1] == 'f') {
				yn = (int) argv[param_1]->f;
			} else if (types[param_1] == 'i') {
				yn = argv[param_1]->i;
			} else {
				return 1;
			}
			if (types[param_1] != 'f' && types[param_1] != 'i') {
				return 1;
			}
			sur->expand_strip = s;
			sur->expand_enable = (bool) yn;
			sur->expand = ssid;
			boost::shared_ptr<Stripable> sel;
			if (yn) {
				sel = s;
			} else {
				sel = boost::shared_ptr<Stripable> ();
			}

			return _strip_select (sel, get_address (msg));
		} else {
			ret = _strip_parse (path, sub_path, types, argv, argc, s, param_1, true, msg);
		}
	} else {
		PBD::warning << "OSC: No such strip" << endmsg;
	}

	return ret;

}

int
OSC::_strip_parse (const char *path, const char *sub_path, const char* types, lo_arg **argv, int argc, boost::shared_ptr<ARDOUR::Stripable> s, int param_1, bool strp, lo_message msg)
{
	int ret = 1;
	int yn = 0;
	float value = 0.0;
	string strng = "";
	char *text;
	bool s_flt = false;
	bool s_int = false;
	if (types[param_1] == 'f') {
		yn = (int) argv[param_1]->f;
		s_int = true;
		value = argv[param_1]->f;
		s_flt = true;
	} else if (types[param_1] == 'i') {
		yn = argv[param_1]->i;
		s_int = true;
	} else if (types[param_1] == 's') {
		text = &argv[param_1]->s;
		strng = &argv[param_1]->s;
		if (atoi(text) || text[0] == '0') {
			yn = atoi(text);
			s_int = true;
		}
		if (atof(text)) {
			value = atof(text);
			s_flt = true;
		}
	}
	OSCSurface *sur = get_surface(get_address (msg));
	bool send_active = strp && sur->temp_mode == BusOnly && get_send (s, get_address (msg));
	bool control_disabled = strp && (sur->temp_mode == BusOnly) && (s != sur->temp_master);
	bool n_mo = !s->is_monitor();
	boost::shared_ptr<Route> rt = boost::dynamic_pointer_cast<Route> (s);

	if (!strlen(sub_path)) {
		// send stripable info
		int sid = 0;
		if (param_1) {
			if (types[0] == 'f') {
				sid = (int) argv[0]->f;
			} else if (types[0] == 'i') {
				sid = argv[0]->i;
			}
		}
		ret = strip_state (path, s, sid, msg);
	}
	else if (!strncmp (sub_path, X_("gain"), 4) || !strncmp (sub_path, X_("fader"), 5) ||  !strncmp (sub_path, X_("db_delta"), 8)){
		boost::shared_ptr<GainControl> gain_control;
		gain_control = s->gain_control();
		if (gain_control) {
			if (argc > (param_1)) {
				if (s_flt) {
					if (send_active) {
						gain_control = get_send(s, get_address (msg))->gain_control();
					}
					float abs;
					if (!strncmp (sub_path, X_("gain"), 4)) {
						if (value < -192) {
							abs = 0;
						} else {
							abs = dB_to_coefficient (value);
						}
					} else if (!strncmp (sub_path, X_("fader"), 5)) {
						abs = gain_control->interface_to_internal (value);
					} else if (!strncmp (sub_path, X_("db_delta"), 8)) {
						float db = accurate_coefficient_to_dB (gain_control->get_value()) + value;
						if (db < -192) {
							abs = 0;
						} else {
							abs = dB_to_coefficient (db);
						}
					} else {
						abs = 0;
					}
					float top = gain_control->upper();
					if (abs > top) {
						abs = top;
					}
					fake_touch (gain_control);
					gain_control->set_value (abs, sur->usegroup);
					ret = 0;
				}
			} else {
				float ret_v;
				if (!strncmp (sub_path, X_("gain"), 4)) {
					ret_v = fast_coefficient_to_dB (gain_control->get_value ());
					ret = 0;
				} else if (!strncmp (sub_path, X_("fader"), 5)) {
					ret_v = gain_control->internal_to_interface (gain_control->get_value ());
					ret = 0;
				} else {
					PBD::warning << "OSC: delta has no info" << endmsg;
				}
				if (!ret) {
					float_message (path, ret_v, get_address (msg));
				}
			}
		}
	}
	else if (!strncmp (sub_path, X_("trimdB"), 6)) {
		if (!control_disabled && s->trim_control() && n_mo) {
			if (argc > (param_1)) {
				if (s_flt) {
					float abs = dB_to_coefficient (value);
					s->trim_control()->set_value (abs, sur->usegroup);
					fake_touch (s->trim_control());
					ret = 0;
				}
			} else {
				float_message (path, fast_coefficient_to_dB (s->trim_control()->get_value ()), get_address (msg));
				ret = 0;
			}
		}
	}
	else if (!strncmp (sub_path, X_("pan_stereo_position"), 19)) {
		boost::shared_ptr<PBD::Controllable> pan_control = boost::shared_ptr<PBD::Controllable>();
		pan_control = s->pan_azimuth_control();
		if (n_mo && pan_control) {
			if (argc > (param_1)) {
				if (s_flt) {
					if (send_active) {
						boost::shared_ptr<ARDOUR::Send> send = get_send (s, get_address (msg));
						if (send->pan_outs() > 1) {
							pan_control = send->panner_shell()->panner()->pannable()->pan_azimuth_control;
						} else {
							pan_control = boost::shared_ptr<PBD::Controllable>();
						}
					}
					if(pan_control) {
						pan_control->set_value (s->pan_azimuth_control()->interface_to_internal (value), sur->usegroup);
						boost::shared_ptr<AutomationControl>pan_automate = boost::dynamic_pointer_cast<AutomationControl> (pan_control);
						fake_touch (pan_automate);
						ret = 0;
					}
				}
			} else {
				float_message (path, pan_control->internal_to_interface (pan_control->get_value ()), get_address (msg));
				ret = 0;
			}
		}
	}
	else if (!strncmp (sub_path, X_("pan_stereo_width"), 16)) {
		if (!control_disabled && s->pan_width_control()) {
			if (argc > (param_1)) {
				if (s_flt) {
					/// this should maybe be active in send mode (see above)
					s->pan_width_control()->set_value (value, sur->usegroup);
					fake_touch (s->pan_width_control());
					ret = 0;
				}
			} else {
				float_message (path, s->pan_width_control()->get_value (), get_address (msg));
				ret = 0;
			}
		}
	}
	else if (!strncmp (sub_path, X_("mute"), 4)) {
		if (!control_disabled && s->mute_control()) {
			if (argc > (param_1)) {
				if (s_int) {
					s->mute_control()->set_value (yn ? 1.0 : 0.0, sur->usegroup);
					fake_touch (s->mute_control());
					ret = 0;
				}
			} else {
				int_message (path, s->mute_control()->get_value (), get_address (msg));
				ret = 0;
			}
		}
	}
	else if (!strncmp (sub_path, X_("solo_iso"), 8)) {
		if (!control_disabled && s->solo_isolate_control()) {
			if (argc > (param_1)) {
				if (s_int) {
					s->solo_isolate_control()->set_value (yn ? 1.0 : 0.0, sur->usegroup);
					ret = 0;
				}
			} else {
				int_message (path, s->solo_isolate_control()->get_value (), get_address (msg));
				ret = 0;
			}
		}
	}
	else if (!strncmp (sub_path, X_("solo_safe"), 9)) {
		if (!control_disabled && s->solo_safe_control()) {
			if (argc > (param_1)) {
				if (s_int) {
					s->solo_safe_control()->set_value (yn ? 1.0 : 0.0, sur->usegroup);
					ret = 0;
				}
			} else {
				int_message (path, s->solo_safe_control()->get_value (), get_address (msg));
				ret = 0;
			}
		}
	}
	else if (!strncmp (sub_path, X_("solo"), 4)) {
		if (!control_disabled && s->solo_control() && !s->is_master() && !s->is_monitor()) {
			if (argc > (param_1)) {
				if (s_int) {
					s->solo_control()->set_value (yn ? 1.0 : 0.0, sur->usegroup);
					ret = 0;
				}
			} else {
				int_message (path, s->solo_control()->get_value (), get_address (msg));
				ret = 0;
			}
		}
	}
	else if (!strncmp (sub_path, X_("monitor_input"), 13)) {
		boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (s);
		if (!control_disabled && track && track->monitoring_control()) {
			std::bitset<32> mon_bs = track->monitoring_control()->get_value ();
			if (argc > (param_1)) {
				if (s_int) {
					mon_bs[0] = yn ? 1 : 0;
					track->monitoring_control()->set_value (mon_bs.to_ulong(), sur->usegroup);
					ret = 0;
				}
			} else {
				int_message (path, (int) mon_bs[0], get_address (msg));
				ret = 0;
			}
		}
	}
	else if (!strncmp (sub_path, X_("monitor_disk"), 12)) {
		boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (s);
		if (!control_disabled && track && track->monitoring_control()) {
			std::bitset<32> mon_bs = track->monitoring_control()->get_value ();
			if (argc > (param_1)) {
				if (s_int) {
					mon_bs[1] = yn ? 1 : 0;
					track->monitoring_control()->set_value (mon_bs.to_ulong(), sur->usegroup);
					ret = 0;
				}
			} else {
				int_message (path, (int) mon_bs[1], get_address (msg));
				ret = 0;
			}
		}
	}
	else if (!strncmp (sub_path, X_("recenable"), 9)) {
		if (!control_disabled && s->rec_enable_control()) {
			if (argc > (param_1)) {
				if (s_int) {
					s->rec_enable_control()->set_value (yn, sur->usegroup);
					ret = 0;
				}
			} else {
				int_message (path, s->rec_enable_control()->get_value (), get_address (msg));
				ret = 0;
			}
		}
	}
	else if (!strncmp (sub_path, X_("record_safe"), 11)) {
		if (!control_disabled && s->rec_safe_control()) {
			if (argc > (param_1)) {
				if (s_int) {
					s->rec_safe_control()->set_value (yn, sur->usegroup);
					ret = 0;
				}
			} else {
				int_message (path, s->rec_safe_control()->get_value (), get_address (msg));
				ret = 0;
			}
		}
	}
	else if (!strncmp (sub_path, X_("hide"), 4)) {
		if (!control_disabled) {
			if (argc > (param_1)) {
				if (s_int && yn != s->is_hidden ()) {
					s->presentation_info().set_hidden ((bool) yn);
					ret = 0;
				} else {
					PBD::warning << string_compose("OSC: value already %1 not changed.", yn) << endmsg;
				}
			} else {
				int_message (path, s->is_hidden (), get_address (msg));
				ret = 0;
			}
		}
	}
	else if (!strncmp (sub_path, X_("select"), 6)) {
		if (argc > (param_1)) {
			if (s_int) {
				//ignore button release
				if (!yn) return 0;
				sur->expand_enable = false;
				set_stripable_selection (s);
				ret = 0;
			}
		} else {
			int_message (path, s->is_selected(), get_address (msg));
			ret = 0;
		}
	}
	else if (!strncmp (sub_path, X_("polarity"), 8)) {
		if (!control_disabled && s->phase_control()) {
			if (argc > (param_1)) {
				if (s_int) {
					for (uint64_t i = 0; i < s->phase_control()->size(); i++) {
						s->phase_control()->set_phase_invert(i, yn ? 1.0 : 0.0);
						/** maybe consider adding a param for which channel
						 * polarity/1 for channel one for example
						 * at that point maybe do in own call
						 */
					}
					ret = 0;
				}
			} else {
				int inv = 0;
				for (uint64_t i = 0; i < s->phase_control()->size(); i++) {
					if (s->phase_control()->inverted (i)) {
						// just check if any are inverted
						inv = 1;
					}
				}
				int_message (path, inv, get_address (msg));
				ret = 0;
			}
		}
	}
	else if (!strncmp (sub_path, X_("name"), 4)) {
		if (argc > (param_1)) {
			if (types[param_1] == 's') {
				if (!control_disabled) {
					s->set_name(strng);
					ret = 0;
				}
			}
		} else {
			text_message (path, s->name(), get_address (msg));
			ret = 0;
		}
	}
	else if (!strncmp (sub_path, X_("group"), 5)) {
		if (!control_disabled) {
			if (rt) {
				RouteGroup *rg = rt->route_group();
				if (argc > (param_1)) {
					if (types[param_1] == 's') {


						if (strng == "" || strng == " ") {
							strng = "none";
						}

						RouteGroup* new_rg = session->route_group_by_name (strng);
						if (rg) {
							string old_group = rg->name();
							if (strng == "none") {
								if (rg->size () == 1) {
									session->remove_route_group (*rg);
								} else {
									rg->remove (rt);
								}
								ret = 0;
							} else if (strng != old_group) {
								if (new_rg) {
									// group exists switch to it
									if (rg->size () == 1) {
										session->remove_route_group (rg);
									} else {
										rg->remove (rt);
									}
									new_rg->add (rt);
								} else {
									rg->set_name (strng);
								}
								ret = 0;
							} else {
								// asked for same group
								ret = 1;
							}
						} else {
							if (strng == "none") {
								ret = 1;
							} else if (new_rg) {
								new_rg->add (rt);
								ret = 0;
							} else {
								// create new group with this strip in it
								RouteGroup* new_rg = new RouteGroup (*session, strng);
								session->add_route_group (new_rg);
								new_rg->add (rt);
								ret = 0;
							}
						}
					}
				} else {
					if (rg) {
						text_message (path, rg->name(), get_address (msg));
					} else {
						text_message (path, "none", get_address (msg));
					}
					ret = 0;
				}
			} else {
				PBD::warning << "OSC: VCAs can not be part of a group." << endmsg;
				///return -1;
			}
		}
	}
	else if (!strncmp (sub_path, X_("comment"), 7)) {
		if (!control_disabled && rt) {
			if (argc > (param_1)) {
				if (types[param_1] == 's') {
					rt->set_comment (strng, this);
					ret = 0;
				}
			} else {
				text_message (path, rt->comment (), get_address (msg));
				ret = 0;
			}
		}
	}
	else if (!strncmp (sub_path, X_("vca"), 3)) {
		boost::shared_ptr<Slavable> slv = boost::dynamic_pointer_cast<Slavable> (s);
		if (!control_disabled && slv) {
			if (argc > (param_1)) {
				string svalue = strng;
				string v_name = svalue.substr (0, svalue.rfind (" ["));
				boost::shared_ptr<VCA> vca = get_vca_by_name (v_name);
				uint32_t ivalue = 0;
				if (!strncmp (sub_path, X_("vca/toggle"), 10)) {
					if (vca) {
						if (s->slaved_to (vca)) {
							slv->unassign (vca);
						} else {
							slv->assign (vca);
						}
						ret = 0;
					}
				}
				else if (strcmp (sub_path, X_("vca")) == 0) {
					if (argc > (param_1 + 1)) {
						if (vca) {
							bool p_good = false;
							if (types[param_1 + 1] == 'i') {
								ivalue = argv[1]->i;
								p_good = true;
							} else if (types[1] == 'f') {
								ivalue = (uint32_t) argv[1]->f;
								p_good = true;
							}
							if (vca && p_good) {
								if (ivalue) {
									slv->assign (vca);
								} else {
									slv->unassign (vca);
								}
								ret = 0;
							} else {
								PBD::warning << "OSC: setting a vca needs both the vca name and it's state" << endmsg;
							}
						}
					}
				}
			} else {
				/// put list of VCAs this strip is controlled by
				_lo_lock.lock ();
				lo_message rmsg = lo_message_new ();
				if (param_1) {
					int sid = 0;
					if (types[0] == 'f') {
						sid = (int) argv[0]->f;
					} else if (types[0] == 'i') {
						sid = argv[0]->i;
					}
					lo_message_add_int32 (rmsg, sid);
				}
				StripableList stripables;
				session->get_stripables (stripables);
				for (StripableList::iterator it = stripables.begin(); it != stripables.end(); ++it) {
					boost::shared_ptr<Stripable> st = *it;
					boost::shared_ptr<VCA> v = boost::dynamic_pointer_cast<VCA> (st);
					if (v && s->slaved_to (v)) {
						lo_message_add_string (rmsg, v->name().c_str());
					}
				}
				lo_send_message (get_address (msg), path, rmsg);
				lo_message_free (rmsg);
				_lo_lock.unlock ();
				ret = 0;
			}
		}
	}


	if (ret) {
		int sid = 0;
		_lo_lock.lock ();
		lo_message rmsg = lo_message_new ();
		if (param_1) {
			if (types[0] == 'f') {
				sid = (int) argv[0]->f;
			} else if (types[0] == 'i') {
				sid = argv[0]->i;
			}
			lo_message_add_int32 (rmsg, sid);
		}
		if (types[param_1] == 'f') {
			if (!strncmp (sub_path, X_("gain"), 4)) {
				lo_message_add_float (rmsg, -200);
			} else {
				lo_message_add_float (rmsg, 0);
			}
		} else if (types[param_1] == 'i') {
			lo_message_add_int32 (rmsg, 0);
		} else if (types[param_1] == 's') {
			//lo_message_add_string (rmsg, val.c_str());
			lo_message_add_string (rmsg, " ");
		}
		lo_send_message (get_address (msg), path, rmsg);
		lo_message_free (rmsg);
		_lo_lock.unlock ();
	}

	return ret;

	/*
	 * for reference

		REGISTER_CALLBACK (serv, X_("/strip/sends"), "i", route_get_sends);
		REGISTER_CALLBACK (serv, X_("/strip/send/gain"), "iif", route_set_send_gain_dB);
		REGISTER_CALLBACK (serv, X_("/strip/send/fader"), "iif", route_set_send_fader);
		REGISTER_CALLBACK (serv, X_("/strip/send/enable"), "iif", route_set_send_enable);
		REGISTER_CALLBACK (serv, X_("/strip/receives"), "i", route_get_receives);

		REGISTER_CALLBACK (serv, X_("/strip/plugin/list"), "i", route_plugin_list);
		REGISTER_CALLBACK (serv, X_("/strip/plugin/parameter"), "iiif", route_plugin_parameter);
		// prints to cerr only
		REGISTER_CALLBACK (serv, X_("/strip/plugin/parameter/print"), "iii", route_plugin_parameter_print);
		REGISTER_CALLBACK (serv, X_("/strip/plugin/activate"), "ii", route_plugin_activate);
		REGISTER_CALLBACK (serv, X_("/strip/plugin/deactivate"), "ii", route_plugin_deactivate);
		REGISTER_CALLBACK (serv, X_("/strip/plugin/descriptor"), "ii", route_plugin_descriptor);
		REGISTER_CALLBACK (serv, X_("/strip/plugin/reset"), "ii", route_plugin_reset);
	*/

}

int
OSC::strip_state (const char *path, boost::shared_ptr<ARDOUR::Stripable> s, int ssid, lo_message msg)
{
	PBD::info << string_compose("OSC: strip_state path:%1", path) << endmsg;
	// some things need the route
	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (s);

	lo_message reply = lo_message_new ();
	if (ssid) {
		// strip number not in path
		lo_message_add_int32 (reply, ssid);
	}

	if (boost::dynamic_pointer_cast<AudioTrack>(s)) {
		lo_message_add_string (reply, "AT");
	} else if (boost::dynamic_pointer_cast<MidiTrack>(s)) {
		lo_message_add_string (reply, "MT");
	} else if (boost::dynamic_pointer_cast<VCA>(s)) {
		lo_message_add_string (reply, "V");
	} else if (s->is_master()) {
		lo_message_add_string (reply, "MA");
	} else if (s->is_monitor()) {
		lo_message_add_string (reply, "MO");
	} else if (boost::dynamic_pointer_cast<Route>(s) && !boost::dynamic_pointer_cast<Track>(s)) {
		if (!(s->presentation_info().flags() & PresentationInfo::MidiBus)) {
			if (s->is_foldbackbus()) {
				lo_message_add_string (reply, "FB");
			} else {
				lo_message_add_string (reply, "B");
			}
		} else {
			lo_message_add_string (reply, "MB");
		}
	}

	lo_message_add_string (reply, s->name().c_str());
	if (r) {
		// routes have inputs and outputs
		lo_message_add_int32 (reply, r->n_inputs().n_audio());
		lo_message_add_int32 (reply, r->n_outputs().n_audio());
	} else {
		// non-routes like VCAs don't
		lo_message_add_int32 (reply, -1);
		lo_message_add_int32 (reply, -1);
	}
	if (s->mute_control()) {
		lo_message_add_int32 (reply, s->mute_control()->get_value());
	} else {
		lo_message_add_int32 (reply, -1);
	}
	if (s->solo_control()) {
		lo_message_add_int32 (reply, s->solo_control()->get_value());
	} else {
		lo_message_add_int32 (reply, -1);
	}
	if (s->rec_enable_control()) {
		lo_message_add_int32 (reply, s->rec_enable_control()->get_value());
	} else {
		lo_message_add_int32 (reply, -1);
	}
	lo_send_message (get_address (msg), X_(path), reply);
	lo_message_free (reply);
	return 0;
}

int
OSC::strip_list (lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg), true);
	string temppath = "/strip";
	int ssid = 0;
	for (int n = 0; n < (int) sur->nstrips; ++n) {
		if (sur->feedback[2]) {
			temppath = string_compose ("/strip/%1", n+1);
		} else {
			ssid = n + 1;
		}

		boost::shared_ptr<Stripable> s = get_strip (n + 1, get_address (msg));

		if (s) {
			strip_state (temppath.c_str(), s, ssid, msg);
		}
	}
	return 0;

}

int
OSC::set_automation (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg)
{
	if (!session) return -1;

	int ret = 1;
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> strp = boost::shared_ptr<Stripable>();
	uint32_t ctr = 0;
	uint32_t aut = 0;
	uint32_t ssid;
	boost::shared_ptr<Send> send = boost::shared_ptr<Send> ();

	if (argc) {
		if (types[argc - 1] == 'f') {
			aut = (int)argv[argc - 1]->f;
		} else {
			aut = argv[argc - 1]->i;
		}
	}

	//parse path first to find stripable
	if (!strncmp (path, X_("/strip/"), 7)) {
		// find ssid and stripable
		if (argc > 1) {
			if (types[1] == 'f') {
				ssid = (uint32_t)argv[0]->f;
			} else {
				ssid = argv[0]->i;
			}
			strp = get_strip (ssid, get_address (msg));
		} else {
			ssid = atoi (&(strrchr (path, '/' ))[1]);
			strp = get_strip (ssid, get_address (msg));
		}
		send = get_send (strp, get_address (msg));
		ctr = 7;
	} else if (!strncmp (path, X_("/select/"), 8)) {
		strp = sur->select;
		ctr = 8;
	} else {
		return ret;
	}
	if (strp) {
		boost::shared_ptr<AutomationControl> control = boost::shared_ptr<AutomationControl>();
		// other automatable controls can be added by repeating the next 6.5 lines
		if ((!strncmp (&path[ctr], X_("fader"), 5)) || (!strncmp (&path[ctr], X_("gain"), 4))) {
			if (send) {
				control = send->gain_control ();
			} else if (strp->gain_control ()) {
				control = strp->gain_control ();
			} else {
				PBD::warning << "No fader for this strip" << endmsg;
			}
		} else if (!strncmp (&path[ctr], X_("pan"), 3)) {
			if (send) {
				if (send->panner_linked_to_route () || !send->has_panner ()) {
					PBD::warning << "Send panner not available" << endmsg;
				} else {
					boost::shared_ptr<Delivery> _send_del = boost::dynamic_pointer_cast<Delivery> (send);
					boost::shared_ptr<Pannable> pannable = _send_del->panner()->pannable();
					if (pannable->pan_azimuth_control) {
						control = pannable->pan_azimuth_control;
					} else {
						PBD::warning << "Automation not available for " << path << endmsg;
					}
				}
			} else if (strp->pan_azimuth_control ()) {
					control = strp->pan_azimuth_control ();
			} else {
				PBD::warning << "Automation not available for " << path << endmsg;
			}

		} else if (!strncmp (&path[ctr], X_("trimdB"), 6)) {
			if (send) {
				PBD::warning << "Send trim not available" << endmsg;
			} else if (strp->trim_control ()) {
				control = strp->trim_control ();
			} else {
				PBD::warning << "No trim for this strip" << endmsg;
			}
		} else if (!strncmp (&path[ctr], X_("mute"), 4)) {
			if (send) {
				PBD::warning << "Send mute not automatable" << endmsg;
			} else if (strp->mute_control ()) {
				control = strp->mute_control ();
			} else {
				PBD::warning << "No trim for this strip" << endmsg;
			}

		} else {
			PBD::warning << "Automation not available for " << path << endmsg;
		}

		if (control) {

			switch (aut) {
				case 0:
					control->set_automation_state (ARDOUR::Off);
					ret = 0;
					break;
				case 1:
					control->set_automation_state (ARDOUR::Play);
					ret = 0;
					break;
				case 2:
					control->set_automation_state (ARDOUR::Write);
					ret = 0;
					break;
				case 3:
					control->set_automation_state (ARDOUR::Touch);
					ret = 0;
					break;
				case 4:
					control->set_automation_state (ARDOUR::Latch);
					ret = 0;
					break;
				default:
					break;
			}
		}
	}

	return ret;
}

int
OSC::touch_detect (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg)
{
	if (!session) return -1;

	int ret = 1;
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> strp = boost::shared_ptr<Stripable>();
	boost::shared_ptr<Send> send = boost::shared_ptr<Send> ();
	uint32_t ctr = 0;
	uint32_t touch = 0;
	uint32_t ssid;

	if (argc) {
		if (types[argc - 1] == 'f') {
			touch = (int)argv[argc - 1]->f;
		} else {
			touch = argv[argc - 1]->i;
		}
	}

	//parse path first to find stripable
	if (!strncmp (path, X_("/strip/"), 7)) {
		// find ssid and stripable
		if (argc > 1) {
			if (types[0] == 'f') {
				ssid = (uint32_t)argv[0]->f;
			} else {
				ssid = argv[0]->i;
			}
			strp = get_strip (ssid, get_address (msg));
		} else {
			ssid = atoi (&(strrchr (path, '/' ))[1]);
			strp = get_strip (ssid, get_address (msg));
		}
		send = get_send (strp, get_address (msg));
		ctr = 7;
	} else if (!strncmp (path, X_("/select/"), 8)) {
		strp = sur->select;
		ctr = 8;
	} else {
		return ret;
	}
	if (strp) {
		boost::shared_ptr<AutomationControl> control = boost::shared_ptr<AutomationControl>();
		// other automatable controls can be added by repeating the next 6.5 lines
		if ((!strncmp (&path[ctr], X_("fader"), 5)) || (!strncmp (&path[ctr], X_("gain"), 4))) {
			if (strp->gain_control ()) {
				control = strp->gain_control ();
			} else {
				PBD::warning << "No fader for this strip" << endmsg;
			}
			if (send) {
				control = send->gain_control ();
			}
		} else if (!strncmp (&path[ctr], X_("pan"), 3)) {
			if (send) {
				if (send->panner_linked_to_route () || !send->has_panner ()) {
					PBD::warning << "Send panner not available" << endmsg;
				} else {
					boost::shared_ptr<Delivery> _send_del = boost::dynamic_pointer_cast<Delivery> (send);
					boost::shared_ptr<Pannable> pannable = _send_del->panner()->pannable();
					if (!strncmp (&path[ctr], X_("pan_stereo_position"), 19)) {
						if (pannable->pan_azimuth_control) {
							control = pannable->pan_azimuth_control;
						} else {
							PBD::warning << "Automation not available for " << path << endmsg;
						}
					} else if (!strncmp (&path[ctr], X_("pan_stereo_width"), 16)) {
						if (strp->pan_width_control ()) {
								control = strp->pan_width_control ();
						} else {
							PBD::warning << "Automation not available for " << path << endmsg;
						}
					}
				}
			}
		} else if (!strncmp (&path[ctr], X_("trimdB"), 6)) {
			if (send) {
				PBD::warning << "Send trim not available" << endmsg;
			} else if (strp->trim_control ()) {
				control = strp->trim_control ();
			} else {
				PBD::warning << "No trim for this strip" << endmsg;
			}
		} else if (!strncmp (&path[ctr], X_("mute"), 4)) {
			if (send) {
				PBD::warning << "Send mute not automatable" << endmsg;
			} else if (strp->mute_control ()) {
				control = strp->mute_control ();
			} else {
				PBD::warning << "No trim for this strip" << endmsg;
			}

		} else {
			PBD::warning << "Automation not available for " << path << endmsg;
		}

		if (control) {
			if (touch) {
				//start touch
				control->start_touch (control->session().transport_sample());
				ret = 0;
			} else {
				// end touch
				control->stop_touch (control->session().transport_sample());
				ret = 0;
			}
			// just in case some crazy surface starts sending control values before touch
			FakeTouchMap::iterator x = _touch_timeout.find(control);
			if (x != _touch_timeout.end()) {
				_touch_timeout.erase (x);
			}
		}
	}

	return ret;
}

int
OSC::fake_touch (boost::shared_ptr<ARDOUR::AutomationControl> ctrl)
{
	if (ctrl) {
		//start touch
		if (ctrl->automation_state() == Touch && !ctrl->touching ()) {
		ctrl->start_touch (ctrl->session().transport_sample());
		_touch_timeout[ctrl] = 10;
		}
	}

	return 0;
}

int
OSC::spill (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg)
{
	/*
	 * spill should have the form of:
	 * /select/spill (may have i or f keypress/release)
	 * /strip/spill i (may have keypress and i may be inline)
	 */
	if (!session || argc > 1) return -1;

	int ret = 1;
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> strp = boost::shared_ptr<Stripable>();
	uint32_t value = 0;
	OSCTempMode new_mode = TempOff;

	if (argc) {
		if (types[0] == 'f') {
			value = (int)argv[0]->f;
		} else {
			value = argv[0]->i;
		}
		if (!value) {
			// key release ignore
			return 0;
		}
	}

	//parse path first to find stripable
	if (!strncmp (path, X_("/strip/"), 7)) {
		/*
		 * we don't know if value is press or ssid
		 * so we have to check if the last / has an int after it first
		 * if not then we use value
		 */
		uint32_t ssid = 0;
		ssid = atoi (&(strrchr (path, '/' ))[1]);
		if (!ssid) {
			ssid = value;
		}
		strp = get_strip (ssid, get_address (msg));
	} else if (!strncmp (path, X_("/select/"), 8)) {
		strp = sur->select;
	} else {
		return ret;
	}
	if (strp) {
		boost::shared_ptr<Route> rt = boost::dynamic_pointer_cast<Route> (strp);
		boost::shared_ptr<VCA> v = boost::dynamic_pointer_cast<VCA> (strp);
		if (strstr (path, X_("/vca")) || v) {
			//strp must be a VCA
			if (v) {
				new_mode = VCAOnly;
			} else {
				return ret;
			}
		} else
		if (strstr (path, X_("/group"))) {
			//strp must be in a group
			if (rt) {
				RouteGroup *rg = rt->route_group();
				if (rg) {
					new_mode = GroupOnly;
				} else {
					return ret;
				}
			}
		} else
		if (strstr (path, X_("/bus"))) {
			//strp must be a bus with either sends or no inputs
			if (rt) {
				if (!rt->is_track () && rt->can_solo ()) {
					new_mode = BusOnly;
				}
			}
		} else {
			// decide by auto
			// vca should never get here
			if (rt->is_track ()) {
				if (rt->route_group()) {
					new_mode = GroupOnly;
				}
			} else if (!rt->is_track () && rt->can_solo ()) {
						new_mode = BusOnly;
			}
		}
		if (new_mode) {
			sur->temp_mode = new_mode;
			sur->temp_master = strp;
			set_temp_mode (get_address (msg));
			set_bank (1, msg);
			return 0;
		}

	}
	return ret;
}

int
OSC::sel_new_personal_send (char *foldback, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	boost::shared_ptr<Route> rt = boost::shared_ptr<Route> ();
	if (s) {
		rt = boost::dynamic_pointer_cast<Route> (s);
		if (!rt) {
			PBD::warning << "OSC: can not send from VCAs." << endmsg;
			return -1;
		}
	}
	/* if a foldbackbus called foldback exists use it
	 * other wise create it. Then create a foldback send from
	 * this route to that bus.
	 */
	string foldbackbus = foldback;
	string foldback_name = foldbackbus;
	if (foldbackbus.find ("- FB") == string::npos) {
		foldback_name = string_compose ("%1 - FB", foldbackbus);
	}
	boost::shared_ptr<Route> lsn_rt = session->route_by_name (foldback_name);
	if (!lsn_rt) {
		// doesn't exist but check if raw name does and is foldbackbus
		boost::shared_ptr<Route> raw_rt = session->route_by_name (foldbackbus);
		if (raw_rt && raw_rt->is_foldbackbus()) {
			lsn_rt = raw_rt;
		} else {
			// create the foldbackbus
			RouteList list = session->new_audio_route (1, 1, 0, 1, foldback_name, PresentationInfo::FoldbackBus, (uint32_t) -1);
			lsn_rt = *(list.begin());
			lsn_rt->presentation_info().set_hidden (true);
			session->set_dirty();
		}
	}
	if (lsn_rt) {
		//boost::shared_ptr<Route> rt_send = ;
		if (rt && (lsn_rt != rt)) {
			// make sure there isn't one already
			bool s_only = true;
			if (!rt->feeds (lsn_rt, &s_only)) {
				// create send
				rt->add_foldback_send (lsn_rt, false);
				//boost::shared_ptr<Send> snd = rt->internal_send_for (aux);
				session->dirty ();
				return 0;
			} else {
				PBD::warning << "OSC: new_send - duplicate send, ignored." << endmsg;
			}
		} else {
			PBD::warning << "OSC: new_send - can't send to self." << endmsg;
		}
	} else {
		PBD::warning << "OSC: new_send - no FoldbackBus to send to." << endmsg;
	}

	return -1;
}

int
OSC::_strip_select (boost::shared_ptr<Stripable> s, lo_address addr)
{
	if (!session) {
		return -1;
	}
	OSCSurface *sur = get_surface(addr, true);
	return _strip_select2 (s, sur, addr);
}

int
OSC::_strip_select2 (boost::shared_ptr<Stripable> s, OSCSurface *sur, lo_address addr)
{
	// this allows get_surface  to call this part without calling itself
	boost::weak_ptr<Stripable> o_sel = sur->select;
	boost::shared_ptr<Stripable> old_sel= o_sel.lock ();
	boost::weak_ptr<Stripable> o_expand = sur->expand_strip;
	boost::shared_ptr<Stripable> old_expand= o_expand.lock ();

	// we got a null strip check that old strips are valid
	if (!s) {
		if (old_expand && sur->expand_enable) {
			sur->expand = get_sid (old_expand, addr);
			if (sur->strip_types[11] || sur->expand) {
				s = old_expand;
			} else {
				sur->expand_strip = boost::shared_ptr<Stripable> ();
			}
		}
	}
	if (!s) {
		sur->expand = 0;
		sur->expand_enable = false;
		if (ControlProtocol::first_selected_stripable()) {
			s = ControlProtocol::first_selected_stripable();
		} else {
			s = session->master_out ();
		}
		_select = s;
	}
	if (s != old_sel) {
		sur->select = s;
	}
	bool sends;
	uint32_t nsends  = 0;
	do {
		sends = false;
		if (s->send_level_controllable (nsends)) {
			sends = true;
			nsends++;
		}
	} while (sends);
	sur->nsends = nsends;

	s->DropReferences.connect (*this, MISSING_INVALIDATOR, boost::bind (&OSC::recalcbanks, this), this);

	OSCSelectObserver* so = dynamic_cast<OSCSelectObserver*>(sur->sel_obs);
	if (sur->feedback[13]) {
		if (so != 0) {
			so->refresh_strip (s, nsends, sur->gainmode, true);
		} else {
			OSCSelectObserver* sel_fb = new OSCSelectObserver (*this, *session, sur);
			sur->sel_obs = sel_fb;
		}
		sur->sel_obs->set_expand (sur->expand_enable);
	} else {
		if (so != 0) {
			delete so;
			sur->sel_obs = 0;
		}
	}
	if (sur->feedback[0] || sur->feedback[1]) {
		uint32_t obs_expand = 0;
		if (sur->expand_enable) {
			sur->expand = get_sid (s, addr);
			obs_expand = sur->expand;
		} else {
			obs_expand = 0;
		}
		for (uint32_t i = 0; i < sur->observers.size(); i++) {
			sur->observers[i]->set_expand (obs_expand);
		}
	}
	// need to set monitor for processor changed signal (for paging)
	string address = lo_address_get_url (addr);
	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route>(s);
	if (r) {
		r->processors_changed.connect  (sur->proc_connection, MISSING_INVALIDATOR, boost::bind (&OSC::processor_changed, this, address), this);
		_sel_plugin (sur->plugin_id, addr);
	}

	return 0;
}

void
OSC::processor_changed (string address)
{
	lo_address addr = lo_address_new_from_url (address.c_str());
	OSCSurface *sur = get_surface (addr);
	_sel_plugin (sur->plugin_id, addr);
	if (sur->sel_obs) {
		sur->sel_obs->renew_sends ();
		sur->sel_obs->eq_restart (-1);
	}
}

int
OSC::sel_expand (uint32_t state, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (!sur->expand_strip) {
		state = 0;
		float_message (X_("/select/expand"), 0.0, get_address (msg));
	}
	if (state) {
		sur->expand_enable = (bool) state;
		s = boost::shared_ptr<Stripable> ();
	} else {
		sur->expand_enable = false;
		s = boost::shared_ptr<Stripable> ();
	}

	return _strip_select (s, get_address (msg));
}

int
OSC::sel_previous (lo_message msg)
{
	return sel_delta (-1, msg);
}

int
OSC::sel_next (lo_message msg)
{
	return sel_delta (1, msg);
}

int
OSC::sel_delta (int delta, lo_message msg)
{
	if (!delta) {
		return 0;
	}
	OSCSurface *sur = get_surface(get_address (msg));
	Sorted sel_strips;
	sel_strips = sur->strips;
	// the current selected strip _should_ be in sel_strips
	uint32_t nstps = sel_strips.size ();
	if (!nstps) {
		return -1;
	}
	boost::shared_ptr<Stripable> new_sel = boost::shared_ptr<Stripable> ();
	boost::weak_ptr<Stripable> o_sel = sur->select;
	boost::shared_ptr<Stripable> old_sel= o_sel.lock ();
	for (uint32_t i = 0; i < nstps; i++) {
		if (old_sel == sel_strips[i]) {
			if (i && delta < 0) {
				// i is > 0 and delta is -1
				new_sel = sel_strips[i - 1];
			} else if ((i + 1) < nstps && delta > 0) {
				// i is at least 1 less than greatest and delta = 1
				new_sel = sel_strips[i + 1];
			} else if ((i + 1) >= nstps && delta > 0) {
				// i is greatest strip and delta 1
				new_sel = sel_strips[0];
			} else if (!i && delta < 0) {
				// i = 0 and delta -1
				new_sel = sel_strips[nstps - 1];
			} else {
				// should not happen
				return -1;
			}
		}
	}
	if (!new_sel) {
		// our selected strip has vanished use the first one
		new_sel = sel_strips[0];
	}
	if (new_sel) {
		if (!sur->expand_enable) {
			set_stripable_selection (new_sel);
		} else {
			sur->expand_strip = new_sel;
			_strip_select (new_sel, get_address (msg));
		}
		return 0;
	}
	return -1;
}

int
OSC::route_set_send_gain_dB (int ssid, int id, float val, lo_message msg)
{
	if (!session) {
		return -1;
	}
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
	OSCSurface *sur = get_surface(get_address (msg));
	float abs;
	if (s) {
		if (id > 0) {
			--id;
		}
		if (val < -192) {
			abs = 0;
		} else {
			abs = dB_to_coefficient (val);
		}
		if (s->send_level_controllable (id)) {
			s->send_level_controllable (id)->set_value (abs, sur->usegroup);
			return 0;
		}
	}
	return 0;
}

int
OSC::route_set_send_fader (int ssid, int id, float val, lo_message msg)
{
	if (!session) {
		return -1;
	}
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
	OSCSurface *sur = get_surface(get_address (msg));
	float abs;
	if (s) {

		if (id > 0) {
			--id;
		}

		if (s->send_level_controllable (id)) {
			abs = s->send_level_controllable(id)->interface_to_internal (val);
			s->send_level_controllable (id)->set_value (abs, sur->usegroup);
			return 0;
		}
	}
	return 0;
}

int
OSC::sel_sendgain (int id, float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	if (sur->send_page_size && (id > (int)sur->send_page_size)) {
		return float_message_with_id (X_("/select/send_gain"), id, -193, sur->feedback[2], get_address (msg));
	}
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	float abs;
	int send_id = 0;
	if (s) {
		if (id > 0) {
			send_id = id - 1;
		}
		if (val < -192) {
			abs = 0;
		} else {
			abs = dB_to_coefficient (val);
		}
		if (sur->send_page_size) {
			send_id = send_id + ((sur->send_page - 1) * sur->send_page_size);
		}
		if (s->send_level_controllable (send_id)) {
			s->send_level_controllable (send_id)->set_value (abs, PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message_with_id (X_("/select/send_gain"), id, -193, sur->feedback[2], get_address (msg));
}

int
OSC::sel_sendfader (int id, float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	if (sur->send_page_size && (id > (int)sur->send_page_size)) {
		return float_message_with_id (X_("/select/send_fader"), id, 0, sur->feedback[2], get_address (msg));
	}
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	float abs;
	int send_id = 0;
	if (s) {

		if (id > 0) {
			send_id = id - 1;
		}
		if (sur->send_page_size) {
			send_id = send_id + ((sur->send_page - 1) * sur->send_page_size);
		}

		if (s->send_level_controllable (send_id)) {
			abs = s->send_level_controllable(send_id)->interface_to_internal (val);
			s->send_level_controllable (send_id)->set_value (abs, PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message_with_id (X_("/select/send_fader"), id, 0, sur->feedback[2], get_address (msg));
}

int
OSC::route_set_send_enable (int ssid, int sid, float val, lo_message msg)
{
	if (!session) {
		return -1;
	}
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
	OSCSurface *sur = get_surface(get_address (msg));

	if (s) {

		/* revert to zero-based counting */

		if (sid > 0) {
			--sid;
		}

		if (s->send_enable_controllable (sid)) {
			s->send_enable_controllable (sid)->set_value (val, sur->usegroup);
			return 0;
		}

		if (s->send_level_controllable (sid)) {
			boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (s);
			if (!r) {
				return 0;
			}
			boost::shared_ptr<Send> snd = boost::dynamic_pointer_cast<Send> (r->nth_send(sid));
			if (snd) {
				if (val) {
					snd->activate();
				} else {
					snd->deactivate();
				}
			}
			return 0;
		}

	}

	return -1;
}

int
OSC::sel_sendenable (int id, float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	if (sur->send_page_size && (id > (int)sur->send_page_size)) {
		return float_message_with_id (X_("/select/send_enable"), id, 0, sur->feedback[2], get_address (msg));
	}
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	int send_id = 0;
	if (s) {
		if (id > 0) {
			send_id = id - 1;
		}
		if (sur->send_page_size) {
			send_id = send_id + ((sur->send_page - 1) * sur->send_page_size);
		}
		if (s->send_enable_controllable (send_id)) {
			s->send_enable_controllable (send_id)->set_value (val, PBD::Controllable::NoGroup);
			return 0;
		}
		if (s->send_level_controllable (send_id)) {
			boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (s);
			if (!r) {
				// should never get here
				return float_message_with_id (X_("/select/send_enable"), id, 0, sur->feedback[2], get_address (msg));
			}
			boost::shared_ptr<Send> snd = boost::dynamic_pointer_cast<Send> (r->nth_send(send_id));
			if (snd) {
				if (val) {
					snd->activate();
				} else {
					snd->deactivate();
				}
			}
			return 0;
		}
	}
	return float_message_with_id (X_("/select/send_enable"), id, 0, sur->feedback[2], get_address (msg));
}

int
OSC::sel_master_send_enable (int state, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	if (s) {
		if (s->master_send_enable_controllable ()) {
			s->master_send_enable_controllable()->set_value (state, PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message (X_("/select/master_send_enable"), 0, get_address(msg));
}

int
OSC::select_plugin_parameter (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg) {
	OSCSurface *sur = get_surface(get_address (msg));
	int paid;
	uint32_t piid = sur->plugin_id;
	float value = 0;
	if (argc > 1) {
		// no inline args
		if (argc == 2) {
			// change parameter in already selected plugin
			if (types[0]  == 'f') {
				paid = (int) argv[0]->f;
			} else {
				paid = argv[0]->i;
			}
			value = argv[1]->f;
		} else if (argc == 3) {
			if (types[0] == 'f') {
				piid = (int) argv[0]->f;
			} else {
				piid = argv[0]->i;
			}
			_sel_plugin (piid, get_address (msg));
			if (types[1] == 'f') {
				paid = (int) argv[1]->f;
			} else {
				paid = argv[1]->i;
			}
			value = argv[2]->f;
		} else if (argc > 3) {
			PBD::warning << "OSC: Too many parameters: " << argc << endmsg;
			return -1;
		}
	} else if (argc) {
		const char * par = strstr (&path[25], "/");
		if (par) {
			piid = atoi (&path[25]);
			_sel_plugin (piid, get_address (msg));
			paid = atoi (&par[1]);
			value = argv[0]->f;
			// we have plugin id too
		} else {
			// just parameter
			paid = atoi (&path[25]);
			value = argv[0]->f;
		}
	} else {
		PBD::warning << "OSC: Must have parameters." << endmsg;
		return -1;
	}
	if (!piid || piid > sur->plugins.size ()) {
		return float_message_with_id (X_("/select/plugin/parameter"), paid, 0, sur->feedback[2], get_address (msg));
	}
	if (sur->plug_page_size && (paid > (int)sur->plug_page_size)) {
		return float_message_with_id (X_("/select/plugin/parameter"), paid, 0, sur->feedback[2], get_address (msg));
	}
	boost::shared_ptr<Stripable> s = sur->select;
	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route>(s);
	if (!r) {
		return 1;
	}

	boost::shared_ptr<Processor> proc = r->nth_plugin (sur->plugins[sur->plugin_id - 1]);
	boost::shared_ptr<PluginInsert> pi;
	if (!(pi = boost::dynamic_pointer_cast<PluginInsert>(proc))) {
		return 1;
	}
	boost::shared_ptr<ARDOUR::Plugin> pip = pi->plugin();
	// paid is paged parameter convert to absolute
	int parid = paid + (int)sur->plug_page - 1;
	if (parid > (int) sur->plug_params.size ()) {
		if (sur->feedback[13]) {
			float_message_with_id (X_("/select/plugin/parameter"), paid, 0, sur->feedback[2], get_address (msg));
		}
		return 0;
	}

	bool ok = false;
	uint32_t controlid = pip->nth_parameter(sur->plug_params[parid - 1], ok);
	if (!ok) {
		return 1;
	}
	ParameterDescriptor pd;
	pip->get_parameter_descriptor(controlid, pd);
	if ( pip->parameter_is_input(controlid) || pip->parameter_is_control(controlid) ) {
		boost::shared_ptr<AutomationControl> c = pi->automation_control(Evoral::Parameter(PluginAutomation, 0, controlid));
		if (c) {
			if (pd.integer_step && pd.upper == 1) {
				if (c->get_value () && value < 1.0) {
					c->set_value (0, PBD::Controllable::NoGroup);
				} else if (!c->get_value () && value) {
					c->set_value (1, PBD::Controllable::NoGroup);
				}
			} else {
				c->set_value (c->interface_to_internal (value), PBD::Controllable::NoGroup);
			}
			return 0;
		}
	}
	return 1;
}

int
OSC::sel_plugin_activate (float state, lo_message msg)
{
	if (!session) {
		return -1;
	}
	OSCSurface *sur = get_surface(get_address (msg));
	if (sur->plugins.size() > 0) {
		boost::shared_ptr<Stripable> s = sur->select;

		boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (s);

		if (r) {
			boost::shared_ptr<Processor> redi=r->nth_plugin (sur->plugins[sur->plugin_id -1]);
			if (redi) {
				boost::shared_ptr<PluginInsert> pi;
				if ((pi = boost::dynamic_pointer_cast<PluginInsert>(redi))) {
					if(state > 0) {
						pi->activate();
					} else {
						pi->deactivate();
					}
					return 0;
				}
			}
		}
	}
	float_message (X_("/select/plugin/activate"), 0, get_address (msg));
	PBD::warning << "OSC: Select has no Plugin." << endmsg;
	return 0;
}

int
OSC::route_plugin_list (int ssid, lo_message msg) {
	if (!session) {
		return -1;
	}

	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route>(get_strip (ssid, get_address (msg)));

	if (!r) {
		PBD::error << "OSC: Invalid Remote Control ID '" << ssid << "'" << endmsg;
		return -1;
	}
	int piid = 0;

	lo_message reply = lo_message_new ();
	lo_message_add_int32 (reply, ssid);


	for (;;) {
		boost::shared_ptr<Processor> redi = r->nth_plugin(piid);
		if ( !redi ) {
			break;
		}

		boost::shared_ptr<PluginInsert> pi;

		if (!(pi = boost::dynamic_pointer_cast<PluginInsert>(redi))) {
			PBD::error << "OSC: given processor # " << piid << " on RID '" << ssid << "' is not a Plugin." << endmsg;
			continue;
		}
		lo_message_add_int32 (reply, piid + 1);

		boost::shared_ptr<ARDOUR::Plugin> pip = pi->plugin();
		lo_message_add_string (reply, pip->name());
		lo_message_add_int32(reply, redi->enabled() ? 1 : 0);

		piid++;
	}

	lo_send_message (get_address (msg), X_("/strip/plugin/list"), reply);
	lo_message_free (reply);
	return 0;
}

int
OSC::route_plugin_descriptor (int ssid, int piid, lo_message msg) {
	if (!session) {
		return -1;
	}

	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route>(get_strip (ssid, get_address (msg)));

	if (!r) {
		PBD::error << "OSC: Invalid Remote Control ID '" << ssid << "'" << endmsg;
		return -1;
	}

	boost::shared_ptr<Processor> redi = r->nth_plugin(piid - 1);

	if (!redi) {
		PBD::error << "OSC: cannot find plugin # " << piid << " for RID '" << ssid << "'" << endmsg;
		return -1;
	}

	boost::shared_ptr<PluginInsert> pi;

	if (!(pi = boost::dynamic_pointer_cast<PluginInsert>(redi))) {
		PBD::error << "OSC: given processor # " << piid << " on RID '" << ssid << "' is not a Plugin." << endmsg;
		return -1;
	}

	boost::shared_ptr<ARDOUR::Plugin> pip = pi->plugin();
	bool ok = false;

	for ( uint32_t ppi = 0; ppi < pip->parameter_count(); ppi++) {

		uint32_t controlid = pip->nth_parameter(ppi, ok);
		if (!ok) {
			continue;
		}
		boost::shared_ptr<AutomationControl> c = pi->automation_control(Evoral::Parameter(PluginAutomation, 0, controlid));

		lo_message reply = lo_message_new();
		lo_message_add_int32 (reply, ssid);
		lo_message_add_int32 (reply, piid);

		lo_message_add_int32 (reply, ppi + 1);
		ParameterDescriptor pd;
		pi->plugin()->get_parameter_descriptor(controlid, pd);
		lo_message_add_string (reply, pd.label.c_str());

		// I've combined those binary descriptor parts in a bit-field to reduce lilo message elements
		int flags = 0;
		flags |= pd.enumeration ? 1 : 0;
		flags |= pd.integer_step ? 2 : 0;
		flags |= pd.logarithmic ? 4 : 0;
		flags |= pd.sr_dependent ? 32 : 0;
		flags |= pd.toggled ? 64 : 0;
		flags |= pip->parameter_is_input(controlid) ? 0x80 : 0;

		std::string param_desc = pi->plugin()->describe_parameter(Evoral::Parameter(PluginAutomation, 0, controlid));
		flags |= (param_desc == X_("hidden")) ? 0x100 : 0;
		lo_message_add_int32 (reply, flags);

		switch(pd.datatype) {
			case ARDOUR::Variant::BEATS:
				lo_message_add_string(reply, _("BEATS"));
				break;
			case ARDOUR::Variant::BOOL:
				lo_message_add_string(reply, _("BOOL"));
				break;
			case ARDOUR::Variant::DOUBLE:
				lo_message_add_string(reply, _("DOUBLE"));
				break;
			case ARDOUR::Variant::FLOAT:
				lo_message_add_string(reply, _("FLOAT"));
				break;
			case ARDOUR::Variant::INT:
				lo_message_add_string(reply, _("INT"));
				break;
			case ARDOUR::Variant::LONG:
				lo_message_add_string(reply, _("LONG"));
				break;
			case ARDOUR::Variant::NOTHING:
				lo_message_add_string(reply, _("NOTHING"));
				break;
			case ARDOUR::Variant::PATH:
				lo_message_add_string(reply, _("PATH"));
				break;
			case ARDOUR::Variant::STRING:
				lo_message_add_string(reply, _("STRING"));
				break;
			case ARDOUR::Variant::URI:
				lo_message_add_string(reply, _("URI"));
				break;
			default:
				lo_message_add_string(reply, _("UNKNOWN"));
				break;
		}
		lo_message_add_float (reply, pd.lower);
		lo_message_add_float (reply, pd.upper);
		lo_message_add_string (reply, pd.print_fmt.c_str());
		if ( pd.scale_points ) {
			lo_message_add_int32 (reply, pd.scale_points->size());
			for ( ARDOUR::ScalePoints::const_iterator i = pd.scale_points->begin(); i != pd.scale_points->end(); ++i) {
				lo_message_add_float (reply, i->second);
				lo_message_add_string (reply, ((std::string)i->first).c_str());
			}
		}
		else {
			lo_message_add_int32 (reply, 0);
		}
		if ( c ) {
			lo_message_add_double (reply, c->get_value());
		}
		else {
			lo_message_add_double (reply, 0);
		}

		lo_send_message (get_address (msg), X_("/strip/plugin/descriptor"), reply);
		lo_message_free (reply);
	}

	lo_message reply = lo_message_new ();
	lo_message_add_int32 (reply, ssid);
	lo_message_add_int32 (reply, piid);
	lo_send_message (get_address (msg), X_("/strip/plugin/descriptor_end"), reply);
	lo_message_free (reply);

	return 0;
}

int
OSC::route_plugin_reset (int ssid, int piid, lo_message msg) {
	if (!session) {
		return -1;
	}

	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route>(get_strip (ssid, get_address (msg)));

	if (!r) {
		PBD::error << "OSC: Invalid Remote Control ID '" << ssid << "'" << endmsg;
		return -1;
	}

	boost::shared_ptr<Processor> redi = r->nth_plugin(piid - 1);

	if (!redi) {
		PBD::error << "OSC: cannot find plugin # " << piid << " for RID '" << ssid << "'" << endmsg;
		return -1;
	}

	boost::shared_ptr<PluginInsert> pi;

	if (!(pi = boost::dynamic_pointer_cast<PluginInsert>(redi))) {
		PBD::error << "OSC: given processor # " << piid << " on RID '" << ssid << "' is not a Plugin." << endmsg;
		return -1;
	}

	pi->reset_parameters_to_default ();

	return 0;
}

int
OSC::route_plugin_parameter (int ssid, int piid, int par, float val, lo_message msg)
{
	if (!session)
		return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));

	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (s);

	if (!r) {
		PBD::error << "OSC: Invalid Remote Control ID '" << ssid << "'" << endmsg;
		return -1;
	}

	boost::shared_ptr<Processor> redi=r->nth_plugin (piid - 1);

	if (!redi) {
		PBD::error << "OSC: cannot find plugin # " << piid << " for RID '" << ssid << "'" << endmsg;
		return -1;
	}

	boost::shared_ptr<PluginInsert> pi;

	if (!(pi = boost::dynamic_pointer_cast<PluginInsert>(redi))) {
		PBD::error << "OSC: given processor # " << piid << " on RID '" << ssid << "' is not a Plugin." << endmsg;
		return -1;
	}

	boost::shared_ptr<ARDOUR::Plugin> pip = pi->plugin();
	bool ok=false;

	uint32_t controlid = pip->nth_parameter (par - 1,ok);

	if (!ok) {
		PBD::error << "OSC: Cannot find parameter # " << par <<  " for plugin # " << piid << " on RID '" << ssid << "'" << endmsg;
		return -1;
	}

	if (!pip->parameter_is_input(controlid)) {
		PBD::error << "OSC: Parameter # " << par <<  " for plugin # " << piid << " on RID '" << ssid << "' is not a control input" << endmsg;
		return -1;
	}

	ParameterDescriptor pd;
	pi->plugin()->get_parameter_descriptor (controlid,pd);

	if (val >= pd.lower && val <= pd.upper) {

		boost::shared_ptr<AutomationControl> c = pi->automation_control (Evoral::Parameter(PluginAutomation, 0, controlid));
		// cerr << "parameter:" << redi->describe_parameter(controlid) << " val:" << val << "\n";
		c->set_value (val, PBD::Controllable::NoGroup);
	} else {
		PBD::warning << "OSC: Parameter # " << par <<  " for plugin # " << piid << " on RID '" << ssid << "' is out of range" << endmsg;
		PBD::info << "OSC: Valid range min=" << pd.lower << " max=" << pd.upper << endmsg;
	}

	return 0;
}

//prints to cerr only
int
OSC::route_plugin_parameter_print (int ssid, int piid, int par, lo_message msg)
{
	if (!session) {
		return -1;
	}
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));

	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (s);

	if (!r) {
		return -1;
	}

	boost::shared_ptr<Processor> redi=r->nth_plugin (piid - 1);

	if (!redi) {
		return -1;
	}

	boost::shared_ptr<PluginInsert> pi;

	if (!(pi = boost::dynamic_pointer_cast<PluginInsert>(redi))) {
		return -1;
	}

	boost::shared_ptr<ARDOUR::Plugin> pip = pi->plugin();
	bool ok=false;

	uint32_t controlid = pip->nth_parameter (par - 1,ok);

	if (!ok) {
		return -1;
	}

	ParameterDescriptor pd;

	if (pi->plugin()->get_parameter_descriptor (controlid, pd) == 0) {
		boost::shared_ptr<AutomationControl> c = pi->automation_control (Evoral::Parameter(PluginAutomation, 0, controlid));

		cerr << "parameter:     " << pd.label  << "\n";
		if (c) {
			cerr << "current value: " << c->get_value () << "\n";
		} else {
			cerr << "current value not available, control does not exist\n";
		}
		cerr << "lower value:   " << pd.lower << "\n";
		cerr << "upper value:   " << pd.upper << "\n";
	}

	return 0;
}

int
OSC::route_plugin_activate (int ssid, int piid, lo_message msg)
{
	if (!session)
		return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, lo_message_get_source (msg));

	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (s);

	if (!r) {
		PBD::error << "OSC: Invalid Remote Control ID '" << ssid << "'" << endmsg;
		return -1;
	}

	boost::shared_ptr<Processor> redi=r->nth_plugin (piid - 1);

	if (!redi) {
		PBD::error << "OSC: cannot find plugin # " << piid << " for RID '" << ssid << "'" << endmsg;
		return -1;
	}

	boost::shared_ptr<PluginInsert> pi;

	if (!(pi = boost::dynamic_pointer_cast<PluginInsert>(redi))) {
		PBD::error << "OSC: given processor # " << piid << " on RID '" << ssid << "' is not a Plugin." << endmsg;
		return -1;
	}

	boost::shared_ptr<ARDOUR::Plugin> pip = pi->plugin();
	pi->activate();

	return 0;
}

int
OSC::route_plugin_deactivate (int ssid, int piid, lo_message msg)
{
	if (!session)
		return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, lo_message_get_source (msg));

	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (s);

	if (!r) {
		PBD::error << "OSC: Invalid Remote Control ID '" << ssid << "'" << endmsg;
		return -1;
	}

	boost::shared_ptr<Processor> redi=r->nth_plugin (piid - 1);

	if (!redi) {
		PBD::error << "OSC: cannot find plugin # " << piid << " for RID '" << ssid << "'" << endmsg;
		return -1;
	}

	boost::shared_ptr<PluginInsert> pi;

	if (!(pi = boost::dynamic_pointer_cast<PluginInsert>(redi))) {
		PBD::error << "OSC: given processor # " << piid << " on RID '" << ssid << "' is not a Plugin." << endmsg;
		return -1;
	}

	boost::shared_ptr<ARDOUR::Plugin> pip = pi->plugin();
	pi->deactivate();

	return 0;
}

// select

int
OSC::sel_pan_elevation (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	if (s) {
		if (s->pan_elevation_control()) {
			s->pan_elevation_control()->set_value (s->pan_elevation_control()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message(X_("/select/pan_elevation_position"), 0, get_address (msg));
}

int
OSC::sel_pan_frontback (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	if (s) {
		if (s->pan_frontback_control()) {
			s->pan_frontback_control()->set_value (s->pan_frontback_control()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message(X_("/select/pan_frontback_position"), 0.5, get_address (msg));
}

int
OSC::sel_pan_lfe (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	if (s) {
		if (s->pan_lfe_control()) {
			s->pan_lfe_control()->set_value (s->pan_lfe_control()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message(X_("/select/pan_lfe_control"), 0, get_address (msg));
}

// compressor control
int
OSC::sel_comp_enable (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	if (s) {
		if (s->comp_enable_controllable()) {
			s->comp_enable_controllable()->set_value (s->comp_enable_controllable()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message(X_("/select/comp_enable"), 0, get_address (msg));
}

int
OSC::sel_comp_threshold (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	if (s) {
		if (s->comp_threshold_controllable()) {
			s->comp_threshold_controllable()->set_value (s->comp_threshold_controllable()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message(X_("/select/comp_threshold"), 0, get_address (msg));
}

int
OSC::sel_comp_speed (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	if (s) {
		if (s->comp_speed_controllable()) {
			s->comp_speed_controllable()->set_value (s->comp_speed_controllable()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message(X_("/select/comp_speed"), 0, get_address (msg));
}

int
OSC::sel_comp_mode (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	if (s) {
		if (s->comp_mode_controllable()) {
			s->comp_mode_controllable()->set_value (s->comp_mode_controllable()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message(X_("/select/comp_mode"), 0, get_address (msg));
}

int
OSC::sel_comp_makeup (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	if (s) {
		if (s->comp_makeup_controllable()) {
			s->comp_makeup_controllable()->set_value (s->comp_makeup_controllable()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message(X_("/select/comp_makeup"), 0, get_address (msg));
}

// EQ control

int
OSC::sel_eq_enable (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	if (s) {
		if (s->eq_enable_controllable()) {
			s->eq_enable_controllable()->set_value (s->eq_enable_controllable()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message(X_("/select/eq_enable"), 0, get_address (msg));
}

int
OSC::sel_eq_hpf_freq (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	if (s) {
		if (s->filter_freq_controllable(true)) {
			s->filter_freq_controllable(true)->set_value (s->filter_freq_controllable(true)->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message(X_("/select/eq_hpf/freq"), 0, get_address (msg));
}

int
OSC::sel_eq_lpf_freq (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	if (s) {
		if (s->filter_freq_controllable(false)) {
			s->filter_freq_controllable(false)->set_value (s->filter_freq_controllable(false)->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message(X_("/select/eq_lpf/freq"), 0, get_address (msg));
}

int
OSC::sel_eq_hpf_enable (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	if (s) {
		if (s->filter_enable_controllable(true)) {
			s->filter_enable_controllable(true)->set_value (s->filter_enable_controllable(true)->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message(X_("/select/eq_hpf/enable"), 0, get_address (msg));
}

int
OSC::sel_eq_lpf_enable (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	if (s) {
		if (s->filter_enable_controllable(false)) {
			s->filter_enable_controllable(false)->set_value (s->filter_enable_controllable(false)->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message(X_("/select/eq_lpf/enable"), 0, get_address (msg));
}

int
OSC::sel_eq_hpf_slope (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	if (s) {
		if (s->filter_slope_controllable(true)) {
			s->filter_slope_controllable(true)->set_value (s->filter_slope_controllable(true)->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message(X_("/select/eq_hpf/slope"), 0, get_address (msg));
}

int
OSC::sel_eq_lpf_slope (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	if (s) {
		if (s->filter_slope_controllable(false)) {
			s->filter_slope_controllable(false)->set_value (s->filter_slope_controllable(false)->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message(X_("/select/eq_lpf/slope"), 0, get_address (msg));
}

int
OSC::sel_eq_gain (int id, float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	if (s) {
		if (id > 0) {
			--id;
		}
		if (s->eq_gain_controllable (id)) {
			s->eq_gain_controllable (id)->set_value (s->eq_gain_controllable(id)->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message_with_id (X_("/select/eq_gain"), id + 1, 0, sur->feedback[2], get_address (msg));
}

int
OSC::sel_eq_freq (int id, float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	if (s) {
		if (id > 0) {
			--id;
		}
		if (s->eq_freq_controllable (id)) {
			s->eq_freq_controllable (id)->set_value (s->eq_freq_controllable(id)->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message_with_id (X_("/select/eq_freq"), id + 1, 0, sur->feedback[2], get_address (msg));
}

int
OSC::sel_eq_q (int id, float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	if (s) {
		if (id > 0) {
			--id;
		}
		if (s->eq_q_controllable (id)) {
			s->eq_q_controllable (id)->set_value (s->eq_q_controllable(id)->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message_with_id (X_("/select/eq_q"), id + 1, 0, sur->feedback[2], get_address (msg));
}

int
OSC::sel_eq_shape (int id, float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	s = sur->select;
	if (s) {
		if (id > 0) {
			--id;
		}
		if (s->eq_shape_controllable (id)) {
			s->eq_shape_controllable (id)->set_value (s->eq_shape_controllable(id)->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message_with_id (X_("/select/eq_shape"), id + 1, 0, sur->feedback[2], get_address (msg));
}

// timer callbacks
bool
OSC::periodic (void)
{
	if (observer_busy) {
		return true;
	}
	if (!tick) {
		Glib::usleep(100); // let flurry of signals subside
		if (global_init) {
			for (uint32_t it = 0; it < _surface.size(); it++) {
				OSCSurface* sur = &_surface[it];
				global_feedback (sur);
			}
			global_init = false;
			tick = true;
		}
		if (bank_dirty) {
			_recalcbanks ();
			bank_dirty = false;
			tick = true;
		}
		return true;
	}

	if (scrub_speed != 0) {
		// for those jog wheels that don't have 0 on release (touch), time out.
		int64_t now = PBD::get_microseconds ();
		int64_t diff = now - scrub_time;
		if (diff > 120000) {
			scrub_speed = 0;
			// locate to the place PH was at last tick
			session->request_locate (scrub_place, MustStop);
		}
	}
	for (uint32_t it = 0; it < _surface.size(); it++) {
		OSCSurface* sur = &_surface[it];
		OSCSelectObserver* so;
		if ((so = dynamic_cast<OSCSelectObserver*>(sur->sel_obs)) != 0) {
			so->tick ();
		}
		OSCCueObserver* co;
		if ((co = dynamic_cast<OSCCueObserver*>(sur->cue_obs)) != 0) {
			co->tick ();
		}
		OSCGlobalObserver* go;
		if ((go = dynamic_cast<OSCGlobalObserver*>(sur->global_obs)) != 0) {
			go->tick ();
		}
		for (uint32_t i = 0; i < sur->observers.size(); i++) {
			OSCRouteObserver* ro;
			if ((ro = dynamic_cast<OSCRouteObserver*>(sur->observers[i])) != 0) {
				ro->tick ();
			}
		}

	}
	for (FakeTouchMap::iterator x = _touch_timeout.begin(); x != _touch_timeout.end();) {
		_touch_timeout[(*x).first] = (*x).second - 1;
		if (!(*x).second) {
			boost::shared_ptr<ARDOUR::AutomationControl> ctrl = (*x).first;
			// turn touch off
			ctrl->stop_touch (timepos_t (ctrl->session().transport_sample()));
			_touch_timeout.erase (x++);
		} else {
			x++;
		}
	}
	return true;
}

XMLNode&
OSC::get_state ()
{
	XMLNode& node (ControlProtocol::get_state());
	node.set_property (X_("debugmode"), (int32_t) _debugmode); // TODO: enum2str
	node.set_property (X_("address-only"), address_only);
	node.set_property (X_("remote-port"), remote_port);
	node.set_property (X_("banksize"), default_banksize);
	node.set_property (X_("striptypes"), default_strip);
	node.set_property (X_("feedback"), default_feedback);
	node.set_property (X_("gainmode"), default_gainmode);
	node.set_property (X_("send-page-size"), default_send_size);
	node.set_property (X_("plug-page-size"), default_plugin_size);
	return node;
}

int
OSC::set_state (const XMLNode& node, int version)
{
	if (ControlProtocol::set_state (node, version)) {
		return -1;
	}
	int32_t debugmode;
	if (node.get_property (X_("debugmode"), debugmode)) {
		_debugmode = OSCDebugMode (debugmode);
	}

	node.get_property (X_("address-only"), address_only);
	node.get_property (X_("remote-port"), remote_port);
	node.get_property (X_("banksize"), default_banksize);
	node.get_property (X_("striptypes"), default_strip);
	node.get_property (X_("feedback"), default_feedback);
	node.get_property (X_("gainmode"), default_gainmode);
	node.get_property (X_("send-page-size"), default_send_size);
	node.get_property (X_("plugin-page-size"), default_plugin_size);

	global_init = true;
	tick = false;

	return 0;
}

// predicate for sort call in get_sorted_stripables
struct StripableByPresentationOrder
{
	bool operator () (const boost::shared_ptr<Stripable> & a, const boost::shared_ptr<Stripable> & b) const
	{
		return a->presentation_info().order() < b->presentation_info().order();
	}

	bool operator () (const Stripable & a, const Stripable & b) const
	{
		return a.presentation_info().order() < b.presentation_info().order();
	}

	bool operator () (const Stripable * a, const Stripable * b) const
	{
		return a->presentation_info().order() < b->presentation_info().order();
	}
};

OSC::Sorted
OSC::get_sorted_stripables(std::bitset<32> types, bool cue, uint32_t custom, Sorted my_list)
{
	Sorted sorted;
	StripableList stripables;
	StripableList custom_list;

	// fetch all stripables
	session->get_stripables (stripables, PresentationInfo::AllStripables);
	if (custom) {
		uint32_t nstps = my_list.size ();
		// check each custom strip to see if it still exists
		boost::shared_ptr<Stripable> s;
		for (uint32_t i = 0; i < nstps; i++) {
			bool exists = false;
			s = my_list[i];
			for (StripableList::iterator it = stripables.begin(); it != stripables.end(); ++it) {
				boost::shared_ptr<Stripable> sl = *it;
				if (s == sl) {
					exists = true;
					break;
				}
			}
			if(!exists) {
				my_list[i] = boost::shared_ptr<Stripable>();
			} else {
				custom_list.push_back (s);
			}
		}
		if (custom == 1) {
			return my_list;
		} else {
			stripables = custom_list;
		}
	}
	// Look for stripables that match bit in sur->strip_types
	for (StripableList::iterator it = stripables.begin(); it != stripables.end(); ++it) {

		boost::shared_ptr<Stripable> s = *it;
		if (!s) {
			break;
		}
		if (custom == 2) {
			// banking off use all valid custom strips
			sorted.push_back (s);
		} else
		if ((!cue) && (!types[9]) && (s->presentation_info().flags() & PresentationInfo::Hidden)) {
			// do nothing... skip it
		} else if (types[8] && (s->is_selected())) {
			sorted.push_back (s);
		} else if (types[9] && (s->presentation_info().flags() & PresentationInfo::Hidden)) {
			sorted.push_back (s);
		} else if (s->is_master() || s->is_monitor() || s->is_auditioner()) {
			// do nothing for these either (we add them later)
		} else {
			if (types[0] && boost::dynamic_pointer_cast<AudioTrack>(s)) {
				sorted.push_back (s);
			} else if (types[1] && boost::dynamic_pointer_cast<MidiTrack>(s)) {
				sorted.push_back (s);
			} else if (types[4] && boost::dynamic_pointer_cast<VCA>(s)) {
				sorted.push_back (s);
			} else  if (s->is_foldbackbus()) {
				if (types[7]) {
					sorted.push_back (s);
				}
			} else
#ifdef MIXBUS
			if (types[2] && Profile->get_mixbus() && s->mixbus()) {
				sorted.push_back (s);
			} else
#endif
			if (boost::dynamic_pointer_cast<Route>(s) && !boost::dynamic_pointer_cast<Track>(s)) {
				boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route>(s);
				if (!(s->presentation_info().flags() & PresentationInfo::MidiBus)) {
					// note some older sessions will show midibuses as busses
					// this is a bus
					if (types[2]) {
						sorted.push_back (s);
					}
				} else if (types[3]) {
						sorted.push_back (s);
				}
			}
		}
	}
	if (!custom || (custom & 0x2)) {
		sort (sorted.begin(), sorted.end(), StripableByPresentationOrder());
	}
	if (!custom) {
		// Master/Monitor might be anywhere... we put them at the end - Sorry ;)
		if (types[5]) {
			if (session->master_out()) {
				sorted.push_back (session->master_out());
			}
		}
		if (types[6]) {
			if (session->monitor_out()) {
				sorted.push_back (session->monitor_out());
			}
		}
	}
	return sorted;
}

int
OSC::cue_parse (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg)
{
	OSCSurface *s = get_surface(get_address (msg), true);
	s->bank_size = 0;
	float value = 0;
	if (argc == 1) {
		if (types[0] == 'f') {
			value = argv[0]->f;
		} else if (types[0] == 'i') {
			value = (float) argv[0]->i;
		}
	}
	int ret = 1; /* unhandled */
	if (!strncmp (path, X_("/cue/bus"), 8) || !strncmp (path, X_("/cue/aux"), 8)) {
		// set our Foldback bus
		if (argc) {
			if (value) {
				ret = cue_set ((uint32_t) value, msg);
			} else {
				ret = 0;
			}
		}
	}
	else if (!strncmp (path, X_("/cue/connect_output"), 16) || !strncmp (path, X_("/cue/connect_aux"), 16)) {
		// connect Foldback bus output
		string dest = "";
		if (argc == 1 && types[0] == 's') {
			dest = &argv[0]->s;
			ret = cue_connect_aux (dest, msg);
		} else {
			PBD::warning << "OSC: connect_aux has wrong number or type of parameters." << endmsg;
		}
	}
	else if (!strncmp (path, X_("/cue/connect"), 12)) {
		// Connect to default Aux bus
		if ((!argc) || argv[0]->f || argv[0]->i) {
			ret = cue_set (1, msg);
		} else {
			ret = 0;
		}
	}
	else if (!strncmp (path, X_("/cue/new_bus"), 12) || !strncmp (path, X_("/cue/new_aux"), 12)) {
		// Create new Aux bus
		string name = "";
		string dest_1 = "";
		string dest_2 = "";
		if (argc == 3 && types[0] == 's' && types[1] == 's' && types[2] == 's') {
			name = &argv[0]->s;
			dest_1 = &argv[1]->s;
			dest_2 = &argv[2]->s;
			ret = cue_new_aux (name, dest_1, dest_2, 2, msg);
		} else if (argc == 2 && types[0] == 's' && types[1] == 's') {
			name = &argv[0]->s;
			dest_1 = &argv[1]->s;
			dest_2 = dest_1;
			ret = cue_new_aux (name, dest_1, dest_2, 1, msg);
		} else if (argc == 1 && types[0] == 's') {
			name = &argv[0]->s;
			ret = cue_new_aux (name, dest_1, dest_2, 1, msg);
		} else {
			PBD::warning << "OSC: new_aux has wrong number or type of parameters." << endmsg;
		}
	}
	else if (!strncmp (path, X_("/cue/new_send"), 13)) {
		// Create new send to Foldback
		string rt_name = "";
		if (argc == 1 && types[0] == 's') {
			rt_name = &argv[0]->s;
			ret = cue_new_send (rt_name, msg);
		} else {
			PBD::warning << "OSC: new_send has wrong number or type of parameters." << endmsg;
		}
	}
	else if (!strncmp (path, X_("/cue/next_bus"), 13) || !strncmp (path, X_("/cue/next_aux"), 13)) {
		// switch to next Foldback bus
		if ((!argc) || argv[0]->f || argv[0]->i) {
			ret = cue_next (msg);
		} else {
			ret = 0;
		}
	}
	else if (!strncmp (path, X_("/cue/previous_bus"), 17) || !strncmp (path, X_("/cue/previous_aux"), 17)) {
		// switch to previous Foldback bus
		if ((!argc) || argv[0]->f || argv[0]->i) {
			ret = cue_previous (msg);
		} else {
			ret = 0;
		}
	}
	else if (!strncmp (path, X_("/cue/send/fader/"), 16) && strlen (path) > 16) {
		if (argc == 1) {
			int id = atoi (&path[16]);
			ret = cue_send_fader (id, value, msg);
		}
	}
	else if (!strncmp (path, X_("/cue/send/enable/"), 17) && strlen (path) > 17) {
		if (argc == 1) {
			int id = atoi (&path[17]);
			ret = cue_send_enable (id, value, msg);
		}
	}
	else if (!strncmp (path, X_("/cue/fader"), 10)) {
		if (argc == 1) {
			ret = cue_aux_fader (value, msg);
		}
	}
	else if (!strncmp (path, X_("/cue/mute"), 9)) {
		if (argc == 1) {
			ret = cue_aux_mute (value, msg);
		}
	}

	return ret;
}

int
OSC::cue_set (uint32_t aux, lo_message msg)
{
	return _cue_set (aux, get_address (msg));
}

int
OSC::_cue_set (uint32_t aux, lo_address addr)
{
	int ret = 1;
	OSCSurface *s = get_surface(addr, true);
	s->bank_size = 0;
	s->strip_types = 128;
	s->feedback = 0;
	s->gainmode = 1;
	s->cue = true;
	s->strips = get_sorted_stripables(s->strip_types, s->cue, false, s->custom_strips);

	s->nstrips = s->strips.size();
	if (!s->nstrips) {
		surface_destroy (s);
		return 0;
	}
	if (aux < 1) {
		aux = 1;
	} else if (aux > s->nstrips) {
		aux = s->nstrips;
	}
	s->aux = aux;
	// get a list of Auxes
	for (uint32_t n = 0; n < s->nstrips; ++n) {
		boost::shared_ptr<Stripable> stp = s->strips[n];
		if (stp) {
			text_message (string_compose (X_("/cue/name/%1"), n+1), stp->name(), addr);
			if (aux == n+1) {
				// aux must be at least one

				stp->DropReferences.connect (*this, MISSING_INVALIDATOR, boost::bind (&OSC::_cue_set, this, aux, addr), this);
				// make a list of stripables with sends that go to this bus
				s->sends = cue_get_sorted_stripables(stp, aux, addr);
				if (s->cue_obs) {
					s->cue_obs->refresh_strip (stp, s->sends, true);
				} else {
					// start cue observer
					OSCCueObserver* co = new OSCCueObserver (*this, s);
					s->cue_obs = co;
				}
				ret = 0;
			}

		}
	}

	return ret;
}

int
OSC::cue_new_aux (string name, string dest_1, string dest_2, uint32_t count, lo_message msg)
{
	// create a new bus named name - monitor
	RouteList list;
	boost::shared_ptr<Stripable> aux;
	name = string_compose ("%1 - FB", name);
	list = session->new_audio_route (count, count, 0, 1, name, PresentationInfo::FoldbackBus, (uint32_t) -1);
	aux = *(list.begin());
	if (aux) {
		boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route>(aux);
		if (dest_1.size()) {
			PortSet& ports = r->output()->ports ();
			if (atoi( dest_1.c_str())) {
				dest_1 = string_compose ("system:playback_%1", dest_1);
			}
			r->output ()->connect (*(ports.begin()), dest_1, this);
			if (count == 2) {
				if (atoi( dest_2.c_str())) {
					dest_2 = string_compose ("system:playback_%1", dest_2);
				}
				PortSet::iterator i = ports.begin();
				++i;
				r->output ()->connect (*(i), dest_2, this);
			}
		}
		cue_set ((uint32_t) -1, msg);
		session->set_dirty();
		return 0;
	}
	return -1;
}

int
OSC::cue_new_send (string rt_name, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg), true);
	if (sur->cue) {
		boost::shared_ptr<Route> aux = boost::dynamic_pointer_cast<Route> (get_strip (sur->aux, get_address(msg)));
		if (aux) {
			boost::shared_ptr<Route> rt_send = session->route_by_name (rt_name);
			if (rt_send && (aux != rt_send)) {
				// make sure there isn't one already
				bool s_only = true;
				if (!rt_send->feeds (aux, &s_only)) {
					// create send
					rt_send->add_foldback_send (aux, false);
					boost::shared_ptr<Send> snd = rt_send->internal_send_for (aux);
					session->dirty ();
					return 0;
				} else {
					PBD::warning << "OSC: new_send - duplicate send, ignored." << endmsg;
				}
			} else {
				PBD::warning << "OSC: new_send - route doesn't exist or is aux." << endmsg;
			}
		} else {
			PBD::warning << "OSC: new_send - No Aux to send to." << endmsg;
		}
	} else {
		PBD::warning << "OSC: new_send - monitoring not set, select aux first." << endmsg;
	}
	return 1;
}

int
OSC::cue_connect_aux (std::string dest, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg), true);
	int ret = 1;
	if (sur->cue) {
		boost::shared_ptr<Route> rt = boost::dynamic_pointer_cast<Route> (get_strip (sur->aux, get_address(msg)));
		if (rt) {
			if (dest.size()) {
				rt->output()->disconnect (this);
				if (atoi( dest.c_str())) {
					dest = string_compose ("system:playback_%1", dest);
				}
				PortSet& ports = rt->output()->ports ();
				rt->output ()->connect (*(ports.begin()), dest, this);
				session->set_dirty();
				ret = 0;
			}
		}
	}
	if (ret) {
		PBD::warning << "OSC: cannot connect, no Aux bus chosen." << endmsg;
	}
	return ret;
}

int
OSC::cue_next (lo_message msg)
{
	OSCSurface *s = get_surface(get_address (msg), true);
	int ret = 1;

	if (!s->cue) {
		ret = cue_set (1, msg);
	}
	if (s->aux < s->nstrips) {
		ret = cue_set (s->aux + 1, msg);
	} else {
		ret = cue_set (s->nstrips, msg);
	}
	return ret;
}

int
OSC::cue_previous (lo_message msg)
{
	OSCSurface *s = get_surface(get_address (msg), true);
	int ret = 1;
	if (!s->cue) {
		ret = cue_set (1, msg);
	}
	if (s->aux > 1) {
		ret = cue_set (s->aux - 1, msg);
	} else {
		ret = cue_set (1, msg);
	}
	return ret;
}

boost::shared_ptr<Send>
OSC::cue_get_send (uint32_t id, lo_address addr)
{
	OSCSurface *s = get_surface(addr, true);
	if (id && s->aux > 0 && id <= s->sends.size()) {
		boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (s->sends[id - 1]);
		boost::shared_ptr<Stripable> aux = get_strip (s->aux, addr);
		if (r && aux) {
			return r->internal_send_for (boost::dynamic_pointer_cast<Route> (aux));
		}
	}
	return boost::shared_ptr<Send>();

}

int
OSC::cue_aux_fader (float position, lo_message msg)
{
	if (!session) return -1;

	OSCSurface *sur = get_surface(get_address (msg), true);
	if (sur->cue) {
		if (sur->aux) {
			boost::shared_ptr<Stripable> s = get_strip (sur->aux, get_address (msg));

			if (s) {
				if (s->gain_control()) {
					s->gain_control()->set_value (s->gain_control()->interface_to_internal (position), PBD::Controllable::NoGroup);
					return 0;
				}
			}
		}
	}
	float_message (X_("/cue/fader"), 0, get_address (msg));
	return -1;
}

int
OSC::cue_aux_mute (float state, lo_message msg)
{
	if (!session) return -1;

	OSCSurface *sur = get_surface(get_address (msg), true);
	if (sur->cue) {
		if (sur->aux) {
			boost::shared_ptr<Stripable> s = get_strip (sur->aux, get_address (msg));
			if (s) {
				if (s->mute_control()) {
					s->mute_control()->set_value (state ? 1.0 : 0.0, PBD::Controllable::NoGroup);
					return 0;
				}
			}
		}
	}
	float_message (X_("/cue/mute"), 0, get_address (msg));
	return -1;
}

int
OSC::cue_send_fader (uint32_t id, float val, lo_message msg)
{
	if (!session) {
		return -1;
	}
	boost::shared_ptr<Send> s = cue_get_send (id, get_address (msg));
	if (s) {
		if (s->gain_control()) {
			s->gain_control()->set_value (s->gain_control()->interface_to_internal(val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	float_message (string_compose (X_("/cue/send/fader/%1"), id), 0, get_address (msg));
	return -1;
}

int
OSC::cue_send_enable (uint32_t id, float state, lo_message msg)
{
	if (!session)
		return -1;
	boost::shared_ptr<Send> s = cue_get_send (id, get_address (msg));
	if (s) {
		if (state) {
			s->activate ();
		} else {
			s->deactivate ();
		}
		return 0;
	}
	float_message (string_compose (X_("/cue/send/enable/%1"), id), 0, get_address (msg));
	return -1;
}

// generic send message
int
OSC::float_message (string path, float val, lo_address addr)
{
	_lo_lock.lock ();

	lo_message reply;
	reply = lo_message_new ();
	lo_message_add_float (reply, (float) val);

	lo_send_message (addr, path.c_str(), reply);
	Glib::usleep(1);
	lo_message_free (reply);
	_lo_lock.unlock ();

	return 0;
}

int
OSC::float_message_with_id (std::string path, uint32_t ssid, float value, bool in_line, lo_address addr)
{
	_lo_lock.lock ();
	lo_message msg = lo_message_new ();
	if (in_line) {
		path = string_compose ("%1/%2", path, ssid);
	} else {
		lo_message_add_int32 (msg, ssid);
	}
	lo_message_add_float (msg, value);

	lo_send_message (addr, path.c_str(), msg);
	Glib::usleep(1);
	lo_message_free (msg);
	_lo_lock.unlock ();
	return 0;
}

int
OSC::int_message (string path, int val, lo_address addr)
{
	_lo_lock.lock ();

	lo_message reply;
	reply = lo_message_new ();
	lo_message_add_int32 (reply, (float) val);

	lo_send_message (addr, path.c_str(), reply);
	Glib::usleep(1);
	lo_message_free (reply);
	_lo_lock.unlock ();

	return 0;
}

int
OSC::int_message_with_id (std::string path, uint32_t ssid, int value, bool in_line, lo_address addr)
{
	_lo_lock.lock ();
	lo_message msg = lo_message_new ();
	if (in_line) {
		path = string_compose ("%1/%2", path, ssid);
	} else {
		lo_message_add_int32 (msg, ssid);
	}
	lo_message_add_int32 (msg, value);

	lo_send_message (addr, path.c_str(), msg);
	Glib::usleep(1);
	lo_message_free (msg);
	_lo_lock.unlock ();
	return 0;
}

int
OSC::text_message (string path, string val, lo_address addr)
{
	_lo_lock.lock ();

	lo_message reply;
	reply = lo_message_new ();
	lo_message_add_string (reply, val.c_str());

	lo_send_message (addr, path.c_str(), reply);
	Glib::usleep(1);
	lo_message_free (reply);
	_lo_lock.unlock ();

	return 0;
}

int
OSC::text_message_with_id (std::string path, uint32_t ssid, std::string val, bool in_line, lo_address addr)
{
	_lo_lock.lock ();
	lo_message msg = lo_message_new ();
	if (in_line) {
		path = string_compose ("%1/%2", path, ssid);
	} else {
		lo_message_add_int32 (msg, ssid);
	}

	lo_message_add_string (msg, val.c_str());

	lo_send_message (addr, path.c_str(), msg);
	Glib::usleep(1);
	lo_message_free (msg);
	_lo_lock.unlock ();
	return 0;
}

// we have to have a sorted list of stripables that have sends pointed at our aux
// we can use the one in osc.cc to get an aux list
OSC::Sorted
OSC::cue_get_sorted_stripables(boost::shared_ptr<Stripable> aux, uint32_t id, lo_address addr)
{
	Sorted sorted;

	boost::shared_ptr<Route> aux_rt = boost::dynamic_pointer_cast<Route> (aux);
	Route::FedBy fed_by = aux_rt->fed_by();
	for (Route::FedBy::iterator i = fed_by.begin(); i != fed_by.end(); ++i) {
		if (i->sends_only) {
			boost::shared_ptr<Stripable> s (i->r.lock());
			sorted.push_back (s);
			s->DropReferences.connect (*this, MISSING_INVALIDATOR, boost::bind (&OSC::_cue_set, this, id, addr), this);
		}
	}
	sort (sorted.begin(), sorted.end(), StripableByPresentationOrder());

	return sorted;
}

