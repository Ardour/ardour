/*
    Copyright (C) 2017 Paul Davis

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

#include "pbd/compose.h"
#include "pbd/i18n.h"

#include "ardour/beatbox.h"

#include "beatbox_gui.h"
#include "timers.h"

using namespace ARDOUR;
using namespace Gtkmm2ext;

using std::cerr;
using std::endl;

BBGUI::BBGUI (boost::shared_ptr<BeatBox> bb)
	: ArdourDialog (_("BeatBox"))
	, bbox (bb)
	, step_sequencer_tab_button (_("Steps"))
	, pad_tab_button (_("Pads"))
	, roll_tab_button (_("Roll"))
	, export_as_region_button (_(">Region"))
	, quantize_off (quantize_group, "None")
	, quantize_32nd (quantize_group, "ThirtySecond")
	, quantize_16th (quantize_group, "Sixteenth")
	, quantize_8th (quantize_group, "Eighth")
	, quantize_quarter (quantize_group, "Quarter")
	, quantize_half (quantize_group, "Half")
	, quantize_whole (quantize_group, "Whole")
	, play_button ("Run")
	, clear_button ("Clear")
	, tempo_adjustment (bb->tempo(), 1, 300, 1, 10)
	, tempo_spinner (tempo_adjustment)
{
	setup_pad_canvas ();
	setup_switch_canvas ();
	setup_roll_canvas ();

	tabs.append_page (switch_canvas);
	tabs.append_page (pad_canvas);
	tabs.append_page (roll_canvas);
	tabs.set_show_tabs (false);

	quantize_off.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 0));
	quantize_32nd.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 32));
	quantize_16th.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 16));
	quantize_8th.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 8));
	quantize_quarter.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 4));
	quantize_half.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 2));
	quantize_whole.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 1));

	quantize_button_box.pack_start (quantize_off);
	quantize_button_box.pack_start (quantize_32nd);
	quantize_button_box.pack_start (quantize_16th);
	quantize_button_box.pack_start (quantize_8th);
	quantize_button_box.pack_start (quantize_quarter);
	quantize_button_box.pack_start (quantize_half);
	quantize_button_box.pack_start (quantize_whole);

	play_button.signal_toggled().connect (sigc::mem_fun (*this, &BBGUI::toggle_play));
	clear_button.signal_clicked().connect (sigc::mem_fun (*this, &BBGUI::clear));

	misc_button_box.pack_start (play_button);
	misc_button_box.pack_start (clear_button);
	misc_button_box.pack_start (step_sequencer_tab_button);
	misc_button_box.pack_start (pad_tab_button);
	misc_button_box.pack_start (roll_tab_button);

	step_sequencer_tab_button.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::switch_tabs), &switch_canvas));
	pad_tab_button.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::switch_tabs), &pad_canvas));
	roll_tab_button.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::switch_tabs), &roll_canvas));

	tempo_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &BBGUI::tempo_changed));

	misc_button_box.pack_start (tempo_spinner);

	get_vbox()->pack_start (misc_button_box, false, false);
	get_vbox()->pack_start (tabs, true, true);
	get_vbox()->pack_start (quantize_button_box, true, true);


	export_as_region_button.signal_clicked.connect (sigc::mem_fun (*this, &BBGUI::export_as_region));
	get_action_area()->pack_end (export_as_region_button);

	show_all ();
}

BBGUI::~BBGUI ()
{
}

void
BBGUI::update ()
{
	switch (tabs.get_current_page()) {
	case 0:
		update_steps ();
		break;
	case 1:
		update_pads ();
		break;
	case 2:
		update_roll ();
		break;
	default:
		return;
	}
}

void
BBGUI::update_steps ()
{
	Timecode::BBT_Time bbt;

	if (!bbox->running()) {
		switches_off ();
		return;
	}

	bbt = bbox->get_last_time ();

	int current_switch_column = (bbt.bars - 1) * bbox->meter_beats ();
	current_switch_column += bbt.beats - 1;

	for (Switches::iterator p = switches.begin(); p != switches.end(); ++p) {
		if ((*p)->col() == current_switch_column) {
			(*p)->flash_on ();
		} else {
			(*p)->flash_off ();
		}
	}
}

void
BBGUI::update_roll ()
{
}

void
BBGUI::update_pads ()
{
	Timecode::BBT_Time bbt;

	if (!bbox->running()) {
		pads_off ();
		return;
	}

	bbt = bbox->get_last_time ();

	int current_pad_column = (bbt.bars - 1) * bbox->meter_beats ();
	current_pad_column += bbt.beats - 1;

	for (Pads::iterator p = pads.begin(); p != pads.end(); ++p) {
		if ((*p)->col() == current_pad_column) {
			(*p)->flash_on ();
		} else {
			(*p)->flash_off ();
		}
	}
}

void
BBGUI::pads_off ()
{
	for (Pads::iterator p = pads.begin(); p != pads.end(); ++p) {
		(*p)->off ();
	}
}

void
BBGUI::switches_off ()
{
	for (Switches::iterator s = switches.begin(); s != switches.end(); ++s) {
		(*s)->off ();
	}
}

void
BBGUI::on_map ()
{
	timer_connection = Timers::rapid_connect (sigc::mem_fun (*this, &BBGUI::update));
	ArdourDialog::on_map ();
}

void
BBGUI::on_unmap ()
{
	timer_connection.disconnect ();
	ArdourDialog::on_unmap ();
}
void
BBGUI::switch_tabs (Gtk::Widget* w)
{
	tabs.set_current_page (tabs.page_num (*w));
}

int BBGUI::Pad::pad_width = 80;
int BBGUI::Pad::pad_height = 80;
int BBGUI::Pad::pad_spacing = 6;

BBGUI::Pad::Pad (ArdourCanvas::Canvas* canvas, int row, int col, int note, std::string const& txt)
	: rect (new ArdourCanvas::Rectangle (canvas, ArdourCanvas::Rect (((col+1) * pad_spacing) + (col * (pad_width - pad_spacing)), ((row+1) * pad_spacing) + (row * (pad_height - pad_spacing)),
	                                                                 ((col+1) * pad_spacing) + ((col + 1) * (pad_width - pad_spacing)), ((row+1) * pad_spacing) + ((row + 1) * (pad_height - pad_spacing)))))
	, _row (row)
	, _col (col)
	, _note (note)
	, _label (txt)
	, _on (false)
	, _flashed (false)
{
	canvas->root()->add (rect);
}

//static std::string show_color (Gtkmm2ext::Color c) { double r, g, b, a; color_to_rgba (c, r, g, b, a); return string_compose ("%1:%2:%3:%4", r, g, b, a); }

void
BBGUI::Pad::on ()
{
	_on = true;
	rect->set_fill_color (hsv.lighter(0.2).color());
}

void
BBGUI::Pad::off ()
{
	_on = false;
	_flashed = false;

	rect->set_fill_color (hsv.color());
}

void
BBGUI::Pad::flash_on ()
{
	_flashed = true;
	rect->set_fill_color (hsv.lighter(0.05).color());
}

void
BBGUI::Pad::flash_off ()
{
	_flashed = false;

	if (_on) {
		on ();
	} else {
		off ();
	}
}

void
BBGUI::Pad::set_color (Gtkmm2ext::Color c)
{
	hsv = c;

	if (_flashed) {
		if (_on) {
			flash_on ();
		} else {
			flash_off ();
		}
	} else {
		if (_on) {
			on ();
		} else {
			off ();
		}
	}
}

void
BBGUI::setup_pad_canvas ()
{
	pad_canvas.set_background_color (Gtkmm2ext::rgba_to_color (0.32, 0.47, 0.89, 1.0));
	size_pads (8, 8);
}

void
BBGUI::size_pads (int cols, int rows)
{
	/* XXX 8 x 8 grid */

	for (Pads::iterator p = pads.begin(); p != pads.end(); ++p) {
		delete *p;
	}

	pads.clear ();

	pad_rows = rows;
	pad_cols = cols;

	Gtkmm2ext::Color c = Gtkmm2ext::rgba_to_color (0.525, 0, 0, 1.0);

	for (int row = 0; row < pad_rows; ++row) {

		int note = random() % 128;

		for (int col = 0; col < pad_cols; ++col) {
			Pad* p = new Pad (&pad_canvas, row, col, note, string_compose ("%1", note));
			/* This is the "off" color */
			p->set_color (c);
			p->rect->Event.connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::pad_event), col, row));
			pads.push_back (p);
		}
	}
}

bool
BBGUI::pad_event (GdkEvent* ev, int col, int row)
{
	Pad* p = pads[row*pad_cols + col];
	Timecode::BBT_Time at;

	at.bars = col / bbox->meter_beats();
	at.beats = col % bbox->meter_beats();
	at.ticks = 0;

	at.bars++;
	at.beats++;

	if (ev->type == GDK_BUTTON_PRESS) {
		/* XXX on/off should be done by model changes */
		if (p->is_on()) {
			bbox->remove_note (pads[row * pad_cols + col]->note(), at);
			p->off ();
		} else {
			bbox->add_note (pads[row * pad_cols + col]->note(), 127, at);
			p->on ();
			return true;
		}
	}

	return false;
}

void
BBGUI::setup_switch_canvas ()
{
	switch_canvas.set_background_color (Gtkmm2ext::rgba_to_color (0.1, 0.09, 0.12, 1.0));
	size_switches (8, 8);
}

int BBGUI::Switch::switch_width = 20;
int BBGUI::Switch::switch_height = 40;
int BBGUI::Switch::switch_spacing = 6;

BBGUI::Switch::Switch (ArdourCanvas::Canvas* canvas, int row, int col, int note, std::string const& txt)
	: rect (new ArdourCanvas::Rectangle (canvas, ArdourCanvas::Rect (((col+1) * switch_spacing) + (col * (switch_width - switch_spacing)), ((row+1) * switch_spacing) + (row * (switch_height - switch_spacing)),
	                                                                 ((col+1) * switch_spacing) + ((col + 1) * (switch_width - switch_spacing)), ((row+1) * switch_spacing) + ((row + 1) * (switch_height - switch_spacing)))))
	, _row (row)
	, _col (col)
	, _note (note)
	, _label (txt)
	, _on (false)
	, _flashed (false)
{
	canvas->root()->add (rect);
}

//static std::string show_color (Gtkmm2ext::Color c) { double r, g, b, a; color_to_rgba (c, r, g, b, a); return string_compose ("%1:%2:%3:%4", r, g, b, a); }

void
BBGUI::Switch::on ()
{
	_on = true;
	rect->set_fill_color (hsv.lighter(0.2).color());
}

void
BBGUI::Switch::off ()
{
	_on = false;
	_flashed = false;

	rect->set_fill_color (hsv.color());
}

void
BBGUI::Switch::flash_on ()
{
	_flashed = true;
	rect->set_fill_color (hsv.lighter(0.05).color());
}

void
BBGUI::Switch::flash_off ()
{
	_flashed = false;

	if (_on) {
		on ();
	} else {
		off ();
	}
}

void
BBGUI::Switch::set_color (Gtkmm2ext::Color c)
{
	hsv = c;

	if (_flashed) {
		if (_on) {
			flash_on ();
		} else {
			flash_off ();
		}
	} else {
		if (_on) {
			on ();
		} else {
			off ();
		}
	}
}

void
BBGUI::size_switches (int cols, int rows)
{
	for (Switches::iterator s = switches.begin(); s != switches.end(); ++s) {
		delete *s;
	}

	switches.clear ();

	switch_rows = rows;
	switch_cols = cols;

	Gtkmm2ext::Color c = Gtkmm2ext::rgba_to_color (0.525, 0, 0, 1.0);

	for (int row = 0; row < switch_rows; ++row) {

		int note = random() % 128;

		for (int col = 0; col < switch_cols; ++col) {
			Switch* s = new Switch (&switch_canvas, row, col, note, string_compose ("%1", note));
			/* This is the "off" color */
			s->set_color (c);
			s->rect->Event.connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::switch_event), col, row));
			switches.push_back (s);
		}
	}
}

bool
BBGUI::switch_event (GdkEvent* ev, int col, int row)
{
	Switch* s = switches[row*switch_cols + col];
	Timecode::BBT_Time at;

	at.bars = col / bbox->meter_beats();
	at.beats = col % bbox->meter_beats();
	at.ticks = 0;

	at.bars++;
	at.beats++;

	if (ev->type == GDK_BUTTON_PRESS) {
		/* XXX on/off should be done by model changes */
		if (s->is_on()) {
			bbox->remove_note (switches[row * switch_cols + col]->note(), at);
			s->off ();
		} else {
			bbox->add_note (switches[row * switch_cols + col]->note(), 127, at);
			s->on ();
			return true;
		}
	}

	return false;
}

void
BBGUI::setup_roll_canvas ()
{
}

void
BBGUI::tempo_changed ()
{
	float t = tempo_adjustment.get_value();
	bbox->set_tempo (t);
}

void
BBGUI::set_quantize (int divisor)
{
	bbox->set_quantize (divisor);
}

void
BBGUI::clear ()
{
	bbox->clear ();
}

void
BBGUI::toggle_play ()
{
	if (bbox->running()) {
		bbox->stop ();
	} else {
		bbox->start ();
	}
}

void
BBGUI::export_as_region ()
{
	std::string path;

	path = "/tmp/foo.smf";
	if (!bbox->export_to_path (path)) {
		cerr << "export failed\n";
	} else {
		cerr << "export in " << path << endl;
	}
}

