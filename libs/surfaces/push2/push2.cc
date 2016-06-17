/*
	Copyright (C) 2016 Paul Davis

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <cairomm/context.h>
#include <cairomm/surface.h>
#include <pangomm/layout.h>

#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/debug.h"
#include "pbd/failed_constructor.h"

#include "midi++/parser.h"
#include "timecode/time.h"
#include "timecode/bbt_time.h"

#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/midiport_manager.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "push2.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;

#include "i18n.h"

#include "pbd/abstract_ui.cc" // instantiate template

const int Push2::cols = 960;
const int Push2::rows = 160;
const int Push2::pixels_per_row = 1024;

#define ABLETON 0x2982
#define PUSH2   0x1967

Push2::Push2 (ARDOUR::Session& s)
	: ControlProtocol (s, string (X_("Ableton Push 2")))
	, AbstractUI<Push2Request> (name())
	, handle (0)
	, device_buffer (0)
	, frame_buffer (Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, cols, rows))
	, modifier_state (None)
	, bank_start (0)
{
	context = Cairo::Context::create (frame_buffer);
	tc_clock_layout = Pango::Layout::create (context);
	bbt_clock_layout = Pango::Layout::create (context);

	Pango::FontDescription fd ("Sans Bold 24");
	tc_clock_layout->set_font_description (fd);
	bbt_clock_layout->set_font_description (fd);

	Pango::FontDescription fd2 ("Sans 10");
	for (int n = 0; n < 8; ++n) {
		upper_layout[n] = Pango::Layout::create (context);
		upper_layout[n]->set_font_description (fd2);
		upper_layout[n]->set_text ("solo");
		lower_layout[n] = Pango::Layout::create (context);
		lower_layout[n]->set_font_description (fd2);
		lower_layout[n]->set_text ("mute");
	}

	Pango::FontDescription fd3 ("Sans Bold 10");
	for (int n = 0; n < 8; ++n) {
		mid_layout[n] = Pango::Layout::create (context);
		mid_layout[n]->set_font_description (fd3);
	}

	build_maps ();

	if (open ()) {
		throw failed_constructor ();
	}

}

Push2::~Push2 ()
{
	stop ();
}

int
Push2::open ()
{
	int err;

	if (handle) {
		/* already open */
		return 0;
	}

	if ((handle = libusb_open_device_with_vid_pid (NULL, ABLETON, PUSH2)) == 0) {
		return -1;
	}

	if ((err = libusb_claim_interface (handle, 0x00))) {
		return -1;
	}

	device_frame_buffer = new uint16_t[rows*pixels_per_row];

	memset (device_frame_buffer, 0, sizeof (uint16_t) * rows * pixels_per_row);

	frame_header[0] = 0xef;
	frame_header[1] = 0xcd;
	frame_header[2] = 0xab;
	frame_header[3] = 0x89;
	memset (&frame_header[4], 0, 12);

	/* setup ports */

	_async_in  = AudioEngine::instance()->register_input_port (DataType::MIDI, X_("push2 in"), true);
	_async_out = AudioEngine::instance()->register_output_port (DataType::MIDI, X_("push2 out"), true);

	if (_async_in == 0 || _async_out == 0) {
		return -1;
	}

	_input_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_async_in).get();
	_output_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_async_out).get();

	connect_to_parser ();

	return 0;
}

int
Push2::close ()
{
	init_buttons (false);

	/* wait for button data to be flushed */
	AsyncMIDIPort* asp;
	asp = dynamic_cast<AsyncMIDIPort*> (_output_port);
	asp->drain (10000, 500000);

	AudioEngine::instance()->unregister_port (_async_in);
	AudioEngine::instance()->unregister_port (_async_out);

	_async_in.reset ((ARDOUR::Port*) 0);
	_async_out.reset ((ARDOUR::Port*) 0);
	_input_port = 0;
	_output_port = 0;

	vblank_connection.disconnect ();
	periodic_connection.disconnect ();
	session_connections.drop_connections ();
	stripable_connections.drop_connections ();

	if (handle) {
		libusb_release_interface (handle, 0x00);
		libusb_close (handle);
		handle = 0;
	}

	for (int n = 0; n < 8; ++n) {
		stripable[n].reset ();
	}

	delete [] device_frame_buffer;
	device_frame_buffer = 0;

	return 0;
}

void
Push2::init_buttons (bool startup)
{
	/* This is a list of buttons that we want lit because they do something
	   in ardour related (loosely, sometimes) to their illuminated label.
	*/

	ButtonID buttons[] = { Mute, Solo, Master, Up, Right, Left, Down, Note, Session, Mix, AddTrack, Delete, Undo,
	                       Metronome, Shift, Select, Play, RecordEnable, Automate, Repeat, Note, Session, DoubleLoop,
	                       Quantize, Duplicate, Browse, PageRight, PageLeft,
	};

	for (size_t n = 0; n < sizeof (buttons) / sizeof (buttons[0]); ++n) {
		Button* b = id_button_map[buttons[n]];

		if (startup) {
			b->set_color (LED::White);
		} else {
			b->set_color (LED::Black);
		}
		b->set_state (LED::OneShot24th);
		write (b->state_msg());
	}

	/* Strip buttons should all be off (black) by default. They will change
	 * color to reflect various conditions
	 */

	ButtonID strip_buttons[] = { Upper1, Upper2, Upper3, Upper4, Upper5, Upper6, Upper7, Upper8,
	                             Lower1, Lower2, Lower3, Lower4, Lower5, Lower6, Lower7, Lower8, };

	for (size_t n = 0; n < sizeof (strip_buttons) / sizeof (strip_buttons[0]); ++n) {
		Button* b = id_button_map[strip_buttons[n]];

		b->set_color (LED::Black);
		b->set_state (LED::OneShot24th);
		write (b->state_msg());
	}

	if (startup) {

		/* all other buttons are off (black) */

		ButtonID off_buttons[] = { TapTempo, Setup, User, Stop, Convert, New, FixedLength,
		                           Fwd32ndT, Fwd32nd, Fwd16thT, Fwd16th, Fwd8thT, Fwd8th, Fwd4trT, Fwd4tr,
		                           Accent, Scale, Layout, Note, Session,  OctaveUp, OctaveDown, };

		for (size_t n = 0; n < sizeof (off_buttons) / sizeof (off_buttons[0]); ++n) {
			Button* b = id_button_map[off_buttons[n]];

			b->set_color (LED::Black);
			b->set_state (LED::OneShot24th);
			write (b->state_msg());
		}
	}

}

bool
Push2::probe ()
{
	libusb_device_handle *h;
	libusb_init (NULL);

	if ((h = libusb_open_device_with_vid_pid (NULL, ABLETON, PUSH2)) == 0) {
		DEBUG_TRACE (DEBUG::Push2, "no Push2 device found\n");
		return false;
	}

	libusb_close (h);
	DEBUG_TRACE (DEBUG::Push2, "Push2 device located\n");
	return true;
}

void*
Push2::request_factory (uint32_t num_requests)
{
	/* AbstractUI<T>::request_buffer_factory() is a template method only
	   instantiated in this source module. To provide something visible for
	   use in the interface/descriptor, we have this static method that is
	   template-free.
	*/
	return request_buffer_factory (num_requests);
}

void
Push2::do_request (Push2Request * req)
{
	DEBUG_TRACE (DEBUG::Push2, string_compose ("doing request type %1\n", req->type));
	if (req->type == CallSlot) {

		call_slot (MISSING_INVALIDATOR, req->the_slot);

	} else if (req->type == Quit) {

		stop ();
	}
}

int
Push2::stop ()
{
	BaseUI::quit ();
	close ();
	return 0;
}

/** render host-side frame buffer (a Cairo ImageSurface) to the current
 * device-side frame buffer. The device frame buffer will be pushed to the
 * device on the next call to vblank()
 */

int
Push2::blit_to_device_frame_buffer ()
{
	/* ensure that all drawing has been done before we fetch pixel data */

	frame_buffer->flush ();

	const int stride = 3840; /* bytes per row for Cairo::FORMAT_ARGB32 */
	const uint8_t* data = frame_buffer->get_data ();

	/* fill frame buffer (320kB) */

	uint16_t* fb = (uint16_t*) device_frame_buffer;

	for (int row = 0; row < rows; ++row) {

		const uint8_t* dp = data + row * stride;

		for (int col = 0; col < cols; ++col) {

			/* fetch r, g, b (range 0..255). Ignore alpha */

			const int r = (*((const uint32_t*)dp) >> 16) & 0xff;
			const int g = (*((const uint32_t*)dp) >> 8) & 0xff;
			const int b = *((const uint32_t*)dp) & 0xff;

			/* convert to 5 bits, 6 bits, 5 bits, respectively */
			/* generate 16 bit BGB565 value */

			*fb++ = (r >> 3) | ((g & 0xfc) << 3) | ((b & 0xf8) << 8);

			/* the push2 docs state that we should xor the pixel
			 * data. Doing so doesn't work correctly, and not doing
			 * so seems to work fine (colors roughly match intended
			 * values).
			 */

			dp += 4;
		}

		/* skip 128 bytes to next line. This is filler, used to avoid line borders occuring in the middle of 512
		   byte USB buffers
		*/

		fb += 64; /* 128 bytes = 64 int16_t */
	}

	return 0;
}

bool
Push2::redraw ()
{
	string tc_clock_text;
	string bbt_clock_text;

	if (session) {
		framepos_t audible = session->audible_frame();
		Timecode::Time TC;
		bool negative = false;

		if (audible < 0) {
			audible = -audible;
			negative = true;
		}

		session->timecode_time (audible, TC);

		TC.negative = TC.negative || negative;

		tc_clock_text = Timecode::timecode_format_time(TC);

		Timecode::BBT_Time bbt = session->tempo_map().bbt_at_frame (audible);
		char buf[16];

#define BBT_BAR_CHAR "|"

		if (negative) {
			snprintf (buf, sizeof (buf), "-%03" PRIu32 BBT_BAR_CHAR "%02" PRIu32 BBT_BAR_CHAR "%04" PRIu32,
			          bbt.bars, bbt.beats, bbt.ticks);
		} else {
			snprintf (buf, sizeof (buf), " %03" PRIu32 BBT_BAR_CHAR "%02" PRIu32 BBT_BAR_CHAR "%04" PRIu32,
			          bbt.bars, bbt.beats, bbt.ticks);
		}

		bbt_clock_text = buf;
	}

	bool dirty = false;

	if (tc_clock_text != tc_clock_layout->get_text()) {
		dirty = true;
		tc_clock_layout->set_text (tc_clock_text);
	}

	if (bbt_clock_text != tc_clock_layout->get_text()) {
		dirty = true;
		bbt_clock_layout->set_text (bbt_clock_text);
	}

	string mid_text;

	for (int n = 0; n < 8; ++n) {
		if (stripable[n]) {
			mid_text = short_version (stripable[n]->name(), 10);
			if (mid_text != mid_layout[n]->get_text()) {
				mid_layout[n]->set_text (mid_text);
				dirty = true;
			}
		}
	}

	if (!dirty) {
		return false;
	}

	context->set_source_rgb (0.764, 0.882, 0.882);
	context->rectangle (0, 0, 960, 160);
	context->fill ();
	context->set_source_rgb (0.23, 0.0, 0.349);
	context->move_to (650, 25);
	tc_clock_layout->update_from_cairo_context (context);
	tc_clock_layout->show_in_cairo_context (context);
	context->move_to (650, 60);
	bbt_clock_layout->update_from_cairo_context (context);
	bbt_clock_layout->show_in_cairo_context (context);

	for (int n = 0; n < 8; ++n) {
		context->move_to (10 + (n*120), 2);
		upper_layout[n]->update_from_cairo_context (context);
		upper_layout[n]->show_in_cairo_context (context);
	}

	for (int n = 0; n < 8; ++n) {
		context->move_to (10 + (n*120), 140);
		lower_layout[n]->update_from_cairo_context (context);
		lower_layout[n]->show_in_cairo_context (context);
	}

	for (int n = 0; n < 8; ++n) {
		if (stripable[n] && stripable[n]->presentation_info().selected()) {
			context->rectangle (10 + (n*120) - 5, 115, 120, 22);
			context->set_source_rgb (1.0, 0.737, 0.172);
			context->fill();
		}
		context->set_source_rgb (0.0, 0.0, 0.0);
		context->move_to (10 + (n*120), 120);
		mid_layout[n]->update_from_cairo_context (context);
		mid_layout[n]->show_in_cairo_context (context);
	}

	/* render clock */
	/* render foo */
	/* render bar */

	return true;
}

bool
Push2::vblank ()
{
	int transferred = 0;
	const int timeout_msecs = 1000;
	int err;

	if ((err = libusb_bulk_transfer (handle, 0x01, frame_header, sizeof (frame_header), &transferred, timeout_msecs))) {
		return false;
	}

	if (redraw()) {
		/* things changed */
		blit_to_device_frame_buffer ();
	}

	if ((err = libusb_bulk_transfer (handle, 0x01, (uint8_t*) device_frame_buffer , 2 * rows * pixels_per_row, &transferred, timeout_msecs))) {
		return false;
	}

	return true;
}

int
Push2::set_active (bool yn)
{
	DEBUG_TRACE (DEBUG::Push2, string_compose("Push2Protocol::set_active init with yn: '%1'\n", yn));

	if (yn == active()) {
		return 0;
	}

	if (yn) {

		/* start event loop */

		BaseUI::run ();

		if (open ()) {
			DEBUG_TRACE (DEBUG::Push2, "device open failed\n");
			close ();
			return -1;
		}

		/* Connect input port to event loop */

		AsyncMIDIPort* asp;

		asp = dynamic_cast<AsyncMIDIPort*> (_input_port);
		asp->xthread().set_receive_handler (sigc::bind (sigc::mem_fun (this, &Push2::midi_input_handler), _input_port));
		asp->xthread().attach (main_loop()->get_context());

		connect_session_signals ();

		/* set up periodic task used to push a frame buffer to the
		 * device (25fps). The device can handle 60fps, but we don't
		 * need that frame rate.
		 */

		Glib::RefPtr<Glib::TimeoutSource> vblank_timeout = Glib::TimeoutSource::create (40); // milliseconds
		vblank_connection = vblank_timeout->connect (sigc::mem_fun (*this, &Push2::vblank));
		vblank_timeout->attach (main_loop()->get_context());


		Glib::RefPtr<Glib::TimeoutSource> periodic_timeout = Glib::TimeoutSource::create (1000); // milliseconds
		periodic_connection = periodic_timeout->connect (sigc::mem_fun (*this, &Push2::periodic));
		periodic_timeout->attach (main_loop()->get_context());

		init_buttons (true);
		init_touch_strip ();
		switch_bank (0);

	} else {

		stop ();

	}

	ControlProtocol::set_active (yn);

	DEBUG_TRACE (DEBUG::Push2, string_compose("Push2Protocol::set_active done with yn: '%1'\n", yn));

	return 0;
}

void
Push2::init_touch_strip ()
{
	MidiByteArray msg (9, 0xf0, 0x00, 0x21, 0x1d, 0x01, 0x01, 0x17, 0x00, 0xf7);
	/* flags are the final byte (ignore end-of-sysex */

	/* show bar, not point
	   autoreturn to center
	   bar starts at center
	*/
	msg[7] = (1<<4) | (1<<5) | (1<<6);
	write (msg);
}

void
Push2::write (const MidiByteArray& data)
{
	/* immediate delivery */
	_output_port->write (&data[0], data.size(), 0);
}

bool
Push2::midi_input_handler (IOCondition ioc, MIDI::Port* port)
{
	if (ioc & ~IO_IN) {
		DEBUG_TRACE (DEBUG::Push2, "MIDI port closed\n");
		return false;
	}

	if (ioc & IO_IN) {

		// DEBUG_TRACE (DEBUG::Push2, string_compose ("something happend on  %1\n", port->name()));

		AsyncMIDIPort* asp = dynamic_cast<AsyncMIDIPort*>(port);
		if (asp) {
			asp->clear ();
		}

		//DEBUG_TRACE (DEBUG::Push2, string_compose ("data available on %1\n", port->name()));
		framepos_t now = AudioEngine::instance()->sample_time();
		port->parse (now);
	}

	return true;
}

bool
Push2::periodic ()
{
	return true;
}

void
Push2::connect_to_parser ()
{
	DEBUG_TRACE (DEBUG::Push2, string_compose ("Connecting to signals on port %2\n", _input_port->name()));

	MIDI::Parser* p = _input_port->parser();

	/* Incoming sysex */
	p->sysex.connect_same_thread (*this, boost::bind (&Push2::handle_midi_sysex, this, _1, _2, _3));
	/* V-Pot messages are Controller */
	p->controller.connect_same_thread (*this, boost::bind (&Push2::handle_midi_controller_message, this, _1, _2));
	/* Button messages are NoteOn */
	p->note_on.connect_same_thread (*this, boost::bind (&Push2::handle_midi_note_on_message, this, _1, _2));
	/* Button messages are NoteOn but libmidi++ sends note-on w/velocity = 0 as note-off so catch them too */
	p->note_off.connect_same_thread (*this, boost::bind (&Push2::handle_midi_note_on_message, this, _1, _2));
	/* Fader messages are Pitchbend */
	p->channel_pitchbend[0].connect_same_thread (*this, boost::bind (&Push2::handle_midi_pitchbend_message, this, _1, _2));
}

void
Push2::handle_midi_sysex (MIDI::Parser&, MIDI::byte* raw_bytes, size_t sz)
{
	DEBUG_TRACE (DEBUG::Push2, string_compose ("Sysex, %1 bytes\n", sz));
}

void
Push2::handle_midi_controller_message (MIDI::Parser&, MIDI::EventTwoBytes* ev)
{
	DEBUG_TRACE (DEBUG::Push2, string_compose ("CC %1 (value %2)\n", (int) ev->controller_number, (int) ev->value));

	CCButtonMap::iterator b = cc_button_map.find (ev->controller_number);

	if (ev->value) {
		/* any press cancels any pending long press timeouts */
		for (set<ButtonID>::iterator x = buttons_down.begin(); x != buttons_down.end(); ++x) {
			Button* bb = id_button_map[*x];
			bb->timeout_connection.disconnect ();
		}
	}

	if (b != cc_button_map.end()) {

		Button* button = b->second;

		if (ev->value) {
			buttons_down.insert (button->id);
			start_press_timeout (*button, button->id);
		} else {
			buttons_down.erase (button->id);
			button->timeout_connection.disconnect ();
		}


		set<ButtonID>::iterator c = consumed.find (button->id);

		if (c == consumed.end()) {
			if (ev->value == 0) {
				(this->*button->release_method)();
			} else {
				(this->*button->press_method)();
			}
		} else {
			DEBUG_TRACE (DEBUG::Push2, "button was consumed, ignored\n");
			consumed.erase (c);
		}

	} else {

		/* encoder/vpot */

		int delta = ev->value;

		if (delta > 63) {
			delta = -(128 - delta);
		}

		switch (ev->controller_number) {
		case 71:
			strip_vpot (0, delta);
			break;
		case 72:
			strip_vpot (1, delta);
			break;
		case 73:
			strip_vpot (2, delta);
			break;
		case 74:
			strip_vpot (3, delta);
			break;
		case 75:
			strip_vpot (4, delta);
			break;
		case 76:
			strip_vpot (5, delta);
			break;
		case 77:
			strip_vpot (6, delta);
			break;
		case 78:
			strip_vpot (7, delta);
			break;

			/* left side pair */
		case 14:
			strip_vpot (8, delta);
			break;
		case 15:
			other_vpot (1, delta);
			break;

			/* right side */
		case 79:
			other_vpot (2, delta);
			break;
		}
	}
}

void
Push2::handle_midi_note_on_message (MIDI::Parser&, MIDI::EventTwoBytes* ev)
{
	DEBUG_TRACE (DEBUG::Push2, string_compose ("Note On %1 (velocity %2)\n", (int) ev->note_number, (int) ev->velocity));

	switch (ev->note_number) {
	case 0:
		strip_vpot_touch (0, ev->velocity > 64);
		break;
	case 1:
		strip_vpot_touch (1, ev->velocity > 64);
		break;
	case 2:
		strip_vpot_touch (2, ev->velocity > 64);
		break;
	case 3:
		strip_vpot_touch (3, ev->velocity > 64);
		break;
	case 4:
		strip_vpot_touch (4, ev->velocity > 64);
		break;
	case 5:
		strip_vpot_touch (5, ev->velocity > 64);
		break;
	case 6:
		strip_vpot_touch (6, ev->velocity > 64);
		break;
	case 7:
		strip_vpot_touch (7, ev->velocity > 64);
		break;

		/* left side */
	case 10:
		other_vpot_touch (0, ev->velocity > 64);
		break;
	case 9:
		other_vpot_touch (1, ev->velocity > 64);
		break;

		/* right side */
	case 8:
		other_vpot_touch (3, ev->velocity > 64);
		break;

		/* touch strip */
	case 12:
		if (ev->velocity < 64) {
			transport_stop ();
		}
		break;
	}
}

void
Push2::handle_midi_note_off_message (MIDI::Parser&, MIDI::EventTwoBytes* ev)
{
	DEBUG_TRACE (DEBUG::Push2, string_compose ("Note Off %1 (velocity %2)\n", (int) ev->note_number, (int) ev->velocity));
}

void
Push2::handle_midi_pitchbend_message (MIDI::Parser&, MIDI::pitchbend_t pb)
{
	if (!session) {
		return;
	}

	float speed;

	/* range of +1 .. -1 */
	speed = ((int32_t) pb - 8192) / 8192.0;
	/* convert to range of +3 .. -3 */
	session->request_transport_speed (speed * 3.0);
}

void
Push2::thread_init ()
{
	struct sched_param rtparam;

	pthread_set_name (event_loop_name().c_str());

	PBD::notify_event_loops_about_thread_creation (pthread_self(), event_loop_name(), 2048);
	ARDOUR::SessionEvent::create_per_thread_pool (event_loop_name(), 128);

	memset (&rtparam, 0, sizeof (rtparam));
	rtparam.sched_priority = 9; /* XXX should be relative to audio (JACK) thread */

	if (pthread_setschedparam (pthread_self(), SCHED_FIFO, &rtparam) != 0) {
		// do we care? not particularly.
	}
}

void
Push2::connect_session_signals()
{
	// receive routes added
	//session->RouteAdded.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&MackieControlProtocol::notify_routes_added, this, _1), this);
	// receive VCAs added
	//session->vca_manager().VCAAdded.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&Push2::notify_vca_added, this, _1), this);

	// receive record state toggled
	session->RecordStateChanged.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&Push2::notify_record_state_changed, this), this);
	// receive transport state changed
	session->TransportStateChange.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&Push2::notify_transport_state_changed, this), this);
	session->TransportLooped.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&Push2::notify_loop_state_changed, this), this);
	// receive punch-in and punch-out
	Config->ParameterChanged.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&Push2::notify_parameter_changed, this, _1), this);
	session->config.ParameterChanged.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&Push2::notify_parameter_changed, this, _1), this);
	// receive rude solo changed
	session->SoloActive.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&Push2::notify_solo_active_changed, this, _1), this);
}

void
Push2::notify_record_state_changed ()
{
	IDButtonMap::iterator b = id_button_map.find (RecordEnable);

	if (b == id_button_map.end()) {
		return;
	}

	switch (session->record_status ()) {
	case Session::Disabled:
		b->second->set_color (LED::White);
		b->second->set_state (LED::NoTransition);
		break;
	case Session::Enabled:
		b->second->set_color (LED::Red);
		b->second->set_state (LED::Blinking4th);
		break;
	case Session::Recording:
		b->second->set_color (LED::Red);
		b->second->set_state (LED::OneShot24th);
		break;
	}

	write (b->second->state_msg());
}

void
Push2::notify_transport_state_changed ()
{
	Button* b = id_button_map[Play];

	if (session->transport_rolling()) {
		b->set_state (LED::OneShot24th);
		b->set_color (LED::Green);
	} else {

		/* disable any blink on FixedLength from pending edit range op */
		Button* fl = id_button_map[FixedLength];

		fl->set_color (LED::Black);
		fl->set_state (LED::NoTransition);
		write (fl->state_msg());

		b->set_color (LED::White);
		b->set_state (LED::NoTransition);
	}

	write (b->state_msg());
}

void
Push2::notify_loop_state_changed ()
{
}

void
Push2::notify_parameter_changed (std::string param)
{
	IDButtonMap::iterator b;

	if (param == "clicking") {
		if ((b = id_button_map.find (Metronome)) == id_button_map.end()) {
			return;
		}
		if (Config->get_clicking()) {
			b->second->set_state (LED::Blinking4th);
			b->second->set_color (LED::White);
		} else {
			b->second->set_color (LED::White);
			b->second->set_state (LED::NoTransition);
		}
		write (b->second->state_msg ());
	}
}

void
Push2::notify_solo_active_changed (bool yn)
{
	IDButtonMap::iterator b = id_button_map.find (Solo);

	if (b == id_button_map.end()) {
		return;
	}

	if (yn) {
		b->second->set_state (LED::Blinking4th);
		b->second->set_color (LED::Red);
	} else {
		b->second->set_state (LED::NoTransition);
		b->second->set_color (LED::White);
	}

	write (b->second->state_msg());
}

XMLNode&
Push2::get_state()
{
	XMLNode& node (ControlProtocol::get_state());
	XMLNode* child;

	child = new XMLNode (X_("Input"));
	child->add_child_nocopy (_async_in->get_state());
	node.add_child_nocopy (*child);
	child = new XMLNode (X_("Output"));
	child->add_child_nocopy (_async_out->get_state());
	node.add_child_nocopy (*child);

	return node;
}

int
Push2::set_state (const XMLNode & node, int version)
{
	DEBUG_TRACE (DEBUG::Push2, string_compose ("Push2::set_state: active %1\n", active()));

	int retval = 0;

	if (ControlProtocol::set_state (node, version)) {
		return -1;
	}

	XMLNode* child;

	if ((child = node.child (X_("Input"))) != 0) {
		XMLNode* portnode = child->child (Port::state_node_name.c_str());
		if (portnode) {
			_async_in->set_state (*portnode, version);
		}
	}

	if ((child = node.child (X_("Output"))) != 0) {
		XMLNode* portnode = child->child (Port::state_node_name.c_str());
		if (portnode) {
			_async_out->set_state (*portnode, version);
		}
	}

	return retval;
}

void
Push2::switch_bank (uint32_t base)
{
	if (!session) {
		return;
	}

	stripable_connections.drop_connections ();

	/* try to get the first stripable for the requested bank */

	stripable[0] = session->get_remote_nth_stripable (base, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));

	if (!stripable[0]) {
		return;
	}

	/* at least one stripable in this bank */
	bank_start = base;

	stripable[1] = session->get_remote_nth_stripable (base+1, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));
	stripable[2] = session->get_remote_nth_stripable (base+2, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));
	stripable[3] = session->get_remote_nth_stripable (base+3, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));
	stripable[4] = session->get_remote_nth_stripable (base+4, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));
	stripable[5] = session->get_remote_nth_stripable (base+5, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));
	stripable[6] = session->get_remote_nth_stripable (base+6, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));
	stripable[7] = session->get_remote_nth_stripable (base+7, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));


	for (int n = 0; n < 8; ++n) {
		if (!stripable[n]) {
			continue;
		}

		/* stripable goes away? refill the bank, starting at the same point */

		stripable[n]->DropReferences.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&Push2::switch_bank, this, bank_start), this);
		boost::shared_ptr<AutomationControl> sc = stripable[n]->solo_control();
		if (sc) {
			sc->Changed.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&Push2::solo_change, this, n), this);
		}

		boost::shared_ptr<AutomationControl> mc = stripable[n]->mute_control();
		if (mc) {
			mc->Changed.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&Push2::mute_change, this, n), this);
		}

		stripable[n]->presentation_info().PropertyChanged.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&Push2::stripable_property_change, this, _1, n), this);

		solo_change (n);
		mute_change (n);

	}

	/* master cannot be removed, so no need to connect to going-away signal */
	master = session->master_out ();
}

void
Push2::stripable_property_change (PropertyChange const& what_changed, int which)
{
	if (what_changed.contains (Properties::selected)) {
		/* cancel string, which will cause a redraw on the next update
		 * cycle. The redraw will reflect selected status
		 */
		mid_layout[which]->set_text (string());
	}
}

void
Push2::solo_change (int n)
{
	ButtonID bid;

	switch (n) {
	case 0:
		bid = Upper1;
		break;
	case 1:
		bid = Upper2;
		break;
	case 2:
		bid = Upper3;
		break;
	case 3:
		bid = Upper4;
		break;
	case 4:
		bid = Upper5;
		break;
	case 5:
		bid = Upper6;
		break;
	case 6:
		bid = Upper7;
		break;
	case 7:
		bid = Upper8;
		break;
	default:
		return;
	}

	boost::shared_ptr<SoloControl> ac = stripable[n]->solo_control ();
	if (!ac) {
		return;
	}

	Button* b = id_button_map[bid];

	if (ac->soloed()) {
		b->set_color (LED::Green);
	} else {
		b->set_color (LED::Black);
	}

	if (ac->soloed_by_others_upstream() || ac->soloed_by_others_downstream()) {
		b->set_state (LED::Blinking4th);
	} else {
		b->set_state (LED::OneShot24th);
	}

	write (b->state_msg());
}

void
Push2::mute_change (int n)
{
	ButtonID bid;

	if (!stripable[n]) {
		return;
	}

	cerr << "Mute changed on " << n << ' ' << stripable[n]->name() << endl;

	switch (n) {
	case 0:
		bid = Lower1;
		break;
	case 1:
		bid = Lower2;
		break;
	case 2:
		bid = Lower3;
		break;
	case 3:
		bid = Lower4;
		break;
	case 4:
		bid = Lower5;
		break;
	case 5:
		bid = Lower6;
		break;
	case 6:
		bid = Lower7;
		break;
	case 7:
		bid = Lower8;
		break;
	default:
		return;
	}

	boost::shared_ptr<MuteControl> mc = stripable[n]->mute_control ();

	if (!mc) {
		return;
	}

	Button* b = id_button_map[bid];

	if (Config->get_show_solo_mutes() && !Config->get_solo_control_is_listen_control ()) {

		if (mc->muted_by_self ()) {
			/* full mute */
			b->set_color (LED::Blue);
			b->set_state (LED::OneShot24th);
			cerr << "FULL MUTE1\n";
		} else if (mc->muted_by_others_soloing () || mc->muted_by_masters ()) {
			/* this will reflect both solo mutes AND master mutes */
			b->set_color (LED::Blue);
			b->set_state (LED::Blinking4th);
			cerr << "OTHER MUTE1\n";
		} else {
			/* no mute at all */
			b->set_color (LED::Black);
			b->set_state (LED::OneShot24th);
			cerr << "NO MUTE1\n";
		}

	} else {

		if (mc->muted_by_self()) {
			/* full mute */
			b->set_color (LED::Blue);
			b->set_state (LED::OneShot24th);
			cerr << "FULL MUTE2\n";
		} else if (mc->muted_by_masters ()) {
			/* this shows only master mutes, not mute-by-others-soloing */
			b->set_color (LED::Blue);
			b->set_state (LED::Blinking4th);
			cerr << "OTHER MUTE1\n";
		} else {
			/* no mute at all */
			b->set_color (LED::Black);
			b->set_state (LED::OneShot24th);
			cerr << "NO MUTE2\n";
		}
	}

	write (b->state_msg());
}

void
Push2::strip_vpot (int n, int delta)
{
	if (stripable[n]) {
		boost::shared_ptr<AutomationControl> ac = stripable[n]->gain_control();
		if (ac) {
			ac->set_value (ac->get_value() + ((2.0/64.0) * delta), PBD::Controllable::UseGroup);
		}
	}
}

void
Push2::strip_vpot_touch (int n, bool touching)
{
	if (stripable[n]) {
		boost::shared_ptr<AutomationControl> ac = stripable[n]->gain_control();
		if (ac) {
			if (touching) {
				ac->start_touch (session->audible_frame());
			} else {
				ac->stop_touch (true, session->audible_frame());
			}
		}
	}
}

void
Push2::other_vpot (int n, int delta)
{
	switch (n) {
	case 0:
		break;
	case 1:
		break;
	case 2:
		/* master gain control */
		if (master) {
			boost::shared_ptr<AutomationControl> ac = master->gain_control();
			if (ac) {
				ac->set_value (ac->get_value() + ((2.0/64.0) * delta), PBD::Controllable::UseGroup);
			}
		}
		break;
	}
}

void
Push2::other_vpot_touch (int n, bool touching)
{
	switch (n) {
	case 0:
		break;
	case 1:
		break;
	case 2:
		if (master) {
			boost::shared_ptr<AutomationControl> ac = master->gain_control();
			if (ac) {
				if (touching) {
					ac->start_touch (session->audible_frame());
				} else {
					ac->stop_touch (true, session->audible_frame());
				}
			}
		}
	}
}

void
Push2::start_shift ()
{
	cerr << "start shift\n";
	modifier_state = ModifierState (modifier_state | ModShift);
	Button* b = id_button_map[Shift];
	b->set_color (LED::White);
	b->set_state (LED::Blinking16th);
	write (b->state_msg());
}

void
Push2::end_shift ()
{
	if (modifier_state & ModShift) {
		cerr << "end shift\n";
		modifier_state = ModifierState (modifier_state & ~(ModShift));
		Button* b = id_button_map[Shift];
		b->timeout_connection.disconnect ();
		b->set_color (LED::White);
		b->set_state (LED::OneShot24th);
		write (b->state_msg());
	}
}

void
Push2::start_select ()
{
	cerr << "start select\n";
	modifier_state = ModifierState (modifier_state | ModSelect);
	Button* b = id_button_map[Select];
	b->set_color (LED::White);
	b->set_state (LED::Blinking16th);
	write (b->state_msg());
}

void
Push2::end_select ()
{
	if (modifier_state & ModSelect) {
		cerr << "end select\n";
		modifier_state = ModifierState (modifier_state & ~(ModSelect));
		Button* b = id_button_map[Select];
		b->timeout_connection.disconnect ();
		b->set_color (LED::White);
		b->set_state (LED::OneShot24th);
		write (b->state_msg());
	}
}
