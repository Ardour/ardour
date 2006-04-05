#include <iostream>
#include <sys/time.h>

#include <pbd/pthread_utils.h>

#include <ardour/route.h>
#include <ardour/audio_track.h>
#include <ardour/session.h>
#include <ardour/location.h>

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

	memset (next_screen, ' ', sizeof (next_screen));
	memset (current_screen, ' ', sizeof (current_screen));

	for (uint32_t i = 0; i < sizeof(lights)/sizeof(lights[0]); ++i) {
		lights[i] = false;
	}
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
	show_transport_time ();

	if (session.soloing()) {
		light_on (LightAnysolo);
	} else {
		light_off (LightAnysolo);
	}

	flush_lcd ();
}

void
TranzportControlProtocol::show_transport_time ()
{
	jack_nframes_t where = session.transport_frame();
	
	if (where != last_where) {

		uint8_t label[12];
		SMPTE_Time smpte;
		char* ptr = (char *) label;

		session.smpte_time (where, smpte);
		memset (label, ' ', sizeof (label));
		
		if (smpte.negative) {
			sprintf (ptr, "-%02ld:", smpte.hours);
		} else {
			sprintf (ptr, " %02ld:", smpte.hours);
		}
		ptr += 4;

		sprintf (ptr, "%02ld:", smpte.minutes);
		ptr += 3;

		sprintf (ptr, "%02ld:", smpte.seconds);
		ptr += 3;

		sprintf (ptr, "%02ld", smpte.frames);
		ptr += 2;
		
		write_clock (label);

		last_where = where;
	}
}

void
TranzportControlProtocol::write_clock (const uint8_t* label)
{
	memcpy (&next_screen[1][8], &label[0], 4);
	memcpy (&next_screen[1][12], &label[4], 4);
	memcpy (&next_screen[1][16], &label[8], 4);
}

void
TranzportControlProtocol::flush_lcd ()
{
	if (memcmp (&next_screen[0][0], &current_screen[0][0], 4)) {
		cerr << "diff 1\n";
		lcd_write (0, &next_screen[0][0]);
	}
	if (memcmp (&next_screen[0][4], &current_screen[0][4], 4)) {
		cerr << "diff 2\n";
		lcd_write (1, &next_screen[0][4]);
	}
	if (memcmp (&next_screen[0][8], &current_screen[0][8], 4)) {
		cerr << "diff 3\n";
		lcd_write (2, &next_screen[0][8]);
	}
	if (memcmp (&next_screen[0][12], &current_screen[0][12], 4)) {
		cerr << "diff 4\n";
		lcd_write (3, &next_screen[0][12]);
	}
	if (memcmp (&next_screen[0][16], &current_screen[0][16], 4)) {
		cerr << "diff 5\n";
		lcd_write (4, &next_screen[0][16]);
	}
	if (memcmp (&next_screen[1][0], &current_screen[1][0], 4)) {
		cerr << "diff 6\n";
		lcd_write (5, &next_screen[1][0]);
	}
	if (memcmp (&next_screen[1][4], &current_screen[1][4], 4)) {
		cerr << "diff 7\n";
		lcd_write (6, &next_screen[1][4]);
	}
	if (memcmp (&next_screen[1][8], &current_screen[1][8], 4)) {
		cerr << "diff 8\n";
		lcd_write (7, &next_screen[1][8]);
	}
	if (memcmp (&next_screen[1][12], &current_screen[1][12], 4)) {
		cerr << "diff 9\n";
		lcd_write (8, &next_screen[1][12]);
	}
	if (memcmp (&next_screen[1][16], &current_screen[1][16], 4)) {
		cerr << "diff 10\n";
		lcd_write (9, &next_screen[1][16]);
	}

	/* copy the current state into the next state */

	memcpy (next_screen, current_screen, sizeof (current_screen));
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

	while (true) {
		if (read()) {
			return 0;
		}
		switch (_device_status) {
		case STATUS_OFFLINE:
			cerr << "offline\n";
			break;
		case STATUS_ONLINE:
		case 0:
			cerr << "online\n";
			break;
		default:
			cerr << "unknown status\n";
			break;
		}

		if (_device_status == STATUS_ONLINE || _device_status == 0) {
			break;
		}
	}

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
	lcd_write (0, "    ");
	lcd_write (1, "    ");
	lcd_write (2, "    ");
	lcd_write (3, "    ");
	lcd_write (4, "    ");
	lcd_write (5, "    ");
	lcd_write (6, "    ");
	lcd_write (7, "    ");
	lcd_write (8, "    ");
	lcd_write (9, "    ");
}

int
TranzportControlProtocol::lcd_write (uint8_t cell, const char* text)       
{
	uint8_t cmd[8];
	
	if (cell > 9) {
		return -1;
	}

	cmd[0] = 0x00;
	cmd[1] = 0x01;
	cmd[2] = cell;
	cmd[3] = text[0];
	cmd[4] = text[1];
	cmd[5] = text[2];
	cmd[6] = text[3];
	cmd[7] = 0x00;

	if (write (cmd, 500) == 0) {
		int row;
		int col;
		
		switch (cell) {
		case 0:
			row = 0;
			col = 0;
			break;
		case 1:
			row = 0;
			col = 4;
			break;
		case 2:
			row = 0;
			col = 8;
			break;
		case 3:
			row = 0;
			col = 12;
			break;
		case 4:
			row = 0;
			col = 16;
			break;
		case 5:
			row = 1;
			col = 0;
			break;
		case 6:
			row = 1;
			col = 4;
			break;
		case 7:
			row = 1;
			col = 8;
			break;
		case 8:
			row = 1;
			col = 12;
			break;
		case 9:
			row = 1;
			col = 16;
			break;
		}
		
		current_screen[row][col]   = text[0];
		current_screen[row][col+1] = text[1];
		current_screen[row][col+2] = text[2];
		current_screen[row][col+3] = text[3];

		cerr << "stored " << text[0] << text[1] << text[2] << text[3] << " to " << row << ',' << col << endl;

		return 0;
	}

	cerr << "failed to write text\n";
	return -1;
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
		char buf[5];
		lcd_write (0, "----");
		lcd_write (1, "----");
		return;
	}

	string name = current_route->name();

	memcpy (&next_screen[0][0], name.substr (0, 4).c_str(), 4);
	memcpy (&next_screen[0][4], name.substr (4, 4).c_str(), 4);

	track_solo_changed (0);
	track_mute_changed (0);
	track_rec_changed (0);

	track_connections.push_back (current_route->solo_changed.connect (mem_fun (*this, &TranzportControlProtocol::track_solo_changed)));
	track_connections.push_back (current_route->mute_changed.connect (mem_fun (*this, &TranzportControlProtocol::track_mute_changed)));
	track_connections.push_back (current_route->record_enable_changed.connect (mem_fun (*this, &TranzportControlProtocol::track_rec_changed)));

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

	cerr << "current track = " << current_track_id << " route = " << current_route << endl;
	
	show_current_track ();
}

void
TranzportControlProtocol::button_event_trackleft_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_trackright_press (bool shifted)
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

	cerr << "current track = " << current_track_id << " route = " << current_route;
	if (current_route) {
		cerr << ' ' << current_route->name();
	} 
	cerr << endl;
	show_current_track ();
}

void
TranzportControlProtocol::button_event_trackright_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_trackrec_press (bool shifted)
{
	if (current_route) {
		AudioTrack* at = dynamic_cast<AudioTrack*>(current_route);
		at->set_record_enable (!at->record_enabled(), this);
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
	if (current_route) {
		current_route->set_solo (!current_route->soloed(), this);
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
}

void
TranzportControlProtocol::button_event_in_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_out_press (bool shifted)
{
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
}

void
TranzportControlProtocol::button_event_loop_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_prev_press (bool shifted)
{
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
}

void
TranzportControlProtocol::button_event_record_release (bool shifted)
{
}
