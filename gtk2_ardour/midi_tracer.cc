#define __STDC_FORMAT_MACROS 1
#include <stdint.h>

#include <sstream>
#include <sys/time.h>
#include <time.h>

#include "midi++/parser.h"

#include "midi_tracer.h"
#include "gui_thread.h"
#include "i18n.h"

using namespace Gtk;
using namespace std;
using namespace MIDI;
using namespace Glib;

MidiTracer::MidiTracer (const std::string& name, Parser& p)
	: ArdourDialog (string_compose (_("MIDI Trace %1"), name))
	, parser (p)
	, line_count_adjustment (200, 1, 2000, 1, 10)
	, line_count_spinner (line_count_adjustment)
	, line_count_label (_("Store this many lines: "))
	, autoscroll (true)
	, show_hex (true)
	, collect (true)
	, autoscroll_button (_("Auto-Scroll"))
	, base_button (_("Decimal"))
	, collect_button (_("Enabled"))
{
	scroller.add (text);
	get_vbox()->set_border_width (12);
	get_vbox()->pack_start (scroller, true, true);
	
	text.show ();
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

	get_action_area()->add (line_count_box);
	get_action_area()->add (base_button);
	get_action_area()->add(collect_button);
	get_action_area()->add (autoscroll_button);

	base_button.signal_toggled().connect (sigc::mem_fun (*this, &MidiTracer::base_toggle));
	collect_button.signal_toggled().connect (sigc::mem_fun (*this, &MidiTracer::collect_toggle));
	autoscroll_button.signal_toggled().connect (sigc::mem_fun (*this, &MidiTracer::autoscroll_toggle));

	base_button.show ();
	collect_button.show ();
	autoscroll_button.show ();

	connect ();
}

MidiTracer::~MidiTracer()
{
}

void
MidiTracer::connect ()
{
	disconnect ();
	parser.any.connect_same_thread (connection, boost::bind (&MidiTracer::tracer, this, _1, _2, _3));
}

void
MidiTracer::disconnect ()
{
	connection.disconnect ();
}

void
MidiTracer::tracer (Parser&, byte* msg, size_t len)
{
	stringstream ss;
	struct timeval tv;
	char buf[256];
	struct tm now;
	size_t bufsize;
	size_t s;

	gettimeofday (&tv, 0);
	localtime_r (&tv.tv_sec, &now);

	s = strftime (buf, sizeof (buf), "%H:%M:%S", &now);
	bufsize = sizeof (buf) - s;
	s += snprintf (&buf[s], bufsize, ".%-9" PRId64, (int64_t) tv.tv_usec);
	bufsize = sizeof (buf) - s;
	
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
			bufsize = sizeof (buf) - s;
		} else {
			s += snprintf (&buf[s], bufsize, " %16s (%d) = [", "Sysex", (int) len);
			bufsize = sizeof (buf) - s;

			for (unsigned int i = 0; i < len && s < sizeof (buf)-3; ++i) {
				if (i > 0) {
					s += snprintf (&buf[s], bufsize, " %02x", msg[i]);
				} else {
					s += snprintf (&buf[s], bufsize, "%02x", msg[i]);
				}
				bufsize = sizeof (buf) - s;
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
	// bufsize = sizeof (buf) - s;

	gui_context()->call_slot (boost::bind (&MidiTracer::add_string, this, string (buf)));
}

void
MidiTracer::add_string (std::string s)
{
	RefPtr<TextBuffer> buf (text.get_buffer());

	int excess = buf->get_line_count() - line_count_adjustment.get_value();

	if (excess > 0) {
		buf->erase (buf->begin(), buf->get_iter_at_line (excess));
	}

	buf->insert (buf->end(), s);

	if (autoscroll) {
		scroller.get_vadjustment()->set_value (scroller.get_vadjustment()->get_upper());
	}
}

void
MidiTracer::base_toggle ()
{
	show_hex = !base_button.get_active();
}

void
MidiTracer::collect_toggle ()
{
	if (collect_button.get_active ()) {
		connect ();
	} else {
		disconnect ();
	}
}

void
MidiTracer::autoscroll_toggle ()
{
	autoscroll = autoscroll_button.get_active ();
}
