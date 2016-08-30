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

#include <pbd/convert.h>
#include <pbd/pthread_utils.h>
#include <pbd/file_utils.h>
#include <pbd/failed_constructor.h>

#include "ardour/amp.h"
#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"
#include "ardour/monitor_control.h"
#include "ardour/dB.h"
#include "ardour/filesystem_paths.h"
#include "ardour/panner.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/presentation_info.h"
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
	, address_only (false)
	, remote_port ("8000")
	, default_banksize (0)
	, default_strip (159)
	, default_feedback (0)
	, default_gainmode (0)
	, tick (true)
	, bank_dirty (false)
	, gui (0)
{
	_instance = this;

	session->Exported.connect (*this, MISSING_INVALIDATOR, boost::bind (&OSC::session_exported, this, _1, _2), this);
}

OSC::~OSC()
{
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

	// catch changes to selection for GUI_select mode
	StripableSelectionChanged.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&OSC::gui_selection_changed, this), this);

	// catch track reordering
	// receive routes added
	session->RouteAdded.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&OSC::notify_routes_added, this, _1), this);
	// receive VCAs added
	session->vca_manager().VCAAdded.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&OSC::notify_vca_added, this, _1), this);
	// order changed
	PresentationInfo::Change.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&OSC::recalcbanks, this), this);

	_select = boost::shared_ptr<Stripable>();

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

	periodic_connection.disconnect ();
	session_connections.drop_connections ();
	cueobserver_connections.drop_connections ();
	// Delete any active route observers
	for (RouteObservers::iterator x = route_observers.begin(); x != route_observers.end();) {

		OSCRouteObserver* rc;

		if ((rc = dynamic_cast<OSCRouteObserver*>(*x)) != 0) {
			delete *x;
			x = route_observers.erase (x);
		} else {
			++x;
		}
	}
// Should maybe do global_observers too
	for (GlobalObservers::iterator x = global_observers.begin(); x != global_observers.end();) {

		OSCGlobalObserver* gc;

		if ((gc = dynamic_cast<OSCGlobalObserver*>(*x)) != 0) {
			delete *x;
			x = global_observers.erase (x);
		} else {
			++x;
		}
	}

// delete select observers
	for (uint32_t it = 0; it < _surface.size(); ++it) {
		OSCSurface* sur = &_surface[it];
		OSCSelectObserver* so;
		if ((so = dynamic_cast<OSCSelectObserver*>(sur->sel_obs)) != 0) {
			delete so;
		}
	}

// delete cue observers
	for (CueObservers::iterator x = cue_observers.begin(); x != cue_observers.end();) {

		OSCCueObserver* co;

		if ((co = dynamic_cast<OSCCueObserver*>(*x)) != 0) {
			delete *x;
			x = cue_observers.erase (x);
		} else {
			++x;
		}
	}

	return 0;
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

		REGISTER_CALLBACK (serv, "/set_surface", "iiii", set_surface);
		REGISTER_CALLBACK (serv, "/set_surface/feedback", "i", set_surface_feedback);
		REGISTER_CALLBACK (serv, "/set_surface/bank_size", "i", set_surface_bank_size);
		REGISTER_CALLBACK (serv, "/set_surface/gainmode", "i", set_surface_gainmode);
		REGISTER_CALLBACK (serv, "/set_surface/strip_types", "i", set_surface_strip_types);
		REGISTER_CALLBACK (serv, "/refresh", "", refresh_surface);
		REGISTER_CALLBACK (serv, "/refresh", "f", refresh_surface);
		REGISTER_CALLBACK (serv, "/strip/list", "", routes_list);
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
		REGISTER_CALLBACK (serv, "/rewind", "", rewind);
		REGISTER_CALLBACK (serv, "/rewind", "f", rewind);
		REGISTER_CALLBACK (serv, "/ffwd", "", ffwd);
		REGISTER_CALLBACK (serv, "/ffwd", "f", ffwd);
		REGISTER_CALLBACK (serv, "/transport_stop", "", transport_stop);
		REGISTER_CALLBACK (serv, "/transport_stop", "f", transport_stop);
		REGISTER_CALLBACK (serv, "/transport_play", "", transport_play);
		REGISTER_CALLBACK (serv, "/transport_play", "f", transport_play);
		REGISTER_CALLBACK (serv, "/transport_frame", "", transport_frame);
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
		REGISTER_CALLBACK (serv, "/bank_up", "f", bank_up);
		REGISTER_CALLBACK (serv, "/bank_down", "", bank_down);
		REGISTER_CALLBACK (serv, "/bank_down", "f", bank_down);

		// controls for "special" strips
		REGISTER_CALLBACK (serv, "/master/gain", "f", master_set_gain);
		REGISTER_CALLBACK (serv, "/master/fader", "f", master_set_fader);
		REGISTER_CALLBACK (serv, "/master/mute", "i", master_set_mute);
		REGISTER_CALLBACK (serv, "/master/trimdB", "f", master_set_trim);
		REGISTER_CALLBACK (serv, "/master/pan_stereo_position", "f", master_set_pan_stereo_position);
		REGISTER_CALLBACK (serv, "/monitor/gain", "f", monitor_set_gain);
		REGISTER_CALLBACK (serv, "/monitor/fader", "f", monitor_set_fader);
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
		REGISTER_CALLBACK (serv, "/select/trimdB", "f", sel_trim);
		REGISTER_CALLBACK (serv, "/select/pan_stereo_position", "f", sel_pan_position);
		REGISTER_CALLBACK (serv, "/select/pan_stereo_width", "f", sel_pan_width);
		REGISTER_CALLBACK (serv, "/select/send_gain", "if", sel_sendgain);
		REGISTER_CALLBACK (serv, "/select/send_fader", "if", sel_sendfader);
		REGISTER_CALLBACK (serv, "/select/send_enable", "if", sel_sendenable);
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
		REGISTER_CALLBACK (serv, "/select/eq_hpf", "f", sel_eq_hpf);
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
		 * other handler matches (used for debug) */
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
OSC::listen_to_route (boost::shared_ptr<Stripable> strip, lo_address addr)
{
	if (!strip) {
		return;
	}
	/* avoid duplicate listens */

	for (RouteObservers::iterator x = route_observers.begin(); x != route_observers.end(); ++x) {

		OSCRouteObserver* ro;

		if ((ro = dynamic_cast<OSCRouteObserver*>(*x)) != 0) {

			int res = strcmp(lo_address_get_url(ro->address()), lo_address_get_url(addr));

			if (ro->strip() == strip && res == 0) {
				return;
			}
		}
	}

	OSCSurface *s = get_surface(addr);
	uint32_t ssid = get_sid (strip, addr);
	OSCRouteObserver* o = new OSCRouteObserver (strip, addr, ssid, s);
	route_observers.push_back (o);

	strip->DropReferences.connect (*this, MISSING_INVALIDATOR, boost::bind (&OSC::route_lost, this, boost::weak_ptr<Stripable> (strip)), this);
}

void
OSC::route_lost (boost::weak_ptr<Stripable> wr)
{
	tick = false;
	drop_route (wr);
	bank_dirty = true;
}

void
OSC::drop_route (boost::weak_ptr<Stripable> wr)
{
	boost::shared_ptr<Stripable> r = wr.lock ();

	if (!r) {
		return;
	}

	for (RouteObservers::iterator x = route_observers.begin(); x != route_observers.end();) {

		OSCRouteObserver* rc;

		if ((rc = dynamic_cast<OSCRouteObserver*>(*x)) != 0) {

			if (rc->strip() == r) {
				delete *x;
				x = route_observers.erase (x);
			} else {
				++x;
			}
		} else {
			++x;
		}
	}
}

void
OSC::end_listen (boost::shared_ptr<Stripable> r, lo_address addr)
{
	RouteObservers::iterator x;

	// Remove the route observers
	for (x = route_observers.begin(); x != route_observers.end();) {

		OSCRouteObserver* ro;

		if ((ro = dynamic_cast<OSCRouteObserver*>(*x)) != 0) {

			int res = strcmp(lo_address_get_url(ro->address()), lo_address_get_url(addr));

			if (ro->strip() == r && res == 0) {
				delete *x;
				x = route_observers.erase (x);
			}
			else {
				++x;
			}
		}
		else {
			++x;
		}
	}
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

	lo_send_message (get_address (msg), "#reply", reply);
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

	if (len >= 17 && !strcmp (&path[len-15], "/#current_value")) {
		current_value_query (path, len, argv, argc, msg);
		ret = 0;

	} else
	if (!strncmp (path, "/cue/", 5)) {

		cue_parse (path, types, argv, argc, msg);

		ret = 0;
	} else
	if (!strncmp (path, "/access_action/", 15)) {
		if (!(argc && !argv[0]->i)) {
			std::string action_path = path;

			access_action (action_path.substr(15));
			std::cout << "access_action path = " << action_path.substr(15) << "\n";
		}

		ret = 0;
	} else
	if (strcmp (path, "/strip/listen") == 0) {

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

		lo_send_message (get_address (msg), "#reply", reply);
		lo_message_free (reply);

		ret = 0;

	} else
	if (strcmp (path, "/strip/ignore") == 0) {

		for (int n = 0; n < argc; ++n) {

			boost::shared_ptr<Route> r = session->get_remote_nth_route (argv[n]->i);

			if (r) {
				end_listen (r, get_address (msg));
			}
		}

		ret = 0;
	} else
	if (!strncmp (path, "/strip/gain/", 12) && strlen (path) > 12) {
		// in dB
		int ssid = atoi (&path[12]);
		route_set_gain_dB (ssid, argv[0]->f, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/strip/fader/", 13) && strlen (path) > 13) {
		// in fader position
		int ssid = atoi (&path[13]);
		route_set_gain_fader (ssid, argv[0]->f, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/strip/trimdB/", 14) && strlen (path) > 14) {
		int ssid = atoi (&path[14]);
		route_set_trim_dB (ssid, argv[0]->f, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/strip/pan_stereo_position/", 27) && strlen (path) > 27) {
		int ssid = atoi (&path[27]);
		route_set_pan_stereo_position (ssid, argv[0]->f, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/strip/mute/", 12) && strlen (path) > 12) {
		int ssid = atoi (&path[12]);
		route_mute (ssid, argv[0]->i, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/strip/solo/", 12) && strlen (path) > 12) {
		int ssid = atoi (&path[12]);
		route_solo (ssid, argv[0]->i, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/strip/monitor_input/", 21) && strlen (path) > 21) {
		int ssid = atoi (&path[21]);
		route_monitor_input (ssid, argv[0]->i, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/strip/monitor_disk/", 20) && strlen (path) > 20) {
		int ssid = atoi (&path[20]);
		route_monitor_disk (ssid, argv[0]->i, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/strip/recenable/", 17) && strlen (path) > 17) {
		int ssid = atoi (&path[17]);
		route_recenable (ssid, argv[0]->i, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/strip/record_safe/", 19) && strlen (path) > 19) {
		int ssid = atoi (&path[19]);
		route_recsafe (ssid, argv[0]->i, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/strip/expand/", 14) && strlen (path) > 14) {
		int ssid = atoi (&path[14]);
		strip_expand (ssid, argv[0]->i, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/strip/select/", 14) && strlen (path) > 14) {
		int ssid = atoi (&path[14]);
		strip_gui_select (ssid, argv[0]->i, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/select/send_gain/", 18) && strlen (path) > 18) {
		int ssid = atoi (&path[18]);
		sel_sendgain (ssid, argv[0]->f, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/select/send_fader/", 19) && strlen (path) > 19) {
		int ssid = atoi (&path[19]);
		sel_sendfader (ssid, argv[0]->f, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/select/send_enable/", 20) && strlen (path) > 20) {
		int ssid = atoi (&path[20]);
		sel_sendenable (ssid, argv[0]->f, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/select/eq_gain/", 16) && strlen (path) > 16) {
		int ssid = atoi (&path[16]);
		sel_eq_gain (ssid, argv[0]->f, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/select/eq_freq/", 16) && strlen (path) > 16) {
		int ssid = atoi (&path[16]);
		sel_eq_freq (ssid, argv[0]->f , msg);
		ret = 0;
	}
	else if (!strncmp (path, "/select/eq_q/", 13) && strlen (path) > 13) {
		int ssid = atoi (&path[13]);
		sel_eq_q (ssid, argv[0]->f, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/select/eq_shape/", 17) && strlen (path) > 17) {
		int ssid = atoi (&path[17]);
		sel_eq_shape (ssid, argv[0]->f, msg);
		ret = 0;
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
			lo_send (addr, retpath, "i", session->transport_frame());
		}

	} else if (strcmp (argv[0]->s, "transport_speed") == 0) {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_frame());
		}

	} else if (strcmp (argv[0]->s, "transport_locked") == 0) {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_frame());
		}

	} else if (strcmp (argv[0]->s, "punch_in") == 0) {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_frame());
		}

	} else if (strcmp (argv[0]->s, "punch_out") == 0) {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_frame());
		}

	} else if (strcmp (argv[0]->s, "rec_enable") == 0) {

		if (session) {
			lo_send (addr, retpath, "i", session->transport_frame());
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
	OSCSurface *sur = get_surface(get_address (msg));
	sur->no_clear = true;

	for (int n = 0; n < (int) sur->nstrips; ++n) {

		boost::shared_ptr<Stripable> s = get_strip (n + 1, get_address (msg));

		if (s) {
			// some things need the route
			boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (s);

			lo_message reply = lo_message_new ();

			if (s->presentation_info().flags() & PresentationInfo::AudioTrack) {
				lo_message_add_string (reply, "AT");
			} else if (s->presentation_info().flags() & PresentationInfo::MidiTrack) {
				lo_message_add_string (reply, "MT");
			} else if (s->presentation_info().flags() & PresentationInfo::AudioBus) {
				// r->feeds (session->master_out()) may make more sense
				if (r->direct_feeds_according_to_reality (session->master_out())) {
					// this is a bus
					lo_message_add_string (reply, "B");
				} else {
					// this is an Aux out
					lo_message_add_string (reply, "AX");
				}
			} else if (s->presentation_info().flags() & PresentationInfo::MidiBus) {
				lo_message_add_string (reply, "MB");
			} else if (s->presentation_info().flags() & PresentationInfo::VCA) {
				lo_message_add_string (reply, "V");
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

			//Automatically listen to stripables listed
			listen_to_route(s, get_address (msg));

			lo_send_message (get_address (msg), "#reply", reply);
			lo_message_free (reply);
		}
	}

	// Send end of listing message
	lo_message reply = lo_message_new ();

	lo_message_add_string (reply, "end_route_list");
	lo_message_add_int64 (reply, session->frame_rate());
	lo_message_add_int64 (reply, session->current_end_frame());
	if (session->monitor_out()) {
		// this session has a monitor section
		lo_message_add_int32 (reply, 1);
	} else {
		lo_message_add_int32 (reply, 0);
	}

	lo_send_message (get_address (msg), "#reply", reply);

	lo_message_free (reply);
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
	if (address_only) {
		// get rid of all surfaces and observers.
		// needs change to only clear those for this address on all ports
		clear_devices();
	}
	OSCSurface *s = get_surface(get_address (msg));
	// restart all observers
	set_surface (s->bank_size, (uint32_t) s->strip_types.to_ulong(), (uint32_t) s->feedback.to_ulong(), (uint32_t) s->gainmode, msg);
	return 0;
}

void
OSC::clear_devices ()
{
	for (RouteObservers::iterator x = route_observers.begin(); x != route_observers.end();) {

		OSCRouteObserver* rc;

		if ((rc = dynamic_cast<OSCRouteObserver*>(*x)) != 0) {
			delete *x;
			x = route_observers.erase (x);
		} else {
			++x;
		}
		// slow devices need time to clear buffers
		usleep ((uint32_t) 10);
	}
	// Should maybe do global_observers too
	for (GlobalObservers::iterator x = global_observers.begin(); x != global_observers.end();) {

		OSCGlobalObserver* gc;

		if ((gc = dynamic_cast<OSCGlobalObserver*>(*x)) != 0) {
			delete *x;
			x = global_observers.erase (x);
		} else {
			++x;
		}
	}
	// delete select observers
	for (uint32_t it = 0; it < _surface.size(); ++it) {
		OSCSurface* sur = &_surface[it];
		OSCSelectObserver* so;
		if ((so = dynamic_cast<OSCSelectObserver*>(sur->sel_obs)) != 0) {
			delete so;
		}
	}
	// delete cue observers
	for (CueObservers::iterator x = cue_observers.begin(); x != cue_observers.end();) {
		OSCCueObserver* co;
		if ((co = dynamic_cast<OSCCueObserver*>(*x)) != 0) {
			delete co;
		} else {
			++x;
		}
	}

	// clear out surfaces
	_surface.clear();
}

int
OSC::set_surface (uint32_t b_size, uint32_t strips, uint32_t fb, uint32_t gm, lo_message msg)
{
	OSCSurface *s = get_surface(get_address (msg));
	s->bank_size = b_size;
	s->strip_types = strips;
	s->feedback = fb;
	s->gainmode = gm;
	// set bank and strip feedback
	set_bank(s->bank, msg);

	global_feedback (s->feedback, get_address (msg), s->gainmode);
	return 0;
}

int
OSC::set_surface_bank_size (uint32_t bs, lo_message msg)
{
	OSCSurface *s = get_surface(get_address (msg));
	s->bank_size = bs;

	// set bank and strip feedback
	set_bank(s->bank, msg);
	return 0;
}

int
OSC::set_surface_strip_types (uint32_t st, lo_message msg)
{
	OSCSurface *s = get_surface(get_address (msg));
	s->strip_types = st;

	// set bank and strip feedback
	set_bank(s->bank, msg);
	return 0;
}


int
OSC::set_surface_feedback (uint32_t fb, lo_message msg)
{
	OSCSurface *s = get_surface(get_address (msg));
	s->feedback = fb;

	// set bank and strip feedback
	set_bank(s->bank, msg);

	// Set global/master feedback
	global_feedback (s->feedback, get_address (msg), s->gainmode);
	return 0;
}


int
OSC::set_surface_gainmode (uint32_t gm, lo_message msg)
{
	OSCSurface *s = get_surface(get_address (msg));
	s->gainmode = gm;

	// set bank and strip feedback
	set_bank(s->bank, msg);

	// Set global/master feedback
	global_feedback (s->feedback, get_address (msg), s->gainmode);
	return 0;
}

OSC::OSCSurface *
OSC::get_surface (lo_address addr)
{
	string r_url;
	char * rurl;
	rurl = lo_address_get_url (addr);
	r_url = rurl;
	free (rurl);
	for (uint32_t it = 0; it < _surface.size(); ++it) {
		//find setup for this server
		if (!_surface[it].remote_url.find(r_url)){
			return &_surface[it];
		}
	}
	// if we do this when OSC is started we get the wrong stripable
	// we don't need this until we actually have a surface to deal with
	if (!_select || (_select != ControlProtocol::first_selected_stripable())) {
		gui_selection_changed();
	}

	// No surface create one with default values
	OSCSurface s;
	s.remote_url = r_url;
	s.bank = 1;
	s.bank_size = default_banksize; // need to find out how many strips there are
	s.strip_types = default_strip; // 159 is tracks, busses, and VCAs (no master/monitor)
	s.feedback = default_feedback;
	s.gainmode = default_gainmode;
	s.sel_obs = 0;
	s.expand = 0;
	s.expand_enable = false;
	s.cue = false;
	s.strips = get_sorted_stripables(s.strip_types, s.cue);

	s.nstrips = s.strips.size();
	_surface.push_back (s);

	return &_surface[_surface.size() - 1];
}

// setup global feedback for a surface
void
OSC::global_feedback (bitset<32> feedback, lo_address addr, uint32_t gainmode)
{
	// first destroy global observer for this surface
	GlobalObservers::iterator x;
	for (x = global_observers.begin(); x != global_observers.end();) {

		OSCGlobalObserver* ro;

		if ((ro = dynamic_cast<OSCGlobalObserver*>(*x)) != 0) {

			int res = strcmp(lo_address_get_url(ro->address()), lo_address_get_url(addr));

			if (res == 0) {
				delete *x;
				x = global_observers.erase (x);
			} else {
				++x;
			}
		} else {
			++x;
		}
	}
	if (feedback[4] || feedback[3] || feedback[5] || feedback[6]) {
		// create a new Global Observer for this surface
		OSCGlobalObserver* o = new OSCGlobalObserver (*session, addr, gainmode, /*s->*/feedback);
		global_observers.push_back (o);
	}
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
	if (!_select || (_select != ControlProtocol::first_selected_stripable())) {
		_select = ControlProtocol::first_selected_stripable();
	}

	// do a set_bank for each surface we know about.
	for (uint32_t it = 0; it < _surface.size(); ++it) {
		OSCSurface* sur = &_surface[it];
		// find lo_address
		lo_address addr = lo_address_new_from_url (sur->remote_url.c_str());
		if (sur->cue) {
			_cue_set (sur->aux, addr);
		} else {
			_set_bank (sur->bank, addr);
		}
		if (sur->no_clear) {
			// This surface uses /strip/list tell it routes have changed
			lo_message reply;
			reply = lo_message_new ();
			lo_send_message (addr, "/strip/list", reply);
			lo_message_free (reply);
		}
	}
}

/*
 * This gets called not only when bank changes but also:
 *  - bank size change
 *  - feedback change
 *  - strip types changes
 *  - fadermode changes
 *  - stripable creation/deletion/flag
 *  - to refresh what is "displayed"
 * Basically any time the bank needs to be rebuilt
 */
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
	// no nstripables yet
	if (!session->nroutes()) {
		return -1;
	}

	OSCSurface *s = get_surface (addr);

	// revert any expand to select
	 s->expand = 0;
	 s->expand_enable = false;
	_strip_select (ControlProtocol::first_selected_stripable(), addr);

	// undo all listeners for this url
	StripableList stripables;
	session->get_stripables (stripables);
	for (StripableList::iterator it = stripables.begin(); it != stripables.end(); ++it) {

		boost::shared_ptr<Stripable> stp = *it;
		if (stp) {
			end_listen (stp, addr);
		}
		// slow devices need time to clear buffers
		usleep ((uint32_t) 10);
	}

	s->strips = get_sorted_stripables(s->strip_types, s->cue);
	s->nstrips = s->strips.size();

	uint32_t b_size;
	if (!s->bank_size) {
		// no banking - bank includes all stripables
		b_size = s->nstrips;
	} else {
		b_size = s->bank_size;
	}

	// Do limits checking
	if (bank_start < 1) bank_start = 1;
	if (b_size >= s->nstrips)  {
		bank_start = 1;
	} else if (bank_start > ((s->nstrips - b_size) + 1)) {
		// top bank is always filled if there are enough strips for at least one bank
		bank_start = (uint32_t)((s->nstrips - b_size) + 1);
	}
	//save bank after bank limit checks
	s->bank = bank_start;

	if (s->feedback[0] || s->feedback[1]) {

		for (uint32_t n = bank_start; n < (min ((b_size + bank_start), s->nstrips + 1)); ++n) {
			if (n <= s->strips.size()) {
				boost::shared_ptr<Stripable> stp = s->strips[n - 1];

				if (stp) {
					listen_to_route(stp, addr);
				}
			}
			// slow devices need time to clear buffers
			usleep ((uint32_t) 10);
		}
	}
	// light bankup or bankdown buttons if it is possible to bank in that direction
	if (s->feedback[4] && !s->no_clear) {
		lo_message reply;
		reply = lo_message_new ();
		if ((s->bank > (s->nstrips - s->bank_size)) || (s->nstrips < s->bank_size)) {
			lo_message_add_int32 (reply, 0);
		} else {
			lo_message_add_int32 (reply, 1);
		}
		lo_send_message (addr, "/bank_up", reply);
		lo_message_free (reply);
		reply = lo_message_new ();
		if (s->bank > 1) {
			lo_message_add_int32 (reply, 1);
		} else {
			lo_message_add_int32 (reply, 0);
		}
		lo_send_message (addr, "/bank_down", reply);
		lo_message_free (reply);
	}
	bank_dirty = false;
	tick = true;
	return 0;
}

int
OSC::bank_up (lo_message msg)
{
	if (!session) {
		return -1;
	}
	OSCSurface *s = get_surface(get_address (msg));
	set_bank (s->bank + s->bank_size, msg);
	return 0;
}

int
OSC::bank_down (lo_message msg)
{
	if (!session) {
		return -1;
	}
	OSCSurface *s = get_surface(get_address (msg));
	if (s->bank < s->bank_size) {
		set_bank (1, msg);
	} else {
		set_bank (s->bank - s->bank_size, msg);
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

void
OSC::transport_frame (lo_message msg)
{
	if (!session) {
		return;
	}
	framepos_t pos = session->transport_frame ();

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
	int re = (int)session->get_record_enabled ();

	lo_message reply = lo_message_new ();
	lo_message_add_int32 (reply, re);

	lo_send_message (get_address (msg), "/record_enabled", reply);

	lo_message_free (reply);
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
			s->gain_control()->set_value (dB_to_coefficient (dB), PBD::Controllable::NoGroup);
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
		s->gain_control()->set_value (slider_position_to_gain_with_max (position, 2.0), PBD::Controllable::NoGroup);
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

	float endposition = .5;
	boost::shared_ptr<Stripable> s = session->master_out();

	if (s) {
		if (s->pan_azimuth_control()) {
			s->pan_azimuth_control()->set_value (s->pan_azimuth_control()->interface_to_internal (position), PBD::Controllable::NoGroup);
			endposition = s->pan_azimuth_control()->internal_to_interface (s->pan_azimuth_control()->get_value ());
		}
	}
	OSCSurface *sur = get_surface(get_address (msg));

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
OSC::monitor_set_gain (float dB)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = session->monitor_out();

	if (s) {
		if (dB < -192) {
			s->gain_control()->set_value (0.0, PBD::Controllable::NoGroup);
		} else {
			s->gain_control()->set_value (dB_to_coefficient (dB), PBD::Controllable::NoGroup);
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
		s->gain_control()->set_value (slider_position_to_gain_with_max (position, 2.0), PBD::Controllable::NoGroup);
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
			lo_message_add_float(reply, gain_to_slider_position(a->gain_control()->get_value()));
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
					lo_message_add_float(reply, gain_to_slider_position(a->gain_control()->get_value()));
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
OSC::route_mute (int ssid, int yn, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));

	if (s) {
		if (s->mute_control()) {
			s->mute_control()->set_value (yn ? 1.0 : 0.0, PBD::Controllable::NoGroup);
			return 0;
		}
	}

	return route_send_fail ("mute", ssid, 0, get_address (msg));
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
	return sel_fail ("mute", 0, get_address (msg));
}

int
OSC::route_solo (int ssid, int yn, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));

	if (s) {
		if (s->solo_control()) {
			s->solo_control()->set_value (yn ? 1.0 : 0.0, PBD::Controllable::NoGroup);
		}
	}

	return route_send_fail ("solo", ssid, 0, get_address (msg));
}

int
OSC::route_solo_iso (int ssid, int yn, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));

	if (s) {
		if (s->solo_isolate_control()) {
			s->solo_isolate_control()->set_value (yn ? 1.0 : 0.0, PBD::Controllable::NoGroup);
			return 0;
		}
	}

	return route_send_fail ("solo_iso", ssid, 0, get_address (msg));
}

int
OSC::route_solo_safe (int ssid, int yn, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, lo_message_get_source (msg));

	if (s) {
		if (s->solo_safe_control()) {
			s->solo_safe_control()->set_value (yn ? 1.0 : 0.0, PBD::Controllable::NoGroup);
			return 0;
		}
	}

	return route_send_fail ("solo_safe", ssid, 0, get_address (msg));
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
	return sel_fail ("solo", 0, get_address (msg));
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
	return sel_fail ("solo_iso", 0, get_address (msg));
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
	return sel_fail ("solo_safe", 0, get_address (msg));
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
	return sel_fail ("recenable", 0, get_address (msg));
}

int
OSC::route_recenable (int ssid, int yn, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));

	if (s) {
		if (s->rec_enable_control()) {
			s->rec_enable_control()->set_value (yn, PBD::Controllable::UseGroup);
			if (s->rec_enable_control()->get_value()) {
				return 0;
			}
		}
	}
	return route_send_fail ("recenable", ssid, 0, get_address (msg));
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
	return sel_fail ("record_safe", 0, get_address (msg));
}

int
OSC::route_recsafe (int ssid, int yn, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
	if (s) {
		if (s->rec_safe_control()) {
			s->rec_safe_control()->set_value (yn, PBD::Controllable::UseGroup);
			if (s->rec_safe_control()->get_value()) {
				return 0;
			}
		}
	}
	return route_send_fail ("record_safe", ssid, 0,get_address (msg));
}

int
OSC::route_monitor_input (int ssid, int yn, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));

	if (s) {
		boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (s);
		if (track) {
			if (track->monitoring_control()) {
				track->monitoring_control()->set_value (yn ? 1.0 : 0.0, PBD::Controllable::NoGroup);
				return 0;
			}
		}
	}

	return route_send_fail ("monitor_input", ssid, 0, get_address (msg));
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
				track->monitoring_control()->set_value (yn ? 1.0 : 0.0, PBD::Controllable::NoGroup);
				return 0;
			}
		}
	}
	return sel_fail ("monitor_input", 0, get_address (msg));
}

int
OSC::route_monitor_disk (int ssid, int yn, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));

	if (s) {
		boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (s);
		if (track) {
			if (track->monitoring_control()) {
				track->monitoring_control()->set_value (yn ? 2.0 : 0.0, PBD::Controllable::NoGroup);
				return 0;
			}
		}
	}

	return route_send_fail ("monitor_disk", ssid, 0, get_address (msg));
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
				track->monitoring_control()->set_value (yn ? 2.0 : 0.0, PBD::Controllable::NoGroup);
				return 0;
			}
		}
	}
	return sel_fail ("monitor_disk", 0, get_address (msg));
}


int
OSC::strip_phase (int ssid, int yn, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));

	if (s) {
		if (s->phase_control()) {
			s->phase_control()->set_value (yn ? 1.0 : 0.0, PBD::Controllable::NoGroup);
			return 0;
		}
	}

	return route_send_fail ("polarity", ssid, 0, get_address (msg));
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
	return sel_fail ("polarity", 0, get_address (msg));
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
		s = ControlProtocol::first_selected_stripable();
	}

	return _strip_select (s, get_address (msg));
}

int
OSC::_strip_select (boost::shared_ptr<Stripable> s, lo_address addr)
{
	if (!session) {
		return -1;
	}
	OSCSurface *sur = get_surface(addr);
	if (sur->sel_obs) {
		delete sur->sel_obs;
		sur->sel_obs = 0;
	}
	bool feedback_on = sur->feedback.to_ulong();
	if (s && feedback_on) {
		OSCSelectObserver* sel_fb = new OSCSelectObserver (s, addr, sur->gainmode, sur->feedback);
		s->DropReferences.connect (*this, MISSING_INVALIDATOR, boost::bind (&OSC::recalcbanks, this), this);
		sur->sel_obs = sel_fb;
	} else if (sur->expand_enable) {
		sur->expand = 0;
		sur->expand_enable = false;
		if (_select && feedback_on) {
			OSCSelectObserver* sel_fb = new OSCSelectObserver (_select, addr, sur->gainmode, sur->feedback);
			_select->DropReferences.connect (*this, MISSING_INVALIDATOR, boost::bind (&OSC::recalcbanks, this), this);
			sur->sel_obs = sel_fb;
		}
	} else if (feedback_on) {
		route_send_fail ("select", sur->expand, 0 , addr);
	}
	if (!feedback_on) {
		return 0;
	}
	//update buttons on surface
	int b_s = sur->bank_size;
	if (!b_s) { // bank size 0 means we need to know how many strips there are.
		b_s = sur->nstrips;
	}
	for (int i = 1;  i <= b_s; i++) {
		string path = "expand";

		if ((i == (int) sur->expand) && sur->expand_enable) {
			lo_message reply = lo_message_new ();
			if (sur->feedback[2]) {
				ostringstream os;
				os << "/strip/" << path << "/" << i;
				path = os.str();
			} else {
				ostringstream os;
				os << "/strip/" << path;
				path = os.str();
				lo_message_add_int32 (reply, i);
			}
			lo_message_add_float (reply, (float) 1);

			lo_send_message (addr, path.c_str(), reply);
			lo_message_free (reply);
			reply = lo_message_new ();
			lo_message_add_float (reply, 1.0);
			lo_send_message (addr, "/select/expand", reply);
			lo_message_free (reply);

		} else {
			lo_message reply = lo_message_new ();
			lo_message_add_int32 (reply, i);
			lo_message_add_float (reply, 0.0);
			lo_send_message (addr, "/strip/expand", reply);
			lo_message_free (reply);
		}
	}
	if (!sur->expand_enable) {
		lo_message reply = lo_message_new ();
		lo_message_add_float (reply, 0.0);
		lo_send_message (addr, "/select/expand", reply);
		lo_message_free (reply);
	}

	return 0;
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
			route_send_fail ("select", ssid, 0, get_address (msg));
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
		s = ControlProtocol::first_selected_stripable();
	}

	return _strip_select (s, get_address (msg));
}

int
OSC::route_set_gain_abs (int ssid, float level, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));

	if (s) {
		if (s->gain_control()) {
			s->gain_control()->set_value (level, PBD::Controllable::NoGroup);
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
		route_send_fail ("gain", ssid, -193, get_address (msg));
		return -1;
	}
	int ret;
	if (dB < -192) {
		ret = route_set_gain_abs (ssid, 0.0, msg);
	} else {
		ret = route_set_gain_abs (ssid, dB_to_coefficient (dB), msg);
	}
	if (ret != 0) {
		return route_send_fail ("gain", ssid, -193, get_address (msg));
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
		if (val < -192) {
			abs = 0;
		} else {
			abs = dB_to_coefficient (val);
		}
		if (s->gain_control()) {
			s->gain_control()->set_value (abs, PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return sel_fail ("gain", -193, get_address (msg));
}

int
OSC::route_set_gain_fader (int ssid, float pos, lo_message msg)
{
	if (!session) {
		route_send_fail ("fader", ssid, 0, get_address (msg));
		return -1;
	}
	int ret;
	ret = route_set_gain_abs (ssid, slider_position_to_gain_with_max (pos, 2.0), msg);
	if (ret != 0) {
		return route_send_fail ("fader", ssid, 0, get_address (msg));
	}
	return 0;
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
		float abs;
		abs = slider_position_to_gain_with_max (val, 2.0);
		if (s->gain_control()) {
			s->gain_control()->set_value (abs, PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return sel_fail ("fader", 0, get_address (msg));
}

int
OSC::route_set_trim_abs (int ssid, float level, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));

	if (s) {
		if (s->trim_control()) {
			s->trim_control()->set_value (level, PBD::Controllable::NoGroup);
			return 0;
		}

	}

	return -1;
}

int
OSC::route_set_trim_dB (int ssid, float dB, lo_message msg)
{
	int ret;
	ret = route_set_trim_abs(ssid, dB_to_coefficient (dB), msg);
	if (ret != 0) {
		return route_send_fail ("trimdB", ssid, 0, get_address (msg));
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
	return sel_fail ("trimdB", 0, get_address (msg));
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
	return sel_fail ("pan_stereo_position", 0.5, get_address (msg));
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
	return sel_fail ("pan_stereo_width", 1, get_address (msg));
}

int
OSC::route_set_pan_stereo_position (int ssid, float pos, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));

	if (s) {
		if(s->pan_azimuth_control()) {
			s->pan_azimuth_control()->set_value (s->pan_azimuth_control()->interface_to_internal (pos), PBD::Controllable::NoGroup);
			return 0;
		}
	}

	return route_send_fail ("pan_stereo_position", ssid, 0.5, get_address (msg));
}

int
OSC::route_set_pan_stereo_width (int ssid, float pos, lo_message msg)
{
	if (!session) return -1;
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));

	if (s) {
		if (s->pan_width_control()) {
			s->pan_width_control()->set_value (pos, PBD::Controllable::NoGroup);
			return 0;
		}
	}

	return route_send_fail ("pan_stereo_width", ssid, 1, get_address (msg));
}

int
OSC::route_set_send_gain_dB (int ssid, int id, float val, lo_message msg)
{
	if (!session) {
		return -1;
	}
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));
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
			s->send_level_controllable (id)->set_value (abs, PBD::Controllable::NoGroup);
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
	float abs;
	if (s) {

		if (id > 0) {
			--id;
		}

		if (s->send_level_controllable (id)) {
#ifdef MIXBUS
			abs = s->send_level_controllable(id)->interface_to_internal (val);
#else
			abs = slider_position_to_gain_with_max (val, 2.0);
#endif
			s->send_level_controllable (id)->set_value (abs, PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return 0;
}

int
OSC::sel_sendgain (int id, float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
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
			s->send_level_controllable (id)->set_value (abs, PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return sel_send_fail ("send_gain", id + 1, -193, get_address (msg));
}

int
OSC::sel_sendfader (int id, float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	float abs;
	if (s) {

		if (id > 0) {
			--id;
		}

		if (s->send_level_controllable (id)) {
#ifdef MIXBUS
			abs = s->send_level_controllable(id)->interface_to_internal (val);
#else
			abs = slider_position_to_gain_with_max (val, 2.0);
#endif
			s->send_level_controllable (id)->set_value (abs, PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return sel_send_fail ("send_fader", id, 0, get_address (msg));
}

int
OSC::route_set_send_enable (int ssid, int sid, float val, lo_message msg)
{
	if (!session) {
		return -1;
	}
	boost::shared_ptr<Stripable> s = get_strip (ssid, get_address (msg));

	if (s) {

		/* revert to zero-based counting */

		if (sid > 0) {
			--sid;
		}

		if (s->send_enable_controllable (sid)) {
			s->send_enable_controllable (sid)->set_value (val, PBD::Controllable::NoGroup);
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
		if (s->send_enable_controllable (id)) {
			s->send_enable_controllable (id)->set_value (val, PBD::Controllable::NoGroup);
			return 0;
		}
		if (s->send_level_controllable (id)) {
			boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (s);
			if (!r) {
				// should never get here
				return sel_send_fail ("send_enable", id + 1, 0, get_address (msg));
			}
			boost::shared_ptr<Send> snd = boost::dynamic_pointer_cast<Send> (r->nth_send(id));
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
	return sel_send_fail ("send_enable", id + 1, 0, get_address (msg));
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

	lo_message reply = lo_message_new();
	lo_message_add_int32 (reply, ssid);
	lo_message_add_int32 (reply, piid);
	lo_message_add_string (reply, pip->name());
	for ( uint32_t ppi = 0; ppi < pip->parameter_count(); ppi++) {

		uint32_t controlid = pip->nth_parameter(ppi, ok);
		if (!ok) {
			continue;
		}
		if ( pip->parameter_is_input(controlid) || pip->parameter_is_control(controlid) ) {
			boost::shared_ptr<AutomationControl> c = pi->automation_control(Evoral::Parameter(PluginAutomation, 0, controlid));

				lo_message_add_int32 (reply, ppi + 1);
				ParameterDescriptor pd;
				pi->plugin()->get_parameter_descriptor(controlid, pd);
				lo_message_add_string (reply, pd.label.c_str());

				// I've combined those binary descriptor parts in a bit-field to reduce lilo message elements
				int flags = 0;
				flags |= pd.enumeration ? 1 : 0;
				flags |= pd.integer_step ? 2 : 0;
				flags |= pd.logarithmic ? 4 : 0;
				flags |= pd.max_unbound ? 8 : 0;
				flags |= pd.min_unbound ? 16 : 0;
				flags |= pd.sr_dependent ? 32 : 0;
				flags |= pd.toggled ? 64 : 0;
				flags |= c != NULL ? 128 : 0; // bit 7 indicates in input control
				lo_message_add_int32 (reply, flags);

				lo_message_add_int32 (reply, pd.datatype);
				lo_message_add_float (reply, pd.lower);
				lo_message_add_float (reply, pd.upper);
				lo_message_add_string (reply, pd.print_fmt.c_str());
				if ( pd.scale_points ) {
					lo_message_add_int32 (reply, pd.scale_points->size());
					for ( ARDOUR::ScalePoints::const_iterator i = pd.scale_points->begin(); i != pd.scale_points->end(); ++i) {
						lo_message_add_int32 (reply, i->second);
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
		}
	}

	lo_send_message (get_address (msg), "/strip/plugin/descriptor", reply);
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
	return sel_fail ("pan_elevation_position", 0, get_address (msg));
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
	return sel_fail ("pan_frontback_position", 0.5, get_address (msg));
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
	return sel_fail ("pan_lfe_control", 0, get_address (msg));
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
	return sel_fail ("comp_enable", 0, get_address (msg));
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
	return sel_fail ("comp_threshold", 0, get_address (msg));
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
	return sel_fail ("comp_speed", 0, get_address (msg));
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
	return sel_fail ("comp_mode", 0, get_address (msg));
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
	return sel_fail ("comp_makeup", 0, get_address (msg));
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
	return sel_fail ("eq_enable", 0, get_address (msg));
}

int
OSC::sel_eq_hpf (float val, lo_message msg)
{
	OSCSurface *sur = get_surface(get_address (msg));
	boost::shared_ptr<Stripable> s;
	if (sur->expand_enable) {
		s = get_strip (sur->expand, get_address (msg));
	} else {
		s = _select;
	}
	if (s) {
		if (s->eq_hpf_controllable()) {
			s->eq_hpf_controllable()->set_value (s->eq_hpf_controllable()->interface_to_internal (val), PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return sel_fail ("eq_hpf", 0, get_address (msg));
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
	return sel_send_fail ("eq_gain", id + 1, 0, get_address (msg));
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
	return sel_send_fail ("eq_freq", id + 1, 0, get_address (msg));
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
	return sel_send_fail ("eq_q", id + 1, 0, get_address (msg));
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
	return sel_send_fail ("eq_shape", id + 1, 0, get_address (msg));
}

void
OSC::gui_selection_changed ()
{
	boost::shared_ptr<Stripable> strip = ControlProtocol::first_selected_stripable();

	if (strip) {
		_select = strip;
		for (uint32_t it = 0; it < _surface.size(); ++it) {
			OSCSurface* sur = &_surface[it];
			if(!sur->expand_enable) {
				lo_address addr = lo_address_new_from_url (sur->remote_url.c_str());
				_strip_select (strip, addr);
			}
		}
	}
}

// timer callbacks
bool
OSC::periodic (void)
{
	if (!tick) {
		Glib::usleep(100); // let flurry of signals subside
		if (global_init) {
			for (uint32_t it = 0; it < _surface.size(); it++) {
				OSCSurface* sur = &_surface[it];
				lo_address addr = lo_address_new_from_url (sur->remote_url.c_str());
				global_feedback (sur->feedback, addr, sur->gainmode);
			}
			global_init = false;
			tick = true;
		}
		if (bank_dirty) {
			_recalcbanks ();
			bank_dirty = false;
			tick = true;
		}
	}

	for (GlobalObservers::iterator x = global_observers.begin(); x != global_observers.end(); x++) {

		OSCGlobalObserver* go;

		if ((go = dynamic_cast<OSCGlobalObserver*>(*x)) != 0) {
			go->tick();
		}
	}
	for (RouteObservers::iterator x = route_observers.begin(); x != route_observers.end(); x++) {

		OSCRouteObserver* ro;

		if ((ro = dynamic_cast<OSCRouteObserver*>(*x)) != 0) {
			ro->tick();
		}
	}
	for (uint32_t it = 0; it < _surface.size(); it++) {
		OSCSurface* sur = &_surface[it];
		OSCSelectObserver* so;
		if ((so = dynamic_cast<OSCSelectObserver*>(sur->sel_obs)) != 0) {
			so->tick();
		}
	}
	for (CueObservers::iterator x = cue_observers.begin(); x != cue_observers.end(); x++) {

		OSCCueObserver* co;

		if ((co = dynamic_cast<OSCCueObserver*>(*x)) != 0) {
			co->tick();
		}
	}
	return true;
}

int
OSC::route_send_fail (string path, uint32_t ssid, float val, lo_address addr)
{
	OSCSurface *sur = get_surface(addr);

	ostringstream os;
	lo_message reply;
	if (ssid) {
		reply = lo_message_new ();
		if (sur->feedback[2]) {
			os << "/strip/" << path << "/" << ssid;
		} else {
			os << "/strip/" << path;
			lo_message_add_int32 (reply, ssid);
		}
		string str_pth = os.str();
		lo_message_add_float (reply, (float) val);

		lo_send_message (addr, str_pth.c_str(), reply);
		lo_message_free (reply);
	}
	if ((_select == get_strip (ssid, addr)) || ((sur->expand == ssid) && (sur->expand_enable))) {
		os.str("");
		os << "/select/" << path;
		string sel_pth = os.str();
		reply = lo_message_new ();
		lo_message_add_float (reply, (float) val);
		lo_send_message (addr, sel_pth.c_str(), reply);
		lo_message_free (reply);
	}

	return 0;
}

int
OSC::sel_fail (string path, float val, lo_address addr)
{
	ostringstream os;
	os.str("");
	os << "/select/" << path;
	string sel_pth = os.str();
	lo_message reply = lo_message_new ();
	lo_message_add_float (reply, (float) val);
	lo_send_message (addr, sel_pth.c_str(), reply);
	lo_message_free (reply);

	return 0;
}

int
OSC::sel_send_fail (string path, uint32_t id, float val, lo_address addr)
{
	OSCSurface *sur = get_surface(addr);

	ostringstream os;
	lo_message reply;
	reply = lo_message_new ();
	if (sur->feedback[2]) {
		os << "/select/" << path << "/" << id;
	} else {
		os << "/select/" << path;
		lo_message_add_int32 (reply, id);
	}
	string str_pth = os.str();
	lo_message_add_float (reply, (float) val);

	lo_send_message (addr, str_pth.c_str(), reply);
	lo_message_free (reply);

	return 0;
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
	if (_surface.size()) {
		XMLNode* config = new XMLNode (X_("Configurations"));
		for (uint32_t it = 0; it < _surface.size(); ++it) {
			OSCSurface* sur = &_surface[it];
			XMLNode* devnode = new XMLNode (X_("Configuration"));
			devnode->set_property (X_("url"), sur->remote_url);
			devnode->set_property (X_("bank-size"), sur->bank_size);
			devnode->set_property (X_("strip-types"), (uint64_t)sur->strip_types.to_ulong());
			devnode->set_property (X_("feedback"), (uint64_t)sur->feedback.to_ulong());
			devnode->set_property (X_("gainmode"), sur->gainmode);
			config->add_child_nocopy (*devnode);
		}
		node.add_child_nocopy (*config);
	}
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

	XMLNode* cnode = node.child (X_("Configurations"));

	if (cnode) {
		XMLNodeList const& devices = cnode->children();
		for (XMLNodeList::const_iterator d = devices.begin(); d != devices.end(); ++d) {
			OSCSurface s;
			if (!(*d)->get_property (X_("url"), s.remote_url)) {
				continue;
			}

			bank_dirty = true;

			(*d)->get_property (X_("bank-size"), s.bank_size);

			uint64_t bits;
			if ((*d)->get_property (X_ ("strip-types"), bits)) {
				s.strip_types = bits;
			}
			if ((*d)->get_property (X_("feedback"), bits)) {
				s.feedback = bits;
			}
			(*d)->get_property (X_("gainmode"), s.gainmode);

			s.bank = 1;
			s.sel_obs = 0;
			s.expand = 0;
			s.expand_enable = false;
			s.strips = get_sorted_stripables (s.strip_types, s.cue);
			s.nstrips = s.strips.size ();
			_surface.push_back (s);
		}
	}
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

	// fetch all stripables
	StripableList stripables;

	session->get_stripables (stripables);

	// Look for stripables that match bit in sur->strip_types
	for (StripableList::iterator it = stripables.begin(); it != stripables.end(); ++it) {

		boost::shared_ptr<Stripable> s = *it;
		if ((!cue) && (!types[9]) && (s->presentation_info().flags() & PresentationInfo::Hidden)) {
			// do nothing... skip it
		} else {

			if (types[0] && (s->presentation_info().flags() & PresentationInfo::AudioTrack)) {
				sorted.push_back (s);
			} else
			if (types[1] && (s->presentation_info().flags() & PresentationInfo::MidiTrack)) {
				sorted.push_back (s);
			} else
			if ((s->presentation_info().flags() & PresentationInfo::AudioBus)) {
				boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (s);
				// r->feeds (session->master_out()) may make more sense
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
			} else
			if (types[3] && (s->presentation_info().flags() & PresentationInfo::MidiBus)) {
				sorted.push_back (s);
			} else
			if (types[4] && (s->presentation_info().flags() & PresentationInfo::VCA)) {
				sorted.push_back (s);
			} else
			if (types[8] && (s->presentation_info().flags() & PresentationInfo::Selected)) {
				sorted.push_back (s);
			} else
			if (types[9] && (s->presentation_info().flags() & PresentationInfo::Hidden)) {
				sorted.push_back (s);
			}
		}
	}
	sort (sorted.begin(), sorted.end(), StripableByPresentationOrder());
	// Master/Monitor might be anywhere... we put them at the end - Sorry ;)
	if (types[5]) {
		sorted.push_back (session->master_out());
	}
	if (types[6]) {
		sorted.push_back (session->monitor_out());
	}
	return sorted;
}

int
OSC::cue_parse (const char *path, const char* types, lo_arg **argv, int argc, lo_message msg)
{
	int ret = 1; /* unhandled */

	if (!strncmp (path, "/cue/aux", 8)) {
		// set our Aux bus
		cue_set (argv[0]->i, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/cue/connect", 12)) {
		// switch to next Aux bus
		cue_set (0, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/cue/next_aux", 13)) {
		// switch to next Aux bus
		cue_next (msg);
		ret = 0;
	}
	else if (!strncmp (path, "/cue/previous_aux", 17)) {
		// switch to previous Aux bus
		cue_previous (msg);
		ret = 0;
	}
	else if (!strncmp (path, "/cue/send/fader/", 16) && strlen (path) > 16) {
		int id = atoi (&path[16]);
		cue_send_fader (id, argv[0]->f, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/cue/send/enable/", 17) && strlen (path) > 17) {
		int id = atoi (&path[17]);
		cue_send_enable (id, argv[0]->f, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/cue/fader", 10)) {
		cue_aux_fader (argv[0]->f, msg);
		ret = 0;
	}
	else if (!strncmp (path, "/cue/mute", 9)) {
		cue_aux_mute (argv[0]->f, msg);
		ret = 0;
	}

	if ((ret && _debugmode == Unhandled)) {
		debugmsg (_("Unhandled OSC cue message"), path, types, argv, argc);
	} else if ((!ret && _debugmode == All)) {
		debugmsg (_("OSC cue"), path, types, argv, argc);
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
	OSCSurface *s = get_surface(addr);
	s->bank_size = 0;
	s->strip_types = 128;
	s->feedback = 0;
	s->gainmode = 1;
	s->cue = true;
	s->aux = aux;
	s->strips = get_sorted_stripables(s->strip_types, s->cue);

	s->nstrips = s->strips.size();
	// get rid of any old CueObsevers for this address
	cueobserver_connections.drop_connections ();
	CueObservers::iterator x;
	for (x = cue_observers.begin(); x != cue_observers.end();) {

		OSCCueObserver* co;

		if ((co = dynamic_cast<OSCCueObserver*>(*x)) != 0) {

			int res = strcmp(lo_address_get_url(co->address()), lo_address_get_url(addr));

			if (res == 0) {
				delete *x;
				x = cue_observers.erase (x);
			} else {
				++x;
			}
		} else {
			++x;
		}
	}

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
				// start cue observer
				OSCCueObserver* co = new OSCCueObserver (stp, s->sends, addr);
				cue_observers.push_back (co);
			}

		}
	}

	return 0;
}

int
OSC::cue_next (lo_message msg)
{
	OSCSurface *s = get_surface(get_address (msg));
	if (s->aux < s->nstrips) {
		cue_set (s->aux + 1, msg);
	} else {
		cue_set (s->nstrips, msg);
	}
	return 0;
}

int
OSC::cue_previous (lo_message msg)
{
	OSCSurface *s = get_surface(get_address (msg));
	if (s->aux > 1) {
		cue_set (s->aux - 1, msg);
	}
	return 0;
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
				float abs;
				abs = slider_position_to_gain_with_max (position, 2.0);
				if (s->gain_control()) {
					s->gain_control()->set_value (abs, PBD::Controllable::NoGroup);
					return 0;
				}
			}
		}
	}
	return cue_float_message ("/cue/fader", 0, get_address (msg));
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
	return cue_float_message ("/cue/mute", 0, get_address (msg));
}

int
OSC::cue_send_fader (uint32_t id, float val, lo_message msg)
{
	if (!session) {
		return -1;
	}
	boost::shared_ptr<Send> s = cue_get_send (id, get_address (msg));
	float abs;
	if (s) {
		if (s->gain_control()) {
			abs = slider_position_to_gain_with_max (val, 2.0);
			s->gain_control()->set_value (abs, PBD::Controllable::NoGroup);
			return 0;
		}
	}
	return cue_float_message (string_compose ("/cue/send/fader/%1", id), 0, get_address (msg));
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
	return cue_float_message (string_compose ("/cue/send/enable/%1", id), 0, get_address (msg));
}

int
OSC::cue_float_message (string path, float val, lo_address addr)
{

	lo_message reply;
	reply = lo_message_new ();
	lo_message_add_float (reply, (float) val);

	lo_send_message (addr, path.c_str(), reply);
	lo_message_free (reply);

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


// we have to have a sorted list of stripables that have sends pointed at our aux
// we can use the one in osc.cc to get an aux list
OSC::Sorted
OSC::cue_get_sorted_stripables(boost::shared_ptr<Stripable> aux, uint32_t id, lo_message msg)
{
	Sorted sorted;
	cueobserver_connections.drop_connections ();
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

