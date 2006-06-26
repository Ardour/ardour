/*
    Copyright (C) 2006 Paul Davis 

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

    $Id$
*/

#include <iostream>
#include <algorithm>
#include <cmath>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <float.h>
#include <sys/time.h>
#include <errno.h>

#include <pbd/pthread_utils.h>

#include <ardour/route.h>
#include <ardour/audio_track.h>
#include <ardour/session.h>
#include <ardour/location.h>
#include <ardour/dB.h>

#include "tranzport_control_protocol.h"

using namespace ARDOUR;
using namespace std;
using namespace sigc;
using namespace PBD;

#include "i18n.h"

#include <pbd/abstract_ui.cc>

BaseUI::RequestType LEDChange = BaseUI::new_request_type ();
BaseUI::RequestType Print = BaseUI::new_request_type ();
BaseUI::RequestType SetCurrentTrack = BaseUI::new_request_type ();

static inline double 
gain_to_slider_position (ARDOUR::gain_t g)
{
	if (g == 0) return 0;
	return pow((6.0*log(g)/log(2.0)+192.0)/198.0, 8.0);

}

static inline ARDOUR::gain_t 
slider_position_to_gain (double pos)
{
	/* XXX Marcus writes: this doesn't seem right to me. but i don't have a better answer ... */
	if (pos == 0.0) return 0;
	return pow (2.0,(sqrt(sqrt(sqrt(pos)))*198.0-192.0)/6.0);
}


TranzportControlProtocol::TranzportControlProtocol (Session& s)
	: ControlProtocol  (s, X_("Tranzport"))
{
	/* tranzport controls one track at a time */

	set_route_table_size (1);
	
	timeout = 60000;
	buttonmask = 0;
	_datawheel = 0;
	_device_status = STATUS_OFFLINE;
	udev = 0;
	current_track_id = 0;
	last_where = max_frames;
	wheel_mode = WheelTimeline;
	wheel_shift_mode = WheelShiftGain;
	timerclear (&last_wheel_motion);
	last_wheel_dir = 1;
	last_track_gain = FLT_MAX;
	display_mode = DisplayNormal;
	gain_fraction = 0.0;

	memset (current_screen, 0, sizeof (current_screen));
	memset (pending_screen, 0, sizeof (pending_screen));

	for (uint32_t i = 0; i < sizeof(lights)/sizeof(lights[0]); ++i) {
		lights[i] = false;
	}

	for (uint32_t i = 0; i < sizeof(pending_lights)/sizeof(pending_lights[0]); ++i) {
		pending_lights[i] = false;
	}
}

TranzportControlProtocol::~TranzportControlProtocol ()
{
	set_active (false);
}

bool
TranzportControlProtocol::probe ()
{
	struct usb_bus *bus;
	struct usb_device *dev;

	usb_init();
	usb_find_busses();
	usb_find_devices();

	for (bus = usb_busses; bus; bus = bus->next) {

		for(dev = bus->devices; dev; dev = dev->next) {
			if (dev->descriptor.idVendor == VENDORID && dev->descriptor.idProduct == PRODUCTID) {
				return true; 
			}
		}
	}

	return false;
}

int
TranzportControlProtocol::set_active (bool yn)
{
	if (yn != _active) {

		if (yn) {

			if (open ()) {
				return -1;
			}
			
			if (pthread_create_and_store (X_("tranzport monitor"), &thread, 0, _monitor_work, this) == 0) {
				_active = true;
			} else {
				return -1;
			}

		} else {
			cerr << "Begin tranzport shutdown\n";
			pthread_cancel_one (thread);
			cerr << "Thread dead\n";
			// lcd_clear ();
			// lights_off ();
			// cerr << "dev reset\n";
			close ();
			_active = false;
			cerr << "End tranzport shutdown\n";
		} 
	}

	return 0;
}

void
TranzportControlProtocol::show_track_gain ()
{
	if (route_table[0]) {
		gain_t g = route_get_gain (0);
		if (g != last_track_gain) {
			char buf[16];
			snprintf (buf, sizeof (buf), "%6.1fdB", coefficient_to_dB (route_get_effective_gain (0)));
			print (0, 9, buf); 
			last_track_gain = g;
		}
	} else {
		print (0, 9, "        "); 
	}
}

void
TranzportControlProtocol::normal_update ()
{
	show_current_track ();
	show_transport_time ();
	show_track_gain ();
	show_wheel_mode ();
}

void
TranzportControlProtocol::next_display_mode ()
{
	switch (display_mode) {
	case DisplayNormal:
		display_mode = DisplayBigMeter;
		break;

	case DisplayBigMeter:
		display_mode = DisplayNormal;
		break;
	}
}

void
TranzportControlProtocol::enter_big_meter_mode ()
{
	lcd_clear ();
	lights_off ();
	last_meter_fill = 0;
	display_mode = DisplayBigMeter;
}

void
TranzportControlProtocol::enter_normal_display_mode ()
{
	last_where += 1; /* force time redisplay */
	last_track_gain = FLT_MAX; /* force gain redisplay */

	lcd_clear ();
	lights_off ();
	show_current_track ();
	show_wheel_mode ();
	show_wheel_mode ();
	show_transport_time ();
	display_mode = DisplayNormal;
}


float
log_meter (float db)
{
	float def = 0.0f; /* Meter deflection %age */
 
	if (db < -70.0f) {
		def = 0.0f;
	} else if (db < -60.0f) {
		def = (db + 70.0f) * 0.25f;
	} else if (db < -50.0f) {
		def = (db + 60.0f) * 0.5f + 2.5f;
	} else if (db < -40.0f) {
		def = (db + 50.0f) * 0.75f + 7.5f;
	} else if (db < -30.0f) {
		def = (db + 40.0f) * 1.5f + 15.0f;
	} else if (db < -20.0f) {
		def = (db + 30.0f) * 2.0f + 30.0f;
	} else if (db < 6.0f) {
		def = (db + 20.0f) * 2.5f + 50.0f;
	} else {
		def = 115.0f;
	}
	
	/* 115 is the deflection %age that would be 
	   when db=6.0. this is an arbitrary
	   endpoint for our scaling.
	*/
	
	return def/115.0f;
}

void
TranzportControlProtocol::show_meter ()
{
	if (route_table[0] == 0) {
		return;
	}

	float level = route_get_peak_input_power (0, 0);
	float fraction = log_meter (level);

	/* we draw using a choice of a sort of double colon-like character ("::") or a single, left-aligned ":".
	   the screen is 20 chars wide, so we can display 40 different levels. compute the level,
	   then figure out how many "::" to fill. if the answer is odd, make the last one a ":"
	*/

	uint32_t fill  = (uint32_t) floor (fraction * 40);
	char buf[21];
	uint32_t i;

	if (fill == last_meter_fill) {
		/* nothing to do */
		return;
	}

	last_meter_fill = fill;

	bool add_single_level = (fill % 2 != 0);
	fill /= 2;
	
	if (fraction > 0.98) {
		light_on (LightAnysolo);
	}

	/* add all full steps */

	for (i = 0; i < fill; ++i) {
		buf[i] = 0x07; /* tranzport special code for 4 quadrant LCD block */
	} 

	/* add a possible half-step */

	if (i < 20 && add_single_level) {
		buf[i] = 0x03; /* tranzport special code for 2 left quadrant LCD block */
		++i;
	}

	/* fill rest with space */

	for (; i < 20; ++i) {
		buf[i] = ' ';
	}

	/* print() requires this */
	
	buf[21] = '\0';

	print (0, 0, buf);
	print (1, 0, buf);
}

void
TranzportControlProtocol::show_transport_time ()
{
	jack_nframes_t where = session->transport_frame();
	
	if (where != last_where) {

		char buf[5];
		SMPTE::Time smpte;

		session->smpte_time (where, smpte);
		
		if (smpte.negative) {
			sprintf (buf, "-%02" PRIu32 ":", smpte.hours);
		} else {
			sprintf (buf, " %02" PRIu32 ":", smpte.hours);
		}
		print (1, 8, buf);

		sprintf (buf, "%02" PRIu32 ":", smpte.minutes);
		print (1, 12, buf);

		sprintf (buf, "%02" PRIu32 ":", smpte.seconds);
		print (1, 15, buf);

		sprintf (buf, "%02" PRIu32, smpte.frames);
		print (1, 18, buf);

		last_where = where;
	}
}

void*
TranzportControlProtocol::_monitor_work (void* arg)
{
	return static_cast<TranzportControlProtocol*>(arg)->monitor_work ();
}

int
TranzportControlProtocol::open ()
{
	struct usb_bus *bus;
	struct usb_device *dev;

	usb_init();
	usb_find_busses();
	usb_find_devices();

	for (bus = usb_busses; bus; bus = bus->next) {

		for(dev = bus->devices; dev; dev = dev->next) {
			if (dev->descriptor.idVendor != VENDORID)
				continue;
			if (dev->descriptor.idProduct != PRODUCTID)
				continue;
			return open_core (dev);
		}
	}

	error << _("Tranzport: no device detected") << endmsg;
	return -1;
}

int
TranzportControlProtocol::open_core (struct usb_device* dev)
{
	if (!(udev = usb_open (dev))) {
		error << _("Tranzport: cannot open USB transport") << endmsg;
		return -1;
	}
	 
	if (usb_claim_interface (udev, 0) < 0) {
		error << _("Tranzport: cannot claim USB interface") << endmsg;
		usb_close (udev);
		udev = 0;
		return -1;
	}

	if (usb_set_configuration (udev, 1) < 0) {
		cerr << _("Tranzport: cannot configure USB interface") << endmsg;
	}

	return 0;
}

int
TranzportControlProtocol::close ()
{
	int ret = 0;

	if (udev == 0) {
		return 0;
	}

	if (usb_release_interface (udev, 0) < 0) {
		error << _("Tranzport: cannot release interface") << endmsg;
		ret = -1;
	}

	if (usb_close (udev)) {
		error << _("Tranzport: cannot close device") << endmsg;
		udev = 0;
		ret = 0;
	}

	return ret;
}
	
int
TranzportControlProtocol::write (uint8_t* cmd, uint32_t timeout_override)
{
	int val;

	val = usb_interrupt_write (udev, WRITE_ENDPOINT, (char*) cmd, 8, timeout_override ? timeout_override : timeout);

	if (val < 0)
		return val;
	if (val != 8)
		return -1;
	return 0;

}	

void
TranzportControlProtocol::lcd_clear ()
{
	/* special case this for speed and atomicity */

	uint8_t cmd[8];
	
	cmd[0] = 0x00;
	cmd[1] = 0x01;
	cmd[3] = ' ';
	cmd[4] = ' ';
	cmd[5] = ' ';
	cmd[6] = ' ';
	cmd[7] = 0x00;

	for (uint8_t i = 0; i < 10; ++i) {
		cmd[2] = i;
		usb_interrupt_write (udev, WRITE_ENDPOINT, (char*) cmd, 8, 1000);
	}
	
	memset (current_screen, ' ', sizeof (current_screen));
	memset (pending_screen, ' ', sizeof (pending_screen));
}

void
TranzportControlProtocol::lights_off ()
{
	uint8_t cmd[8];

	cmd[0] = 0x00;
	cmd[1] = 0x00;
	cmd[3] = 0x00;
	cmd[4] = 0x00;
	cmd[5] = 0x00;
	cmd[6] = 0x00;
	cmd[7] = 0x00;

	cmd[2] = LightRecord;
	if (write (cmd, 1000) == 0) {
		lights[LightRecord] = false;
	}
	cmd[2] = LightTrackrec;
	if (write (cmd, 1000) == 0) {
		lights[LightTrackrec] = false;
	}
	cmd[2] = LightTrackmute;
	if (write (cmd, 1000) == 0) {
		lights[LightTrackmute] = false;
	}
	cmd[2] = LightTracksolo;
	if (write (cmd, 1000) == 0) {
		lights[LightTracksolo] = false;
	}
	cmd[2] = LightAnysolo;
	if (write (cmd, 1000) == 0) {
		lights[LightAnysolo] = false;
	}
	cmd[2] = LightLoop;
	if (write (cmd, 1000) == 0) {
		lights[LightLoop] = false;
	}
	cmd[2] = LightPunch;
	if (write (cmd, 1000) == 0) {
		lights[LightPunch] = false;
	}
}

int
TranzportControlProtocol::light_on (LightID light)
{
	uint8_t cmd[8];

	if (!lights[light]) {

		cmd[0] = 0x00;
		cmd[1] = 0x00;
		cmd[2] = light;
		cmd[3] = 0x01;
		cmd[4] = 0x00;
		cmd[5] = 0x00;
		cmd[6] = 0x00;
		cmd[7] = 0x00;

		if (write (cmd, 1000) == 0) {
			lights[light] = true;
			return 0;
		} else {
			return -1;
		}

	} else {
		return 0;
	}
}

int
TranzportControlProtocol::light_off (LightID light)
{
	uint8_t cmd[8];

	if (lights[light]) {

		cmd[0] = 0x00;
		cmd[1] = 0x00;
		cmd[2] = light;
		cmd[3] = 0x00;
		cmd[4] = 0x00;
		cmd[5] = 0x00;
		cmd[6] = 0x00;
		cmd[7] = 0x00;

		if (write (cmd, 1000) == 0) {
			lights[light] = false;
			return 0;
		} else {
			return -1;
		}

	} else {
		return 0;
	}
}

void*
TranzportControlProtocol::monitor_work ()
{
	struct sched_param rtparam;
	int err;
	uint8_t buf[8];
	int val;
	bool first_time = true;

	PBD::ThreadCreated (pthread_self(), X_("Tranzport"));

	memset (&rtparam, 0, sizeof (rtparam));
	rtparam.sched_priority = 3; /* XXX should be relative to audio (JACK) thread */
	
	if ((err = pthread_setschedparam (pthread_self(), SCHED_FIFO, &rtparam)) != 0) {
		// do we care? not particularly.
		info << string_compose (_("%1: thread not running with realtime scheduling (%2)"), name(), strerror (errno)) << endmsg;
	} 

	pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, 0);
	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	next_track ();

	while (true) {

		/* bInterval for this beastie is 10ms */

		/* anything to read ? */

		if (_device_status == STATUS_OFFLINE) {
			light_off (LightRecord);
			first_time = true;
		}

		pthread_testcancel();
		val = usb_interrupt_read (udev, READ_ENDPOINT, (char*) buf, 8, 10);
		pthread_testcancel();

		if (val == 8) {
			process (buf);
		}

		if (_device_status != STATUS_OFFLINE) {
			if (first_time) {
				lcd_clear ();
				lights_off ();
				first_time = false;
			}
			/* update whatever needs updating */
			update_state ();
		}
	}

	return (void*) 0;
}

int
TranzportControlProtocol::update_state ()
{
	int row;
	int col_base;
	int col;
	int cell;

	/* do the text updates */

	switch (display_mode) {
	case DisplayBigMeter:
		show_meter ();
		break;

	case DisplayNormal:
		normal_update ();
		break;
	}

	/* next: flush LCD */

	cell = 0;
	
	for (row = 0; row < 2; ++row) {

		for (col_base = 0, col = 0; col < 20; ) {
			
			if (pending_screen[row][col] != current_screen[row][col]) {

				/* something in this cell is different, so dump the cell
				   to the device.
				*/

				uint8_t cmd[8];
				
				cmd[0] = 0x00;
				cmd[1] = 0x01;
				cmd[2] = cell;
				cmd[3] = pending_screen[row][col_base];
				cmd[4] = pending_screen[row][col_base+1];
				cmd[5] = pending_screen[row][col_base+2];
				cmd[6] = pending_screen[row][col_base+3];
				cmd[7] = 0x00;

				if (usb_interrupt_write (udev, WRITE_ENDPOINT, (char *) cmd, 8, 1000) == 8) {
					/* successful write: copy to current */
					memcpy (&current_screen[row][col_base], &pending_screen[row][col_base], 4);
				}

				/* skip the rest of the 4 character cell since we wrote+copied it already */
				
				col_base += 4;
				col = col_base;
				cell++;

			} else {

				col++;
				
				if (col && col % 4 == 0) {
					cell++;
					col_base += 4;
				}
			}
		}
	}

	/* now update LED's */

	/* per track */

	if (route_table[0]) {
		AudioTrack* at = dynamic_cast<AudioTrack*> (route_table[0]);
		if (at && at->record_enabled()) {
			pending_lights[LightTrackrec] = true;
		} else {
			pending_lights[LightTrackrec] = false;
		}
		if (route_get_muted (0)) {
			pending_lights[LightTrackmute] = true;
		} else {
			pending_lights[LightTrackmute] = false;
		}
		if (route_get_soloed (0)) {
			pending_lights[LightTracksolo] = true;
		} else {
			pending_lights[LightTracksolo] = false;
		}

	} else {
		pending_lights[LightTrackrec] = false;
		pending_lights[LightTracksolo] = false;
		pending_lights[LightTrackmute] = false;
	}

	/* global */

	if (session->get_auto_loop()) {
		pending_lights[LightLoop] = true;
	} else {
		pending_lights[LightLoop] = false;
	}

	if (session->get_punch_in() || session->get_punch_out()) {
		pending_lights[LightPunch] = true;
	} else {
		pending_lights[LightPunch] = false;
	}

	if (session->get_record_enabled()) {
		pending_lights[LightRecord] = true;
	} else {
		pending_lights[LightRecord] = false;
	}

	if (session->soloing ()) {
		pending_lights[LightAnysolo] = true;
	} else {
		pending_lights[LightAnysolo] = false;
	}

	/* flush changed light change */

	if (pending_lights[LightRecord] != lights[LightRecord]) {
		if (pending_lights[LightRecord]) {
			light_on (LightRecord);
		} else {
			light_off (LightRecord);
		}
	}

	if (pending_lights[LightTracksolo] != lights[LightTracksolo]) {
		if (pending_lights[LightTracksolo]) {
			light_on (LightTracksolo);
		} else {
			light_off (LightTracksolo);
		}
	}

	if (pending_lights[LightTrackmute] != lights[LightTrackmute]) {
		if (pending_lights[LightTrackmute]) {
			light_on (LightTrackmute);
		} else {
			light_off (LightTrackmute);
		}
	}

	if (pending_lights[LightTracksolo] != lights[LightTracksolo]) {
		if (pending_lights[LightTracksolo]) {
			light_on (LightTracksolo);
		} else {
			light_off (LightTracksolo);
		}
	}

	if (pending_lights[LightAnysolo] != lights[LightAnysolo]) {
		if (pending_lights[LightAnysolo]) {
			light_on (LightAnysolo);
		} else {
			light_off (LightAnysolo);
		}
	}

	if (pending_lights[LightLoop] != lights[LightLoop]) {
		if (pending_lights[LightLoop]) {
			light_on (LightLoop);
		} else {
			light_off (LightLoop);
		}
	}

	if (pending_lights[LightPunch] != lights[LightPunch]) {
		if (pending_lights[LightPunch]) {
			light_on (LightPunch);
		} else {
			light_off (LightPunch);
		}
	}

	return 0;
}

int
TranzportControlProtocol::process (uint8_t* buf)
{
	// printf("read: %02x %02x %02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);

	uint32_t this_button_mask;
	uint32_t button_changes;

	_device_status = buf[1];
	this_button_mask = 0;
	this_button_mask |= buf[2] << 24;
	this_button_mask |= buf[3] << 16;
	this_button_mask |= buf[4] << 8;
	this_button_mask |= buf[5];
	_datawheel = buf[6];

	button_changes = (this_button_mask ^ buttonmask);
	buttonmask = this_button_mask;

	if (_datawheel) {
		datawheel ();
	}

	if (button_changes & ButtonBattery) {
		if (buttonmask & ButtonBattery) {
			button_event_battery_press (buttonmask&ButtonShift);
		} else {
			button_event_battery_release (buttonmask&ButtonShift);
		}
	}
	if (button_changes & ButtonBacklight) {
		if (buttonmask & ButtonBacklight) {
			button_event_backlight_press (buttonmask&ButtonShift);
		} else {
			button_event_backlight_release (buttonmask&ButtonShift);
		}
	}
	if (button_changes & ButtonTrackLeft) {
		if (buttonmask & ButtonTrackLeft) {
			button_event_trackleft_press (buttonmask&ButtonShift);
		} else {
			button_event_trackleft_release (buttonmask&ButtonShift);
		}
	}
	if (button_changes & ButtonTrackRight) {
		if (buttonmask & ButtonTrackRight) {
			button_event_trackright_press (buttonmask&ButtonShift);
		} else {
			button_event_trackright_release (buttonmask&ButtonShift);
		}
	}
	if (button_changes & ButtonTrackRec) {
		if (buttonmask & ButtonTrackRec) {
			button_event_trackrec_press (buttonmask&ButtonShift);
		} else {
			button_event_trackrec_release (buttonmask&ButtonShift);
		}
	}
	if (button_changes & ButtonTrackMute) {
		if (buttonmask & ButtonTrackMute) {
			button_event_trackmute_press (buttonmask&ButtonShift);
		} else {
			button_event_trackmute_release (buttonmask&ButtonShift);
		}
	}
	if (button_changes & ButtonTrackSolo) {
		if (buttonmask & ButtonTrackSolo) {
			button_event_tracksolo_press (buttonmask&ButtonShift);
		} else {
			button_event_tracksolo_release (buttonmask&ButtonShift);
		}
	}
	if (button_changes & ButtonUndo) {
		if (buttonmask & ButtonUndo) {
			button_event_undo_press (buttonmask&ButtonShift);
		} else {
			button_event_undo_release (buttonmask&ButtonShift);
		}
	}
	if (button_changes & ButtonIn) {
		if (buttonmask & ButtonIn) {
			button_event_in_press (buttonmask&ButtonShift);
		} else {
			button_event_in_release (buttonmask&ButtonShift);
		}
	}
	if (button_changes & ButtonOut) {
		if (buttonmask & ButtonOut) {
			button_event_out_press (buttonmask&ButtonShift);
		} else {
			button_event_out_release (buttonmask&ButtonShift);
		}
	}
	if (button_changes & ButtonPunch) {
		if (buttonmask & ButtonPunch) {
			button_event_punch_press (buttonmask&ButtonShift);
		} else {
			button_event_punch_release (buttonmask&ButtonShift);
		}
	}
	if (button_changes & ButtonLoop) {
		if (buttonmask & ButtonLoop) {
			button_event_loop_press (buttonmask&ButtonShift);
		} else {
			button_event_loop_release (buttonmask&ButtonShift);
		}
	}
	if (button_changes & ButtonPrev) {
		if (buttonmask & ButtonPrev) {
			button_event_prev_press (buttonmask&ButtonShift);
		} else {
			button_event_prev_release (buttonmask&ButtonShift);
		}
	}
	if (button_changes & ButtonAdd) {
		if (buttonmask & ButtonAdd) {
			button_event_add_press (buttonmask&ButtonShift);
		} else {
			button_event_add_release (buttonmask&ButtonShift);
		}
	}
	if (button_changes & ButtonNext) {
		if (buttonmask & ButtonNext) {
			button_event_next_press (buttonmask&ButtonShift);
		} else {
			button_event_next_release (buttonmask&ButtonShift);
		}
	}
	if (button_changes & ButtonRewind) {
		if (buttonmask & ButtonRewind) {
			button_event_rewind_press (buttonmask&ButtonShift);
		} else {
			button_event_rewind_release (buttonmask&ButtonShift);
		}
	}
	if (button_changes & ButtonFastForward) {
		if (buttonmask & ButtonFastForward) {
			button_event_fastforward_press (buttonmask&ButtonShift);
		} else {
			button_event_fastforward_release (buttonmask&ButtonShift);
		}
	}
	if (button_changes & ButtonStop) {
		if (buttonmask & ButtonStop) {
			button_event_stop_press (buttonmask&ButtonShift);
		} else {
			button_event_stop_release (buttonmask&ButtonShift);
		}
	}
	if (button_changes & ButtonPlay) {
		if (buttonmask & ButtonPlay) {
			button_event_play_press (buttonmask&ButtonShift);
		} else {
			button_event_play_release (buttonmask&ButtonShift);
		}
	}
	if (button_changes & ButtonRecord) {
		if (buttonmask & ButtonRecord) {
			button_event_record_press (buttonmask&ButtonShift);
		} else {
			button_event_record_release (buttonmask&ButtonShift);
		}
	}
		
	return 0;
}

void
TranzportControlProtocol::show_current_track ()
{
	if (route_table[0] == 0) {
		print (0, 0, "--------");
	} else {
		print (0, 0, route_get_name (0).substr (0, 8).c_str());
	}
}

void
TranzportControlProtocol::button_event_battery_press (bool shifted)
{
}

void
TranzportControlProtocol::button_event_battery_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_backlight_press (bool shifted)
{
}

void
TranzportControlProtocol::button_event_backlight_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_trackleft_press (bool shifted)
{
	prev_track ();
}

void
TranzportControlProtocol::button_event_trackleft_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_trackright_press (bool shifted)
{
	next_track ();
}

void
TranzportControlProtocol::button_event_trackright_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_trackrec_press (bool shifted)
{
	if (shifted) {
		toggle_all_rec_enables ();
	} else {
		route_set_rec_enable (0, !route_get_rec_enable (0));
	}
}

void
TranzportControlProtocol::button_event_trackrec_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_trackmute_press (bool shifted)
{
	route_set_muted (0, !route_get_muted (0));
}

void
TranzportControlProtocol::button_event_trackmute_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_tracksolo_press (bool shifted)
{
	if (display_mode == DisplayBigMeter) {
		light_off (LightAnysolo);
		return;
	}

	if (shifted) {
		session->set_all_solo (!session->soloing());
	} else {
		route_set_soloed (0, !route_get_soloed (0));
	}
}

void
TranzportControlProtocol::button_event_tracksolo_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_undo_press (bool shifted)
{
	if (shifted) {
		redo ();
	} else {
		undo ();
	}
}

void
TranzportControlProtocol::button_event_undo_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_in_press (bool shifted)
{
	if (shifted) {
		toggle_punch_in ();
	} else {
		ControlProtocol::ZoomIn (); /* EMIT SIGNAL */
	}
}

void
TranzportControlProtocol::button_event_in_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_out_press (bool shifted)
{
	if (shifted) {
		toggle_punch_out ();
	} else {
		ControlProtocol::ZoomOut (); /* EMIT SIGNAL */
	}
}

void
TranzportControlProtocol::button_event_out_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_punch_press (bool shifted)
{
}

void
TranzportControlProtocol::button_event_punch_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_loop_press (bool shifted)
{
	if (shifted) {
		next_wheel_shift_mode ();
	} else {
		loop_toggle ();
	}
}

void
TranzportControlProtocol::button_event_loop_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_prev_press (bool shifted)
{
	if (shifted) {
		ControlProtocol::ZoomToSession (); /* EMIT SIGNAL */
	} else {
		prev_marker ();
	}
}

void
TranzportControlProtocol::button_event_prev_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_add_press (bool shifted)
{
	add_marker ();
}

void
TranzportControlProtocol::button_event_add_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_next_press (bool shifted)
{
	if (shifted) {
		next_wheel_mode ();
	} else {
		next_marker ();
	}
}

void
TranzportControlProtocol::button_event_next_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_rewind_press (bool shifted)
{
	if (shifted) {
		goto_start ();
	} else {
		rewind ();
	}
}

void
TranzportControlProtocol::button_event_rewind_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_fastforward_press (bool shifted)
{
	if (shifted) {
		goto_end ();
	} else {
		ffwd ();
	}
}

void
TranzportControlProtocol::button_event_fastforward_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_stop_press (bool shifted)
{
	if (shifted) {
		next_display_mode ();
	} else {
		transport_stop ();
	}
}

void
TranzportControlProtocol::button_event_stop_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_play_press (bool shifted)
{
	transport_play ();
}

void
TranzportControlProtocol::button_event_play_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_record_press (bool shifted)
{
	if (shifted) {
		save_state ();
	} else {
		rec_enable_toggle ();
	}
}

void
TranzportControlProtocol::button_event_record_release (bool shifted)
{
}

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

		timerclear (&last_wheel_motion);

	} else if ((buttonmask & ButtonPrev) || (buttonmask & ButtonNext)) {
		
		if (_datawheel < WheelDirectionThreshold) {
			next_marker ();
		} else {
			prev_marker ();
		}

		timerclear (&last_wheel_motion);

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

			case WheelShiftMaster:
				break;
			}
		}

		timerclear (&last_wheel_motion);

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
	if (_datawheel < WheelDirectionThreshold) {
		ScrollTimeline (0.2);
	} else {
		ScrollTimeline (-0.2);
	}
}

void
TranzportControlProtocol::scrub ()
{
	float speed;
	struct timeval now;
	struct timeval delta;
	int dir;
	
	gettimeofday (&now, 0);
	
	if (_datawheel < WheelDirectionThreshold) {
		dir = 1;
	} else {
		dir = -1;
	}
	
	if (dir != last_wheel_dir) {
		/* changed direction, start over */
		speed = 0.1f;
	} else {
		if (timerisset (&last_wheel_motion)) {
			
			timersub (&now, &last_wheel_motion, &delta);
			
			/* 10 clicks per second => speed == 1.0 */
			
			speed = 100000.0f / (delta.tv_sec * 1000000 + delta.tv_usec);
			
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
		if (session->transport_speed() < 0) {
			session->request_transport_speed (1.0);
		} else {
			session->request_transport_speed (session->transport_speed() + 0.1);
		}
	} else {
		if (session->transport_speed() > 0) {
			session->request_transport_speed (-1.0);
		} else {
			session->request_transport_speed (session->transport_speed() - 0.1);
		}
	}
}

void
TranzportControlProtocol::step_gain_up ()
{
	if (buttonmask & ButtonStop) {
		gain_fraction += 0.001;
	} else {
		gain_fraction += 0.01;
	}

	if (gain_fraction > 2.0) {
		gain_fraction = 2.0;
	}
	
	route_set_gain (0, slider_position_to_gain (gain_fraction));
}

void
TranzportControlProtocol::step_gain_down ()
{
	if (buttonmask & ButtonStop) {
		gain_fraction -= 0.001;
	} else {
		gain_fraction -= 0.01;
	}

	if (gain_fraction < 0.0) {
		gain_fraction = 0.0;
	}
	
	route_set_gain (0, slider_position_to_gain (gain_fraction));
}

void
TranzportControlProtocol::step_pan_right ()
{
}

void
TranzportControlProtocol::step_pan_left ()
{
}

void
TranzportControlProtocol::next_wheel_shift_mode ()
{
	switch (wheel_shift_mode) {
	case WheelShiftGain:
		wheel_shift_mode = WheelShiftPan;
		break;
	case WheelShiftPan:
		wheel_shift_mode = WheelShiftMaster;
		break;
	case WheelShiftMaster:
		wheel_shift_mode = WheelShiftGain;
	}

	show_wheel_mode ();
}

void
TranzportControlProtocol::next_wheel_mode ()
{
	switch (wheel_mode) {
	case WheelTimeline:
		wheel_mode = WheelScrub;
		break;
	case WheelScrub:
		wheel_mode = WheelShuttle;
		break;
	case WheelShuttle:
		wheel_mode = WheelTimeline;
	}

	show_wheel_mode ();
}

void
TranzportControlProtocol::next_track ()
{
	ControlProtocol::next_track (current_track_id);
	gain_fraction = gain_to_slider_position (route_get_effective_gain (0));
}

void
TranzportControlProtocol::prev_track ()
{
	ControlProtocol::prev_track (current_track_id);
	gain_fraction = gain_to_slider_position (route_get_effective_gain (0));
}

void
TranzportControlProtocol::show_wheel_mode ()
{
	string text;

	switch (wheel_mode) {
	case WheelTimeline:
		text = "Time";
		break;
	case WheelScrub:
		text = "Scrb";
		break;
	case WheelShuttle:
		text = "Shtl";
		break;
	}

	switch (wheel_shift_mode) {
	case WheelShiftGain:
		text += ":Gain";
		break;

	case WheelShiftPan:
		text += ":Pan";
		break;

	case WheelShiftMaster:
		text += ":Mstr";
		break;
	}
	
	print (1, 0, text.c_str());
}

void
TranzportControlProtocol::print (int row, int col, const char *text)
{
	int cell;
	uint32_t left = strlen (text);
	char tmp[5];
	int base_col;
	
	if (row < 0 || row > 1) {
		return;
	}

	if (col < 0 || col > 19) {
		return;
	}

	while (left) {

		if (col >= 0 && col < 4) {
			cell = 0;
			base_col = 0;
		} else if (col >= 4 && col < 8) {
			cell = 1;
			base_col = 4;
		} else if (col >= 8 && col < 12) {
			cell = 2;
			base_col = 8;
		} else if (col >= 12 && col < 16) {
			cell = 3;
			base_col = 12;
		} else if (col >= 16 && col < 20) {
			cell = 4;
			base_col = 16;
		} else {
			return;
		}

		int offset = col % 4;

		/* copy current cell contents into tmp */
		
		memcpy (tmp, &pending_screen[row][base_col], 4);
		
		/* overwrite with new text */
		
		uint32_t tocopy = min ((4U - offset), left);
		
		memcpy (tmp+offset, text, tocopy);
		
		/* copy it back to pending */
		
		memcpy (&pending_screen[row][base_col], tmp, 4);
		
		text += tocopy;
		left -= tocopy;
		col  += tocopy;
	}
}	

