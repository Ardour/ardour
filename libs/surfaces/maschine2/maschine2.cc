/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

#include "pbd/compose.h"
#include "pbd/error.h"

#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/session.h"

#include "midi++/port.h"

#include "maschine2.h"

#include "m2_dev_mk2.h"
#include "m2_map_mk2.h"

#include "m2_dev_mikro.h"
#include "m2_map_mikro.h"

#include "canvas.h"

#include "pbd/abstract_ui.cc" // instantiate template, includes i18n

using namespace ARDOUR;
using namespace PBD;
using namespace ArdourSurface;

#if 1 // TEST
#include "gtkmm2ext/colors.h"
#include "canvas/line.h"
#include "canvas/rectangle.h"
#include "canvas/text.h"

#include "layout.h"
#include "ui_knob.h"
#include "ui_menu.h"

class TestLayout : public Maschine2Layout
{
	public:
		TestLayout (Maschine2& m2, Session& s, std::string const & name)
			: Maschine2Layout (m2, s, name)
			, knob (0)
			, menu (0)
		{
			using namespace ArdourCanvas;
			bg = new ArdourCanvas::Rectangle (this);
			bg->set (ArdourCanvas::Rect (0, 0, display_width(), display_height()));
			bg->set_fill_color (0x000000ff);

			upper_line = new Line (this);
			upper_line->set (Duple (0, 16.5), Duple (display_width(), 16.5));
			upper_line->set_outline_color (0xffffffff);

			knob = new Maschine2Knob(&m2, this);
			knob->set_position (Duple (64 + 32, 32));
			if (_session.master_out ()) {
				knob->set_controllable (_session.master_out ()->gain_control());
			}

			std::vector<std::string> strs;
			strs.push_back("T|sg1");
			strs.push_back("Test2");
			strs.push_back("Test3");
			strs.push_back("Test4");
			strs.push_back("Test5");
			menu = new Maschine2Menu(&m2, this, strs);
			menu->set_position (Duple (0, 19));

		}
		Maschine2Knob* get_knob () { return knob; }
		Maschine2Menu* get_menu () { return menu; }
	private:
		ArdourCanvas::Rectangle* bg;
		ArdourCanvas::Line* upper_line;
		Maschine2Knob* knob;
		Maschine2Menu* menu;
};

static TestLayout* tl = NULL;
#endif

Maschine2::Maschine2 (ARDOUR::Session& s)
	: ControlProtocol (s, string (X_("NI Maschine2")))
	, AbstractUI<Maschine2Request> (name())
	, _handle (0)
	, _hw (0)
	, _ctrl (0)
	, _canvas (0)
	, _maschine_type (Maschine)
	, _master_state (MST_NONE)
{
	if (hid_init()) {
		throw Maschine2Exception ("HIDAPI initialization failed");
	}
	run_event_loop ();
}

Maschine2::~Maschine2 ()
{
	stop ();
	hid_exit ();
}

void*
Maschine2::request_factory (uint32_t num_requests)
{
	return request_buffer_factory (num_requests);
}

void
Maschine2::do_request (Maschine2Request* req)
{
	if (req->type == CallSlot) {
		call_slot (MISSING_INVALIDATOR, req->the_slot);
	} else if (req->type == Quit) {
		stop ();
	}
}

int
Maschine2::set_active (bool yn)
{
	if (yn == active()) {
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
Maschine2::get_state()
{
	XMLNode& node (ControlProtocol::get_state());
	return node;
}

int
Maschine2::set_state (const XMLNode & node, int version)
{
	if (ControlProtocol::set_state (node, version)) {
		return -1;
	}
	return 0;
}

int
Maschine2::start ()
{
	_maschine_type = Maschine;
	_handle = hid_open (0x17cc, 0x1140, NULL); // Maschine

#if 0
	if (!_handle) {
		if ((_handle = hid_open (0x17cc, 0x1300, NULL))) {
			_maschine_type = Studio;
		}
	}
#endif

	if (!_handle) {
		if ((_handle = hid_open (0x17cc, 0x1110, NULL))) {
			_maschine_type = Mikro;
		}
	}
	if (!_handle) {
		if ((_handle = hid_open (0x17cc, 0x1200, NULL))) {
			_maschine_type = Mikro;
		}
	}

	if (!_handle) {
		error << _("Cannot find or connect to Maschine2\n");
		return -1;
	}

	hid_set_nonblocking (_handle, 1);

	_midi_out = AudioEngine::instance()->register_output_port (DataType::MIDI, X_("Maschine2 out"), true);
	if (!_midi_out) {
		error << _("Cannot create Maschine2 PAD MIDI Port");
		stop ();
		return -1;
	}

	boost::dynamic_pointer_cast<AsyncMIDIPort>(_midi_out)->set_flush_at_cycle_start (true);
	_output_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_midi_out).get();

	switch (_maschine_type) {
		case Mikro:
			_hw = new Maschine2Mikro ();
			_ctrl = new M2MapMikro ();
			info << _("Maschine2 Mikro control surface intialized");
			break;
		case Maschine:
			_hw = new Maschine2Mk2 ();
			_ctrl = new M2MapMk2 ();
			info << _("Maschine2 control surface intialized");
			break;
		case Studio:
			error << _("Maschine2 Studio is not yet supported");
			stop ();
			return -1;
			break;
	}

	_canvas = new Maschine2Canvas (*this, _hw);
	connect_signals ();

	Glib::RefPtr<Glib::TimeoutSource> write_timeout = Glib::TimeoutSource::create (40);
	write_connection = write_timeout->connect (sigc::mem_fun (*this, &Maschine2::dev_write));
	write_timeout->attach (main_loop()->get_context());

#ifdef PLATFORM_WINDOWS
	Glib::RefPtr<Glib::TimeoutSource> read_timeout = Glib::TimeoutSource::create (20);
#else
	Glib::RefPtr<Glib::TimeoutSource> read_timeout = Glib::TimeoutSource::create (1);
#endif
	read_connection = read_timeout->connect (sigc::mem_fun (*this, &Maschine2::dev_read));
	read_timeout->attach (main_loop ()->get_context());

#if 1 // TEST
	tl = new TestLayout (*this, *session, "test");
	tl->get_menu ()->set_control (_ctrl->encoder (1));
	tl->get_knob ()->set_control (_ctrl->encoder (2));
#endif

	return 0;
}

int
Maschine2::stop ()
{
	read_connection.disconnect ();
	write_connection.disconnect ();

	session_connections.drop_connections ();
	button_connections.drop_connections ();

	if (_handle && _hw) {
		_hw->clear ();
		_hw->write (_handle, NULL);
	}

	hid_close (_handle);
	_handle = 0;

	stop_event_loop ();

	if (_midi_out) {
		AsyncMIDIPort* asp = dynamic_cast<AsyncMIDIPort*> (_output_port);
		asp->drain (10000, 500000);

		AudioEngine::instance()->unregister_port (_midi_out);
		_midi_out.reset ((ARDOUR::Port*) 0);
		_output_port = 0;
	}

	delete _canvas;
	delete _hw;
	delete _ctrl;

	_canvas = 0;
	_hw = 0;
	_ctrl = 0;
	return 0;
}

void
Maschine2::thread_init ()
{
	pthread_set_name (event_loop_name().c_str());
	ARDOUR::SessionEvent::create_per_thread_pool (event_loop_name(), 1024);
	PBD::notify_event_loops_about_thread_creation (pthread_self(), event_loop_name(), 1024);

	struct sched_param rtparam;
	memset (&rtparam, 0, sizeof (rtparam));
	rtparam.sched_priority = 9; /* XXX should be relative to audio (JACK) thread */
	if (pthread_setschedparam (pthread_self(), SCHED_FIFO, &rtparam) != 0) {
		// do we care? not particularly.
	}
}

void
Maschine2::run_event_loop ()
{
	BaseUI::run ();
}

void
Maschine2::stop_event_loop ()
{
	BaseUI::quit ();
}

bool
Maschine2::dev_read ()
{
	_hw->read (_handle, _ctrl);
	return true;
}

bool
Maschine2::dev_write ()
{
	_hw->write (_handle, _ctrl);
	return true;
}

// move to callbacks.c || M2Contols implementation
Maschine2Layout*
Maschine2::current_layout() const
{
#if 1 // TEST
	return tl;
#else
	return NULL;
#endif
}
