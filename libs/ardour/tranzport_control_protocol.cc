#include <iostream>

#include <pbd/pthread_utils.h>

#include <ardour/tranzport_control_protocol.h>
#include <ardour/route.h>
#include <ardour/session.h>

using namespace ARDOUR;
using namespace std;

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
}

TranzportControlProtocol::~TranzportControlProtocol ()
{
	if (udev) {
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

	pthread_create_and_store (X_("Tranzport"), &thread, 0, _thread_work, this);

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
	jack_nframes_t where = session.transport_frame();

	if (where != last_where) {

		char clock_label[16];
		SMPTE_Time smpte;
		char* ptr = clock_label;

		session.smpte_time (where, smpte);
		memset (clock_label, ' ', sizeof (clock_label));
		
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
		
		lcd_write (7, &clock_label[0]);
		lcd_write (8, &clock_label[4]);
		lcd_write (9, &clock_label[8]);

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
	cerr << "tranzport thread here, sending message to LCD\n";

	while (true) {
		if (read()) {
			return 0;
		}
		switch (_device_status) {
		case STATUS_OFFLINE:
			cerr << "offline\n";
			break;
		case STATUS_ONLINE:
			cerr << "online\n";
			break;
		default:
			cerr << "unknown status\n";
			break;
		}

		if (_device_status == STATUS_ONLINE) {
			break;
		}
	}

	lcd_write (0, "    ");
	lcd_write (1, "WELC");
	lcd_write (2, "OME ");
	lcd_write (3, "TO  ");
	lcd_write (4, "    ");
	lcd_write (5, "    ");
	lcd_write (6, "    ");
	lcd_write (7, "ARDO");
	lcd_write (8, "UR  ");
	lcd_write (9, "    ");

	while (true) {
		if (read ()) {
			cerr << "Tranzport command received\n";
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

	cerr << "checking busses\n";
	
	for (bus = usb_busses; bus; bus = bus->next) {

		cerr << "checking devices\n";

		for(dev = bus->devices; dev; dev = dev->next) {
			cerr << "Checking " << dev->descriptor.idVendor << '/' << dev->descriptor.idProduct << endl;
			if (dev->descriptor.idVendor != VENDORID)
				continue;
			if (dev->descriptor.idProduct != PRODUCTID)
				continue;
			cerr << "Open this one" << endl;
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
TranzportControlProtocol::write (uint8_t* cmd)
{
	int val;
	val = usb_interrupt_write (udev, WRITE_ENDPOINT, (char*) cmd, 8, timeout);
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

	return write (cmd);
}

int
TranzportControlProtocol::light_on (LightID light)
{
	uint8_t cmd[8];

	cmd[0] = 0x00;
	cmd[1] = 0x00;
	cmd[2] = light;
	cmd[3] = 0x01;
	cmd[4] = 0x00;
	cmd[5] = 0x00;
	cmd[6] = 0x00;
	cmd[7] = 0x00;

	return write (cmd);
}

int
TranzportControlProtocol::light_off (LightID light)
{
	uint8_t cmd[8];

	cmd[0] = 0x00;
	cmd[1] = 0x00;
	cmd[2] = light;
	cmd[3] = 0x00;
	cmd[4] = 0x00;
	cmd[5] = 0x00;
	cmd[6] = 0x00;
	cmd[7] = 0x00;

	return write (cmd);
}

int
TranzportControlProtocol::read ()
{
	uint8_t buf[8];
	int val;

	memset(buf, 0, 8);
	val = usb_interrupt_read(udev, READ_ENDPOINT, (char*) buf, 8, timeout);
	if (val < 0) {
		cerr << "Tranzport read error, val = " << val << endl;
		return val;
	}
	if (val != 8) {
		cerr << "Tranzport short read, val = " << val << endl;
		return -1;
	}

	/*printf("read: %02x %02x %02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);*/

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
	current_route = session.route_by_remote_id (current_track_id);
	
	if (current_route == 0) {
		char buf[5];
		lcd_clear ();
		lcd_write (0, "NO T");
		lcd_write (1, "RACK");
		lcd_write (2, " ID ");
		snprintf (buf, sizeof (buf), "%4d", current_track_id);
		lcd_write (3, buf);
		return;
	}

	string name = current_route->name();

	lcd_write (0, name.substr (0, 4).c_str());
	lcd_write (1, name.substr (4, 4).c_str());
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
	
	show_current_track ();
}

void
TranzportControlProtocol::button_event_trackleft_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_trackright_press (bool shifted)
{
	if (current_track_id == session.nroutes()) {
		current_track_id = 0;
	} else {
		current_track_id++;
	}
	
	show_current_track ();
}

void
TranzportControlProtocol::button_event_trackright_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_trackrec_press (bool shifted)
{
}

void
TranzportControlProtocol::button_event_trackrec_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_trackmute_press (bool shifted)
{
}

void
TranzportControlProtocol::button_event_trackmute_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_tracksolo_press (bool shifted)
{
}

void
TranzportControlProtocol::button_event_tracksolo_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_undo_press (bool shifted)
{
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
	session.request_transport_speed (-2.0f);
}

void
TranzportControlProtocol::button_event_rewind_release (bool shifted)
{
}

void
TranzportControlProtocol::button_event_fastforward_press (bool shifted)
{
	session.request_transport_speed (2.0f);
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
