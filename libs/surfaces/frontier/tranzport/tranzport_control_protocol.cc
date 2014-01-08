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

*/

/* Design notes: The tranzport is a unique device, basically a 
   20 lcd gui with 22 shift keys and 8 blinking lights. 

   As such it has several unique constraints. The device exerts flow control
   by having a usb write fail. It is pointless to retry madly at that point,
   the device is busy, and it's not going to become unbusy very quickly. 

   So writes need to be either "mandatory" or "unreliable", and therein 
   lies the rub, as the kernel can also drop writes, and missing an
   interrupt in userspace is also generally bad.

   It will be good one day, to break the gui, keyboard, and blinking light
   components into separate parts, but for now, this remains monolithic.

   A more complex surface might have hundreds of lights and several displays.

   mike.taht@gmail.com
 */

#define DEFAULT_USB_TIMEOUT 10
#define MAX_RETRY 1
#define MAX_TRANZPORT_INFLIGHT 4
#define DEBUG_TRANZPORT 0
#define HAVE_TRANZPORT_KERNEL_DRIVER 0

#include <iostream>
#include <algorithm>
#include <cmath>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <float.h>
#include <sys/time.h>
#include <errno.h>

#include "pbd/pthread_utils.h"

#include "ardour/route.h"
#include "ardour/audio_track.h"
#include "ardour/tempo.h"
#include "ardour/location.h"
#include "ardour/dB.h"

#include "tranzport_control_protocol.h"

using namespace ARDOUR;
using namespace std;
using namespace sigc;
using namespace PBD;

#include "i18n.h"

#include "pbd/abstract_ui.cc"

BaseUI::RequestType LEDChange = BaseUI::new_request_type ();
BaseUI::RequestType Print = BaseUI::new_request_type ();
BaseUI::RequestType SetCurrentTrack = BaseUI::new_request_type ();

/* Base Tranzport cmd strings */

static const uint8_t cmd_light_on[] =  { 0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00 };
static const uint8_t cmd_light_off[] = { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };
static const uint8_t cmd_write_screen[] =  { 0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00 };

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
	timeout = 6000; // what is this for?
	buttonmask = 0;
	_datawheel = 0;
	_device_status = STATUS_OFFLINE;
	udev = 0;
	current_track_id = 0;
	last_where = max_frames;
	wheel_mode = WheelTimeline;
	wheel_shift_mode = WheelShiftGain;
	wheel_increment = WheelIncrScreen;
	bling_mode = BlingOff;
	timerclear (&last_wheel_motion);
	last_wheel_dir = 1;
	last_track_gain = FLT_MAX;
	display_mode = DisplayNormal;
	gain_fraction = 0.0;
	invalidate();
	screen_init();
	lights_init();
	print(0,0,"!!Welcome to Ardour!!");
	print(1,0,"!Peace through Music!");
}

void TranzportControlProtocol::light_validate (LightID light) 
{
	lights_invalid[light] = 0;
}

void TranzportControlProtocol::light_invalidate (LightID light) 
{
	lights_invalid[light] = 1;
}

void TranzportControlProtocol::lights_validate () 
{
	memset (lights_invalid, 0, sizeof (lights_invalid)); 
}

void TranzportControlProtocol::lights_invalidate () 
{
	memset (lights_invalid, 1, sizeof (lights_invalid)); 
}

void TranzportControlProtocol::lights_init()
{
	for (uint32_t i = 0; i < sizeof(lights_current)/sizeof(lights_current[0]); i++) {
		lights_invalid[i] = lights_current[i] = 
			lights_pending[i] = lights_flash[i] = false;
	}
}



int
TranzportControlProtocol::lights_flush ()
{
	if ( _device_status == STATUS_OFFLINE) { return (0); }

	//  Figure out iterators one day soon
	//  for (LightID i = i.start(), i = i.end(); i++) {
	//  if (lights_pending[i] != lights_current[i] || lights_invalid[i]) {
	//    if (light_set(i, lights_pending[i])) { 
	//       return i-1;
	//    } 
	//  }
	//}
	if ((lights_pending[LightRecord] != lights_current[LightRecord]) || lights_invalid[LightRecord]) {
		if (light_set(LightRecord,lights_pending[LightRecord])) {
			return 1;
		}
	}
	if ((lights_pending[LightTrackrec] != lights_current[LightTrackrec]) || lights_invalid[LightTrackrec]) {
		if (light_set(LightTrackrec,lights_pending[LightTrackrec])) {
			return 1;
		}
	}

	if ((lights_pending[LightTrackmute] != lights_current[LightTrackmute]) || lights_invalid[LightTrackmute]) {
		if (light_set(LightTrackmute,lights_pending[LightTrackmute])) {
			return 1;
		}
	}

	if ((lights_pending[LightTracksolo] != lights_current[LightTracksolo]) || lights_invalid[LightTracksolo]) {
		if (light_set(LightTracksolo,lights_pending[LightTracksolo])) {
			return 1;
		}
	}
	if ((lights_pending[LightAnysolo] != lights_current[LightAnysolo]) || lights_invalid[LightAnysolo]) {
		if (light_set(LightAnysolo,lights_pending[LightAnysolo])) {
			return 1;
		}
	}
	if ((lights_pending[LightLoop] != lights_current[LightLoop]) || lights_invalid[LightLoop]) {
		if (light_set(LightLoop,lights_pending[LightLoop])) {
			return 1;
		}
	}
	if ((lights_pending[LightPunch] != lights_current[LightPunch]) || lights_invalid[LightPunch]) {
		if (light_set(LightPunch,lights_pending[LightPunch])) {
			return 1;
		}
	}

	return 0;
}

// Screen specific commands

void
TranzportControlProtocol::screen_clear ()
{
	const char *blank = "                    ";
	print(0,0,blank); 
	print(1,0,blank);
}

void TranzportControlProtocol::screen_invalidate ()
{
	for(int row = 0; row < 2; row++) {
		for(int col = 0; col < 20; col++) {
			screen_invalid[row][col] = true;
			screen_current[row][col] = 0x7f;
			screen_pending[row][col] = ' ';
			// screen_flash[row][col] = ' ';
		}
	}
	// memset (&screen_invalid, 1, sizeof(screen_invalid));
	// memset (&screen_current, 0x7F, sizeof (screen_current)); // fill cache with a character we otherwise never use
}

void TranzportControlProtocol::screen_validate ()
{
}

void TranzportControlProtocol::screen_init ()
{
	screen_invalidate();
}

int
TranzportControlProtocol::screen_flush ()
{
	int cell = 0, row, col_base, col, pending = 0;
	if ( _device_status == STATUS_OFFLINE) { return (-1); }

	for (row = 0; row < 2 && pending == 0; row++) {
		for (col_base = 0, col = 0; col < 20 && pending == 0; ) {
			if ((screen_pending[row][col] != screen_current[row][col]) 
					|| screen_invalid[row][col]) {

				/* something in this cell is different, so dump the cell to the device. */

				uint8_t cmd[8]; 
				cmd[0] = 0x00; 
				cmd[1] = 0x01; 
				cmd[2] = cell; 
				cmd[3] = screen_pending[row][col_base]; 
				cmd[4] = screen_pending[row][col_base+1];
				cmd[5] = screen_pending[row][col_base+2]; 
				cmd[6] = screen_pending[row][col_base+3];
				cmd[7] = 0x00;

				if(write(cmd) != 0) {
					/* try to update this cell on the next go-round */
#if DEBUG_TRANZPORT > 4
					printf("usb screen update failed for some reason... why? \ncmd and data were %02x %02x %02x %02x %02x %02x %02x %02x\n", 
							cmd[0],cmd[1],cmd[2], cmd[3], cmd[4], cmd[5],cmd[6],cmd[7]); 
#endif
					pending += 1;	
					// Shouldn't need to do this
					// screen_invalid[row][col_base] = screen_invalid[row][col_base+1] = 
					// screen_invalid[row][col_base+2] = screen_invalid[row][col_base+3] = true;

				} else {
					/* successful write: copy to current cached display */
					screen_invalid[row][col_base] = screen_invalid[row][col_base+1] = 
						screen_invalid[row][col_base+2] = screen_invalid[row][col_base+3] = false;
					memcpy (&screen_current[row][col_base], &screen_pending[row][col_base], 4);
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
	return pending;
}


//  Tranzport specific

void TranzportControlProtocol::invalidate() 
{
	lcd_damage(); lights_invalidate(); screen_invalidate(); // one of these days lcds can be fine but screens not
}

TranzportControlProtocol::~TranzportControlProtocol ()
{
	set_active (false);
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
			screen_clear ();
			lcd_damage();
			lights_off ();
			for(int x = 0; x < 10 && flush(); x++) { usleep(1000); }
			pthread_cancel_one (thread);
			cerr << "Tranzport Thread dead\n";
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
		if ((g != last_track_gain) || lcd_isdamaged(0,9,8)) {
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
			enter_big_meter_mode();
			break;

		case DisplayBigMeter:
			enter_normal_display_mode();
			break;

		case DisplayRecording:
			enter_normal_display_mode();
			break;

		case DisplayRecordingMeter:
			enter_big_meter_mode();
			break;

		case DisplayConfig: 
		case DisplayBling:
		case DisplayBlingMeter:
			enter_normal_display_mode();
			break;
	}
}

// FIXME, these 3 aren't done yet

void
TranzportControlProtocol::enter_recording_mode ()
{
	lcd_damage(); // excessive
	screen_clear ();
	lights_off ();
	display_mode = DisplayRecording;
}

void
TranzportControlProtocol::enter_bling_mode ()
{
	lcd_damage();
	screen_clear ();
	lights_off ();
	display_mode = DisplayBling;
}

void
TranzportControlProtocol::enter_config_mode ()
{
	lcd_damage();
	screen_clear ();
	lights_off ();
	display_mode = DisplayConfig;
}


void
TranzportControlProtocol::enter_big_meter_mode ()
{
	screen_clear ();
	lcd_damage();
	lights_off ();
	last_meter_fill = 0;
	display_mode = DisplayBigMeter;
}

void
TranzportControlProtocol::enter_normal_display_mode ()
{
	screen_clear ();
	lcd_damage();
	lights_off ();
	display_mode = DisplayNormal;
	//  normal_update();
}


float
log_meter (float db)
{
	float def = 0.0f; /* Meter deflection %age */

	if (db < -70.0f) return 0.0f;
	if (db > 6.0f) return 1.0f;

	if (db < -60.0f) {
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
	// you only seem to get a route_table[0] on moving forward - bug elsewhere
	if (route_table[0] == 0) {
		// Principle of least surprise
		print (0, 0, "No audio to meter!!!");
		print (1, 0, "Select another track"); 
		return;
	}

	float level = route_get_peak_input_power (0, 0);
	float fraction = log_meter (level);

	/* Someday add a peak bar*/

	/* we draw using a choice of a sort of double colon-like character ("::") or a single, left-aligned ":".
	   the screen is 20 chars wide, so we can display 40 different levels. compute the level,
	   then figure out how many "::" to fill. if the answer is odd, make the last one a ":"
	   */

	uint32_t fill  = (uint32_t) floor (fraction * 40);
	char buf[22];
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
TranzportControlProtocol::show_bbt (framepos_t where)
{ 
	if ((where != last_where) || lcd_isdamaged(1,9,8)) {
		char buf[16];
		Timecode::BBT_Time bbt;
		session->tempo_map().bbt_time (where, bbt);
		sprintf (buf, "%03" PRIu32 "|%02" PRIu32 "|%04" PRIu32, bbt.bars,bbt.beats,bbt.ticks);
		last_bars = bbt.bars;
		last_beats = bbt.beats;
		last_ticks = bbt.ticks;
		last_where = where;

		if(last_ticks < 1960) { print (1, 9, buf); } // save a write so we can do leds

		// if displaymode is recordmode show beats but not yet
		lights_pending[LightRecord] = false;
		lights_pending[LightAnysolo] = false;
		switch(last_beats) {
			case 1: if(last_ticks < 500 || last_ticks > 1960) lights_pending[LightRecord] = true; break;
			default: if(last_ticks < 250) lights_pending[LightAnysolo] = true;
		}

		// update lights for tempo one day
		//        if (bbt_upper_info_label) {
		//     TempoMap::Metric m (session->tempo_map().metric_at (when));
		//     sprintf (buf, "%-5.2f", m.tempo().beats_per_minute());
		//      bbt_lower_info_label->set_text (buf);
		//      sprintf (buf, "%g|%g", m.meter().beats_per_bar(), m.meter().note_divisor());
		//      bbt_upper_info_label->set_text (buf);
	}
	}


void
TranzportControlProtocol::show_transport_time ()
{
	show_bbt (session->transport_frame ());
}	

void
TranzportControlProtocol::show_smpte (framepos_t where)
{
	if ((where != last_where) || lcd_isdamaged(1,9,10)) {

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
		print_noretry (1, 18, buf); 

		last_where = where;
	}
}

void*
TranzportControlProtocol::_monitor_work (void* arg)
{
	return static_cast<TranzportControlProtocol*>(arg)->monitor_work ();
}

// I note that these usb specific open, close, probe, read routines are basically 
// pure boilerplate and could easily be abstracted elsewhere

#if !HAVE_TRANZPORT_KERNEL_DRIVER

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

int TranzportControlProtocol::read(uint8_t *buf, uint32_t timeout_override) 
{
	int val;
	// Get smarter about handling usb errors soon. Like disconnect
	//  pthread_testcancel();
	val = usb_interrupt_read (udev, READ_ENDPOINT, (char *) buf, 8, 10);
	//  pthread_testcancel();
	return val;
} 

	
int
TranzportControlProtocol::write_noretry (uint8_t* cmd, uint32_t timeout_override)
{
	int val;
	if(inflight > MAX_TRANZPORT_INFLIGHT) { return (-1); }
	val = usb_interrupt_write (udev, WRITE_ENDPOINT, (char*) cmd, 8, timeout_override ? timeout_override : timeout);

	if (val < 0) {
#if DEBUG_TRANZPORT
		printf("usb_interrupt_write failed: %d\n", val);
#endif
		return val;
		}

	if (val != 8) {
#if DEBUG_TRANZPORT
		printf("usb_interrupt_write failed: %d\n", val);
#endif
		return -1;
		}
	++inflight;

	return 0;

}	

int
TranzportControlProtocol::write (uint8_t* cmd, uint32_t timeout_override)
{
#if MAX_RETRY > 1
	int val;
	int retry = 0;
	if(inflight > MAX_TRANZPORT_INFLIGHT) { return (-1); }
	
	while((val = usb_interrupt_write (udev, WRITE_ENDPOINT, (char*) cmd, 8, timeout_override ? timeout_override : timeout))!=8 && retry++ < MAX_RETRY) {
		printf("usb_interrupt_write failed, retrying: %d\n", val);
	}

	if (retry == MAX_RETRY) {
		printf("Too many retries on a tranzport write, aborting\n");
		}

	if (val < 0) {
		printf("usb_interrupt_write failed: %d\n", val);
		return val;
		}
	if (val != 8) {
		printf("usb_interrupt_write failed: %d\n", val);
		return -1;
		}
	++inflight;
	return 0;
#else
	return (write_noretry(cmd,timeout_override));
#endif

}	

#else
#error Kernel API not defined yet for Tranzport
// Something like open(/dev/surface/tranzport/event) for reading and raw for writing)
#endif

// We have a state "Unknown" - STOP USING SPACES FOR IT - switching to arrow character
// We have another state - no_retry. Misleading, as we still retry on the next pass
// I think it's pointless to keep no_retry and instead we should throttle writes 
// We have an "displayed" screen
// We always draw into the pending screen, which could be any of several screens
// We have an active screen
// Print arg - we have 
// setactive
// so someday I think we need a screen object.

/*
screen_flash.clear();
screen_flash.print(0,0,"Undone:"); // Someday pull the undo stack from somewhere
screen_flash.print(1,0,"Nextup:"); 

if(flash_messages && lcd.getactive() != screen_flash) lcd.setactive(screen_flash,2000);

screen::setactive(screen_name,duration); // duration in ms
screen::getactive();
*/


int
TranzportControlProtocol::flush ()
{
	int pending = 0;
	if(!(pending = lights_flush())) {
		pending = screen_flush(); 
	} 
	return pending;
}

// doing these functions made me realize that screen_invalid should be lcd_isdamaged FIXME soon

bool TranzportControlProtocol::lcd_damage() 
{
	screen_invalidate();
	return true;
}

bool TranzportControlProtocol::lcd_damage (int row, int col, int length)
{
	bool result = false;
	int endcol = col+length-1;
	if((endcol > 19)) { endcol = 19; } 
	if((row >= 0 && row < 2) && (col >=0 && col < 20)) {
		for(int c = col; c < endcol; c++) {
			screen_invalid[row][c] = true;
		}
		result = true;
	}
	return result;
}

// Gotta switch to bitfields, this is collossally dumb
// Still working on the layering, arguably screen_invalid should be lcd_invalid

bool TranzportControlProtocol::lcd_isdamaged () 
{
	for(int r = 0; r < 2; r++) {
		for(int c = 0; c < 20; c++) {
			if(screen_invalid[r][c]) {
#if DEBUG_TRANZPORT > 5	
				printf("row: %d,col: %d is damaged, should redraw it\n", r,c);
#endif
				return true;
			}
		}
	}
	return false;
}

bool TranzportControlProtocol::lcd_isdamaged (int row, int col, int length)
{
	bool result = 0;
	int endcol = col+length;
	if((endcol > 19)) { endcol = 19; } 
	if((row >= 0 && row < 2) && (col >=0 && col < 20)) {
		for(int c = col; c < endcol; c++) {
			if(screen_invalid[row][c]) {
#if DEBUG_TRANZPORT > 5	
				printf("row: %d,col: %d is damaged, should redraw it\n", row,c);
#endif
				return true;
			}
		}
	}
	return result;
}

// lcd_clear would be a separate function for a smart display
// here it does nothing, but for the sake of completeness it should
// probably write the lcd, and while I'm on the topic it should probably
// take a row, col, length argument....

void
TranzportControlProtocol::lcd_clear ()
{

}

// These lcd commands are not universally used yet and may drop out of the api

int
TranzportControlProtocol::lcd_flush ()
{
	return 0; 
}

int 
TranzportControlProtocol::lcd_write(uint8_t* cmd, uint32_t timeout_override)
{
	return write(cmd,timeout_override);
}

void 
TranzportControlProtocol::lcd_fill (uint8_t fill_char) 
{
}

void 
TranzportControlProtocol::lcd_print (int row, int col, const char* text) 
{
	print(row,col,text);
}

void TranzportControlProtocol::lcd_print_noretry (int row, int col, const char* text)
{
	print(row,col,text);
}

// Lights are buffered

void
TranzportControlProtocol::lights_on ()
{
	lights_pending[LightRecord] = lights_pending[LightTrackrec] = 
		lights_pending[LightTrackmute] =  lights_pending[LightTracksolo] = 
		lights_pending[LightAnysolo] =   lights_pending[LightLoop] = 
		lights_pending[LightPunch] = true;
}

void
TranzportControlProtocol::lights_off ()
{
	lights_pending[LightRecord] = lights_pending[LightTrackrec] = 
		lights_pending[LightTrackmute] =  lights_pending[LightTracksolo] = 
		lights_pending[LightAnysolo] =   lights_pending[LightLoop] = 
		lights_pending[LightPunch] = false;
}

int
TranzportControlProtocol::light_on (LightID light)
{
	lights_pending[light] = true;
	return 0;
}

int
TranzportControlProtocol::light_off (LightID light)
{
	lights_pending[light] = false;
	return 0;
}

int
TranzportControlProtocol::light_set (LightID light, bool offon)
{
	uint8_t cmd[8];
	cmd[0] = 0x00;  cmd[1] = 0x00;  cmd[2] = light;  cmd[3] = offon;
	cmd[4] = 0x00;  cmd[5] = 0x00;  cmd[6] = 0x00;  cmd[7] = 0x00;

	if (write (cmd) == 0) {
		lights_current[light] = offon;
		lights_invalid[light] = false;
		return 0;
	} else {
		return -1;
	}
}

int TranzportControlProtocol::rtpriority_set(int priority) 
{
	struct sched_param rtparam;
	int err;
	// preallocate and memlock some stack with memlock?
	char *a = (char*) alloca(4096*2); a[0] = 'a'; a[4096] = 'b';
	memset (&rtparam, 0, sizeof (rtparam));
	rtparam.sched_priority = priority; /* XXX should be relative to audio (JACK) thread */
	// Note - try SCHED_RR with a low limit 
	// - we don't care if we can't write everything this ms
	// and it will help if we lose the device
	if ((err = pthread_setschedparam (pthread_self(), SCHED_FIFO, &rtparam)) != 0) {
		PBD::info << string_compose (_("%1: thread not running with realtime scheduling (%2)"), name(), strerror (errno)) << endmsg;
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

// Slowly breaking this into where I can make usb processing it's own thread.

void*
TranzportControlProtocol::monitor_work ()
{
	uint8_t buf[8];
	int val = 0, pending = 0;
	bool first_time = true;
	uint8_t offline = 0;


	PBD::notify_gui_about_thread_creation ("gui", pthread_self(), X_("Tranzport"));
	pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, 0);
	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);
	next_track ();
	rtpriority_set();
	inflight=0;
	flush();

	while (true) {

		/* bInterval for this beastie is 10ms */

		if (_device_status == STATUS_OFFLINE) {
			first_time = true;
			if(offline++ == 1) { 
				cerr << "Transport has gone offline\n";
			}
		} else { 
			offline = 0; // hate writing this
		}

		val = read(buf);

		if (val == 8) {
			process (buf);
		}

#if DEBUG_TRANZPORT > 2
		if(inflight > 1) printf("Inflight: %d\n", inflight);
#endif


		if (_device_status != STATUS_OFFLINE) {
			if (first_time) {
				invalidate();
				lcd_clear ();
				lights_off ();
				first_time = false;
				offline = 0;
				pending = 3; // Give some time for the device to recover
			}
			/* update whatever needs updating */
			update_state ();

			/* still struggling with a good means of exerting flow control */
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
			// pending = 0;
		} 
	}

	return (void*) 0;
}

int TranzportControlProtocol::lights_show_recording() 
{
	//   FIXME, flash recording light when recording and transport is moving
	return     lights_show_normal();
}

// gotta do bling next!

int TranzportControlProtocol::lights_show_bling() 
{
	switch (bling_mode) {
		case BlingOff: break;
		case BlingKit: break; // rotate rec/mute/solo/any solo back and forth
		case BlingRotating: break; // switch between lights
		case BlingPairs: break; // Show pairs of lights
		case BlingRows: break; // light each row in sequence
		case BlingFlashAll: break; // Flash everything randomly
	}
	return 0;
}

int TranzportControlProtocol::lights_show_normal() 
{
	/* Track only */

	if (route_table[0]) {
		boost::shared_ptr<AudioTrack> at = boost::dynamic_pointer_cast<AudioTrack> (route_table[0]);
		lights_pending[LightTrackrec]  = at && at->record_enabled();
		lights_pending[LightTrackmute] = route_get_muted(0); 
		lights_pending[LightTracksolo] = route_get_soloed(0);
	} else {
		lights_pending[LightTrackrec]  = false;
		lights_pending[LightTracksolo] = false;
		lights_pending[LightTrackmute] = false;
	}

	/* Global settings */

	lights_pending[LightLoop]        = session->get_play_loop(); 
	lights_pending[LightPunch]       = Config->get_punch_in() || Config->get_punch_out();
	lights_pending[LightRecord]      = session->get_record_enabled();
	lights_pending[LightAnysolo]     = session->soloing();

	return 0;
}

int TranzportControlProtocol::lights_show_tempo() 
{
	// someday soon fiddle with the lights based on the tempo 
	return     lights_show_normal();
}

int
TranzportControlProtocol::update_state ()
{
	/* do the text and light updates */

	switch (display_mode) {
		case DisplayBigMeter:
			lights_show_tempo();
			show_meter ();
			break;

		case DisplayNormal:
			lights_show_normal();
			normal_update ();
			break;

		case DisplayConfig:
			break;

		case DisplayRecording:
			lights_show_recording();
			normal_update(); 
			break;

		case DisplayRecordingMeter:
			lights_show_recording();
			show_meter(); 
			break;

		case DisplayBling:
			lights_show_bling();
			normal_update();
			break;

		case DisplayBlingMeter:
			lights_show_bling();
			show_meter();
			break;
	}
	return 0;

}

#define TRANZPORT_BUTTON_HANDLER(callback, button_arg) if (button_changes & button_arg) { \
    if (buttonmask & button_arg) { \
      callback##_press (buttonmask&ButtonShift); } else { callback##_release (buttonmask&ButtonShift); } }

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

	// SHIFT + STOP + PLAY for bling mode?
	// if (button_changes & ButtonPlay & ButtonStop) {
	// bling_mode_toggle();  
	// } or something like that

	TRANZPORT_BUTTON_HANDLER(button_event_battery,ButtonBattery);
	TRANZPORT_BUTTON_HANDLER(button_event_backlight,ButtonBacklight);
	TRANZPORT_BUTTON_HANDLER(button_event_trackleft,ButtonTrackLeft);
	TRANZPORT_BUTTON_HANDLER(button_event_trackright,ButtonTrackRight);
	TRANZPORT_BUTTON_HANDLER(button_event_trackrec,ButtonTrackRec);
	TRANZPORT_BUTTON_HANDLER(button_event_trackmute,ButtonTrackMute);
	TRANZPORT_BUTTON_HANDLER(button_event_tracksolo,ButtonTrackSolo);
	TRANZPORT_BUTTON_HANDLER(button_event_undo,ButtonUndo);
	TRANZPORT_BUTTON_HANDLER(button_event_in,ButtonIn);
	TRANZPORT_BUTTON_HANDLER(button_event_out,ButtonOut);
	TRANZPORT_BUTTON_HANDLER(button_event_punch,ButtonPunch);
	TRANZPORT_BUTTON_HANDLER(button_event_loop,ButtonLoop);
	TRANZPORT_BUTTON_HANDLER(button_event_prev,ButtonPrev);
	TRANZPORT_BUTTON_HANDLER(button_event_add,ButtonAdd);
	TRANZPORT_BUTTON_HANDLER(button_event_next,ButtonNext);
	TRANZPORT_BUTTON_HANDLER(button_event_rewind,ButtonRewind);
	TRANZPORT_BUTTON_HANDLER(button_event_fastforward,ButtonFastForward);
	TRANZPORT_BUTTON_HANDLER(button_event_stop,ButtonStop);
	TRANZPORT_BUTTON_HANDLER(button_event_play,ButtonPlay);
	TRANZPORT_BUTTON_HANDLER(button_event_record,ButtonRecord);
	return 0;
}

void
TranzportControlProtocol::show_current_track ()
{
	char pad[11];
	char *v;
	int len;
	if (route_table[0] == 0) {
		print (0, 0, "----------");
		last_track_gain = FLT_MAX;
	} else {
		strcpy(pad,"          ");
		v =  (char *)route_get_name (0).substr (0, 10).c_str();
		if((len = strlen(v)) > 0) {
			strncpy(pad,(char *)v,len);
		}
		print (0, 0, pad);
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
#if DEBUG_TRANZPORT
	printf("backlight pressed\n");
#endif
}

void
TranzportControlProtocol::button_event_backlight_release (bool shifted)
{
#if DEBUG_TRANZPORT
	printf("backlight released\n\n");
#endif
	if (shifted) {
		lcd_damage();
		lcd_clear();
		last_where += 1; /* force time redisplay */
		last_track_gain = FLT_MAX;
		normal_update(); //  redraw_screen();  
	}
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
	if (shifted) {
	  // Mute ALL? Something useful when a phone call comes in. Mute master?
	} else {
	  route_set_muted (0, !route_get_muted (0));
	}
}

void
TranzportControlProtocol::button_event_trackmute_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_tracksolo_press (bool shifted)
{
#if DEBUG_TRANZPORT
	printf("solo pressed\n");
#endif
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
#if DEBUG_TRANZPORT
	printf("solo released\n");
#endif
}

void
TranzportControlProtocol::button_event_undo_press (bool shifted)
{
	if (shifted) {
		redo (); // someday flash the screen with what was redone
	} else {
		undo (); // someday flash the screen with what was undone
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
	if (shifted) {
	  set_transport_speed (1.0f);
	} else {
	  transport_play ();
	}
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

void button_event_mute (bool pressed, bool shifted)
{
	static int was_pressed = 0;
	//  if(pressed) { }
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

				case WheelShiftMarker:
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
	float m = 1.0;
	if (_datawheel < WheelDirectionThreshold) {
		m = 1.0;
	} else {
		m = -1.0;
	}
	switch(wheel_increment) {
		case WheelIncrScreen: ScrollTimeline (0.2*m); break;
		default: break; // other modes unimplemented as yet
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
TranzportControlProtocol::config ()
{
  // FIXME
}

void
TranzportControlProtocol::shuttle ()
{
	if (_datawheel < WheelDirectionThreshold) {
		if (session->transport_speed() < 0) {
			session->request_transport_speed (1.0);
		} else {
			session->request_transport_speed_nonzero (session->transport_speed() + 0.1);
		}
	} else {
		if (session->transport_speed() > 0) {
			session->request_transport_speed (-1.0);
		} else {
			session->request_transport_speed_nonzero (session->transport_speed() - 0.1);
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
		break;
	case WheelShiftMarker: // Not done yet, disabled
 	        wheel_shift_mode = WheelShiftGain;
		break;
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
			text += ":Pan ";
			break;

		case WheelShiftMaster:
			text += ":Mstr";
			break;

		case WheelShiftMarker:
			text += ":Mrkr";
			break;
	}

	print (1, 0, text.c_str());
}

// Was going to keep state around saying to retry or not
// haven't got to it yet, still not sure it's a good idea

void
TranzportControlProtocol::print (int row, int col, const char *text) {
	print_noretry(row,col,text);
}

void
TranzportControlProtocol::print_noretry (int row, int col, const char *text)
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
		
		memcpy (tmp, &screen_pending[row][base_col], 4);
		
		/* overwrite with new text */
		
		uint32_t tocopy = min ((4U - offset), left);
		
		memcpy (tmp+offset, text, tocopy);
		
		/* copy it back to pending */
		
		memcpy (&screen_pending[row][base_col], tmp, 4);
		
		text += tocopy;
		left -= tocopy;
		col  += tocopy;
	}
}	

XMLNode&
TranzportControlProtocol::get_state () 
{
	return ControlProtocol::get_state();
}

int
TranzportControlProtocol::set_state (const XMLNode& node)
{
	return 0;
}

int
TranzportControlProtocol::save (char *name) 
{
	// Presently unimplemented
	return 0;
}

int
TranzportControlProtocol::load (char *name) 
{
	// Presently unimplemented
	return 0;
}
