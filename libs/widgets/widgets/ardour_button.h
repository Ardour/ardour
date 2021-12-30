/*
 * Copyright (C) 2010 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
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

#ifndef _WIDGETS_ARDOUR_BUTTON_H_
#define _WIDGETS_ARDOUR_BUTTON_H_

#include <list>
#include <stdint.h>

#include <gtkmm/action.h>

#include "pbd/signals.h"
#include "gtkmm2ext/activatable.h"
#include "gtkmm2ext/cairo_widget.h"

#include "widgets/ardour_icon.h"
#include "widgets/binding_proxy.h"
#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API ArdourButton : public CairoWidget , public Gtkmm2ext::Activatable
{
	public:
	enum Element {
		Edge = 0x1,
		Body = 0x2,
		Text = 0x4,
		Indicator = 0x8,
		ColorBox = 0x18,  //also sets Indicator
		Menu = 0x20,
		Inactive = 0x40, // no _action is defined AND state is not used
		VectorIcon = 0x80,
		IconRenderCallback = 0x100,
	};

	typedef void (* rendercallback_t) (cairo_t*, int, int, uint32_t, void*);

	static Element default_elements;
	static Element led_default_elements;
	static Element just_led_default_elements;

	ArdourButton (Element e = default_elements, bool toggle = false);
	ArdourButton (const std::string&, Element e = default_elements, bool toggle = false);
	virtual ~ArdourButton ();

	enum Tweaks {
		Square = 0x1,
		TrackHeader = 0x2,
		OccasionalText = 0x4,
		OccasionalLED = 0x8,
		ForceBoxy = 0x10,
		ForceFlat = 0x20,
	};

	Tweaks tweaks() const { return _tweaks; }
	void set_tweaks (Tweaks);

	void set_active_state (Gtkmm2ext::ActiveState);
	void set_visual_state (Gtkmm2ext::VisualState);

	void set_custom_led_color (const uint32_t c, const bool useit = true);

	void set_act_on_release (bool onRelease) { _act_on_release = onRelease; }

	Element elements() const { return _elements; }
	void set_elements (Element);
	void add_elements (Element);

	ArdourIcon::Icon icon() const { return _icon; }
	void set_icon (ArdourIcon::Icon);
	void set_icon (rendercallback_t, void*);

	void set_corner_radius (float);

	void set_text (const std::string&, bool markup = false);
	const std::string& get_text () const { return _text; }
	bool get_markup () const { return _markup; }
	void set_angle (const double);
	void set_alignment (const float, const float);
	void get_alignment (float& xa, float& ya) {xa = _xalign; ya = _yalign;};

	void set_led_left (bool yn);
	void set_distinct_led_click (bool yn);

	void set_layout_ellipsize_width (int w);
	void set_layout_font (const Pango::FontDescription&);
	void set_text_ellipsize (Pango::EllipsizeMode);

    /* Sets the text used for size request computation. Pass an
     * empty string to return to the default behavior which uses
     * the currently displayed text for measurement. */
	void set_sizing_text (const std::string&);
	const std::string& get_sizing_text () {return _sizing_text;}

	sigc::signal<void, GdkEventButton*> signal_led_clicked;
	sigc::signal<void> signal_clicked;

	boost::shared_ptr<PBD::Controllable> get_controllable() { return binding_proxy.get_controllable(); }
	void set_controllable (boost::shared_ptr<PBD::Controllable> c);
	void watch ();

	void set_related_action (Glib::RefPtr<Gtk::Action>);

	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);

	void set_image (const Glib::RefPtr<Gdk::Pixbuf>&);

	void set_fixed_colors   (const uint32_t active_color, const uint32_t inactive_color);
	void set_active_color   (const uint32_t active_color);
	void set_inactive_color (const uint32_t inactive_color);
	void reset_fixed_colors ();

	void set_fallthrough_to_parent(bool fall) { _fallthrough_to_parent = fall; }

	unsigned int char_pixel_width() { if (_char_pixel_width < 1) recalc_char_pixel_geometry() ; return _char_pixel_width; }
	unsigned int char_pixel_height() { if (_char_pixel_height < 1) recalc_char_pixel_geometry() ; return _char_pixel_height; }
	float char_avg_pixel_width() { if (_char_pixel_width < 1) recalc_char_pixel_geometry() ; return _char_avg_pixel_width; }

	protected:
	void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);
	void on_size_request (Gtk::Requisition* req);
	void on_size_allocate (Gtk::Allocation&);
	void on_style_changed (const Glib::RefPtr<Gtk::Style>&);
	void on_name_changed ();
	void on_realize ();
	bool on_enter_notify_event (GdkEventCrossing*);
	bool on_leave_notify_event (GdkEventCrossing*);
	bool on_grab_broken_event(GdkEventGrabBroken*);
	bool on_focus_in_event (GdkEventFocus*);
	bool on_focus_out_event (GdkEventFocus*);
	bool on_key_release_event (GdkEventKey *);
	bool on_key_press_event (GdkEventKey *);

	void controllable_changed ();
	PBD::ScopedConnection watch_connection;

	protected:
	Glib::RefPtr<Pango::Layout> _layout;
	Glib::RefPtr<Gdk::Pixbuf>   _pixbuf;
	std::string                 _text;
	std::string                 _sizing_text;
	bool                        _markup;
	Element                     _elements;
	ArdourIcon::Icon            _icon;
	rendercallback_t            _icon_render_cb;
	void*                       _icon_render_cb_data;
	Tweaks                      _tweaks;
	BindingProxy                binding_proxy;

	void set_text_internal ();
	void recalc_char_pixel_geometry ();

	unsigned int _char_pixel_width;
	unsigned int _char_pixel_height;
	float        _char_avg_pixel_width;
	bool         _custom_font_set;

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
	uint32_t led_custom_color;
	bool     use_custom_led_color;

	cairo_pattern_t* convex_pattern;
	cairo_pattern_t* concave_pattern;
	cairo_pattern_t* led_inset_pattern;
	cairo_rectangle_t* _led_rect;

	bool _act_on_release;
	bool _auto_toggle;
	bool _led_left;
	bool _distinct_led_click;
	bool _hovering;
	bool _focused;
	int  _fixed_colors_set;
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

} /* end namespace */

#endif
