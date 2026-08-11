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

/* Reconnect polling, used only while the device is absent.  hid_enumerate()
 * walks every hidraw node in sysfs and reads a file per node, so this must not
 * run anywhere near the read poll's rate.  A second is imperceptible to
 * somebody who has just pushed a plug in.
 */
static const unsigned int reconnect_interval_ms = 1000;

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
	, _leds_dirty (false)
{
	memset (_knob_raw, 0, sizeof (_knob_raw));
	memset (_knob_accum, 0, sizeof (_knob_accum));
	memset (_payload_prev, 0, sizeof (_payload_prev));
	memset (_led, KKA::LEDOff, sizeof (_led));

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
KompleteKontrolA::open_device (bool quiet_if_absent)
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
	Glib::Threads::Mutex::Lock lm (_device_mutex);

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
		/* Silent when polling for a replug: the whole point of that loop is
		 * that the device is expected to be absent, most of the time, for as
		 * long as the user leaves it unplugged.
		 */
		if (!quiet_if_absent) {
			error << _("Komplete Kontrol A-Series: no device found") << endmsg;
		}
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

	/* Load-bearing for hotplug, not just for latency. This does not set
	 * O_NONBLOCK; it makes hid_read() run hid_read_timeout() with a zero
	 * timeout, which polls first and returns -1 when revents carries
	 * POLLERR/POLLHUP/POLLNVAL. That is how a disconnect is detected -- and
	 * hidapi works that way precisely because some kernels will not report
	 * disconnection through a bare non-blocking read(). Drop this call and
	 * reads would block the event loop forever instead.
	 */
	hid_set_nonblocking (_handle, 1);
	return 0;
}

void
KompleteKontrolA::close_device (bool graceful)
{
	/* The goodbye writes take the lock individually, so it cannot be held
	 * across them; taking it only for the close itself is enough, because
	 * every other path rechecks _handle under it.
	 */
	{
		Glib::Threads::Mutex::Lock lm (_device_mutex);
		if (!_handle) {
			return;
		}
	}

	/* Once interactive mode has been entered the firmware stops drawing the
	 * panel and never resumes -- not even on a switch back to MIDI mode. So
	 * leaving without blanking strands the user's last frame on the screen
	 * until they replug.
	 *
	 * Skipped when the device has vanished, where none of these writes can
	 * land anyway and there is nothing to be polite to: a replugged device
	 * comes back power-on clean, in MIDI mode, with the firmware drawing its
	 * own panel again.
	 */
	if (graceful) {
		clear_leds ();
		blank_display ();
		set_device_mode (KKA::ModeMIDI);
	}

	Glib::Threads::Mutex::Lock lm (_device_mutex);

	if (!_handle) {
		return; /* raced with another close */
	}

	hid_close (_handle);
	_handle = 0;
	_variant = 0;
}

/* Everything that has to happen against a device we have just opened, whether
 * at startup or after a replug.  Interactive mode does not survive a power
 * cycle, so a returning device is back in MIDI mode -- sending CC 14-21 at
 * whatever the user has armed -- and has to be taken over from scratch.
 */
int
KompleteKontrolA::take_over_device ()
{
	if (set_device_mode (KKA::ModeInteractive)) {
		return -1;
	}

	clear_leds ();
	blank_display ();

	/* Whatever the transport was doing while the device was away, the panel
	 * now has to agree with it.
	 */
	refresh_transport_leds ();

	/* A returning device is a different device as far as this state is
	 * concerned: its knob counters start wherever they start.
	 */
	_seeded = false;
	_buttons = 0;
	_encoder_pos = 0;
	memset (_knob_raw, 0, sizeof (_knob_raw));
	memset (_knob_accum, 0, sizeof (_knob_accum));
	memset (_payload_prev, 0, sizeof (_payload_prev));
	_warned_short_report = false;

	return 0;
}

void
KompleteKontrolA::device_vanished ()
{
	info << _("Komplete Kontrol A-Series: device disconnected, watching for its return")
	     << endmsg;

	close_device (false);
	start_reconnect_poll ();
}

void
KompleteKontrolA::start_read_poll ()
{
	Glib::RefPtr<Glib::TimeoutSource> t = Glib::TimeoutSource::create (read_interval_ms);
	_read_connection = t->connect (sigc::mem_fun (*this, &KompleteKontrolA::dev_read));
	t->attach (main_loop ()->get_context ());
}

void
KompleteKontrolA::start_reconnect_poll ()
{
	Glib::RefPtr<Glib::TimeoutSource> t = Glib::TimeoutSource::create (reconnect_interval_ms);
	_reconnect_connection = t->connect (sigc::mem_fun (*this, &KompleteKontrolA::dev_reconnect));
	t->attach (main_loop ()->get_context ());
}

bool
KompleteKontrolA::dev_reconnect ()
{
	if (open_device (true)) {
		return true; /* still gone; keep watching */
	}

	if (take_over_device ()) {
		/* Present but not yet talking to us -- udev may still be applying
		 * permissions to the new node. Drop it and come back in a second
		 * rather than holding a handle we cannot drive.
		 */
		close_device (false);
		return true;
	}

	info << _("Komplete Kontrol A-Series: device reconnected, interactive mode re-asserted")
	     << endmsg;

	start_read_poll ();
	return false; /* hand over to the read poll */
}

int
KompleteKontrolA::start ()
{
	if (open_device (false)) {
		return -1;
	}

	if (take_over_device ()) {
		error << _("Komplete Kontrol A-Series: failed to enter interactive mode") << endmsg;
		/* Nothing was taken over, so there is nothing to hand back; a
		 * graceful close here would only add three more failing writes.
		 */
		close_device (false);
		return -1;
	}

	connect_session_signals ();
	refresh_transport_leds ();

	start_read_poll ();
	return 0;
}

int
KompleteKontrolA::stop ()
{
	/* Whichever of the two is live -- disconnecting an already-finished
	 * connection is a no-op, so there is no need to know which.
	 */
	_read_connection.disconnect ();
	_reconnect_connection.disconnect ();

	/* Before the device goes, so nothing can arrive mid-teardown and try to
	 * light a lamp on a handle that is being closed.
	 */
	_session_connections.drop_connections ();

	close_device (true);

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
	Glib::Threads::Mutex::Lock lm (_device_mutex);

	if (!_handle) {
		return -1;
	}

	uint8_t buf[3] = { KKA::ReportMode, mode[0], mode[1] };
	return hid_write (_handle, buf, sizeof (buf)) < 0 ? -1 : 0;
}

void
KompleteKontrolA::set_led (KKA::ControlID c, uint8_t brightness)
{
	if (!KKA::control_has_led (c)) {
		return;
	}

	Glib::Threads::Mutex::Lock lm (_device_mutex);

	if (_led[c] != brightness) {
		_led[c] = brightness;
		_leds_dirty = true;
	}
}

int
KompleteKontrolA::flush_leds ()
{
	Glib::Threads::Mutex::Lock lm (_device_mutex);
	return flush_leds_locked ();
}

int
KompleteKontrolA::flush_leds_locked ()
{
	if (!_leds_dirty) {
		return 0;
	}

	if (!_handle) {
		return -1;
	}

	/* All 21 go out together -- there is no partial LED write -- so a single
	 * changed lamp costs the same 22 bytes as all of them.
	 */
	uint8_t buf[KKA::LEDReportSize];
	buf[0] = KKA::ReportLEDs;
	memcpy (buf + 1, _led, KKA::LEDCount);

	if (hid_write (_handle, buf, sizeof (buf)) < 0) {
		return -1;
	}

	_leds_dirty = false;
	return 0;
}

int
KompleteKontrolA::clear_leds ()
{
	Glib::Threads::Mutex::Lock lm (_device_mutex);

	memset (_led, KKA::LEDOff, sizeof (_led));
	_leds_dirty = true;

	return flush_leds_locked ();
}

int
KompleteKontrolA::blank_display ()
{
	Glib::Threads::Mutex::Lock lm (_device_mutex);

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
	uint8_t buf[KKA::InputReportSize];

	/* Reports are emitted on change only, but a fast knob spin coalesces
	 * several between wakeups, so drain rather than taking one per tick.
	 * The bound is a safety valve against a device that never goes quiet.
	 */
	for (int i = 0; i < 32; ++i) {
		int n;

		{
			/* Held across the read alone. decode() reaches into Ardour from
			 * here, and holding a device lock across that is how a deadlock
			 * gets invented later.
			 */
			Glib::Threads::Mutex::Lock lm (_device_mutex);

			if (!_handle) {
				return false;
			}

			n = hid_read (_handle, buf, sizeof (buf));
		}

		if (n == 0) {
			break;
		}

		if (n < 0) {
			/* In non-blocking mode "nothing to read" is 0, so a negative
			 * return is a real failure and in practice always means the
			 * device is gone. Hand over to the reconnect poll rather than
			 * hammering a dead handle every millisecond for the rest of the
			 * session, which is what the previous warn-once-and-carry-on did.
			 */
			device_vanished ();
			return false;
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

	/* The whole payload, so that a session at the hardware can settle what
	 * this decode does not explain -- payload[28] above all, a constant 36 in
	 * every capture so far, and the field free-m32 calls "keyshift" on the
	 * M32.  Guarded rather than merely gated: this runs on a 1ms poll, and
	 * formatting 29 bytes we then throw away would not be free.
	 */
	if (DEBUG_ENABLED (DEBUG::KompleteKontrolA)) {
		std::string hex;
		for (size_t i = 0; i < KKA::InputPayloadSize; ++i) {
			char b[4];
			snprintf (b, sizeof (b), "%02x ", p[i]);
			hex += b;
		}
		DEBUG_TRACE (DEBUG::KompleteKontrolA,
		             string_compose ("KKA raw %1%2\n", hex,
		                             _seeded ? "" : "(first report -- analog baseline)"));
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

	/* The knobs and the 4-D encoder are wrapping counters, so a delta against
	 * them means nothing until there is a baseline; take it from the first
	 * report.
	 *
	 * Buttons are not like that, and seeding them was a mistake.  They are
	 * absolute, and the device reports only on change -- so it stays silent
	 * until the user does something, and the first report to arrive after
	 * start() is already a user action rather than a resting state.  Seeding
	 * buttons from it therefore swallows that action every time: the first
	 * hardware trace ate a Shift press and reported only the release.
	 * "Nothing is held" is the correct opening assumption, and it is the safe
	 * one, so buttons dispatch from the very first report.
	 */
	if (!_seeded) {
		memcpy (_knob_raw, knobs, sizeof (_knob_raw));
		_encoder_pos = encoder;
		memcpy (_payload_prev, p, KKA::InputPayloadSize);
		_seeded = true;
	}

	uint64_t changed = buttons ^ _buttons;
	_buttons = buttons;

	/* Declared bits 34..39 are padding on this hardware and are deliberately
	 * not dispatched.  Say so loudly if they ever move: that would mean a
	 * control exists which the map does not know about.
	 */
	uint64_t undeclared = changed >> KKA::NumControls;
	if (undeclared) {
		char b[32];
		snprintf (b, sizeof (b), "0x%llx", (unsigned long long) undeclared);
		DEBUG_TRACE (DEBUG::KompleteKontrolA,
		             string_compose ("KKA *** bits 34..39 changed (%1) -- the map is incomplete\n", b));
	}

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
		 * this hardware is an exact multiple of the step size, so today this
		 * is a no-op -- but a dropped report would otherwise silently discard
		 * the fraction.
		 */
		_knob_accum[k] += raw;
		int steps = _knob_accum[k] / KKA::KnobUnitsPerStep;
		_knob_accum[k] -= steps * KKA::KnobUnitsPerStep;

		if (steps) {
			handle_knob (k, steps);
		}
	}

	int ed = KKA::encoder_delta (_encoder_pos, encoder);
	_encoder_pos = encoder;
	if (ed) {
		handle_encoder (ed);
	}

	/* Everything from the end of the knob block onwards: the analog fields
	 * that read constant zero on this unit, the encoder byte, and payload[28].
	 * Only the encoder's low nibble means anything to us, so any other
	 * movement here is a fact the map does not yet account for -- report it
	 * instead of dropping it on the floor.
	 */
	for (size_t i = KKA::KnobsOffset + 2 * KKA::NumKnobs; i < KKA::InputPayloadSize; ++i) {
		if (p[i] != _payload_prev[i]) {
			DEBUG_TRACE (DEBUG::KompleteKontrolA,
			             string_compose ("KKA payload[%1] %2 -> %3\n",
			                             i, (int) _payload_prev[i], (int) p[i]));
		}
	}

	memcpy (_payload_prev, p, KKA::InputPayloadSize);
}

/* ------------------------------------------------------- Ardour bindings */

void
KompleteKontrolA::connect_session_signals ()
{
	if (!session) {
		return;
	}

	/* The trailing `this` is the PBD::EventLoop these are delivered on. We are
	 * an AbstractUI, so passing ourselves marshals every callback onto this
	 * surface's own thread -- the same thread as the read poll. That is what
	 * makes it safe for these handlers to write to the device directly.
	 */
	session->TransportStateChange.connect (_session_connections, MISSING_INVALIDATOR,
	                                       std::bind (&KompleteKontrolA::transport_state_changed, this), this);
	session->RecordStateChanged.connect (_session_connections, MISSING_INVALIDATOR,
	                                     std::bind (&KompleteKontrolA::transport_state_changed, this), this);
	session->TransportLooped.connect (_session_connections, MISSING_INVALIDATOR,
	                                  std::bind (&KompleteKontrolA::transport_state_changed, this), this);

	/* The metronome is a global config item rather than session transport
	 * state, so it arrives by a different road.
	 */
	Config->ParameterChanged.connect (_session_connections, MISSING_INVALIDATOR,
	                                  std::bind (&KompleteKontrolA::parameter_changed, this, _1), this);
}

void
KompleteKontrolA::transport_state_changed ()
{
	refresh_transport_leds ();
}

void
KompleteKontrolA::parameter_changed (std::string p)
{
	if (p == "clicking") {
		refresh_transport_leds ();
	}
}

void
KompleteKontrolA::refresh_transport_leds ()
{
	if (!session) {
		return;
	}

	bool rolling = session->transport_rolling ();

	set_led (KKA::Play, rolling ? KKA::LEDBright : KKA::LEDOff);
	set_led (KKA::Stop, rolling ? KKA::LEDOff : KKA::LEDBright);
	set_led (KKA::Loop, session->get_play_loop () ? KKA::LEDBright : KKA::LEDOff);
	set_led (KKA::Metro, Config->get_clicking () ? KKA::LEDBright : KKA::LEDOff);

	/* Armed and actually capturing are different states and the panel has the
	 * range to say so. LEDDim is also the probe for open question 1 -- see
	 * kka_protocol.h.
	 */
	if (session->actively_recording ()) {
		set_led (KKA::Rec, KKA::LEDBright);
	} else if (session->record_status () != ARDOUR::Disabled) {
		set_led (KKA::Rec, KKA::LEDDim);
	} else {
		set_led (KKA::Rec, KKA::LEDOff);
	}

	flush_leds ();
}

void
KompleteKontrolA::handle_button (KKA::ControlID c, bool pressed)
{
	DEBUG_TRACE (DEBUG::KompleteKontrolA,
	             string_compose ("KKA button %1 (bit %2) %3\n",
	                             KKA::control_name (c), (int) c,
	                             pressed ? "pressed" : "released"));

	/* Act on press. Release matters only for controls used as modifiers, and
	 * none of the transport buttons are.
	 */
	if (!pressed) {
		return;
	}

	/* These go straight through BasicUI, which reaches the session by its
	 * request queue rather than by touching transport state here, so calling
	 * them from this thread is correct. The LEDs are not updated here either:
	 * the session tells us what actually happened, and a button that lit
	 * because it was pressed rather than because the transport moved would be
	 * lying whenever the request was refused.
	 */
	switch (c) {
	case KKA::Play:
		/* Same behaviour as the spacebar, which is what a player expects; the
		 * comment on transport_play() says toggle_roll is preferred.
		 */
		toggle_roll (false, true);
		break;
	case KKA::Stop:
		transport_stop ();
		break;
	case KKA::Rec:
		rec_enable_toggle ();
		break;
	case KKA::Loop:
		loop_toggle ();
		break;
	case KKA::Metro:
		toggle_click ();
		break;
	default:
		break;
	}
}

void
KompleteKontrolA::handle_knob (int knob, int steps)
{
	DEBUG_TRACE (DEBUG::KompleteKontrolA,
	             string_compose ("KKA knob %1 %2%3\n",
	                             knob + 1, steps > 0 ? "+" : "", steps));
}

void
KompleteKontrolA::handle_encoder (int steps)
{
	DEBUG_TRACE (DEBUG::KompleteKontrolA,
	             string_compose ("KKA 4-D encoder %1%2\n",
	                             steps > 0 ? "+" : "", steps));
}
