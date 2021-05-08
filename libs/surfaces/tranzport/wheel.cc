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
 *   */

#include <iostream>
#include <algorithm>
#include <cmath>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <float.h>
#include <sys/time.h>
#include <errno.h>
#include "ardour/route.h"
#include "ardour/audio_track.h"
#include "ardour/session.h"
#include "ardour/location.h"
#include "ardour/dB.h"

using namespace ARDOUR;
using namespace std;
using namespace sigc;
using namespace PBD;

#include "pbd/i18n.h"

#include "pbd/abstract_ui.cc"

BaseUI::RequestType LEDChange = BaseUI::new_request_type ();
BaseUI::RequestType Print = BaseUI::new_request_type ();
BaseUI::RequestType SetCurrentTrack = BaseUI::new_request_type ();


#include "tranzport_control_protocol.h"

void
TranzportControlProtocol::datawheel ()
{
	if ((buttonmask & ButtonTrackRight) || (buttonmask & ButtonTrackLeft)) {

		/* track scrolling */

		if (_datawheel < WheelDirectionThreshold) {
			next_track ();
		} else {
			prev_track ();
		}

		last_wheel_motion = 0;

	} else if ((buttonmask & ButtonPrev) || (buttonmask & ButtonNext)) {

		if (_datawheel < WheelDirectionThreshold) {
			next_marker ();
		} else {
			prev_marker ();
		}

		last_wheel_motion = 0;

	} else if (buttonmask & ButtonShift) {

		/* parameter control */

		if (route_table[0]) {
			switch (wheel_shift_mode) {
			case WheelShiftGain:
				if (_datawheel < WheelDirectionThreshold) {
					step_gain_up ();
				} else {
					step_gain_down ();
				}
				break;
			case WheelShiftPan:
				if (_datawheel < WheelDirectionThreshold) {
					step_pan_right ();
				} else {
					step_pan_left ();
				}
				break;

			case WheelShiftMarker:
				break;

			case WheelShiftMaster:
				break;

			}
		}

		last_wheel_motion = 0;

	} else {

		switch (wheel_mode) {
		case WheelTimeline:
			scroll ();
			break;

		case WheelScrub:
			scrub ();
			break;

		case WheelShuttle:
			shuttle ();
			break;
		}
	}
}

void
TranzportControlProtocol::scroll ()
{
	float m = 1.0;
	if (_datawheel < WheelDirectionThreshold) {
		m = 1.0;
	} else {
		m = -1.0;
	}
	switch(wheel_increment) {
	case WheelIncrScreen: ScrollTimeline (0.2*m); break;
	case WheelIncrSlave:
	case WheelIncrSample:
	case WheelIncrBeat:
	case WheelIncrBar:
	case WheelIncrSecond:
	case WheelIncrMinute:
	default: break; // other modes unimplemented as yet
	}
}

void
TranzportControlProtocol::scrub ()
{
	float speed;
	uint64_t now;
	int dir;

	now = g_get_monotonic_time();

	if (_datawheel < WheelDirectionThreshold) {
		dir = 1;
	} else {
		dir = -1;
	}

	if (dir != last_wheel_dir) {
		/* changed direction, start over */
		speed = 0.1f;
	} else {
		if (last_wheel_motion != 0) {
			/* 10 clicks per second => speed == 1.0 */

			speed = 100000.0f / (float) (now - last_wheel_motion)

		} else {

			/* start at half-speed and see where we go from there */

			speed = 0.5f;
		}
	}

	last_wheel_motion = now;
	last_wheel_dir = dir;

	set_transport_speed (speed * dir);
}

void
TranzportControlProtocol::shuttle ()
{
	if (_datawheel < WheelDirectionThreshold) {
		if (get_transport_speed() < 0) {
			session->request_transport_speed (1.0);
		} else {
			session->request_transport_speed_nonzero (get_transport_speed() + 0.1);
		}
	} else {
		if (session->get_transport_speed() > 0) {
			session->request_transport_speed (-1.0);
		} else {
			session->request_transport_speed_nonzero (get_transport_speed() - 0.1);
		}
	}
	session->request_roll ();
}
