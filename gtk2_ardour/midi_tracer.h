#ifndef __ardour_gtk_midi_tracer_h__
#define __ardour_gtk_midi_tracer_h__

#include <gtkmm/textview.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/label.h>

#include "pbd/signals.h"
#include "midi++/types.h"
#include "ardour_dialog.h"

namespace MIDI {
	class Parser;
}

class MidiTracer : public ArdourDialog
{
  public:
	MidiTracer (const std::string&, MIDI::Parser&);
	~MidiTracer();

  private:
	MIDI::Parser& parser;
	Gtk::TextView text;
	Gtk::ScrolledWindow scroller;
	Gtk::Adjustment line_count_adjustment;
	Gtk::SpinButton line_count_spinner;
	Gtk::Label line_count_label;
	Gtk::HBox line_count_box;

	bool autoscroll;
	bool show_hex;
	bool collect;

	void tracer (MIDI::Parser&, MIDI::byte*, size_t);
	void add_string (std::string);
	
	Gtk::ToggleButton autoscroll_button;
	Gtk::ToggleButton base_button;
	Gtk::ToggleButton collect_button;

	void base_toggle ();
	void autoscroll_toggle ();
	void collect_toggle ();

	void connect ();
	void disconnect ();
	PBD::Connection connection;
};

#endif /* __ardour_gtk_midi_tracer_h__ */
