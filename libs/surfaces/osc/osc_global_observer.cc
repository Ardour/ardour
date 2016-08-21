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

#include "ardour/session.h"
#include "ardour/dB.h"
#include "ardour/meter.h"

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
		boost::shared_ptr<Stripable> strip = session->master_out();

		boost::shared_ptr<Controllable> mute_controllable = boost::dynamic_pointer_cast<Controllable>(strip->mute_control());
		mute_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, bind (&OSCGlobalObserver::send_change_message, this, X_("/master/mute"), strip->mute_control()), OSC::instance());
		send_change_message ("/master/mute", strip->mute_control());

		boost::shared_ptr<Controllable> trim_controllable = boost::dynamic_pointer_cast<Controllable>(strip->trim_control());
		trim_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, bind (&OSCGlobalObserver::send_trim_message, this, X_("/master/trimdB"), strip->trim_control()), OSC::instance());
		send_trim_message ("/master/trimdB", strip->trim_control());

		boost::shared_ptr<Controllable> pan_controllable = boost::dynamic_pointer_cast<Controllable>(strip->pan_azimuth_control());
		if (pan_controllable) {
			pan_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, bind (&OSCGlobalObserver::send_change_message, this, X_("/master/pan_stereo_position"), strip->pan_azimuth_control()), OSC::instance());
			send_change_message ("/master/pan_stereo_position", strip->pan_azimuth_control());
		}

		boost::shared_ptr<Controllable> gain_controllable = boost::dynamic_pointer_cast<Controllable>(strip->gain_control());
		if (gainmode) {
			gain_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, bind (&OSCGlobalObserver::send_gain_message, this, X_("/master/fader"), strip->gain_control()), OSC::instance());
			send_gain_message ("/master/fader", strip->gain_control());
		} else {
			gain_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, bind (&OSCGlobalObserver::send_gain_message, this, X_("/master/gain"), strip->gain_control()), OSC::instance());
			send_gain_message ("/master/gain", strip->gain_control());
		}

		// monitor stuff next
		/*
		* 	Monitor (todo)
		* 		Mute
		* 		Dim
		* 		Mono
		* 		Rude Solo
		* 		etc.
		*/
		strip = session->monitor_out();
		if (strip) {

			// Hmm, it seems the monitor mute is not at route->mute_control()
			/*boost::shared_ptr<Controllable> mute_controllable2 = boost::dynamic_pointer_cast<Controllable>(strip->mute_control());
			//mute_controllable = boost::dynamic_pointer_cast<Controllable>(r2->mute_control());
			mute_controllable2->Changed.connect (monitor_mute_connection, MISSING_INVALIDATOR, bind (&OSCGlobalObserver::send_change_message, this, X_("/monitor/mute"), strip->mute_control()), OSC::instance());
			send_change_message ("/monitor/mute", strip->mute_control());
			*/
			gain_controllable = boost::dynamic_pointer_cast<Controllable>(strip->gain_control());
			if (gainmode) {
				gain_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, bind (&OSCGlobalObserver::send_gain_message, this, X_("/monitor/fader"), strip->gain_control()), OSC::instance());
				send_gain_message ("/monitor/fader", strip->gain_control());
			} else {
				gain_controllable->Changed.connect (strip_connections, MISSING_INVALIDATOR, bind (&OSCGlobalObserver::send_gain_message, this, X_("/monitor/gain"), strip->gain_control()), OSC::instance());
				send_gain_message ("/monitor/gain", strip->gain_control());
			}
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
		session->StateSaved.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::send_session_saved, this, _1), OSC::instance());
		send_session_saved (session->snap_name());
		session->SoloActive.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&OSCGlobalObserver::solo_active, this, _1), OSC::instance());
		solo_active (session->soloing() || session->listening());

		/*
		* 	Maybe (many) more
		*/
	}
}

OSCGlobalObserver::~OSCGlobalObserver ()
{

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

			lo_message msg = lo_message_new ();
			lo_message_add_string (msg, os.str().c_str());
			lo_send_message (addr, "/position/smpte", msg);
			lo_message_free (msg);
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

			lo_message msg = lo_message_new ();
			lo_message_add_string (msg, os.str().c_str());
			lo_send_message (addr, "/position/bbt", msg);
			lo_message_free (msg);
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

			lo_message msg = lo_message_new ();
			lo_message_add_string (msg, os.str().c_str());
			lo_send_message (addr, "/position/time", msg);
			lo_message_free (msg);
		}
		if (feedback[10]) { // samples
			ostringstream os;
			os << now_frame;
			lo_message msg = lo_message_new ();
			lo_message_add_string (msg, os.str().c_str());
			lo_send_message (addr, "/position/samples", msg);
			lo_message_free (msg);
		}
		_last_frame = now_frame;
	}
	if (feedback[3]) { //heart beat enabled
		if (_heartbeat == 10) {
			lo_message msg = lo_message_new ();
			lo_message_add_float (msg, 1.0);
			lo_send_message (addr, "/heartbeat", msg);
			lo_message_free (msg);
		}
		if (!_heartbeat) {
			lo_message msg = lo_message_new ();
			lo_message_add_float (msg, 0.0);
			lo_send_message (addr, "/heartbeat", msg);
			lo_message_free (msg);
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
				lo_message msg = lo_message_new ();
				if (gainmode && feedback[7]) {
					// change from db to 0-1
					lo_message_add_float (msg, ((now_meter + 94) / 100));
					lo_send_message (addr, "/master/meter", msg);
				} else if ((!gainmode) && feedback[7]) {
					lo_message_add_float (msg, now_meter);
					lo_send_message (addr, "/master/meter", msg);
				} else if (feedback[8]) {
					uint32_t ledlvl = (uint32_t)(((now_meter + 54) / 3.75)-1);
					uint32_t ledbits = ~(0xfff<<ledlvl);
					lo_message_add_int32 (msg, ledbits);
					lo_send_message (addr, "/master/meter", msg);
				}
				lo_message_free (msg);
			}
			if (feedback[9]) {
				lo_message msg = lo_message_new ();
				float signal;
				if (now_meter < -40) {
					signal = 0;
				} else {
					signal = 1;
				}
				lo_message_add_float (msg, signal);
				lo_send_message (addr, "/master/signal", msg);
				lo_message_free (msg);
			}
		}
		_last_meter = now_meter;

	}
}

void
OSCGlobalObserver::send_change_message (string path, boost::shared_ptr<Controllable> controllable)
{
	lo_message msg = lo_message_new ();

	lo_message_add_float (msg, (float) controllable->get_value());


	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

void
OSCGlobalObserver::send_gain_message (string path, boost::shared_ptr<Controllable> controllable)
{
	lo_message msg = lo_message_new ();

	if (gainmode) {
		lo_message_add_float (msg, gain_to_slider_position (controllable->get_value()));
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
OSCGlobalObserver::send_trim_message (string path, boost::shared_ptr<Controllable> controllable)
{
	lo_message msg = lo_message_new ();

	lo_message_add_float (msg, (float) accurate_coefficient_to_dB (controllable->get_value()));

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}


void
OSCGlobalObserver::send_transport_state_changed()
{

	lo_message msg = lo_message_new ();
	lo_message_add_int32 (msg, session->get_play_loop());
	lo_send_message (addr, "/loop_toggle", msg);
	lo_message_free (msg);

	msg = lo_message_new ();
	lo_message_add_int32 (msg, session->transport_speed() == 1.0);
	lo_send_message (addr, "/transport_play", msg);
	lo_message_free (msg);

	msg = lo_message_new ();
	lo_message_add_int32 (msg, session->transport_stopped ());
	lo_send_message (addr, "/transport_stop", msg);
	lo_message_free (msg);

	msg = lo_message_new ();
	lo_message_add_int32 (msg, session->transport_speed() < 0.0);
	lo_send_message (addr, "/rewind", msg);
	lo_message_free (msg);

	msg = lo_message_new ();
	lo_message_add_int32 (msg, (session->transport_speed() != 1.0 && session->transport_speed() > 0.0));
	lo_send_message (addr, "/ffwd", msg);
	lo_message_free (msg);

}

void
OSCGlobalObserver::send_record_state_changed ()
{
	lo_message msg = lo_message_new ();
	lo_message_add_int32 (msg, (int)session->get_record_enabled ());
	lo_send_message (addr, "/rec_enable_toggle", msg);
	lo_message_free (msg);

	msg = lo_message_new ();
	if (session->have_rec_enabled_track ()) {
		lo_message_add_int32 (msg, 1);
	} else {
		lo_message_add_int32 (msg, 0);
	}
	lo_send_message (addr, "/record_tally", msg);
	lo_message_free (msg);

}

void
OSCGlobalObserver::send_session_saved (std::string name)
{
	lo_message msg = lo_message_new ();
	lo_message_add_string (msg, name.c_str());
	lo_send_message (addr, "/session_name", msg);
	lo_message_free (msg);

}

void
OSCGlobalObserver::solo_active (bool active)
{
	lo_message msg = lo_message_new ();
	lo_message_add_float (msg, (float) active);
	lo_send_message (addr, "/cancel_all_solos", msg);
	lo_message_free (msg);
}
