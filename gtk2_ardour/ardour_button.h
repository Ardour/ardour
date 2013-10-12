/*
    Copyright (C) 2010 Paul Davis

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

#ifndef __gtk2_ardour_ardour_button_h__
#define __gtk2_ardour_ardour_button_h__

#include <list>
#include <stdint.h>

#include <gtkmm/action.h>

#include "pbd/signals.h"
#include "gtkmm2ext/binding_proxy.h"
#include "gtkmm2ext/activatable.h"
#include "gtkmm2ext/cairo_widget.h"

class ArdourButton : public CairoWidget , public Gtkmm2ext::Activatable
{
  public:
	enum Element {
		Edge = 0x1,
		Body = 0x2,
		Text = 0x4,
		Indicator = 0x8,
		FlatFace = 0x10,
	};

	static Element default_elements;
	static Element led_default_elements;
	static Element just_led_default_elements;

	static void set_flat_buttons (bool yn);
	static bool flat_buttons() { return _flat_buttons; }

	ArdourButton (Element e = default_elements);
	ArdourButton (const std::string&, Element e = default_elements);
	virtual ~ArdourButton ();

	enum Tweaks {
		ShowClick = 0x1,
		NoModel = 0x2,
		ImplicitUsesSolidColor = 0x4,
	};

	Tweaks tweaks() const { return _tweaks; }
	void set_tweaks (Tweaks);

	void set_active_state (Gtkmm2ext::ActiveState);
	void set_visual_state (Gtkmm2ext::VisualState);

	Element elements() const { return _elements; }
	void set_elements (Element);
	void add_elements (Element);
	
	void set_corner_radius (float);
	void set_rounded_corner_mask (int);
	void set_diameter (float);

        void set_padding (int x, int y);

	void set_text (const std::string&);
	void set_markup (const std::string&);
	void set_angle (const double);
	void set_alignment (const float, const float);
	void get_alignment (float& xa, float& ya) {xa = _xalign; ya = _yalign;};

	void set_led_left (bool yn);
	void set_distinct_led_click (bool yn);

	Glib::RefPtr<Pango::Layout> layout() const { return _layout; }

	sigc::signal<void> signal_led_clicked;
	sigc::signal<void> signal_clicked;

	boost::shared_ptr<PBD::Controllable> get_controllable() { return binding_proxy.get_controllable(); }
 	void set_controllable (boost::shared_ptr<PBD::Controllable> c);
	void watch ();

	void set_related_action (Glib::RefPtr<Gtk::Action>);

	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);

	void set_image (const Glib::RefPtr<Gdk::Pixbuf>&);

  protected:
	void render (cairo_t *);
	void on_size_request (Gtk::Requisition* req);
	void on_size_allocate (Gtk::Allocation&);
	void on_style_changed (const Glib::RefPtr<Gtk::Style>&);
	bool on_enter_notify_event (GdkEventCrossing*);
	bool on_leave_notify_event (GdkEventCrossing*);

	void controllable_changed ();
	PBD::ScopedConnection watch_connection;

  private:
	Glib::RefPtr<Pango::Layout> _layout;
	Glib::RefPtr<Gdk::Pixbuf>   _pixbuf;
	std::string                 _text;
	Element                     _elements;
	Tweaks                      _tweaks;
	BindingProxy                binding_proxy;

	int   _text_width;
	int   _text_height;
	float _diameter;
	float _corner_radius;
	int   _corner_mask;

	double _angle;
	float _xalign, _yalign;

	uint32_t bg_color;
	uint32_t border_color;
	uint32_t fill_color_active;
	uint32_t fill_color_inactive;
	cairo_pattern_t* fill_pattern;
	cairo_pattern_t* fill_pattern_active;
	cairo_pattern_t* shine_pattern;
	cairo_pattern_t* led_inset_pattern;
	cairo_pattern_t* reflection_pattern;

	cairo_rectangle_t* _led_rect;

	double text_r;
	double text_g;
	double text_b;
	double text_a;

	double led_r;
	double led_g;
	double led_b;
	double led_a;

	double active_r;
	double active_g;
	double active_b;
	double active_a;

	bool _act_on_release;
	bool _led_left;
	bool _fixed_diameter;
	bool _distinct_led_click;
	bool _hovering;
    
        int _xpad_request;
        int _ypad_request;

	static bool _flat_buttons;

	void setup_led_rect ();
	void set_colors ();
	void color_handler ();

	void action_toggled ();

	void action_sensitivity_changed ();
	void action_visibility_changed ();
	void action_tooltip_changed ();
};

#endif /* __gtk2_ardour_ardour_button_h__ */
