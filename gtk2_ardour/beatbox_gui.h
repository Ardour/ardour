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

#ifndef __gtk2_ardour_beatbox_gui_h__
#define __gtk2_ardour_beatbox_gui_h__

#include <string>
#include <boost/shared_ptr.hpp>

#include <gtkmm/radiobutton.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/button.h>
#include <gtkmm/scrollbar.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/box.h>
#include <gtkmm/notebook.h>

#include "gtkmm2ext/colors.h"

#include "widgets/ardour_button.h"

#include "canvas/box.h"
#include "canvas/canvas.h"
#include "canvas/constraint_packer.h"
#include "canvas/rectangle.h"

#include "ardour/step_sequencer.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"
#include "ardour_dialog.h"

namespace PBD {
class PropertyChange;
}

namespace ArdourCanvas {
class Item;
class StepButton;
class Polygon;
class Text;
class VBox;
class Widget;
}

namespace ARDOUR {
class BeatBox;
}

class SequencerView;
class SequenceView;
class StepView;
class FloatingTextEntry;
class SequencerStepIndicator;

class SequencerView : public ArdourCanvas::ConstraintPacker, public sigc::trackable {
  public:
	enum Mode {
		Velocity,
		Pitch,
		Duration,
		Octave,
		Group,
		Timing,
	};

	SequencerView (ARDOUR::StepSequencer&, ArdourCanvas::Item*);

	ARDOUR::StepSequencer& sequencer() const { return _sequencer; }

	SequenceView& sequence_view (size_t n) const;

	Mode mode() const { return _mode; }
	void set_mode (Mode m);

	void render (ArdourCanvas::Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	void update ();

	static double rhs_xoffset;
	static double mode_button_width;
	static double mode_button_height;
	static double mode_button_spacing;
	static double mode_button_xdim;
	static double mode_button_ydim;

  private:
	ARDOUR::StepSequencer& _sequencer;
	typedef std::vector<SequencerStepIndicator*> StepIndicators;
	StepIndicators step_indicators;
	typedef std::vector<SequenceView*> SequenceViews;
	SequenceViews sequence_views;

	double _width;
	double _height;
	Mode   _mode;
	ArdourCanvas::ConstraintPacker* button_packer;

	ArdourCanvas::ConstraintPacker* step_indicator_box;

	ArdourCanvas::ConstraintPacker* sequence_hbox;
	ArdourCanvas::ConstraintPacker* lhs_vbox;
	ArdourCanvas::ConstraintPacker* steps_vbox;
	ArdourCanvas::ConstraintPacker* rhs_vbox;

	ArdourCanvas::Rectangle* velocity_mode_button;
	ArdourCanvas::Rectangle* pitch_mode_button;
	ArdourCanvas::Rectangle* octave_mode_button;
	ArdourCanvas::Rectangle* gate_mode_button;
	ArdourCanvas::Rectangle* timing_mode_button;

	ArdourCanvas::Text* velocity_mode_text;
	ArdourCanvas::Text* pitch_mode_text;
	ArdourCanvas::Text* octave_mode_text;
	ArdourCanvas::Text* gate_mode_text;
	ArdourCanvas::Text* timing_mode_text;

	static Gtkmm2ext::Color current_mode_color;
	static Gtkmm2ext::Color not_current_mode_color;

	void sequencer_changed (PBD::PropertyChange const &);
	bool mode_button_event (GdkEvent*, SequencerView::Mode);

	PBD::ScopedConnection sequencer_connection;

};

class SequenceView : public sigc::trackable
{
  public:
	SequenceView (SequencerView&, ARDOUR::StepSequence&, ArdourCanvas::Canvas* canvas);

	void view_mode_changed ();
	SequencerView::Mode mode() const { return sv.mode(); }
	ARDOUR::StepSequencer& sequencer() const  { return sv.sequencer(); }

	ArdourCanvas::ConstraintPacker* lhs_box;
	ArdourCanvas::ConstraintPacker* rhs_box;
	ArdourCanvas::ConstraintPacker* step_box;

   private:
	SequencerView& sv;
	ARDOUR::StepSequence& sequence;

	ArdourCanvas::Text* number_text;
	ArdourCanvas::Text* name_text;
	ArdourCanvas::Text* root_text;
	ArdourCanvas::Rectangle* step_cnt_button;
	ArdourCanvas::Rectangle* speed_slide;

	bool name_text_event (GdkEvent*);
	void edit_name ();
	void name_edited (std::string, int);

	FloatingTextEntry* floating_entry;

	typedef std::vector<StepView*> StepViews;
	StepViews step_views;

	void sequence_changed ();
	PBD::ScopedConnection sequence_connection;
};

class StepView : public ArdourCanvas::Rectangle, public sigc::trackable {
   public:
	StepView (SequenceView&, ARDOUR::Step&, ArdourCanvas::Canvas* canvas);

	void render (ArdourCanvas::Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	bool on_event (GdkEvent*);

	void view_mode_changed ();
	ARDOUR::StepSequencer& sequencer() const  { return sv.sequencer(); }

   private:
	ARDOUR::Step& _step;
	SequenceView& sv;
	ArdourCanvas::Text* text;
	bool grabbed;

	std::pair<double,double> grab_at;
	std::pair<double,double> last_motion;

	bool motion_event (GdkEventMotion*);
	bool button_press_event (GdkEventButton*);
	bool button_release_event (GdkEventButton*);
	bool scroll_event (GdkEventScroll*);

	void adjust_step_pitch (int amt);
	void adjust_step_velocity (int amt);
	void adjust_step_duration (ARDOUR::Step::DurationRatio const &);
	void adjust_step_octave (int amt);
	void adjust_step_timing (double fract);

	void step_changed (PBD::PropertyChange const &);
	PBD::ScopedConnection step_connection;

	void set_octave_text ();
	void set_group_text ();
	void set_timing_text ();

	static Gtkmm2ext::Color on_fill_color;
	static Gtkmm2ext::Color off_fill_color;
};

class SequencerStepIndicator : public ArdourCanvas::Rectangle, public sigc::trackable {
  public:
	SequencerStepIndicator (SequencerView&, ArdourCanvas::Canvas*, size_t n);
	void render (ArdourCanvas::Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	bool on_event (GdkEvent*);

	void set_current (bool);

  private:
	SequencerView& sv;
	size_t number;
	ArdourCanvas::Polygon* poly;
	ArdourCanvas::Text*    text;
	bool being_dragged;

	static int dragging;

	bool motion_event (GdkEventMotion*);
	bool button_press_event (GdkEventButton*);
	bool button_release_event (GdkEventButton*);

	void set_text ();

	void sequencer_changed (PBD::PropertyChange const &);
	PBD::ScopedConnection sequencer_connection;

	static Gtkmm2ext::Color current_color;
	static Gtkmm2ext::Color other_color;
	static Gtkmm2ext::Color current_text_color;
	static Gtkmm2ext::Color other_text_color;
	static Gtkmm2ext::Color bright_outline_color;
};

class BBGUI : public ArdourDialog {
  public:
	BBGUI (boost::shared_ptr<ARDOUR::BeatBox> bb);
	~BBGUI ();

	double width() const { return _width; }
	double height() const { return _height; }

  protected:
	void on_map ();
	void on_unmap ();

  private:
	boost::shared_ptr<ARDOUR::BeatBox> bbox;
	double _width;
	double _height;

	Gtk::Adjustment horizontal_adjustment;
	Gtk::Adjustment vertical_adjustment;

	ArdourCanvas::GtkCanvasViewport* _canvas_viewport;
	ArdourCanvas::GtkCanvas* _canvas;

	SequencerView* _sequencer;

	ArdourWidgets::ArdourButton start_button;
	void toggle_play ();

	ArdourWidgets::ArdourButton export_as_region_button;
	void export_as_region ();

	Gtk::HBox canvas_hbox;
	Gtk::VScrollbar vscrollbar;

	void clear ();
	void update ();
	void update_sequencer ();

	sigc::connection timer_connection;

	void sequencer_changed (PBD::PropertyChange const &);
	PBD::ScopedConnection sequencer_connection;
};

#endif /* __gtk2_ardour_beatbox_gui_h__ */
