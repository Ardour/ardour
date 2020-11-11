/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include "boost/lambda/lambda.hpp"

#include "pbd/control_math.h"

#include "ardour/amp.h"
#include "ardour/session.h"
#include "ardour/dB.h"
#include "ardour/meter.h"
#include "ardour/monitor_processor.h"

#include "osc.h"
#include "osc_global_observer.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace ArdourSurface;

OSCGlobalObserver::OSCGlobalObserver (OSC& o, Session& s, ArdourSurface::OSC::OSCSurface* su)
	: _osc (o)
	,sur (su)
	,_init (true)
	,_last_master_gain (-1.0)
	,_last_master_trim (-1.0)
	,_last_monitor_gain (-1.0)
	,_jog_mode (1024)
	,last_punchin (4)
	,last_punchout (4)
	,last_click (4)
{
	addr = lo_address_new_from_url 	(sur->remote_url.c_str());
	session = &s;
	gainmode = sur->gainmode;
	feedback = sur->feedback;
	uint32_t jogmode = sur->jogmode;
	_last_sample = -1;
	mark_text = "";
	if (feedback[4]) {

		// connect to all the things we want to send feed back from

		/*
		* 	Master (todo)
		* 		Pan width
		*/

		// Master channel first
		_osc.text_message (X_("/master/name"), "Master", addr);
		boost::shared_ptr<Stripable> strip = session->master_out();

		boost::shared_ptr<Controllable> mute_controllable = boost::dynamic_pointer_cast<Controllable>(strip->mute_control());
		mute_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_change_message, this, X_("/master/mute"), strip->mute_control()), OSC::instance());
		send_change_message (X_("/master/mute"), mute_controllable);

		boost::shared_ptr<Controllable> trim_controllable = boost::dynamic_pointer_cast<Controllable>(strip->trim_control());
		trim_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_trim_message, this, X_("/master/trimdB"), strip->trim_control()), OSC::instance());
		send_trim_message (X_("/master/trimdB"), trim_controllable);

		boost::shared_ptr<Controllable> pan_controllable = boost::dynamic_pointer_cast<Controllable>(strip->pan_azimuth_control());
		if (pan_controllable) {
			pan_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_change_message, this, X_("/master/pan_stereo_position"), strip->pan_azimuth_control()), OSC::instance());
			send_change_message (X_("/master/pan_stereo_position"), pan_controllable);
		}

		boost::shared_ptr<Controllable> gain_controllable = boost::dynamic_pointer_cast<Controllable>(strip->gain_control());
		gain_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_gain_message, this, X_("/master/"), strip->gain_control()), OSC::instance());
		send_gain_message (X_("/master/"), gain_controllable);

		// monitor stuff next
		strip = session->monitor_out();
		if (strip) {
			_osc.text_message (X_("/monitor/name"), "Monitor", addr);

			boost::shared_ptr<Controllable> mon_mute_cont = strip->monitor_control()->cut_control();
			mon_mute_cont->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_change_message, this, X_("/monitor/mute"), mon_mute_cont), OSC::instance());
			send_change_message (X_("/monitor/mute"), mon_mute_cont);

			boost::shared_ptr<Controllable> mon_dim_cont = strip->monitor_control()->dim_control();
			mon_dim_cont->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_change_message, this, X_("/monitor/dim"), mon_dim_cont), OSC::instance());
			send_change_message (X_("/monitor/dim"), mon_dim_cont);

			boost::shared_ptr<Controllable> mon_mono_cont = strip->monitor_control()->mono_control();
			mon_mono_cont->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_change_message, this, X_("/monitor/mono"), mon_mono_cont), OSC::instance());
			send_change_message (X_("/monitor/mono"), mon_mono_cont);

			gain_controllable = boost::dynamic_pointer_cast<Controllable>(strip->gain_control());
			gain_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_gain_message, this, X_("/monitor/"), strip->gain_control()), OSC::instance());
			send_gain_message (X_("/monitor/"), gain_controllable);
		}

		//Transport feedback
		session->TransportStateChange.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_transport_state_changed, this), OSC::instance());
		send_transport_state_changed ();
		session->TransportLooped.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_transport_state_changed, this), OSC::instance());
		session->RecordStateChanged.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_record_state_changed, this), OSC::instance());
		send_record_state_changed ();
		marks_changed ();

		// session feedback
		session->StateSaved.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::session_name, this, X_("/session_name"), _1), OSC::instance());
		session_name (X_("/session_name"), session->snap_name());
		session->SoloActive.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::solo_active, this, _1), OSC::instance());
		solo_active (session->soloing() || session->listening());

		boost::shared_ptr<Controllable> click_controllable = boost::dynamic_pointer_cast<Controllable>(session->click_gain()->gain_control());
		click_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_change_message, this, X_("/click/level"), click_controllable), OSC::instance());
		send_change_message (X_("/click/level"), click_controllable);

		session->route_group_added.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::group_changed, this, _1), OSC::instance());
		session->route_group_removed.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::group_changed, this), OSC::instance());
		session->route_groups_reordered.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::group_changed, this), OSC::instance());
		_osc.send_group_list (addr);

		extra_check ();
		jog_mode (jogmode);

		/*
		* 	Maybe (many) more
		*/
	}
	_init = false;
}

OSCGlobalObserver::~OSCGlobalObserver ()
{
	_init = true;

	// need to add general zero everything messages
	strip_connections.drop_connections ();
	session_connections.drop_connections ();

	lo_address_free (addr);
}

void
OSCGlobalObserver::clear_observer ()
{
	strip_connections.drop_connections ();
	session_connections.drop_connections ();
	_osc.text_message (X_("/master/name"), " ", addr);
	_osc.text_message (X_("/monitor/name"), " ", addr);
	_osc.text_message (X_("/session_name"), " ", addr);
	_osc.text_message (X_("/marker"), " ", addr);
	if (feedback[6]) { // timecode enabled
		_osc.text_message (X_("/position/smpte"), " ", addr);
	}
	if (feedback[5]) { // Bar beat enabled
		_osc.text_message (X_("/position/bbt"), " ", addr);
	}
	if (feedback[11]) { // minutes/seconds enabled
		_osc.text_message (X_("/position/time"), " ", addr);
	}
	if (feedback[10]) { // samples
		_osc.text_message (X_("/position/samples"), " ", addr);
	}
	if (feedback[3]) { //heart beat enabled
		_osc.float_message (X_("/heartbeat"), 0.0, addr);
	}
	if (feedback[7] || feedback[8]) { // meters enabled
		float meter = 0;
		if (feedback[7] && !gainmode) {
			meter = -193;
		}
		_osc.float_message (X_("/master/meter"), meter, addr);
	}
	if (feedback[9]) {
		_osc.float_message (X_("/master/signal"), 0, addr);
	}
	_osc.float_message (X_("/master/fader"), 0, addr);
	_osc.float_message (X_("/monitor/fader"), 0, addr);
	_osc.float_message (X_("/master/gain"), -193, addr);
	_osc.float_message (X_("/monitor/gain"), -193, addr);
	_osc.float_message (X_("/master/trimdB"), 0, addr);
	_osc.float_message (X_("/master/mute"), 0, addr);
	_osc.float_message (X_("/master/pan_stereo_position"), 0.5, addr);
	_osc.float_message (X_("/monitor/mute"), 0, addr);
	_osc.float_message (X_("/monitor/dim"), 0, addr);
	_osc.float_message (X_("/monitor/mono"), 0, addr);
	_osc.float_message (X_("/loop_toggle"), 0, addr);
	_osc.float_message (X_("/transport_play"), 0, addr);
	_osc.float_message (X_("/transport_stop"), 0, addr);
	_osc.float_message (X_("/toggle_roll"), 0, addr);
	_osc.float_message (X_("/rewind"), 0, addr);
	_osc.float_message (X_("/ffwd"), 0, addr);
	_osc.float_message (X_("/record_tally"), 0, addr);
	_osc.float_message (X_("/rec_enable_toggle"), 0, addr);
	_osc.float_message (X_("/cancel_all_solos"), 0, addr);
	_osc.float_message (X_("/toggle_punch_out"), 0, addr);
	_osc.float_message (X_("/toggle_punch_in"), 0, addr);
	_osc.float_message (X_("/toggle_click"), 0, addr);
	_osc.float_message (X_("/click/level"), 0, addr);
	_osc.text_message (X_("/group/list"), " ", addr);
	_osc.text_message (X_("/jog/mode/name"), " ", addr);
	_osc.int_message (X_("/jog/mode"), 0, addr);


}

void
OSCGlobalObserver::tick ()
{
	if (_init) {
		return;
	}
	samplepos_t now_sample = session->transport_sample();
	if (now_sample != _last_sample) {
		if (feedback[6]) { // timecode enabled
			Timecode::Time timecode;
			session->timecode_time (now_sample, timecode);

			// Timecode mode: Hours/Minutes/Seconds/Samples
			ostringstream os;
			os << setw(2) << setfill('0') << timecode.hours;
			os << ':';
			os << setw(2) << setfill('0') << timecode.minutes;
			os << ':';
			os << setw(2) << setfill('0') << timecode.seconds;
			os << ':';
			os << setw(2) << setfill('0') << timecode.frames;

			_osc.text_message (X_("/position/smpte"), os.str(), addr);
		}
		if (feedback[5]) { // Bar beat enabled
			Temporal::BBT_Time bbt_time;

			session->bbt_time (timepos_t (now_sample), bbt_time);

			// semantics:  BBB/bb/tttt
			ostringstream os;

			os << setw(3) << setfill('0') << bbt_time.bars;
			os << '|';
			os << setw(2) << setfill('0') << bbt_time.beats;
			os << '|';
			os << setw(4) << setfill('0') << bbt_time.ticks;

			_osc.text_message (X_("/position/bbt"), os.str(), addr);
		}
		if (feedback[11]) { // minutes/seconds enabled
			samplepos_t left = now_sample;
			int hrs = (int) floor (left / (session->sample_rate() * 60.0f * 60.0f));
			left -= (samplecnt_t) floor (hrs * session->sample_rate() * 60.0f * 60.0f);
			int mins = (int) floor (left / (session->sample_rate() * 60.0f));
			left -= (samplecnt_t) floor (mins * session->sample_rate() * 60.0f);
			int secs = (int) floor (left / (float) session->sample_rate());
			left -= (samplecnt_t) floor ((double)(secs * session->sample_rate()));
			int millisecs = floor (left * 1000.0 / (float) session->sample_rate());

			// Min/sec mode: Hours/Minutes/Seconds/msec
			ostringstream os;
			os << setw(2) << setfill('0') << hrs;
			os << ':';
			os << setw(2) << setfill('0') << mins;
			os << ':';
			os << setw(2) << setfill('0') << secs;
			os << '.';
			os << setw(3) << setfill('0') << millisecs;

			_osc.text_message (X_("/position/time"), os.str(), addr);
		}
		if (feedback[10]) { // samples
			ostringstream os;
			os << now_sample;
			_osc.text_message (X_("/position/samples"), os.str(), addr);
		}
		_last_sample = now_sample;
		mark_update ();
	} else {
		if (!_heartbeat) {
			marks_changed ();
		}
	}
	if (feedback[3]) { //heart beat enabled
		if (_heartbeat == 10) {
			_osc.float_message (X_("/heartbeat"), 1.0, addr);
		}
		if (!_heartbeat) {
			_osc.float_message (X_("/heartbeat"), 0.0, addr);
		}
	}
	if (feedback[7] || feedback[8] || feedback[9]) { // meters enabled
		// the only meter here is master
		float now_meter = session->master_out()->peak_meter()->meter_level(0, MeterMCP);
		if (now_meter < -94) now_meter = -193;
		if (_last_meter != now_meter) {
			if (feedback[7] || feedback[8]) {
				if (gainmode && feedback[7]) {
					// change from db to 0-1
					_osc.float_message (X_("/master/meter"), ((now_meter + 94) / 100), addr);
				} else if ((!gainmode) && feedback[7]) {
					_osc.float_message (X_("/master/meter"), now_meter, addr);
				} else if (feedback[8]) {
					uint32_t ledlvl = (uint32_t)(((now_meter + 54) / 3.75)-1);
					uint32_t ledbits = ~(0xfff<<ledlvl);
					_osc.float_message (X_("/master/meter"), ledbits, addr);
				}
			}
			if (feedback[9]) {
				float signal;
				if (now_meter < -40) {
					signal = 0;
				} else {
					signal = 1;
				}
				_osc.float_message (X_("/master/signal"), signal, addr);
			}
		}
		_last_meter = now_meter;

	}
	if (feedback[4]) {
		if (master_timeout) {
			if (master_timeout == 1) {
				_osc.text_message (X_("/master/name"), "Master", addr);
			}
			master_timeout--;
		}
		if (monitor_timeout) {
			if (monitor_timeout == 1) {
				_osc.text_message (X_("/monitor/name"), "Monitor", addr);
			}
			monitor_timeout--;
		}
		extra_check ();
	}
	_heartbeat++;
	if (_heartbeat > 20) _heartbeat = 0;
}

void
OSCGlobalObserver::send_change_message (string path, boost::shared_ptr<Controllable> controllable)
{
	float val = controllable->get_value();
	_osc.float_message (path, (float) controllable->internal_to_interface (val), addr);
}

void
OSCGlobalObserver::session_name (string path, string name)
{
	_osc.text_message (path, name, addr);
}

void
OSCGlobalObserver::send_gain_message (string path, boost::shared_ptr<Controllable> controllable)
{
	bool ismaster = false;

	if (path.find(X_("master")) != std::string::npos) {
		ismaster = true;
		if (_last_master_gain != controllable->get_value()) {
			_last_master_gain = controllable->get_value();
		} else {
			return;
		}
	} else {
		if (_last_monitor_gain != controllable->get_value()) {
			_last_monitor_gain = controllable->get_value();
		} else {
			return;
		}
	}
	if (gainmode) {
		_osc.float_message (string_compose (X_("%1fader"), path), controllable->internal_to_interface (controllable->get_value()), addr);
		if (gainmode == 1) {
			_osc.text_message (string_compose (X_("%1name"), path), string_compose ("%1%2%3", std::fixed, std::setprecision(2), accurate_coefficient_to_dB (controllable->get_value())), addr);
			if (ismaster) {
				master_timeout = 8;
			} else {
				monitor_timeout = 8;
			}
		}
	}
	if (!gainmode || gainmode == 2) {
		if (controllable->get_value() < 1e-15) {
			_osc.float_message (string_compose (X_("%1gain"),path), -200, addr);
		} else {
			_osc.float_message (string_compose (X_("%1gain"),path), accurate_coefficient_to_dB (controllable->get_value()), addr);
		}
	}
}

void
OSCGlobalObserver::send_trim_message (string path, boost::shared_ptr<Controllable> controllable)
{
	if (_last_master_trim != controllable->get_value()) {
		_last_master_trim = controllable->get_value();
	} else {
		return;
	}
	_osc.float_message (X_("/master/trimdB"), (float) accurate_coefficient_to_dB (controllable->get_value()), addr);
}


void
OSCGlobalObserver::send_transport_state_changed()
{
	_osc.float_message (X_("/loop_toggle"), session->get_play_loop(), addr);
	_osc.float_message (X_("/transport_play"), session->actual_speed() == 1.0, addr);
	_osc.float_message (X_("/toggle_roll"), session->actual_speed() == 1.0, addr);
	_osc.float_message (X_("/transport_stop"), session->transport_stopped_or_stopping(), addr);
	_osc.float_message (X_("/rewind"), session->actual_speed() < 0.0, addr);
	_osc.float_message (X_("/ffwd"), (session->actual_speed() != 1.0 && session->actual_speed() > 0.0), addr);
}

void
OSCGlobalObserver::marks_changed ()
{
	lm.clear();
	const Locations::LocationList& ll (session->locations ()->list ());
	// get Locations that are marks
	for (Locations::LocationList::const_iterator l = ll.begin(); l != ll.end(); ++l) {
		if ((*l)->is_session_range ()) {
			lm.push_back (LocationMarker(_("start"), (*l)->start_sample ()));
			lm.push_back (LocationMarker(_("end"), (*l)->end_sample ()));
			continue;
		}
		if ((*l)->is_mark ()) {
			lm.push_back (LocationMarker((*l)->name(), (*l)->start_sample ()));
		}
	}
	// sort them by position
	LocationMarkerSort location_marker_sort;
	std::sort (lm.begin(), lm.end(), location_marker_sort);
	mark_update ();

}

void
OSCGlobalObserver::mark_update ()
{
	string send_str = "No Marks";
	if (lm.size()) {
		uint32_t prev = 0;
		uint32_t next = lm.size() - 1;
		for (uint32_t i = 0; i < lm.size (); i++) {
			if (lm[i].when <= _last_sample) {
				prev = i;
			}
			if (lm[i].when >= _last_sample) {
				next = i;
				break;
			}
		}
		if (_last_sample > lm[lm.size() - 1].when) {
			send_str = string_compose ("%1 <-", lm[lm.size() - 1].label);
		} else if (_last_sample < lm[0].when) {
			send_str = string_compose ("-> %1", lm[0].label);
		} else if (prev == next) {
			send_str = lm[prev].label;
			prev_mark = lm[prev].when;
			next_mark = lm[next].when;
		} else if ((prev_mark != lm[prev].when) || (next_mark != lm[next].when)) {
			send_str = string_compose ("%1 <-> %2", lm[prev].label, lm[next].label);
		}
	}
	if (send_str != mark_text) {
		mark_text = send_str;
		_osc.text_message (X_("/marker"), send_str, addr);
	}

}

void
OSCGlobalObserver::send_record_state_changed ()
{
	_osc.float_message (X_("/rec_enable_toggle"), (int)session->get_record_enabled (), addr);

	if (session->have_rec_enabled_track () || session->get_record_enabled ()) {
		_osc.float_message (X_("/record_tally"), 1, addr);
	} else {
		_osc.float_message (X_("/record_tally"), 0, addr);
	}
}

void
OSCGlobalObserver::solo_active (bool active)
{
	_osc.float_message (X_("/cancel_all_solos"), (float) active, addr);
}

void
OSCGlobalObserver::extra_check ()
{
	if (last_punchin != session->config.get_punch_in()) {
		last_punchin = session->config.get_punch_in();
		_osc.float_message (X_("/toggle_punch_in"), last_punchin, addr);
	}
	if (last_punchout != session->config.get_punch_out()) {
		last_punchout = session->config.get_punch_out();
		_osc.float_message (X_("/toggle_punch_out"), last_punchout, addr);
	}
	if (last_click != Config->get_clicking()) {
		last_click = Config->get_clicking();
		_osc.float_message (X_("/toggle_click"), last_click, addr);
	}
}

void
OSCGlobalObserver::jog_mode (uint32_t jogmode)
{
	if (jogmode == _jog_mode || !feedback[4]) {
		// no change
		return;
	}
	_jog_mode = jogmode;

	switch(jogmode)
	{
		case 0:
			_osc.text_message (X_("/jog/mode/name"), "Jog", addr);
			break;
		case 1:
			_osc.text_message (X_("/jog/mode/name"), "Nudge", addr);
			break;
		case 2:
			_osc.text_message (X_("/jog/mode/name"), "Scrub", addr);
			break;
		case 3:
			_osc.text_message (X_("/jog/mode/name"), "Shuttle", addr);
			break;
		case 4:
			_osc.text_message (X_("/jog/mode/name"), "Marker", addr);
			break;
		case 5:
			_osc.text_message (X_("/jog/mode/name"), "Scroll", addr);
			break;
		case 6:
			_osc.text_message (X_("/jog/mode/name"), "Track", addr);
			break;
		case 7:
			_osc.text_message (X_("/jog/mode/name"), "Bank", addr);
			break;
		default:
			PBD::warning << X_("Jog Mode: ") << jogmode << X_(" is not valid.") << endmsg;
			break;
	}
	_osc.int_message (X_("/jog/mode"), jogmode, addr);
}

void
OSCGlobalObserver::group_changed (ARDOUR::RouteGroup *rg)
{
	_osc.send_group_list (addr);
}

void
OSCGlobalObserver::group_changed ()
{
	_osc.send_group_list (addr);
}

