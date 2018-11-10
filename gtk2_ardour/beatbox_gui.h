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
#include "canvas/rectangle.h"

#include "ardour/step_sequencer.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"
#include "ardour_dialog.h"

namespace PBD {
class PropertyChange;
}

namespace ArdourCanvas {
class Grid;
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

class SequencerGrid;

class StepView : public ArdourCanvas::Rectangle, public sigc::trackable {
   public:
	StepView (SequencerGrid&, ARDOUR::Step&, ArdourCanvas::Item*);

	void render (ArdourCanvas::Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	bool on_event (GdkEvent*);

	void view_mode_changed ();

   private:
	ARDOUR::Step& _step;
	SequencerGrid& _seq;
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

	void step_changed (PBD::PropertyChange const &);
	PBD::ScopedConnection step_connection;

	void set_octave_text ();
	void set_group_text ();

	static Gtkmm2ext::Color on_fill_color;
	static Gtkmm2ext::Color off_fill_color;
};

class SequencerStepIndicator : public ArdourCanvas::Rectangle, public sigc::trackable {
  public:
	SequencerStepIndicator (SequencerGrid&, ArdourCanvas::Item *, size_t n);
	void render (ArdourCanvas::Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	bool on_event (GdkEvent*);

	void set_current (bool);

  private:
	SequencerGrid& grid;
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

class SequencerGrid : public ArdourCanvas::Rectangle, public sigc::trackable {
  public:
	enum Mode {
		Velocity,
		Pitch,
		Duration,
		Octave,
		Group,
	};

	SequencerGrid (ARDOUR::StepSequencer&, ArdourCanvas::Canvas*);

	ARDOUR::StepSequencer& sequencer() const { return _sequencer; }

	Mode mode() const { return _mode; }
	void set_mode (Mode m);

	void render (ArdourCanvas::Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	void update ();

  private:
	ARDOUR::StepSequencer& _sequencer;
	typedef std::vector<StepView*> StepViews;
	StepViews step_views;
	typedef std::vector<SequencerStepIndicator*> StepIndicators;
	StepIndicators step_indicators;
	double _width;
	double _height;
	Mode   _mode;
	ArdourCanvas::ScrollGroup* v_scroll_group;
	ArdourCanvas::Container* no_scroll_group;
	ArdourCanvas::Rectangle* step_indicator_bg;
	ArdourCanvas::Container* step_indicator_box;

	ArdourCanvas::Rectangle* velocity_mode_button;
	ArdourCanvas::Rectangle* pitch_mode_button;
	ArdourCanvas::Rectangle* octave_mode_button;
	ArdourCanvas::Rectangle* gate_mode_button;

	void sequencer_changed (PBD::PropertyChange const &);

	PBD::ScopedConnection sequencer_connection;
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

	SequencerGrid* _sequencer;

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

	Gtk::HBox mode_box;
	ArdourWidgets::ArdourButton mode_velocity_button;
	ArdourWidgets:: ArdourButton mode_pitch_button;
	ArdourWidgets::ArdourButton mode_octave_button;
	ArdourWidgets::ArdourButton mode_group_button;
	ArdourWidgets::ArdourButton mode_duration_button;

	void mode_clicked (SequencerGrid::Mode);

	PBD::ScopedConnection sequencer_connection;
};

#endif /* __gtk2_ardour_beatbox_gui_h__ */
