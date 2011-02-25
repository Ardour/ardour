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
#include <vector>

#include <glibmm/refptr.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/window.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/adjustment.h>

#include "pbd/cartesian.h"

#include "ardour_dialog.h"

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
	Panner2d (boost::shared_ptr<ARDOUR::Panner>, int32_t height);
	~Panner2d ();

	void allow_target_motion (bool);

	int  add_target (const PBD::AngularVector&);
	int  add_puck (const char* text, const PBD::AngularVector&);
	void move_puck (int which, const PBD::AngularVector&);
	void reset (uint32_t n_inputs);

	boost::shared_ptr<ARDOUR::Panner> get_panner() const { return panner; }

	sigc::signal<void,int> PuckMoved;
	sigc::signal<void,int> TargetMoved;

	void cart_to_gtk (PBD::CartesianVector&) const;
	void gtk_to_cart (PBD::CartesianVector&) const;

  protected:
	bool on_expose_event (GdkEventExpose *);
	bool on_button_press_event (GdkEventButton *);
	bool on_button_release_event (GdkEventButton *);
	bool on_motion_notify_event (GdkEventMotion *);
	bool on_scroll_event (GdkEventScroll *);
	void on_size_allocate (Gtk::Allocation& alloc);

  private:
	class Target {
	public:
		PBD::AngularVector position;
		bool visible;
		std::string text;
                
		Target (const PBD::AngularVector&, const char* txt = 0);
		~Target ();
                
		void set_text (const char*);
		void set_selected (bool yn) {
			_selected = yn;
		}
		bool selected() const { 
			return _selected;
		}
                
	private:
		bool _selected;
	};

	boost::shared_ptr<ARDOUR::Panner> panner;
	Glib::RefPtr<Pango::Layout> layout;

	typedef std::vector<Target*> Targets;
	Targets targets;
	Targets pucks;
        Target  position;

	Target *drag_target;
	int     drag_x;
	int     drag_y;
	bool    allow_target;
	int     width;
	int     height;
        int     dimen; 
        double  last_width;

	bool bypassflag;

	gint compute_x (float);
	gint compute_y (float);

	Target *find_closest_object (gdouble x, gdouble y);

	gint handle_motion (gint, gint, GdkModifierType);

	void toggle_bypass ();
	void handle_state_change ();
	void handle_position_change ();
        void label_pucks ();

	PBD::ScopedConnectionList connections;

	/* cartesian coordinates in GTK units ; adjust to same but on a circle of radius 1.0
	   and centered in the middle of our area
	*/
	void clamp_to_circle (double& x, double& y);
};

class Panner2dWindow : public ArdourDialog
{
  public:
	Panner2dWindow (boost::shared_ptr<ARDOUR::Panner>, int32_t height, uint32_t inputs);

	void reset (uint32_t n_inputs);

  private:
	Panner2d widget;

	Gtk::HBox         hpacker;
	Gtk::VBox         button_box;
	Gtk::ToggleButton bypass_button;
	Gtk::VBox         spinner_box;
	Gtk::VBox         left_side;

	std::vector<Gtk::SpinButton*> spinners;

        void bypass_toggled ();
        bool on_key_press_event (GdkEventKey*);
        bool on_key_release_event (GdkEventKey*);
};

#endif /* __ardour_panner_2d_h__ */
