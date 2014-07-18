/*
    Copyright (C) 2014 Paul Davis

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

#ifndef __gtk2_ardour_ardour_knob_h__
#define __gtk2_ardour_ardour_knob_h__

#include <list>
#include <stdint.h>

#include <gtkmm/action.h>

#include "pbd/signals.h"
#include "gtkmm2ext/binding_proxy.h"
#include "gtkmm2ext/activatable.h"
#include "gtkmm2ext/cairo_widget.h"

class ArdourKnob : public CairoWidget , public Gtkmm2ext::Activatable
{
public:

	enum Element {
		Arc = 0x1,
		Bevel = 0x2,
		unused2 = 0x4,
		unused3 = 0x8,
		unused4 = 0x10,
		unused5 = 0x20,
	};

	ArdourKnob (Element e = default_elements);
	virtual ~ArdourKnob ();

	void set_active_state (Gtkmm2ext::ActiveState);
	void set_visual_state (Gtkmm2ext::VisualState);

	Element elements() const { return _elements; }
	void set_elements (Element);
	void add_elements (Element);
	static Element default_elements;

	boost::shared_ptr<PBD::Controllable> get_controllable() { return binding_proxy.get_controllable(); }
 	void set_controllable (boost::shared_ptr<PBD::Controllable> c);

	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	bool on_scroll_event (GdkEventScroll* ev);
	bool on_motion_notify_event (GdkEventMotion *ev) ;

	void color_handler ();

  protected:
	void render (cairo_t *, cairo_rectangle_t *);
	void on_size_request (Gtk::Requisition* req);
	void on_size_allocate (Gtk::Allocation&);
	void on_style_changed (const Glib::RefPtr<Gtk::Style>&);
	void on_name_changed ();
	bool on_enter_notify_event (GdkEventCrossing*);
	bool on_leave_notify_event (GdkEventCrossing*);
	bool on_focus_in_event (GdkEventFocus*);
	bool on_focus_out_event (GdkEventFocus*);

	void controllable_changed ();
	PBD::ScopedConnection watch_connection;


  private:
	Element                     _elements;

	BindingProxy                binding_proxy;

	bool _hovering;
	bool _grabbed;
	float _grabbed_y;

	float _val;  //percent of knob travel

	void action_sensitivity_changed ();
	void action_visibility_changed ();
	void action_tooltip_changed ();
};

#endif /* __gtk2_ardour_ardour_knob_h__ */
