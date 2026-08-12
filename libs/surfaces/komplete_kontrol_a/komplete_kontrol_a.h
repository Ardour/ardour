/*
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

#ifndef _ardour_surfaces_komplete_kontrol_a_h_
#define _ardour_surfaces_komplete_kontrol_a_h_

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#endif

#include <exception>
#include <string>

#include <glibmm/threads.h>

#include <hidapi.h>

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"

#include "ardour/types.h"
#include "control_protocol/control_protocol.h"

#include "kka_protocol.h"

namespace ArdourSurface {

class KompleteKontrolAException : public std::exception
{
public:
	KompleteKontrolAException (const std::string& msg) : _msg (msg) {}
	virtual ~KompleteKontrolAException () throw () {}
	const char* what () const throw () { return _msg.c_str (); }

private:
	std::string _msg;
};

struct KompleteKontrolARequest : public BaseUI::BaseRequestObject {
public:
	KompleteKontrolARequest () {}
	~KompleteKontrolARequest () {}
};

/* One class for the A25, A49 and A61.  They are the same device with three
 * keybeds, and the keybed is not ours -- it leaves over the device's own MIDI
 * port -- so the only per-model state is a name and a key count.  See
 * KKA::Variants in kka_protocol.cc.
 */
class KompleteKontrolA : public ARDOUR::ControlProtocol, public AbstractUI<KompleteKontrolARequest>
{
public:
	KompleteKontrolA (ARDOUR::Session&, const KKA::Variant& variant);
	~KompleteKontrolA ();

	static bool available ();

	int set_active (bool yn);

	XMLNode& get_state () const;
	int      set_state (const XMLNode&, int version);

	CONTROL_PROTOCOL_THREADS_NEED_TEMPO_MAP_DECL();

private:
	void do_request (KompleteKontrolARequest*);
	void stripable_selection_changed () {}

	int start ();
	int stop ();

	void thread_init ();

	/* device lifecycle */
	int  open_device (bool quiet_if_absent);
	void close_device (bool graceful);
	int  take_over_device ();
	void device_vanished ();

	/* output paths */
	int set_device_mode (const uint8_t mode[2]);
	int clear_leds ();
	int blank_display ();

	/* LEDs.  The report is all 21 at once, so callers set what they want and
	 * flush once; flush_leds() writes only when something actually changed.
	 */
	void set_led (KKA::ControlID, uint8_t brightness);
	int  flush_leds ();
	int  flush_leds_locked (); /* caller holds _device_mutex */

	/* Ardour bindings */
	void connect_session_signals ();
	void transport_state_changed ();
	void parameter_changed (std::string);
	void refresh_transport_leds ();

	/* input path */
	bool dev_read ();
	bool dev_reconnect ();
	bool blink ();
	void start_read_poll ();
	void start_reconnect_poll ();
	void decode (const uint8_t* payload, size_t len);

	/* Phase 3 binds these; for now they trace. */
	void handle_button (KKA::ControlID, bool pressed);
	void handle_knob (int knob, int steps);
	void handle_encoder (int steps);

	hid_device* _handle;

	/* The model the user ticked in the surface list, resolved before the
	 * surface was constructed and fixed for its lifetime.  Only this model is
	 * ever opened: a user who selected an A25 has told us which device they
	 * have, and silently driving an A61 instead makes the selection a lie.
	 * Unlike _handle it survives a replug, because which keyboard is
	 * configured does not change when the cable is pulled.
	 */
	const KKA::Variant& _variant;

	/* Exactly one of these is live at a time: the fast input poll while the
	 * device is present, the slow reconnect poll while it is not.  Each hands
	 * over by attaching the other and then returning false, so neither ever
	 * disconnects the source it is running inside.
	 */
	sigc::connection    _read_connection;
	sigc::connection    _reconnect_connection;

	/* Belongs to the surface being active rather than to the device being
	 * present, so unlike the two above it is attached once and stays.
	 */
	sigc::connection    _blink_connection;
	bool                _blink_on;

	/* Input state.  _seeded covers the analog fields only -- the knobs and the
	 * encoder are wrapping counters needing a baseline before a delta means
	 * anything.  Buttons are absolute and are dispatched from the first report
	 * onwards; see decode() for why seeding them was wrong.
	 */
	bool     _seeded;
	uint64_t _buttons;
	uint16_t _knob_raw[KKA::NumKnobs];
	int      _knob_accum[KKA::NumKnobs];
	uint8_t  _encoder_pos;

	/* Whole previous payload, so that movement in the bytes this decode does
	 * not interpret is reported rather than discarded.  See decode().
	 */
	uint8_t  _payload_prev[KKA::InputPayloadSize];

	bool _warned_short_report;

	/* Session signals are connected with this surface as their event loop, so
	 * they arrive on our thread rather than Ardour's -- which is what lets the
	 * LED writes below share a thread with the read poll.
	 */
	PBD::ScopedConnectionList _session_connections;

	/* Desired LED state, indexed by ControlID, which for 0..LEDCount-1 is also
	 * the index in the LED report. No mapping table in either direction.
	 */
	uint8_t _led[KKA::LEDCount];
	bool    _leds_dirty;

	/* Guards _handle. Everything else touching the device runs on this
	 * surface's event loop, but stop() arrives on Ardour's GUI thread and
	 * closes the handle out from under it.
	 */
	Glib::Threads::Mutex _device_mutex;
};

} /* namespace ArdourSurface */

#endif /* _ardour_surfaces_komplete_kontrol_a_h_ */
