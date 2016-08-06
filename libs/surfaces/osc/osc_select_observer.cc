/*
    Copyright (C) 2009 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <vector>
#include "boost/lambda/lambda.hpp"

#include "ardour/session.h"
#include "ardour/track.h"
#include "ardour/monitor_control.h"
#include "ardour/dB.h"
#include "ardour/meter.h"
#include "ardour/phase_control.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/solo_safe_control.h"
#include "ardour/route.h"

#include "osc.h"
#include "osc_select_observer.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace ArdourSurface;

OSCSelectObserver::OSCSelectObserver (boost::shared_ptr<Stripable> s, lo_address a, uint32_t gm, std::bitset<32> fb)
	: _strip (s)
	,gainmode (gm)
	,feedback (fb)
	,nsends (0)
{
	addr = lo_address_new (lo_address_get_hostname(a) , lo_address_get_port(a));

	if (feedback[0]) { // buttons are separate feedback
		_strip->PropertyChanged.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::name_changed, this, boost::lambda::_1), OSC::instance());
		name_changed (ARDOUR::Properties::name);

		_strip->mute_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/mute"), _strip->mute_control()), OSC::instance());
		change_message ("/select/mute", _strip->mute_control());

		_strip->solo_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/solo"), _strip->solo_control()), OSC::instance());
		change_message ("/select/solo", _strip->solo_control());

		if (_strip->solo_isolate_control()) {
			_strip->solo_isolate_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/solo_iso"), _strip->solo_isolate_control()), OSC::instance());
			change_message ("/select/solo_iso", _strip->solo_isolate_control());
		}

		if (_strip->solo_safe_control()) {
			_strip->solo_safe_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/solo_safe"), _strip->solo_safe_control()), OSC::instance());
			change_message ("/select/solo_safe", _strip->solo_safe_control());
		}

		boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (_strip);
		if (track) {
			track->monitoring_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::monitor_status, this, track->monitoring_control()), OSC::instance());
			monitor_status (track->monitoring_control());
		}

		boost::shared_ptr<AutomationControl> rec_controllable = _strip->rec_enable_control ();
		if (rec_controllable) {
			rec_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/recenable"), _strip->rec_enable_control()), OSC::instance());
			change_message ("/select/recenable", _strip->rec_enable_control());
		}

		boost::shared_ptr<AutomationControl> recsafe_controllable = _strip->rec_safe_control ();
		if (recsafe_controllable) {
			recsafe_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/record_safe"), _strip->rec_safe_control()), OSC::instance());
			change_message ("/select/record_safe", _strip->rec_safe_control());
		}

		boost::shared_ptr<AutomationControl> phase_controllable = _strip->phase_control ();
		if (phase_controllable) {
			phase_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/polarity"), _strip->phase_control()), OSC::instance());
			change_message ("/select/polarity", _strip->phase_control());
		}

	}

	if (feedback[1]) { // level controls
		if (gainmode) {
			_strip->gain_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::gain_message, this, X_("/select/fader"), _strip->gain_control()), OSC::instance());
			gain_message ("/select/fader", _strip->gain_control());
		} else {
			_strip->gain_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::gain_message, this, X_("/select/gain"), _strip->gain_control()), OSC::instance());
			gain_message ("/select/gain", _strip->gain_control());
		}

		boost::shared_ptr<Controllable> trim_controllable = boost::dynamic_pointer_cast<Controllable>(_strip->trim_control());
		if (trim_controllable) {
			trim_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::trim_message, this, X_("/select/trimdB"), _strip->trim_control()), OSC::instance());
			trim_message ("/select/trimdB", _strip->trim_control());
		}

		boost::shared_ptr<Controllable> pan_controllable = boost::dynamic_pointer_cast<Controllable>(_strip->pan_azimuth_control());
		if (pan_controllable) {
			pan_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/pan_stereo_position"), _strip->pan_azimuth_control()), OSC::instance());
			change_message ("/select/pan_stereo_position", _strip->pan_azimuth_control());
		}

		boost::shared_ptr<Controllable> width_controllable = boost::dynamic_pointer_cast<Controllable>(_strip->pan_width_control());
		if (width_controllable) {
			width_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/pan_stereo_width"), _strip->pan_width_control()), OSC::instance());
			change_message ("/select/pan_stereo_width", _strip->pan_width_control());
		}

	}
	if (feedback[13]) { // Well known controls
		// Rest of possible pan controls... Untested because I can't find a way to get them in the GUI :)
		if (_strip->pan_elevation_control ()) {
			_strip->pan_elevation_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/pan_elevation_position"), _strip->pan_elevation_control()), OSC::instance());
			change_message ("/select/pan_elevation_position", _strip->pan_elevation_control());
		}
		if (_strip->pan_frontback_control ()) {
			_strip->pan_frontback_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/pan_frontback_position"), _strip->pan_frontback_control()), OSC::instance());
			change_message ("/select/pan_frontback_position", _strip->pan_frontback_control());
		}
		if (_strip->pan_lfe_control ()) {
			_strip->pan_lfe_control()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/pan_lfe_control"), _strip->pan_lfe_control()), OSC::instance());
			change_message ("/select/pan_lfe_control", _strip->pan_lfe_control());
		}

		// sends and eq
		// detecting processor changes requires cast to route
		boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route>(_strip);
		if (r) {
			r->processors_changed.connect  (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::send_restart, this, -1), OSC::instance());
			send_init();
			eq_init();
		}

		// Compressor
		if (_strip->comp_enable_controllable ()) {
			_strip->comp_enable_controllable ()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::enable_message, this, X_("/select/comp_enable"), _strip->comp_enable_controllable()), OSC::instance());
			enable_message ("/select/comp_enable", _strip->comp_enable_controllable());
		}
		if (_strip->comp_threshold_controllable ()) {
			_strip->comp_threshold_controllable ()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/comp_threshold"), _strip->comp_threshold_controllable()), OSC::instance());
			change_message ("/select/comp_threshold", _strip->comp_threshold_controllable());
		}
		if (_strip->comp_speed_controllable ()) {
			_strip->comp_speed_controllable ()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/comp_speed"), _strip->comp_speed_controllable()), OSC::instance());
			change_message ("/select/comp_speed", _strip->comp_speed_controllable());
		}
		if (_strip->comp_mode_controllable ()) {
			_strip->comp_mode_controllable ()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::comp_mode, this), OSC::instance());
			comp_mode ();
		}
		if (_strip->comp_makeup_controllable ()) {
			_strip->comp_makeup_controllable ()->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/comp_makeup"), _strip->comp_makeup_controllable()), OSC::instance());
			change_message ("/select/comp_makeup", _strip->comp_makeup_controllable());
		}

	}

	tick();
}

OSCSelectObserver::~OSCSelectObserver ()
{

	strip_connections.drop_connections ();
	// all strip buttons should be off and faders 0 and etc.
	clear_strip ("/select/expand", 0);
	if (feedback[0]) { // buttons are separate feedback
		text_message ("/select/name", " ");
		text_message ("/select/comment", " ");
		clear_strip ("/select/mute", 0);
		clear_strip ("/select/solo", 0);
		clear_strip ("/select/recenable", 0);
		clear_strip ("/select/record_safe", 0);
		clear_strip ("/select/monitor_input", 0);
		clear_strip ("/select/monitor_disk", 0);
		clear_strip ("/select/polarity", 0);
		clear_strip ("/select/n_inputs", 0);
		clear_strip ("/select/n_outputs", 0);
	}
	if (feedback[1]) { // level controls
		if (gainmode) {
			clear_strip ("/select/fader", 0);
		} else {
			clear_strip ("/select/gain", -193);
		}
		clear_strip ("/select/trimdB", 0);
		clear_strip ("/select/pan_stereo_position", 0.5);
		clear_strip ("/select/pan_stereo_width", 1);
	}
	if (feedback[9]) {
		clear_strip ("/select/signal", 0);
	}
	if (feedback[7]) {
		if (gainmode) {
			clear_strip ("/select/meter", 0);
		} else {
			clear_strip ("/select/meter", -193);
		}
	}else if (feedback[8]) {
		clear_strip ("/select/meter", 0);
	}
	if (feedback[13]) { // Well known controls
		clear_strip ("/select/pan_elevation_position", 0);
		clear_strip ("/select/pan_frontback_position", .5);
		clear_strip ("/select/pan_lfe_control", 0);
		clear_strip ("/select/comp_enable", 0);
		clear_strip ("/select/comp_threshold", 0);
		clear_strip ("/select/comp_speed", 0);
		clear_strip ("/select/comp_mode", 0);
		text_message ("/select/comp_mode_name", " ");
		text_message ("/select/comp_speed_name", " ");
		clear_strip ("/select/comp_makeup", 0);
	}
	send_end();
	eq_end();

	lo_address_free (addr);
}

void
OSCSelectObserver::send_init()
{
	// we don't know how many there are, so find out.
	bool sends;
	do {
		sends = false;
		if (_strip->send_level_controllable (nsends)) {
			_strip->send_level_controllable(nsends)->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::send_gain, this, nsends, _strip->send_level_controllable(nsends)), OSC::instance());
			send_timeout.push_back (0);
			send_gain (nsends, _strip->send_level_controllable(nsends));
			sends = true;
		}

		if (_strip->send_enable_controllable (nsends)) {
			_strip->send_enable_controllable(nsends)->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::enable_message_with_id, this, X_("/select/send_enable"), nsends + 1, _strip->send_enable_controllable(nsends)), OSC::instance());
			enable_message_with_id ("/select/send_enable", nsends + 1, _strip->send_enable_controllable(nsends));
			sends = true;
		} else if (sends) {
			// not used by Ardour, just mixbus so in Ardour always true
			clear_strip_with_id ("/select/send_enable", nsends + 1, 1);
		}
		// this should get signalled by the route the send goes to, (TODO)
		if (!gainmode && sends) { // if the gain control is there, this is too
			text_with_id ("/select/send_name", nsends + 1, _strip->send_name(nsends));
		}
		// Send numbers are 0 based, OSC is 1 based so this gets incremented at the end
		if (sends) {
			nsends++;
		}
	} while (sends);
}

void
OSCSelectObserver::send_end ()
{
	send_connections.drop_connections ();
	for (uint32_t i = 1; i <= nsends; i++) {
		if (gainmode) {
			clear_strip_with_id ("/select/send_fader", i, 0);
		} else {
			clear_strip_with_id ("/select/send_gain", i, -193);
		}
		// next enable
		clear_strip_with_id ("/select/send_enable", i, 0);
		// next name
		text_with_id ("/select/send_name", i, " ");
	}
	nsends = 0;
}

void
OSCSelectObserver::send_restart(int x)
{
	send_end();
	send_init();
}

void
OSCSelectObserver::tick ()
{
	if (feedback[7] || feedback[8] || feedback[9]) { // meters enabled
		float now_meter;
		if (_strip->peak_meter()) {
			now_meter = _strip->peak_meter()->meter_level(0, MeterMCP);
		} else {
			now_meter = -193;
		}
		if (now_meter < -144) now_meter = -193;
		if (_last_meter != now_meter) {
			if (feedback[7] || feedback[8]) {
				string path = "/select/meter";
				lo_message msg = lo_message_new ();
				if (gainmode && feedback[7]) {
					lo_message_add_float (msg, ((now_meter + 94) / 100));
					lo_send_message (addr, path.c_str(), msg);
				} else if ((!gainmode) && feedback[7]) {
					lo_message_add_float (msg, now_meter);
					lo_send_message (addr, path.c_str(), msg);
				} else if (feedback[8]) {
					uint32_t ledlvl = (uint32_t)(((now_meter + 54) / 3.75)-1);
					uint16_t ledbits = ~(0xfff<<ledlvl);
					lo_message_add_int32 (msg, ledbits);
					lo_send_message (addr, path.c_str(), msg);
				}
				lo_message_free (msg);
			}
			if (feedback[9]) {
				string path = "/select/signal";
				lo_message msg = lo_message_new ();
				float signal;
				if (now_meter < -40) {
					signal = 0;
				} else {
					signal = 1;
				}
				lo_message_add_float (msg, signal);
				lo_send_message (addr, path.c_str(), msg);
				lo_message_free (msg);
			}
		}
		_last_meter = now_meter;

	}
	if (feedback[1]) {
		if (gain_timeout) {
			if (gain_timeout == 1) {
				text_message ("/select/name", _strip->name());
			}
			gain_timeout--;
		}
	}
	if (feedback[13]) {
		for (uint32_t i = 0; i < send_timeout.size(); i++) {
			if (send_timeout[i]) {
				if (send_timeout[i] == 1) {
					text_with_id ("/select/send_name", i + 1, _strip->send_name(i));
				}
				send_timeout[i]--;
			}
		}
	}

}

void
OSCSelectObserver::name_changed (const PBD::PropertyChange& what_changed)
{
	if (!what_changed.contains (ARDOUR::Properties::name)) {
		return;
	}

	if (!_strip) {
		return;
	}

	text_message ("/select/name", _strip->name());
	boost::shared_ptr<Route> route = boost::dynamic_pointer_cast<Route> (_strip);
	if (route) {
		//spit out the comment at the same time
		text_message ("/select/comment", route->comment());
		// lets tell the surface how many inputs this strip has
		clear_strip ("/select/n_inputs", (float) route->n_inputs().n_total());
		// lets tell the surface how many outputs this strip has
		clear_strip ("/select/n_outputs", (float) route->n_outputs().n_total());
	}
}

void
OSCSelectObserver::change_message (string path, boost::shared_ptr<Controllable> controllable)
{
	lo_message msg = lo_message_new ();
	float val = controllable->get_value();

	lo_message_add_float (msg, (float) controllable->internal_to_interface (val));

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

void
OSCSelectObserver::enable_message (string path, boost::shared_ptr<Controllable> controllable)
{
	float val = controllable->get_value();
	if (val) {
		clear_strip (path, 1);
	} else {
		clear_strip (path, 0);
	}

}

void
OSCSelectObserver::change_message_with_id (string path, uint32_t id, boost::shared_ptr<Controllable> controllable)
{
	lo_message msg = lo_message_new ();
	float val = controllable->get_value();
	if (feedback[2]) {
		path = set_path (path, id);
	} else {
		lo_message_add_int32 (msg, id);
	}

	lo_message_add_float (msg, (float) controllable->internal_to_interface (val));

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

void
OSCSelectObserver::enable_message_with_id (string path, uint32_t id, boost::shared_ptr<Controllable> controllable)
{
	float val = controllable->get_value();
	if (val) {
		clear_strip_with_id (path, id, 1);
	} else {
		clear_strip_with_id (path, id, 0);
	}
}

void
OSCSelectObserver::text_message (string path, std::string text)
{
	lo_message msg = lo_message_new ();

	lo_message_add_string (msg, text.c_str());

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

void
OSCSelectObserver::monitor_status (boost::shared_ptr<Controllable> controllable)
{
	int disk, input;
	float val = controllable->get_value();
	switch ((int) val) {
		case 1:
			disk = 0;
			input = 1;
			break;
		case 2:
			disk = 1;
			input = 0;
			break;
		default:
			disk = 0;
			input = 0;
	}

	clear_strip ("/select/monitor_input", (float) input);
	clear_strip ("/select/monitor_disk", (float) disk);
}

void
OSCSelectObserver::trim_message (string path, boost::shared_ptr<Controllable> controllable)
{
	lo_message msg = lo_message_new ();

	lo_message_add_float (msg, (float) accurate_coefficient_to_dB (controllable->get_value()));

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

void
OSCSelectObserver::gain_message (string path, boost::shared_ptr<Controllable> controllable)
{
	lo_message msg = lo_message_new ();

	if (gainmode) {
		lo_message_add_float (msg, gain_to_slider_position (controllable->get_value()));
		text_message ("/select/name", string_compose ("%1%2%3", std::fixed, std::setprecision(2), accurate_coefficient_to_dB (controllable->get_value())));
		gain_timeout = 8;
	} else {
		if (controllable->get_value() < 1e-15) {
			lo_message_add_float (msg, -200);
		} else {
			lo_message_add_float (msg, accurate_coefficient_to_dB (controllable->get_value()));
		}
	}

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

void
OSCSelectObserver::send_gain (uint32_t id, boost::shared_ptr<PBD::Controllable> controllable)
{
	lo_message msg = lo_message_new ();
	string path;
	float value;
	float db;
#ifdef MIXBUS
		db = controllable->get_value();
#else
		if (controllable->get_value() < 1e-15) {
			db = -193;
		} else {
			db = accurate_coefficient_to_dB (controllable->get_value());
		}
#endif

	if (gainmode) {
		path = "/select/send_fader";
#ifdef MIXBUS
		value = controllable->internal_to_interface (controllable->get_value());
#else
		value = gain_to_slider_position (controllable->get_value());
#endif
	text_with_id ("/select/send_name" , id + 1, string_compose ("%1%2%3", std::fixed, std::setprecision(2), db));
	if (send_timeout.size() > id) {
		send_timeout[id] = 8;
	}
	} else {
		path = "/select/send_gain";
		value = db;
	}

	if (feedback[2]) {
		path = set_path (path, id + 1);
	} else {
		lo_message_add_int32 (msg, id + 1);
	}

	lo_message_add_float (msg, value);
	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

void
OSCSelectObserver::text_with_id (string path, uint32_t id, string name)
{
	lo_message msg = lo_message_new ();
	if (feedback[2]) {
		path = set_path (path, id);
	} else {
		lo_message_add_int32 (msg, id);
	}

	lo_message_add_string (msg, name.c_str());

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

void
OSCSelectObserver::comp_mode ()
{
	change_message ("/select/comp_mode", _strip->comp_mode_controllable());
	text_message ("/select/comp_mode_name", _strip->comp_mode_name(_strip->comp_mode_controllable()->get_value()));
	text_message ("/select/comp_speed_name", _strip->comp_speed_name(_strip->comp_mode_controllable()->get_value()));
}

void
OSCSelectObserver::eq_init()
{
	// HPF and enable are special case, rest are in bands
	if (_strip->eq_hpf_controllable ()) {
		_strip->eq_hpf_controllable ()->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message, this, X_("/select/eq_hpf"), _strip->eq_hpf_controllable()), OSC::instance());
		change_message ("/select/eq_hpf", _strip->eq_hpf_controllable());
	}
	if (_strip->eq_enable_controllable ()) {
		_strip->eq_enable_controllable ()->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::enable_message, this, X_("/select/eq_enable"), _strip->eq_enable_controllable()), OSC::instance());
		enable_message ("/select/eq_enable", _strip->eq_enable_controllable());
	}

	uint32_t eq_bands = _strip->eq_band_cnt ();
	if (!eq_bands) {
		return;
	}

	for (uint32_t i = 0; i < eq_bands; i++) {
		if (_strip->eq_band_name(i).size()) {
			text_with_id ("/select/eq_band_name", i + 1, _strip->eq_band_name (i));
		}
		if (_strip->eq_gain_controllable (i)) {
			_strip->eq_gain_controllable(i)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message_with_id, this, X_("/select/eq_gain"), i + 1, _strip->eq_gain_controllable(i)), OSC::instance());
			change_message_with_id ("/select/eq_gain", i + 1, _strip->eq_gain_controllable(i));
		}
		if (_strip->eq_freq_controllable (i)) {
			_strip->eq_freq_controllable(i)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message_with_id, this, X_("/select/eq_freq"), i + 1, _strip->eq_freq_controllable(i)), OSC::instance());
			change_message_with_id ("/select/eq_freq", i + 1, _strip->eq_freq_controllable(i));
		}
		if (_strip->eq_q_controllable (i)) {
			_strip->eq_q_controllable(i)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message_with_id, this, X_("/select/eq_q"), i + 1, _strip->eq_q_controllable(i)), OSC::instance());
			change_message_with_id ("/select/eq_q", i + 1, _strip->eq_q_controllable(i));
		}
		if (_strip->eq_shape_controllable (i)) {
			_strip->eq_shape_controllable(i)->Changed.connect (eq_connections, MISSING_INVALIDATOR, boost::bind (&OSCSelectObserver::change_message_with_id, this, X_("/select/eq_shape"), i + 1, _strip->eq_shape_controllable(i)), OSC::instance());
			change_message_with_id ("/select/eq_shape", i + 1, _strip->eq_shape_controllable(i));
		}
	}
}

void
OSCSelectObserver::eq_end ()
{
	//need to check feedback for [13]
	eq_connections.drop_connections ();
	clear_strip ("/select/eq_hpf", 0);
	clear_strip ("/select/eq_enable", 0);

	for (uint32_t i = 1; i <= _strip->eq_band_cnt (); i++) {
		text_with_id ("/select/eq_band_name", i, " ");
		clear_strip_with_id ("/select/eq_gain", i, 0);
		clear_strip_with_id ("/select/eq_freq", i, 0);
		clear_strip_with_id ("/select/eq_q", i, 0);
		clear_strip_with_id ("/select/eq_shape", i, 0);


	}
}

void
OSCSelectObserver::eq_restart(int x)
{
	eq_end();
	eq_init();
}

string
OSCSelectObserver::set_path (string path, uint32_t id)
{
	if (feedback[2]) {
		path = string_compose ("%1/%2", path, id);
	}
	return path;
}

void
OSCSelectObserver::clear_strip (string path, float val)
{
	lo_message msg = lo_message_new ();
	lo_message_add_float (msg, val);

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);

}

void
OSCSelectObserver::clear_strip_with_id (string path, uint32_t id, float val)
{
	lo_message msg = lo_message_new ();
	if (feedback[2]) {
		path = set_path (path, id);
	} else {
		lo_message_add_int32 (msg, id);
	}

	lo_message_add_float (msg, val);

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);

}

