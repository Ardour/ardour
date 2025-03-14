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

#pragma once

#include <atomic>

#include <ytkmm/textview.h>
#include <ytkmm/scrolledwindow.h>
#include <ytkmm/togglebutton.h>
#include <ytkmm/adjustment.h>
#include <ytkmm/spinbutton.h>
#include <ytkmm/label.h>
#include <ytkmm/liststore.h>
#include <ytkmm/comboboxtext.h>
#include <ytkmm/box.h>

#include "pbd/signals.h"
#include "pbd/ringbuffer.h"
#include "pbd/pool.h"

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

protected:
	void on_show ();
	void on_hide ();

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
	std::atomic<int> _update_queued;

	PBD::RingBuffer<char *> fifo;
	PBD::Pool buffer_pool;
	static const size_t buffer_size = 256;

	void tracer (MIDI::Parser&, MIDI::byte*, size_t, MIDI::samplecnt_t);
	void update ();

	Gtk::CheckButton autoscroll_button;
	Gtk::CheckButton base_button;
	Gtk::CheckButton collect_button;
	Gtk::CheckButton delta_time_button;
	Gtk::ComboBox    _midi_port_combo;

	class MidiPortCols : public Gtk::TreeModelColumnRecord
	{
		public:
			MidiPortCols ()
			{
				add (pretty_name);
				add (port_name);
			}
			Gtk::TreeModelColumn<std::string> pretty_name;
			Gtk::TreeModelColumn<std::string> port_name;
	};

	MidiPortCols                 _midi_port_cols;
	Glib::RefPtr<Gtk::ListStore> _midi_port_list;

	void base_toggle ();
	void autoscroll_toggle ();
	void collect_toggle ();
	void delta_toggle ();

	void port_changed ();
	void ports_changed ();
	void disconnect ();
	PBD::ScopedConnection _parser_connection;
	PBD::ScopedConnection _manager_connection;
	std::shared_ptr<MIDI::Parser> _midi_parser;

	std::shared_ptr<ARDOUR::MidiPort> tracer_port;
	std::shared_ptr<ARDOUR::MidiPort> traced_port;

	static unsigned int window_count;
};

