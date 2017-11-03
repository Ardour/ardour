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

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <algorithm>

#include <unistd.h>
#include <fcntl.h>

#include "pbd/gstdio_compat.h"
#include <glibmm.h>

#include "pbd/control_math.h"
#include <pbd/convert.h>
#include <pbd/pthread_utils.h>
#include <pbd/file_utils.h>
#include <pbd/failed_constructor.h>

#include "ardour/amp.h"
#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"
#include "ardour/vca.h"
#include "ardour/monitor_control.h"
#include "ardour/dB.h"
#include "ardour/filesystem_paths.h"
#include "ardour/panner.h"
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
	, default_strip (159)
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
		co->clear_observer ();
		delete co;
		sur->cue_obs = 0;
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

		REGISTER_CALLBACK (serv, "/refresh", "", refresh_surface);
		REGISTER_CALLBACK (serv, "/refresh", "f", refresh_surface);
		REGISTER_CALLBACK (serv, "/strip/list", "", routes_list);
		REGISTER_CALLBACK (serv, "/strip/list", "f", routes_list);
		REGISTER_CALLBACK (serv, "/surface/list", "", surface_list);
		REGISTER_CALLBACK (serv, "/surface/list", "f", surface_list);
		REGISTER_CALLBACK (serv, "/add_marker", "", add_marker);
		REGISTER_CALLBACK (serv, "/add_marker", "f", add_marker);
		REGISTER_CALLBACK (serv, "/access_action", "s", access_action);
		REGISTER_CALLBACK (serv, "/loop_toggle", "", loop_toggle);
		REGISTER_CALLBACK (serv, "/loop_toggle", "f", loop_toggle);
		REGISTER_CALLBACK (serv, "/loop_location", "ii", loop_location);
		REGISTER_CALLBACK (serv, "/goto_start", "", goto_start);
		REGISTER_CALLBACK (serv, "/goto_start", "f", goto_start);
		REGISTER_CALLBACK (serv, "/goto_end", "", goto_end);
		REGISTER_CALLBACK (serv, "/goto_end", "f", goto_end);
		REGISTER_CALLBACK (serv, "/scrub", "f", scrub);
		REGISTER_CALLBACK (serv, "/jog", "f", jog);
		REGISTER_CALLBACK (serv, "/jog/mode", "f", jog_mode);
		REGISTER_CALLBACK (serv, "/rewind", "", rewind);
		REGISTER_CALLBACK (serv, "/rewind", "f", rewind);
		REGISTER_CALLBACK (serv, "/ffwd", "", ffwd);
		REGISTER_CALLBACK (serv, "/ffwd", "f", ffwd);
		REGISTER_CALLBACK (serv, "/transport_stop", "", transport_stop);
		REGISTER_CALLBACK (serv, "/transport_stop", "f", transport_stop);
		REGISTER_CALLBACK (serv, "/transport_play", "", transport_play);
		REGISTER_CALLBACK (serv, "/transport_play", "f", transport_play);
		REGISTER_CALLBACK (serv, "/transport_frame", "", transport_sample);
		REGISTER_CALLBACK (serv, "/transport_speed", "", transport_speed);
		REGISTER_CALLBACK (serv, "/record_enabled", "", record_enabled);
		REGISTER_CALLBACK (serv, "/set_transport_speed", "f", set_transport_speed);
		// locate ii is position and bool roll
		REGISTER_CALLBACK (serv, "/locate", "ii", locate);
		REGISTER_CALLBACK (serv, "/save_state", "", save_state);
		REGISTER_CALLBACK (serv, "/save_state", "f", save_state);
		REGISTER_CALLBACK (serv, "/prev_marker", "", prev_marker);
		REGISTER_CALLBACK (serv, "/prev_marker", "f", prev_marker);
		REGISTER_CALLBACK (serv, "/next_marker", "", next_marker);
		REGISTER_CALLBACK (serv, "/next_marker", "f", next_marker);
		REGISTER_CALLBACK (serv, "/undo", "", undo);
		REGISTER_CALLBACK (serv, "/undo", "f", undo);
		REGISTER_CALLBACK (serv, "/redo", "", redo);
		REGISTER_CALLBACK (serv, "/redo", "f", redo);
		REGISTER_CALLBACK (serv, "/toggle_punch_in", "", toggle_punch_in);
		REGISTER_CALLBACK (serv, "/toggle_punch_in", "f", toggle_punch_in);
		REGISTER_CALLBACK (serv, "/toggle_punch_out", "", toggle_punch_out);
		REGISTER_CALLBACK (serv, "/toggle_punch_out", "f", toggle_punch_out);
		REGISTER_CALLBACK (serv, "/rec_enable_toggle", "", rec_enable_toggle);
		REGISTER_CALLBACK (serv, "/rec_enable_toggle", "f", rec_enable_toggle);
		REGISTER_CALLBACK (serv, "/toggle_all_rec_enables", "", toggle_all_rec_enables);
		REGISTER_CALLBACK (serv, "/toggle_all_rec_enables", "f", toggle_all_rec_enables);
		REGISTER_CALLBACK (serv, "/all_tracks_rec_in", "f", all_tracks_rec_in);
		REGISTER_CALLBACK (serv, "/all_tracks_rec_out", "f", all_tracks_rec_out);
		REGISTER_CALLBACK (serv, "/cancel_all_solos", "f", cancel_all_solos);
		REGISTER_CALLBACK (serv, "/remove_marker", "", remove_marker_at_playhead);
		REGISTER_CALLBACK (serv, "/remove_marker", "f", remove_marker_at_playhead);
		REGISTER_CALLBACK (serv, "/jump_bars", "f", jump_by_bars);
		REGISTER_CALLBACK (serv, "/jump_seconds", "f", jump_by_seconds);
		REGISTER_CALLBACK (serv, "/mark_in", "", mark_in);
		REGISTER_CALLBACK (serv, "/mark_in", "f", mark_in);
		REGISTER_CALLBACK (serv, "/mark_out", "", mark_out);
		REGISTER_CALLBACK (serv, "/mark_out", "f", mark_out);
		REGISTER_CALLBACK (serv, "/toggle_click", "", toggle_click);
		REGISTER_CALLBACK (serv, "/toggle_click", "f", toggle_click);
		REGISTER_CALLBACK (serv, "/click/level", "f", click_level);
		REGISTER_CALLBACK (serv, "/midi_panic", "", midi_panic);
		REGISTER_CALLBACK (serv, "/midi_panic", "f", midi_panic);
		REGISTER_CALLBACK (serv, "/toggle_roll", "", toggle_roll);
		REGISTER_CALLBACK (serv, "/toggle_roll", "f", toggle_roll);
		REGISTER_CALLBACK (serv, "/stop_forget", "", stop_forget);
		REGISTER_CALLBACK (serv, "/stop_forget", "f", stop_forget);
		REGISTER_CALLBACK (serv, "/set_punch_range", "", set_punch_range);
		REGISTER_CALLBACK (serv, "/set_punch_range", "f", set_punch_range);
		REGISTER_CALLBACK (serv, "/set_loop_range", "", set_loop_range);
		REGISTER_CALLBACK (serv, "/set_loop_range", "f", set_loop_range);
		REGISTER_CALLBACK (serv, "/set_session_range", "", set_session_range);
		REGISTER_CALLBACK (serv, "/set_session_range", "f", set_session_range);
		REGISTER_CALLBACK (serv, "/toggle_monitor_mute", "", toggle_monitor_mute);
		REGISTER_CALLBACK (serv, "/toggle_monitor_mute", "f", toggle_monitor_mute);
		REGISTER_CALLBACK (serv, "/toggle_monitor_dim", "", toggle_monitor_dim);
		REGISTER_CALLBACK (serv, "/toggle_monitor_dim", "f", toggle_monitor_dim);
		REGISTER_CALLBACK (serv, "/toggle_monitor_mono", "", toggle_monitor_mono);
		REGISTER_CALLBACK (serv, "/toggle_monitor_mono", "f", toggle_monitor_mono);
		REGISTER_CALLBACK (serv, "/quick_snapshot_switch", "", quick_snapshot_switch);
		REGISTER_CALLBACK (serv, "/quick_snapshot_switch", "f", quick_snapshot_switch);
		REGISTER_CALLBACK (serv, "/quick_snapshot_stay", "", quick_snapshot_stay);
		REGISTER_CALLBACK (serv, "/quick_snapshot_stay", "f", quick_snapshot_stay);
		REGISTER_CALLBACK (serv, "/fit_1_track", "", fit_1_track);
		REGISTER_CALLBACK (serv, "/fit_1_track", "f", fit_1_track);
		REGISTER_CALLBACK (serv, "/fit_2_tracks", "", fit_2_tracks);
		REGISTER_CALLBACK (serv, "/fit_2_tracks", "f", fit_2_tracks);
		REGISTER_CALLBACK (serv, "/fit_4_tracks", "", fit_4_tracks);
		REGISTER_CALLBACK (serv, "/fit_4_tracks", "f", fit_4_tracks);
		REGISTER_CALLBACK (serv, "/fit_8_tracks", "", fit_8_tracks);
		REGISTER_CALLBACK (serv, "/fit_8_tracks", "f", fit_8_tracks);
		REGISTER_CALLBACK (serv, "/fit_16_tracks", "", fit_16_tracks);
		REGISTER_CALLBACK (serv, "/fit_16_tracks", "f", fit_16_tracks);
		REGISTER_CALLBACK (serv, "/fit_32_tracks", "", fit_32_tracks);
		REGISTER_CALLBACK (serv, "/fit_32_tracks", "f", fit_32_tracks);
		REGISTER_CALLBACK (serv, "/fit_all_tracks", "", fit_all_tracks);
		REGISTER_CALLBACK (serv, "/fit_all_tracks", "f", fit_all_tracks);
		REGISTER_CALLBACK (serv, "/zoom_100_ms", "", zoom_100_ms);
		REGISTER_CALLBACK (serv, "/zoom_100_ms", "f", zoom_100_ms);
		REGISTER_CALLBACK (serv, "/zoom_1_sec", "", zoom_1_sec);
		REGISTER_CALLBACK (serv, "/zoom_1_sec", "f", zoom_1_sec);
		REGISTER_CALLBACK (serv, "/zoom_10_sec", "", zoom_10_sec);
		REGISTER_CALLBACK (serv, "/zoom_10_sec", "f", zoom_10_sec);
		REGISTER_CALLBACK (serv, "/zoom_1_min", "", zoom_1_min);
		REGISTER_CALLBACK (serv, "/zoom_1_min", "f", zoom_1_min);
		REGISTER_CALLBACK (serv, "/zoom_5_min", "", zoom_5_min);
		REGISTER_CALLBACK (serv, "/zoom_5_min", "f", zoom_5_min);
		REGISTER_CALLBACK (serv, "/zoom_10_min", "", zoom_10_min);
		REGISTER_CALLBACK (serv, "/zoom_10_min", "f", zoom_10_min);
		REGISTER_CALLBACK (serv, "/zoom_to_session", "", zoom_to_session);
		REGISTER_CALLBACK (serv, "/zoom_to_session", "f", zoom_to_session);
		REGISTER_CALLBACK (serv, "/temporal_zoom_in", "f", temporal_zoom_in);
		REGISTER_CALLBACK (serv, "/temporal_zoom_in", "", temporal_zoom_in);
		REGISTER_CALLBACK (serv, "/temporal_zoom_out", "", temporal_zoom_out);
		REGISTER_CALLBACK (serv, "/temporal_zoom_out", "f", temporal_zoom_out);
		REGISTER_CALLBACK (serv, "/scroll_up_1_track", "f", scroll_up_1_track);
		REGISTER_CALLBACK (serv, "/scroll_up_1_track", "", scroll_up_1_track);
		REGISTER_CALLBACK (serv, "/scroll_dn_1_track", "f", scroll_dn_1_track);
		REGISTER_CALLBACK (serv, "/scroll_dn_1_track", "", scroll_dn_1_track);
		REGISTER_CALLBACK (serv, "/scroll_up_1_page", "f", scroll_up_1_page);
		REGISTER_CALLBACK (serv, "/scroll_up_1_page", "", scroll_up_1_page);
		REGISTER_CALLBACK (serv, "/scroll_dn_1_page", "f", scroll_dn_1_page);
		REGISTER_CALLBACK (serv, "/scroll_dn_1_page", "", scroll_dn_1_page);
		REGISTER_CALLBACK (serv, "/bank_up", "", bank_up);
		REGISTER_CALLBACK (serv, "/bank_up", "f", bank_delta);
		REGISTER_CALLBACK (serv, "/bank_down", "", bank_down);
		REGISTER_CALLBACK (serv, "/bank_down", "f", bank_down);
		REGISTER_CALLBACK (serv, "/use_group", "f", use_group);

		// controls for "special" strips
		REGISTER_CALLBACK (serv, "/master/gain", "f", master_set_gain);
		REGISTER_CALLBACK (serv, "/master/fader", "f", master_set_fader);
		REGISTER_CALLBACK (serv, "/master/db_delta", "f", master_delta_gain);
		REGISTER_CALLBACK (serv, "/master/mute", "i", master_set_mute);
		REGISTER_CALLBACK (serv, "/master/trimdB", "f", master_set_trim);
		REGISTER_CALLBACK (serv, "/master/pan_stereo_position", "f", master_set_pan_stereo_position);
		REGISTER_CALLBACK (serv, "/master/select", "f", master_select);
		REGISTER_CALLBACK (serv, "/monitor/gain", "f", monitor_set_gain);
		REGISTER_CALLBACK (serv, "/monitor/fader", "f", monitor_set_fader);
		REGISTER_CALLBACK (serv, "/monitor/db_delta", "f", monitor_delta_gain);
		REGISTER_CALLBACK (serv, "/monitor/mute", "i", monitor_set_mute);
		REGISTER_CALLBACK (serv, "/monitor/dim", "i", monitor_set_dim);
		REGISTER_CALLBACK (serv, "/monitor/mono", "i", monitor_set_mono);

		// Controls for the Selected strip
		REGISTER_CALLBACK (serv, "/select/recenable", "i", sel_recenable);
		REGISTER_CALLBACK (serv, "/select/record_safe", "i", sel_recsafe);
		REGISTER_CALLBACK (serv, "/select/mute", "i", sel_mute);
		REGISTER_CALLBACK (serv, "/select/solo", "i", sel_solo);
		REGISTER_CALLBACK (serv, "/select/solo_iso", "i", sel_solo_iso);
		REGISTER_CALLBACK (serv, "/select/solo_safe", "i", sel_solo_safe);
		REGISTER_CALLBACK (serv, "/select/monitor_input", "i", sel_monitor_input);
		REGISTER_CALLBACK (serv, "/select/monitor_disk", "i", sel_monitor_disk);
		REGISTER_CALLBACK (serv, "/select/polarity", "i", sel_phase);
		REGISTER_CALLBACK (serv, "/select/gain", "f", sel_gain);
		REGISTER_CALLBACK (serv, "/select/fader", "f", sel_fader);
		REGISTER_CALLBACK (serv, "/select/db_delta", "f", sel_dB_delta);
		REGISTER_CALLBACK (serv, "/select/trimdB", "f", sel_trim);
		REGISTER_CALLBACK (serv, "/select/pan_stereo_position", "f", sel_pan_position);
		REGISTER_CALLBACK (serv, "/select/pan_stereo_width", "f", sel_pan_width);
		REGISTER_CALLBACK (serv, "/select/send_gain", "if", sel_sendgain);
		REGISTER_CALLBACK (serv, "/select/send_fader", "if", sel_sendfader);
		REGISTER_CALLBACK (serv, "/select/send_enable", "if", sel_sendenable);
		REGISTER_CALLBACK (serv, "/select/master_send_enable", "i", sel_master_send_enable);
		REGISTER_CALLBACK (serv, "/select/send_page", "f", sel_send_page);
		REGISTER_CALLBACK (serv, "/select/plug_page", "f", sel_plug_page);
		REGISTER_CALLBACK (serv, "/select/plugin", "f", sel_plugin);
		REGISTER_CALLBACK (serv, "/select/expand", "i", sel_expand);
		REGISTER_CALLBACK (serv, "/select/pan_elevation_position", "f", sel_pan_elevation);
		REGISTER_CALLBACK (serv, "/select/pan_frontback_position", "f", sel_pan_frontback);
		REGISTER_CALLBACK (serv, "/select/pan_lfe_control", "f", sel_pan_lfe);
		REGISTER_CALLBACK (serv, "/select/comp_enable", "f", sel_comp_enable);
		REGISTER_CALLBACK (serv, "/select/comp_threshold", "f", sel_comp_threshold);
		REGISTER_CALLBACK (serv, "/select/comp_speed", "f", sel_comp_speed);
		REGISTER_CALLBACK (serv, "/select/comp_mode", "f", sel_comp_mode);
		REGISTER_CALLBACK (serv, "/select/comp_makeup", "f", sel_comp_makeup);
		REGISTER_CALLBACK (serv, "/select/eq_enable", "f", sel_eq_enable);
		REGISTER_CALLBACK (serv, "/select/eq_hpf/freq", "f", sel_eq_hpf_freq);
		REGISTER_CALLBACK (serv, "/select/eq_hpf/enable", "f", sel_eq_hpf_enable);
		REGISTER_CALLBACK (serv, "/select/eq_hpf/slope", "f", sel_eq_hpf_slope);
		REGISTER_CALLBACK (serv, "/select/eq_lpf/freq", "f", sel_eq_lpf_freq);
		REGISTER_CALLBACK (serv, "/select/eq_lpf/enable", "f", sel_eq_lpf_enable);
		REGISTER_CALLBACK (serv, "/select/eq_lpf/slope", "f", sel_eq_lpf_slope);
		REGISTER_CALLBACK (serv, "/select/eq_gain", "if", sel_eq_gain);
		REGISTER_CALLBACK (serv, "/select/eq_freq", "if", sel_eq_freq);
		REGISTER_CALLBACK (serv, "/select/eq_q", "if", sel_eq_q);
		REGISTER_CALLBACK (serv, "/select/eq_shape", "if", sel_eq_shape);

		/* These commands require the route index in addition to the arg; TouchOSC (et al) can't use these  */
		REGISTER_CALLBACK (serv, "/strip/mute", "ii", route_mute);
		REGISTER_CALLBACK (serv, "/strip/solo", "ii", route_solo);
		REGISTER_CALLBACK (serv, "/strip/solo_iso", "ii", route_solo_iso);
		REGISTER_CALLBACK (serv, "/strip/solo_safe", "ii", route_solo_safe);
		REGISTER_CALLBACK (serv, "/strip/recenable", "ii", route_recenable);
		REGISTER_CALLBACK (serv, "/strip/record_safe", "ii", route_recsafe);
		REGISTER_CALLBACK (serv, "/strip/monitor_input", "ii", route_monitor_input);
		REGISTER_CALLBACK (serv, "/strip/monitor_disk", "ii", route_monitor_disk);
		REGISTER_CALLBACK (serv, "/strip/expand", "ii", strip_expand);
		REGISTER_CALLBACK (serv, "/strip/select", "ii", strip_gui_select);
		REGISTER_CALLBACK (serv, "/strip/polarity", "ii", strip_phase);
		REGISTER_CALLBACK (serv, "/strip/gain", "if", route_set_gain_dB);
		REGISTER_CALLBACK (serv, "/strip/fader", "if", route_set_gain_fader);
		REGISTER_CALLBACK (serv, "/strip/trimdB", "if", route_set_trim_dB);
		REGISTER_CALLBACK (serv, "/strip/pan_stereo_position", "if", route_set_pan_stereo_position);
		REGISTER_CALLBACK (serv, "/strip/pan_stereo_width", "if", route_set_pan_stereo_width);
		REGISTER_CALLBACK (serv, "/strip/plugin/parameter", "iiif", route_plugin_parameter);
		// prints to cerr only
		REGISTER_CALLBACK (serv, "/strip/plugin/parameter/print", "iii", route_plugin_parameter_print);
		REGISTER_CALLBACK (serv, "/strip/plugin/activate", "ii", route_plugin_activate);
		REGISTER_CALLBACK (serv, "/strip/plugin/deactivate", "ii", route_plugin_deactivate);
		REGISTER_CALLBACK (serv, "/strip/send/gain", "iif", route_set_send_gain_dB);
		REGISTER_CALLBACK (serv, "/strip/send/fader", "iif", route_set_send_fader);
		REGISTER_CALLBACK (serv, "/strip/send/enable", "iif", route_set_send_enable);
		REGISTER_CALLBACK(serv, "/strip/name", "is", route_rename);
		REGISTER_CALLBACK(serv, "/strip/sends", "i", route_get_sends);
		REGISTER_CALLBACK(serv, "/strip/receives", "i", route_get_receives);
		REGISTER_CALLBACK(serv, "/strip/plugin/list", "i", route_plugin_list);
		REGISTER_CALLBACK(serv, "/strip/plugin/descriptor", "ii", route_plugin_descriptor);
		REGISTER_CALLBACK(serv, "/strip/plugin/reset", "ii", route_plugin_reset);

		/* still not-really-standardized query interface */
		//REGISTER_CALLBACK (serv, "/ardour/*/#current_value", "", current_value);
		//REGISTER_CALLBACK (serv, "/ardour/set", "", set);

		// un/register_update args= s:ctrl s:returl s:retpath
		//lo_server_add_method(serv, "/register_update", "sss", OSC::global_register_update_handler, this);
		//lo_server_add_method(serv, "/unregister_update", "sss", OSC::global_unregister_update_handler, this);
		//lo_server_add_method(serv, "/register_auto_update", "siss", OSC::global_register_auto_update_handler, this);
		//lo_server_add_method(serv, "/unregister_auto_update", "sss", OSC::_global_unregister_auto_update_handler, this);

		/* this is a special catchall handler,
		 * register at the end so this is only called if no
		 * other handler matches (also used for debug) */
		lo_server_add_method (serv, 0, 0, _catchall, this);
	}
}

bool
OSC::osc_input_handler (IOCondition ioc, lo_server srv)
{
	if (ioc & ~IO_IN) {
		return false;
	}

	if (ioc & IO_IN) {
		lo_server_recv (srv);
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

			if (strcmp (path, "/strip/state") == 0) {

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

			} else if (strcmp (path, "/strip/mute") == 0) {

				lo_message_add_int32 (reply, (float) r->muted());

			} else if (strcmp (path, "/strip/solo") == 0) {

				lo_message_add_int32 (reply, r->soloed());
			}
		}
	}
	OSCSurface *sur = get_surface(get_address (msg));

	if (sur->feedback[14]) {
		lo_send_message (get_address (msg), "/reply", reply);
	} else {
		lo_send_message (get_address (msg), "#reply", reply);
	}
	lo_message_free (reply);
}

int
OSC::_catchall (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data)
{
	return ((OSC*)user_data)->catchall (path, types, argv, argc, data);
}

int
OSC::catchall (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg)
{
	size_t len;
	int ret = 1; /* unhandled */

	//cerr << "Received a message, path = " << path << " types = \""
	//     << (types ? types : "NULL") << '"' << endl;

	/* 15 for /#current_value plus 2 for /<path> */

	len = strlen (path);
	/*
	 * not needed until /strip/listen/ignore fixed
	OSCSurface *sur = get_surface(get_address (msg));
	*/

	if (strstr (path, "/automation")) {
		ret = set_automation (path, types, argv, argc, msg);

	} else
	if (strstr (path, "/touch")) {
		ret = touch_detect (path, types, argv, argc, msg);

	} else
	if (len >= 17 && !strcmp (&path[len-15], "/#current_value")) {
		current_value_query (path, len, argv, argc, msg);
		ret = 0;

	} else
	if (!strncmp (path, "/cue/", 5)) {

		ret = cue_parse (path, types, argv, argc, msg);

	} else
	if (!strncmp (path, "/select/plugin/parameter", 24)) {

		ret = select_plugin_parameter (path, types, argv, argc, msg);

	} else
	if (!strncmp (path, "/access_action/", 15)) {
		check_surface (msg);
		if (!(argc && !argv[0]->i)) {
			std::string action_path = path;

			access_action (action_path.substr(15));
		}

		ret = 0;
	} else
/*	if (strcmp (path, "/strip/listen") == 0) {
		check_surface (msg);

		cerr << "set up listener\n";

		lo_message reply = lo_message_new ();

		if (argc <= 0) {
			lo_message_add_string (reply, "syntax error");
		} else {
			for (int n = 0; n < argc; ++n) {

				boost::shared_ptr<Route> r = session->get_remote_nth_route (argv[n]->i);

				if (!r) {
					lo_message_add_string (reply, "not found");
					cerr << "no such route\n";
					break;
				} else {
					cerr << "add listener\n";
					listen_to_route (r, get_address (msg));
					lo_message_add_int32 (reply, argv[n]->i);
				}
			}
		}

		if (sur->feedback[14]) {
			lo_send_message (get_address (msg), "/reply", reply);
		} else {
			lo_send_message (get_address (msg), "#reply", reply);
		}
		lo_message_free (reply);

		ret = 0;

	} else
	if (strcmp (path, "/strip/ignore") == 0) {
		check_surface (msg);

		for (int n = 0; n < argc; ++n) {

			boost::shared_ptr<Route> r = session->get_remote_nth_route (argv[n]->i);

			if (r) {
				end_listen (r, get_address (msg));
			}
		}

		ret = 0;
	} else */
	if (strstr (path, "/strip") && (argc != 1)) {
		// All of the strip commands below require 1 parameter
		PBD::warning << "OSC: Wrong number of parameters." << endmsg;
	} else
	if (!strncmp (path, "/strip/gain/", 12) && strlen (path) > 12) {
		// in dB
		int ssid = atoi (&path[12]);
		ret = route_set_gain_dB (ssid, argv[0]->f, msg);
	}
	else if (!strncmp (path, "/strip/fader/", 13) && strlen (path) > 13) {
		// in fader position
		int ssid = atoi (&path[13]);
		ret = route_set_gain_fader (ssid, argv[0]->f, msg);
	}
	else if (!strncmp (path, "/strip/db_delta", 15)) {
		// in db delta
		int ssid;
		int ar_off = 0;
		float delta;
		if (strlen (path) > 15 && argc == 1) {
			ssid = atoi (&path[16]);
		} else if (argc == 2) {
			if (types[0] == 'f') {
				ssid = (int) argv[0]->f;
			} else {
				ssid = argv[0]->i;
			}
			ar_off = 1;
		} else {
			return -1;
		}
		if (types[ar_off] == 'f') {
			delta = argv[ar_off]->f;
		} else {
			delta = (float) argv[ar_off]->i;
		}
		ret = strip_db_delta (ssid, delta, msg);
	}
	else if (!strncmp (path, "/strip/trimdB/", 14) && strlen (path) > 14) {
		int ssid = atoi (&path[14]);
		ret = route_set_trim_dB (ssid, argv[0]->f, msg);
	}
	else if (!strncmp (path, "/strip/pan_stereo_position/", 27) && strlen (path) > 27) {
		int ssid = atoi (&path[27]);
		ret = route_set_pan_stereo_position (ssid, argv[0]->f, msg);
	}
	else if (!strncmp (path, "/strip/mute/", 12) && strlen (path) > 12) {
		int ssid = atoi (&path[12]);
		ret = route_mute (ssid, argv[0]->i, msg);
	}
	else if (!strncmp (path, "/strip/solo/", 12) && strlen (path) > 12) {
		int ssid = atoi (&path[12]);
		ret = route_solo (ssid, argv[0]->i, msg);
	}
	else if (!strncmp (path, "/strip/monitor_input/", 21) && strlen (path) > 21) {
		int ssid = atoi (&path[21]);
		ret = route_monitor_input (ssid, argv[0]->i, msg);
	}
	else if (!strncmp (path, "/strip/monitor_disk/", 20) && strlen (path) > 20) {
		int ssid = atoi (&path[20]);
		ret = route_monitor_disk (ssid, argv[0]->i, msg);
	}
	else if (!strncmp (path, "/strip/recenable/", 17) && strlen (path) > 17) {
		int ssid = atoi (&path[17]);
		ret = route_recenable (ssid, argv[0]->i, msg);
	}
	else if (!strncmp (path, "/strip/record_safe/", 19) && strlen (path) > 19) {
		int ssid = atoi (&path[19]);
		ret = route_recsafe (ssid, argv[0]->i, msg);
	}
	else if (!strncmp (path, "/strip/expand/", 14) && strlen (path) > 14) {
		int ssid = atoi (&path[14]);
		ret = strip_expand (ssid, argv[0]->i, msg);
	}
	else if (!strncmp (path, "/strip/select/", 14) && strlen (path) > 14) {
		int ssid = atoi (&path[14]);
		ret = strip_gui_select (ssid, argv[0]->i, msg);
	} else
	if (strstr (path, "/select") && (argc != 1)) {
		// All of the select commands below require 1 parameter
		PBD::warning << "OSC: Wrong number of parameters." << endmsg;
	}
	else if (!strncmp (path, "/select/send_gain/", 18) && strlen (path) > 18) {
		int ssid = atoi (&path[18]);
		ret = sel_sendgain (ssid, argv[0]->f, msg);
	}
	else if (!strncmp (path, "/select/send_fader/", 19) && strlen (path) > 19) {
		int ssid = atoi (&path[19]);
		ret = sel_sendfader (ssid, argv[0]->f, msg);
	}
	else if (!strncmp (path, "/select/send_enable/", 20) && strlen (path) > 20) {
		int ssid = atoi (&path[20]);
		ret = sel_sendenable (ssid, argv[0]->f, msg);
	}
	else if (!strncmp (path, "/select/eq_gain/", 16) && strlen (path) > 16) {
		int ssid = atoi (&path[16]);
		ret = sel_eq_gain (ssid, argv[0]->f, msg);
	}
	else if (!strncmp (path, "/select/eq_freq/", 16) && strlen (path) > 16) {
		int ssid = atoi (&path[16]);
		ret = sel_eq_freq (ssid, argv[0]->f , msg);
	}
	else if (!strncmp (path, "/select/eq_q/", 13) && strlen (path) > 13) {
		int ssid = atoi (&path[13]);
		ret = sel_eq_q (ssid, argv[0]->f, msg);
	}
	else if (!strncmp (path, "/select/eq_shape/", 17) && strlen (path) > 17) {
		int ssid = atoi (&path[17]);
		ret = sel_eq_shape (ssid, argv[0]->f, msg);
	}
	else if (!strncmp (path, "/set_surface", 12)) {
		ret = surface_parse (path, types, argv, argc, msg);
	}
	else if (strstr (path, "/link")) {
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
	lo_send (listener, "/session/exported", "ss", path.c_str(), name.c_str());
	lo_address_free (listener);
}

// end "Application Hook" Handlers //

/* path callbacks */

int
OSC::current_value (const char */*path*/, const char */*types*/, lo_arg **/*argv*/, int /*argc*/, void */*data*/, void* /*user_data*/)
{
#if 0
	const char* returl;

	if (argc < 3 || types == 0 || strlen (types) < 3 || types[0] != 's' || types[1] != 's' || types[2] != s) {
		return 1;
	}

	const char *returl = argv[1]->s;
	lo_address addr = find_or_cache_addr (returl);

	const char *retpath = argv[2]->s;


	if (strcmp (argv[0]->s, "transport_frame") == 0) {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_sample());
		}

	} else if (strcmp (argv[0]->s, "transport_speed") == 0) {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_sample());
		}

	} else if (strcmp (argv[0]->s, "transport_locked") == 0) {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_sample());
		}

	} else if (strcmp (argv[0]->s, "punch_in") == 0) {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_sample());
		}

	} else if (strcmp (argv[0]->s, "punch_out") == 0) {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_sample());
		}

	} else if (strcmp (argv[0]->s, "rec_enable") == 0) {

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
					// r->feeds (session->master_out()) may make more sense
					if (r->direct_feeds_according_to_reality (session->master_out())) {
						// this is a bus
						lo_message_add_string (reply, "B");
					} else {
						// this is an Aux out
						lo_message_add_string (reply, "AX");
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
				lo_send_message (get_address (msg), "/reply", reply);
			} else {
				lo_send_message (get_address (msg), "#reply", reply);
			}
			lo_message_free (reply);
		}
	}

	// Send end of listing message
	lo_message reply = lo_message_new ();

	lo_message_add_string (reply, "end_route_list");
	lo_message_add_int64 (reply, session->sample_rate());
	lo_message_add_int64 (reply, session->current_end_sample());
	if (session->monitor_out()) {
		// this session has a monitor section
		lo_message_add_int32 (reply, 1);
	} else {
		lo_message_add_int32 (reply, 0);
	}

	if (sur->feedback[14]) {
		lo_send_message (get_address (msg), "/reply", reply);
	} else {
		lo_send_message (get_address (msg), "#reply", reply);
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
	/* this function is for debugging and prints lots of
	 * information about what surfaces Ardour knows about and their
	 * internal parameters. It is best accessed by sending:
	 * /surface/list from oscsend. This command does not create
	 * a surface entry.
	 */

	cerr << "List of known Surfaces: " << _surface.size() << "\n\n";

	Glib::Threads::Mutex::Lock lm (surfaces_lock);
	for (uint32_t it = 0; it < _surface.size(); it++) {
		OSCSurface* sur = &_surface[it];
		cerr << string_compose ("  Surface: %1 URL: %2\n", it, sur->remote_url);
		cerr << string_compose ("	Number of strips: %1 Bank size: %2 Current Bank %3\n", sur->nstrips, sur->bank_size, sur->bank);
		bool ug = false;
		if (sur->usegroup == PBD::Controllable::UseGroup) {
			ug = true;
		}
		cerr << string_compose ("	Strip Types: %1 Feedback: %2 no_clear: %3 gain mode: %4 use groups? %5\n", \
			sur->strip_types.to_ulong(), sur->feedback.to_ulong(), sur->no_clear, sur->gainmode, ug);
		cerr << string_compose ("	using plugin: %1 of %2 plugins, with %3 params. page size: %4 page: %5\n", \
			sur->plugin_id, sur->plugins.size(), sur->plug_params.size(), sur->plug_page_size, sur->plug_page);
		cerr << string_compose ("	send page size: %1 page: %2\n", sur->send_page_size, sur->send_page);
		cerr << string_compose ("	expanded? %1 track: %2 jogmode: %3\n", sur->expand_enable, sur->expand, sur->jogmode);
		cerr << string_compose ("	personal monitor? %1, Aux master: %2, number of sends: %3\n", sur->cue, sur->aux, sur->sends.size());
		cerr << string_compose ("	Linkset: %1 Device Id: %2\n", sur->linkset, sur->linkid);
	}
	cerr << "\nList of LinkSets " << link_sets.size() << "\n\n";
	std::map<uint32_t, LinkSet>::iterator it;
	for (it = link_sets.begin(); it != link_sets.end(); it++) {
		if (!(*it).first) {
			continue;
		}
		uint32_t devices = 0;
		LinkSet* set = &(*it).second;
		if (set->linked.size()) {
			devices = set->linked.size() - 1;
		}
		cerr << string_compose ("  Linkset %1 has %2 devices and sees %3 strips\n", (*it).first, devices, set->strips.size());
		cerr << string_compose ("	Bank size: %1 Current bank: %2 Strip Types: %3\n", set->banksize, set->bank, set->strip_types.to_ulong());
		cerr << string_compose ("	auto bank sizing: %1 linkset not ready: %2\n", set->autobank, set->not_ready);
	}
	cerr << "\n";
}

int
OSC::cancel_all_solos ()
{
	session->cancel_all_solo ();
	return 0;
}

lo_address
OSC::get_address (lo_message msg)
{
	if (address_only) {
		lo_address addr = lo_message_get_source (msg);
		string host = lo_address_get_hostname (addr);
		int protocol = lo_address_get_protocol (addr);
		return lo_address_new_with_proto (protocol, host.c_str(), remote_port.c_str());
	} else {
		return lo_message_get_source (msg);
	}
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
	OSCSurface *sur = get_surface(get_address (msg));
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
			link_sets[set] = new_ls;
		}
		ls = &link_sets[set];

	} else {
		// User expects this surface to be removed from any sets
		int oldset = sur->linkset;
		if (oldset) {
			int oldid = sur->linkid;
			sur->linkid = 1;
			sur->linkset = 0;
			ls = &link_sets[oldid];
			if (ls) {
				ls->not_ready = 1;
				ls->linked[(uint32_t) data] = 0;
			}
		}
		return 0;
	}
	if (!strncmp (path, "/link/bank_size", 15)) {
		ls->banksize = (uint32_t) data;
		ls->autobank = false;
		ls->not_ready = link_check (set);
		if (ls->not_ready) {
			ls->bank = 1;
			strip_feedback (sur, true);
		} else {
			_set_bank (ls->bank, get_address (msg));
		}
		ret = 0;

	} else if (!strncmp (path, "/link/set", 9)) {
		sur->linkset = set;
		sur->linkid = (uint32_t) data;
		if (ls->linked.size() <= (uint32_t) data) {
			ls->linked.resize((int) data + 1);
		}
		ls->linked[(uint32_t) data] = sur;
		ls->not_ready = link_check (set);
		if (ls->not_ready) {
			strip_feedback (sur, true);
		} else {
			_set_bank (1, get_address (msg));
		}
		ret = 0;
	}

	return ret;
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
	uint32_t set_ready = 0;
	for (uint32_t dv = 1; dv < ls->linked.size(); dv++) {
		if ((ls->linked[dv]) && ((ls->linked[dv]))->linkset == set) {
			OSCSurface *su = (ls->linked[dv]);
			bank_total = bank_total + su->bank_size;
		} else if (!set_ready) {
			set_ready = dv;
		}
	}
	if (ls->autobank) {
		ls->banksize = bank_total;
	} else {
		if (!set_ready && bank_total != ls->banksize) {
			set_ready = ls->linked.size();
		}
	}
	return set_ready;
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


	if (argc == 1 && !strncmp (path, "/set_surface/feedback", 21)) {
		if (types[0] == 'f') {
			ret = set_surface_feedback ((int)argv[0]->f, msg);
		} else {
			ret = set_surface_feedback (argv[0]->i, msg);
		}
	}
	else if (argc == 1 && !strncmp (path, "/set_surface/bank_size", 22)) {
		if (types[0] == 'f') {
			ret = set_surface_bank_size ((int)argv[0]->f, msg);
		} else {
			ret = set_surface_bank_size (argv[0]->i, msg);
		}
	}
	else if (argc == 1 && !strncmp (path, "/set_surface/gainmode", 21)) {
		if (types[0] == 'f') {
			ret = set_surface_gainmode ((int)argv[0]->f, msg);
		} else {
			ret = set_surface_gainmode (argv[0]->i, msg);
		}
	}
	else if (argc == 1 && !strncmp (path, "/set_surface/strip_types", 24)) {
		if (types[0] == 'f') {
			ret = set_surface_strip_types ((int)argv[0]->f, msg);
		} else {
			ret = set_surface_strip_types (argv[0]->i, msg);
		}
	}
	else if (argc == 1 && !strncmp (path, "/set_surface/send_page_size", 27)) {
		if (types[0] == 'f') {
			ret = sel_send_pagesize ((int)argv[0]->f, msg);
		} else {
			ret = sel_send_pagesize (argv[0]->i, msg);
		}
	}
	else if (argc == 1 && !strncmp (path, "/set_surface/plugin_page_size", 29)) {
		if (types[0] == 'f') {
			ret = sel_plug_pagesize ((int)argv[0]->f, msg);
		} else {
			ret = sel_plug_pagesize (argv[0]->i, msg);
		}
	} else if (strlen(path) == 12) {

		// command is in /set_surface iii form
		switch (argc) {
			case 6:
				if (types[5] == 'f') {
					pi_page = (int) argv[5]->f;
				} else {
					pi_page = argv[5]->i;
				}
			case 5:
				if (types[4] == 'f') {
					se_page = (int) argv[4]->f;
				} else {
					se_page = argv[4]->i;
				}
			case 4:
				if (types[3] == 'f') {
					fadermode = (int) argv[3]->f;
				} else {
					fadermode = argv[3]->i;
				}
			case 3:
				if (types[2] == 'f') {
					feedback = (int) argv[2]->f;
				} else {
					feedback = argv[2]->i;
				}
			case 2:
				if (types[1] == 'f') {
					strip_types = (int) argv[1]->f;
				} else {
					strip_types = argv[1]->i;
				}
			case 1:
				if (types[0] == 'f') {
					bank_size = (int) argv[0]->f;
				} else {
					bank_size = argv[0]->i;
				}
				ret = set_surface (bank_size, strip_types, feedback, fadermode, se_page, pi_page, msg);
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
					lo_send_message (get_address (msg), "/set_surface", reply);
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
		ret = set_surface (bank_size, strip_types, feedback, fadermode, se_page, pi_page, msg);
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
	// set bank and strip feedback
	// XXXX check if we are already in a linkset
	_strip_select (boost::shared_ptr<ARDOUR::Stripable>(), get_address (msg), true);
	strip_feedback(s, true);

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
	// XXXX check if we are already in a linkset
	s->bank = 1;

	// set bank and strip feedback
	strip_feedback (s, true);
	_strip_select (boost::shared_ptr<ARDOUR::Stripable>(), get_address (msg));
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
	if (s->strip_types[10]) {
		s->usegroup = PBD::Controllable::UseGroup;
	} else {
		s->usegroup = PBD::Controllable::NoGroup;
	}

	// set bank and strip feedback
	// XXXX check for linkset
	s->bank = 1;
	strip_feedback (s, true);
	_strip_select (boost::shared_ptr<ARDOUR::Stripable>(), get_address (msg));
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

	strip_feedback (s, false);
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

	strip_feedback (s, false);
	global_feedback (s);
	_strip_select (boost::shared_ptr<ARDOUR::Stripable>(), get_address (msg));
	return 0;
}

int
OSC::check_surface (lo_message msg)
{
	if (!session) {
		return -1;
	}
	get_surface(get_address (msg));
	return 0;
}

OSC::OSCSurface *
OSC::get_surface (lo_address addr , bool quiet)
{
	string r_url;
	char * rurl;
	if (address_only) {
		string host = lo_address_get_hostname (addr);
		int protocol = lo_address_get_protocol (addr);
		addr = lo_address_new_with_proto (protocol, host.c_str(), remote_port.c_str());
	}

	rurl = lo_address_get_url (addr);
	r_url = rurl;
	free (rurl);
	{
		for (uint32_t it = 0; it < _surface.size(); ++it) {
			//find setup for this surface
			if (!_surface[it].remote_url.find(r_url)){
				return &_surface[it];
			}
		}
	}

	// No surface create one with default values
	OSCSurface s;
	s.remote_url = r_url;
	s.no_clear = false;
	s.jogmode = JOG;
	s.bank = 1;
	s.bank_size = default_banksize;
	s.observers.clear();
	s.sel_obs = 0;
	s.global_obs = 0;
	s.strip_types = default_strip;
	s.feedback = default_feedback;
	s.gainmode = default_gainmode;
	s.usegroup = PBD::Controllable::NoGroup;
	s.sel_obs = 0;
	s.expand = 0;
	s.expand_enable = false;
	s.cue = false;
	s.aux = 0;
	s.cue_obs = 0;
	s.strips = get_sorted_stripables(s.strip_types, s.cue);
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
		_strip_select (boost::shared_ptr<ARDOUR::Stripable>(), addr);
	}

	return &_surface[_surface.size() - 1];
}

// setup global feedback for a surface
void
OSC::global_feedback (OSCSurface* sur)
{
	if (sur->feedback[4] || sur->feedback[3] || sur->feedback[5] || sur->feedback[6]) {

		// create a new Global Observer for this surface
		OSCGlobalObserver* o = new OSCGlobalObserver (*this, *session, sur);
		sur->global_obs = o;
	}
}

void
OSC::strip_feedback (OSCSurface* sur, bool new_bank_size)
{
	sur->strips = get_sorted_stripables(sur->strip_types, sur->cue);
	sur->nstrips = sur->strips.size();
	if (new_bank_size || (!sur->feedback[0] && !sur->feedback[1])) {
		// delete old observers
		for (uint32_t i = 0; i < sur->observers.size(); i++) {
			if (!sur->bank_size) {
				sur->observers[i]->clear_strip ();
			}
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
			}
		}
	} else {
		if (sur->feedback[0] || sur->feedback[1]) {
			for (uint32_t i = 0; i < sur->observers.size(); i++) {
				sur->observers[i]->refresh_strip(true);
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
	 * of strips. For a recalc, We want to tear down all strips but not send
	 * a reset value for any of the controls and then rebuild all observers.
	 * this is easier than detecting change in "bank" size and deleting or
	 * adding just a few.
	 */

	// refresh each surface we know about.
	for (uint32_t it = 0; it < _surface.size(); ++it) {
		OSCSurface* sur = &_surface[it];
		sur->strips = get_sorted_stripables(sur->strip_types, sur->cue);
		sur->nstrips = sur->strips.size();
		// find lo_address
		lo_address addr = lo_address_new_from_url (sur->remote_url.c_str());
		_strip_select (boost::shared_ptr<ARDOUR::Stripable>(), addr, true);
		if (sur->cue) {
			_cue_set (sur->aux, addr);
		} else if (!sur->bank_size) {
			strip_feedback (sur, true);
			// This surface uses /strip/list tell it routes have changed
			lo_message reply;
			reply = lo_message_new ();
			lo_send_message (addr, "/strip/list", reply);
			lo_message_free (reply);
		} else {
			strip_feedback (sur, false);
		}
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
	uint32_t l_set = s->linkset;

	if (l_set) {
		//we have a linkset... deal with each surface
		set = &(link_sets[l_set]);
		if (set->not_ready) {
			return 1;
		}
		uint32_t s_count = set->linked.size();
		set->strips = striplist;
		bank_start = bank_limits_check (bank_start, set->banksize, nstrips);
		set->bank = bank_start;
		for (uint32_t ls = 1; ls < s_count; ls++) {
			OSCSurface *sur = (set->linked[ls]);
			if (!sur || sur->linkset != l_set) {
				if (!set->not_ready) {
					set->not_ready = ls;
				}
				set->bank = 1;
				return 1;
			}
			lo_address sur_addr = lo_address_new_from_url (sur->remote_url.c_str());
			_strip_select (boost::shared_ptr<ARDOUR::Stripable>(), addr, true);

			sur->bank = bank_start;
			bank_start = bank_start + sur->bank_size;
			strip_feedback (sur, false);
			bank_leds (sur);
			lo_address_free (sur_addr);
		}
	} else {

		_strip_select (boost::shared_ptr<ARDOUR::Stripable>(), addr, true);
		s->bank = bank_limits_check (bank_start, s->bank_size, nstrips);
		strip_feedback (s, true);
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
		lo_send_message (addr, "/bank_up", reply);
		lo_message_free (reply);
		reply = lo_message_new ();
		if (bank > 1) {
			lo_message_add_int32 (reply, 1);
		} else {
			lo_message_add_int32 (reply, 0);
		}
		lo_send_message (addr, "/bank_down", reply);
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
	// failsafe... should never get here.
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
		s->sel_obs->renew_sends();
	}
	return 0;
}

int
OSC::sel_send_page (int page, lo_message msg)
{
	OSCSurface *s = get_surface(get_address (msg));
	s->send_page = s->send_page + page;
	s->sel_obs->renew_sends();
	return 0;
}

int
OSC::sel_plug_pagesize (uint32_t size, lo_message msg)
{
	OSCSurface *s = get_surface(get_address (msg));
	if (size != s->plug_page_size) {
		s->plug_page_size = size;
		s->sel_obs->renew_plugin();
	}
	return 0;
}

int
OSC::sel_plug_page (int page, lo_message msg)
{
	OSCSurface *s = get_surface(get_address (msg));
	s->plug_page = s->plug_page + page;
	s->sel_obs->renew_plugin();
	return 0;
}

int
OSC::sel_plugin (int delta, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	return _sel_plugin (sur->plugin_id + delta, get_address (msg));
}

int
OSC::_sel_plugin (int id, lo_address addr)
{
	OSCSurface *sur = get_surface(addr);
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, addr);
	} else {
		s = _select;
	}
	if (s) {
		boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route>(s);
		if (!r) {
			return 1;
		}

		// find out how many plugins we have
		bool plugs;
		int nplugs  = 0;
		sur->plugins.clear();
		do {
			plugs = false;
			if (r->nth_plugin (nplugs)) {
				if (r->nth_plugin(nplugs)->display_to_user()) {
#ifdef MIXBUS
					// need to check for mixbus channel strips (and exclude them)
					boost::shared_ptr<Processor> proc = r->nth_plugin (nplugs);
					boost::shared_ptr<PluginInsert> pi;
					if ((pi = boost::dynamic_pointer_cast<PluginInsert>(proc))) {

						if (!pi->is_channelstrip()) {
#endif
							sur->plugins.push_back (nplugs);
#ifdef MIXBUS
						}
					}
#endif
				}
				plugs = true;
				nplugs++;
			}
		} while (plugs);

		// limit plugin_id to actual plugins
		if (!sur->plugins.size()) {
			sur->plugin_id = 0;
			return 0;
		} else if (sur->plugins.size() < (uint32_t) id) {
			sur->plugin_id = sur->plugins.size();
		} else  if (sur->plugins.size() && !id) {
			sur->plugin_id = 1;
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
			sur->sel_obs->renew_plugin();
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

	lo_send_message (get_address (msg), "/transport_frame", reply);

	lo_message_free (reply);
}

void
OSC::transport_speed (lo_message msg)
{
	if (!session) {
		return;
	}
	check_surface (msg);
	double ts = session->transport_speed ();

	lo_message reply = lo_message_new ();
	lo_message_add_double (reply, ts);

	lo_send_message (get_address (msg), "/transport_speed", reply);

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

	lo_send_message (get_address (msg), "/record_enabled", reply);

	lo_message_free (reply);
}

int
OSC::scrub (float delta, lo_message msg)
{
	if (!session) return -1;
	check_surface (msg);

	scrub_place = session->transport_sample ();

	float speed;

	int64_t now = ARDOUR::get_microseconds ();
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
		session->request_transport_speed (0);
	}

	return 0;
}

int
OSC::jog (float delta, lo_message msg)
{
	if (!session) return -1;

	OSCSurface *s = get_surface(get_address (msg));

	string path = "/jog/mode/name";
	switch(s->jogmode)
	{
		case JOG  :
			text_message (path, "Jog", get_address (msg));
			if (delta) {
				jump_by_seconds (delta / 5);
			}
			break;
		case SCRUB:
			text_message (path, "Scrub", get_address (msg));
			scrub (delta, msg);
			break;
		case SHUTTLE:
			text_message (path, "Shuttle", get_address (msg));
			if (delta) {
				double speed = get_transport_speed ();
				set_transport_speed (speed + (delta / 8.1));
			} else {
				set_transport_speed (0);
			}
			break;
		case SCROLL:
			text_message (path, "Scroll", get_address (msg));
			if (delta > 0) {
				access_action ("Editor/scroll-forward");
			} else if (delta < 0) {
				access_action ("Editor/scroll-backward");
			}
			break;
		case TRACK:
			text_message (path, "Track", get_address (msg));
			if (delta > 0) {
				set_bank (s->bank + 1, msg);
			} else if (delta < 0) {
				set_bank (s->bank - 1, msg);
			}
			break;
		case BANK:
			text_message (path, "Bank", get_address (msg));
			if (delta > 0) {
				bank_up (msg);
			} else if (delta < 0) {
				bank_down (msg);
			}
			break;
		case NUDGE:
			text_message (path, "Nudge", get_address (msg));
			if (delta > 0) {
				access_action ("Common/nudge-playhead-forward");
			} else if (delta < 0) {
				access_action ("Common/nudge-playhead-backward");
			}
			break;
		case MARKER:
			text_message (path, "Marker", get_address (msg));
			if (delta > 0) {
				next_marker ();
			} else if (delta < 0) {
				prev_marker ();
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

	switch((uint32_t)mode)
	{
		case JOG  :
			text_message ("/jog/mode/name", "Jog", get_address (msg));
			s->jogmode = JOG;
			break;
		case SCRUB:
			text_message ("/jog/mode/name", "Scrub", get_address (msg));
			s->jogmode = SCRUB;
			break;
		case SHUTTLE:
			text_message ("/jog/mode/name", "Shuttle", get_address (msg));
			s->jogmode = SHUTTLE;
			break;
		case SCROLL:
			text_message ("/jog/mode/name", "Scroll", get_address (msg));
			s->jogmode = SCROLL;
			break;
		case TRACK:
			text_message ("/jog/mode/name", "Track", get_address (msg));
			s->jogmode = TRACK;
			break;
		case BANK:
			text_message ("/jog/mode/name", "Bank", get_address (msg));
			s->jogmode = BANK;
			break;
		case NUDGE:
			text_message ("/jog/mode/name", "Nudge", get_address (msg));
			s->jogmode = NUDGE;
			break;
		case MARKER:
			text_message ("/jog/mode/name", "Marker", get_address (msg));
			s->jogmode = MARKER;
			break;
		default:
			PBD::warning << "Jog Mode: " << mode << " is not valid." << endmsg;
			break;
	lo_message reply = lo_message_new ();
	lo_message_add_int32 (reply, s->jogmode);
	lo_send_message (get_address(msg), "/jog/mode", reply);
	lo_message_free (reply);

	}
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

// master and monitor calls
int
OSC::master_set_gain (float dB)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = session->master_out();
	if (s) {
		if (dB < -192) {
			s->gain_control()->set_value (0.0, PBD::Controllable::NoGroup);
		} else {
			float abs = dB_to_coefficient (dB);
			float top = s->gain_control()->upper();
			if (abs > top) {
				abs = top;
			}
			s->gain_control()->set_value (abs, PBD::Controllable::NoGroup);
		}
	}
	return 0;
}

int
OSC::master_delta_gain (float delta)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = session->master_out();
	if (s) {
		float dB = accurate_coefficient_to_dB (s->gain_control()->get_value()) + delta;
		if (dB < -192) {
			s->gain_control()->set_value (0.0, PBD::Controllable::NoGroup);
		} else {
			float abs = dB_to_coefficient (dB);
			float top = s->gain_control()->upper();
			if (abs > top) {
				abs = top;
			}
			s->gain_control()->set_value (abs, PBD::Controllable::NoGroup);
		}
	}
	return 0;
}

int
OSC::master_set_fader (float position)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = session->master_out();
	if (s) {
		s->gain_control()->set_value (s->gain_control()->interface_to_internal (position), PBD::Controllable::NoGroup);
	}
	return 0;
}

int
OSC::master_set_trim (float dB)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = session->master_out();

	if (s) {
		s->trim_control()->set_value (dB_to_coefficient (dB), PBD::Controllable::NoGroup);
	}

	return 0;
}

int
OSC::master_set_pan_stereo_position (float position, lo_message msg)
{
	if (!session) return -1;
	OSCSurface *sur = get_surface(get_address (msg));

	float endposition = .5;
	boost::shared_ptr<Stripable> s = session->master_out();

	if (s) {
		if (s->pan_azimuth_control()) {
			s->pan_azimuth_control()->set_value (s->pan_azimuth_control()->interface_to_internal (position), PBD::Controllable::NoGroup);
			endposition = s->pan_azimuth_control()->internal_to_interface (s->pan_azimuth_control()->get_value ());
		}
	}

	if (sur->feedback[4]) {
		lo_message reply = lo_message_new ();
		lo_message_add_float (reply, endposition);

		lo_send_message (get_address (msg), "/master/pan_stereo_position", reply);
		lo_message_free (reply);
	}

	return 0;
}

int
OSC::master_set_mute (uint32_t state)
{
	if (!session) return -1;

	boost::shared_ptr<Stripable> s = session->master_out();

	if (s) {
		s->mute_control()->set_value (state, PBD::Controllable::NoGroup);
	}

	return 0;
}

int
OSC::master_select (lo_message msg)
{
	if (!session) {
		return -1;
	}
	OSCSurface *sur = get_surface(get_address (msg));
	sur->expand_enable = false;
	boost::shared_ptr<Stripable> s = session->master_out();
	if (s) {
		SetStripableSelection (s);
	}

	return 0;
}

int
OSC::monitor_set_gain (float dB)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = session->monitor_out();

	if (s) {
		if (dB < -192) {
			s->gain_control()->set_value (0.0, PBD::Controllable::NoGroup);
		} else {
			float abs = dB_to_coefficient (dB);
			float top = s->gain_control()->upper();
			if (abs > top) {
				abs = top;
			}
			s->gain_control()->set_value (abs, PBD::Controllable::NoGroup);
		}
	}
	return 0;
}

int
OSC::monitor_delta_gain (float delta)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = session->monitor_out();
	if (s) {
		float dB = accurate_coefficient_to_dB (s->gain_control()->get_value()) + delta;
		if (dB < -192) {
			s->gain_control()->set_value (0.0, PBD::Controllable::NoGroup);
		} else {
			float abs = dB_to_coefficient (dB);
			float top = s->gain_control()->upper();
			if (abs > top) {
				abs = top;
			}
			s->gain_control()->set_value (abs, PBD::Controllable::NoGroup);
		}
	}
	return 0;
}

int
OSC::monitor_set_fader (float position)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = session->monitor_out();
	if (s) {
		s->gain_control()->set_value (s->gain_control()->interface_to_internal (position), PBD::Controllable::NoGroup);
	}
	return 0;
}

int
OSC::monitor_set_mute (uint32_t state)
{
	if (!session) return -1;

	if (session->monitor_out()) {
		boost::shared_ptr<MonitorProcessor> mon = session->monitor_out()->monitor_control();
		mon->set_cut_all (state);
	}
	return 0;
}

int
OSC::monitor_set_dim (uint32_t state)
{
	if (!session) return -1;

	if (session->monitor_out()) {
		boost::shared_ptr<MonitorProcessor> mon = session->monitor_out()->monitor_control();
		mon->set_dim_all (state);
	}
	return 0;
}

int
OSC::monitor_set_mono (uint32_t state)
{
	if (!session) return -1;

	if (session->monitor_out()) {
		boost::shared_ptr<MonitorProcessor> mon = session->monitor_out()->monitor_control();
		mon->set_mono (state);
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
	lo_send_message(get_address (msg), "/strip/sends", reply);

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
	lo_send_message(get_address (msg), "/strip/receives", reply);
	lo_message_free(reply);
	return 0;
}

// strip calls

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

	if (argc) {
		if (types[argc - 1] == 'f') {
			aut = (int)argv[argc - 1]->f;
		} else {
			aut = argv[argc - 1]->i;
		}
	}

	//parse path first to find stripable
	if (!strncmp (path, "/strip/", 7)) {
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
		ctr = 7;
	} else if (!strncmp (path, "/select/", 8)) {
		if (sur->expand_enable && sur->expand) {
			strp = get_strip (sur->expand, get_address (msg));
		} else {
			strp = _select;
		}
		ctr = 8;
	} else {
		return ret;
	}
	if (strp) {
		boost::shared_ptr<AutomationControl> control = boost::shared_ptr<AutomationControl>();
		// other automatable controls can be added by repeating the next 6.5 lines
		if ((!strncmp (&path[ctr], "fader", 5)) || (!strncmp (&path[ctr], "gain", 4))) {
			if (strp->gain_control ()) {
				control = strp->gain_control ();
			} else {
				PBD::warning << "No fader for this strip" << endmsg;
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
	if (!strncmp (path, "/strip/", 7)) {
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
		ctr = 7;
	} else if (!strncmp (path, "/select/", 8)) {
		if (sur->expand_enable && sur->expand) {
			strp = get_strip (sur->expand, get_address (msg));
		} else {
			strp = _select;
		}
		ctr = 8;
	} else {
		return ret;
	}
	if (strp) {
		boost::shared_ptr<AutomationControl> control = boost::shared_ptr<AutomationControl>();
		// other automatable controls can be added by repeating the next 6.5 lines
		if ((!strncmp (&path[ctr], "fader", 5)) || (!strncmp (&path[ctr], "gain", 4))) {
			if (strp->gain_control ()) {
				control = strp->gain_control ();
			} else {
				PBD::warning << "No fader for this strip" << endmsg;
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
OSC::route_mute (int ssid, int yn, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
	OSCSurface *sur = get_surface(get_address (msg));

	if (s) {
		if (s->mute_control()) {
			s->mute_control()->set_value (yn ? 1.0 : 0.0, sur->usegroup);
			return 0;
		}
	}

	return float_message_with_id ("/strip/mute", ssid, 0, sur->feedback[2], get_address (msg));
}

int
OSC::sel_mute (uint32_t yn, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->mute_control()) {
			s->mute_control()->set_value (yn ? 1.0 : 0.0, PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/mute", 0, get_address (msg));
}

int
OSC::route_solo (int ssid, int yn, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
	OSCSurface *sur = get_surface(get_address (msg));

	if (s) {
		if (s->solo_control()) {
			s->solo_control()->set_value (yn ? 1.0 : 0.0, sur->usegroup);
		}
	}

	return float_message_with_id ("/strip/solo", ssid, 0, sur->feedback[2], get_address (msg));
}

int
OSC::route_solo_iso (int ssid, int yn, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
	OSCSurface *sur = get_surface(get_address (msg));

	if (s) {
		if (s->solo_isolate_control()) {
			s->solo_isolate_control()->set_value (yn ? 1.0 : 0.0, sur->usegroup);
			return 0;
		}
	}

	return float_message_with_id ("/strip/solo_iso", ssid, 0, sur->feedback[2], get_address (msg));
}

int
OSC::route_solo_safe (int ssid, int yn, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, lo_message_get_source (msg));
	OSCSurface *sur = get_surface(get_address (msg));

	if (s) {
		if (s->solo_safe_control()) {
			s->solo_safe_control()->set_value (yn ? 1.0 : 0.0, sur->usegroup);
			return 0;
		}
	}

	return float_message_with_id ("/strip/solo_safe", ssid, 0, sur->feedback[2], get_address (msg));
}

int
OSC::sel_solo (uint32_t yn, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->solo_control()) {
			session->set_control (s->solo_control(), yn ? 1.0 : 0.0, PBD::Controllable::NoGroup);
		}
	}
	return float_message("/select/solo", 0, get_address (msg));
}

int
OSC::sel_solo_iso (uint32_t yn, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->solo_isolate_control()) {
			s->solo_isolate_control()->set_value (yn ? 1.0 : 0.0, PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/solo_iso", 0, get_address (msg));
}

int
OSC::sel_solo_safe (uint32_t yn, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->solo_safe_control()) {
			s->solo_safe_control()->set_value (yn ? 1.0 : 0.0, PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/solo_safe", 0, get_address (msg));
}

int
OSC::sel_recenable (uint32_t yn, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->rec_enable_control()) {
			s->rec_enable_control()->set_value (yn ? 1.0 : 0.0, PBD::Controllable::NoGroup);
			if (s->rec_enable_control()->get_value()) {
				return 0;
			}
		}
	}
	return float_message("/select/recenable", 0, get_address (msg));
}

int
OSC::route_recenable (int ssid, int yn, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
	OSCSurface *sur = get_surface(get_address (msg));

	if (s) {
		if (s->rec_enable_control()) {
			s->rec_enable_control()->set_value (yn, sur->usegroup);
			if (s->rec_enable_control()->get_value()) {
				return 0;
			}
		}
	}
	return float_message_with_id ("/strip/recenable", ssid, 0, sur->feedback[2], get_address (msg));
}

int
OSC::route_rename(int ssid, char *newname, lo_message msg) {
    if (!session) {
        return -1;
    }

    boost::shared_ptr<Stripable> s = get_strip(ssid, get_address(msg));

    if (s) {
        s->set_name(std::string(newname));
    }

    return 0;
}

int
OSC::sel_recsafe (uint32_t yn, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->rec_safe_control()) {
			s->rec_safe_control()->set_value (yn ? 1.0 : 0.0, PBD::Controllable::NoGroup);
			if (s->rec_safe_control()->get_value()) {
				return 0;
			}
		}
	}
	return float_message("/select/record_safe", 0, get_address (msg));
}

int
OSC::route_recsafe (int ssid, int yn, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
	OSCSurface *sur = get_surface(get_address (msg));
	if (s) {
		if (s->rec_safe_control()) {
			s->rec_safe_control()->set_value (yn, sur->usegroup);
			if (s->rec_safe_control()->get_value()) {
				return 0;
			}
		}
	}
	return float_message_with_id ("/strip/record_safe", ssid, 0, sur->feedback[2], get_address (msg));
}

int
OSC::route_monitor_input (int ssid, int yn, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
	OSCSurface *sur = get_surface(get_address (msg));

	if (s) {
		boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (s);
		if (track) {
			if (track->monitoring_control()) {
				std::bitset<32> value = track->monitoring_control()->get_value ();
				value[0] = yn ? 1 : 0;
				track->monitoring_control()->set_value (value.to_ulong(), sur->usegroup);
				return 0;
			}
		}
	}

	return float_message_with_id ("/strip/monitor_input", ssid, 0, sur->feedback[2], get_address (msg));
}

int
OSC::sel_monitor_input (uint32_t yn, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (s);
		if (track) {
			if (track->monitoring_control()) {
				std::bitset<32> value = track->monitoring_control()->get_value ();
				value[0] = yn ? 1 : 0;
				track->monitoring_control()->set_value (value.to_ulong(), sur->usegroup);
				return 0;
			}
		}
	}
	return float_message("/select/monitor_input", 0, get_address (msg));
}

int
OSC::route_monitor_disk (int ssid, int yn, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
	OSCSurface *sur = get_surface(get_address (msg));

	if (s) {
		boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (s);
		if (track) {
			if (track->monitoring_control()) {
				std::bitset<32> value = track->monitoring_control()->get_value ();
				value[1] = yn ? 1 : 0;
				track->monitoring_control()->set_value (value.to_ulong(), sur->usegroup);
				return 0;
			}
		}
	}

	return float_message_with_id ("/strip/monitor_disk", ssid, 0, sur->feedback[2], get_address (msg));
}

int
OSC::sel_monitor_disk (uint32_t yn, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (s);
		if (track) {
			if (track->monitoring_control()) {
				std::bitset<32> value = track->monitoring_control()->get_value ();
				value[1] = yn ? 1 : 0;
				track->monitoring_control()->set_value (value.to_ulong(), sur->usegroup);
				return 0;
			}
		}
	}
	return float_message("/select/monitor_disk", 0, get_address (msg));
}


int
OSC::strip_phase (int ssid, int yn, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
	OSCSurface *sur = get_surface(get_address (msg));

	if (s) {
		if (s->phase_control()) {
			s->phase_control()->set_value (yn ? 1.0 : 0.0, sur->usegroup);
			return 0;
		}
	}

	return float_message_with_id ("/strip/polarity", ssid, 0, sur->feedback[2], get_address (msg));
}

int
OSC::sel_phase (uint32_t yn, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->phase_control()) {
			s->phase_control()->set_value (yn ? 1.0 : 0.0, PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/polarity", 0, get_address (msg));
}

int
OSC::strip_expand (int ssid, int yn, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	sur->expand_enable = (bool) yn;
	sur->expand = ssid;
	boost::shared_ptr<Stripable> s;
	if (yn) {
		s = get_strip (ssid, get_address (msg));
	} else {
		s = _select;
	}

	return _strip_select (s, get_address (msg));
}

int
OSC::_strip_select (boost::shared_ptr<Stripable> s, lo_address addr, bool quiet)
{
	if (!session) {
		return -1;
	}
	OSCSurface *sur = get_surface(addr, true);
	if (!s) {
		if (sur->expand_enable) {
			// expand doesn't point to a stripable, turn it off and use select
			sur->expand = 0;
			sur->expand_enable = false;
		}
		if(ControlProtocol::first_selected_stripable()) {
			s = ControlProtocol::first_selected_stripable();
		} else {
			s = session->master_out ();
		}
			_select = s;
	}
	sur->select = s;
	s->DropReferences.connect (*this, MISSING_INVALIDATOR, boost::bind (&OSC::recalcbanks, this), this);

	OSCSelectObserver* so = dynamic_cast<OSCSelectObserver*>(sur->sel_obs);
	if (sur->feedback[13]) {
		if (so != 0) {
			delete so;
		}
		OSCSelectObserver* sel_fb = new OSCSelectObserver (*this, sur);
		sur->sel_obs = sel_fb;
	} else {
		if (so != 0) {
			delete so;
			sur->sel_obs = 0;
		}
	}

	// need to set monitor for processor changed signal (for paging)
	// detecting processor changes requires cast to route
	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route>(s);
	if (r) {
		r->processors_changed.connect  (sur->proc_connection, MISSING_INVALIDATOR, boost::bind (&OSC::processor_changed, this, addr), this);
		processor_changed (addr);
	}

	if (!quiet) {
		strip_feedback (sur, false);
	}

	return 0;
}

void
OSC::processor_changed (lo_address addr)
{
	OSCSurface *sur = get_surface (addr);
	sur->proc_connection.disconnect ();
	_sel_plugin (sur->plugin_id, addr);
	if (sur->sel_obs) {
		sur->sel_obs->renew_sends ();
		sur->sel_obs->eq_restart (-1);
	}
}

int
OSC::strip_gui_select (int ssid, int yn, lo_message msg)
{
	//ignore button release
	if (!yn) return 0;

	if (!session) {
		return -1;
	}
	OSCSurface *sur = get_surface(get_address (msg));
	sur->expand_enable = false;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
	if (s) {
		SetStripableSelection (s);
	} else {
		if ((int) (sur->feedback.to_ulong())) {
			float_message_with_id ("/strip/select", ssid, 0, sur->feedback[2], get_address (msg));
		}
	}

	return 0;
}

int
OSC::sel_expand (uint32_t state, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	sur->expand_enable = (bool) state;
	if (state && sur->expand) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}

	return _strip_select (s, get_address (msg));
}

int
OSC::route_set_gain_abs (int ssid, float level, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
	OSCSurface *sur = get_surface(get_address (msg));

	if (s) {
		if (s->gain_control()) {
			fake_touch (s->gain_control());
			s->gain_control()->set_value (level, sur->usegroup);
		} else {
			return 1;
		}
	} else {
		return 1;
	}

	return 0;
}

int
OSC::route_set_gain_dB (int ssid, float dB, lo_message msg)
{
	if (!session) {
		return -1;
	}
	OSCSurface *sur = get_surface(get_address (msg));
	int ret;
	if (dB < -192) {
		ret = route_set_gain_abs (ssid, 0.0, msg);
	} else {
		ret = route_set_gain_abs (ssid, dB_to_coefficient (dB), msg);
	}
	if (ret != 0) {
		return float_message_with_id ("/strip/gain", ssid, -193, sur->feedback[2], get_address (msg));
	}
	return 0;
}

int
OSC::sel_gain (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		float abs;
		if (s->gain_control()) {
			if (val < -192) {
				abs = 0;
			} else {
				abs = dB_to_coefficient (val);
				float top = s->gain_control()->upper();
				if (abs > top) {
					abs = top;
				}
			}
			fake_touch (s->gain_control());
			s->gain_control()->set_value (abs, PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/gain", -193, get_address (msg));
}

int
OSC::sel_dB_delta (float delta, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->gain_control()) {
			float dB = accurate_coefficient_to_dB (s->gain_control()->get_value()) + delta;
			float abs;
			if (dB < -192) {
				abs = 0;
			} else {
				abs = dB_to_coefficient (dB);
				float top = s->gain_control()->upper();
				if (abs > top) {
					abs = top;
				}
			}
			fake_touch (s->gain_control());
			s->gain_control()->set_value (abs, PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/gain", -193, get_address (msg));
}

int
OSC::route_set_gain_fader (int ssid, float pos, lo_message msg)
{
	if (!session) {
		return -1;
	}
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
	OSCSurface *sur = get_surface(get_address (msg));

	if (s) {
		if (s->gain_control()) {
			fake_touch (s->gain_control());
			s->gain_control()->set_value (s->gain_control()->interface_to_internal (pos), sur->usegroup);
		} else {
			return float_message_with_id ("/strip/fader", ssid, 0, sur->feedback[2], get_address (msg));
		}
	} else {
		return float_message_with_id ("/strip/fader", ssid, 0, sur->feedback[2], get_address (msg));
	}
	return 0;
}

int
OSC::strip_db_delta (int ssid, float delta, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
	OSCSurface *sur = get_surface(get_address (msg));
	if (s) {
		float db = accurate_coefficient_to_dB (s->gain_control()->get_value()) + delta;
		float abs;
		if (db < -192) {
			abs = 0;
		} else {
			abs = dB_to_coefficient (db);
			float top = s->gain_control()->upper();
			if (abs > top) {
				abs = top;
			}
		}
		s->gain_control()->set_value (abs, sur->usegroup);
		return 0;
	}
	return -1;
}

int
OSC::sel_fader (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->gain_control()) {
			fake_touch (s->gain_control());
			s->gain_control()->set_value (s->gain_control()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/fader", 0, get_address (msg));
}

int
OSC::route_set_trim_abs (int ssid, float level, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
	OSCSurface *sur = get_surface(get_address (msg));

	if (s) {
		if (s->trim_control()) {
			s->trim_control()->set_value (level, sur->usegroup);
			return 0;
		}

	}

	return -1;
}

int
OSC::route_set_trim_dB (int ssid, float dB, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	int ret;
	ret = route_set_trim_abs(ssid, dB_to_coefficient (dB), msg);
	if (ret != 0) {
		return float_message_with_id ("/strip/trimdB", ssid, 0, sur->feedback[2], get_address (msg));
	}

return 0;
}

int
OSC::sel_trim (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->trim_control()) {
			s->trim_control()->set_value (dB_to_coefficient (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/trimdB", 0, get_address (msg));
}

int
OSC::sel_pan_position (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if(s->pan_azimuth_control()) {
			s->pan_azimuth_control()->set_value (s->pan_azimuth_control()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/pan_stereo_position", 0.5, get_address (msg));
}

int
OSC::sel_pan_width (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->pan_width_control()) {
			s->pan_width_control()->set_value (s->pan_width_control()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/pan_stereo_width", 1, get_address (msg));
}

int
OSC::route_set_pan_stereo_position (int ssid, float pos, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
	OSCSurface *sur = get_surface(get_address (msg));

	if (s) {
		if(s->pan_azimuth_control()) {
			s->pan_azimuth_control()->set_value (s->pan_azimuth_control()->interface_to_internal (pos), sur->usegroup);
			return 0;
		}
	}

	return float_message_with_id ("/strip/pan_stereo_position", ssid, 0.5, sur->feedback[2], get_address (msg));
}

int
OSC::route_set_pan_stereo_width (int ssid, float pos, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
	OSCSurface *sur = get_surface(get_address (msg));

	if (s) {
		if (s->pan_width_control()) {
			s->pan_width_control()->set_value (pos, sur->usegroup);
			return 0;
		}
	}

	return float_message_with_id ("/strip/pan_stereo_width", ssid, 1, sur->feedback[2], get_address (msg));
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
#ifdef MIXBUS
		abs = val;
#else
		if (val < -192) {
			abs = 0;
		} else {
			abs = dB_to_coefficient (val);
		}
#endif
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
		return float_message_with_id ("/select/send_gain", id, -193, sur->feedback[2], get_address (msg));
	}
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	float abs;
	int send_id = 0;
	if (s) {
		if (id > 0) {
			send_id = id - 1;
		}
#ifdef MIXBUS
		abs = val;
#else
		if (val < -192) {
			abs = 0;
		} else {
			abs = dB_to_coefficient (val);
		}
#endif
		if (sur->send_page_size) {
			send_id = send_id + ((sur->send_page - 1) * sur->send_page_size);
		}
		if (s->send_level_controllable (send_id)) {
			s->send_level_controllable (send_id)->set_value (abs, PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message_with_id ("/select/send_gain", id, -193, sur->feedback[2], get_address (msg));
}

int
OSC::sel_sendfader (int id, float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	if (sur->send_page_size && (id > (int)sur->send_page_size)) {
		return float_message_with_id ("/select/send_fader", id, 0, sur->feedback[2], get_address (msg));
	}
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
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
	return float_message_with_id ("/select/send_fader", id, 0, sur->feedback[2], get_address (msg));
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
		return float_message_with_id ("/select/send_enable", id, 0, sur->feedback[2], get_address (msg));
	}
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
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
				return float_message_with_id ("/select/send_enable", id, 0, sur->feedback[2], get_address (msg));
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
	return float_message_with_id ("/select/send_enable", id, 0, sur->feedback[2], get_address (msg));
}

int
OSC::sel_master_send_enable (int state, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->master_send_enable_controllable ()) {
			s->master_send_enable_controllable()->set_value (state, PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message ("/select/master_send_enable", 0, get_address(msg));
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
			_sel_plugin (piid, msg);
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
		return float_message_with_id ("/select/plugin/parameter", paid, 0, sur->feedback[2], get_address (msg));
	}
	if (sur->plug_page_size && (paid > (int)sur->plug_page_size)) {
		return float_message_with_id ("/select/plugin/parameter", paid, 0, sur->feedback[2], get_address (msg));
	}
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
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
	int parid = paid + (int)(sur->plug_page_size * (sur->plug_page - 1));
	if (parid > (int) sur->plug_params.size ()) {
		if (sur->feedback[13]) {
			float_message_with_id ("/select/plugin/parameter", paid, 0, sur->feedback[2], get_address (msg));
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

	lo_send_message (get_address (msg), "/strip/plugin/list", reply);
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

		lo_send_message (get_address (msg), "/strip/plugin/descriptor", reply);
		lo_message_free (reply);
	}

	lo_message reply = lo_message_new ();
	lo_message_add_int32 (reply, ssid);
	lo_message_add_int32 (reply, piid);
	lo_send_message (get_address (msg), "/strip/plugin/descriptor_end", reply);
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
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->pan_elevation_control()) {
			s->pan_elevation_control()->set_value (s->pan_elevation_control()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/pan_elevation_position", 0, get_address (msg));
}

int
OSC::sel_pan_frontback (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->pan_frontback_control()) {
			s->pan_frontback_control()->set_value (s->pan_frontback_control()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/pan_frontback_position", 0.5, get_address (msg));
}

int
OSC::sel_pan_lfe (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->pan_lfe_control()) {
			s->pan_lfe_control()->set_value (s->pan_lfe_control()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/pan_lfe_control", 0, get_address (msg));
}

// compressor control
int
OSC::sel_comp_enable (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->comp_enable_controllable()) {
			s->comp_enable_controllable()->set_value (s->comp_enable_controllable()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/comp_enable", 0, get_address (msg));
}

int
OSC::sel_comp_threshold (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->comp_threshold_controllable()) {
			s->comp_threshold_controllable()->set_value (s->comp_threshold_controllable()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/comp_threshold", 0, get_address (msg));
}

int
OSC::sel_comp_speed (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->comp_speed_controllable()) {
			s->comp_speed_controllable()->set_value (s->comp_speed_controllable()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/comp_speed", 0, get_address (msg));
}

int
OSC::sel_comp_mode (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->comp_mode_controllable()) {
			s->comp_mode_controllable()->set_value (s->comp_mode_controllable()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/comp_mode", 0, get_address (msg));
}

int
OSC::sel_comp_makeup (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->comp_makeup_controllable()) {
			s->comp_makeup_controllable()->set_value (s->comp_makeup_controllable()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/comp_makeup", 0, get_address (msg));
}

// EQ control

int
OSC::sel_eq_enable (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->eq_enable_controllable()) {
			s->eq_enable_controllable()->set_value (s->eq_enable_controllable()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/eq_enable", 0, get_address (msg));
}

int
OSC::sel_eq_hpf_freq (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->filter_freq_controllable(true)) {
			s->filter_freq_controllable(true)->set_value (s->filter_freq_controllable(true)->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/eq_hpf/freq", 0, get_address (msg));
}

int
OSC::sel_eq_lpf_freq (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->filter_freq_controllable(false)) {
			s->filter_freq_controllable(false)->set_value (s->filter_freq_controllable(false)->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/eq_lpf/freq", 0, get_address (msg));
}

int
OSC::sel_eq_hpf_enable (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->filter_enable_controllable(true)) {
			s->filter_enable_controllable(true)->set_value (s->filter_enable_controllable(true)->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/eq_hpf/enable", 0, get_address (msg));
}

int
OSC::sel_eq_lpf_enable (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->filter_enable_controllable(false)) {
			s->filter_enable_controllable(false)->set_value (s->filter_enable_controllable(false)->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/eq_lpf/enable", 0, get_address (msg));
}

int
OSC::sel_eq_hpf_slope (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->filter_slope_controllable(true)) {
			s->filter_slope_controllable(true)->set_value (s->filter_slope_controllable(true)->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/eq_hpf/slope", 0, get_address (msg));
}

int
OSC::sel_eq_lpf_slope (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->filter_slope_controllable(false)) {
			s->filter_slope_controllable(false)->set_value (s->filter_slope_controllable(false)->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message("/select/eq_lpf/slope", 0, get_address (msg));
}

int
OSC::sel_eq_gain (int id, float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (id > 0) {
			--id;
		}
		if (s->eq_gain_controllable (id)) {
			s->eq_gain_controllable (id)->set_value (s->eq_gain_controllable(id)->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message_with_id ("/select/eq_gain", id + 1, 0, sur->feedback[2], get_address (msg));
}

int
OSC::sel_eq_freq (int id, float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (id > 0) {
			--id;
		}
		if (s->eq_freq_controllable (id)) {
			s->eq_freq_controllable (id)->set_value (s->eq_freq_controllable(id)->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message_with_id ("/select/eq_freq", id + 1, 0, sur->feedback[2], get_address (msg));
}

int
OSC::sel_eq_q (int id, float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (id > 0) {
			--id;
		}
		if (s->eq_q_controllable (id)) {
			s->eq_q_controllable (id)->set_value (s->eq_q_controllable(id)->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message_with_id ("/select/eq_q", id + 1, 0, sur->feedback[2], get_address (msg));
}

int
OSC::sel_eq_shape (int id, float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (id > 0) {
			--id;
		}
		if (s->eq_shape_controllable (id)) {
			s->eq_shape_controllable (id)->set_value (s->eq_shape_controllable(id)->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return float_message_with_id ("/select/eq_shape", id + 1, 0, sur->feedback[2], get_address (msg));
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
		int64_t now = ARDOUR::get_microseconds ();
		int64_t diff = now - scrub_time;
		if (diff > 120000) {
			scrub_speed = 0;
			session->request_transport_speed (0);
			// locate to the place PH was at last tick
			session->request_locate (scrub_place, false);
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
			ctrl->stop_touch (ctrl->session().transport_sample());
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
	node.set_property ("debugmode", (int32_t) _debugmode); // TODO: enum2str
	node.set_property ("address-only", address_only);
	node.set_property ("remote-port", remote_port);
	node.set_property ("banksize", default_banksize);
	node.set_property ("striptypes", default_strip);
	node.set_property ("feedback", default_feedback);
	node.set_property ("gainmode", default_gainmode);
	node.set_property ("send-page-size", default_send_size);
	node.set_property ("plug-page-size", default_plugin_size);
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
OSC::get_sorted_stripables(std::bitset<32> types, bool cue)
{
	Sorted sorted;
	StripableList stripables;

	// fetch all stripables
	session->get_stripables (stripables);

	// Look for stripables that match bit in sur->strip_types
	for (StripableList::iterator it = stripables.begin(); it != stripables.end(); ++it) {

		boost::shared_ptr<Stripable> s = *it;
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
			} else
#ifdef MIXBUS
			if (types[2] && Profile->get_mixbus() && s->mixbus()) {
				sorted.push_back (s);
			} else
			if (types[7] && boost::dynamic_pointer_cast<Route>(s) && !boost::dynamic_pointer_cast<Track>(s)) {
				if (Profile->get_mixbus() && !s->mixbus()) {
					sorted.push_back (s);
				}
			} else
#endif
			if ((types[2] || types[3] || types[7]) && boost::dynamic_pointer_cast<Route>(s) && !boost::dynamic_pointer_cast<Track>(s)) {
				boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route>(s);
				if (!(s->presentation_info().flags() & PresentationInfo::MidiBus)) {
					// note some older sessions will show midibuses as busses
					if (r->direct_feeds_according_to_reality (session->master_out())) {
						// this is a bus
						if (types[2]) {
							sorted.push_back (s);
						}
					} else {
						// this is an Aux out
						if (types[7]) {
							sorted.push_back (s);
						}
					}
				} else if (types[3]) {
						sorted.push_back (s);
				}
			}
		}
	}
	sort (sorted.begin(), sorted.end(), StripableByPresentationOrder());
	// Master/Monitor might be anywhere... we put them at the end - Sorry ;)
	if (types[5]) {
		sorted.push_back (session->master_out());
	}
	if (types[6]) {
		if (session->monitor_out()) {
			sorted.push_back (session->monitor_out());
		}
	}
	return sorted;
}

int
OSC::cue_parse (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg)
{
	int ret = 1; /* unhandled */

	if (!strncmp (path, "/cue/aux", 8)) {
		// set our Aux bus
		if (argv[0]->f) {
			ret = cue_set (argv[0]->f, msg);
		} else {
			ret = 0;
		}
	}
	else if (!strncmp (path, "/cue/connect", 12)) {
		// Connect to default Aux bus
		if ((!argc) || argv[0]->f) {
			ret = cue_set (1, msg);
		} else {
			ret = 0;
		}
	}
	else if (!strncmp (path, "/cue/next_aux", 13)) {
		// switch to next Aux bus
		if ((!argc) || argv[0]->f) {
			ret = cue_next (msg);
		} else {
			ret = 0;
		}
	}
	else if (!strncmp (path, "/cue/previous_aux", 17)) {
		// switch to previous Aux bus
		if ((!argc) || argv[0]->f) {
			ret = cue_previous (msg);
		} else {
			ret = 0;
		}
	}
	else if (!strncmp (path, "/cue/send/fader/", 16) && strlen (path) > 16) {
		int id = atoi (&path[16]);
		ret = cue_send_fader (id, argv[0]->f, msg);
	}
	else if (!strncmp (path, "/cue/send/enable/", 17) && strlen (path) > 17) {
		int id = atoi (&path[17]);
		ret = cue_send_enable (id, argv[0]->f, msg);
	}
	else if (!strncmp (path, "/cue/fader", 10)) {
		ret = cue_aux_fader (argv[0]->f, msg);
	}
	else if (!strncmp (path, "/cue/mute", 9)) {
		ret = cue_aux_mute (argv[0]->f, msg);
	}

	return ret;
}

int
OSC::cue_set (uint32_t aux, lo_message msg)
{
	set_surface_feedback (0, msg);
	return _cue_set (aux, get_address (msg));
}

int
OSC::_cue_set (uint32_t aux, lo_address addr)
{
	int ret = 1;
	OSCSurface *s = get_surface(addr);
	s->bank_size = 0;
	s->strip_types = 128;
	s->feedback = 0;
	s->gainmode = 1;
	s->cue = true;
	s->strips = get_sorted_stripables(s->strip_types, s->cue);

	s->nstrips = s->strips.size();

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
			text_message (string_compose ("/cue/name/%1", n+1), stp->name(), addr);
			if (aux == n+1) {
				// aux must be at least one
				// need a signal if aux vanishes
				stp->DropReferences.connect (*this, MISSING_INVALIDATOR, boost::bind (&OSC::_cue_set, this, aux, addr), this);

				// make a list of stripables with sends that go to this bus
				s->sends = cue_get_sorted_stripables(stp, aux, addr);
				if (s->cue_obs) {
					s->cue_obs->refresh_strip (false);
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
OSC::cue_next (lo_message msg)
{
	OSCSurface *s = get_surface(get_address (msg));
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
	OSCSurface *s = get_surface(get_address (msg));
	int ret = 1;
	if (!s->cue) {
		ret = cue_set (1, msg);
	}
	if (s->aux > 1) {
		ret = cue_set (s->aux - 1, msg);
	}
	return ret;
}

boost::shared_ptr<Send>
OSC::cue_get_send (uint32_t id, lo_address addr)
{
	OSCSurface *s = get_surface(addr);
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

	OSCSurface *sur = get_surface(get_address (msg));
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
	float_message ("/cue/fader", 0, get_address (msg));
	return -1;
}

int
OSC::cue_aux_mute (float state, lo_message msg)
{
	if (!session) return -1;

	OSCSurface *sur = get_surface(get_address (msg));
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
	float_message ("/cue/mute", 0, get_address (msg));
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
	float_message (string_compose ("/cue/send/fader/%1", id), 0, get_address (msg));
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
	float_message (string_compose ("/cue/send/enable/%1", id), 0, get_address (msg));
	return -1;
}

// generic send message
int
OSC::float_message (string path, float val, lo_address addr)
{

	lo_message reply;
	reply = lo_message_new ();
	lo_message_add_float (reply, (float) val);

	lo_send_message (addr, path.c_str(), reply);
	lo_message_free (reply);

	return 0;
}

int
OSC::float_message_with_id (std::string path, uint32_t ssid, float value, bool in_line, lo_address addr)
{
	lo_message msg = lo_message_new ();
	if (in_line) {
		path = string_compose ("%1/%2", path, ssid);
	} else {
		lo_message_add_int32 (msg, ssid);
	}
	lo_message_add_float (msg, value);

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
	return 0;
}

int
OSC::int_message_with_id (std::string path, uint32_t ssid, int value, bool in_line, lo_address addr)
{
	lo_message msg = lo_message_new ();
	if (in_line) {
		path = string_compose ("%1/%2", path, ssid);
	} else {
		lo_message_add_int32 (msg, ssid);
	}
	lo_message_add_int32 (msg, value);

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
	return 0;
}

int
OSC::text_message (string path, string val, lo_address addr)
{

	lo_message reply;
	reply = lo_message_new ();
	lo_message_add_string (reply, val.c_str());

	lo_send_message (addr, path.c_str(), reply);
	lo_message_free (reply);

	return 0;
}

int
OSC::text_message_with_id (std::string path, uint32_t ssid, std::string val, bool in_line, lo_address addr)
{
	lo_message msg = lo_message_new ();
	if (in_line) {
		path = string_compose ("%1/%2", path, ssid);
	} else {
		lo_message_add_int32 (msg, ssid);
	}

	lo_message_add_string (msg, val.c_str());

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
	return 0;
}

// we have to have a sorted list of stripables that have sends pointed at our aux
// we can use the one in osc.cc to get an aux list
OSC::Sorted
OSC::cue_get_sorted_stripables(boost::shared_ptr<Stripable> aux, uint32_t id, lo_message msg)
{
	Sorted sorted;
	// fetch all stripables
	StripableList stripables;

	session->get_stripables (stripables);

	// Look for stripables that have a send to aux
	for (StripableList::iterator it = stripables.begin(); it != stripables.end(); ++it) {

		boost::shared_ptr<Stripable> s = *it;
		// we only want routes
		boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (s);
		if (r) {
			r->processors_changed.connect  (*this, MISSING_INVALIDATOR, boost::bind (&OSC::recalcbanks, this), this);
			boost::shared_ptr<Send> snd = r->internal_send_for (boost::dynamic_pointer_cast<Route> (aux));
			if (snd) { // test for send to aux
				sorted.push_back (s);
				s->DropReferences.connect (*this, MISSING_INVALIDATOR, boost::bind (&OSC::cue_set, this, id, msg), this);
			}
		}


	}
	sort (sorted.begin(), sorted.end(), StripableByPresentationOrder());

	return sorted;
}

