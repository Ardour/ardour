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

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "beatbox_gui.h"
#include "timers.h"
#include "ui_config.h"

using namespace PBD;
using namespace ARDOUR;
using namespace Gtkmm2ext;
using namespace ArdourCanvas;

using std::cerr;
using std::endl;

const int _nsteps = 32;
const int _nrows = 8;
const double _step_dimen = 25;

BBGUI::BBGUI (boost::shared_ptr<BeatBox> bb)
	: ArdourDialog (_("BeatBox"))
	, bbox (bb)
	, horizontal_adjustment (0.0, 0.0, 800.0)
	, vertical_adjustment (0.0, 0.0, 10.0, 400.0)
	, vscrollbar (vertical_adjustment)
	, mode_velocity_button (_("Velocity"))
	, mode_pitch_button (_("Pitch"))
	, mode_octave_button (_("Octave"))
	, mode_group_button (_("Group"))
	, mode_duration_button (_("Gate"))

{
	_canvas_viewport = new GtkCanvasViewport (horizontal_adjustment, vertical_adjustment);
	_canvas = _canvas_viewport->canvas();
	_canvas->set_background_color (UIConfiguration::instance().color ("gtk_bright_color"));
	_canvas->use_nsglview ();

	_sequencer = new SequencerGrid (bbox->sequencer(), _canvas);
	_sequencer->set_position (Duple (0, _step_dimen));
	_sequencer->set_fill_color (UIConfiguration::instance().color ("gtk_contrasting_indicator"));

	srandom (time(0));

	canvas_hbox.pack_start (*_canvas_viewport, true, true);
	canvas_hbox.pack_start (vscrollbar, false, false);

	mode_box.pack_start (mode_velocity_button);
	mode_box.pack_start (mode_pitch_button);
	mode_box.pack_start (mode_duration_button);
	mode_box.pack_start (mode_octave_button);
	mode_box.pack_start (mode_group_button);

	mode_velocity_button.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::mode_clicked), SequencerGrid::Velocity));
	mode_pitch_button.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::mode_clicked), SequencerGrid::Pitch));
	mode_octave_button.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::mode_clicked), SequencerGrid::Octave));
	mode_group_button.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::mode_clicked), SequencerGrid::Group));
	mode_duration_button.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::mode_clicked), SequencerGrid::Duration));

	get_vbox()->set_spacing (12);
	get_vbox()->pack_start (mode_box, false, false);
	get_vbox()->pack_start (canvas_hbox, true, true);

	start_button.signal_clicked.connect (sigc::mem_fun (*this, &BBGUI::toggle_play));
	get_action_area()->pack_end (start_button);

	export_as_region_button.signal_clicked.connect (sigc::mem_fun (*this, &BBGUI::export_as_region));
	get_action_area()->pack_end (export_as_region_button);

	bbox->sequencer().PropertyChanged.connect (sequencer_connection, invalidator (*this), boost::bind (&BBGUI::sequencer_changed, this, _1), gui_context());

	{
		/* trigger initial draw */
		PropertyChange pc;
		sequencer_changed (pc);
	}

	show_all ();
}

BBGUI::~BBGUI ()
{
}

void
BBGUI::mode_clicked (SequencerGrid::Mode m)
{
	cerr << "Set mode to " << m << endl;
	_sequencer->set_mode (m);
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

void
BBGUI::sequencer_changed (PropertyChange const &)
{
	const size_t nsteps = bbox->sequencer().nsteps();
	const size_t nsequences = bbox->sequencer().nsequences();

	_width = _step_dimen * nsteps;
	_height = _step_dimen * nsequences;

	/* height is 1 step_dimen larger to accomodate the "step indicator"
	 * line at the top
	 */

	_canvas_viewport->set_size_request (_width, _height + _step_dimen);
}

/**/

SequencerGrid::SequencerGrid (StepSequencer& s, Canvas* c)
	: Rectangle (c)
	, _sequencer (s)
	, _mode (Velocity)
{
	no_scroll_group = new ArdourCanvas::Container (_canvas->root());
	step_indicator_box = new ArdourCanvas::Container (no_scroll_group);

	step_indicator_bg = new ArdourCanvas::Rectangle (step_indicator_box);
	step_indicator_bg->set_fill_color (UIConfiguration::instance().color ("gtk_lightest"));
	step_indicator_bg->set_outline (false);

	v_scroll_group = new ScrollGroup (_canvas->root(), ScrollGroup::ScrollsVertically);
	_canvas->add_scroller (*v_scroll_group);
	v_scroll_group->add (this);

	_sequencer.PropertyChanged.connect (sequencer_connection, invalidator (*this), boost::bind (&SequencerGrid::sequencer_changed, this, _1), gui_context());

	{
		/* trigger initial draw */
		PropertyChange pc;
		sequencer_changed (pc);
	}
}

void
SequencerGrid::sequencer_changed (PropertyChange const &)
{
	const size_t nsteps = _sequencer.nsteps();
	const size_t nsequences = _sequencer.nsequences();

	_width = _step_dimen * nsteps;
	_height = _step_dimen * nsequences;

	set (Rect (0, 0, _width, _height));

	step_indicator_bg->set (Rect (0, 0, _width, _step_dimen));
	step_indicator_box->clear (true); /* delete all existing step indicators */

	/* indicator row */

	for (size_t n = 0; n < nsteps; ++n) {
		SequencerStepIndicator* ssi = new SequencerStepIndicator (step_indicator_box, n+1);
		ssi->set (Rect (n * _step_dimen, 0, (n+1) * _step_dimen, _step_dimen));
		ssi->set_position (Duple (n * _step_dimen, 0.0));
		ssi->set_fill_color (random());
	}
	/* step views, one per step per sequence */

	clear (true);
	_step_views.clear ();

	for (size_t s = 0; s < nsequences; ++s) {
		for (size_t n = 0; n < nsteps; ++n) {
			StepView* sv = new StepView (*this, _sequencer.sequence (s).step (n), v_scroll_group);
			/* sequence row is 1-row down ... because of the step indicator row */
			sv->set_position (Duple (n * _step_dimen, (s+1) * _step_dimen));
			sv->set (Rect (1, 1, _step_dimen - 2, _step_dimen - 2));
			_step_views.push_back (sv);
		}
	}
}

void
SequencerGrid::set_mode (Mode m)
{
	_mode = m;

	for (StepViews::iterator s = _step_views.begin(); s != _step_views.end(); ++s) {
		(*s)->view_mode_changed ();
	}

	redraw ();
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

	render_children (area, context);
}

SequencerStepIndicator::SequencerStepIndicator (Item *p, int n)
	: Rectangle (p)
{
	set_fill (false);
	set_outline (false);

	text = new Text (this);
	if (n == 7) {
		text->set ("\u21a9");
	} else {
		text->set (string_compose ("%1", n));
	}
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

StepView::StepView (SequencerGrid& sg, Step& s, ArdourCanvas::Item* parent)
	: ArdourCanvas::Rectangle (parent)
	, _step (s)
	, _seq (sg)
	, text (new Text (this))
{
	set_fill_color (UIConfiguration::instance().color ("selection"));

	text->set_fill_color (contrasting_text_color (fill_color()));
	text->set_font_description (UIConfiguration::instance ().get_SmallFont ());
	text->hide ();

	Event.connect (sigc::mem_fun (*this, &StepView::on_event));
	_step.PropertyChanged.connect (step_connection, invalidator (*this), boost::bind (&StepView::step_changed, this, _1), gui_context());
}

void
StepView::view_mode_changed ()
{
	cerr << "view mode changed\n";

	if (_seq.mode() == SequencerGrid::Octave) {
		cerr << "octave\n";

		text->show ();
		if (_step.octave_shift() >= 0) {
			text->set (string_compose ("+%1", _step.octave_shift()));
		} else {
			text->set (string_compose ("%1", _step.octave_shift()));
		}

		const double w = text->width();
		const double h = text->height();

		cerr << "put octave text " << text->text() << " dimen " << w << " x " << h << " @ " << Duple (_step_dimen/2 - (w/2), _step_dimen/2 - (h/2)) << endl;

		text->set_position (Duple (_step_dimen/2 - (w/2), _step_dimen/2 - (h/2)));

	} else if (_seq.mode() == SequencerGrid::Group) {
		text->set (std::string());
		text->show ();
	} else {
		text->hide ();
	}
}

void
StepView::step_changed (PropertyChange const &)
{
	redraw ();
}

void
StepView::render (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (_seq.mode() == SequencerGrid::Velocity) {
		const double height = (_step_dimen - 4) * _step.velocity();
		const Duple origin = item_to_window (Duple (0, 0));
		context->rectangle (origin.x + 2, origin.y + (_step_dimen - height - 2), _step_dimen - 4, height);
		context->fill ();
	} else if (_seq.mode() == SequencerGrid::Pitch) {
		const double height = (_step_dimen - 4) * (_step.note() / 128.0);
		const Duple origin = item_to_window (Duple (0, 0));
		context->rectangle (origin.x + 2, origin.y + (_step_dimen - height - 2), _step_dimen - 4, height);
		context->fill ();
	} else if (_seq.mode() == SequencerGrid::Duration) {
		const Step::DurationRatio d (_step.duration());
		const double height = ((_step_dimen - 4.0) * d.numerator()) / d.denominator();
		const Duple origin = item_to_window (Duple (0, 0));
		context->rectangle (origin.x + 2, origin.y + (_step_dimen - height - 2), _step_dimen - 4, height);
		context->fill ();
	}

	/* now deal with any children (e.g. text) */

	render_children (area, context);
}

bool
StepView::on_event (GdkEvent *ev)
{
	bool ret = false;

	switch (ev->type) {
	case GDK_MOTION_NOTIFY:
		ret = motion_event (&ev->motion);
		break;
	case GDK_BUTTON_PRESS:
		ret = button_press_event (&ev->button);
		break;
	case GDK_BUTTON_RELEASE:
		ret = button_release_event (&ev->button);
		break;
	case GDK_SCROLL:
		ret = scroll_event (&ev->scroll);
	default:
		break;
	}

	return ret;
}


bool
StepView::motion_event (GdkEventMotion* ev)
{
	return true;
}

bool
StepView::button_press_event (GdkEventButton* ev)
{
	grab_at = std::make_pair (ev->x, ev->y);
	return true;
}

bool
StepView::button_release_event (GdkEventButton* ev)
{
	return true;
}

bool
StepView::scroll_event (GdkEventScroll* ev)
{
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

	if ((ev->state & GDK_MOD1_MASK) || _seq.mode() == SequencerGrid::Pitch) {
		cerr << "adjust pitch by " << amt << endl;
		adjust_step_pitch (amt);
	} else if (_seq.mode() == SequencerGrid::Velocity) {
		cerr << "adjust velocity by " << amt << endl;
		adjust_step_velocity (amt);
	} else if (_seq.mode() == SequencerGrid::Duration) {
		cerr << "adjust duration by " << Step::DurationRatio (amt, 32) << endl;
		adjust_step_duration (Step::DurationRatio (amt, 32)); /* adjust by 1/32 of the sequencer step size */
	} else if (_seq.mode() == SequencerGrid::Octave) {
		adjust_step_octave (amt);
	} else if (_seq.mode() == SequencerGrid::Group) {
	}

	return true;
}

void
StepView::adjust_step_pitch (int amt)
{
	_step.adjust_pitch (amt);
}

void
StepView::adjust_step_velocity (int amt)
{
	_step.adjust_velocity (amt);
}

void
StepView::adjust_step_octave (int amt)
{
	_step.adjust_octave (amt);
}

void
StepView::adjust_step_duration (Step::DurationRatio const & amt)
{
	_step.adjust_duration (amt);
}
