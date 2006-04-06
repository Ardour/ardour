#include <iostream>
#include <algorithm>

#include <sys/time.h>

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

#include "i18n.h"

TranzportControlProtocol::TranzportControlProtocol (Session& s)
	: ControlProtocol  (s, _("Tranzport"))
{
	timeout = 60000;
	buttonmask = 0;
	_datawheel = 0;
	_device_status = STATUS_OFFLINE;
	udev = 0;
	current_route = 0;
	current_track_id = 0;
	last_where = max_frames;
	wheel_mode = WheelGain;
	timerclear (&last_wheel_motion);
	last_wheel_dir = 1;

	memset (current_screen, 0, sizeof (current_screen));

	for (uint32_t i = 0; i < sizeof(lights)/sizeof(lights[0]); ++i) {
		lights[i] = false;
	}

	session.RecordStateChanged.connect (mem_fun (*this, &TranzportControlProtocol::record_status_changed));
}

TranzportControlProtocol::~TranzportControlProtocol ()
{
	if (udev) {
		lcd_clear ();
		pthread_cancel_one (thread);
		close ();
	}
}

int
TranzportControlProtocol::init ()
{
	if (open ()) {
		return -1;
	}

	/* outbound thread */

	init_thread ();

	/* inbound thread */
	
	pthread_create_and_store (X_("tranzport monitor"), &thread, 0, _thread_work, this);

	return 0;
}

bool
TranzportControlProtocol::active() const
{
	return true;
}
		
void
TranzportControlProtocol::send_route_feedback (list<Route*>& routes)
{
}

void
TranzportControlProtocol::send_global_feedback ()
{
	if (_device_status == STATUS_OFFLINE) {
		return;
	}

	show_transport_time ();

	if (session.soloing()) {
		light_on (LightAnysolo);
	} else {
		light_off (LightAnysolo);
	}
}

void
TranzportControlProtocol::show_transport_time ()
{
	jack_nframes_t where = session.transport_frame();
	
	if (where != last_where) {

		char buf[5];
		SMPTE_Time smpte;

		session.smpte_time (where, smpte);
		
		if (smpte.negative) {
			sprintf (buf, "-%02ld:", smpte.hours);
		} else {
			sprintf (buf, " %02ld:", smpte.hours);
		}
		print (1, 8, buf);

		sprintf (buf, "%02ld:", smpte.minutes);
		print (1, 12, buf);

		sprintf (buf, "%02ld:", smpte.seconds);
		print (1, 15, buf);

		sprintf (buf, "%02ld", smpte.frames);
		print (1, 18, buf);

		last_where = where;
	}
}

void*
TranzportControlProtocol::_thread_work (void* arg)
{
	return static_cast<TranzportControlProtocol*>(arg)->thread_work ();
}

void*
TranzportControlProtocol::thread_work ()
{
	PBD::ThreadCreated (pthread_self(), X_("tranzport monitor"));

	/* wait for the device to go online */

	while (true) {
		if (read()) {
			return 0;
		}
		switch (_device_status) {
		case STATUS_OFFLINE:
			cerr << "tranzport offline\n";
			break;
		case STATUS_ONLINE:
		case 0:
			cerr << "tranzport online\n";
			break;
		default:
			cerr << "tranzport: unknown status\n";
			break;
		}

		if (_device_status == STATUS_ONLINE || _device_status == 0) {
			break;
		}
	}

	lcd_clear ();
	show_wheel_mode();

	while (true) {
		if (read ()) {
			break;
		}
	}

	return 0;
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
	print (0, 0, "                    ");
	print (1, 0, "                    ");
}

int
TranzportControlProtocol::lcd_write (int row, int col, uint8_t cell, const char* text)       
{
	uint8_t cmd[8];
	
	if (cell > 9) {
		return -1;
	}

	if (memcmp (text, &current_screen[row][col], 4)) {
	
		current_screen[row][col]   = text[0];
		current_screen[row][col+1] = text[1];
		current_screen[row][col+2] = text[2];
		current_screen[row][col+3] = text[3];

		cmd[0] = 0x00;
		cmd[1] = 0x01;
		cmd[2] = cell;
		cmd[3] = text[0];
		cmd[4] = text[1];
		cmd[5] = text[2];
		cmd[6] = text[3];
		cmd[7] = 0x00;

		return write (cmd, 500);
	}

	return 0;
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

		if (write (cmd, 500) == 0) {
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

		if (write (cmd, 500) == 0) {
			lights[light] = false;
			return 0;
		} else {
			return -1;
		}

	} else {
		return 0;
	}
}

int
TranzportControlProtocol::read (uint32_t timeout_override)
{
	uint8_t buf[8];
	int val;

	memset(buf, 0, 8);
  again:
	val = usb_interrupt_read(udev, READ_ENDPOINT, (char*) buf, 8, timeout_override ? timeout_override : timeout);
	if (val < 0) {
		return val;
	}
	if (val != 8) {
		if (val == 0) {
			goto again;
		}
		return -1;
	}

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
	for (vector<sigc::connection>::iterator i = track_connections.begin(); i != track_connections.end(); ++i) {
		(*i).disconnect ();
	}
	track_connections.clear ();

	if (current_route == 0) {
		print (0, 0, "--------");
		return;
	}

	string name = current_route->name();

	print (0, 0, name.substr (0, 8).c_str());

	track_solo_changed (0);
	track_mute_changed (0);
	track_rec_changed (0);

	track_connections.push_back (current_route->solo_changed.connect (mem_fun (*this, &TranzportControlProtocol::track_solo_changed)));
	track_connections.push_back (current_route->mute_changed.connect (mem_fun (*this, &TranzportControlProtocol::track_mute_changed)));
	track_connections.push_back (current_route->record_enable_changed.connect (mem_fun (*this, &TranzportControlProtocol::track_rec_changed)));
	track_connections.push_back (current_route->gain_changed.connect (mem_fun (*this, &TranzportControlProtocol::track_gain_changed)));
}

void
TranzportControlProtocol::record_status_changed ()
{
	if (session.get_record_enabled()) {
		light_on (LightRecord);
	} else {
		light_off (LightRecord);
	}
}

void
TranzportControlProtocol::track_gain_changed (void* ignored)
{
	char buf[8];
	snprintf (buf, sizeof (buf), "%.1f", coefficient_to_dB (current_route->gain()));
	print (0, 9, buf);
}

void
TranzportControlProtocol::track_solo_changed (void* ignored)
{
	if (current_route->soloed()) {
		light_on (LightTracksolo);
	} else {
		light_off (LightTracksolo);
	}
}

void
TranzportControlProtocol::track_mute_changed (void *ignored)
{
	if (current_route->muted()) {
		light_on (LightTrackmute);
	} else {
		light_off (LightTrackmute);
	}
}

void
TranzportControlProtocol::track_rec_changed (void *ignored)
{
	if (current_route->record_enabled()) {
		light_on (LightTrackrec);
	} else {
		light_off (LightTrackrec);
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
		if (session.get_record_enabled()) {
			session.record_disenable_all ();
		} else {
			session.record_enable_all ();
		}
	} else {
		if (current_route) {
			AudioTrack* at = dynamic_cast<AudioTrack*>(current_route);
			at->set_record_enable (!at->record_enabled(), this);
		}
	}
}

void
TranzportControlProtocol::button_event_trackrec_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_trackmute_press (bool shifted)
{
	if (current_route) {
		current_route->set_mute (!current_route->muted(), this);
	}
}

void
TranzportControlProtocol::button_event_trackmute_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_tracksolo_press (bool shifted)
{
	if (shifted) {
		session.set_all_solo (!session.soloing());
	} else {
		if (current_route) {
			current_route->set_solo (!current_route->soloed(), this);
		}
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
		session.redo (1);
	} else {
		session.undo (1);
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
		next_wheel_mode ();
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
	jack_nframes_t when = session.audible_frame();
	session.locations()->add (new Location (when, when, _("unnamed"), Location::IsMark));
}

void
TranzportControlProtocol::button_event_add_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_next_press (bool shifted)
{
	next_marker ();
}

void
TranzportControlProtocol::button_event_next_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_rewind_press (bool shifted)
{
	if (shifted) {
		session.goto_start ();
	} else {
		session.request_transport_speed (-2.0f);
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
		session.goto_end();
	} else {
		session.request_transport_speed (2.0f);}
}

void
TranzportControlProtocol::button_event_fastforward_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_stop_press (bool shifted)
{
	session.request_transport_speed (0.0);
}

void
TranzportControlProtocol::button_event_stop_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_play_press (bool shifted)
{
	session.request_transport_speed (1.0);
}

void
TranzportControlProtocol::button_event_play_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_record_press (bool shifted)
{
	if (shifted) {
		session.save_state ("");
	} else {
		switch (session.record_status()) {
		case Session::Disabled:
			if (session.ntracks() == 0) {
				// string txt = _("Please create 1 or more track\nbefore trying to record.\nCheck the Session menu.");
				// MessageDialog msg (*editor, txt);
				// msg.run ();
				return;
			}
			session.maybe_enable_record ();
			break;
		case Session::Recording:
		case Session::Enabled:
			session.disable_record (true);
		}
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

		if (current_route) {
			switch (wheel_mode) {
			case WheelGain:
				if (_datawheel < WheelDirectionThreshold) {
					step_gain_up ();
				} else {
					step_gain_down ();
				}
				break;
			case WheelPan:
				if (_datawheel < WheelDirectionThreshold) {
					step_pan_right ();
				} else {
					step_pan_left ();
				}
				break;

			case WheelMaster:
				break;
			}
		}

		timerclear (&last_wheel_motion);

	} else {

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
			speed = 1.0f;
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

		session.request_transport_speed (speed * dir);
	}
}

void
TranzportControlProtocol::step_gain_up ()
{
	if (buttonmask & ButtonStop) {
		current_route->inc_gain (0.01, this);
	} else {
		current_route->inc_gain (0.1, this);
	}
}

void
TranzportControlProtocol::step_gain_down ()
{
	if (buttonmask & ButtonStop) {
		current_route->inc_gain (-0.01, this);
	} else {
		current_route->inc_gain (-0.1, this);
	}
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
TranzportControlProtocol::next_wheel_mode ()
{
	switch (wheel_mode) {
	case WheelGain:
		wheel_mode = WheelPan;
		break;
	case WheelPan:
		wheel_mode = WheelMaster;
		break;
	case WheelMaster:
		wheel_mode = WheelGain;
	}

	show_wheel_mode ();
}

void
TranzportControlProtocol::next_marker ()
{
	Location *location = session.locations()->first_location_after (session.transport_frame());

	if (location) {
		session.request_locate (location->start(), session.transport_rolling());
	} else {
		session.request_locate (session.current_end_frame());
	}
}

void
TranzportControlProtocol::prev_marker ()
{
	Location *location = session.locations()->first_location_before (session.transport_frame());
	
	if (location) {
		session.request_locate (location->start(), session.transport_rolling());
	} else {
		session.goto_start ();
	}
}

void
TranzportControlProtocol::next_track ()
{
	uint32_t limit = session.nroutes();

	if (current_track_id == limit) {
		current_track_id = 0;
	} else {
		current_track_id++;
	}

	while (current_track_id < limit) {
		if ((current_route = session.route_by_remote_id (current_track_id)) != 0) {
			break;
		}
		current_track_id++;
	}

	if (current_track_id == limit) {
		current_track_id = 0;
	}

	show_current_track ();
}

void
TranzportControlProtocol::prev_track ()
{
	if (current_track_id == 0) {
		current_track_id = session.nroutes() - 1;
	} else {
		current_track_id--;
	}

	while (current_track_id >= 0) {
		if ((current_route = session.route_by_remote_id (current_track_id)) != 0) {
			break;
		}
		current_track_id--;
	}

	if (current_track_id < 0) {
		current_track_id = 0;
	}

	show_current_track ();
}

void
TranzportControlProtocol::show_wheel_mode ()
{
	switch (wheel_mode) {
	case WheelGain:
		print (1, 0, _("Wh: gain"));
		break;

	case WheelPan:
		print (1, 0, _("Wh: pan "));
		break;

	case WheelMaster:
		print (1, 0, _("Wh: mstr"));
		break;
	}
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

		memcpy (tmp, &current_screen[row][base_col], 4);

		/* overwrite with new text */

		uint32_t tocopy = min ((4U - offset), left);

		memcpy (tmp+offset, text, tocopy);
		
		cell += (row * 5);

		lcd_write (row, base_col, cell, tmp);
		
		text += tocopy;
		left -= tocopy;
		col  += tocopy;
	}
}	

