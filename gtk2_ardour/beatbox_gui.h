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

#include "canvas/box.h"
#include "canvas/canvas.h"
#include "canvas/rectangle.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"
#include "ardour_dialog.h"

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

class SequencerGrid : public ArdourCanvas::Rectangle {
  public:
	SequencerGrid (ArdourCanvas::Canvas*);
	SequencerGrid (ArdourCanvas::Item*);

	void render (ArdourCanvas::Rect const &, Cairo::RefPtr<Cairo::Context>) const;
};

class SequencerStepIndicator : public ArdourCanvas::Rectangle {
  public:
	SequencerStepIndicator (ArdourCanvas::Item *, int n);
	void render (ArdourCanvas::Rect const &, Cairo::RefPtr<Cairo::Context>) const;
  private:
	ArdourCanvas::Polygon* poly;
	ArdourCanvas::Text*    text;
};

class BBGUI : public ArdourDialog {
  public:
	BBGUI (boost::shared_ptr<ARDOUR::BeatBox> bb);
	~BBGUI ();

  protected:
	void on_map ();
	void on_unmap ();

  private:
	boost::shared_ptr<ARDOUR::BeatBox> bbox;

	Gtk::Adjustment horizontal_adjustment;
	Gtk::Adjustment vertical_adjustment;

	ArdourCanvas::GtkCanvasViewport* _canvas_viewport;
	ArdourCanvas::GtkCanvas* _canvas;
	ArdourCanvas::ScrollGroup* v_scroll_group;
	ArdourCanvas::Container* no_scroll_group;

	SequencerGrid* _sequencer;
	ArdourCanvas::Rectangle* step_indicator_bg;
	ArdourCanvas::Container* step_indicator_box;

	ArdourWidgets::ArdourButton start_button;
	void toggle_play ();

	ArdourWidgets::ArdourButton export_as_region_button;
	void export_as_region ();

	Gtk::Button clear_button;
	Gtk::HBox misc_button_box;
	Gtk::HBox canvas_hbox;
	Gtk::VScrollbar vscrollbar;

	void clear ();
	void update ();
	void update_sequencer ();

	sigc::connection timer_connection;

	bool grid_event (GdkEvent*);
	bool grid_motion_event (GdkEventMotion*);
	bool grid_button_press_event (GdkEventButton*);
	bool grid_button_release_event (GdkEventButton*);
	bool grid_scroll_event (GdkEventScroll*);

	std::pair<int,int> grab_step;
	std::pair<double,double> grab_at;
	void set_grab_step (double x, double y);

	void adjust_step_pitch (int seq, int step, int amt);
};

#endif /* __gtk2_ardour_beatbox_gui_h__ */
