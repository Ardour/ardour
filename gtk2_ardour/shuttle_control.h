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

#include "gtkmm2ext/binding_proxy.h"

#include "pbd/controllable.h"
#include "ardour/session_handle.h"

namespace Gtk {
	class Menu;
}

#include "ardour/types.h"

class ShuttleControl : public CairoWidget, public ARDOUR::SessionHandlePtr
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

		ShuttleControl& sc;
	};

	boost::shared_ptr<ShuttleControllable> controllable() const { return _controllable; }

  protected:
	bool _hovering;
	float  shuttle_max_speed;
	float  last_speed_displayed;
	bool   shuttle_grabbed;
	double shuttle_speed_on_grab;
	float shuttle_fract;
	boost::shared_ptr<ShuttleControllable> _controllable;
	cairo_pattern_t* pattern;
	cairo_pattern_t* shine_pattern;
	ARDOUR::microseconds_t last_shuttle_request;
	PBD::ScopedConnection parameter_connection;
	Gtk::Menu*        shuttle_unit_menu;
	Gtk::Menu*        shuttle_style_menu;
	Gtk::Menu*        shuttle_context_menu;
	BindingProxy      binding_proxy;

	void build_shuttle_context_menu ();
	void show_shuttle_context_menu ();
	void shuttle_style_changed();
	void shuttle_unit_clicked ();
	void set_shuttle_max_speed (float);

	bool on_enter_notify_event (GdkEventCrossing*);
	bool on_leave_notify_event (GdkEventCrossing*);
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event(GdkEventButton*);
	bool on_scroll_event (GdkEventScroll*);
	bool on_motion_notify_event(GdkEventMotion*);

	void render (cairo_t *);

	void on_size_allocate (Gtk::Allocation&);
	bool on_query_tooltip (int, int, bool, const Glib::RefPtr<Gtk::Tooltip>&);

	gint mouse_shuttle (double x, bool force);
	void use_shuttle_fract (bool force);
	void parameter_changed (std::string);

	void set_shuttle_units (ARDOUR::ShuttleUnits);
	void set_shuttle_style (ARDOUR::ShuttleBehaviour);

	int speed_as_semitones (float, bool&);
	int fract_as_semitones (float, bool&);

	float semitones_as_speed (int, bool);
	float semitones_as_fract (int, bool);
};

#endif /* __gtk2_ardour_shuttle_control_h__ */
