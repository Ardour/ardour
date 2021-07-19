/*
 * Copyright (C) 2011-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __gtk2_ardour_shuttle_control_h__
#define __gtk2_ardour_shuttle_control_h__

#include <gtkmm/drawingarea.h>

#include "pbd/controllable.h"

#include "ardour/session_handle.h"
#include "ardour/types.h"

#include "gtkmm2ext/cairo_widget.h"

#include "widgets/ardour_button.h"
#include "widgets/binding_proxy.h"

#include "varispeed_dialog.h"

namespace Gtk {
	class Menu;
}

class ShuttleInfoButton : public ArdourWidgets::ArdourButton, public ARDOUR::SessionHandlePtr
{
public:
	ShuttleInfoButton ();
	~ShuttleInfoButton ();

	bool on_button_press_event (GdkEventButton*);

	void set_shuttle_units (ARDOUR::ShuttleUnits s);

private:
	void                  parameter_changed (std::string);
	void                  build_disp_context_menu ();
	Gtk::Menu*            disp_context_menu;
	PBD::ScopedConnection parameter_connection;

	bool _ignore_change;
};

class ShuttleControl : public CairoWidget, public ARDOUR::SessionHandlePtr
{
public:
	ShuttleControl ();
	~ShuttleControl ();

	void   map_transport_state ();
	void   set_shuttle_fract (double, bool zero_ok = false);
	double get_shuttle_fract () const
	{
		return shuttle_fract;
	}
	void set_session (ARDOUR::Session*);

	void do_blink (bool);
	void set_colors ();

	struct ShuttleControllable : public PBD::Controllable {
		ShuttleControllable (ShuttleControl&);
		void   set_value (double, PBD::Controllable::GroupControlDisposition group_override);
		double get_value (void) const;

		double lower () const { return -1.0; }
		double upper () const { return 1.0; }

		ShuttleControl& sc;
	};

	boost::shared_ptr<ShuttleControllable> controllable () const
	{
		return _controllable;
	}

	ArdourWidgets::ArdourButton* info_button ()
	{
		return &_info_button;
	}

	ArdourWidgets::ArdourButton* vari_button ()
	{
		return &_vari_button;
	}

public:
	static int speed_as_semitones (float, bool&);
	static int fract_as_semitones (float, bool&);

	static float semitones_as_speed (int, bool);
	static float semitones_as_fract (int, bool);

	static int   speed_as_cents (float, bool&);
	static float cents_as_speed (int, bool);

protected:
	bool                                   _hovering;
	float                                  shuttle_max_speed;
	float                                  last_speed_displayed;
	bool                                   shuttle_grabbed;
	double                                 shuttle_speed_on_grab;
	double                                 requested_speed;
	float                                  shuttle_fract;
	boost::shared_ptr<ShuttleControllable> _controllable;
	cairo_pattern_t*                       pattern;
	cairo_pattern_t*                       shine_pattern;
	PBD::microseconds_t                    last_shuttle_request;
	PBD::ScopedConnection                  parameter_connection;
	ShuttleInfoButton                      _info_button;
	Gtk::Menu*                             shuttle_context_menu;
	ArdourWidgets::BindingProxy            binding_proxy;
	float                                  bg_r, bg_g, bg_b;
	void                                   build_shuttle_context_menu ();
	void                                   set_shuttle_max_speed (float);

	VarispeedDialog             _vari_dialog;
	ArdourWidgets::ArdourButton _vari_button;
	void                        varispeed_button_clicked ();
	bool                        varispeed_button_scroll_event (GdkEventScroll*);

	bool on_enter_notify_event (GdkEventCrossing*);
	bool on_leave_notify_event (GdkEventCrossing*);
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	bool on_motion_notify_event (GdkEventMotion*);

	bool on_button_press_event_for_display (GdkEventButton*);

	void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);

	void on_size_allocate (Gtk::Allocation&);
	bool on_query_tooltip (int, int, bool, const Glib::RefPtr<Gtk::Tooltip>&);

	gint mouse_shuttle (double x, bool force);
	void use_shuttle_fract (bool force, bool zero_ok = false);
	void parameter_changed (std::string);

	void set_shuttle_units (ARDOUR::ShuttleUnits);

	bool _ignore_change;
};

#endif /* __gtk2_ardour_shuttle_control_h__ */
