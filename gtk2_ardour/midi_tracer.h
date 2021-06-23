/*
 * Copyright (C) 2010-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_gtk_midi_tracer_h__
#define __ardour_gtk_midi_tracer_h__

#include <gtkmm/textview.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/label.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/box.h>

#include "pbd/signals.h"
#include "pbd/ringbuffer.h"
#include "pbd/pool.h"
#include "pbd/g_atomic_compat.h"

#include "midi++/types.h"
#include "ardour_window.h"

namespace MIDI {
	class Parser;
}

namespace ARDOUR {
	class MidiPort;
}

class MidiTracer : public ArdourWindow
{
public:
	MidiTracer ();
	~MidiTracer();

private:
	Gtk::TextView text;
	Gtk::ScrolledWindow scroller;
	Gtk::Adjustment line_count_adjustment;
	Gtk::SpinButton line_count_spinner;
	Gtk::Label line_count_label;
	Gtk::HBox line_count_box;
	MIDI::samplecnt_t _last_receipt;

	bool autoscroll;
	bool show_hex;
	bool show_delta_time;

	/** Incremented when an update is requested, decremented when one is handled; hence
	 *  equal to 0 when an update is not queued.  May temporarily be negative if a
	 *  update is handled before it was noted that it had just been queued.
	 */
	GATOMIC_QUAL gint _update_queued;

	PBD::RingBuffer<char *> fifo;
	Pool buffer_pool;
	static const size_t buffer_size = 256;

	void tracer (MIDI::Parser&, MIDI::byte*, size_t, MIDI::samplecnt_t);
	void update ();

	Gtk::CheckButton autoscroll_button;
	Gtk::CheckButton base_button;
	Gtk::CheckButton collect_button;
	Gtk::CheckButton delta_time_button;
	Gtk::ComboBoxText _port_combo;

	void base_toggle ();
	void autoscroll_toggle ();
	void collect_toggle ();
	void delta_toggle ();

	void port_changed ();
	void ports_changed ();
	void disconnect ();
	PBD::ScopedConnection _parser_connection;
	PBD::ScopedConnection _manager_connection;
	MIDI::Parser my_parser;

	boost::shared_ptr<ARDOUR::Port>	tracer_port;
	boost::shared_ptr<ARDOUR::MidiPort> traced_port;

	static unsigned int window_count;
};

#endif /* __ardour_gtk_midi_tracer_h__ */
