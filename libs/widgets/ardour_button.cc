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

#include <iostream>
#include <cmath>
#include <algorithm>

#include <pangomm/layout.h>
#include <gtkmm/toggleaction.h>

#include "pbd/compose.h"
#include "pbd/controllable.h"
#include "pbd/error.h"

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/utils.h"

#include "widgets/ardour_button.h"
#include "widgets/tooltips.h"
#include "widgets/ui_config.h"

#include "pbd/i18n.h"

#define BASELINESTRETCH (1.25)
#define TRACKHEADERBTNW (3.10)

using namespace Gtk;
using namespace Glib;
using namespace PBD;
using namespace ArdourWidgets;
using std::max;
using std::min;
using namespace std;

ArdourButton::Element ArdourButton::default_elements = ArdourButton::Element (ArdourButton::Edge|ArdourButton::Body|ArdourButton::Text);
ArdourButton::Element ArdourButton::led_default_elements = ArdourButton::Element (ArdourButton::default_elements|ArdourButton::Indicator);
ArdourButton::Element ArdourButton::just_led_default_elements = ArdourButton::Element (ArdourButton::Edge|ArdourButton::Body|ArdourButton::Indicator);

ArdourButton::ArdourButton (Element e, bool toggle)
	: _sizing_text("")
	, _markup (false)
	, _elements (e)
	, _icon (ArdourIcon::NoIcon)
	, _icon_render_cb (0)
	, _icon_render_cb_data (0)
	, _tweaks (Tweaks (0))
	, _char_pixel_width (0)
	, _char_pixel_height (0)
	, _char_avg_pixel_width (0)
	, _custom_font_set (false)
	, _text_width (0)
	, _text_height (0)
	, _diameter (0)
	, _corner_radius (3.5)
	, _corner_mask (0xf)
	, _angle(0)
	, _xalign(.5)
	, _yalign(.5)
	, fill_inactive_color (0)
	, fill_active_color (0)
	, text_active_color(0)
	, text_inactive_color(0)
	, led_active_color(0)
	, led_inactive_color(0)
	, led_custom_color (0)
	, use_custom_led_color (false)
	, convex_pattern (0)
	, concave_pattern (0)
	, led_inset_pattern (0)
	, _led_rect (0)
	, _act_on_release (true)
	, _auto_toggle (toggle)
	, _led_left (false)
	, _distinct_led_click (false)
	, _hovering (false)
	, _focused (false)
	, _fixed_colors_set (false)
	, _fallthrough_to_parent (false)
	, _layout_ellipsize_width (-1)
	, _ellipsis (Pango::ELLIPSIZE_NONE)
	, _update_colors (true)
	, _pattern_height (0)
{
	UIConfigurationBase::instance().ColorsChanged.connect (sigc::mem_fun (*this, &ArdourButton::color_handler));
	/* This is not provided by gtkmm */
	signal_grab_broken_event().connect (sigc::mem_fun (*this, &ArdourButton::on_grab_broken_event));
}

ArdourButton::ArdourButton (const std::string& str, Element e, bool toggle)
	: _sizing_text("")
	, _markup (false)
	, _elements (e)
	, _icon (ArdourIcon::NoIcon)
	, _tweaks (Tweaks (0))
	, _char_pixel_width (0)
	, _char_pixel_height (0)
	, _char_avg_pixel_width (0)
	, _custom_font_set (false)
	, _text_width (0)
	, _text_height (0)
	, _diameter (0)
	, _corner_radius (3.5)
	, _corner_mask (0xf)
	, _angle(0)
	, _xalign(.5)
	, _yalign(.5)
	, fill_inactive_color (0)
	, fill_active_color (0)
	, text_active_color(0)
	, text_inactive_color(0)
	, led_active_color(0)
	, led_inactive_color(0)
	, led_custom_color (0)
	, use_custom_led_color (false)
	, convex_pattern (0)
	, concave_pattern (0)
	, led_inset_pattern (0)
	, _led_rect (0)
	, _act_on_release (true)
	, _auto_toggle (toggle)
	, _led_left (false)
	, _distinct_led_click (false)
	, _hovering (false)
	, _focused (false)
	, _fixed_colors_set (false)
	, _fallthrough_to_parent (false)
	, _layout_ellipsize_width (-1)
	, _ellipsis (Pango::ELLIPSIZE_NONE)
	, _update_colors (true)
	, _pattern_height (0)
{
	set_text (str);
	UIConfigurationBase::instance().ColorsChanged.connect (sigc::mem_fun (*this, &ArdourButton::color_handler));
	UIConfigurationBase::instance().DPIReset.connect (sigc::mem_fun (*this, &ArdourButton::on_name_changed));
	/* This is not provided by gtkmm */
	signal_grab_broken_event().connect (sigc::mem_fun (*this, &ArdourButton::on_grab_broken_event));
}

ArdourButton::~ArdourButton()
{
	delete _led_rect;

	if (convex_pattern) {
		cairo_pattern_destroy (convex_pattern);
	}

	if (concave_pattern) {
		cairo_pattern_destroy (concave_pattern);
	}

	if (led_inset_pattern) {
		cairo_pattern_destroy (led_inset_pattern);
	}
}

void
ArdourButton::set_layout_font (const Pango::FontDescription& fd)
{
	ensure_layout ();
	if (_layout) {
		_layout->set_font_description (fd);
		queue_resize ();
		_char_pixel_width = 0;
		_char_pixel_height = 0;
		_custom_font_set = true;
	}
}

void
ArdourButton::set_text_internal () {
	assert (_layout);
	if (_markup) {
		_layout->set_markup (_text);
	} else {
		_layout->set_text (_text);
	}
}

void
ArdourButton::set_text (const std::string& str, bool markup)
{
	if (!(_elements & Text)) {
		return;
	}
	if (_text == str && _markup == markup) {
		return;
	}

	_text = str;
	_markup = markup;
	if (!is_realized()) {
		return;
	}
	ensure_layout ();
	if (_layout && _layout->get_text() != _text) {
		set_text_internal ();
		/* on_size_request() will fill in _text_width/height
		 * so queue it even if _sizing_text != "" */
		if (_sizing_text.empty ()) {
			queue_resize ();
		} else {
			_layout->get_pixel_size (_text_width, _text_height);
			CairoWidget::set_dirty ();
		}
	}
}

void
ArdourButton::set_sizing_text (const std::string& str)
{
	if (_sizing_text == str) {
		return;
	}
	_sizing_text = str;
	queue_resize ();
}

void
ArdourButton::set_angle (const double angle)
{
	_angle = angle;
}

void
ArdourButton::set_alignment (const float xa, const float ya)
{
	_xalign = xa;
	_yalign = ya;
}


/* TODO make this a dedicated function elsewhere.
 *
 * Option 1:
 * virtual ArdourButton::render_vector_icon()
 * ArdourIconButton::render_vector_icon
 *
 * Option 2:
 * ARDOUR_UI_UTILS::render_vector_icon()
 */
void
ArdourButton::render (Cairo::RefPtr<Cairo::Context> const& ctx, cairo_rectangle_t*)
{
	cairo_t* cr = ctx->cobj();

	uint32_t text_color;
	uint32_t led_color;

	const bool boxy = (_tweaks & ForceBoxy) | boxy_buttons ();
	const bool flat = (_tweaks & ForceFlat) | flat_buttons ();

	const float corner_radius = boxy ? 0 : std::max(2.f, _corner_radius * UIConfigurationBase::instance().get_ui_scale());

	const float scale = UIConfigurationBase::instance().get_ui_scale();

	if (_update_colors) {
		set_colors ();
	}
	if (get_height() != _pattern_height) {
		build_patterns ();
	}

	if ( active_state() == Gtkmm2ext::ExplicitActive ) {
		text_color = text_active_color;
		led_color = led_active_color;
	} else {
		text_color = text_inactive_color;
		led_color = led_inactive_color;
	}

	if (use_custom_led_color) {
		led_color = led_custom_color;
	}

	void (*rounded_function)(cairo_t*, double, double, double, double, double);

	switch (_corner_mask) {
	case 0x1: /* upper left only */
		rounded_function = Gtkmm2ext::rounded_top_left_rectangle;
		break;
	case 0x2: /* upper right only */
		rounded_function = Gtkmm2ext::rounded_top_right_rectangle;
		break;
	case 0x3: /* upper only */
		rounded_function = Gtkmm2ext::rounded_top_rectangle;
		break;
		/* should really have functions for lower right, lower left,
		   lower only, but for now, we don't
		*/
	default:
		rounded_function = Gtkmm2ext::rounded_rectangle;
	}

	// draw edge (filling a rect underneath, rather than stroking a border on top, allows the corners to be lighter-weight.
	if ((_elements & (Body|Edge)) == (Body|Edge)) {
		rounded_function (cr, 0, 0, get_width(), get_height(), corner_radius + 1.5);
		cairo_set_source_rgba (cr, 0, 0, 0, 1);
		cairo_fill(cr);
	}

	// background fill
	if ((_elements & Body)==Body) {
		rounded_function (cr, 1, 1, get_width() - 2, get_height() - 2, corner_radius);
		if (active_state() == Gtkmm2ext::ImplicitActive && !((_elements & Indicator)==Indicator)) {
			Gtkmm2ext::set_source_rgba (cr, fill_inactive_color);
			cairo_fill (cr);
		} else if ( (active_state() == Gtkmm2ext::ExplicitActive) && !((_elements & Indicator)==Indicator) ) {
			//background color
			Gtkmm2ext::set_source_rgba (cr, fill_active_color);
			cairo_fill (cr);
		} else {  //inactive, or it has an indicator
			//background color
			Gtkmm2ext::set_source_rgba (cr, fill_inactive_color);
		}
		cairo_fill (cr);
	}

	// IMPLICIT ACTIVE: draw a border of the active color
	if ((_elements & Body)==Body) {
		if (active_state() == Gtkmm2ext::ImplicitActive && !((_elements & Indicator)==Indicator)) {
			cairo_set_line_width (cr, 2.0);
			rounded_function (cr, 2, 2, get_width() - 4, get_height() - 4, corner_radius-0.5);
			Gtkmm2ext::set_source_rgba (cr, fill_active_color);
			cairo_stroke (cr);
		}
	}

	//show the "convex" or "concave" gradient
	if (!flat && (_elements & Body)==Body) {
		if ( active_state() == Gtkmm2ext::ExplicitActive && ( !((_elements & Indicator)==Indicator) || use_custom_led_color) ) {
			//concave
			cairo_set_source (cr, concave_pattern);
			Gtkmm2ext::rounded_rectangle (cr, 1, 1, get_width() - 2, get_height() - 2, corner_radius);
			cairo_fill (cr);
		} else {
			cairo_set_source (cr, convex_pattern);
			Gtkmm2ext::rounded_rectangle (cr, 1, 1, get_width() - 2, get_height() - 2, corner_radius);
			cairo_fill (cr);
		}
	}

	const int text_margin = char_pixel_width();

	//Pixbuf, if any
	if (_pixbuf) {
		double x = rint((get_width() - _pixbuf->get_width()) * .5);
		const double y = rint((get_height() - _pixbuf->get_height()) * .5);
#if 0 // DEBUG style (print on hover)
		if (_hovering || (_elements & Inactive)) {
			printf("%s: p:%dx%d (%dx%d)\n",
					get_name().c_str(),
					_pixbuf->get_width(), _pixbuf->get_height(),
					get_width(), get_height());
		}
#endif
		if (_elements & Menu) {
			//if this is a DropDown with an icon, then we need to
			//move the icon left slightly to accomomodate the arrow
			x -= _diameter - 2;
		}
		cairo_rectangle (cr, x, y, _pixbuf->get_width(), _pixbuf->get_height());
		gdk_cairo_set_source_pixbuf (cr, _pixbuf->gobj(), x, y);
		cairo_fill (cr);
	}
	else /* VectorIcon, IconRenderCallback are exclusive to Pixbuf Icons */
	if (_elements & (VectorIcon | IconRenderCallback)) {
		int vw = get_width();
		int vh = get_height();
		cairo_save (cr);

		if (_elements & Menu) {
			vw -= _diameter + 4;
		}
		if (_elements & Indicator) {
			vw -= _diameter + .5 * text_margin;
			if (_led_left) {
				cairo_translate (cr, _diameter + text_margin, 0);
			}
		}
		if (_elements & Text) {
			vw -= _text_width + text_margin;
		}
		if (_elements & VectorIcon) {
			ArdourIcon::render (cr, _icon, vw, vh, active_state(), text_color);
		} else {
			rounded_function (cr, 0, 0, get_width(), get_height(), corner_radius + 1.5);
			cairo_clip (cr);
			_icon_render_cb (cr, vw, vh, text_color, _icon_render_cb_data);
		}
		cairo_restore (cr);
	}

	// Text, if any
	if (!_pixbuf && ((_elements & Text)==Text) && !_text.empty()) {
		assert(_layout);
#if 0 // DEBUG style (print on hover)
		if (_hovering || (_elements & Inactive)) {
			bool layout_font = true;
			Pango::FontDescription fd = _layout->get_font_description();
			if (fd.gobj() == NULL) {
				layout_font = false;
				fd = get_pango_context()->get_font_description();
			}
			printf("%s: f:%dx%d aw:%.3f bh:%.0f t:%dx%d (%dx%d) %s\"%s\"\n",
					get_name().c_str(),
					char_pixel_width(), char_pixel_height(), char_avg_pixel_width(),
					ceil(char_pixel_height() * BASELINESTRETCH),
					_text_width, _text_height,
					get_width(), get_height(),
					layout_font ? "L:" : "W:",
					fd.to_string().c_str());
		}
#endif

		cairo_save (cr);
		cairo_rectangle (cr, 2, 1, get_width() - 4, get_height() - 2);
		cairo_clip(cr);

		cairo_new_path (cr);
		Gtkmm2ext::set_source_rgba (cr, text_color);
		const double text_ypos = round ((get_height() - _text_height) * .5);

		if (_elements & Menu) {
			// always left align (dropdown)
			cairo_move_to (cr, text_margin, text_ypos);
			pango_cairo_show_layout (cr, _layout->gobj());
		} else if ( (_elements & Indicator)  == Indicator) {
			// left/right align depending on LED position
			if (_led_left) {
				cairo_move_to (cr, round (text_margin + _diameter + .5 * char_pixel_width()), text_ypos);
			} else {
				cairo_move_to (cr, text_margin, text_ypos);
			}
			pango_cairo_show_layout (cr, _layout->gobj());
		} else if (VectorIcon == (_elements & VectorIcon)) {
			cairo_move_to (cr, get_width () - text_margin - _text_width, text_ypos);
			pango_cairo_show_layout (cr, _layout->gobj());
		} else {
			/* centered text otherwise */
			double ww, wh;
			double xa, ya;
			ww = get_width();
			wh = get_height();

			cairo_matrix_t m1;
			cairo_get_matrix (cr, &m1);
			cairo_matrix_t m2 = m1;
			m2.x0 = 0;
			m2.y0 = 0;
			cairo_set_matrix (cr, &m2);

			if (_angle) {
				cairo_rotate(cr, _angle * M_PI / 180.0);
			}

			cairo_device_to_user(cr, &ww, &wh);
			xa = text_margin + (ww - _text_width - 2 * text_margin) * _xalign;
			ya = (wh - _text_height) * _yalign;

			/* quick hack for left/bottom alignment at -90deg
			 * TODO this should be generalized incl rotation.
			 * currently only 'user' of this API is meter_strip.cc
			 */
			if (_xalign < 0) xa = ceil(.5 + (ww * fabs(_xalign) + text_margin));

			cairo_move_to (cr, round (xa + m1.x0), round (ya + m1.y0));
			pango_cairo_update_layout(cr, _layout->gobj());
			pango_cairo_show_layout (cr, _layout->gobj());
		}
		cairo_restore (cr);
	}

	//Menu "triangle"
	if (_elements & Menu) {
		const float trih = ceil(_diameter * .5);
		const float triw2 = ceil(.577 * _diameter * .5); // 1/sqrt(3) Equilateral triangle
		//menu arrow
		cairo_set_source_rgba (cr, 1, 1, 1, 0.4);
		cairo_move_to(cr, get_width() - triw2 - 3. , rint((get_height() + trih) * .5));
		cairo_rel_line_to(cr, -triw2, -trih);
		cairo_rel_line_to(cr, 2. * triw2, 0);
		cairo_close_path(cr);

		cairo_set_source_rgba (cr, 1, 1, 1, 0.4);
		cairo_fill(cr);

		cairo_move_to(cr, get_width() - triw2 - 3 , rint((get_height() + trih) * .5));
		cairo_rel_line_to(cr, .5 - triw2, .5 - trih);
		cairo_rel_line_to(cr, 2. * triw2 - 1, 0);
		cairo_close_path(cr);
		cairo_set_source_rgba (cr, 0, 0, 0, 0.8);
		cairo_set_line_width(cr, 1);
		cairo_stroke(cr);
	}

	//Indicator LED
	if ((_elements & ColorBox)==ColorBox) {
		cairo_save (cr);

		/* move to the center of the indicator/led */
		if (_elements & (Text | VectorIcon | IconRenderCallback)) {
			int led_xoff = ceil((char_pixel_width() + _diameter) * .5);
			if (_led_left) {
				cairo_translate (cr, led_xoff, get_height() * .5);
			} else {
				cairo_translate (cr, get_width() - led_xoff, get_height() * .5);
			}
		} else {
			cairo_translate (cr, get_width() * .5, get_height() * .5);
		}

		float size = ceil(std::min (get_width(), get_height())/2 - 3*scale);

		//black border
		cairo_set_source_rgb (cr, 0, 0, 0);
		rounded_function (cr, -size, -size, size*2, size*2, corner_radius - 1*scale);
		cairo_fill(cr);

		//inset by 1 px
		size = size - 1*scale;

		//led color
		Gtkmm2ext::set_source_rgba (cr, led_color);
		rounded_function (cr, -size, -size, size*2, size*2, corner_radius - 2*scale);
		cairo_fill(cr);

		cairo_restore (cr);
	} else if (_elements & Indicator) {
		cairo_save (cr);

		/* move to the center of the indicator/led */
		if (_elements & (Text | VectorIcon | IconRenderCallback)) {
			int led_xoff = ceil((char_pixel_width() + _diameter) * .5);
			if (_led_left) {
				cairo_translate (cr, led_xoff, get_height() * .5);
			} else {
				cairo_translate (cr, get_width() - led_xoff, get_height() * .5);
			}
		} else {
			cairo_translate (cr, get_width() * .5, get_height() * .5);
		}

		//inset
		if (!flat) {
			cairo_arc (cr, 0, 0, _diameter * .5, 0, 2 * M_PI);
			cairo_set_source (cr, led_inset_pattern);
			cairo_fill (cr);
		}

		//black ring
		cairo_set_source_rgb (cr, 0, 0, 0);
		cairo_arc (cr, 0, 0, _diameter * .5 - 1 * UIConfigurationBase::instance().get_ui_scale(), 0, 2 * M_PI);
		cairo_fill(cr);

		//led color
		Gtkmm2ext::set_source_rgba (cr, led_color);
		cairo_arc (cr, 0, 0, _diameter * .5 - 3 * UIConfigurationBase::instance().get_ui_scale(), 0, 2 * M_PI);
		cairo_fill(cr);

		cairo_restore (cr);
	}

	// a transparent overlay to indicate insensitivity
	if ((visual_state() & Gtkmm2ext::Insensitive)) {
		rounded_function (cr, 1, 1, get_width() - 2, get_height() - 2, corner_radius);
		uint32_t ins_color = UIConfigurationBase::instance().color ("gtk_background");
		Gtkmm2ext::set_source_rgb_a (cr, ins_color, 0.6);
		cairo_fill (cr);
	}

	// if requested, show hovering
	if (UIConfigurationBase::instance().get_widget_prelight()
			&& !((visual_state() & Gtkmm2ext::Insensitive))) {
		if (_hovering) {
			rounded_function (cr, 1, 1, get_width() - 2, get_height() - 2, corner_radius);
			cairo_set_source_rgba (cr, 0.905, 0.917, 0.925, 0.2);
			cairo_fill (cr);
		}
	}

	//user is currently pressing the button. dark outline helps to indicate this
	if (_grabbed && !(_elements & (Inactive|Menu))) {
		rounded_function (cr, 1, 1, get_width() - 2, get_height() - 2, corner_radius);
		cairo_set_line_width(cr, 2);
		cairo_set_source_rgba (cr, 0.1, 0.1, 0.1, .5);
		cairo_stroke (cr);
	}

	//some buttons (like processor boxes) can be selected  (so they can be deleted).  Draw a selection indicator
	if (visual_state() & Gtkmm2ext::Selected) {
		cairo_set_line_width(cr, 1);
		cairo_set_source_rgba (cr, 1, 0, 0, 0.8);
		rounded_function (cr, 0.5, 0.5, get_width() - 1, get_height() - 1, corner_radius);
		cairo_stroke (cr);
	}

	//I guess this means we have keyboard focus.  I don't think this works currently
	//
	//A: yes, it's keyboard focus and it does work when there's no editor window
	//   (the editor is always the first receiver for KeyDown).
	//   It's needed for eg. the engine-dialog at startup or after closing a sesion.
	if (_focused) {
		rounded_function (cr, 1.5, 1.5, get_width() - 3, get_height() - 3, corner_radius);
		cairo_set_source_rgba (cr, 0.905, 0.917, 0.925, 0.8);
		double dashes = 1;
		cairo_set_dash (cr, &dashes, 1, 0);
		cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
		cairo_set_line_width (cr, 1.0);
		cairo_stroke (cr);
		cairo_set_dash (cr, 0, 0, 0);
	}
}

void
ArdourButton::set_corner_radius (float r)
{
	_corner_radius = r;
	CairoWidget::set_dirty ();
}

void
ArdourButton::on_realize()
{
	CairoWidget::on_realize ();
	ensure_layout ();
	if (_layout) {
		if (_layout->get_text() != _text) {
			set_text_internal ();
			queue_resize ();
		}
	}
}

void
ArdourButton::on_size_request (Gtk::Requisition* req)
{
	req->width = req->height = 0;
	CairoWidget::on_size_request (req);

	if (_diameter == 0) {
		const float newdia = rintf (11.f * UIConfigurationBase::instance().get_ui_scale());
		if (_diameter != newdia) {
			_pattern_height = 0;
			_diameter = newdia;
		}
	}

	if (_elements & Text) {

		ensure_layout();
		set_text_internal ();

		/* render() needs the size of the displayed text */
		_layout->get_pixel_size (_text_width, _text_height);

		if (_tweaks & OccasionalText) {

			/* size should not change based on presence or absence
			 * of text.
			 */

		} else if (_layout_ellipsize_width > 0 && _sizing_text.empty()) {

			req->height = std::max(req->height, (int) ceil(char_pixel_height() * BASELINESTRETCH + 1.0));
			req->width += _layout_ellipsize_width / PANGO_SCALE;

		} else /*if (!_text.empty() || !_sizing_text.empty()) */ {

			req->height = std::max(req->height, (int) ceil(char_pixel_height() * BASELINESTRETCH + 1.0));
			req->width += rint(1.75 * char_pixel_width()); // padding

			if (!_sizing_text.empty()) {
				_layout->set_text (_sizing_text); /* use sizing text */
			}

			int sizing_text_width = 0, sizing_text_height = 0;
			_layout->get_pixel_size (sizing_text_width, sizing_text_height);

			req->width += sizing_text_width;

			if (!_sizing_text.empty()) {
				set_text_internal (); /* restore display text */
			}
		}

		/* XXX hack (surprise). Deal with two common rotation angles */

		if (_angle == 90 || _angle == 270) {
			/* do not swap text width or height because we rely on
			   these being the un-rotated values in ::render()
			*/
			swap (req->width, req->height);
		}

	} else {
		_text_width = 0;
		_text_height = 0;
	}

	if (_pixbuf) {
		req->width += _pixbuf->get_width() + char_pixel_width();
		req->height = std::max(req->height, _pixbuf->get_height() + 4);
	}

	if ((_elements & Indicator) || (_tweaks & OccasionalLED)) {
		req->width += ceil (_diameter + char_pixel_width());
		req->height = std::max (req->height, (int) lrint (_diameter) + 4);
	}

	if ((_elements & Menu)) {
		req->width += _diameter + 4;
	}

	if (_elements & (VectorIcon | IconRenderCallback)) {
		const int wh = std::max (8., std::max (ceil (TRACKHEADERBTNW * char_avg_pixel_width()), ceil (char_pixel_height() * BASELINESTRETCH + 1.)));
		req->width += wh;
		req->height = std::max(req->height, wh);
	}

	/* Tweaks to mess the nice stuff above up again. */
	if (_tweaks & TrackHeader) {
		// forget everything above and just use a fixed square [em] size
		// "TrackHeader Buttons" are single letter (usually uppercase)
		// a SizeGroup is much less efficient (lots of gtk work under the hood for each track)
		const int wh = std::max (rint (TRACKHEADERBTNW * char_avg_pixel_width()), ceil (char_pixel_height() * BASELINESTRETCH + 1.));
		req->width  = wh;
		req->height = wh;
	}
	else if (_tweaks & Square) {
		// currerntly unused (again)
		if (req->width < req->height)
			req->width = req->height;
		if (req->height < req->width)
			req->height = req->width;
	} else if (_sizing_text.empty() && _text_width > 0 && !(_elements & Menu)) {
		// properly centered text for those elements that are centered
		// (no sub-pixel offset)
		if ((req->width - _text_width) & 1) { ++req->width; }
		if ((req->height - _text_height) & 1) { ++req->height; }
	}
#if 0
		printf("REQ: %s: %dx%d\n", get_name().c_str(), req->width, req->height);
#endif
}

/**
 * This sets the colors used for rendering based on the name of the button, and
 * thus uses information from the GUI config data.
 */
void
ArdourButton::set_colors ()
{
	_update_colors = false;

	if (_fixed_colors_set == 0x3) {
		return;
	}

	std::string name = get_name();
	bool failed = false;

	if (!(_fixed_colors_set & 0x1)) {
		fill_active_color = UIConfigurationBase::instance().color (string_compose ("%1: fill active", name), &failed);
		if (failed) {
			fill_active_color = UIConfigurationBase::instance().color ("generic button: fill active");
		}
	}

	if (!(_fixed_colors_set & 0x2)) {
		fill_inactive_color = UIConfigurationBase::instance().color (string_compose ("%1: fill", name), &failed);
		if (failed) {
			fill_inactive_color = UIConfigurationBase::instance().color ("generic button: fill");
		}
	}

	text_active_color = Gtkmm2ext::contrasting_text_color (fill_active_color);
	text_inactive_color = Gtkmm2ext::contrasting_text_color (fill_inactive_color);

	led_active_color = UIConfigurationBase::instance().color (string_compose ("%1: led active", name), &failed);
	if (failed) {
		led_active_color = UIConfigurationBase::instance().color ("generic button: led active");
	}

	/* The inactive color for the LED is just a fairly dark version of the
	 * active color.
	 */

	Gtkmm2ext::HSV inactive (led_active_color);
	inactive.v = 0.35;

	led_inactive_color = inactive.color ();
}

/**
 * This sets the colors used for rendering based on two fixed values, rather
 * than basing them on the button name, and thus information in the GUI config
 * data.
 */
void ArdourButton::set_fixed_colors (const uint32_t color_active, const uint32_t color_inactive)
{
	set_active_color (color_active);
	set_inactive_color (color_inactive);
}

void ArdourButton::set_active_color (const uint32_t color)
{
	_fixed_colors_set |= 0x1;

	fill_active_color = color;

	unsigned char r, g, b, a;
	UINT_TO_RGBA(color, &r, &g, &b, &a);

	double white_contrast = (max (double(r), 255.) - min (double(r), 255.)) +
		(max (double(g), 255.) - min (double(g), 255.)) +
		(max (double(b), 255.) - min (double(b), 255.));

	double black_contrast = (max (double(r), 0.) - min (double(r), 0.)) +
		(max (double(g), 0.) - min (double(g), 0.)) +
		(max (double(b), 0.) - min (double(b), 0.));

	text_active_color = (white_contrast > black_contrast) ?
		RGBA_TO_UINT(255, 255, 255, 255) : /* use white */
		RGBA_TO_UINT(  0,   0,   0,   255);  /* use black */

	/* XXX what about led colors ? */
	CairoWidget::set_dirty ();
}

void ArdourButton::set_inactive_color (const uint32_t color)
{
	_fixed_colors_set |= 0x2;

	fill_inactive_color = color;

	unsigned char r, g, b, a;
	UINT_TO_RGBA(color, &r, &g, &b, &a);

	double white_contrast = (max (double(r), 255.) - min (double(r), 255.)) +
		(max (double(g), 255.) - min (double(g), 255.)) +
		(max (double(b), 255.) - min (double(b), 255.));

	double black_contrast = (max (double(r), 0.) - min (double(r), 0.)) +
		(max (double(g), 0.) - min (double(g), 0.)) +
		(max (double(b), 0.) - min (double(b), 0.));

	text_inactive_color = (white_contrast > black_contrast) ?
		RGBA_TO_UINT(255, 255, 255, 255) : /* use white */
		RGBA_TO_UINT(  0,   0,   0,   255);  /* use black */

	/* XXX what about led colors ? */
	CairoWidget::set_dirty ();
}

void ArdourButton::reset_fixed_colors ()
{
	if (_fixed_colors_set == 0) {
		return;
	}
	_fixed_colors_set = 0;
	_update_colors = true;
	CairoWidget::set_dirty ();
}

void
ArdourButton::build_patterns ()
{
	if (convex_pattern) {
		cairo_pattern_destroy (convex_pattern);
		convex_pattern = 0;
	}

	if (concave_pattern) {
		cairo_pattern_destroy (concave_pattern);
		concave_pattern = 0;
	}

	if (led_inset_pattern) {
		cairo_pattern_destroy (led_inset_pattern);
		led_inset_pattern = 0;
	}

	//convex gradient
	convex_pattern = cairo_pattern_create_linear (0.0, 0, 0.0,  get_height());
	cairo_pattern_add_color_stop_rgba (convex_pattern, 0.0, 0,0,0, 0.0);
	cairo_pattern_add_color_stop_rgba (convex_pattern, 1.0, 0,0,0, 0.35);

	//concave gradient
	concave_pattern = cairo_pattern_create_linear (0.0, 0, 0.0,  get_height());
	cairo_pattern_add_color_stop_rgba (concave_pattern, 0.0, 0,0,0, 0.5);
	cairo_pattern_add_color_stop_rgba (concave_pattern, 0.7, 0,0,0, 0.0);

	led_inset_pattern = cairo_pattern_create_linear (0.0, 0.0, 0.0, _diameter);
	cairo_pattern_add_color_stop_rgba (led_inset_pattern, 0, 0,0,0, 0.4);
	cairo_pattern_add_color_stop_rgba (led_inset_pattern, 1, 1,1,1, 0.7);

	_pattern_height = get_height() ;
}

void
ArdourButton::set_led_left (bool yn)
{
	_led_left = yn;
}

bool
ArdourButton::on_button_press_event (GdkEventButton *ev)
{
	focus_handler (this);

	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS) {
		return _fallthrough_to_parent ? false : true;
	}

	if (ev->button == 1 && (_elements & Indicator) && _led_rect && _distinct_led_click) {
		if (ev->x >= _led_rect->x && ev->x < _led_rect->x + _led_rect->width &&
		    ev->y >= _led_rect->y && ev->y < _led_rect->y + _led_rect->height) {
			return true;
		}
	}

	if (binding_proxy.button_press_handler (ev)) {
		return true;
	}

	_grabbed = true;
	CairoWidget::set_dirty ();

	if (ev->button == 1 && !_act_on_release) {
		if (_action) {
			_action->activate ();
			return true;
		} else if (_auto_toggle) {
			set_active (!get_active ());
			signal_clicked ();
			return true;
		}
	}

	return _fallthrough_to_parent ? false : true;
}

bool
ArdourButton::on_button_release_event (GdkEventButton *ev)
{
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS) {
		return _fallthrough_to_parent ? false : true;
	}
	if (ev->button == 1 && _hovering && (_elements & Indicator) && _led_rect && _distinct_led_click) {
		if (ev->x >= _led_rect->x && ev->x < _led_rect->x + _led_rect->width &&
		    ev->y >= _led_rect->y && ev->y < _led_rect->y + _led_rect->height) {
			signal_led_clicked(ev); /* EMIT SIGNAL */
			return true;
		}
	}

	_grabbed = false;
	CairoWidget::set_dirty ();

	if (ev->button == 1 && _hovering) {
		if (_act_on_release && _auto_toggle && !_action) {
			set_active (!get_active ());
		}
		signal_clicked ();
		if (_act_on_release) {
			if (_action) {
				_action->activate ();
				return true;
			}
		}
	}

	return _fallthrough_to_parent ? false : true;
}

void
ArdourButton::set_distinct_led_click (bool yn)
{
	_distinct_led_click = yn;
	setup_led_rect ();
}

void
ArdourButton::color_handler ()
{
	_update_colors = true;
	CairoWidget::set_dirty ();
}

void
ArdourButton::on_size_allocate (Allocation& alloc)
{
	CairoWidget::on_size_allocate (alloc);
	setup_led_rect ();
	if (_layout) {
		/* re-center text */
		//_layout->get_pixel_size (_text_width, _text_height);
	}
}

void
ArdourButton::set_controllable (boost::shared_ptr<Controllable> c)
{
	watch_connection.disconnect ();
	binding_proxy.set_controllable (c);
}

void
ArdourButton::watch ()
{
	boost::shared_ptr<Controllable> c (binding_proxy.get_controllable ());

	if (!c) {
		warning << _("button cannot watch state of non-existing Controllable\n") << endmsg;
		return;
	}
	c->Changed.connect (watch_connection, invalidator(*this), boost::bind (&ArdourButton::controllable_changed, this), gui_context());
}

void
ArdourButton::controllable_changed ()
{
	float val = binding_proxy.get_controllable()->get_value();

	if (fabs (val) >= 0.5f) {
		set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
		unset_active_state ();
	}
	CairoWidget::set_dirty ();
}

void
ArdourButton::set_related_action (RefPtr<Action> act)
{
	Gtkmm2ext::Activatable::set_related_action (act);

	if (_action) {

		action_tooltip_changed ();
		action_sensitivity_changed ();

		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (_action);
		if (tact) {
			action_toggled ();
			tact->signal_toggled().connect (sigc::mem_fun (*this, &ArdourButton::action_toggled));
		}

		_action->connect_property_changed ("sensitive", sigc::mem_fun (*this, &ArdourButton::action_sensitivity_changed));
		_action->connect_property_changed ("visible", sigc::mem_fun (*this, &ArdourButton::action_visibility_changed));
		_action->connect_property_changed ("tooltip", sigc::mem_fun (*this, &ArdourButton::action_tooltip_changed));
	}
}

void
ArdourButton::action_toggled ()
{
	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (_action);

	if (tact) {
		if (tact->get_active()) {
			set_active_state (Gtkmm2ext::ExplicitActive);
		} else {
			unset_active_state ();
		}
	}
}

void
ArdourButton::on_style_changed (const RefPtr<Gtk::Style>& style)
{
	CairoWidget::on_style_changed (style);
	Glib::RefPtr<Gtk::Style> const& new_style = get_style();

	CairoWidget::set_dirty ();
	_update_colors = true;
	_char_pixel_width = 0;
	_char_pixel_height = 0;

	if (!_custom_font_set && _layout && _layout->get_font_description () != new_style->get_font ()) {
		_layout->set_font_description (new_style->get_font ());
		queue_resize ();
	} else if (is_realized()) {
		queue_resize ();
	}
}

void
ArdourButton::on_name_changed ()
{
	_char_pixel_width = 0;
	_char_pixel_height = 0;
	_diameter = 0;
	_update_colors = true;
	if (is_realized()) {
		queue_resize ();
	}
}

void
ArdourButton::setup_led_rect ()
{
	if (!(_elements & Indicator)) {
		delete _led_rect;
		_led_rect = 0;
		return;
	}

	if (!_led_rect) {
		_led_rect = new cairo_rectangle_t;
	}

	if (_elements & Text) {
		if (_led_left) {
			_led_rect->x = char_pixel_width();
		} else {
			_led_rect->x = get_width() - char_pixel_width() + _diameter;
		}
	} else {
		/* centered */
		_led_rect->x = .5 * get_width() - _diameter;
	}

	_led_rect->y = .5 * (get_height() - _diameter);
	_led_rect->width = _diameter;
	_led_rect->height = _diameter;
}

void
ArdourButton::set_image (const RefPtr<Gdk::Pixbuf>& img)
{
	 _elements = (ArdourButton::Element) (_elements & ~ArdourButton::Text);
	_pixbuf = img;
	if (is_realized()) {
		queue_resize ();
	}
}

void
ArdourButton::set_active_state (Gtkmm2ext::ActiveState s)
{
	bool changed = (_active_state != s);
	CairoWidget::set_active_state (s);
	if (changed) {
		_update_colors = true;
		CairoWidget::set_dirty ();
	}
}

void
ArdourButton::set_visual_state (Gtkmm2ext::VisualState s)
{
	bool changed = (_visual_state != s);
	CairoWidget::set_visual_state (s);
	if (changed) {
		_update_colors = true;
		CairoWidget::set_dirty ();
	}
}

bool
ArdourButton::on_focus_in_event (GdkEventFocus* ev)
{
	_focused = true;
	CairoWidget::set_dirty ();
	return CairoWidget::on_focus_in_event (ev);
}

bool
ArdourButton::on_focus_out_event (GdkEventFocus* ev)
{
	_focused = false;
	CairoWidget::set_dirty ();
	return CairoWidget::on_focus_out_event (ev);
}

bool
ArdourButton::on_key_release_event (GdkEventKey *ev) {
	if (_act_on_release && _focused &&
			(ev->keyval == GDK_space || ev->keyval == GDK_Return))
	{
		if (_auto_toggle && !_action) {
				set_active (!get_active ());
		}
		signal_clicked();
		if (_action) {
			_action->activate ();
		}
		return true;
	}
	return CairoWidget::on_key_release_event (ev);
}

bool
ArdourButton::on_key_press_event (GdkEventKey *ev) {
	if (!_act_on_release && _focused &&
			(ev->keyval == GDK_space || ev->keyval == GDK_Return))
	{
		if (_auto_toggle && !_action) {
				set_active (!get_active ());
		}
		signal_clicked();
		if (_action) {
			_action->activate ();
		}
		return true;
	}
	return CairoWidget::on_key_release_event (ev);
}

bool
ArdourButton::on_enter_notify_event (GdkEventCrossing* ev)
{
	_hovering = (_elements & Inactive) ? false : true;

	if (UIConfigurationBase::instance().get_widget_prelight()) {
		CairoWidget::set_dirty ();
	}

	boost::shared_ptr<PBD::Controllable> c (binding_proxy.get_controllable ());
	if (c) {
		PBD::Controllable::GUIFocusChanged (boost::weak_ptr<PBD::Controllable> (c));
	}

	return CairoWidget::on_enter_notify_event (ev);
}

bool
ArdourButton::on_leave_notify_event (GdkEventCrossing* ev)
{
	_hovering = false;

	if (UIConfigurationBase::instance().get_widget_prelight()) {
		CairoWidget::set_dirty ();
	}

	if (binding_proxy.get_controllable()) {
		PBD::Controllable::GUIFocusChanged (boost::weak_ptr<PBD::Controllable> ());
	}

	return CairoWidget::on_leave_notify_event (ev);
}

bool
ArdourButton::on_grab_broken_event(GdkEventGrabBroken* grab_broken_event) {
	/* Our implicit grab due to a button_press was broken by another grab:
	 * the button will not get any button_release event if the mouse leaves
	 * while the grab is taken, so unpress ourselves */
	_grabbed = false;
	CairoWidget::set_dirty ();
	return true;
}

void
ArdourButton::set_tweaks (Tweaks t)
{
	if (_tweaks != t) {
		_tweaks = t;
		if (is_realized()) {
			queue_resize ();
		}
	}
}

void
ArdourButton::action_sensitivity_changed ()
{
	if (_action->property_sensitive ()) {
		set_visual_state (Gtkmm2ext::VisualState (visual_state() & ~Gtkmm2ext::Insensitive));
	} else {
		set_visual_state (Gtkmm2ext::VisualState (visual_state() | Gtkmm2ext::Insensitive));
	}
}

void
ArdourButton::set_layout_ellipsize_width (int w)
{
	if (_layout_ellipsize_width == w) {
		return;
	}
	_layout_ellipsize_width = w;
	if (!_layout) {
		return;
	}
	if (_layout_ellipsize_width > 3 * PANGO_SCALE) {
		_layout->set_width (_layout_ellipsize_width - 3 * PANGO_SCALE);
	}
	if (is_realized ()) {
		queue_resize ();
	}
}

void
ArdourButton::set_text_ellipsize (Pango::EllipsizeMode e)
{
	if (_ellipsis == e) {
		return;
	}
	_ellipsis = e;
	if (!_layout) {
		return;
	}
	_layout->set_ellipsize(_ellipsis);
	if (_layout_ellipsize_width > 3 * PANGO_SCALE) {
		_layout->set_width (_layout_ellipsize_width - 3 * PANGO_SCALE);
	}
	if (is_realized ()) {
		queue_resize ();
	}
}

void
ArdourButton::ensure_layout ()
{
	if (!_layout) {
		ensure_style ();
		_layout = Pango::Layout::create (get_pango_context());
		_layout->set_font_description (get_style()->get_font());
		_layout->set_ellipsize(_ellipsis);
		if (_layout_ellipsize_width > 3 * PANGO_SCALE) {
			_layout->set_width (_layout_ellipsize_width - 3* PANGO_SCALE);
		}
	}
}

void
ArdourButton::recalc_char_pixel_geometry ()
{
	if (_char_pixel_height > 0 && _char_pixel_width > 0) {
		return;
	}
	ensure_layout();
	// NB. this is not static, since the geometry is different
	// depending on the font used.
	int w, h;
	std::string x = _("@ABCDEFGHIJLKMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
	_layout->set_text (x);
	_layout->get_pixel_size (w, h);
	_char_pixel_height = std::max(4, h);
	// number of actual chars in the string (not bytes)
	// Glib to the rescue.
	Glib::ustring gx(x);
	_char_avg_pixel_width = w / (float)gx.size();
	_char_pixel_width = std::max(4, (int) ceil (_char_avg_pixel_width));
	set_text_internal (); /* restore display text */
}

void
ArdourButton::action_visibility_changed ()
{
	if (_action->property_visible ()) {
		show ();
	} else {
		hide ();
	}
}

void
ArdourButton::action_tooltip_changed ()
{
	string str = _action->property_tooltip().get_value();
	set_tooltip (*this, str);
}

void
ArdourButton::set_elements (Element e)
{
	_elements = e;
	CairoWidget::set_dirty ();
}

void
ArdourButton::add_elements (Element e)
{
	_elements = (ArdourButton::Element) (_elements | e);
	CairoWidget::set_dirty ();
}

void
ArdourButton::set_icon (ArdourIcon::Icon i)
{
	_icon = i;
	_icon_render_cb = 0;
	_icon_render_cb_data = 0;
	_elements = (ArdourButton::Element) ((_elements | VectorIcon) & ~(ArdourButton::Text | IconRenderCallback));
	CairoWidget::set_dirty ();
}

void
ArdourButton::set_icon (rendercallback_t cb, void* d)
{
	if (!cb) {
		_elements = (ArdourButton::Element) ((_elements | ArdourButton::Text) & ~(IconRenderCallback | VectorIcon));
		_icon_render_cb = 0;
		_icon_render_cb_data = 0;
	} else {
		_elements = (ArdourButton::Element) ((_elements | IconRenderCallback) & ~(ArdourButton::Text | VectorIcon));
		_icon_render_cb = cb;
		_icon_render_cb_data = d;
	}
	CairoWidget::set_dirty ();
}

void
ArdourButton::set_custom_led_color (uint32_t c, bool useit)
{
	if (led_custom_color == c && use_custom_led_color == useit) {
		return;
	}

	led_custom_color = c;
	use_custom_led_color = useit;
	CairoWidget::set_dirty ();
}
