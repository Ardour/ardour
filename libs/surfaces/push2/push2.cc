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

#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/debug.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "pbd/search_path.h"
#include "pbd/enumwriter.h"

#include "midi++/parser.h"
#include "timecode/time.h"
#include "timecode/bbt_time.h"

#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"
#include "ardour/midiport_manager.h"
#include "ardour/midi_track.h"
#include "ardour/midi_port.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "push2.h"
#include "gui.h"
#include "menu.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;

#include "pbd/abstract_ui.cc" // instantiate template

const int Push2::cols = 960;
const int Push2::rows = 160;
const int Push2::pixels_per_row = 1024;

#define ABLETON 0x2982
#define PUSH2   0x1967

__attribute__((constructor)) static void
register_enums ()
{
	EnumWriter& enum_writer (EnumWriter::instance());
	vector<int> i;
	vector<string> s;

	MusicalMode::Type mode;

#define REGISTER(e) enum_writer.register_distinct (typeid(e).name(), i, s); i.clear(); s.clear()
#define REGISTER_CLASS_ENUM(t,e) i.push_back (t::e); s.push_back (#e)

	REGISTER_CLASS_ENUM (MusicalMode,Dorian);
	REGISTER_CLASS_ENUM (MusicalMode, IonianMajor);
	REGISTER_CLASS_ENUM (MusicalMode, Minor);
	REGISTER_CLASS_ENUM (MusicalMode, HarmonicMinor);
	REGISTER_CLASS_ENUM (MusicalMode, MelodicMinorAscending);
	REGISTER_CLASS_ENUM (MusicalMode, MelodicMinorDescending);
	REGISTER_CLASS_ENUM (MusicalMode, Phrygian);
	REGISTER_CLASS_ENUM (MusicalMode, Lydian);
	REGISTER_CLASS_ENUM (MusicalMode, Mixolydian);
	REGISTER_CLASS_ENUM (MusicalMode, Aeolian);
	REGISTER_CLASS_ENUM (MusicalMode, Locrian);
	REGISTER_CLASS_ENUM (MusicalMode, PentatonicMajor);
	REGISTER_CLASS_ENUM (MusicalMode, PentatonicMinor);
	REGISTER_CLASS_ENUM (MusicalMode, Chromatic);
	REGISTER_CLASS_ENUM (MusicalMode, BluesScale);
	REGISTER_CLASS_ENUM (MusicalMode, NeapolitanMinor);
	REGISTER_CLASS_ENUM (MusicalMode, NeapolitanMajor);
	REGISTER_CLASS_ENUM (MusicalMode, Oriental);
	REGISTER_CLASS_ENUM (MusicalMode, DoubleHarmonic);
	REGISTER_CLASS_ENUM (MusicalMode, Enigmatic);
	REGISTER_CLASS_ENUM (MusicalMode, Hirajoshi);
	REGISTER_CLASS_ENUM (MusicalMode, HungarianMinor);
	REGISTER_CLASS_ENUM (MusicalMode, HungarianMajor);
	REGISTER_CLASS_ENUM (MusicalMode, Kumoi);
	REGISTER_CLASS_ENUM (MusicalMode, Iwato);
	REGISTER_CLASS_ENUM (MusicalMode, Hindu);
	REGISTER_CLASS_ENUM (MusicalMode, Spanish8Tone);
	REGISTER_CLASS_ENUM (MusicalMode, Pelog);
	REGISTER_CLASS_ENUM (MusicalMode, HungarianGypsy);
	REGISTER_CLASS_ENUM (MusicalMode, Overtone);
	REGISTER_CLASS_ENUM (MusicalMode, LeadingWholeTone);
	REGISTER_CLASS_ENUM (MusicalMode, Arabian);
	REGISTER_CLASS_ENUM (MusicalMode, Balinese);
	REGISTER_CLASS_ENUM (MusicalMode, Gypsy);
	REGISTER_CLASS_ENUM (MusicalMode, Mohammedan);
	REGISTER_CLASS_ENUM (MusicalMode, Javanese);
	REGISTER_CLASS_ENUM (MusicalMode, Persian);
	REGISTER_CLASS_ENUM (MusicalMode, Algerian);
	REGISTER (mode);
}

Push2::Push2 (ARDOUR::Session& s)
	: ControlProtocol (s, string (X_("Ableton Push 2")))
	, AbstractUI<Push2Request> (name())
	, handle (0)
	, device_buffer (0)
	, frame_buffer (Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, cols, rows))
	, _modifier_state (None)
	, splash_start (0)
	, _current_layout (0)
	, drawn_layout (0)
	, connection_state (ConnectionState (0))
	, gui (0)
	, _mode (MusicalMode::IonianMajor)
	, _scale_root (0)
	, _root_octave (3)
	, _in_key (true)
	, octave_shift (0)
	, percussion (false)
{
	context = Cairo::Context::create (frame_buffer);

	build_pad_table ();
	build_maps ();

	/* master cannot be removed, so no need to connect to going-away signal */
	master = session->master_out ();

	if (open ()) {
		throw failed_constructor ();
	}

	ControlProtocol::StripableSelectionChanged.connect (selection_connection, MISSING_INVALIDATOR, boost::bind (&Push2::stripable_selection_change, this, _1), this);

	/* catch current selection, if any */
	{
		StripableNotificationListPtr sp (new StripableNotificationList (ControlProtocol::last_selected()));
		stripable_selection_change (sp);
	}

	/* catch arrival and departure of Push2 itself */
	ARDOUR::AudioEngine::instance()->PortRegisteredOrUnregistered.connect (port_reg_connection, MISSING_INVALIDATOR, boost::bind (&Push2::port_registration_handler, this), this);

	/* Catch port connections and disconnections */
	ARDOUR::AudioEngine::instance()->PortConnectedOrDisconnected.connect (port_connection, MISSING_INVALIDATOR, boost::bind (&Push2::connection_handler, this, _1, _2, _3, _4, _5), this);

	/* ports might already be there */
	port_registration_handler ();
}

Push2::~Push2 ()
{
	stop ();
}

void
Push2::port_registration_handler ()
{
	if (_async_in->connected() && _async_out->connected()) {
		/* don't waste cycles here */
		return;
	}

	string input_port_name = X_("Ableton Push 2 MIDI 1 in");
	string output_port_name = X_("Ableton Push 2 MIDI 1 out");
	vector<string> in;
	vector<string> out;

	AudioEngine::instance()->get_ports (string_compose (".*%1", input_port_name), DataType::MIDI, PortFlags (IsPhysical|IsOutput), in);
	AudioEngine::instance()->get_ports (string_compose (".*%1", output_port_name), DataType::MIDI, PortFlags (IsPhysical|IsInput), out);

	if (!in.empty() && !out.empty()) {
		cerr << "Push2: both ports found\n";
		cerr << "\tconnecting to " << in.front() <<  " + " << out.front() << endl;
		if (!_async_in->connected()) {
			AudioEngine::instance()->connect (_async_in->name(), in.front());
		}
		if (!_async_out->connected()) {
			AudioEngine::instance()->connect (_async_out->name(), out.front());
		}
	}
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

	_async_in  = AudioEngine::instance()->register_input_port (DataType::MIDI, X_("Push 2 in"), true);
	_async_out = AudioEngine::instance()->register_output_port (DataType::MIDI, X_("Push 2 out"), true);

	if (_async_in == 0 || _async_out == 0) {
		return -1;
	}

	/* We do not add our ports to the input/output bundles because we don't
	 * want users wiring them by hand. They could use JACK tools if they
	 * really insist on that.
	 */

	_input_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_async_in).get();
	_output_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_async_out).get();

	/* Create a shadow port where, depending on the state of the surface,
	 * we will make pad note on/off events appear. The surface code will
	 * automatically this port to the first selected MIDI track.
	 */

	boost::dynamic_pointer_cast<AsyncMIDIPort>(_async_in)->add_shadow_port (string_compose (_("%1 Pads"), X_("Push 2")), boost::bind (&Push2::pad_filter, this, _1, _2));
	boost::shared_ptr<MidiPort> shadow_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_async_in)->shadow_port();

	if (shadow_port) {

		_output_bundle.reset (new ARDOUR::Bundle (_("Push 2 Pads"), false));

		_output_bundle->add_channel (
			shadow_port->name(),
			ARDOUR::DataType::MIDI,
			session->engine().make_port_name_non_relative (shadow_port->name())
			);
	}

	session->BundleAddedOrRemoved ();

	connect_to_parser ();

	mix_layout = new MixLayout (*this, *session, context);
	scale_layout = new ScaleLayout (*this, *session, context);
	_current_layout = mix_layout;

	return 0;
}

list<boost::shared_ptr<ARDOUR::Bundle> >
Push2::bundles ()
{
	list<boost::shared_ptr<ARDOUR::Bundle> > b;

	if (_output_bundle) {
		b.push_back (_output_bundle);
	}

	return b;
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

	_current_layout = 0;
	drawn_layout = 0;
	delete mix_layout;
	mix_layout = 0;
	delete scale_layout;
	scale_layout = 0;

	if (handle) {
		libusb_release_interface (handle, 0x00);
		libusb_close (handle);
		handle = 0;
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
	                       Quantize, Duplicate, Browse, PageRight, PageLeft, OctaveUp, OctaveDown, Layout, Scale
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
		                           Accent, Note, Session,  };

		for (size_t n = 0; n < sizeof (off_buttons) / sizeof (off_buttons[0]); ++n) {
			Button* b = id_button_map[off_buttons[n]];

			b->set_color (LED::Black);
			b->set_state (LED::OneShot24th);
			write (b->state_msg());
		}
	}

	if (!startup) {
		for (NNPadMap::iterator pi = nn_pad_map.begin(); pi != nn_pad_map.end(); ++pi) {
			Pad* pad = pi->second;

			pad->set_color (LED::Black);
			pad->set_state (LED::OneShot24th);
			write (pad->state_msg());
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
	if (splash_start) {

		/* display splash for 3 seconds */

		if (get_microseconds() - splash_start > 3000000) {
			splash_start = 0;
		} else {
			return false;
		}
	}

	Glib::Threads::Mutex::Lock lm (layout_lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		/* can't get layout, no re-render needed */
		return false;
	}

	bool render_needed = false;

	if (drawn_layout != _current_layout) {
		render_needed = true;
	}

	bool dirty = _current_layout->redraw (context);
	drawn_layout = _current_layout;

	return dirty || render_needed;
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
		set_pad_scale (_scale_root, _root_octave, _mode, _in_key);
		splash ();


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
			_current_layout->strip_vpot (0, delta);
			break;
		case 72:
			_current_layout->strip_vpot (1, delta);
			break;
		case 73:
			_current_layout->strip_vpot (2, delta);
			break;
		case 74:
			_current_layout->strip_vpot (3, delta);
			break;
		case 75:
			_current_layout->strip_vpot (4, delta);
			break;
		case 76:
			_current_layout->strip_vpot (5, delta);
			break;
		case 77:
			_current_layout->strip_vpot (6, delta);
			break;
		case 78:
			_current_layout->strip_vpot (7, delta);
			break;

			/* left side pair */
		case 14:
			other_vpot (8, delta);
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
Push2::handle_midi_note_on_message (MIDI::Parser& parser, MIDI::EventTwoBytes* ev)
{
	DEBUG_TRACE (DEBUG::Push2, string_compose ("Note On %1 (velocity %2)\n", (int) ev->note_number, (int) ev->velocity));

	if (ev->velocity == 0) {
		handle_midi_note_off_message (parser, ev);
		return;
	}

	switch (ev->note_number) {
	case 0:
		_current_layout->strip_vpot_touch (0, ev->velocity > 64);
		break;
	case 1:
		_current_layout->strip_vpot_touch (1, ev->velocity > 64);
		break;
	case 2:
		_current_layout->strip_vpot_touch (2, ev->velocity > 64);
		break;
	case 3:
		_current_layout->strip_vpot_touch (3, ev->velocity > 64);
		break;
	case 4:
		_current_layout->strip_vpot_touch (4, ev->velocity > 64);
		break;
	case 5:
		_current_layout->strip_vpot_touch (5, ev->velocity > 64);
		break;
	case 6:
		_current_layout->strip_vpot_touch (6, ev->velocity > 64);
		break;
	case 7:
		_current_layout->strip_vpot_touch (7, ev->velocity > 64);
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

	if (ev->note_number < 11) {
		return;
	}

	/* Pad */

	NNPadMap::iterator pi = nn_pad_map.find (ev->note_number);

	if (pi == nn_pad_map.end()) {
		return;
	}

	Pad* pad = pi->second;

	if (pad->do_when_pressed == Pad::FlashOn) {
		pad->set_color (LED::White);
		pad->set_state (LED::OneShot24th);
		write (pad->state_msg());
	} else if (pad->do_when_pressed == Pad::FlashOff) {
		pad->set_color (LED::Black);
		pad->set_state (LED::OneShot24th);
		write (pad->state_msg());
	}
}

void
Push2::handle_midi_note_off_message (MIDI::Parser&, MIDI::EventTwoBytes* ev)
{
	DEBUG_TRACE (DEBUG::Push2, string_compose ("Note Off %1 (velocity %2)\n", (int) ev->note_number, (int) ev->velocity));

	if (ev->note_number < 11) {
		/* theoretically related to encoder touch start/end, but
		 * actually they send note on with two different velocity
		 * values (127 & 64).
		 */
		return;
	}

	NNPadMap::iterator pi = nn_pad_map.find (ev->note_number);

	if (pi == nn_pad_map.end()) {
		return;
	}

	Pad* pad = pi->second;

	if (pad->do_when_pressed == Pad::FlashOn) {
		pad->set_color (LED::Black);
		pad->set_state (LED::OneShot24th);
		write (pad->state_msg());
	} else if (pad->do_when_pressed == Pad::FlashOff) {
		pad->set_color (pad->perma_color);
		pad->set_state (LED::OneShot24th);
		write (pad->state_msg());
	}
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

	node.add_property (X_("root"), to_string (_scale_root, std::dec));
	node.add_property (X_("root_octave"), to_string (_root_octave, std::dec));
	node.add_property (X_("in_key"), _in_key ? X_("yes") : X_("no"));
	node.add_property (X_("mode"), enum_2_string (_mode));

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

	XMLProperty const* prop;

	if ((prop = node.property (X_("root"))) != 0) {
		_scale_root = atoi (prop->value());
	}

	if ((prop = node.property (X_("root_octave"))) != 0) {
		_root_octave = atoi (prop->value());
	}

	if ((prop = node.property (X_("in_key"))) != 0) {
		_in_key = string_is_affirmative (prop->value());
	}

	if ((prop = node.property (X_("mode"))) != 0) {
		_mode = (MusicalMode::Type) string_2_enum (prop->value(), _mode);
	}

	return retval;
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
	_modifier_state = ModifierState (_modifier_state | ModShift);
	Button* b = id_button_map[Shift];
	b->set_color (LED::White);
	b->set_state (LED::Blinking16th);
	write (b->state_msg());
}

void
Push2::end_shift ()
{
	if (_modifier_state & ModShift) {
		cerr << "end shift\n";
		_modifier_state = ModifierState (_modifier_state & ~(ModShift));
		Button* b = id_button_map[Shift];
		b->timeout_connection.disconnect ();
		b->set_color (LED::White);
		b->set_state (LED::OneShot24th);
		write (b->state_msg());
	}
}

void
Push2::splash ()
{
	std::string splash_file;

	Searchpath rc (ARDOUR::ardour_data_search_path());
	rc.add_subdirectory_to_paths ("resources");

	if (!find_file (rc, PROGRAM_NAME "-splash.png", splash_file)) {
		cerr << "Cannot find splash screen image file\n";
		throw failed_constructor();
	}

	Cairo::RefPtr<Cairo::ImageSurface> img = Cairo::ImageSurface::create_from_png (splash_file);

	double x_ratio = (double) img->get_width() / (cols - 20);
	double y_ratio = (double) img->get_height() / (rows - 20);
	double scale = min (x_ratio, y_ratio);

	/* background */

	context->set_source_rgb (0.764, 0.882, 0.882);
	context->paint ();

	/* image */

	context->save ();
	context->translate (5, 5);
	context->scale (scale, scale);
	context->set_source (img, 0, 0);
	context->paint ();
	context->restore ();

	/* text */

	Glib::RefPtr<Pango::Layout> some_text = Pango::Layout::create (context);

	Pango::FontDescription fd ("Sans 38");
	some_text->set_font_description (fd);
	some_text->set_text (string_compose ("%1 %2", PROGRAM_NAME, VERSIONSTRING));

	context->move_to (200, 10);
	context->set_source_rgb (0, 0, 0);
	some_text->update_from_cairo_context (context);
	some_text->show_in_cairo_context (context);

	Pango::FontDescription fd2 ("Sans Italic 18");
	some_text->set_font_description (fd2);
	some_text->set_text (_("Ableton Push 2 Support"));

	context->move_to (200, 80);
	context->set_source_rgb (0, 0, 0);
	some_text->update_from_cairo_context (context);
	some_text->show_in_cairo_context (context);

	splash_start = get_microseconds ();
	blit_to_device_frame_buffer ();
}

bool
Push2::pad_filter (MidiBuffer& in, MidiBuffer& out) const
{
	/* This filter is called asynchronously from a realtime process
	   context. It must use atomics to check state, and must not block.
	*/

	bool matched = false;

	for (MidiBuffer::iterator ev = in.begin(); ev != in.end(); ++ev) {
		if ((*ev).is_note_on() || (*ev).is_note_off()) {

			/* encoder touch start/touch end use note
			 * 0-10. touchstrip uses note 12
			 */

			if ((*ev).note() > 10 && (*ev).note() != 12) {

				const int n = (*ev).note ();
				NNPadMap::const_iterator nni = nn_pad_map.find (n);

				if (nni != nn_pad_map.end()) {
					Pad const * pad = nni->second;
					/* shift for output to the shadow port */
					if (pad->filtered >= 0) {
						(*ev).set_note (pad->filtered);
						out.push_back (*ev);
						/* shift back so that the pads light correctly  */
						(*ev).set_note (n);
					} else {
						/* no mapping, don't send event */
					}
				} else {
					out.push_back (*ev);
				}

				matched = true;
			}
		} else if ((*ev).is_pitch_bender() || (*ev).is_aftertouch() || (*ev).is_channel_pressure()) {
			out.push_back (*ev);
		}
	}

	return matched;
}

bool
Push2::connection_handler (boost::weak_ptr<ARDOUR::Port>, std::string name1, boost::weak_ptr<ARDOUR::Port>, std::string name2, bool yn)
{
	DEBUG_TRACE (DEBUG::FaderPort, "FaderPort::connection_handler  start\n");
	if (!_input_port || !_output_port) {
		return false;
	}

	string ni = ARDOUR::AudioEngine::instance()->make_port_name_non_relative (boost::shared_ptr<ARDOUR::Port>(_async_in)->name());
	string no = ARDOUR::AudioEngine::instance()->make_port_name_non_relative (boost::shared_ptr<ARDOUR::Port>(_async_out)->name());

	if (ni == name1 || ni == name2) {
		if (yn) {
			connection_state |= InputConnected;
		} else {
			connection_state &= ~InputConnected;
		}
	} else if (no == name1 || no == name2) {
		if (yn) {
			connection_state |= OutputConnected;
		} else {
			connection_state &= ~OutputConnected;
		}
	} else {
		DEBUG_TRACE (DEBUG::FaderPort, string_compose ("Connections between %1 and %2 changed, but I ignored it\n", name1, name2));
		/* not our ports */
		return false;
	}

	if ((connection_state & (InputConnected|OutputConnected)) == (InputConnected|OutputConnected)) {

		/* XXX this is a horrible hack. Without a short sleep here,
		   something prevents the device wakeup messages from being
		   sent and/or the responses from being received.
		*/

		g_usleep (100000);
                DEBUG_TRACE (DEBUG::FaderPort, "device now connected for both input and output\n");
                // connected ();

	} else {
		DEBUG_TRACE (DEBUG::FaderPort, "Device disconnected (input or output or both) or not yet fully connected\n");
	}

	ConnectionChange (); /* emit signal for our GUI */

	DEBUG_TRACE (DEBUG::FaderPort, "FaderPort::connection_handler  end\n");

	return true; /* connection status changed */
}

boost::shared_ptr<Port>
Push2::output_port()
{
	return _async_out;
}

boost::shared_ptr<Port>
Push2::input_port()
{
	return _async_in;
}

void
Push2::build_pad_table ()
{
	for (int n = 36; n < 100; ++n) {
		pad_map[n] = n + (octave_shift*12);
	}

	PadChange (); /* emit signal */
}

int
Push2::pad_note (int row, int col) const
{
	NNPadMap::const_iterator nni = nn_pad_map.find (36+(row*8)+col);

	if (nni != nn_pad_map.end()) {
		return nni->second->filtered;
	}

	return 0;
}

void
Push2::set_pad_scale (int root, int octave, MusicalMode::Type mode, bool inkey)
{
	MusicalMode m (mode);
	vector<float>::iterator interval;
	int note;
	const int original_root = root;

	interval = m.steps.begin();
	root += (octave*12);
	note = root;

	const int root_start = root;

	set<int> mode_map; /* contains only notes in mode, O(logN) lookup */
	vector<int> mode_vector; /* sorted in note order */

	mode_map.insert (note);
	mode_vector.push_back (note);

	/* build a map of all notes in the mode, from the root to 127 */

	while (note < 128) {

		if (interval == m.steps.end()) {

			/* last distance was the end of the scale,
			   so wrap, adding the next note at one
			   octave above the last root.
			*/

			interval = m.steps.begin();
			root += 12;
			mode_map.insert (root);
			mode_vector.push_back (root);

		} else {
			note = (int) floor (root + (2.0 * (*interval)));
			interval++;
			mode_map.insert (note);
			mode_vector.push_back (note);
		}
	}

	if (inkey) {

		vector<int>::iterator notei;
		int row_offset = 0;
		for (int row = 0; row < 8; ++row) {

			/* Ableton's grid layout wraps the available notes in the scale
			 * by offsetting 3 notes per row (from the bottom)
			 */

			notei = mode_vector.begin();
			notei += row_offset;
			row_offset += 3;

			for (int col = 0; col < 8; ++col) {
				int index = 36 + (row*8) + col;
				Pad* pad = nn_pad_map[index];
				int notenum;
				if (notei != mode_vector.end()) {

					notenum = *notei;
					pad->filtered = notenum;

					if ((notenum % 12) == original_root) {
						pad->set_color (LED::Green);
						pad->perma_color = LED::Green;
					} else {
						pad->set_color (LED::White);
						pad->perma_color = LED::White;
					}

					pad->do_when_pressed = Pad::FlashOff;
					notei++;

				} else {

					pad->set_color (LED::Black);
					pad->do_when_pressed = Pad::Nothing;
					pad->filtered = -1;
				}

				write (pad->state_msg());
			}
		}

	} else {

		/* chromatic: all notes available, but highlight those in the scale */

		for (note = 36; note < 100; ++note) {

			Pad* pad = nn_pad_map[note];

			/* Chromatic: all pads play, half-tone steps. Light
			 * those in the scale, and highlight root notes
			 */

			pad->filtered = root_start + (note - 36);

			if (mode_map.find (note) != mode_map.end()) {

				if ((note % 12) == original_root) {
					pad->set_color (LED::Green);
					pad->perma_color = LED::Green;
				} else {
					pad->set_color (LED::White);
					pad->perma_color = LED::White;
				}

				pad->do_when_pressed = Pad::FlashOff;

			} else {

				/* note is not in mode, turn it off */

				pad->do_when_pressed = Pad::FlashOn;
				pad->set_color (LED::Black);

			}

			write (pad->state_msg());
		}
	}

	PadChange (); /* EMIT SIGNAL */

	/* store state */

	_scale_root = original_root;
	_root_octave = octave;
	_in_key = inkey;
	_mode = mode;
}

void
Push2::set_percussive_mode (bool yn)
{
	if (!yn) {
		cerr << "back to scale\n";
		set_pad_scale (_scale_root, _root_octave, _mode, _in_key);
		percussion = false;
		return;
	}

	int drum_note = 36;

	for (int row = 0; row < 8; ++row) {

		for (int col = 0; col < 4; ++col) {

			int index = 36 + (row*8) + col;
			Pad* pad = nn_pad_map[index];

			pad->filtered = drum_note;
			drum_note++;
		}
	}

	for (int row = 0; row < 8; ++row) {

		for (int col = 4; col < 8; ++col) {

			int index = 36 + (row*8) + col;
			Pad* pad = nn_pad_map[index];

			pad->filtered = drum_note;
			drum_note++;
		}
	}

	percussion = true;

	PadChange (); /* EMIT SIGNAL */
}

Push2Layout*
Push2::current_layout () const
{
	Glib::Threads::Mutex::Lock lm (layout_lock);
	return _current_layout;
}

void
Push2::stripable_selection_change (StripableNotificationListPtr selected)
{
	boost::shared_ptr<MidiPort> pad_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_async_in)->shadow_port();
	boost::shared_ptr<MidiTrack> current_midi_track = current_pad_target.lock();
	boost::shared_ptr<MidiTrack> new_pad_target;

	/* See if there's a MIDI track selected */

	for (StripableNotificationList::iterator si = selected->begin(); si != selected->end(); ++si) {

		new_pad_target = boost::dynamic_pointer_cast<MidiTrack> ((*si).lock());

		if (new_pad_target) {
			break;
		}
	}

	if (new_pad_target) {
		cerr << "new midi pad target " << new_pad_target->name() << endl;
	} else {
		cerr << "no midi pad target\n";
	}

	if (current_midi_track == new_pad_target) {
		/* nothing to do */
		return;
	}

	if (!new_pad_target) {
		/* leave existing connection alone */
		return;
	}

	/* disconnect from pad port, if appropriate */

	if (current_midi_track && pad_port) {
		cerr << "Disconnect pads from " << current_midi_track->name() << endl;
		current_midi_track->input()->disconnect (current_midi_track->input()->nth(0), pad_port->name(), this);
	}

	/* now connect the pad port to this (newly) selected midi
	 * track, if indeed there is one.
	 */

	if (new_pad_target && pad_port) {
		cerr << "Reconnect pads to " << new_pad_target->name() << endl;
		new_pad_target->input()->connect (new_pad_target->input()->nth (0), pad_port->name(), this);
		current_pad_target = new_pad_target;
	} else {
		current_pad_target.reset ();
	}
}

Push2::Button*
Push2::button_by_id (ButtonID bid)
{
	return id_button_map[bid];
}
