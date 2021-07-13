/*
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2010-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
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
#include <iomanip>
#include <cstring>
#include <cmath>

#include <gtkmm/window.h>
#include <pangomm/layout.h>

#include "pbd/compose.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/persistent_tooltip.h"

#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"

#include "gtkmm2ext/colors.h"

#include "stereo_panner.h"
#include "stereo_panner_editor.h"
#include "rgb_macros.h"
#include "utils.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR_UI_UTILS;

using PBD::Controllable;

StereoPanner::ColorScheme StereoPanner::colors[3];

uint32_t StereoPanner::colors_send_bg;
uint32_t StereoPanner::colors_send_pan;
bool     StereoPanner::have_colors = false;

Pango::AttrList StereoPanner::panner_font_attributes;
bool            StereoPanner::have_font = false;

using namespace ARDOUR;

StereoPanner::StereoPanner (boost::shared_ptr<PannerShell> p)
	: PannerInterface (p->panner())
	, _panner_shell (p)
	, position_control (_panner->pannable()->pan_azimuth_control)
	, width_control (_panner->pannable()->pan_width_control)
	, dragging_position (false)
	, dragging_left (false)
	, dragging_right (false)
	, drag_start_x (0)
	, last_drag_x (0)
	, accumulated_delta (0)
	, detented (false)
	, position_binder (position_control)
	, width_binder (width_control)
	, _dragging (false)
{
	if (!have_colors) {
		set_colors ();
		have_colors = true;
	}
	if (!have_font) {
		Pango::FontDescription font;
		Pango::AttrFontDesc* font_attr;
		font = Pango::FontDescription (UIConfiguration::instance().get_SmallBoldMonospaceFont());
		font_attr = new Pango::AttrFontDesc (Pango::Attribute::create_attr_font_desc (font));
		panner_font_attributes.change(*font_attr);
		delete font_attr;
		have_font = true;
	}

	position_control->Changed.connect (panvalue_connections, invalidator(*this), boost::bind (&StereoPanner::value_change, this), gui_context());
	width_control->Changed.connect (panvalue_connections, invalidator(*this), boost::bind (&StereoPanner::value_change, this), gui_context());

	_panner_shell->Changed.connect (panshell_connections, invalidator (*this), boost::bind (&StereoPanner::bypass_handler, this), gui_context());
	_panner_shell->PannableChanged.connect (panshell_connections, invalidator (*this), boost::bind (&StereoPanner::pannable_handler, this), gui_context());

	UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &StereoPanner::color_handler));

	set_tooltip ();
}

StereoPanner::~StereoPanner ()
{

}

void
StereoPanner::set_tooltip ()
{
	if (_panner_shell->bypassed()) {
		_tooltip.set_tip (_("bypassed"));
		return;
	}
	double pos = position_control->get_value(); // 0..1

	/* We show the position of the center of the image relative to the left & right.
	   This is expressed as a pair of percentage values that ranges from (100,0)
	   (hard left) through (50,50) (hard center) to (0,100) (hard right).

	   This is pretty weird, but its the way audio engineers expect it. Just remember that
	   the center of the USA isn't Kansas, its (50LA, 50NY) and it will all make sense.
	*/

	char buf[64];
	snprintf (buf, sizeof (buf), _("L:%3d R:%3d Width:%d%%"), (int) rint (100.0 * (1.0 - pos)),
	          (int) rint (100.0 * pos),
	          (int) floor (100.0 * width_control->get_value()));
	_tooltip.set_tip (buf);
}

bool
StereoPanner::on_expose_event (GdkEventExpose*)
{
	Glib::RefPtr<Gdk::Window> win (get_window());
	Glib::RefPtr<Gdk::GC> gc (get_style()->get_base_gc (get_state()));
	Cairo::RefPtr<Cairo::Context> context = get_window()->create_cairo_context();
	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create(get_pango_context());
	layout->set_attributes (panner_font_attributes);

	int tw, th;
	int width, height;
	const double pos = position_control->get_value (); /* 0..1 */
	const double swidth = width_control->get_value (); /* -1..+1 */
	const double fswidth = fabs (swidth);
	uint32_t o, f, t, b, r;
	State state;

	width = get_width();
	height = get_height ();

	const int step_down = rint(height / 3.5);
	const double corner_radius = 5.0 * UIConfiguration::instance().get_ui_scale();
	const int lr_box_size = height - 2 * step_down;
	const int pos_box_size = (int)(rint(step_down * .8)) | 1;
	const int top_step = step_down - pos_box_size;

	if (swidth == 0.0) {
		state = Mono;
	} else if (swidth < 0.0) {
		state = Inverted;
	} else {
		state = Normal;
	}

	o = colors[state].outline;
	f = colors[state].fill;
	t = colors[state].text;
	b = colors[state].background;
	r = colors[state].rule;

	if (_send_mode) {
		b = colors_send_bg;
	}

	if (_panner_shell->bypassed()) {
		b  = 0x20202040;
		f  = 0x404040ff;
		o  = 0x606060ff;
		t  = 0x606060ff;
		r  = 0x606060ff;
	}

	/* background */

	context->set_source_rgba (UINT_RGBA_R_FLT(b), UINT_RGBA_G_FLT(b), UINT_RGBA_B_FLT(b), UINT_RGBA_A_FLT(b));
	cairo_rectangle (context->cobj(), 0, 0, width, height);
	context->fill_preserve ();
	context->clip();

	/* the usable width is reduced from the real width, because we need space for
	   the two halves of LR boxes that will extend past the actual left/right
	   positions (indicated by the vertical line segment above them).
	*/

	double usable_width = width - lr_box_size;

	/* compute the centers of the L/R boxes based on the current stereo width */

	if (fmod (usable_width,2.0) == 0) {
		/* even width, but we need odd, so that there is an exact center.
		   So, offset cairo by 1, and reduce effective width by 1
		*/
		usable_width -= 1.0;
		context->translate (1.0, 0.0);
	}

	const double half_lr_box = lr_box_size/2.0;
	const double center = rint(half_lr_box + (usable_width * pos));
	const double pan_spread = rint((fswidth * (usable_width-1.0))/2.0);
	const double left  = center - pan_spread;
	const double right = center + pan_spread;

	/* center line */
	context->set_line_width (1.0);
	context->move_to ((usable_width + lr_box_size)/2.0, 0);
	context->rel_line_to (0, height);
	context->set_source_rgba (UINT_RGBA_R_FLT(r), UINT_RGBA_G_FLT(r), UINT_RGBA_B_FLT(r), UINT_RGBA_A_FLT(r));
	context->stroke ();

	/* compute & draw the line through the box */
	context->set_line_width (2);
	context->set_source_rgba (UINT_RGBA_R_FLT(o), UINT_RGBA_G_FLT(o), UINT_RGBA_B_FLT(o), UINT_RGBA_A_FLT(o));
	context->move_to (left,  top_step + (pos_box_size/2.0) + step_down + 1.0);
	context->line_to (left,  top_step + (pos_box_size/2.0));
	context->line_to (right, top_step + (pos_box_size/2.0));
	context->line_to (right, top_step + (pos_box_size/2.0) + step_down + 1.0);
	context->stroke ();

	context->set_line_width (1.0);

	/* left box */
	if (state != Mono) {
		rounded_rectangle (context, left - half_lr_box,
				half_lr_box+step_down,
				lr_box_size, lr_box_size, corner_radius);
		context->set_source_rgba (UINT_RGBA_R_FLT(f), UINT_RGBA_G_FLT(f), UINT_RGBA_B_FLT(f), UINT_RGBA_A_FLT(f));
		context->fill_preserve();
		context->set_source_rgba (UINT_RGBA_R_FLT(o), UINT_RGBA_G_FLT(o), UINT_RGBA_B_FLT(o), UINT_RGBA_A_FLT(o));
		context->stroke();

		/* add text */
		context->set_source_rgba (UINT_RGBA_R_FLT(t), UINT_RGBA_G_FLT(t), UINT_RGBA_B_FLT(t), UINT_RGBA_A_FLT(t));
		if (swidth < 0.0) {
			layout->set_text (S_("Panner|R"));
		} else {
			layout->set_text (S_("Panner|L"));
		}
		layout->get_pixel_size(tw, th);
		context->move_to (rint(left - tw/2), rint(lr_box_size + step_down - th/2));
		pango_cairo_show_layout (context->cobj(), layout->gobj());
	}

	/* right box */
	rounded_rectangle (context, right - half_lr_box,
			half_lr_box+step_down,
			lr_box_size, lr_box_size, corner_radius);
	context->set_source_rgba (UINT_RGBA_R_FLT(f), UINT_RGBA_G_FLT(f), UINT_RGBA_B_FLT(f), UINT_RGBA_A_FLT(f));
	context->fill_preserve();
	context->set_source_rgba (UINT_RGBA_R_FLT(o), UINT_RGBA_G_FLT(o), UINT_RGBA_B_FLT(o), UINT_RGBA_A_FLT(o));
	context->stroke();

	/* add text */
	context->set_source_rgba (UINT_RGBA_R_FLT(t), UINT_RGBA_G_FLT(t), UINT_RGBA_B_FLT(t), UINT_RGBA_A_FLT(t));

	if (state == Mono) {
		layout->set_text (S_("Panner|M"));
	} else {
		if (swidth < 0.0) {
			layout->set_text (S_("Panner|L"));
		} else {
			layout->set_text (S_("Panner|R"));
		}
	}
	layout->get_pixel_size(tw, th);
	context->move_to (rint(right - tw/2), rint(lr_box_size + step_down - th/2));
	pango_cairo_show_layout (context->cobj(), layout->gobj());

	/* draw the central box */
	context->set_line_width (2.0);
	context->move_to (center + (pos_box_size/2.0), top_step); /* top right */
	context->rel_line_to (0.0, pos_box_size); /* lower right */
	context->rel_line_to (-pos_box_size/2.0, 4.0); /* bottom point */
	context->rel_line_to (-pos_box_size/2.0, -4.0); /* lower left */
	context->rel_line_to (0.0, -pos_box_size); /* upper left */
	context->close_path ();

	if (_send_mode && !_panner_shell->is_linked_to_route()) {
		f = colors_send_pan;
	}

	context->set_source_rgba (UINT_RGBA_R_FLT(o), UINT_RGBA_G_FLT(o), UINT_RGBA_B_FLT(o), UINT_RGBA_A_FLT(o));
	context->stroke_preserve ();
	context->set_source_rgba (UINT_RGBA_R_FLT(f), UINT_RGBA_G_FLT(f), UINT_RGBA_B_FLT(f), UINT_RGBA_A_FLT(f));
	context->fill ();

	return true;
}

bool
StereoPanner::on_button_press_event (GdkEventButton* ev)
{
	if (PannerInterface::on_button_press_event (ev)) {
		return true;
	}

	if (_panner_shell->bypassed()) {
		return true;
	}

	drag_start_x = ev->x;
	last_drag_x = ev->x;

	dragging_position = false;
	dragging_left = false;
	dragging_right = false;
	_dragging = false;
	_tooltip.target_stop_drag ();
	accumulated_delta = 0;
	detented = false;

	/* Let the binding proxies get first crack at the press event
	 */

	if (ev->y < 20) {
		if (position_binder.button_press_handler (ev)) {
			return true;
		}
	} else {
		if (width_binder.button_press_handler (ev)) {
			return true;
		}
	}

	if (ev->button != 1) {
		return false;
	}

	if (ev->type == GDK_2BUTTON_PRESS) {
		int width = get_width();

		if (Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier)) {
			/* handled by button release */
			return true;
		}

		if (ev->y < 20) {

			/* upper section: adjusts position, constrained by width */

			const double w = fabs (width_control->get_value ());
			const double max_pos = 1.0 - (w/2.0);
			const double min_pos = w/2.0;

			if (ev->x <= width/3) {
				/* left side dbl click */
				if (Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier)) {
					/* 2ndary-double click on left, collapse to hard left */
					width_control->set_value (0, Controllable::NoGroup);
					position_control->set_value (0, Controllable::NoGroup);
				} else {
					position_control->set_value (min_pos, Controllable::NoGroup);
				}
			} else if (ev->x > 2*width/3) {
				if (Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier)) {
					/* 2ndary-double click on right, collapse to hard right */
					width_control->set_value (0, Controllable::NoGroup);
					position_control->set_value (1.0, Controllable::NoGroup);
				} else {
					position_control->set_value (max_pos, Controllable::NoGroup);
				}
			} else {
				position_control->set_value (0.5, Controllable::NoGroup);
			}

		} else {

			/* lower section: adjusts width, constrained by position */

			const double p = position_control->get_value ();
			const double max_width = 2.0 * min ((1.0 - p), p);

			if (ev->x <= width/3) {
				/* left side dbl click */
				width_control->set_value (max_width, Controllable::NoGroup); // reset width to 100%
			} else if (ev->x > 2*width/3) {
				/* right side dbl click */
				width_control->set_value (-max_width, Controllable::NoGroup); // reset width to inverted 100%
			} else {
				/* center dbl click */
				width_control->set_value (0, Controllable::NoGroup); // collapse width to 0%
			}
		}

		_dragging = false;
		_tooltip.target_stop_drag ();

	} else if (ev->type == GDK_BUTTON_PRESS) {

		if (Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier)) {
			/* handled by button release */
			return true;
		}

		if (ev->y < 20) {
			/* top section of widget is for position drags */
			dragging_position = true;
			StartPositionGesture ();
		} else {
			/* lower section is for dragging width */

			double pos = position_control->get_value (); /* 0..1 */
			double swidth = width_control->get_value (); /* -1..+1 */
			double fswidth = fabs (swidth);
			const int lr_box_size = get_height() - 2 * rint(get_height() / 3.5);
			int usable_width = get_width() - lr_box_size;
			double center = (lr_box_size/2.0) + (usable_width * pos);
			int left = lrint (center - (fswidth * usable_width / 2.0)); // center of leftmost box
			int right = lrint (center +  (fswidth * usable_width / 2.0)); // center of rightmost box
			const int half_box = lr_box_size/2;

			if (ev->x >= (left - half_box) && ev->x < (left + half_box)) {
				if (swidth < 0.0) {
					dragging_right = true;
				} else {
					dragging_left = true;
				}
			} else if (ev->x >= (right - half_box) && ev->x < (right + half_box)) {
				if (swidth < 0.0) {
					dragging_left = true;
				} else {
					dragging_right = true;
				}
			}
			StartWidthGesture ();
		}

		_dragging = true;
		_tooltip.target_start_drag ();
	}

	return true;
}

bool
StereoPanner::on_button_release_event (GdkEventButton* ev)
{
	if (PannerInterface::on_button_release_event (ev)) {
		return true;
	}

	if (ev->button != 1) {
		return false;
	}

	if (_panner_shell->bypassed()) {
		return false;
	}

	bool const dp = dragging_position;

	_dragging = false;
	_tooltip.target_stop_drag ();
	dragging_position = false;
	dragging_left = false;
	dragging_right = false;
	accumulated_delta = 0;
	detented = false;

	if (Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier)) {
		_panner->reset ();
	} else {
		if (dp) {
			StopPositionGesture ();
		} else {
			StopWidthGesture ();
		}
	}

	return true;
}

bool
StereoPanner::on_scroll_event (GdkEventScroll* ev)
{
	double one_degree = 1.0/180.0; // one degree as a number from 0..1, since 180 degrees is the full L/R axis
	double pv = position_control->get_value(); // 0..1.0 ; 0 = left
	double wv = width_control->get_value(); // 0..1.0 ; 0 = left
	double step;

	if (_panner_shell->bypassed()) {
		return false;
	}

	if (Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier)) {
		step = one_degree;
	} else {
		step = one_degree * 5.0;
	}

	switch (ev->direction) {
	case GDK_SCROLL_LEFT:
		wv += step;
		width_control->set_value (wv, Controllable::NoGroup);
		break;
	case GDK_SCROLL_UP:
		pv -= step;
		position_control->set_value (pv, Controllable::NoGroup);
		break;
	case GDK_SCROLL_RIGHT:
		wv -= step;
		width_control->set_value (wv, Controllable::NoGroup);
		break;
	case GDK_SCROLL_DOWN:
		pv += step;
		position_control->set_value (pv, Controllable::NoGroup);
		break;
	}

	return true;
}

bool
StereoPanner::on_motion_notify_event (GdkEventMotion* ev)
{
	if (_panner_shell->bypassed()) {
		_dragging = false;
	}
	if (!_dragging) {
		return false;
	}

	const int lr_box_size = get_height() - 2 * rint(get_height() / 3.5);
	int usable_width = get_width() - lr_box_size;
	double delta = (ev->x - last_drag_x) / (double) usable_width;
	double current_width = width_control->get_value ();

	if (dragging_left) {
		delta = -delta;
	}

	if (dragging_left || dragging_right) {

		if (Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier)) {

			/* change width and position in a way that keeps the
			 * other side in the same place
			 */

			_panner->freeze ();

			double pv = position_control->get_value();

			if (dragging_left) {
				position_control->set_value (pv - delta, Controllable::NoGroup);
			} else {
				position_control->set_value (pv + delta, Controllable::NoGroup);
			}

			if (delta > 0.0) {
				/* delta is positive, so we're about to
				   increase the width. But we need to increase it
				   by twice the required value so that the
				   other side remains in place when we set
				   the position as well.
				*/
				width_control->set_value (current_width + (delta * 2.0), Controllable::NoGroup);
			} else {
				width_control->set_value (current_width + delta, Controllable::NoGroup);
			}

			_panner->thaw ();

		} else {

			/* maintain position as invariant as we change the width */

			/* create a detent close to the center */

			if (!detented && fabs (current_width) < 0.02) {
				detented = true;
				/* snap to zero */
				width_control->set_value (0, Controllable::NoGroup);
			}

			if (detented) {

				accumulated_delta += delta;

				/* have we pulled far enough to escape ? */

				if (fabs (accumulated_delta) >= 0.025) {
					width_control->set_value (current_width + accumulated_delta, Controllable::NoGroup);
					detented = false;
					accumulated_delta = false;
				}

			} else {
				/* width needs to change by 2 * delta because both L & R move */
				width_control->set_value (current_width + (delta * 2.0), Controllable::NoGroup);
			}
		}

	} else if (dragging_position) {

		double pv = position_control->get_value(); // 0..1.0 ; 0 = left
		position_control->set_value (pv + delta, Controllable::NoGroup);
	}

	last_drag_x = ev->x;
	return true;
}

bool
StereoPanner::on_key_press_event (GdkEventKey* ev)
{
	double one_degree = 1.0/180.0;
	double pv = position_control->get_value(); // 0..1.0 ; 0 = left
	double wv = width_control->get_value(); // 0..1.0 ; 0 = left
	double step;

	if (_panner_shell->bypassed()) {
		return false;
	}

	if (Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier)) {
		step = one_degree;
	} else {
		step = one_degree * 5.0;
	}

	/* up/down control width because we consider pan position more "important"
	   (and thus having higher "sense" priority) than width.
	*/

	switch (ev->keyval) {
	case GDK_Up:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {
			width_control->set_value (1.0, Controllable::NoGroup);
		} else {
			width_control->set_value (wv + step, Controllable::NoGroup);
		}
		break;
	case GDK_Down:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {
			width_control->set_value (-1.0, Controllable::NoGroup);
		} else {
			width_control->set_value (wv - step, Controllable::NoGroup);
		}
		break;

	case GDK_Left:
		pv -= step;
		position_control->set_value (pv, Controllable::NoGroup);
		break;
	case GDK_Right:
		pv += step;
		position_control->set_value (pv, Controllable::NoGroup);
		break;
	case GDK_0:
	case GDK_KP_0:
		width_control->set_value (0.0, Controllable::NoGroup);
		break;

	default:
		return false;
	}

	return true;
}

void
StereoPanner::set_colors ()
{
	colors[Normal].fill = UIConfiguration::instance().color ("stereo panner fill");
	colors[Normal].outline = UIConfiguration::instance().color ("stereo panner outline");
	colors[Normal].text = UIConfiguration::instance().color ("stereo panner text");
	colors[Normal].background = UIConfiguration::instance().color ("stereo panner bg");
	colors[Normal].rule = UIConfiguration::instance().color ("stereo panner rule");

	colors[Mono].fill = UIConfiguration::instance().color ("stereo panner mono fill");
	colors[Mono].outline = UIConfiguration::instance().color ("stereo panner mono outline");
	colors[Mono].text = UIConfiguration::instance().color ("stereo panner mono text");
	colors[Mono].background = UIConfiguration::instance().color ("stereo panner mono bg");
	colors[Mono].rule = UIConfiguration::instance().color ("stereo panner rule");

	colors[Inverted].fill = UIConfiguration::instance().color ("stereo panner inverted fill");
	colors[Inverted].outline = UIConfiguration::instance().color ("stereo panner inverted outline");
	colors[Inverted].text = UIConfiguration::instance().color ("stereo panner inverted text");
	colors[Inverted].background = UIConfiguration::instance().color ("stereo panner inverted bg");
	colors[Inverted].rule = UIConfiguration::instance().color ("stereo panner rule");

	colors_send_bg  = UIConfiguration::instance().color ("send bg");
	colors_send_pan =  UIConfiguration::instance().color ("send pan");
}

void
StereoPanner::color_handler ()
{
	set_colors ();
	queue_draw ();
}

void
StereoPanner::bypass_handler ()
{
	queue_draw ();
}

void
StereoPanner::pannable_handler ()
{
	panvalue_connections.drop_connections();
	position_control = _panner->pannable()->pan_azimuth_control;
	width_control = _panner->pannable()->pan_width_control;
	position_binder.set_controllable(position_control);
	width_binder.set_controllable(width_control);

	position_control->Changed.connect (panvalue_connections, invalidator(*this), boost::bind (&StereoPanner::value_change, this), gui_context());
	width_control->Changed.connect (panvalue_connections, invalidator(*this), boost::bind (&StereoPanner::value_change, this), gui_context());
	queue_draw ();
}

PannerEditor*
StereoPanner::editor ()
{
	return new StereoPannerEditor (this);
}
