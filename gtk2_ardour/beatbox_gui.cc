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

#include <boost/integer/common_factor.hpp>

#include "pbd/compose.h"
#include "pbd/i18n.h"

#ifdef HAVE_BEATBOX
#include "ardour/beatbox.h"
#endif
#include "ardour/parameter_descriptor.h"
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
#include "canvas/constrained_item.h"
#include "canvas/rectangle.h"
#include "canvas/polygon.h"
#include "canvas/scroll_group.h"
#include "canvas/step_button.h"
#include "canvas/text.h"
#include "canvas/widget.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/colors.h"

#include "beatbox_gui.h"
#include "floating_text_entry.h"
#include "timers.h"
#include "ui_config.h"

using namespace PBD;
using namespace ARDOUR;
using namespace Gtkmm2ext;
using namespace ArdourCanvas;

using std::cerr;
using std::endl;

const double _step_dimen = 32;

BBGUI::BBGUI (boost::shared_ptr<BeatBox> bb)
	: ArdourDialog (_("BeatBox"))
	, bbox (bb)
	, horizontal_adjustment (0.0, 0.0, 800.0)
	, vertical_adjustment (0.0, 0.0, 10.0, 400.0)
	, export_as_region_button (_("Export as Region"))
{
	_canvas_viewport = new GtkCanvasViewport (horizontal_adjustment, vertical_adjustment);
	_canvas = _canvas_viewport->canvas();
	_canvas->set_background_color (UIConfiguration::instance().color ("gtk_bases"));
	_canvas->use_nsglview ();

	_sequencer = new SequencerView (bbox->sequencer(), _canvas->root());

	canvas_hbox.pack_start (*_canvas_viewport, true, true);

	get_vbox()->set_spacing (12);
	get_vbox()->pack_start (canvas_hbox, true, true);

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
BBGUI::update ()
{
	_sequencer->update ();
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
		/* XXX changes should be driven by model */
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
}

void
BBGUI::export_as_region ()
{
	std::string path = bbox->session().new_midi_source_path (bbox->owner()->name());
	boost::shared_ptr<Source> src = bbox->sequencer().write_to_source (bbox->session(), path);

	if (!src) {
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
	const size_t nsteps = bbox->sequencer().step_capacity ();
	const size_t nsequences = bbox->sequencer().nsequences();

	_width = _step_dimen * nsteps;
	_height = (_step_dimen * (1+nsequences)) + SequencerView::mode_button_ydim;

	size_t required_scroll = _height - _canvas_viewport->get_allocation().get_height();

	vertical_adjustment.set_upper (required_scroll);

	/* height is 1 step_dimen larger to accommodate the "step indicator"
	 * line at the top
	 */

	_canvas_viewport->set_size_request (SequencerView::rhs_xoffset + _width, _height);
}

/**/

Gtkmm2ext::Color SequencerView::current_mode_color = Gtkmm2ext::Color (0);
Gtkmm2ext::Color SequencerView::not_current_mode_color = Gtkmm2ext::Color (0);

double SequencerView::rhs_xoffset = 250.0;
double SequencerView::mode_button_width = 110.0;
double SequencerView::mode_button_height = 60.0;
double SequencerView::mode_button_spacing = 10;
double SequencerView::mode_button_xdim = mode_button_width + mode_button_spacing;
double SequencerView::mode_button_ydim = mode_button_height + mode_button_spacing;

SequencerView::SequencerView (StepSequencer& s, ArdourCanvas::Item *p)
	: ConstraintPacker (p, Vertical)
	, _sequencer (s)
	, _mode (Velocity)
{
	name = "SequencerView";
	set_fill (false);

	if (current_mode_color == 0) {
		current_mode_color = UIConfiguration::instance().color ("gtk_lightest");
		not_current_mode_color = contrasting_text_color (current_mode_color);
	}

	const Duple mode_button_center (mode_button_width/2.0, mode_button_height/2.0);

	button_packer = new ArdourCanvas::ConstraintPacker (_canvas, ArdourCanvas::Horizontal);
	button_packer->name = "ButtonPacker";
	pack_start (button_packer);

	sequence_hbox = new ConstraintPacker (_canvas, Horizontal);
	sequence_hbox->name = "shbox";
	pack_start (sequence_hbox);

	lhs_vbox = new ArdourCanvas::ConstraintPacker (_canvas, Vertical);
	lhs_vbox->name = "lhs";

	ConstrainedItem* ci;
	ci = sequence_hbox->pack_start (lhs_vbox);
	ci->add_constraint (ci->width() == 0.8 * sequence_hbox->width);

	steps_vbox = new ConstraintPacker (_canvas, Vertical);
	steps_vbox->name = "steps";
	sequence_hbox->pack_start (steps_vbox, PackOptions (PackExpand|PackFill));

	rhs_vbox = new ConstraintPacker (_canvas, Vertical);
	rhs_vbox->name = "rhs";
	sequence_hbox->pack_start (rhs_vbox);

	velocity_mode_button = new Rectangle (_canvas);
	velocity_mode_button->set_corner_radius (10.0);
	velocity_mode_button->set_intrinsic_size (mode_button_width, mode_button_height);
	velocity_mode_button->set_fill_color (current_mode_color);
	velocity_mode_text = new Text (_canvas);
	velocity_mode_text->set_font_description (UIConfiguration::instance().get_LargeFont());
	velocity_mode_text->set (_("Velocity"));
	velocity_mode_text->set_color (contrasting_text_color (velocity_mode_button->fill_color()));

	pitch_mode_button = new Rectangle (_canvas);
	pitch_mode_button->set_corner_radius (10.0);
	pitch_mode_button->set_intrinsic_size (mode_button_width, mode_button_height);
	pitch_mode_button->set_fill_color (not_current_mode_color);
	pitch_mode_text = new Text (pitch_mode_button);
	pitch_mode_text->set_font_description (UIConfiguration::instance().get_LargeFont());
	pitch_mode_text->set (_("Pitch"));
	pitch_mode_text->set_color (contrasting_text_color (pitch_mode_button->fill_color()));

	gate_mode_button = new Rectangle (_canvas);
	gate_mode_button->set_corner_radius (10.0);
	gate_mode_button->set_intrinsic_size (mode_button_width, mode_button_height);
	gate_mode_button->set_fill_color (not_current_mode_color);
	gate_mode_text = new Text (gate_mode_button);
	gate_mode_text->set_font_description (UIConfiguration::instance().get_LargeFont());
	gate_mode_text->set (_("Gate"));
	gate_mode_text->set_color (contrasting_text_color (gate_mode_button->fill_color()));

	octave_mode_button = new Rectangle (_canvas);
	octave_mode_button->set_corner_radius (10.0);
	octave_mode_button->set_intrinsic_size (mode_button_width, mode_button_height);
	octave_mode_button->set_fill_color (not_current_mode_color);
	octave_mode_text = new Text (octave_mode_button);
	octave_mode_text->set_font_description (UIConfiguration::instance().get_LargeFont());
	octave_mode_text->set (_("Octave"));
	octave_mode_text->set_color (contrasting_text_color (octave_mode_button->fill_color()));

	timing_mode_button = new Rectangle (_canvas);
	timing_mode_button->set_corner_radius (10.0);
	timing_mode_button->set_intrinsic_size (mode_button_width, mode_button_height);
	timing_mode_button->set_fill_color (not_current_mode_color);
	timing_mode_text = new Text (timing_mode_button);
	timing_mode_text->set_font_description (UIConfiguration::instance().get_LargeFont());
	timing_mode_text->set (_("Timing"));
	timing_mode_text->set_color (contrasting_text_color (timing_mode_button->fill_color()));

	ConstrainedItem* b;
	ConstrainedItem* t;

	b = button_packer->pack_start (velocity_mode_button, PackOptions (PackExpand|PackFill), PackOptions (0));
	t = button_packer->add_constrained (velocity_mode_text);
	t->centered_on (*b);

	b = button_packer->pack_start (pitch_mode_button, PackOptions (PackExpand|PackFill), PackOptions (0));
	t = button_packer->add_constrained (pitch_mode_text);
	t->centered_on (*b);

	b = button_packer->pack_start (gate_mode_button, PackOptions (PackExpand|PackFill), PackOptions (0));
	t = button_packer->add_constrained (gate_mode_text);
	t->centered_on (*b);

	b = button_packer->pack_start (octave_mode_button, PackOptions (PackExpand|PackFill), PackOptions (0));
	t = button_packer->add_constrained (octave_mode_text);
	t->centered_on (*b);

	b = button_packer->pack_start (timing_mode_button, PackOptions (PackExpand|PackFill), PackOptions (0));
	t = button_packer->add_constrained (timing_mode_text);
	t->centered_on (*b);

	octave_mode_button->Event.connect (sigc::bind (sigc::mem_fun (*this, &SequencerView::mode_button_event), Octave));
	gate_mode_button->Event.connect (sigc::bind (sigc::mem_fun (*this, &SequencerView::mode_button_event), Duration));
	pitch_mode_button->Event.connect (sigc::bind (sigc::mem_fun (*this, &SequencerView::mode_button_event), Pitch));
	velocity_mode_button->Event.connect (sigc::bind (sigc::mem_fun (*this, &SequencerView::mode_button_event), Velocity));
	timing_mode_button->Event.connect (sigc::bind (sigc::mem_fun (*this, &SequencerView::mode_button_event), Timing));
	octave_mode_text->set_ignore_events (true);
	gate_mode_text->set_ignore_events (true);
	pitch_mode_text->set_ignore_events (true);
	velocity_mode_text->set_ignore_events (true);
	timing_mode_text->set_ignore_events (true);

	_sequencer.PropertyChanged.connect (sequencer_connection, invalidator (*this), boost::bind (&SequencerView::sequencer_changed, this, _1), gui_context());

	{
		/* trigger initial draw */
		PropertyChange pc;
		sequencer_changed (pc);
	}
}

bool
SequencerView::mode_button_event (GdkEvent* ev, Mode m)
{
	switch (ev->type) {
	case GDK_BUTTON_RELEASE:
		set_mode (m);
		return true;
	default:
		break;
	}

	return false;
}

void
SequencerView::update ()
{
	bool running = true;
	size_t step = _sequencer.last_step ();

	for (SequenceViews::iterator s = sequence_views.begin(); s != sequence_views.end(); ++s) {

	}
}

void
SequencerView::sequencer_changed (PropertyChange const &)
{
	const size_t nsteps = _sequencer.step_capacity ();
	const size_t nsequences = _sequencer.nsequences();
	size_t n;

	_width = _step_dimen * nsteps;
	_height = _step_dimen * nsequences;

	/* indicator row */

#if 0
	while (step_indicators.size() > nsteps) {
		SequencerStepIndicator* ssi = step_indicators.back();
		step_indicators.pop_back();
		step_indicator_box->remove (ssi);
		delete ssi;
	}

	n = step_indicators.size();

	while (step_indicators.size() < nsteps) {
		SequencerStepIndicator* ssi = new SequencerStepIndicator (*this, canvas(), n);
		step_indicator_box->pack_start (ssi, PackOptions (PackExpand|PackFill), PackOptions (0));
		step_indicators.push_back (ssi);
		++n;
	}
#endif

	while (sequence_views.size() > nsequences) {
		SequenceView* sh = sequence_views.back ();
		sequence_views.pop_back ();
		delete sh;
	}

	n = sequence_views.size();

	while (sequence_views.size() < nsequences) {
		SequenceView* sv = new SequenceView (*this, _sequencer.sequence (n), canvas());

		lhs_vbox->pack_start (sv->lhs_box);
		steps_vbox->pack_start (sv->step_box);
		rhs_vbox->pack_start (sv->rhs_box);

		sequence_views.push_back (sv);
		++n;
	}
}

void
SequencerView::set_mode (Mode m)
{
	if (_mode == m) {
		return;
	}

	_mode = m;

	switch (_mode) {
	case Octave:
		octave_mode_button->set_fill_color (current_mode_color);
		octave_mode_text->set_color (contrasting_text_color (current_mode_color));

		timing_mode_button->set_fill_color (not_current_mode_color);
		velocity_mode_button->set_fill_color (not_current_mode_color);
		gate_mode_button->set_fill_color (not_current_mode_color);
		pitch_mode_button->set_fill_color (not_current_mode_color);
		pitch_mode_text->set_color (contrasting_text_color (not_current_mode_color));
		gate_mode_text->set_color (pitch_mode_text->color ());
		velocity_mode_text->set_color (pitch_mode_text->color());
		timing_mode_text->set_color (pitch_mode_text->color());
		break;

	case Duration:
		gate_mode_button->set_fill_color (current_mode_color);
		gate_mode_text->set_color (contrasting_text_color (current_mode_color));

		timing_mode_button->set_fill_color (not_current_mode_color);
		velocity_mode_button->set_fill_color (not_current_mode_color);
		octave_mode_button->set_fill_color (not_current_mode_color);
		pitch_mode_button->set_fill_color (not_current_mode_color);
		pitch_mode_text->set_color (contrasting_text_color (not_current_mode_color));
		octave_mode_text->set_color (pitch_mode_text->color ());
		velocity_mode_text->set_color (pitch_mode_text->color());
		timing_mode_text->set_color (pitch_mode_text->color());
		break;

	case Pitch:
		pitch_mode_button->set_fill_color (current_mode_color);
		pitch_mode_text->set_color (contrasting_text_color (current_mode_color));

		timing_mode_button->set_fill_color (not_current_mode_color);
		velocity_mode_button->set_fill_color (not_current_mode_color);
		octave_mode_button->set_fill_color (not_current_mode_color);
		gate_mode_button->set_fill_color (not_current_mode_color);
		gate_mode_text->set_color (contrasting_text_color (not_current_mode_color));
		octave_mode_text->set_color (gate_mode_text->color ());
		velocity_mode_text->set_color (gate_mode_text->color());
		timing_mode_text->set_color (gate_mode_text->color());
		break;

	case Velocity:
		velocity_mode_button->set_fill_color (current_mode_color);
		velocity_mode_text->set_color (contrasting_text_color (current_mode_color));

		timing_mode_button->set_fill_color (not_current_mode_color);
		pitch_mode_button->set_fill_color (not_current_mode_color);
		octave_mode_button->set_fill_color (not_current_mode_color);
		gate_mode_button->set_fill_color (not_current_mode_color);
		pitch_mode_text->set_color (contrasting_text_color (not_current_mode_color));
		octave_mode_text->set_color (pitch_mode_text->color ());
		gate_mode_text->set_color (pitch_mode_text->color());
		timing_mode_text->set_color (pitch_mode_text->color());
		break;

	case Timing:
		timing_mode_button->set_fill_color (current_mode_color);
		timing_mode_text->set_color (contrasting_text_color (current_mode_color));

		velocity_mode_button->set_fill_color (not_current_mode_color);
		pitch_mode_button->set_fill_color (not_current_mode_color);
		octave_mode_button->set_fill_color (not_current_mode_color);
		gate_mode_button->set_fill_color (not_current_mode_color);
		pitch_mode_text->set_color (contrasting_text_color (not_current_mode_color));
		octave_mode_text->set_color (pitch_mode_text->color ());
		gate_mode_text->set_color (pitch_mode_text->color());
		velocity_mode_text->set_color (gate_mode_text->color());

	default:
		break;
	}

	for (SequenceViews::iterator s = sequence_views.begin(); s != sequence_views.end(); ++s) {
		(*s)->view_mode_changed ();
	}

	redraw ();}


void
SequencerView::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	/* might do more one day */
	ConstraintPacker::render (area, context);
}

SequenceView&
SequencerView::sequence_view (size_t n) const
{
	assert (n < sequence_views.size());
	return *sequence_views[n];
}

/**/

Gtkmm2ext::Color SequencerStepIndicator::other_color = Gtkmm2ext::Color (0);
Gtkmm2ext::Color SequencerStepIndicator::current_color = Gtkmm2ext::Color (0);
Gtkmm2ext::Color SequencerStepIndicator::other_text_color = Gtkmm2ext::Color (0);
Gtkmm2ext::Color SequencerStepIndicator::current_text_color = Gtkmm2ext::Color (0);
Gtkmm2ext::Color SequencerStepIndicator::bright_outline_color = Gtkmm2ext::Color (0);
int SequencerStepIndicator::dragging = 0;

SequencerStepIndicator::SequencerStepIndicator (SequencerView& s, ArdourCanvas::Canvas* c, size_t n)
	: Rectangle (c)
	, sv (s)
	, number (n)
	, being_dragged (false)
{
	if (current_color == 0) { /* zero alpha? not set */
		other_color = UIConfiguration::instance().color ("gtk_bases");
		current_color = UIConfiguration::instance().color ("gtk_bright_color");
		other_text_color = contrasting_text_color (other_color);
		current_text_color = contrasting_text_color (current_color);
		bright_outline_color = UIConfiguration::instance().color ("gtk_bright_indicator");
	}

	set_intrinsic_size (_step_dimen - 1, _step_dimen - 1);

	set_fill_color (HSV (UIConfiguration::instance().color ("gtk_bases")).lighter (0.1));
	set_fill (true);
	set_outline (false);

	poly = new Polygon (this);
	Points points;
	points.push_back (Duple (1, 1));
	points.push_back (Duple (_step_dimen - 3.0, 1));
	points.push_back (Duple (_step_dimen - 3.0, (_step_dimen - 3.0)/2.0));
	points.push_back (Duple ((_step_dimen - 3.0)/2.0, _step_dimen - 3.0));
	points.push_back (Duple (1, (_step_dimen - 3.0)/2.0));
	poly->set (points);
	poly->set_fill_color (current_color);
	poly->set_outline_color (other_color);
	poly->set_ignore_events (true);
	poly->move (Duple (-0.5, -0.5));

	text = new Text (this);

	set_text ();

	text->set_font_description (UIConfiguration::instance ().get_NormalFont ());
	text->set_position (Duple ((_step_dimen/2.0) - (text->width()/2.0), 5.0));
	text->set_color (other_text_color);
	text->set_ignore_events (true);
	text->name = string_compose ("SI %1", n);

	Event.connect (sigc::mem_fun (*this, &SequencerStepIndicator::on_event));
	sv.sequencer().PropertyChanged.connect (sequencer_connection, invalidator (*this), boost::bind (&SequencerStepIndicator::sequencer_changed, this, _1), gui_context());
}

void
SequencerStepIndicator::sequencer_changed (PropertyChange const &)
{
	set_text ();
}

void
SequencerStepIndicator::set_text ()
{
	if (number == sv.sequencer().end_step() - 1) {
		text->set ("\u21a9");
	} else if (number == sv.sequencer().start_step()) {
		text->set ("\u21aa");
	} else {
		text->set (string_compose ("%1", number+1));
	}
}

bool
SequencerStepIndicator::on_event (GdkEvent* ev)
{
	bool ret = false;

	switch (ev->type) {
	case GDK_ENTER_NOTIFY:
		switch (dragging) {
		case 1: /* end */
			poly->set_outline_color (bright_outline_color);
			poly->set_fill_color (current_color);
			being_dragged = true;
			break;
		case -1:
			poly->set_outline_color (bright_outline_color);
			poly->set_fill_color (current_color);
			being_dragged = true;
			break;
		default:
			poly->set_outline_color (other_color);
			poly->set_fill_color (other_color);
		}
		break;
	case GDK_LEAVE_NOTIFY:
		if (dragging) {
			poly->set_outline_color (other_color);
			poly->set_fill_color (other_color);
			being_dragged = false;
		}
		break;
	case GDK_MOTION_NOTIFY:
		ret = motion_event (&ev->motion);
		break;
	case GDK_BUTTON_PRESS:
		ret = button_press_event (&ev->button);
		break;
	case GDK_BUTTON_RELEASE:
		ret = button_release_event (&ev->button);
		break;
	default:
		break;
	}

	return ret;
}

bool
SequencerStepIndicator::motion_event (GdkEventMotion* ev)
{
	return true;
}

bool
SequencerStepIndicator::button_press_event (GdkEventButton* ev)
{
	if (number == sv.sequencer().end_step() - 1) {
		dragging = 1;
	} else if (number == sv.sequencer().start_step()) {
		dragging = -1;
	}

	return true;
}

bool
SequencerStepIndicator::button_release_event (GdkEventButton* ev)
{
	switch (dragging) {
	case 1:
		sv.sequencer().set_end_step (number+1);
		break;
	case -1:
		sv.sequencer().set_start_step (number);
		break;
	default:
		break;
	}

	dragging = 0;
	being_dragged = false;

	return true;
}

void
SequencerStepIndicator::set_current (bool yn)
{
	if (being_dragged) {
		return;
	}

	if (yn) {
		poly->set_fill_color (current_color);
		text->set_color (current_text_color);
	} else {
		poly->set_fill_color (other_color);
		text->set_color (other_text_color);
	}
}

void
SequencerStepIndicator::render  (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Rectangle::render (area, context);
	render_children (area, context);
}

Gtkmm2ext::Color StepView::on_fill_color = Gtkmm2ext::Color (0);
Gtkmm2ext::Color StepView::off_fill_color = Gtkmm2ext::Color (0);

StepView::StepView (SequenceView& sview, Step& s, ArdourCanvas::Canvas* c)
	: ArdourCanvas::Rectangle (c)
	, _step (s)
	, sv (sview)
	, text (new Text (this))
	, grabbed (false)
{
	set_intrinsic_size (_step_dimen - 1, _step_dimen - 1);
	name = string_compose ("stepview for %1", _step.index());

	if (on_fill_color == 0) {
		on_fill_color = UIConfiguration::instance().color ("gtk_bases");
		off_fill_color = HSV (on_fill_color).lighter (0.1);
	}

	set_fill_color (off_fill_color);
	set_outline_color (UIConfiguration::instance().color ("gtk_bright_color"));
	set_outline (true);

	text->set_color (contrasting_text_color (fill_color()));
	text->set_font_description (UIConfiguration::instance ().get_SmallFont ());
	text->hide ();
	text->name = string_compose ("step %1", _step.index());

	Event.connect (sigc::mem_fun (*this, &StepView::on_event));
	_step.PropertyChanged.connect (step_connection, invalidator (*this), boost::bind (&StepView::step_changed, this, _1), gui_context());
}

void
StepView::view_mode_changed ()
{
	/* this should leave the text to the last text-displaying mode */

	switch (sv.mode()) {
	case SequencerView::Octave:
		set_octave_text ();
		break;
	case SequencerView::Group:
		set_group_text ();
		break;
	case SequencerView::Timing:
		set_timing_text ();
		break;
	default:
		text->hide ();
	}
}

void
StepView::set_group_text ()
{
	text->set ("-");
}

void
StepView::set_octave_text ()
{
	if (_step.octave_shift() > 0) {
		text->set (string_compose ("+%1", _step.octave_shift()));
		text->show ();
	} else if (_step.octave_shift() == 0) {
		text->hide ();
	} else {
		text->set (string_compose ("%1", _step.octave_shift()));
		text->show ();
	}

	if (text->self_visible()) {
		const double w = text->width();
		const double h = text->height();
		text->set_position (Duple (_step_dimen/2 - (w/2), _step_dimen/2 - (h/2)));
	}
}

void
StepView::set_timing_text ()
{
	if (!_step.velocity()) {
		text->hide ();
		return;
	}

	if (_step.offset() == Temporal::Beats()) {
		text->set (X_("0"));
	} else {
		const int64_t gcd = boost::integer::gcd (_step.offset().to_ticks(), int64_t (1920));
		const int64_t n = _step.offset().to_ticks() / gcd;
		const int64_t d = 1920 / gcd;
		text->set (string_compose ("%1/%2", n, d));
		text->show ();
	}

	if (text->self_visible()) {
		const double w = text->width();
		const double h = text->height();
		text->set_position (Duple (_step_dimen/2 - (w/2), _step_dimen/2 - (h/2)));
	}
}

void
StepView::step_changed (PropertyChange const &)
{
	if (sv.mode() == SequencerView::Octave) {
		set_octave_text ();
	} else if (sv.mode() == SequencerView::Timing) {
		set_timing_text ();
	}

	if (_step.velocity()) {
		set_fill_color (on_fill_color);
	} else {
		set_fill_color (off_fill_color);
	}

	redraw ();
}

void
StepView::render (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Rectangle::render (area, context);

	if (sv.mode() == SequencerView::Velocity) {
		if (_step.velocity()) {
			const double height = (_step_dimen - 4) * _step.velocity();
			const Duple origin = item_to_window (Duple (0, 0));
			set_source_rgba (context, outline_color());
			context->rectangle (origin.x + 2, origin.y + (_step_dimen - height - 2), _step_dimen - 4, height);
			context->fill ();
		}
	} else if (sv.mode() == SequencerView::Pitch) {
		if (_step.velocity()) {
			const double height = (_step_dimen - 4) * (_step.note() / 128.0);
			const Duple origin = item_to_window (Duple (0, 0));
			set_source_rgba (context, outline_color());
			context->rectangle (origin.x + 2, origin.y + (_step_dimen - height - 2), _step_dimen - 4, height);
			context->fill ();
		}
	} else if (sv.mode() == SequencerView::Duration) {
		if (_step.velocity()) {
			const Step::DurationRatio d (_step.duration());
			const double height = ((_step_dimen - 4.0) * d.numerator()) / d.denominator();
			const Duple origin = item_to_window (Duple (0, 0));
			set_source_rgba (context, outline_color());
			context->rectangle (origin.x + 2, origin.y + (_step_dimen - height - 2), _step_dimen - 4, height);
			context->fill ();
		}
	} else if (sv.mode() == SequencerView::Timing) {
		if (_step.velocity()) {
			Temporal::Beats offset = _step.offset ();
			const double fract = offset.to_ticks() / (double) sequencer().step_size().to_ticks();
			const double width = ((_step_dimen - 4.0)/2.0) * fract;
			const Duple origin = item_to_window (Duple (_step_dimen / 2.0, 0.0));

			/* draw sideways "bars" from center" to indicate extent
			 * of time displacement from nominal time for this step
			 */

			set_source_rgba (context, outline_color());
			context->rectangle (origin.x + 2.0, origin.y + 2.0, width, _step_dimen - 4.0);
			context->fill ();
		}
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
	if (!grabbed) {
		return false;
	}

	int distance;

	switch (sv.mode()) {
	case SequencerView::Timing:
		distance = ev->x - last_motion.first;
		break;
	default:
		distance = last_motion.second - ev->y;
		break;
	}

	if ((ev->state & GDK_MOD1_MASK) || sv.mode() == SequencerView::Pitch) {
		adjust_step_pitch (distance);
	} else if (sv.mode() == SequencerView::Velocity) {
		adjust_step_velocity (distance);
	} else if (sv.mode() == SequencerView::Duration) {
		adjust_step_duration (Step::DurationRatio (distance, 32)); /* adjust by 1/32 of the sequencer step size */
	} else if (sv.mode() == SequencerView::Octave) {
		adjust_step_octave (distance);
	} else if (sv.mode() == SequencerView::Timing) {
		adjust_step_timing (distance/(_step_dimen / 2.0));
	} else if (sv.mode() == SequencerView::Group) {
	}

	last_motion = std::make_pair (ev->x, ev->y);
	return true;
}

bool
StepView::button_press_event (GdkEventButton* ev)
{
	grab_at = std::make_pair (ev->x, ev->y);
	last_motion = grab_at;
	grab ();
	grabbed = true;

	return true;
}

bool
StepView::button_release_event (GdkEventButton* ev)
{
	if (grabbed) {
		ungrab ();
		grabbed = false;

		int distance_moved;

		switch (sv.mode()) {
		case SequencerView::Timing:
			distance_moved = grab_at.first - last_motion.first;
			break;
		default:
			distance_moved = last_motion.second - grab_at.second;
			break;
		}

		if (::abs (distance_moved) < 4) {
			/* just a click */

			/* in all modes except octave, turn step on if it is off */

			if (sv.mode() == SequencerView::Octave) {
				if (_step.velocity()) {
					_step.set_octave_shift (0);
				} else {
					_step.set_velocity (0.8);
				}
			} else {
				if (_step.velocity()) {
					_step.set_velocity (0.0);
				} else {
					_step.set_velocity (0.8);
				}
			}
		}
	}

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

	if ((ev->state & GDK_MOD1_MASK) || sv.mode() == SequencerView::Pitch) {
		adjust_step_pitch (amt);
	} else if (sv.mode() == SequencerView::Velocity) {
		adjust_step_velocity (amt);
	} else if (sv.mode() == SequencerView::Duration) {
		/* adjust by 1/32 of the sequencer step size */
		adjust_step_duration (Step::DurationRatio (amt, 32));
	} else if (sv.mode() == SequencerView::Octave) {
		adjust_step_octave (amt);
	} else if (sv.mode() == SequencerView::Timing) {
		adjust_step_timing (amt);
	} else if (sv.mode() == SequencerView::Group) {
	}

	return true;
}

void
StepView::adjust_step_timing (double fract)
{
	_step.adjust_offset (fract);
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

/**/

SequenceView::SequenceView (SequencerView& sview, StepSequence& sq, Canvas* canvas)
	: sv (sview)
	, sequence (sq)
{
	lhs_box = new ArdourCanvas::ConstraintPacker (canvas, ArdourCanvas::Horizontal);
	lhs_box->set_padding (12);
	rhs_box = new ArdourCanvas::ConstraintPacker (canvas, ArdourCanvas::Horizontal);
	rhs_box->set_padding (12);
	step_box = new ArdourCanvas::ConstraintPacker (canvas, ArdourCanvas::Horizontal);
	step_box->set_padding (6);

	lhs_box->set_fill_color (UIConfiguration::instance().color ("gtk_bright_color"));
	lhs_box->set_fill (true);
	lhs_box->set_outline (false);
	lhs_box->set_spacing (5.0);
	lhs_box->name = string_compose ("lhs box for %1", sequence.index());

	number_text = new Text (canvas);
	number_text->set_font_description (UIConfiguration::instance().get_LargeFont());
	number_text->set_intrinsic_size (_step_dimen - 1, _step_dimen - 1);
	number_text->name = string_compose ("%1 text bit", sequence.index() + 1);
	number_text->set_color (contrasting_text_color (lhs_box->fill_color()));
	number_text->set (string_compose ("%1", sequence.index() + 1));

	name_text = new Text (canvas);
	name_text->set_font_description (UIConfiguration::instance().get_LargeFont());
	name_text->set_color (contrasting_text_color (lhs_box->fill_color()));
	name_text->Event.connect (sigc::mem_fun (*this, &SequenceView::name_text_event));
	name_text->set (_("Snare"));
	name_text->set_intrinsic_size (name_text->width(), name_text->height());

	root_text = new Text (canvas);
	root_text->set ("F#2"); // likely widest root label
	root_text->set (ParameterDescriptor::midi_note_name (sequence.root()));
	root_text->set_font_description (UIConfiguration::instance().get_LargeFont());
	root_text->set_intrinsic_size (root_text->width(), _step_dimen - 1);
	root_text->set_color (contrasting_text_color (lhs_box->fill_color()));

	lhs_box->pack_start (name_text);
	lhs_box->pack_start (number_text);
	lhs_box->pack_start (root_text);

	step_cnt_button = new Rectangle (canvas);
	step_cnt_button->set_fill_color (0x0000ffff);
	step_cnt_button->set_fill (true);

	const size_t nsteps = sequencer().nsteps();

	for (size_t n = 0; n < nsteps; ++n) {
		StepView* stepview = new StepView (*this, sequence.step (n), canvas);
		step_views.push_back (stepview);
		step_box->pack_start (stepview, PackOptions (PackExpand|PackFill), PackOptions (PackExpand|PackFill));
	}

	step_box->pack_start (step_cnt_button, PackOptions (PackExpand|PackFill), PackOptions (PackExpand|PackFill));

	speed_slide = new Rectangle (canvas);
	speed_slide->set_intrinsic_size (_step_dimen - 1, _step_dimen - 1);
	speed_slide->set_fill_color (0xff0000ff);
	speed_slide->set_fill (true);

	rhs_box->pack_start (speed_slide, PackOptions (PackExpand|PackFill));

}

void
SequenceView::view_mode_changed ()
{
	for (StepViews::iterator s = step_views.begin(); s != step_views.end(); ++s) {
		(*s)->view_mode_changed ();
	}
}

bool
SequenceView::name_text_event (GdkEvent *ev)
{
	switch (ev->type) {
	case GDK_2BUTTON_PRESS:
		edit_name ();
		return true;
	default:
		break;
	}

	return false;
}

void
SequenceView::edit_name ()
{
	GtkCanvas* gc = dynamic_cast<GtkCanvas*> (name_text->canvas());
	assert (gc);
	Gtk::Window* toplevel = (Gtk::Window*) gc->get_toplevel();
	assert (toplevel);

	floating_entry = new FloatingTextEntry (toplevel, name_text->text());
	floating_entry->use_text.connect (sigc::mem_fun (*this, &SequenceView::name_edited));
	floating_entry->set_name (X_("LargeTextEntry"));

	/* move fte top left corner to top left corner of name text */

	Duple wc = name_text->item_to_window (Duple (0,0));
	int x, y;
	int wx, wy;

	gc->translate_coordinates (*toplevel, wc.x, wc.y, x, y);
	toplevel->get_window()->get_origin (wx, wy);

	floating_entry->move (wx + x, wy + y);
	floating_entry->present ();
}

void
SequenceView::name_edited (std::string str, int next)
{
	name_text->set (str);

	switch (next) {
	case 0:
		break;
	case 1:
		if (sequence.index() < sv.sequencer().nsequences() - 1) {
			sv.sequence_view (sequence.index() + 1).edit_name();
		}
		break;
	case -1:
		if (sequence.index() > 0) {
			sv.sequence_view (sequence.index() - 1).edit_name();
		}
		break;
	}
}
