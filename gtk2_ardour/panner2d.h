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

    $Id$
*/

#ifndef __ardour_panner_2d_h__
#define __ardour_panner_2d_h__

#include <sys/types.h>
#include <map>
#include <string>
#include <gtk--.h>

using std::map;
using std::string;

namespace ARDOUR {
	class Panner;
}

class Panner2d : public Gtk::DrawingArea
{
  public:
	Panner2d (ARDOUR::Panner&, int32_t width, int32_t height);
	~Panner2d ();
	
	int puck_position (int which_puck, float& x, float& y);
	int target_position (int which_target, float& x, float& y);

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

	SigC::Signal1<void,int> PuckMoved;
	SigC::Signal1<void,int> TargetMoved;

  protected:
	gint expose_event_impl (GdkEventExpose *);
	gint button_press_event_impl (GdkEventButton *);
	gint button_release_event_impl (GdkEventButton *);
	gint motion_notify_event_impl (GdkEventMotion *);
	void size_allocate_impl (GtkAllocation* alloc);

  private:
	struct Target {
	    float x;
	    float y;
	    bool visible;
	    char* text;
	    size_t textlen;

	    Target (float xa, float ya, const char* txt = 0);
	    ~Target ();
	};

	ARDOUR::Panner& panner;
	Gtk::Menu* context_menu;
	Gtk::CheckMenuItem* bypass_menu_item;

	typedef map<int,Target *> Targets;
	Targets targets;
	Targets pucks;

	Target *drag_target;
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
	void show_context_menu ();
	void handle_state_change ();
};

#endif /* __ardour_panner_2d_h__ */
