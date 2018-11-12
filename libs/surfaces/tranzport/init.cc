/*
 *   Copyright (C) 2006 Paul Davis
 *   Copyright (C) 2007 Michael Taht
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <tranzport_common.h>
#include <tranzport_control_protocol.h>

using namespace ARDOUR;
using namespace std;
using namespace sigc;
using namespace PBD;

#include "pbd/i18n.h"

#include <pbd/abstract_ui.cc>

void*
TranzportControlProtocol::_monitor_work (void* arg)
{
	return static_cast<TranzportControlProtocol*>(arg)->monitor_work ();
}

TranzportControlProtocol::~TranzportControlProtocol ()
{
	set_active (false);
}

int TranzportControlProtocol::rtpriority_set(int priority)
{
	char *a = (char*) alloca(4096*2); a[0] = 'a'; a[4096] = 'b';
	// Note - try SCHED_RR with a low limit
	// - we don't care if we can't write everything this ms
	// and it will help if we lose the device
	if (set_thread_priority (SCHED_FIFO, priority)) {
		PBD::info << string_compose (_("%1: thread not running with realtime scheduling."), name(), strerror (errno)) << endmsg;
		return 1;
	}
	return 0;
}

// Running with realtime privs is bad when you have problems

int TranzportControlProtocol::rtpriority_unset(int priority)
{
	struct sched_param rtparam;
	int err;
	memset (&rtparam, 0, sizeof (rtparam));
	rtparam.sched_priority = priority;
	if ((err = pthread_setschedparam (pthread_self(), SCHED_FIFO, &rtparam)) != 0) {
		PBD::info << string_compose (_("%1: can't stop realtime scheduling (%2)"), name(), strerror (errno)) << endmsg;
		return 1;
	}
	PBD::info << string_compose (_("%1: realtime scheduling stopped (%2)"), name(), strerror (errno)) << endmsg;
	return 0;
}


int
TranzportControlProtocol::set_active (bool yn)
{
	if (yn != _active) {

		if (yn) {

			if (open ()) {
				return -1;
			}

			if (pthread_create_and_store (X_("tranzport monitor"), &thread, _monitor_work, this) == 0) {
				_active = true;
			} else {
				return -1;
			}

		} else {
			cerr << "Begin tranzport shutdown\n";
//                      if we got here due to an error, prettifying things will only make it worse
//                      And with threads involved, oh boy...
			if(!(last_write_error || last_read_error)) {
				bling_mode   = BlingExit;
				enter_bling_mode();
// thread FIXME - wait til all writes are done
				for(int x = 0; (x < 20/MAX_TRANZPORT_INFLIGHT) && flush(); x++) { usleep(100); }
			}

			pthread_cancel_one (thread);

			cerr << "Tranzport Thread dead\n";
			close ();
			_active = false;
			cerr << "End tranzport shutdown\n";
		}
	}

	return 0;
}

TranzportControlProtocol::TranzportControlProtocol (Session& s)
	: ControlProtocol  (s, X_("Tranzport"))
{
	/* tranzport controls one track at a time */

	set_route_table_size (1);
	timeout = 6000; // what is this for?
	buttonmask = 0;
	_datawheel = 0;
	_device_status = STATUS_OFFLINE;
	udev = 0;
	current_track_id = 0;
	last_where = max_samples;
	wheel_mode = WheelTimeline;
	wheel_shift_mode = WheelShiftGain;
	wheel_increment = WheelIncrScreen;
	bling_mode = BlingEnter;
	last_notify_msg[0] = '\0';
	last_notify = 0;
	timerclear (&last_wheel_motion);
	last_wheel_dir = 1;
	last_track_gain = FLT_MAX;
	last_write_error = 0;
	last_read_error = 0;
	display_mode = DisplayBling;
	gain_fraction = 0.0;
	invalidate();
	screen_init();
	lights_init();
// FIXME: Wait til device comes online somewhere
// About 3 reads is enough
// enter_bling_mode();

}

void*
TranzportControlProtocol::monitor_work ()
{
	uint8_t buf[8]; //  = { 0,0,0,0,0,0,0,0 };
	int val = 0, pending = 0;
	bool first_time = true;
	uint8_t offline = 0;

	register_thread (X_("Tranzport"));
	pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, 0);
	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);
	rtpriority_set();
	inflight=0;
	//int intro = 20;

	// wait for the device to come online
	invalidate();
	screen_init();
	lights_init();
	update_state();
//      There has to be some specific command to enable the device!!
// 	while((val = read(buf,DEFAULT_USB_TIMEOUT*5)) == -110 && pending !=0) {
// 		pending = lights_flush(); // poke the device for a while
// 	}

// 	pending = 1;
// 	while(intro-- > 0 && pending != 0) {
// 		usleep(1000);
// 		pending = screen_flush(); // kinder, gentler init
// 	}
// 	usleep(1000);
// 	lights_on();
// 	while(flush()!=0) ;
// 	lights_off();
	display_mode = DisplayNormal;

	while (true) {

		/* bInterval for this beastie is 10ms */

		if (_device_status == STATUS_OFFLINE) {
			first_time = true; offline++;
#if TRANZPORT_DEBUG > 3
			if(offline == 1) {
				cerr << "Transport has gone offline\n";
			}
#endif
		} else {
			offline = 0; // hate writing this
		}
		unsigned int s = (last_write_error == 0) | ((last_read_error == 0) << 1);
		switch (s) {
		case 0: val = read(buf,DEFAULT_USB_TIMEOUT); break;
		case 1: val = read(buf,DEFAULT_USB_TIMEOUT); break;
		case 2: val = read(buf,DEFAULT_USB_TIMEOUT); break;
		case 3: val = read(buf,DEFAULT_USB_TIMEOUT*2); break; // Hoo, boy, we're in trouble
		default: break; // not reached
		}

#if DEBUG_TRANZPORT_BITS > 9
		if(_device_status != STATUS_OFFLINE && _device_status != STATUS_ONLINE && _device_status != STATUS_OK) {
			printf("The device has more status bits than off or online: %d\n",_device_status);
		}
#endif

#if DEBUG_TRANZPORT_BITS > 99
		if (val != 8) {
			printf("val = %d errno = %d\n",val,errno);
			buf[0] = buf[1] = buf[2] = buf[3] =
				buf[4] = buf[5] = buf[6] = buf[7] =
				buf[8] = 0;
		}
#endif

		if(val == 8) {
			last_write_error = 0;
			process (buf);
		}

#if DEBUG_TRANZPORT > 9
		if(inflight > 1) printf("Inflight: %d\n", inflight);
#endif

		if (_device_status == STATUS_ONLINE) {
			if (first_time) {
				invalidate();
				lcd_clear ();
				lights_off ();
				first_time = false;
				last_write_error = 0;
				offline = 0;
				pending = 3; // Give some time for the device to recover
			}
#if DEBUG_TRANZPORT_BITS > 10
			// Perhaps an online message indicates something

			if(_device_status != buf[1]) {
				printf("WTF- val: %d, device status != buf! %d != %d \n",val,_device_status,buf[1]); _device_status = buf[1];
			}
#endif

		}

#if DEBUG_TRANZPORT_BITS > 10

		if(val == 8) {

			if(_device_status == STATUS_ONLINE) {
				printf("ONLINE   : %02x %02x %02x %02x %02x %02x %02x %02x\n",
				       buf[0],buf[1],buf[2], buf[3], buf[4], buf[5],buf[6],buf[7]);
			}
			if(_device_status == STATUS_OFFLINE) {
				printf("OFFLINE  : %02x %02x %02x %02x %02x %02x %02x %02x\n",
				       buf[0],buf[1],buf[2], buf[3], buf[4], buf[5],buf[6],buf[7]);
			}

			if(_device_status == STATUS_OK) {
				printf("OK       : %02x %02x %02x %02x %02x %02x %02x %02x\n",
				       buf[0],buf[1],buf[2], buf[3], buf[4], buf[5],buf[6],buf[7]);
			}

		}

#endif

		/* update whatever needs updating */
		if(last_write_error == 0 && (_device_status == STATUS_ONLINE || _device_status == STATUS_OK)) {
			update_state ();

			/* still struggling with a good means of exerting flow control without having to create threads */
			// pending = flush();

			if(pending == 0) {
				pending = flush();
			} else {
				if(inflight > 0) {
					pending = --inflight; // we just did a whole bunch of writes so wait
				} else {
					pending = 0;
				}
			}
		}
		// pending = 0;
	}
	return (void*) 0;
}

