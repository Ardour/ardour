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

#include <cstdlib>
#include <ctime>

#include "pbd/compose.h"
#include "pbd/i18n.h"

#include "ardour/beatbox.h"
#include "ardour/session.h"
#include "ardour/smf_source.h"
#include "ardour/source_factory.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/utils.h"

#include "canvas/canvas.h"
#include "canvas/grid.h"
#include "canvas/box.h"
#include "canvas/rectangle.h"
#include "canvas/step_button.h"
#include "canvas/text.h"
#include "canvas/widget.h"

#include "gtkmm2ext/utils.h"

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
	srandom (time(0));
	setup_pad_canvas ();
	setup_switch_canvas ();
	setup_roll_canvas ();

	tabs.append_page (switch_canvas);
	tabs.append_page (pad_canvas);
	tabs.append_page (roll_canvas);
	tabs.set_show_tabs (false);
	tabs.set_show_border (false);

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

	get_vbox()->set_spacing (12);
	get_vbox()->pack_start (misc_button_box, false, false);
	get_vbox()->pack_start (tabs, false, false);
	get_vbox()->pack_start (quantize_button_box, true, true);


	export_as_region_button.signal_clicked.connect (sigc::mem_fun (*this, &BBGUI::export_as_region));
	get_action_area()->pack_end (export_as_region_button);

	show_all ();
}

BBGUI::~BBGUI ()
{
	for (SwitchRows::iterator s = switch_rows.begin(); s != switch_rows.end(); ++s) {
		delete *s;
	}

	switch_rows.clear ();
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

	for (SwitchRows::iterator sr = switch_rows.begin(); sr != switch_rows.end(); ++sr) {
		(*sr)->update (current_switch_column);
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
			(*p)->button->set_highlight (true);
		} else {
			(*p)->button->set_highlight (false);
		}
	}
}

void
BBGUI::pads_off ()
{
	for (Pads::iterator p = pads.begin(); p != pads.end(); ++p) {
		(*p)->button->set_highlight (false);
	}
}

void
BBGUI::switches_off ()
{
#if 0
	for (Switches::iterator s = switches.begin(); s != switches.end(); ++s) {
		(*s)->off ();
	}
#endif
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
	: button (new ArdourCanvas::StepButton (canvas, pad_width, pad_height, 0))
	, _row (row)
	, _col (col)
	, _note (note)
	, _label (txt)
{
}

int
BBGUI::Pad::velocity () const
{
	return button->value();
}

//static std::string show_color (Gtkmm2ext::Color c) { double r, g, b, a; color_to_rgba (c, r, g, b, a); return string_compose ("%1:%2:%3:%4", r, g, b, a); }

void
BBGUI::setup_pad_canvas ()
{
	pad_canvas.set_background_color (Gtkmm2ext::rgba_to_color (0.32, 0.47, 0.89, 1.0));
	pad_grid = new ArdourCanvas::Grid (&pad_canvas);
	pad_canvas.root()->add (pad_grid);

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
			pad_grid->place (p->button, col, row, 1, 1);
			p->button->set_color (c);
			p->button->Event.connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::pad_event), col, row));
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
		if (p->button->value()) {
			bbox->remove_note (pads[row * pad_cols + col]->note(), at);
			p->button->set_value (0);
		} else {
			bbox->add_note (pads[row * pad_cols + col]->note(), 127, at);
			p->button->set_value (64);
			return true;
		}
	}

	return false;
}

void
BBGUI::setup_switch_canvas ()
{
	switch_canvas.set_background_color (Gtkmm2ext::rgba_to_color (0.1, 0.09, 0.12, 1.0));
	switch_vbox = new ArdourCanvas::VBox (&switch_canvas);
	switch_vbox->name = "swvbox";

	size_switches (8, 8);

	switch_canvas.root()->add (switch_vbox);
}

int BBGUI::Switch::switch_width = 50;
int BBGUI::Switch::switch_height = 50;

BBGUI::Switch::Switch (ArdourCanvas::Canvas* canvas, int row, int col, int note, Gtkmm2ext::Color c, std::string const& txt)
	: button (new ArdourCanvas::StepButton (canvas, switch_width, switch_height, c))
	, _row (row)
	, _col (col)
	, _note (note)
	, _label (txt)
	, _on (false)
	, _flashed (false)
{
}

//static std::string show_color (Gtkmm2ext::Color c) { double r, g, b, a; color_to_rgba (c, r, g, b, a); return string_compose ("%1:%2:%3:%4", r, g, b, a); }

BBGUI::SwitchRow::SwitchRow (BBGUI& bbg, ArdourCanvas::Item* parent, int r, int cols)
	: owner (bbg)
	, row (r)
	, clear_row_button (new ArdourWidgets::ArdourButton ("C"))
	, row_note_button (new ArdourWidgets::ArdourDropdown)
	, clear_row_item (new ArdourCanvas::Widget (parent->canvas(), *clear_row_button))
	, row_note_item (new ArdourCanvas::Widget (parent->canvas(), *row_note_button))
{
	/* populate note dropdown */
	for (int n = 0; n < 127; ++n) {
		row_note_button->AddMenuElem (Gtk::Menu_Helpers::MenuElem (print_midi_note (n), sigc::bind (sigc::mem_fun (*this, &BBGUI::SwitchRow::set_note), n)));
	}

#define COMBO_TRIANGLE_WIDTH 25 // ArdourButton _diameter (11) + 2 * arrow-padding (2*2) + 2 * text-padding (2*5)
	set_size_request_to_display_given_text (*row_note_button, "G#-1\n127", COMBO_TRIANGLE_WIDTH, 2);
	note = 130; /* invalid value to force change in set_note() */
	set_note (r + 64);

	switch_grid = new ArdourCanvas::Grid (parent->canvas());
	switch_grid->name = string_compose ("Grid for row %1", r);
	//switch_grid->set_border_width (0);
	//switch_grid->set_col_spacing (0);

	row_note_item->name = string_compose ("row note for %1", r);
	switch_grid->place (row_note_item, 0, r, 2, 1);

	resize (cols);

	parent->add (switch_grid);
}

std::string
BBGUI::SwitchRow::print_midi_note (int n)
{
	return string_compose ("%1\n%2", ParameterDescriptor::midi_note_name (n), n+1);
}

void
BBGUI::SwitchRow::resize (int cols)
{
	const Gtkmm2ext::Color c = Gtkmm2ext::rgba_to_color (0.525, 0.1, 0.0, 1.0);

	switch_grid->remove (clear_row_item);

	Switches::size_type n = switches.size();

	while (n > (Switches::size_type) cols) {
		Switch* s = switches.back ();
		switch_grid->remove (s->button);
		delete s;
		switches.pop_back ();
		--n;
	}

	while (n < (Switches::size_type) cols) {
		Switch* s = new Switch (switch_grid->canvas(), row, n, note, c, string_compose ("%1", note));
		s->button->Event.connect (sigc::bind (sigc::mem_fun (*this, &SwitchRow::switch_event), n));
		s->button->name = string_compose ("switch %1,%2", n, row);
		switch_grid->place (s->button, 2 + n, row, 1, 1);
		switches.push_back (s);
		++n;
	}

	clear_row_item->name = string_compose ("clear for %1", row);
	switch_grid->place (clear_row_item, 2 + cols, row, 1, 1);
}

BBGUI::SwitchRow::~SwitchRow()
{
	drop_switches ();
}

void
BBGUI::SwitchRow::drop_switches ()
{
	for (Switches::iterator s = switches.begin(); s != switches.end(); ++s) {
		delete *s;
	}

	switches.clear ();
}

void
BBGUI::SwitchRow::update (int current_col)
{
	for (Switches::iterator s = switches.begin(); s != switches.end(); ++s) {
		if ((*s)->col() == current_col) {
			(*s)->button->set_highlight (true);
		} else {
			(*s)->button->set_highlight (false);
		}
	}
}

void
BBGUI::size_switches (int cols, int rows)
{
	switch_cols = cols;

	for (int row = 0; row < rows; ++row) {
		SwitchRow*sr = new SwitchRow (*this, switch_vbox, row, cols);
		switch_rows.push_back (sr);
	}
}

bool
BBGUI::SwitchRow::switch_event (GdkEvent* ev, int col)
{
	Timecode::BBT_Time at;

	const int beats_per_bar = owner.bbox->meter_beats();

	at.bars = col / beats_per_bar;
	at.beats = col % beats_per_bar;
	at.ticks = 0;

	at.bars++;
	at.beats++;

	Switch* s = switches[col];

	if (ev->type == GDK_BUTTON_PRESS) {
		/* XXX changes hould be driven by model */
		if (s->button->value()) {
			owner.bbox->remove_note (note, at);
			s->button->set_value (0);
		} else {
			s->button->set_value (64);
			owner.bbox->add_note (note, rint (s->button->value()), at);
		}
		return true;
	} else if (ev->type == GDK_SCROLL) {
		switch (ev->scroll.direction) {
		case GDK_SCROLL_UP:
		case GDK_SCROLL_RIGHT:
			s->button->set_value (s->button->value() + 1);
			break;
		case GDK_SCROLL_DOWN:
		case GDK_SCROLL_LEFT:
			s->button->set_value (s->button->value() - 1);
			break;
		}
		return true;
	}

	return false;
}

void
BBGUI::SwitchRow::set_note (int n)
{
	if (n < 0) {
		n = 0;
	} else if (n > 127) {
		n = 127;
	}

	if (note == n) {
		return;
	}

	int old_note = note;

	note = n;

	owner.bbox->edit_note_number (old_note, note);
	row_note_button->set_text (print_midi_note (note));
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
	std::string path = bbox->session().new_midi_source_path (bbox->owner()->name());

	boost::shared_ptr<SMFSource> src;

	/* caller must check for pre-existing file */

	assert (!path.empty());
	assert (!Glib::file_test (path, Glib::FILE_TEST_EXISTS));

	src = boost::dynamic_pointer_cast<SMFSource>(SourceFactory::createWritable (DataType::MIDI, bbox->session(), path, false, bbox->session().sample_rate()));

	try {
		if (src->create (path)) {
			return;
		}
	} catch (...) {
		return;
	}

	if (!bbox->fill_source (src)) {

		return;
	}

	std::string region_name = region_name_from_path (src->name(), true);

	PBD::PropertyList plist;

	plist.add (ARDOUR::Properties::start, 0);
	plist.add (ARDOUR::Properties::length, src->length (0));
	plist.add (ARDOUR::Properties::name, region_name);
	plist.add (ARDOUR::Properties::layer, 0);
	plist.add (ARDOUR::Properties::whole_file, true);
	plist.add (ARDOUR::Properties::external, false);

	boost::shared_ptr<Region> region = RegionFactory::create (src, plist, true);
}
