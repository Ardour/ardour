/*
    Copyright (C) 2011 Paul Davis

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

#ifndef __gtk2_ardour_shuttle_control_h__
#define __gtk2_ardour_shuttle_control_h__

#include <gtkmm/drawingarea.h>

#include "pbd/controllable.h"
#include "ardour/session_handle.h"

namespace Gtk {
	class Menu;
}

#include "ardour/types.h"

class ShuttleControl : public Gtk::DrawingArea, public ARDOUR::SessionHandlePtr 
{
  public:
	ShuttleControl ();
	~ShuttleControl ();

	void map_transport_state ();
	void update_speed_display ();
	void set_shuttle_fract (double);
	double get_shuttle_fract () const { return shuttle_fract; }
	void set_session (ARDOUR::Session*);

	struct ShuttleControllable : public PBD::Controllable {
		ShuttleControllable (ShuttleControl&);
		void set_value (double);
		double get_value (void) const;
		
		void set_id (const std::string&);
		
		ShuttleControl& sc;
	};

	ShuttleControllable& controllable() { return _controllable; }

  protected:
	float  shuttle_max_speed;
	float  last_speed_displayed;
	bool   shuttle_grabbed;
	double shuttle_fract;
	ShuttleControllable _controllable;
	cairo_pattern_t* pattern;
	ARDOUR::microseconds_t last_shuttle_request;
	PBD::ScopedConnection parameter_connection;
	Gtk::Menu*        shuttle_unit_menu;
	Gtk::Menu*        shuttle_style_menu;
	Gtk::Menu*        shuttle_context_menu;

	void build_shuttle_context_menu ();
	void show_shuttle_context_menu ();
	void shuttle_style_changed();
	void shuttle_unit_clicked ();
	void set_shuttle_max_speed (float);

	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event(GdkEventButton*);
	bool on_scroll_event (GdkEventScroll*);
	bool on_motion_notify_event(GdkEventMotion*);
	bool on_expose_event(GdkEventExpose*);
	void on_size_allocate (Gtk::Allocation&);

	gint mouse_shuttle (double x, bool force);
	void use_shuttle_fract (bool force);
	void parameter_changed (std::string);

	void set_shuttle_units (ARDOUR::ShuttleUnits);
	void set_shuttle_style (ARDOUR::ShuttleBehaviour);
};

#endif /* __gtk2_ardour_shuttle_control_h__ */
