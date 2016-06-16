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
#include "pbd/debug.h"
#include "pbd/failed_constructor.h"

#include "midi++/parser.h"

#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/midiport_manager.h"
#include "ardour/session.h"
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
{
	if (open ()) {
		throw failed_constructor ();
	}

	build_maps ();
}

Push2::~Push2 ()
{
	close ();
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

	device_frame_buffer[0] = new uint16_t[rows*pixels_per_row];
	device_frame_buffer[1] = new uint16_t[rows*pixels_per_row];

	memset (device_frame_buffer[0], 0, sizeof (uint16_t) * rows * pixels_per_row);
	memset (device_frame_buffer[1], 0, sizeof (uint16_t) * rows * pixels_per_row);

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
	AudioEngine::instance()->unregister_port (_async_in);
	AudioEngine::instance()->unregister_port (_async_out);

	_async_in.reset ((ARDOUR::Port*) 0);
	_async_out.reset ((ARDOUR::Port*) 0);
	_input_port = 0;
	_output_port = 0;

	vblank_connection.disconnect ();
	periodic_connection.disconnect ();
	session_connections.drop_connections ();

	if (handle) {
		libusb_release_interface (handle, 0x00);
		libusb_close (handle);
		handle = 0;
	}

	delete [] device_frame_buffer[0];
	device_frame_buffer[0] = 0;

	delete [] device_frame_buffer[1];
	device_frame_buffer[1] = 0;

	return 0;
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
Push2::render ()
{
	/* ensure that all drawing has been done before we fetch pixel data */

	frame_buffer->flush ();

	const int stride = 3840; /* bytes per row for Cairo::FORMAT_ARGB32 */
	const uint8_t* data = frame_buffer->get_data ();

	/* fill frame buffer (320kB) */

	Glib::Threads::Mutex::Lock lm (fb_lock);

	uint16_t* fb = (uint16_t*) device_frame_buffer[device_buffer];

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

			dp += 4;
		}

		/* skip 128 bytes to next line. This is filler, used to avoid line borders occuring in the middle of 512
		   byte USB buffers
		*/

		fb += 64; /* 128 bytes = 64 int16_t */
	}

	/* swap buffers (under lock protection) */
	// device_buffer = (device_buffer ? 0 : 1);

	return 0;
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

	{
		Glib::Threads::Mutex::Lock lm (fb_lock);

		if ((err = libusb_bulk_transfer (handle, 0x01, (uint8_t*) device_frame_buffer[device_buffer] , 2 * rows * pixels_per_row, &transferred, timeout_msecs))) {
			return false;
		}
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

		/* say hello */

		Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create (frame_buffer);
		Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (context);

		layout->set_text ("hello, Ardour");
		Pango::FontDescription fd ("Sans Bold 12");
		layout->set_font_description (fd);

		context->set_source_rgb (0.0, 1.0, 1.0);
		context->rectangle (0, 0, 960, 160);
		context->fill ();
		context->set_source_rgb (0.0, 0.0, 0.0);
		context->rectangle (50, 50, 860, 60);
		context->fill ();
		context->move_to (60, 60);
		context->set_source_rgb ((random()%255) / 255.0, (random()%255) / 255.0, (random()%255) / 255.0);
		layout->update_from_cairo_context (context);
		layout->show_in_cairo_context (context);

		render ();

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

	} else {

		stop ();

	}

	ControlProtocol::set_active (yn);

	DEBUG_TRACE (DEBUG::Push2, string_compose("Push2Protocol::set_active done with yn: '%1'\n", yn));

	return 0;
}

void
Push2::write (const MidiByteArray& data)
{
	cerr << data << endl;
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
	cerr << "sysex, " << sz << " bytes\n";
}

void
Push2::handle_midi_controller_message (MIDI::Parser&, MIDI::EventTwoBytes* ev)
{
	CCButtonMap::iterator b = cc_button_map.find (ev->controller_number);
	if (b != cc_button_map.end()) {
		if (ev->value == 0) {
			(this->*b->second->release_method)();
		} else {
			(this->*b->second->press_method)();
		}
	}
}

void
Push2::handle_midi_note_on_message (MIDI::Parser&, MIDI::EventTwoBytes* ev)
{
	cerr << "note on" << (int) ev->note_number << ", velocity " << (int) ev->velocity << endl;
}

void
Push2::handle_midi_note_off_message (MIDI::Parser&, MIDI::EventTwoBytes* ev)
{
	cerr << "note on" << (int) ev->note_number << ", velocity " << (int) ev->velocity << endl;
}

void
Push2::handle_midi_pitchbend_message (MIDI::Parser&, MIDI::pitchbend_t pb)
{
	cerr << "pitchbend @ " << pb << endl;
}

void
Push2::build_maps ()
{
	/* Pads */

	Pad* pad;

#define MAKE_PAD(x,y,nn) \
	pad = new Pad ((x), (y), (nn)); \
	nn_pad_map.insert (make_pair (pad->extra(), pad)); \
	coord_pad_map.insert (make_pair (pad->coord(), pad));

	MAKE_PAD (0, 1, 93);
	MAKE_PAD (0, 2, 94);
	MAKE_PAD (0, 3, 95);
	MAKE_PAD (0, 4, 96);
	MAKE_PAD (0, 5, 97);
	MAKE_PAD (0, 6, 98);
	MAKE_PAD (0, 7, 90);
	MAKE_PAD (1, 0, 84);
	MAKE_PAD (1, 1, 85);
	MAKE_PAD (1, 2, 86);
	MAKE_PAD (1, 3, 87);
	MAKE_PAD (1, 4, 88);
	MAKE_PAD (1, 5, 89);
	MAKE_PAD (1, 6, 90);
	MAKE_PAD (1, 7, 91);
	MAKE_PAD (2, 0, 76);
	MAKE_PAD (2, 1, 77);
	MAKE_PAD (2, 2, 78);
	MAKE_PAD (2, 3, 79);
	MAKE_PAD (2, 4, 80);
	MAKE_PAD (2, 5, 81);
	MAKE_PAD (2, 6, 82);
	MAKE_PAD (2, 7, 83);
	MAKE_PAD (3, 0, 68);
	MAKE_PAD (3, 1, 69);
	MAKE_PAD (3, 2, 70);
	MAKE_PAD (3, 3, 71);
	MAKE_PAD (3, 4, 72);
	MAKE_PAD (3, 5, 73);
	MAKE_PAD (3, 6, 74);
	MAKE_PAD (3, 7, 75);
	MAKE_PAD (4, 0, 60);
	MAKE_PAD (4, 1, 61);
	MAKE_PAD (4, 2, 62);
	MAKE_PAD (4, 3, 63);
	MAKE_PAD (4, 4, 64);
	MAKE_PAD (4, 5, 65);
	MAKE_PAD (4, 6, 66);
	MAKE_PAD (4, 7, 67);
	MAKE_PAD (5, 0, 52);
	MAKE_PAD (5, 1, 53);
	MAKE_PAD (5, 2, 54);
	MAKE_PAD (5, 3, 56);
	MAKE_PAD (5, 4, 56);
	MAKE_PAD (5, 5, 57);
	MAKE_PAD (5, 6, 58);
	MAKE_PAD (5, 7, 59);
	MAKE_PAD (6, 0, 44);
	MAKE_PAD (6, 1, 45);
	MAKE_PAD (6, 2, 46);
	MAKE_PAD (6, 3, 47);
	MAKE_PAD (6, 4, 48);
	MAKE_PAD (6, 5, 49);
	MAKE_PAD (6, 6, 50);
	MAKE_PAD (6, 7, 51);
	MAKE_PAD (7, 0, 36);
	MAKE_PAD (7, 1, 37);
	MAKE_PAD (7, 2, 38);
	MAKE_PAD (7, 3, 39);
	MAKE_PAD (7, 4, 40);
	MAKE_PAD (7, 5, 41);
	MAKE_PAD (7, 6, 42);
	MAKE_PAD (7, 7, 43);

	/* Now color buttons */

	Button *button;

#define MAKE_COLOR_BUTTON(i,cc) \
	button = new ColorButton ((i), (cc)); \
	cc_button_map.insert (make_pair (button->controller_number(), button)); \
	id_button_map.insert (make_pair (button->id, button));
#define MAKE_COLOR_BUTTON_PRESS(i,cc,p)\
	button = new ColorButton ((i), (cc), (p)); \
	cc_button_map.insert (make_pair (button->controller_number(), button)); \
	id_button_map.insert (make_pair (button->id, button))

	MAKE_COLOR_BUTTON (Upper1, 102);
	MAKE_COLOR_BUTTON (Upper2, 103);
	MAKE_COLOR_BUTTON (Upper3, 104);
	MAKE_COLOR_BUTTON (Upper4, 105);
	MAKE_COLOR_BUTTON (Upper5, 106);
	MAKE_COLOR_BUTTON (Upper6, 107);
	MAKE_COLOR_BUTTON (Upper7, 108);
	MAKE_COLOR_BUTTON (Upper8, 109);
	MAKE_COLOR_BUTTON (Lower1, 21);
	MAKE_COLOR_BUTTON (Lower2, 22);
	MAKE_COLOR_BUTTON (Lower3, 23);
	MAKE_COLOR_BUTTON (Lower4, 24);
	MAKE_COLOR_BUTTON (Lower5, 25);
	MAKE_COLOR_BUTTON (Lower6, 26);
	MAKE_COLOR_BUTTON (Lower7, 27);
	MAKE_COLOR_BUTTON (Mute, 60);
	MAKE_COLOR_BUTTON (Solo, 61);
	MAKE_COLOR_BUTTON (Stop, 29);
	MAKE_COLOR_BUTTON (Fwd32ndT, 43);
	MAKE_COLOR_BUTTON (Fwd32nd,42 );
	MAKE_COLOR_BUTTON (Fwd16thT, 41);
	MAKE_COLOR_BUTTON (Fwd16th, 40);
	MAKE_COLOR_BUTTON (Fwd8thT, 39 );
	MAKE_COLOR_BUTTON (Fwd8th, 38);
	MAKE_COLOR_BUTTON (Fwd4trT, 37);
	MAKE_COLOR_BUTTON (Fwd4tr, 36);
	MAKE_COLOR_BUTTON (Automate, 89);
	MAKE_COLOR_BUTTON_PRESS (RecordEnable, 86, &Push2::button_recenable);
	MAKE_COLOR_BUTTON_PRESS (Play, 85, &Push2::button_play);

#define MAKE_WHITE_BUTTON(i,cc)\
	button = new WhiteButton ((i), (cc)); \
	cc_button_map.insert (make_pair (button->controller_number(), button)); \
	id_button_map.insert (make_pair (button->id, button))
#define MAKE_WHITE_BUTTON_PRESS(i,cc,p)\
	button = new WhiteButton ((i), (cc), (p)); \
	cc_button_map.insert (make_pair (button->controller_number(), button)); \
	id_button_map.insert (make_pair (button->id, button))

	MAKE_WHITE_BUTTON (TapTempo, 3);
	MAKE_WHITE_BUTTON_PRESS (Metronome, 9, &Push2::button_metronome);
	MAKE_WHITE_BUTTON (Setup, 30);
	MAKE_WHITE_BUTTON (User, 59);
	MAKE_WHITE_BUTTON (Delete, 118);
	MAKE_WHITE_BUTTON (AddDevice, 52);
	MAKE_WHITE_BUTTON (Device, 110);
	MAKE_WHITE_BUTTON (Mix, 112);
	MAKE_WHITE_BUTTON (Undo, 119);
	MAKE_WHITE_BUTTON (AddTrack, 53);
	MAKE_WHITE_BUTTON (Browse, 113);
	MAKE_WHITE_BUTTON (Convert, 35);
	MAKE_WHITE_BUTTON (DoubleLoop, 117);
	MAKE_WHITE_BUTTON (Quantize, 116);
	MAKE_WHITE_BUTTON (Duplicate, 88);
	MAKE_WHITE_BUTTON (New, 87);
	MAKE_WHITE_BUTTON (FixedLength, 90);
	MAKE_WHITE_BUTTON_PRESS (Up, 46, &Push2::button_up);
	MAKE_WHITE_BUTTON_PRESS (Right, 45, &Push2::button_right);
	MAKE_WHITE_BUTTON_PRESS (Down, 47, &Push2::button_down);
	MAKE_WHITE_BUTTON_PRESS (Left, 44, &Push2::button_left);
	MAKE_WHITE_BUTTON_PRESS (Repeat, 56, &Push2::button_repeat);
	MAKE_WHITE_BUTTON (Accent, 57);
	MAKE_WHITE_BUTTON (Scale, 58);
	MAKE_WHITE_BUTTON (Layout, 31);
	MAKE_WHITE_BUTTON (OctaveUp, 55);
	MAKE_WHITE_BUTTON (PageRight, 63);
	MAKE_WHITE_BUTTON (OctaveDown, 54);
	MAKE_WHITE_BUTTON (PageLeft, 62);
	MAKE_WHITE_BUTTON (Shift, 49);
	MAKE_WHITE_BUTTON (Select, 48);
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

	b->second->set_color (LED::Red);

	switch (session->record_status ()) {
	case Session::Disabled:
		b->second->set_state (LED::Off);
		break;
	case Session::Enabled:
		b->second->set_state (LED::Blinking4th);
		break;
	case Session::Recording:
		b->second->set_state (LED::OneShot24th);
		break;
	}

	write (b->second->state_msg());
}

void
Push2::notify_transport_state_changed ()
{
	IDButtonMap::iterator b = id_button_map.find (Play);

	if (b == id_button_map.end()) {
		return;
	}

	if (session->transport_rolling()) {
		b->second->set_state (LED::OneShot24th);
		b->second->set_color (LED::Blue);
	} else {
		b->second->set_state (LED::Off);
	}

	write (b->second->state_msg());
}

void
Push2::notify_loop_state_changed ()
{
}

void
Push2::notify_parameter_changed (std::string)
{
}

void
Push2::notify_solo_active_changed (bool yn)
{
	IDButtonMap::iterator b = id_button_map.find (Solo);

	if (b == id_button_map.end()) {
		return;
	}

	if (yn) {
		b->second->set_state (LED::Blinking24th);
	} else {
		b->second->set_state (LED::Off);
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
