/*
    Copyright (C) 2002 Paul Davis

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

#ifndef __ardour_panner_2d_h__
#define __ardour_panner_2d_h__

#include <sys/types.h>
#include <map>
#include <string>
#include <vector>

#include <glibmm/refptr.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/window.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/adjustment.h>

using std::map;
using std::string;

namespace ARDOUR {
	class Panner;
}

namespace Gtk {
	class Menu;
	class CheckMenuItem;
}

namespace Pango {
	class Layout;
}

class Panner2dWindow;

class Panner2d : public Gtk::DrawingArea
{
  public:
	Panner2d (ARDOUR::Panner&, int32_t height);
	~Panner2d ();
	
	void allow_x_motion(bool);
	void allow_y_motion(bool);
	void allow_target_motion (bool);

	void hide_puck (int which);
	void show_puck (int which);
	int  add_target (float x, float y);
	int  add_puck (const char* text, float x, float y);
	void hide_target (int);
	void show_target (int);
	void remove_target (int);
	void drop_targets ();
	void drop_pucks ();
	void move_target (int, float x, float y);
	void move_puck (int, float x, float y);
	void reset (uint32_t n_inputs);

	Gtk::Adjustment& azimuth (uint32_t which);

	ARDOUR::Panner& get_panner() const { return panner; }
	
	sigc::signal<void,int> PuckMoved;
	sigc::signal<void,int> TargetMoved;

  protected:
	bool on_expose_event (GdkEventExpose *);
	bool on_button_press_event (GdkEventButton *);
	bool on_button_release_event (GdkEventButton *);
	bool on_motion_notify_event (GdkEventMotion *);
	void on_size_allocate (Gtk::Allocation& alloc);

  private:
	struct Target {
	    Gtk::Adjustment x;
	    Gtk::Adjustment y;
	    Gtk::Adjustment azimuth;
	    bool visible;
	    char* text;

	    Target (float xa, float ya, const char* txt = 0);
	    ~Target ();

	    void set_text (const char*);
	};

	ARDOUR::Panner& panner;
	Glib::RefPtr<Pango::Layout> layout;

	typedef std::map<int,Target *> Targets;
	Targets targets;
	Targets pucks;

	Target *drag_target;
	int drag_x;
	int drag_y;
	int     drag_index;
	bool    drag_is_puck;
	bool  allow_x;
	bool  allow_y;
	bool  allow_target;
	int width;
	int height;

	bool bypassflag;
	
	gint compute_x (float);
	gint compute_y (float);

	Target *find_closest_object (gdouble x, gdouble y, int& which, bool& is_puck) const;

	gint handle_motion (gint, gint, GdkModifierType);

	void toggle_bypass ();
	void handle_state_change ();
	void handle_position_change ();
};

class Panner2dWindow : public Gtk::Window
{
  public:
	Panner2dWindow (ARDOUR::Panner&, int32_t height, uint32_t inputs);
	
	void reset (uint32_t n_inputs);

  private:
	Panner2d widget;

	Gtk::HBox   hpacker;
	Gtk::VBox   button_box;
	Gtk::Button reset_button;
	Gtk::ToggleButton bypass_button;
	Gtk::ToggleButton mute_button;
	Gtk::VBox   spinner_box;
	Gtk::VBox   left_side;

	std::vector<Gtk::SpinButton*> spinners;
};

#endif /* __ardour_panner_2d_h__ */
