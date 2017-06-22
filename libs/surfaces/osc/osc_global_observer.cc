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

#include "boost/lambda/lambda.hpp"

#include "pbd/control_math.h"

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

OSCGlobalObserver::OSCGlobalObserver (Session& s, lo_address a, uint32_t gm, std::bitset<32> fb)
	: gainmode (gm)
	,feedback (fb)
{
	addr = lo_address_new (lo_address_get_hostname(a) , lo_address_get_port(a));
	session = &s;
	_last_frame = -1;
	if (feedback[4]) {

		// connect to all the things we want to send feed back from

		/*
		* 	Master (todo)
		* 		Pan width
		*/

		// Master channel first
		text_message (X_("/master/name"), "Master");
		boost::shared_ptr<Stripable> strip = session->master_out();

		boost::shared_ptr<Controllable> mute_controllable = boost::dynamic_pointer_cast<Controllable>(strip->mute_control());
		mute_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_change_message, this, X_("/master/mute"), strip->mute_control()), OSC::instance());
		send_change_message ("/master/mute", strip->mute_control());

		boost::shared_ptr<Controllable> trim_controllable = boost::dynamic_pointer_cast<Controllable>(strip->trim_control());
		trim_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_trim_message, this, X_("/master/trimdB"), strip->trim_control()), OSC::instance());
		send_trim_message ("/master/trimdB", strip->trim_control());

		boost::shared_ptr<Controllable> pan_controllable = boost::dynamic_pointer_cast<Controllable>(strip->pan_azimuth_control());
		if (pan_controllable) {
			pan_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_change_message, this, X_("/master/pan_stereo_position"), strip->pan_azimuth_control()), OSC::instance());
			send_change_message ("/master/pan_stereo_position", strip->pan_azimuth_control());
		}

		boost::shared_ptr<Controllable> gain_controllable = boost::dynamic_pointer_cast<Controllable>(strip->gain_control());
		gain_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_gain_message, this, X_("/master/"), strip->gain_control()), OSC::instance());
		send_gain_message ("/master/", strip->gain_control());

		// monitor stuff next
		strip = session->monitor_out();
		if (strip) {
			text_message (X_("/monitor/name"), "Monitor");

			boost::shared_ptr<Controllable> mon_mute_cont = strip->monitor_control()->cut_control();
			mon_mute_cont->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_change_message, this, X_("/monitor/mute"), mon_mute_cont), OSC::instance());
			send_change_message ("/monitor/mute", mon_mute_cont);

			boost::shared_ptr<Controllable> mon_dim_cont = strip->monitor_control()->dim_control();
			mon_dim_cont->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_change_message, this, X_("/monitor/dim"), mon_dim_cont), OSC::instance());
			send_change_message ("/monitor/dim", mon_dim_cont);

			boost::shared_ptr<Controllable> mon_mono_cont = strip->monitor_control()->mono_control();
			mon_mono_cont->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_change_message, this, X_("/monitor/mono"), mon_mono_cont), OSC::instance());
			send_change_message ("/monitor/mono", mon_mono_cont);

			gain_controllable = boost::dynamic_pointer_cast<Controllable>(strip->gain_control());
				gain_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_gain_message, this, X_("/monitor/"), strip->gain_control()), OSC::instance());
				send_gain_message ("/monitor/", strip->gain_control());
		}

		/*
		* 	Transport (todo)
		* 		punchin/out
		*/
		//Transport feedback
		session->TransportStateChange.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_transport_state_changed, this), OSC::instance());
		send_transport_state_changed ();
		session->TransportLooped.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_transport_state_changed, this), OSC::instance());
		session->RecordStateChanged.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_record_state_changed, this), OSC::instance());
		send_record_state_changed ();

		// session feedback
		session->StateSaved.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::text_message, this, X_("/session_name"), _1), OSC::instance());
		text_message (X_("/session_name"), session->snap_name());
		session->SoloActive.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::solo_active, this, _1), OSC::instance());
		solo_active (session->soloing() || session->listening());

		/*
		* 	Maybe (many) more
		*/
	}
}

OSCGlobalObserver::~OSCGlobalObserver ()
{

	// need to add general zero everything messages
	strip_connections.drop_connections ();
	session_connections.drop_connections ();

	lo_address_free (addr);
}

void
OSCGlobalObserver::tick ()
{
	framepos_t now_frame = session->transport_frame();
	if (now_frame != _last_frame) {
		if (feedback[6]) { // timecode enabled
			Timecode::Time timecode;
			session->timecode_time (now_frame, timecode);

			// Timecode mode: Hours/Minutes/Seconds/Frames
			ostringstream os;
			os << setw(2) << setfill('0') << timecode.hours;
			os << ':';
			os << setw(2) << setfill('0') << timecode.minutes;
			os << ':';
			os << setw(2) << setfill('0') << timecode.seconds;
			os << ':';
			os << setw(2) << setfill('0') << timecode.frames;

			text_message ("/position/smpte", os.str());
		}
		if (feedback[5]) { // Bar beat enabled
			Timecode::BBT_Time bbt_time;

			session->bbt_time (now_frame, bbt_time);

			// semantics:  BBB/bb/tttt
			ostringstream os;

			os << setw(3) << setfill('0') << bbt_time.bars;
			os << '|';
			os << setw(2) << setfill('0') << bbt_time.beats;
			os << '|';
			os << setw(4) << setfill('0') << bbt_time.ticks;

			text_message ("/position/bbt", os.str());
		}
		if (feedback[11]) { // minutes/seconds enabled
			framepos_t left = now_frame;
			int hrs = (int) floor (left / (session->frame_rate() * 60.0f * 60.0f));
			left -= (framecnt_t) floor (hrs * session->frame_rate() * 60.0f * 60.0f);
			int mins = (int) floor (left / (session->frame_rate() * 60.0f));
			left -= (framecnt_t) floor (mins * session->frame_rate() * 60.0f);
			int secs = (int) floor (left / (float) session->frame_rate());
			left -= (framecnt_t) floor ((double)(secs * session->frame_rate()));
			int millisecs = floor (left * 1000.0 / (float) session->frame_rate());

			// Min/sec mode: Hours/Minutes/Seconds/msec
			ostringstream os;
			os << setw(2) << setfill('0') << hrs;
			os << ':';
			os << setw(2) << setfill('0') << mins;
			os << ':';
			os << setw(2) << setfill('0') << secs;
			os << '.';
			os << setw(3) << setfill('0') << millisecs;

			text_message ("/position/time", os.str());
		}
		if (feedback[10]) { // samples
			ostringstream os;
			os << now_frame;
			text_message ("/position/samples", os.str());
		}
		_last_frame = now_frame;
	}
	if (feedback[3]) { //heart beat enabled
		if (_heartbeat == 10) {
			float_message (X_("/heartbeat"), 1.0);
		}
		if (!_heartbeat) {
			float_message (X_("/heartbeat"), 0.0);
		}
		_heartbeat++;
		if (_heartbeat > 20) _heartbeat = 0;
	}
	if (feedback[7] || feedback[8] || feedback[9]) { // meters enabled
		// the only meter here is master
		float now_meter = session->master_out()->peak_meter()->meter_level(0, MeterMCP);
		if (now_meter < -94) now_meter = -193;
		if (_last_meter != now_meter) {
			if (feedback[7] || feedback[8]) {
				if (gainmode && feedback[7]) {
					// change from db to 0-1
					float_message (X_("/master/meter"), ((now_meter + 94) / 100));
				} else if ((!gainmode) && feedback[7]) {
					float_message (X_("/master/meter"), now_meter);
				} else if (feedback[8]) {
					uint32_t ledlvl = (uint32_t)(((now_meter + 54) / 3.75)-1);
					uint32_t ledbits = ~(0xfff<<ledlvl);
					int_message (X_("/master/meter"), ledbits);
				}
			}
			if (feedback[9]) {
				float signal;
				if (now_meter < -40) {
					signal = 0;
				} else {
					signal = 1;
				}
				float_message (X_("/master/signal"), signal);
			}
		}
		_last_meter = now_meter;

	}
	if (feedback[4]) {
		if (master_timeout) {
			if (master_timeout == 1) {
				text_message (X_("/master/name"), "Master");
			}
			master_timeout--;
		}
		if (monitor_timeout) {
			if (monitor_timeout == 1) {
				text_message (X_("/monitor/name"), "Monitor");
			}
			monitor_timeout--;
		}
	}
}

void
OSCGlobalObserver::send_change_message (string path, boost::shared_ptr<Controllable> controllable)
{
	float_message (path, (float) controllable->get_value());
}

void
OSCGlobalObserver::send_gain_message (string path, boost::shared_ptr<Controllable> controllable)
{
	if (gainmode) {
		float_message (string_compose ("%1fader", path), controllable->internal_to_interface (controllable->get_value()));
		text_message (string_compose ("%1name", path), string_compose ("%1%2%3", std::fixed, std::setprecision(2), accurate_coefficient_to_dB (controllable->get_value())));
		if (path.find("master") != std::string::npos) {
			master_timeout = 8;
		} else {
			monitor_timeout = 8;
		}

	} else {
		if (controllable->get_value() < 1e-15) {
			float_message (string_compose ("%1gain",path), -200);
		} else {
			float_message (string_compose ("%1gain",path), accurate_coefficient_to_dB (controllable->get_value()));
		}
	}
}

void
OSCGlobalObserver::send_trim_message (string path, boost::shared_ptr<Controllable> controllable)
{
	float_message (X_("/master/trimdB"), (float) accurate_coefficient_to_dB (controllable->get_value()));
}


void
OSCGlobalObserver::send_transport_state_changed()
{
	int_message (X_("/loop_toggle"), session->get_play_loop());
	int_message (X_("/transport_play"), session->transport_speed() == 1.0);
	int_message (X_("/transport_stop"), session->transport_stopped());
	int_message (X_("/rewind"), session->transport_speed() < 0.0);
	int_message (X_("/ffwd"), (session->transport_speed() != 1.0 && session->transport_speed() > 0.0));
}

void
OSCGlobalObserver::send_record_state_changed ()
{
	int_message (X_("/rec_enable_toggle"), (int)session->get_record_enabled ());

	if (session->have_rec_enabled_track ()) {
		int_message (X_("/record_tally"), 1);
	} else {
		int_message (X_("/record_tally"), 0);
	}
}

void
OSCGlobalObserver::solo_active (bool active)
{
	float_message (X_("/cancel_all_solos"), (float) active);
}

void
OSCGlobalObserver::text_message (string path, std::string text)
{
	lo_message msg = lo_message_new ();

	lo_message_add_string (msg, text.c_str());

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

void
OSCGlobalObserver::float_message (string path, float value)
{
	lo_message msg = lo_message_new ();

	lo_message_add_float (msg, value);

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

void
OSCGlobalObserver::int_message (string path, uint32_t value)
{
	lo_message msg = lo_message_new ();

	lo_message_add_int32 (msg, value);

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}
