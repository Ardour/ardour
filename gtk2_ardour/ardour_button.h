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
		unused = 0x10,
		Menu = 0x20,
		Inactive = 0x40, // no _action is defined AND state is not used
		RecButton = 0x80, // tentative, see commit message
		RecTapeMode = 0x100, // tentative
		CloseCross = 0x200, // tentative
	};

	static Element default_elements;
	static Element led_default_elements;
	static Element just_led_default_elements;

	ArdourButton (Element e = default_elements);
	ArdourButton (const std::string&, Element e = default_elements);
	virtual ~ArdourButton ();

	enum Tweaks {
		Square = 0x1,
		TrackHeader = 0x2,
		unused3 = 0x4,
	};

	Tweaks tweaks() const { return _tweaks; }
	void set_tweaks (Tweaks);

	void set_active_state (Gtkmm2ext::ActiveState);
	void set_visual_state (Gtkmm2ext::VisualState);

	void set_act_on_release (bool onRelease) { _act_on_release = onRelease; }

	Element elements() const { return _elements; }
	void set_elements (Element);
	void add_elements (Element);

	void set_corner_radius (float);

	void set_text (const std::string&);
	const std::string& get_text () {return _text;}
	void set_angle (const double);
	void set_alignment (const float, const float);
	void get_alignment (float& xa, float& ya) {xa = _xalign; ya = _yalign;};

	void set_led_left (bool yn);
	void set_distinct_led_click (bool yn);

	void set_layout_ellisize_width (int w);
	void set_text_ellipsize (Pango::EllipsizeMode);

	sigc::signal<void> signal_led_clicked;
	sigc::signal<void> signal_clicked;

	boost::shared_ptr<PBD::Controllable> get_controllable() { return binding_proxy.get_controllable(); }
	void set_controllable (boost::shared_ptr<PBD::Controllable> c);
	void watch ();

	void set_related_action (Glib::RefPtr<Gtk::Action>);

	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);

	void set_image (const Glib::RefPtr<Gdk::Pixbuf>&);

	void set_fixed_colors (const uint32_t active_color, const uint32_t inactive_color);

	void set_fallthrough_to_parent(bool fall) { _fallthrough_to_parent = fall; }

	unsigned int char_pixel_width() { if (_char_pixel_width < 1) recalc_char_pixel_geometry() ; return _char_pixel_width; }
	unsigned int char_pixel_height() { if (_char_pixel_height < 1) recalc_char_pixel_geometry() ; return _char_pixel_height; }
	float char_avg_pixel_width() { if (_char_pixel_width < 1) recalc_char_pixel_geometry() ; return _char_avg_pixel_width; }

	protected:
	void render (cairo_t *, cairo_rectangle_t *);
	void on_size_request (Gtk::Requisition* req);
	void on_size_allocate (Gtk::Allocation&);
	void on_style_changed (const Glib::RefPtr<Gtk::Style>&);
	void on_name_changed ();
	void on_realize ();
	bool on_enter_notify_event (GdkEventCrossing*);
	bool on_leave_notify_event (GdkEventCrossing*);
	bool on_focus_in_event (GdkEventFocus*);
	bool on_focus_out_event (GdkEventFocus*);
	bool on_key_release_event (GdkEventKey *);

	void controllable_changed ();
	PBD::ScopedConnection watch_connection;

	protected:
	Glib::RefPtr<Pango::Layout> _layout;
	Glib::RefPtr<Gdk::Pixbuf>   _pixbuf;
	std::string                 _text;
	Element                     _elements;
	Tweaks                      _tweaks;
	BindingProxy                binding_proxy;

	void recalc_char_pixel_geometry ();
	unsigned int _char_pixel_width;
	unsigned int _char_pixel_height;
	float _char_avg_pixel_width;

	int   _text_width;
	int   _text_height;
	float _diameter;
	float _corner_radius;
	int   _corner_mask;

	double _angle;
	float _xalign, _yalign;

	uint32_t fill_inactive_color;
	uint32_t fill_active_color;

	uint32_t text_active_color;
	uint32_t text_inactive_color;

	uint32_t led_active_color;
	uint32_t led_inactive_color;

	cairo_pattern_t* convex_pattern;
	cairo_pattern_t* concave_pattern;
	cairo_pattern_t* led_inset_pattern;
	cairo_rectangle_t* _led_rect;

	bool _act_on_release;
	bool _led_left;
	bool _distinct_led_click;
	bool _hovering;
	bool _focused;
	bool _fixed_colors_set;
	bool _fallthrough_to_parent;
	int _layout_ellipsize_width;
	Pango::EllipsizeMode _ellipsis;
	bool _update_colors;
	int _pattern_height;

	void setup_led_rect ();
	void set_colors ();
	void color_handler ();
	void build_patterns ();
	void ensure_layout ();

	void action_toggled ();
	void action_sensitivity_changed ();
	void action_visibility_changed ();
	void action_tooltip_changed ();
};

#endif /* __gtk2_ardour_ardour_button_h__ */
