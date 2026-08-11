/*
 * Copyright (C) 2026 Ada <ada@6bit.com>
 * Copyright (C) 2026 Joshua Perry <josh@6bit.com>
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

#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <glibmm/main.h>

#include "pbd/compose.h"
#include "pbd/error.h"

#include "ardour/debug.h"
#include "ardour/session.h"

#include "komplete_kontrol_a.h"

#include "pbd/abstract_ui.inc.cc" // instantiate template, includes i18n

using namespace ARDOUR;
using namespace PBD;
using namespace ArdourSurface;

/* The device reports on change only, so this is a poll for "did anything
 * happen", not a sample clock -- it costs a syscall that almost always
 * returns nothing.  1ms matches maschine2, which is the in-tree precedent.
 */
static const unsigned int read_interval_ms = 1;

bool
KompleteKontrolA::available ()
{
	if (hid_init ()) {
		return false;
	}
	hid_exit ();
	return true;
}

KompleteKontrolA::KompleteKontrolA (ARDOUR::Session& s)
	: ControlProtocol (s, std::string (X_("NI Komplete Kontrol A-Series")))
	, AbstractUI<KompleteKontrolARequest> (name ())
	, _handle (0)
	, _variant (0)
	, _seeded (false)
	, _buttons (0)
	, _encoder_pos (0)
	, _warned_short_report (false)
	, _warned_read_error (false)
{
	memset (_knob_raw, 0, sizeof (_knob_raw));
	memset (_knob_accum, 0, sizeof (_knob_accum));

	if (hid_init ()) {
		throw KompleteKontrolAException ("HIDAPI initialization failed");
	}

	BaseUI::run ();
}

KompleteKontrolA::~KompleteKontrolA ()
{
	stop ();
	hid_exit ();
}

void
KompleteKontrolA::do_request (KompleteKontrolARequest* req)
{
	if (req->type == CallSlot) {
		call_slot (MISSING_INVALIDATOR, req->the_slot);
	} else if (req->type == Quit) {
		stop ();
	}
}

int
KompleteKontrolA::set_active (bool yn)
{
	if (yn == active ()) {
		return 0;
	}

	if (yn) {
		if (start ()) {
			return -1;
		}
	} else {
		if (stop ()) {
			return -1;
		}
	}

	ControlProtocol::set_active (yn);
	return 0;
}

XMLNode&
KompleteKontrolA::get_state () const
{
	XMLNode& node (ControlProtocol::get_state ());
	return node;
}

int
KompleteKontrolA::set_state (const XMLNode& node, int version)
{
	if (ControlProtocol::set_state (node, version)) {
		return -1;
	}
	return 0;
}

/* ------------------------------------------------------------------------ */

int
KompleteKontrolA::open_device ()
{
	/* Deliberately not hid_open (vid, pid, NULL).  To filter by ids, hidapi's
	 * Linux backend re-reads each candidate's sysfs uevent with
	 *
	 *	open (path, O_RDONLY | FD_CLOEXEC)
	 *
	 * but FD_CLOEXEC is a descriptor flag rather than an open() flag, and its
	 * value is 1 -- which is O_WRONLY.  Opening a root-owned sysfs attribute
	 * write-only is refused for any ordinary user, so the parse fails and the
	 * device is skipped without comment.  Every device is skipped, so
	 * hid_open() by ids cannot match at all unless we are root.  Unfiltered
	 * enumeration never takes that path, so matching the ids here and opening
	 * by path is both a fix and cheaper than the call it replaces.
	 */
	struct hid_device_info* devs = hid_enumerate (0x0, 0x0);

	for (size_t i = 0; i < KKA::NumVariants && !_handle; ++i) {
		for (struct hid_device_info* d = devs; d; d = d->next) {
			if (d->vendor_id != KKA::VendorID || d->product_id != KKA::Variants[i].pid) {
				continue;
			}
			if ((_handle = hid_open_path (d->path)) != 0) {
				_variant = &KKA::Variants[i];
				break;
			}
		}
	}

	hid_free_enumeration (devs);

	if (!_handle) {
		error << _("Komplete Kontrol A-Series: no device found") << endmsg;
		return -1;
	}

	char usbid[16];
	snprintf (usbid, sizeof (usbid), "%04x:%04x", KKA::VendorID, _variant->pid);

	info << string_compose (_("Komplete Kontrol A-Series: found %1 (%2), %3 keys"),
	                        _variant->model, usbid, _variant->keys)
	     << endmsg;

	/* The A25 and A49 are believed identical to the A61 apart from the
	 * keybed, but only the A61 has been tested.  Rather than assume, check
	 * the one thing that would actually break parsing -- the shape of the
	 * report descriptor -- and say so loudly if it differs.  That turns the
	 * first A25 user into a useful bug report instead of a silent misdecode.
	 */
	if (!_variant->tested) {
		unsigned char desc[4096];
		int n = hid_get_report_descriptor (_handle, desc, sizeof (desc));

		if (n < 0) {
			info << string_compose (_("Komplete Kontrol A-Series: %1 is untested; "
			                          "please report any misbehaviour."),
			                        _variant->model)
			     << endmsg;
		} else if ((size_t) n != KKA::ExpectedDescriptorSize) {
			warning << string_compose (_("Komplete Kontrol A-Series: %1 reports a %2 byte HID "
			                             "descriptor, expected %3. This model is untested and "
			                             "controls may be misread. Please report this, with the "
			                             "output of `lsusb -v -d %4`."),
			                           _variant->model, n, KKA::ExpectedDescriptorSize, usbid)
			        << endmsg;
		} else {
			info << string_compose (_("Komplete Kontrol A-Series: %1 is untested, but its HID "
			                          "descriptor matches the A61. Proceeding."),
			                        _variant->model)
			     << endmsg;
		}
	}

	hid_set_nonblocking (_handle, 1);
	return 0;
}

void
KompleteKontrolA::close_device ()
{
	if (!_handle) {
		return;
	}

	/* Once interactive mode has been entered the firmware stops drawing the
	 * panel and never resumes -- not even on a switch back to MIDI mode. So
	 * leaving without blanking strands the user's last frame on the screen
	 * until they replug.
	 */
	clear_leds ();
	blank_display ();
	set_device_mode (KKA::ModeMIDI);

	hid_close (_handle);
	_handle = 0;
	_variant = 0;
}

int
KompleteKontrolA::start ()
{
	if (open_device ()) {
		return -1;
	}

	/* Interactive mode takes the knobs off the MIDI port and hands us the
	 * display. It does not survive a replug, so a user who reconnects
	 * mid-session silently gets a device sending CC 14-21 at their tracks
	 * again; re-asserting on hotplug is Phase 3 work.
	 */
	if (set_device_mode (KKA::ModeInteractive)) {
		error << _("Komplete Kontrol A-Series: failed to enter interactive mode") << endmsg;
		close_device ();
		return -1;
	}

	clear_leds ();
	blank_display ();

	_seeded = false;
	_buttons = 0;
	_encoder_pos = 0;
	memset (_knob_raw, 0, sizeof (_knob_raw));
	memset (_knob_accum, 0, sizeof (_knob_accum));

	Glib::RefPtr<Glib::TimeoutSource> read_timeout = Glib::TimeoutSource::create (read_interval_ms);
	_read_connection = read_timeout->connect (sigc::mem_fun (*this, &KompleteKontrolA::dev_read));
	read_timeout->attach (main_loop ()->get_context ());

	return 0;
}

int
KompleteKontrolA::stop ()
{
	_read_connection.disconnect ();

	close_device ();

	BaseUI::quit ();
	return 0;
}

void
KompleteKontrolA::thread_init ()
{
	ARDOUR::SessionEvent::create_per_thread_pool (event_loop_name (), 1024);
	PBD::notify_event_loops_about_thread_creation (pthread_self (), event_loop_name (), 1024);
	set_thread_priority ();
}

/* --------------------------------------------------------------- output */

int
KompleteKontrolA::set_device_mode (const uint8_t mode[2])
{
	if (!_handle) {
		return -1;
	}

	uint8_t buf[3] = { KKA::ReportMode, mode[0], mode[1] };
	return hid_write (_handle, buf, sizeof (buf)) < 0 ? -1 : 0;
}

int
KompleteKontrolA::clear_leds ()
{
	if (!_handle) {
		return -1;
	}

	uint8_t buf[KKA::LEDReportSize];
	memset (buf, KKA::LEDOff, sizeof (buf));
	buf[0] = KKA::ReportLEDs;

	return hid_write (_handle, buf, sizeof (buf)) < 0 ? -1 : 0;
}

int
KompleteKontrolA::blank_display ()
{
	if (!_handle) {
		return -1;
	}

	int rv = 0;

	/* Two full-width two-page bands cover the panel. A band costs exactly
	 * what a single page costs -- the report is a fixed 265 bytes either
	 * way -- so this is the cheapest possible full-panel write.
	 */
	for (int page = 0; page < KKA::DisplayPages; page += 2) {
		uint8_t buf[KKA::DisplayReportSize];

		/* Polarity is inverted: a set bit renders dark, so all-ones blanks. */
		memset (buf, 0xff, sizeof (buf));

		buf[0] = KKA::ReportDisplay;
		buf[1] = 0;                                  /* x offset, pixels     */
		buf[2] = 0;
		buf[3] = page;                               /* y offset, pages      */
		buf[4] = 0;
		buf[5] = KKA::DisplayWidth & 0xff;           /* width, pixels        */
		buf[6] = (KKA::DisplayWidth >> 8) & 0xff;
		buf[7] = 2;                                  /* height, pages        */
		buf[8] = 0;

		if (hid_write (_handle, buf, sizeof (buf)) < 0) {
			rv = -1;
		}
	}

	return rv;
}

/* ---------------------------------------------------------------- input */

bool
KompleteKontrolA::dev_read ()
{
	if (!_handle) {
		return false;
	}

	uint8_t buf[KKA::InputReportSize];

	/* Reports are emitted on change only, but a fast knob spin coalesces
	 * several between wakeups, so drain rather than taking one per tick.
	 * The bound is a safety valve against a device that never goes quiet.
	 */
	for (int i = 0; i < 32; ++i) {
		int n = hid_read (_handle, buf, sizeof (buf));

		if (n == 0) {
			break;
		}

		if (n < 0) {
			if (!_warned_read_error) {
				_warned_read_error = true;
				error << _("Komplete Kontrol A-Series: HID read failed; was the device unplugged?")
				      << endmsg;
			}
			break;
		}

		if (buf[0] != KKA::ReportInput) {
			continue;
		}

		if ((size_t) n != KKA::InputReportSize) {
			if (!_warned_short_report) {
				_warned_short_report = true;
				warning << string_compose (_("Komplete Kontrol A-Series: %1 byte input report, "
				                             "expected %2. Controls may be misread."),
				                           n, KKA::InputReportSize)
				        << endmsg;
			}
			continue;
		}

		decode (buf + 1, n - 1);
	}

	return true;
}

void
KompleteKontrolA::decode (const uint8_t* p, size_t len)
{
	if (len < KKA::InputPayloadSize) {
		return;
	}

	uint64_t buttons = 0;
	for (size_t i = 0; i < KKA::ButtonsBytes; ++i) {
		buttons |= (uint64_t) p[KKA::ButtonsOffset + i] << (8 * i);
	}

	uint16_t knobs[KKA::NumKnobs];
	for (int k = 0; k < KKA::NumKnobs; ++k) {
		size_t o = KKA::KnobsOffset + 2 * k;
		knobs[k] = (uint16_t) (p[o] | (p[o + 1] << 8));
	}

	uint8_t encoder = p[KKA::EncoderOffset] & 0x0f;

	if (!_seeded) {
		_buttons = buttons;
		memcpy (_knob_raw, knobs, sizeof (_knob_raw));
		_encoder_pos = encoder;
		_seeded = true;
		return;
	}

	uint64_t changed = buttons ^ _buttons;
	_buttons = buttons;

	/* Declared bits 34..39 are padding on this hardware and are deliberately
	 * not dispatched.
	 */
	for (int b = 0; b < KKA::NumControls; ++b) {
		if (changed & (1ULL << b)) {
			handle_button ((KKA::ControlID) b, (buttons >> b) & 1);
		}
	}

	for (int k = 0; k < KKA::NumKnobs; ++k) {
		int raw = KKA::knob_delta (_knob_raw[k], knobs[k]);
		_knob_raw[k] = knobs[k];

		if (!raw) {
			continue;
		}

		/* Carry the remainder rather than truncating it. Every delta seen on
		 * this hardware is an exact multiple of the detent size, so today
		 * this is a no-op -- but a dropped report would otherwise silently
		 * discard the fraction.
		 */
		_knob_accum[k] += raw;
		int detents = _knob_accum[k] / KKA::KnobUnitsPerDetent;
		_knob_accum[k] -= detents * KKA::KnobUnitsPerDetent;

		if (detents) {
			handle_knob (k, detents);
		}
	}

	int ed = KKA::encoder_delta (_encoder_pos, encoder);
	_encoder_pos = encoder;
	if (ed) {
		handle_encoder (ed);
	}
}

/* Phase 2 stops here: decode and trace. Phase 3 binds these to Ardour. */

void
KompleteKontrolA::handle_button (KKA::ControlID c, bool pressed)
{
	DEBUG_TRACE (DEBUG::KompleteKontrolA,
	             string_compose ("KKA button %1 (bit %2) %3\n",
	                             KKA::control_name (c), (int) c,
	                             pressed ? "pressed" : "released"));
}

void
KompleteKontrolA::handle_knob (int knob, int detents)
{
	DEBUG_TRACE (DEBUG::KompleteKontrolA,
	             string_compose ("KKA knob %1 %2%3\n",
	                             knob + 1, detents > 0 ? "+" : "", detents));
}

void
KompleteKontrolA::handle_encoder (int detents)
{
	DEBUG_TRACE (DEBUG::KompleteKontrolA,
	             string_compose ("KKA 4-D encoder %1%2\n",
	                             detents > 0 ? "+" : "", detents));
}
