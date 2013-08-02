/*
    Copyright (C) 2010 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <stdint.h>

#include <sstream>
#include <sys/time.h>
#include <time.h>

#include "pbd/localtime_r.h"
#include "pbd/timersub.h"

#include "midi++/parser.h"
#include "midi++/manager.h"

#include "midi_tracer.h"
#include "gui_thread.h"
#include "i18n.h"

using namespace Gtk;
using namespace std;
using namespace MIDI;
using namespace Glib;

MidiTracer::MidiTracer ()
	: ArdourWindow (_("MIDI Tracer"))
	, parser (0)
	, line_count_adjustment (200, 1, 2000, 1, 10)
	, line_count_spinner (line_count_adjustment)
	, line_count_label (_("Line history: "))
	, autoscroll (true)
	, show_hex (true)
	, collect (true)
	, show_delta_time (false)
	, _update_queued (0)
	, fifo (1024)
	, buffer_pool ("miditracer", buffer_size, 1024) // 1024 256 byte buffers
	, autoscroll_button (_("Auto-Scroll"))
	, base_button (_("Decimal"))
	, collect_button (_("Enabled"))
	, delta_time_button (_("Delta times"))
{
	Manager::instance()->PortsChanged.connect (_manager_connection, invalidator (*this), boost::bind (&MidiTracer::ports_changed, this), gui_context());

	_last_receipt.tv_sec = 0;
	_last_receipt.tv_usec = 0;

	VBox* vbox = manage (new VBox);
	vbox->set_spacing (4);

	HBox* pbox = manage (new HBox);
	pbox->set_spacing (6);
	pbox->pack_start (*manage (new Label (_("Port:"))), false, false);

	_port_combo.signal_changed().connect (sigc::mem_fun (*this, &MidiTracer::port_changed));
	pbox->pack_start (_port_combo);
	pbox->show_all ();
	vbox->pack_start (*pbox, false, false);

	scroller.add (text);
	vbox->set_border_width (12);
	vbox->pack_start (scroller, true, true);

	text.show ();
	text.set_name ("MidiTracerTextView");
	scroller.show ();
	scroller.set_size_request (400, 400);

	collect_button.set_active (true);
	base_button.set_active (false);
	autoscroll_button.set_active (true);

	line_count_box.set_spacing (6);
	line_count_box.pack_start (line_count_label, false, false);
	line_count_box.pack_start (line_count_spinner, false, false);

	line_count_spinner.show ();
	line_count_label.show ();
	line_count_box.show ();

	HBox* bbox = manage (new HBox);
	bbox->add (line_count_box);
	bbox->add (delta_time_button);
	bbox->add (base_button);
	bbox->add (collect_button);
	bbox->add (autoscroll_button);
	bbox->show ();

	vbox->pack_start (*bbox, false, false);

	add (*vbox);

	base_button.signal_toggled().connect (sigc::mem_fun (*this, &MidiTracer::base_toggle));
	collect_button.signal_toggled().connect (sigc::mem_fun (*this, &MidiTracer::collect_toggle));
	autoscroll_button.signal_toggled().connect (sigc::mem_fun (*this, &MidiTracer::autoscroll_toggle));
	delta_time_button.signal_toggled().connect (sigc::mem_fun (*this, &MidiTracer::delta_toggle));

	base_button.show ();
	collect_button.show ();
	autoscroll_button.show ();

	ports_changed ();
	port_changed ();
}


MidiTracer::~MidiTracer()
{
}

void
MidiTracer::ports_changed ()
{
	string const c = _port_combo.get_active_text ();
	_port_combo.clear ();

	boost::shared_ptr<const Manager::PortList> p = Manager::instance()->get_midi_ports ();
	for (Manager::PortList::const_iterator i = p->begin(); i != p->end(); ++i) {
		_port_combo.append_text ((*i)->name());
	}

	_port_combo.set_active_text (c);
}

void
MidiTracer::port_changed ()
{
	disconnect ();

	Port* p = Manager::instance()->port (_port_combo.get_active_text());

	if (p) {
		p->parser()->any.connect_same_thread (_parser_connection, boost::bind (&MidiTracer::tracer, this, _1, _2, _3));
	}
}

void
MidiTracer::disconnect ()
{
	_parser_connection.disconnect ();
}

void
MidiTracer::tracer (Parser&, byte* msg, size_t len)
{
	stringstream ss;
	struct timeval tv;
	char* buf;
	struct tm now;
	size_t bufsize;
	size_t s;

	gettimeofday (&tv, 0);

	buf = (char *) buffer_pool.alloc ();
	bufsize = buffer_size;

	if (_last_receipt.tv_sec != 0 && show_delta_time) {
		struct timeval delta;
		timersub (&tv, &_last_receipt, &delta);
		s = snprintf (buf, bufsize, "+%02" PRId64 ":%06" PRId64, (int64_t) delta.tv_sec, (int64_t) delta.tv_usec);
		bufsize -= s;
	} else {
		localtime_r (&tv.tv_sec, &now);
		s = strftime (buf, bufsize, "%H:%M:%S", &now);
		bufsize -= s;
		s += snprintf (&buf[s], bufsize, ".%06" PRId64, (int64_t) tv.tv_usec);
		bufsize -= s;
	}

	_last_receipt = tv;

	switch ((eventType) msg[0]&0xf0) {
	case off:
		if (show_hex) {
			s += snprintf (&buf[s], bufsize, "%16s chn %2d %02x %02x\n", "NoteOff", (msg[0]&0xf)+1, (int) msg[1], (int) msg[2]);
		} else {
			s += snprintf (&buf[s], bufsize, "%16s chn %2d %-3d %-3d\n", "NoteOff", (msg[0]&0xf)+1, (int) msg[1], (int) msg[2]);
		}
		break;

	case on:
		if (show_hex) {
			s += snprintf (&buf[s], bufsize, "%16s chn %2d %02x %02x\n", "NoteOn", (msg[0]&0xf)+1, (int) msg[1], (int) msg[2]);
		} else {
			s += snprintf (&buf[s], bufsize, "%16s chn %2d %-3d %-3d\n", "NoteOn", (msg[0]&0xf)+1, (int) msg[1], (int) msg[2]);
		}
		break;

	case polypress:
		if (show_hex) {
			s += snprintf (&buf[s], bufsize, "%16s chn %2d %02x\n", "PolyPressure", (msg[0]&0xf)+1, (int) msg[1]);
		} else {
			s += snprintf (&buf[s], bufsize, "%16s chn %2d %-3d\n", "PolyPressure", (msg[0]&0xf)+1, (int) msg[1]);
		}
		break;

	case MIDI::controller:
		if (show_hex) {
			s += snprintf (&buf[s], bufsize, "%16s chn %2d %02x %02x\n", "Controller", (msg[0]&0xf)+1, (int) msg[1], (int) msg[2]);
		} else {
			s += snprintf (&buf[s], bufsize, "%16s chn %2d %2d %-3d\n", "Controller", (msg[0]&0xf)+1, (int) msg[1], (int) msg[2]);
		}
		break;

	case program:
		if (show_hex) {
			s += snprintf (&buf[s], bufsize, "%16s chn %2d %02x\n", "Program Change", (msg[0]&0xf)+1, (int) msg[1]);
		} else {
			s += snprintf (&buf[s], bufsize, "%16s chn %2d %-3d\n", "Program Change", (msg[0]&0xf)+1, (int) msg[1]);
		}
		break;

	case chanpress:
		if (show_hex) {
			s += snprintf (&buf[s], bufsize, "%16s chn %2d %02x/%-3d\n", "Channel Pressure", (msg[0]&0xf)+1, (int) msg[1], (int) msg[1]);
		} else {
			s += snprintf (&buf[s], bufsize, "%16s chn %2d %02x/%-3d\n", "Channel Pressure", (msg[0]&0xf)+1, (int) msg[1], (int) msg[1]);
		}
		break;

	case MIDI::pitchbend:
		if (show_hex) {
			s += snprintf (&buf[s], bufsize, "%16s chn %2d %02x\n", "Pitch Bend", (msg[0]&0xf)+1, (int) msg[1]);
		} else {
			s += snprintf (&buf[s], bufsize, "%16s chn %2d %-3d\n", "Pitch Bend", (msg[0]&0xf)+1, (int) msg[1]);
		}
		break;

	case MIDI::sysex:
		if (len == 1) {
			switch (msg[0]) {
			case 0xf8:
				s += snprintf (&buf[s], bufsize, "%16s\n", "Clock");
				break;
			case 0xfa:
				s += snprintf (&buf[s], bufsize, "%16s\n", "Start");
				break;
			case 0xfb:
				s += snprintf (&buf[s], bufsize, "%16s\n", "Continue");
				break;
			case 0xfc:
				s += snprintf (&buf[s], bufsize, "%16s\n", "Stop");
				break;
			case 0xfe:
				s += snprintf (&buf[s], bufsize, "%16s\n", "Active Sense");
				break;
			case 0xff:
				s += snprintf (&buf[s], bufsize, "%16s\n", "Reset");
				break;
			default:
				s += snprintf (&buf[s], bufsize, "%16s %02x\n", "Sysex", (int) msg[1]);
				break;
			}

		} else if (len > 5 && msg[0] == 0xf0 && msg[1] == 0x7f && msg[3] == 0x06) {
			/* MMC */
			int cmd = msg[4];
			if (cmd == 0x44 && msg[5] == 0x06 && msg[6] == 0x01) {
				s += snprintf (
					&buf[s], bufsize, " MMC locate to %02d:%02d:%02d:%02d.%02d\n",
					msg[7], msg[8], msg[9], msg[10], msg[11]
					);
			} else {
				std::string name;
				if (cmd == 0x1) {
					name = "STOP";
				} else if (cmd == 0x3) {
					name = "DEFERRED PLAY";
				} else if (cmd == 0x6) {
					name = "RECORD STROBE";
				} else if (cmd == 0x7) {
					name = "RECORD EXIT";
				} else if (cmd == 0x8) {
					name = "RECORD PAUSE";
				}
				if (!name.empty()) {
					s += snprintf (&buf[s], bufsize, " MMC command %s\n", name.c_str());
				} else {
					s += snprintf (&buf[s], bufsize, " MMC command %02x\n", cmd);
				}
			}

		} else if (len == 10 && msg[0] == 0xf0 && msg[1] == 0x7f && msg[9] == 0xf7)  {

			/* MTC full frame */
			s += snprintf (
				&buf[s], bufsize, " MTC full frame to %02d:%02d:%02d:%02d\n", msg[5] & 0x1f, msg[6], msg[7], msg[8]
				);
		} else if (len == 3 && msg[0] == MIDI::position) {

			/* MIDI Song Position */
			uint16_t midi_beats = (uint16_t) msg[1];
			midi_beats |= msg[2];
			s += snprintf (&buf[s], bufsize, "%16s %d\n", "Position", (int) midi_beats);
		} else {

			/* other sys-ex */

			s += snprintf (&buf[s], bufsize, "%16s (%d) = [", "Sysex", (int) len);
			bufsize -= s;

			for (unsigned int i = 0; i < len && bufsize > 3; ++i) {
				if (i > 0) {
					s += snprintf (&buf[s], bufsize, " %02x", msg[i]);
				} else {
					s += snprintf (&buf[s], bufsize, "%02x", msg[i]);
				}
				bufsize -= s;
			}
			s += snprintf (&buf[s], bufsize, "]\n");
		}
		break;

	case MIDI::song:
		s += snprintf (&buf[s], bufsize, "%16s\n", "Song");
		break;

	case MIDI::tune:
		s += snprintf (&buf[s], bufsize, "%16s\n", "Tune");
		break;

	case MIDI::eox:
		s += snprintf (&buf[s], bufsize, "%16s\n", "EOX");
		break;

	case MIDI::timing:
		s += snprintf (&buf[s], bufsize, "%16s\n", "Timing");
		break;

	case MIDI::start:
		s += snprintf (&buf[s], bufsize, "%16s\n", "Start");
		break;

	case MIDI::stop:
		s += snprintf (&buf[s], bufsize, "%16s\n", "Stop");
		break;

	case MIDI::contineu:
		s += snprintf (&buf[s], bufsize, "%16s\n", "Continue");
		break;

	case active:
		s += snprintf (&buf[s], bufsize, "%16s\n", "Active Sense");
		break;

	default:
		s += snprintf (&buf[s], bufsize, "%16s\n", "Unknown");
		break;
	}

	// If you want to append more to the line, uncomment this first
	// bufsize -= s;

	fifo.write (&buf, 1);

	if (g_atomic_int_get (&_update_queued) == 0) {
		gui_context()->call_slot (invalidator (*this), boost::bind (&MidiTracer::update, this));
		g_atomic_int_inc (&_update_queued);
	}
}

void
MidiTracer::update ()
{
	bool updated = false;
	g_atomic_int_dec_and_test (&_update_queued);

	RefPtr<TextBuffer> buf (text.get_buffer());

	int excess = buf->get_line_count() - line_count_adjustment.get_value();

	if (excess > 0) {
		buf->erase (buf->begin(), buf->get_iter_at_line (excess));
	}

	char *str;

	while (fifo.read (&str, 1)) {
		buf->insert (buf->end(), string (str));
		buffer_pool.release (str);
		updated = true;
	}

	if (updated && autoscroll) {
		scroller.get_vadjustment()->set_value (scroller.get_vadjustment()->get_upper());
	}
}

void
MidiTracer::base_toggle ()
{
	show_hex = !base_button.get_active();
}

void
MidiTracer::delta_toggle ()
{
	show_delta_time = delta_time_button.get_active();
}

void
MidiTracer::collect_toggle ()
{
	if (collect_button.get_active ()) {
		port_changed ();
	} else {
		disconnect ();
	}
}

void
MidiTracer::autoscroll_toggle ()
{
	autoscroll = autoscroll_button.get_active ();
}
