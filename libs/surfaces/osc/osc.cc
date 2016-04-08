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
#include <glibmm/miscutils.h>

#include <pbd/convert.h>
#include <pbd/pthread_utils.h>
#include <pbd/file_utils.h>
#include <pbd/failed_constructor.h>

#include "ardour/amp.h"
#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"
#include "ardour/dB.h"
#include "ardour/filesystem_paths.h"
#include "ardour/panner.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/send.h"

#include "osc.h"
#include "osc_controllable.h"
#include "osc_route_observer.h"
#include "i18n.h"

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
	, _send_route_changes (true)
	, _debugmode (Off)
	, gui (0)
{
	_instance = this;

	session->Exported.connect (*this, MISSING_INVALIDATOR, boost::bind (&OSC::session_exported, this, _1, _2), this);
}

OSC::~OSC()
{
	stop ();
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
OSC::set_feedback (bool yn)
{
	_send_route_changes = yn;
	return 0;
}

bool
OSC::get_feedback () const
{
	return _send_route_changes;
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

		REGISTER_CALLBACK (serv, "/routes/list", "", routes_list);
		REGISTER_CALLBACK (serv, "/ardour/add_marker", "", add_marker);
		REGISTER_CALLBACK (serv, "/ardour/access_action", "s", access_action);
		REGISTER_CALLBACK (serv, "/ardour/loop_toggle", "", loop_toggle);
		REGISTER_CALLBACK (serv, "/ardour/loop_location", "ii", loop_location);
		REGISTER_CALLBACK (serv, "/ardour/goto_start", "", goto_start);
		REGISTER_CALLBACK (serv, "/ardour/goto_end", "", goto_end);
		REGISTER_CALLBACK (serv, "/ardour/rewind", "", rewind);
		REGISTER_CALLBACK (serv, "/ardour/ffwd", "", ffwd);
		REGISTER_CALLBACK (serv, "/ardour/transport_stop", "", transport_stop);
		REGISTER_CALLBACK (serv, "/ardour/transport_play", "", transport_play);
		REGISTER_CALLBACK (serv, "/ardour/transport_frame", "", transport_frame);
		REGISTER_CALLBACK (serv, "/ardour/transport_speed", "", transport_speed);
		REGISTER_CALLBACK (serv, "/ardour/record_enabled", "", record_enabled);
		REGISTER_CALLBACK (serv, "/ardour/set_transport_speed", "f", set_transport_speed);
		REGISTER_CALLBACK (serv, "/ardour/locate", "ii", locate);
		REGISTER_CALLBACK (serv, "/ardour/save_state", "", save_state);
		REGISTER_CALLBACK (serv, "/ardour/prev_marker", "", prev_marker);
		REGISTER_CALLBACK (serv, "/ardour/next_marker", "", next_marker);
		REGISTER_CALLBACK (serv, "/ardour/undo", "", undo);
		REGISTER_CALLBACK (serv, "/ardour/redo", "", redo);
		REGISTER_CALLBACK (serv, "/ardour/toggle_punch_in", "", toggle_punch_in);
		REGISTER_CALLBACK (serv, "/ardour/toggle_punch_out", "", toggle_punch_out);
		REGISTER_CALLBACK (serv, "/ardour/rec_enable_toggle", "", rec_enable_toggle);
		REGISTER_CALLBACK (serv, "/ardour/toggle_all_rec_enables", "", toggle_all_rec_enables);
		REGISTER_CALLBACK (serv, "/ardour/all_tracks_rec_in", "f", all_tracks_rec_in);
		REGISTER_CALLBACK (serv, "/ardour/all_tracks_rec_out", "f", all_tracks_rec_out);
		REGISTER_CALLBACK (serv, "/ardour/remove_marker", "", remove_marker_at_playhead);
		REGISTER_CALLBACK (serv, "/ardour/jump_bars", "f", jump_by_bars);
		REGISTER_CALLBACK (serv, "/ardour/jump_seconds", "f", jump_by_seconds);
		REGISTER_CALLBACK (serv, "/ardour/mark_in", "", mark_in);
		REGISTER_CALLBACK (serv, "/ardour/mark_out", "", mark_out);
		REGISTER_CALLBACK (serv, "/ardour/toggle_click", "", toggle_click);
		REGISTER_CALLBACK (serv, "/ardour/midi_panic", "", midi_panic);
		REGISTER_CALLBACK (serv, "/ardour/toggle_roll", "", toggle_roll);
		REGISTER_CALLBACK (serv, "/ardour/stop_forget", "", stop_forget);
		REGISTER_CALLBACK (serv, "/ardour/set_punch_range", "", set_punch_range);
		REGISTER_CALLBACK (serv, "/ardour/set_loop_range", "", set_loop_range);
		REGISTER_CALLBACK (serv, "/ardour/set_session_range", "", set_session_range);
		REGISTER_CALLBACK (serv, "/ardour/toggle_monitor_mute", "", toggle_monitor_mute);
		REGISTER_CALLBACK (serv, "/ardour/toggle_monitor_dim", "", toggle_monitor_dim);
		REGISTER_CALLBACK (serv, "/ardour/toggle_monitor_mono", "", toggle_monitor_mono);
		REGISTER_CALLBACK (serv, "/ardour/quick_snapshot_switch", "", quick_snapshot_switch);
		REGISTER_CALLBACK (serv, "/ardour/quick_snapshot_stay", "", quick_snapshot_stay);
		REGISTER_CALLBACK (serv, "/ardour/fit_1_track", "", fit_1_track);
		REGISTER_CALLBACK (serv, "/ardour/fit_2_tracks", "", fit_2_tracks);
		REGISTER_CALLBACK (serv, "/ardour/fit_4_tracks", "", fit_4_tracks);
		REGISTER_CALLBACK (serv, "/ardour/fit_8_tracks", "", fit_8_tracks);
		REGISTER_CALLBACK (serv, "/ardour/fit_16_tracks", "", fit_16_tracks);
		REGISTER_CALLBACK (serv, "/ardour/fit_32_tracks", "", fit_32_tracks);
		REGISTER_CALLBACK (serv, "/ardour/fit_all_tracks", "", fit_all_tracks);
		REGISTER_CALLBACK (serv, "/ardour/zoom_100_ms", "", zoom_100_ms);
		REGISTER_CALLBACK (serv, "/ardour/zoom_1_sec", "", zoom_1_sec);
		REGISTER_CALLBACK (serv, "/ardour/zoom_10_sec", "", zoom_10_sec);
		REGISTER_CALLBACK (serv, "/ardour/zoom_1_min", "", zoom_1_min);
		REGISTER_CALLBACK (serv, "/ardour/zoom_5_min", "", zoom_5_min);
		REGISTER_CALLBACK (serv, "/ardour/zoom_10_min", "", zoom_10_min);
		REGISTER_CALLBACK (serv, "/ardour/zoom_to_session", "", zoom_to_session);
		REGISTER_CALLBACK (serv, "/ardour/temporal_zoom_in", "f", temporal_zoom_in);
		REGISTER_CALLBACK (serv, "/ardour/temporal_zoom_out", "f", temporal_zoom_out);
		REGISTER_CALLBACK (serv, "/ardour/scroll_up_1_track", "f", scroll_up_1_track);
		REGISTER_CALLBACK (serv, "/ardour/scroll_dn_1_track", "f", scroll_dn_1_track);
		REGISTER_CALLBACK (serv, "/ardour/scroll_up_1_page", "f", scroll_up_1_page);
		REGISTER_CALLBACK (serv, "/ardour/scroll_dn_1_page", "f", scroll_dn_1_page);


		/*
		 * NOTE: these messages are provided for (arguably broken) apps
		 *   that MUST send float args ( TouchOSC and Lemur ).
		 * Normally these ardour transport messages don't require an argument,
		 * so we're providing redundant calls with vestigial "float" args.
		 *
		 * These controls are active on 1.0 only (to prevent duplicate action on
		 * press "/button 1", and release "/button 0")
		 * http://hexler.net/docs/touchosc-controls-reference
		 */
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/loop_toggle", "f", loop_toggle);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/add_marker", "f", add_marker);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/goto_start", "f", goto_start);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/goto_end", "f", goto_end);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/rewind", "f", rewind);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/ffwd", "f", ffwd);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/transport_stop", "f", transport_stop);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/transport_play", "f", transport_play);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/save_state", "f", save_state);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/prev_marker", "f", prev_marker);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/next_marker", "f", next_marker);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/undo", "f", undo);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/redo", "f", redo);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/toggle_punch_in", "f", toggle_punch_in);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/toggle_punch_out", "f", toggle_punch_out);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/rec_enable_toggle", "f", rec_enable_toggle);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/toggle_all_rec_enables", "f", toggle_all_rec_enables);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/all_tracks_rec_in", "f", all_tracks_rec_in);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/all_tracks_rec_out", "f", all_tracks_rec_out);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/remove_marker", "f", remove_marker_at_playhead);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/mark_in", "f", mark_in);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/mark_out", "f", mark_out);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/toggle_click", "f", toggle_click);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/midi_panic", "f", midi_panic);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/toggle_roll", "f", toggle_roll);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/stop_forget", "f", stop_forget);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/set_punch_range", "f", set_punch_range);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/set_loop_range", "f", set_loop_range);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/set_session_range", "f", set_session_range);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/toggle_monitor_mute", "f", toggle_monitor_mute);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/toggle_monitor_dim", "f", toggle_monitor_dim);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/toggle_monitor_mono", "f", toggle_monitor_mono);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/quick_snapshot_switch", "f", quick_snapshot_switch);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/quick_snapshot_stay", "f", quick_snapshot_stay);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/fit_1_track", "f", fit_1_track);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/fit_2_tracks", "f", fit_2_tracks);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/fit_4_tracks", "f", fit_4_tracks);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/fit_8_tracks", "f", fit_8_tracks);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/fit_16_tracks", "f", fit_16_tracks);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/fit_32_tracks", "f", fit_32_tracks);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/fit_all_tracks", "f", fit_all_tracks);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/zoom_100_ms", "f", zoom_100_ms);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/zoom_1_sec", "f", zoom_1_sec);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/zoom_10_sec", "f", zoom_10_sec);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/zoom_1_min", "f", zoom_1_min);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/zoom_5_min", "f", zoom_5_min);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/zoom_10_min", "f", zoom_10_min);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/zoom_to_session", "f", zoom_to_session);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/temporal_zoom_in", "f", temporal_zoom_in);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/temporal_zoom_out", "f", temporal_zoom_out);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/scroll_up_1_track", "f", scroll_up_1_track);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/scroll_dn_1_track", "f", scroll_dn_1_track);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/scroll_up_1_page", "f", scroll_up_1_page);
		REGISTER_CALLBACK (serv, "/ardour/pushbutton/scroll_dn_1_page", "f", scroll_dn_1_page);

		/* These commands require the route index in addition to the arg; TouchOSC (et al) can't use these  */ 
		REGISTER_CALLBACK (serv, "/ardour/routes/mute", "ii", route_mute);
		REGISTER_CALLBACK (serv, "/ardour/routes/solo", "ii", route_solo);
		REGISTER_CALLBACK (serv, "/ardour/routes/recenable", "ii", route_recenable);
		REGISTER_CALLBACK (serv, "/ardour/routes/gainabs", "if", route_set_gain_abs);
		REGISTER_CALLBACK (serv, "/ardour/routes/gaindB", "if", route_set_gain_dB);
		REGISTER_CALLBACK (serv, "/ardour/routes/gainfader", "if", route_set_gain_fader);
		REGISTER_CALLBACK (serv, "/ardour/routes/trimabs", "if", route_set_trim_abs);
		REGISTER_CALLBACK (serv, "/ardour/routes/trimdB", "if", route_set_trim_dB);
		REGISTER_CALLBACK (serv, "/ardour/routes/pan_stereo_position", "if", route_set_pan_stereo_position);
		REGISTER_CALLBACK (serv, "/ardour/routes/pan_stereo_width", "if", route_set_pan_stereo_width);
		REGISTER_CALLBACK (serv, "/ardour/routes/plugin/parameter", "iiif", route_plugin_parameter);
		REGISTER_CALLBACK (serv, "/ardour/routes/plugin/parameter/print", "iii", route_plugin_parameter_print);
		REGISTER_CALLBACK (serv, "/ardour/routes/send/gainabs", "iif", route_set_send_gain_abs);
		REGISTER_CALLBACK (serv, "/ardour/routes/send/gaindB", "iif", route_set_send_gain_dB);

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
OSC::listen_to_route (boost::shared_ptr<Route> route, lo_address addr)
{
	/* avoid duplicate listens */

	for (RouteObservers::iterator x = route_observers.begin(); x != route_observers.end(); ++x) {

		OSCRouteObserver* ro;

		if ((ro = dynamic_cast<OSCRouteObserver*>(*x)) != 0) {

			int res = strcmp(lo_address_get_hostname(ro->address()), lo_address_get_hostname(addr));

			if (ro->route() == route && res == 0) {
				return;
			}
		}
	}

	OSCRouteObserver* o = new OSCRouteObserver (route, addr);
	route_observers.push_back (o);

	route->DropReferences.connect (*this, MISSING_INVALIDATOR, boost::bind (&OSC::drop_route, this, boost::weak_ptr<Route> (route)), this);
}

void
OSC::drop_route (boost::weak_ptr<Route> wr)
{
	boost::shared_ptr<Route> r = wr.lock ();

	if (!r) {
		return;
	}

	for (RouteObservers::iterator x = route_observers.begin(); x != route_observers.end();) {

		OSCRouteObserver* rc;

		if ((rc = dynamic_cast<OSCRouteObserver*>(*x)) != 0) {

			if (rc->route() == r) {
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
OSC::end_listen (boost::shared_ptr<Route> r, lo_address addr)
{
	RouteObservers::iterator x;

	// Remove the route observers
	for (x = route_observers.begin(); x != route_observers.end();) {

		OSCRouteObserver* ro;

		if ((ro = dynamic_cast<OSCRouteObserver*>(*x)) != 0) {

			int res = strcmp(lo_address_get_hostname(ro->address()), lo_address_get_hostname(addr));

			if (ro->route() == r && res == 0) {
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
		r = session->route_by_remote_id (id);

		if (!r) {
			lo_message_add_string (reply, "not found");
		} else {

			if (strcmp (path, "/routes/state") == 0) {

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

			} else if (strcmp (path, "/routes/mute") == 0) {

				lo_message_add_int32 (reply, (float) r->muted());

			} else if (strcmp (path, "/routes/solo") == 0) {

				lo_message_add_int32 (reply, r->soloed());
			}
		}
	}

	lo_send_message (lo_message_get_source (msg), "#reply", reply);
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

	} else if (strcmp (path, "/routes/listen") == 0) {

		cerr << "set up listener\n";

		lo_message reply = lo_message_new ();

		if (argc <= 0) {
			lo_message_add_string (reply, "syntax error");
		} else {
			for (int n = 0; n < argc; ++n) {

				boost::shared_ptr<Route> r = session->route_by_remote_id (argv[n]->i);

				if (!r) {
					lo_message_add_string (reply, "not found");
					cerr << "no such route\n";
					break;
				} else {
					cerr << "add listener\n";
					listen_to_route (r, lo_message_get_source (msg));
					lo_message_add_int32 (reply, argv[n]->i);
				}
			}
		}

		lo_send_message (lo_message_get_source (msg), "#reply", reply);
		lo_message_free (reply);

		ret = 0;

	} else if (strcmp (path, "/routes/ignore") == 0) {

		for (int n = 0; n < argc; ++n) {

			boost::shared_ptr<Route> r = session->route_by_remote_id (argv[n]->i);

			if (r) {
				end_listen (r, lo_message_get_source (msg));
			}
		}

		ret = 0;
	} else if (argc == 1 && types[0] == 'f') { // single float -- probably TouchOSC
		if (!strncmp (path, "/ardour/routes/gainabs/", 23) && strlen (path) > 23) {
			int rid = atoi (&path[23]);
			// use some power-scale mapping??
			route_set_gain_abs (rid, argv[0]->f);
			ret = 0;
		}
		else if (!strncmp (path, "/ardour/routes/trimabs/", 23) && strlen (path) > 23) {
			int rid = atoi (&path[23]);
			// normalize 0..1 ?
			route_set_trim_abs (rid, argv[0]->f);
			ret = 0;
		}
		else if (!strncmp (path, "/ardour/routes/mute/", 20) && strlen (path) > 20) {
			int rid = atoi (&path[20]);
			route_mute (rid, argv[0]->f == 1.0);
			ret = 0;
		}
		else if (!strncmp (path, "/ardour/routes/solo/", 20) && strlen (path) > 20) {
			int rid = atoi (&path[20]);
			route_solo (rid, argv[0]->f == 1.0);
			ret = 0;
		}
		else if (!strncmp (path, "/ardour/routes/recenable/", 25) && strlen (path) > 25) {
			int rid = atoi (&path[25]);
			route_recenable (rid, argv[0]->f == 1.0);
			ret = 0;
		}
	}

	if ((ret && _debugmode == Unhandled)) {
		debugmsg (_("Unhandled OSC message"), path, types, argv, argc);
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

void
OSC::update_clock ()
{

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
	for (int n = 0; n < (int) session->nroutes(); ++n) {

		boost::shared_ptr<Route> r = session->route_by_remote_id (n);

		if (r) {

			lo_message reply = lo_message_new ();

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
			lo_message_add_int32 (reply, r->remote_control_id());

			if (boost::dynamic_pointer_cast<AudioTrack>(r)
					|| boost::dynamic_pointer_cast<MidiTrack>(r)) {

				boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track>(r);
				lo_message_add_int32 (reply, (int32_t) t->rec_enable_control()->get_value());
			}

			//Automatically listen to routes listed
			listen_to_route(r, lo_message_get_source (msg));

			lo_send_message (lo_message_get_source (msg), "#reply", reply);
			lo_message_free (reply);
		}
	}

	// Send end of listing message
	lo_message reply = lo_message_new ();

	lo_message_add_string (reply, "end_route_list");
	lo_message_add_int64 (reply, session->frame_rate());
	lo_message_add_int64 (reply, session->current_end_frame());

	lo_send_message (lo_message_get_source (msg), "#reply", reply);

	lo_message_free (reply);
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

	lo_send_message (lo_message_get_source (msg), "/ardour/transport_frame", reply);

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

	lo_send_message (lo_message_get_source (msg), "/ardour/transport_speed", reply);

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

	lo_send_message (lo_message_get_source (msg), "/ardour/record_enabled", reply);

	lo_message_free (reply);
}


int
OSC::route_mute (int rid, int yn)
{
	if (!session) return -1;

	boost::shared_ptr<Route> r = session->route_by_remote_id (rid);

	if (r) {
		r->mute_control()->set_value (yn ? 1.0 : 0.0, PBD::Controllable::NoGroup);
	}

	return 0;
}

int
OSC::route_solo (int rid, int yn)
{
	if (!session) return -1;

	boost::shared_ptr<Route> r = session->route_by_remote_id (rid);

	if (r) {
		r->solo_control()->set_value (yn ? 1.0 : 0.0, PBD::Controllable::NoGroup);
	}

	return 0;
}

int
OSC::route_recenable (int rid, int yn)
{
	if (!session) return -1;

	boost::shared_ptr<Route> r = session->route_by_remote_id (rid);

	if (r) {
		boost::shared_ptr<Track> trk = boost::dynamic_pointer_cast<Track> (r);
		if (trk) {
			trk->rec_enable_control()->set_value (yn, PBD::Controllable::UseGroup);
		}
	}

	return 0;
}

int
OSC::route_set_gain_abs (int rid, float level)
{
	if (!session) return -1;

	boost::shared_ptr<Route> r = session->route_by_remote_id (rid);

	if (r) {
		r->gain_control()->set_value (level, PBD::Controllable::NoGroup);
	}

	return 0;
}

int
OSC::route_set_gain_dB (int rid, float dB)
{
	if (dB < -192) {
		return route_set_gain_abs (rid, 0.0);
	}
	return route_set_gain_abs (rid, dB_to_coefficient (dB));
}

int
OSC::route_set_gain_fader (int rid, float pos)
{
	return route_set_gain_abs (rid, slider_position_to_gain_with_max (pos, 2.0));
}


int
OSC::route_set_trim_abs (int rid, float level)
{
	if (!session) return -1;

	boost::shared_ptr<Route> r = session->route_by_remote_id (rid);

	if (r) {
		r->set_trim (level, PBD::Controllable::NoGroup);
	}

	return 0;
}

int
OSC::route_set_trim_dB (int rid, float dB)
{
	return route_set_trim_abs(rid, dB_to_coefficient (dB));
}


int
OSC::route_set_pan_stereo_position (int rid, float pos)
{
	if (!session) return -1;

	boost::shared_ptr<Route> r = session->route_by_remote_id (rid);

	if (r) {
		boost::shared_ptr<Panner> panner = r->panner();
		if (panner) {
			panner->set_position (pos);
		}
	}

	return 0;

}

int
OSC::route_set_pan_stereo_width (int rid, float pos)
{
	if (!session) return -1;

	boost::shared_ptr<Route> r = session->route_by_remote_id (rid);

	if (r) {
		boost::shared_ptr<Panner> panner = r->panner();
		if (panner) {
			panner->set_width (pos);
		}
	}

	return 0;

}

int
OSC::route_set_send_gain_abs (int rid, int sid, float val)
{
	if (!session) {
		return -1;
	}

	boost::shared_ptr<Route> r = session->route_by_remote_id (rid);

	if (!r) {
		return -1;
	}

	/* revert to zero-based counting */

	if (sid > 0) {
		--sid;
	}

	boost::shared_ptr<Processor> p = r->nth_send (sid);

	if (p) {
		boost::shared_ptr<Send> s = boost::dynamic_pointer_cast<Send>(p);
		boost::shared_ptr<Amp> a = s->amp();

		if (a) {
			a->gain_control()->set_value (val, PBD::Controllable::NoGroup);
		}
	}
	return 0;
}

int
OSC::route_set_send_gain_dB (int rid, int sid, float val)
{
	if (!session) {
		return -1;
	}

	boost::shared_ptr<Route> r = session->route_by_remote_id (rid);

	if (!r) {
		return -1;
	}

	/* revert to zero-based counting */

	if (sid > 0) {
		--sid;
	}

	boost::shared_ptr<Processor> p = r->nth_send (sid);

	if (p) {
		boost::shared_ptr<Send> s = boost::dynamic_pointer_cast<Send>(p);
		boost::shared_ptr<Amp> a = s->amp();

		if (a) {
			a->gain_control()->set_value (dB_to_coefficient (val), PBD::Controllable::NoGroup);
		}
	}
	return 0;
}

int
OSC::route_plugin_parameter (int rid, int piid, int par, float val)
{
	if (!session)
		return -1;

	boost::shared_ptr<Route> r = session->route_by_remote_id (rid);

	if (!r) {
		PBD::error << "OSC: Invalid Remote Control ID '" << rid << "'" << endmsg;
		return -1;
	}

	boost::shared_ptr<Processor> redi=r->nth_plugin (piid);

	if (!redi) {
		PBD::error << "OSC: cannot find plugin # " << piid << " for RID '" << rid << "'" << endmsg;
		return -1;
	}

	boost::shared_ptr<PluginInsert> pi;

	if (!(pi = boost::dynamic_pointer_cast<PluginInsert>(redi))) {
		PBD::error << "OSC: given processor # " << piid << " on RID '" << rid << "' is not a Plugin." << endmsg;
		return -1;
	}

	boost::shared_ptr<ARDOUR::Plugin> pip = pi->plugin();
	bool ok=false;

	uint32_t controlid = pip->nth_parameter (par,ok);

	if (!ok) {
		PBD::error << "OSC: Cannot find parameter # " << par <<  " for plugin # " << piid << " on RID '" << rid << "'" << endmsg;
		return -1;
	}

	if (!pip->parameter_is_input(controlid)) {
		PBD::error << "OSC: Parameter # " << par <<  " for plugin # " << piid << " on RID '" << rid << "' is not a control input" << endmsg;
		return -1;
	}

	ParameterDescriptor pd;
	pi->plugin()->get_parameter_descriptor (controlid,pd);

	if (val >= pd.lower && val <= pd.upper) {

		boost::shared_ptr<AutomationControl> c = pi->automation_control (Evoral::Parameter(PluginAutomation, 0, controlid));
		// cerr << "parameter:" << redi->describe_parameter(controlid) << " val:" << val << "\n";
		c->set_value (val, PBD::Controllable::NoGroup);
	} else {
		PBD::warning << "OSC: Parameter # " << par <<  " for plugin # " << piid << " on RID '" << rid << "' is out of range" << endmsg;
		PBD::info << "OSC: Valid range min=" << pd.lower << " max=" << pd.upper << endmsg;
	}

	return 0;
}

int
OSC::route_plugin_parameter_print (int rid, int piid, int par)
{
	if (!session) {
		return -1;
	}

	boost::shared_ptr<Route> r = session->route_by_remote_id (rid);

	if (!r) {
		return -1;
	}

	boost::shared_ptr<Processor> redi=r->nth_processor (piid);

	if (!redi) {
		return -1;
	}

	boost::shared_ptr<PluginInsert> pi;

	if (!(pi = boost::dynamic_pointer_cast<PluginInsert>(redi))) {
		return -1;
	}

	boost::shared_ptr<ARDOUR::Plugin> pip = pi->plugin();
	bool ok=false;

	uint32_t controlid = pip->nth_parameter (par,ok);

	if (!ok) {
		return -1;
	}

	ParameterDescriptor pd;

	if (pi->plugin()->get_parameter_descriptor (controlid, pd) == 0) {
		boost::shared_ptr<AutomationControl> c = pi->automation_control (Evoral::Parameter(PluginAutomation, 0, controlid));

		cerr << "parameter:     " << redi->describe_parameter(controlid)  << "\n";
		cerr << "current value: " << c->get_value ();
		cerr << "lower value:   " << pd.lower << "\n";
		cerr << "upper value:   " << pd.upper << "\n";
	}

	return 0;
}

XMLNode&
OSC::get_state ()
{
	XMLNode& node (ControlProtocol::get_state());
	node.add_property("debugmode", (int) _debugmode); // TODO: enum2str
	return node;
}

int
OSC::set_state (const XMLNode& node, int version)
{
	if (ControlProtocol::set_state (node, version)) {
		return -1;
	}
	XMLProperty const * p = node.property (X_("debugmode"));
	if (p) {
		_debugmode = OSCDebugMode (PBD::atoi(p->value ()));
	}

	return 0;
}
