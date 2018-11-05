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
#include "ardour/step_sequencer.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/utils.h"

#include "canvas/canvas.h"
#include "canvas/grid.h"
#include "canvas/box.h"
#include "canvas/rectangle.h"
#include "canvas/polygon.h"
#include "canvas/scroll_group.h"
#include "canvas/step_button.h"
#include "canvas/text.h"
#include "canvas/widget.h"

#include "gtkmm2ext/utils.h"

#include "beatbox_gui.h"
#include "timers.h"
#include "ui_config.h"

using namespace ARDOUR;
using namespace Gtkmm2ext;
using namespace ArdourCanvas;

using std::cerr;
using std::endl;

const int _nsteps = 32;
const int _nrows = 8;
const double _step_dimen = 25;
const double _width = _step_dimen * _nsteps;
const double _height = _step_dimen * _nrows;

BBGUI::BBGUI (boost::shared_ptr<BeatBox> bb)
	: ArdourDialog (_("BeatBox"))
	, bbox (bb)
	, horizontal_adjustment (0.0, 0.0, 800.0)
	, vertical_adjustment (0.0, 0.0, 10.0, 400.0)
	, clear_button ("Clear")
	, vscrollbar (vertical_adjustment)

{
	_canvas_viewport = new GtkCanvasViewport (horizontal_adjustment, vertical_adjustment);
	_canvas_viewport->set_size_request (_width, _height + _step_dimen);
	_canvas = _canvas_viewport->canvas();

	_canvas->set_background_color (UIConfiguration::instance().color ("gtk_bright_color"));
	_canvas->use_nsglview ();

	no_scroll_group = new ArdourCanvas::Container (_canvas->root());

	step_indicator_box = new ArdourCanvas::Container (no_scroll_group);

	step_indicator_bg = new ArdourCanvas::Rectangle (step_indicator_box);
	step_indicator_bg->set (Rect (0, 0, _width, _step_dimen));
	step_indicator_bg->set_fill_color (UIConfiguration::instance().color ("gtk_lightest"));
	step_indicator_bg->set_outline (false);

	for (int n = 0; n < _nsteps; ++n) {
		SequencerStepIndicator* ssi = new SequencerStepIndicator (step_indicator_box, n+1);
		ssi->set (Rect (n * _step_dimen, 0, (n+1) * _step_dimen, _step_dimen));
		ssi->set_position (Duple (n * _step_dimen, 0.0));
		ssi->set_fill_color (random());
	}

	v_scroll_group = new ScrollGroup (_canvas->root(), ScrollGroup::ScrollsVertically);
	_canvas->add_scroller (*v_scroll_group);

	_sequencer = new SequencerGrid (v_scroll_group);
	_sequencer->set_position (Duple (0, _step_dimen));
	_sequencer->set_fill_color (UIConfiguration::instance().color ("gtk_contrasting_indicator"));
	_sequencer->Event.connect (sigc::mem_fun (*this, &BBGUI::grid_event));

	srandom (time(0));

	clear_button.signal_clicked().connect (sigc::mem_fun (*this, &BBGUI::clear));

	misc_button_box.pack_start (clear_button);

	canvas_hbox.pack_start (*_canvas_viewport, true, true);
	canvas_hbox.pack_start (vscrollbar, false, false);

	get_vbox()->set_spacing (12);
	get_vbox()->pack_start (misc_button_box, false, false);
	get_vbox()->pack_start (canvas_hbox, true, true);

	start_button.signal_clicked.connect (sigc::mem_fun (*this, &BBGUI::toggle_play));
	get_action_area()->pack_end (start_button);

	export_as_region_button.signal_clicked.connect (sigc::mem_fun (*this, &BBGUI::export_as_region));
	get_action_area()->pack_end (export_as_region_button);

	show_all ();
}

BBGUI::~BBGUI ()
{
}

bool
BBGUI::grid_event (GdkEvent* ev)
{
	bool ret = false;

	cerr << "grid event, type: " << event_type_string (ev->type);
	switch (ev->type) {
	case GDK_MOTION_NOTIFY:
		ret = grid_motion_event (&ev->motion);
		cerr << ' ' << ev->motion.x << ',' << ev->motion.y << " state " << show_gdk_event_state (ev->motion.state);
		break;
	case GDK_BUTTON_PRESS:
		cerr << ' ' << ev->button.x << ',' << ev->button.y << " state " << show_gdk_event_state (ev->button.state);
		ret = grid_button_press_event (&ev->button);
		break;
	case GDK_BUTTON_RELEASE:
		cerr << ' ' << ev->button.x << ',' << ev->button.y << " state " << show_gdk_event_state (ev->button.state);
		ret = grid_button_release_event (&ev->button);
		break;
	case GDK_SCROLL:
		cerr << ' ' << ev->scroll.direction << show_gdk_event_state (ev->scroll.state);
		ret = grid_scroll_event (&ev->scroll);
	default:
		break;
	}
	cerr << endl;

	return ret;
}


bool
BBGUI::grid_motion_event (GdkEventMotion* ev)
{
	return true;
}

bool
BBGUI::grid_button_press_event (GdkEventButton* ev)
{
	set_grab_step (ev->x, ev->y);
	grab_at = std::make_pair (ev->x, ev->y);
	return true;
}

bool
BBGUI::grid_button_release_event (GdkEventButton* ev)
{
	return true;
}

bool
BBGUI::grid_scroll_event (GdkEventScroll* ev)
{
	int step = ev->x / _step_dimen;
	int seq = ev->y / _step_dimen;
	int amt = 0;

	switch (ev->direction) {
	case GDK_SCROLL_UP:
		amt = 1;
		break;
	case GDK_SCROLL_LEFT:
		amt = -1;
		break;
	case GDK_SCROLL_RIGHT:
		amt = 1;
		break;
	case GDK_SCROLL_DOWN:
		amt = -1;
		break;
	}

	if (ev->state & GDK_MOD1_MASK) {
		cerr << "adjust pitch by " << amt << endl;
		adjust_step_pitch (seq, step, amt);
	} else {
		cerr << "adjust velocity by " << amt << endl;
		adjust_step_velocity (seq, step, amt);
	}

	return true;
}

void
BBGUI::set_grab_step (double x, double y)
{
	int step = x / _step_dimen;
	int seq = y / _step_dimen;

	grab_step = std::make_pair (seq, step);
}


void
BBGUI::adjust_step_pitch (int seq, int step, int amt)
{
	bbox->sequencer().adjust_step_pitch (seq, step, amt);
}

void
BBGUI::adjust_step_velocity (int seq, int step, int amt)
{
	bbox->sequencer().adjust_step_velocity (seq, step, amt);
}

void
BBGUI::update ()
{
	update_sequencer ();
}

void
BBGUI::update_sequencer ()
{
	Timecode::BBT_Time bbt;

	if (!bbox->running()) {
		/* do something */
		return;
	}
#if 0
	bbt = bbox->get_last_time ();

	int current_switch_column = (bbt.bars - 1) * bbox->meter_beats ();
	current_switch_column += bbt.beats - 1;

	for (SwitchRows::iterator sr = switch_rows.begin(); sr != switch_rows.end(); ++sr) {
		(*sr)->update (current_switch_column);
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

#if 0
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
#endif

void
BBGUI::clear ()
{
	bbox->clear ();
}

void
BBGUI::toggle_play()
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

SequencerGrid::SequencerGrid (Canvas *c)
	: Rectangle (c)
{
	set (Rect (0, 0, _width, _height));
}

SequencerGrid::SequencerGrid (Item *p)
	: Rectangle (p)
{
	set (Rect (0, 0, _width, _height));
}

void
SequencerGrid::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Rect self (item_to_window (get(), false));
	const Rect draw = self.intersection (area);

	if (!draw) {
		return;
	}

	setup_fill_context (context);
	context->rectangle (draw.x0, draw.y0, draw.width(), draw.height());
	context->fill ();

	context->set_line_width (1.0);

	/* horizontal lines */

	Gtkmm2ext::set_source_rgba (context, 0x000000ff);

	for (int n = 0; n < _nrows; ++n) {
		double x = 0;
		double y = n * _step_dimen;
		Duple start = Duple (x, y).translate (_position).translate (Duple (0.5, 0.5));

		context->move_to (start.x, start.y);
		context->line_to (start.x + _width, start.y);
		context->stroke ();
	}

	/* vertical */

	for (int n = 0; n < _nsteps; ++n) {
		double x = n * _step_dimen;
		double y = 0;
		Duple start = Duple (x, y).translate (_position).translate (Duple (0.5, 0.5));

		context->move_to (start.x, start.y);
		context->line_to (start.x, start.y + _height);
		context->stroke ();
	}

	/* vertical bars for velocity */

	Gtkmm2ext::set_source_rgba (context, UIConfiguration::instance().color ("gtk_somewhat_bright_indicator"));

	for (int r = 0; r < _nrows; ++r) {
		for (int c = 0; c < _nsteps; ++c) {
			const double velocity = random() % 127 / 127.0;
			const double height = (_step_dimen - 2) * velocity;
			context->rectangle ((c * _step_dimen) + 1, ((r + 2) * _step_dimen) - height, _step_dimen - 2, height);
			context->fill ();
		}
	}
}

SequencerStepIndicator::SequencerStepIndicator (Item *p, int n)
	: Rectangle (p)
{
	set_fill (false);
	set_outline (false);

	text = new Text (this);
	text->set (string_compose ("%1", n));
	text->set_font_description (UIConfiguration::instance ().get_SmallFont ());
	text->set_position (Duple ((_step_dimen/2.0) - (text->width()/2.0), 5.0));

	poly = new Polygon (this);
	Points points;
	points.push_back (Duple (0.0, 0.0));
	points.push_back (Duple (_step_dimen, 0.0));
	points.push_back (Duple (_step_dimen, _step_dimen/2.0));
	points.push_back (Duple (_step_dimen/2.0, _step_dimen));
	points.push_back (Duple (0.0, _step_dimen/2.0));
	poly->set (points);
	poly->set_fill_color (Gtkmm2ext::color_at_alpha (random(), 0.4));
}

void
SequencerStepIndicator::render  (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Rectangle::render (area, context);
	render_children (area, context);
}
