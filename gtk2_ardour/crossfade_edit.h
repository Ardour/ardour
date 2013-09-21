/*
    Copyright (C) 2000-2007 Paul Davis

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

#ifndef __gtk_ardour_xfade_edit_h__
#define __gtk_ardour_xfade_edit_h__

#include <list>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/radiobutton.h>

#include "canvas/canvas.h"

#include "evoral/Curve.hpp"
#include "ardour/session_handle.h"

#include "ardour_dialog.h"

namespace ARDOUR
{
	class Session;
	class AutomationList;
	class Crossfade;
}

namespace ArdourCanvas {
	class Rectangle;
	class Line;
	class Polygon;
	class WaveView;
}

class CrossfadeEditor : public ArdourDialog
{
  public:
	CrossfadeEditor (ARDOUR::Session*, boost::shared_ptr<ARDOUR::Crossfade>, double miny, double maxy);
	~CrossfadeEditor ();

	void apply ();

	static const double canvas_border;

	/* these are public so that a caller/subclass can make them do the right thing.
	 */

	Gtk::Button* cancel_button;
	Gtk::Button* ok_button;

	struct PresetPoint {
	    double x;
	    double y;

	    PresetPoint (double a, double b)
		    : x (a), y (b) {}
	};

	struct Preset : public std::list<PresetPoint> {
	    const char* name;
	    const char* image_name;

	    Preset (const char* n, const char* x) : name (n), image_name (x) {}
	};

	typedef std::list<Preset*> Presets;

	static Presets* fade_in_presets;
	static Presets* fade_out_presets;

  protected:
	bool on_key_press_event (GdkEventKey*);
	bool on_key_release_event (GdkEventKey*);

  private:
	boost::shared_ptr<ARDOUR::Crossfade> xfade;

	Gtk::VBox vpacker;

	struct Point {
	    ~Point();

	    ArdourCanvas::Rectangle* box;
	    ArdourCanvas::PolyLine* curve;
	    double x;
	    double y;

	    static const int32_t size;

	    void move_to (double x, double y, double xfract, double yfract);
	};

	struct PointSorter {
	    bool operator() (const CrossfadeEditor::Point* a, const CrossfadeEditor::Point *b) {
		    return a->x < b->x;
	    }
	};

	ArdourCanvas::Rectangle*   toplevel;
	ArdourCanvas::GtkCanvas* canvas;

	struct Half {
	    ArdourCanvas::PolyLine* line;
	    ArdourCanvas::Polygon*  shading;
	    std::list<Point*>       points;
	    ARDOUR::AutomationList  normative_curve; /* 0 - 1.0, linear */
	    ARDOUR::AutomationList  gain_curve;      /* 0 - 2.0, gain mapping */
	    std::vector<ArdourCanvas::WaveView*>  waves;

	    Half();
	};

	enum WhichFade {
		In = 0,
		Out = 1
	};

	Half fade[2];
	WhichFade current;

	bool point_grabbed;
	std::vector<Gtk::Button*> fade_out_buttons;
	std::vector<Gtk::Button*> fade_in_buttons;

	Gtk::VBox vpacker2;

	Gtk::Button clear_button;
	Gtk::Button revert_button;

	Gtk::ToggleButton audition_both_button;
	Gtk::ToggleButton audition_left_dry_button;
	Gtk::ToggleButton audition_left_button;
	Gtk::ToggleButton audition_right_dry_button;
	Gtk::ToggleButton audition_right_button;

	Gtk::ToggleButton preroll_button;
	Gtk::ToggleButton postroll_button;

	Gtk::HBox roll_box;

	gint event_handler (GdkEvent*);

	bool canvas_event (GdkEvent* event);
	bool point_event (GdkEvent* event, Point*);
	bool curve_event (GdkEvent* event);

	void canvas_allocation (Gtk::Allocation&);
	void add_control_point (double x, double y);
	Point* make_point ();
	void redraw ();

	double effective_width () const { return canvas->get_allocation().get_width() - (2.0 * canvas_border); }
	double effective_height () const { return canvas->get_allocation().get_height() - (2.0 * canvas_border); }

	void clear ();
	void reset ();

	double miny;
	double maxy;

	Gtk::Table fade_in_table;
	Gtk::Table fade_out_table;

	void build_presets ();
	void apply_preset (Preset*);

	Gtk::RadioButton select_in_button;
	Gtk::RadioButton select_out_button;
	Gtk::HBox   curve_button_box;
	Gtk::HBox   audition_box;

	void curve_select_clicked (WhichFade);

	double x_coordinate (double& xfract) const;
	double y_coordinate (double& yfract) const;

	void set (const ARDOUR::AutomationList& alist, WhichFade);

	PBD::ScopedConnection* _peaks_ready_connection;
	PBD::ScopedConnection state_connection;

	void make_waves (boost::shared_ptr<ARDOUR::AudioRegion>, WhichFade);
	void peaks_ready (boost::weak_ptr<ARDOUR::AudioRegion> r, WhichFade);

	void _apply_to (boost::shared_ptr<ARDOUR::Crossfade> xf);
	void setup (boost::shared_ptr<ARDOUR::Crossfade>);
	void cancel_audition ();
	void audition_state_changed (bool);

	enum Audition {
		Both,
		Left,
		Right
	};

	void audition_toggled ();
	void audition_right_toggled ();
	void audition_right_dry_toggled ();
	void audition_left_toggled ();
	void audition_left_dry_toggled ();

	void audition (Audition);
	void audition_both ();
	void audition_left_dry ();
	void audition_left ();
	void audition_right_dry ();
	void audition_right ();

	void xfade_changed (const PBD::PropertyChange&);

	void dump ();
};

#endif /* __gtk_ardour_xfade_edit_h__ */
