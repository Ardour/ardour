/*
 * Copyright (C) 2011-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2011-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2011-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#include "mono_panner.h"
#include "mono_panner_editor.h"
#include "rgb_macros.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR_UI_UTILS;

using PBD::Controllable;

MonoPanner::ColorScheme MonoPanner::colors;
bool MonoPanner::have_colors = false;

Pango::AttrList MonoPanner::panner_font_attributes;
bool            MonoPanner::have_font = false;

MonoPanner::MonoPanner (boost::shared_ptr<ARDOUR::PannerShell> p)
	: PannerInterface (p->panner())
	, _panner_shell (p)
	, position_control (_panner->pannable()->pan_azimuth_control)
	, drag_start_x (0)
	, last_drag_x (0)
	, accumulated_delta (0)
	, detented (false)
	, position_binder (position_control)
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

	position_control->Changed.connect (panvalue_connections, invalidator(*this), boost::bind (&MonoPanner::value_change, this), gui_context());

	_panner_shell->Changed.connect (panshell_connections, invalidator (*this), boost::bind (&MonoPanner::bypass_handler, this), gui_context());
	_panner_shell->PannableChanged.connect (panshell_connections, invalidator (*this), boost::bind (&MonoPanner::pannable_handler, this), gui_context());
	UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &MonoPanner::color_handler));

	set_tooltip ();
}

MonoPanner::~MonoPanner ()
{

}

void
MonoPanner::set_tooltip ()
{
	if (_panner_shell->bypassed()) {
		_tooltip.set_tip (_("bypassed"));
		return;
	}
	double pos = position_control->get_value(); // 0..1

	/* We show the position of the center of the image relative to the left & right.
		 This is expressed as a pair of percentage values that ranges from (100,0)
		 (hard left) through (50,50) (hard center) to (0,100) (hard right).

		 This is pretty wierd, but its the way audio engineers expect it. Just remember that
		 the center of the USA isn't Kansas, its (50LA, 50NY) and it will all make sense.
		 */

	char buf[64];
	snprintf (buf, sizeof (buf), _("L:%3d R:%3d"),
			(int) rint (100.0 * (1.0 - pos)),
			(int) rint (100.0 * pos));
	_tooltip.set_tip (buf);
}

bool
MonoPanner::on_expose_event (GdkEventExpose*)
{
	Glib::RefPtr<Gdk::Window> win (get_window());
	Glib::RefPtr<Gdk::GC> gc (get_style()->get_base_gc (get_state()));
	Cairo::RefPtr<Cairo::Context> context = get_window()->create_cairo_context();

	int width, height;
	double pos = position_control->get_value (); /* 0..1 */
	uint32_t o, f, t, b, pf, po;

	width = get_width();
	height = get_height ();

	const int step_down = rint(height / 3.5);
	const int lr_box_size = height - 2 * step_down;
	const int pos_box_size = (int)(rint(step_down * .8)) | 1;
	const int top_step = step_down - pos_box_size;
	const double corner_radius = 5 * UIConfiguration::instance().get_ui_scale();

	o = colors.outline;
	f = colors.fill;
	t = colors.text;
	b = colors.background;
	pf = colors.pos_fill;
	po = colors.pos_outline;

	if (_panner_shell->bypassed()) {
		b  = 0x20202040;
		f  = 0x404040ff;
		o  = 0x606060ff;
		po = 0x606060ff;
		pf = 0x404040ff;
		t  = 0x606060ff;
	}

	if (_send_mode) {
		b = UIConfiguration::instance().color ("send bg");
	}
	/* background */
	context->set_source_rgba (UINT_RGBA_R_FLT(b), UINT_RGBA_G_FLT(b), UINT_RGBA_B_FLT(b), UINT_RGBA_A_FLT(b));
	context->rectangle (0, 0, width, height);
	context->fill ();

	double usable_width = width - pos_box_size;

	/* compute the centers of the L/R boxes based on the current stereo width */
	if (fmod (usable_width,2.0) == 0) {
		usable_width -= 1.0;
	}
	const double half_lr_box = lr_box_size/2.0;
	const double left = pos_box_size * .5 + half_lr_box; // center of left box
	const double right = width - pos_box_size * .5 - half_lr_box; // center of right box

	/* center line */
	context->set_source_rgba (UINT_RGBA_R_FLT(o), UINT_RGBA_G_FLT(o), UINT_RGBA_B_FLT(o), UINT_RGBA_A_FLT(o));
	context->set_line_width (1.0);
	context->move_to ((pos_box_size/2.0) + (usable_width/2.0), 0);
	context->line_to ((pos_box_size/2.0) + (usable_width/2.0), height);
	context->stroke ();

	context->set_line_width (1.0);
	/* left box */

	rounded_left_half_rectangle (context,
			left - half_lr_box + .5,
			half_lr_box + step_down,
			lr_box_size, lr_box_size, corner_radius);
	context->set_source_rgba (UINT_RGBA_R_FLT(f), UINT_RGBA_G_FLT(f), UINT_RGBA_B_FLT(f), UINT_RGBA_A_FLT(f));
	context->fill_preserve ();
	context->set_source_rgba (UINT_RGBA_R_FLT(o), UINT_RGBA_G_FLT(o), UINT_RGBA_B_FLT(o), UINT_RGBA_A_FLT(o));
	context->stroke();

	/* add text */
	int tw, th;
	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create(get_pango_context());
	layout->set_attributes (panner_font_attributes);

	layout->set_text (S_("Panner|L"));
	layout->get_pixel_size(tw, th);
	context->move_to (rint(left - tw/2), rint(lr_box_size + step_down - th/2));
	context->set_source_rgba (UINT_RGBA_R_FLT(t), UINT_RGBA_G_FLT(t), UINT_RGBA_B_FLT(t), UINT_RGBA_A_FLT(t));
	pango_cairo_show_layout (context->cobj(), layout->gobj());

	/* right box */
	rounded_right_half_rectangle (context,
			right - half_lr_box - .5,
			half_lr_box + step_down,
			lr_box_size, lr_box_size, corner_radius);
	context->set_source_rgba (UINT_RGBA_R_FLT(f), UINT_RGBA_G_FLT(f), UINT_RGBA_B_FLT(f), UINT_RGBA_A_FLT(f));
	context->fill_preserve ();
	context->set_source_rgba (UINT_RGBA_R_FLT(o), UINT_RGBA_G_FLT(o), UINT_RGBA_B_FLT(o), UINT_RGBA_A_FLT(o));
	context->stroke();

	/* add text */
	layout->set_text (S_("Panner|R"));
	layout->get_pixel_size(tw, th);
	context->move_to (rint(right - tw/2), rint(lr_box_size + step_down - th/2));
	context->set_source_rgba (UINT_RGBA_R_FLT(t), UINT_RGBA_G_FLT(t), UINT_RGBA_B_FLT(t), UINT_RGBA_A_FLT(t));
	pango_cairo_show_layout (context->cobj(), layout->gobj());

	/* 2 lines that connect them both */
	context->set_line_width (1.0);

	if (_panner_shell->panner_gui_uri() != "http://ardour.org/plugin/panner_balance#ui") {
		context->set_source_rgba (UINT_RGBA_R_FLT(o), UINT_RGBA_G_FLT(o), UINT_RGBA_B_FLT(o), UINT_RGBA_A_FLT(o));
		context->move_to (left  + half_lr_box, half_lr_box + step_down);
		context->line_to (right - half_lr_box, half_lr_box + step_down);
		context->stroke ();

		context->move_to (left  + half_lr_box, half_lr_box+step_down+lr_box_size);
		context->line_to (right - half_lr_box, half_lr_box+step_down+lr_box_size);
		context->stroke ();
	} else {
		context->move_to (left  + half_lr_box, half_lr_box+step_down+lr_box_size);
		context->line_to (left  + half_lr_box, half_lr_box + step_down);
		context->line_to ((pos_box_size/2.0) + (usable_width/2.0), half_lr_box+step_down+lr_box_size);
		context->line_to (right - half_lr_box, half_lr_box + step_down);
		context->line_to (right - half_lr_box, half_lr_box+step_down+lr_box_size);
		context->close_path();

		context->set_source_rgba (UINT_RGBA_R_FLT(f), UINT_RGBA_G_FLT(f), UINT_RGBA_B_FLT(f), UINT_RGBA_A_FLT(f));
		context->fill_preserve ();
		context->set_source_rgba (UINT_RGBA_R_FLT(o), UINT_RGBA_G_FLT(o), UINT_RGBA_B_FLT(o), UINT_RGBA_A_FLT(o));
		context->stroke ();
	}

	/* draw the position indicator */
	double spos = (pos_box_size/2.0) + (usable_width * pos);

	context->set_line_width (2.0);
	context->move_to (spos + (pos_box_size/2.0), top_step); /* top right */
	context->rel_line_to (0.0, pos_box_size); /* lower right */
	context->rel_line_to (-pos_box_size/2.0, 4.0 * UIConfiguration::instance().get_ui_scale()); /* bottom point */
	context->rel_line_to (-pos_box_size/2.0, -4.0 * UIConfiguration::instance().get_ui_scale()); /* lower left */
	context->rel_line_to (0.0, -pos_box_size); /* upper left */
	context->close_path ();


	context->set_source_rgba (UINT_RGBA_R_FLT(po), UINT_RGBA_G_FLT(po), UINT_RGBA_B_FLT(po), UINT_RGBA_A_FLT(po));
	context->stroke_preserve ();
	context->set_source_rgba (UINT_RGBA_R_FLT(pf), UINT_RGBA_G_FLT(pf), UINT_RGBA_B_FLT(pf), UINT_RGBA_A_FLT(pf));
	context->fill ();

	/* marker line */
	context->set_line_width (1.0);
	context->move_to (spos, 1 + top_step + pos_box_size + 4.0 * UIConfiguration::instance().get_ui_scale());
	context->line_to (spos, half_lr_box + step_down + lr_box_size - 1);
	context->set_source_rgba (UINT_RGBA_R_FLT(po), UINT_RGBA_G_FLT(po), UINT_RGBA_B_FLT(po), UINT_RGBA_A_FLT(po));
	context->stroke ();

	/* done */

	return true;
}

bool
MonoPanner::on_button_press_event (GdkEventButton* ev)
{
	if (PannerInterface::on_button_press_event (ev)) {
		return true;
	}
	if (_panner_shell->bypassed()) {
		return false;
	}

	drag_start_x = ev->x;
	last_drag_x = ev->x;

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


		if (ev->x <= width/3) {
			/* left side dbl click */
			position_control->set_value (0, Controllable::NoGroup);
		} else if (ev->x > 2*width/3) {
			position_control->set_value (1.0, Controllable::NoGroup);
		} else {
			position_control->set_value (0.5, Controllable::NoGroup);
		}

		_dragging = false;
		_tooltip.target_stop_drag ();

	} else if (ev->type == GDK_BUTTON_PRESS) {

		if (Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier)) {
			/* handled by button release */
			return true;
		}

		_dragging = true;
		_tooltip.target_start_drag ();
		StartGesture ();
	}

	return true;
}

bool
MonoPanner::on_button_release_event (GdkEventButton* ev)
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

	_dragging = false;
	_tooltip.target_stop_drag ();
	accumulated_delta = 0;
	detented = false;

	if (Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier)) {
		_panner->reset ();
	} else {
		StopGesture ();
	}

	return true;
}

bool
MonoPanner::on_scroll_event (GdkEventScroll* ev)
{
	double one_degree = 1.0/180.0; // one degree as a number from 0..1, since 180 degrees is the full L/R axis
	double pv = position_control->get_value(); // 0..1.0 ; 0 = left
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
		case GDK_SCROLL_UP:
		case GDK_SCROLL_LEFT:
			pv -= step;
			position_control->set_value (pv, Controllable::NoGroup);
			break;
		case GDK_SCROLL_DOWN:
		case GDK_SCROLL_RIGHT:
			pv += step;
			position_control->set_value (pv, Controllable::NoGroup);
			break;
	}

	return true;
}

bool
MonoPanner::on_motion_notify_event (GdkEventMotion* ev)
{
	if (_panner_shell->bypassed()) {
		_dragging = false;
	}
	if (!_dragging) {
		return false;
	}

	int w = get_width();
	double delta = (ev->x - last_drag_x) / (double) w;

	/* create a detent close to the center, at approx 1/180 deg */
	if (!detented && fabsf (position_control->get_value() - .5) < 0.006) {
		detented = true;
		/* snap to center */
		position_control->set_value (0.5, Controllable::NoGroup);
	}

	if (detented) {
		accumulated_delta += delta;

		/* have we pulled far enough to escape ? */

		if (fabs (accumulated_delta) >= 0.048) {
			position_control->set_value (position_control->get_value() + (accumulated_delta > 0 ? 0.006 : -0.006), Controllable::NoGroup);
			detented = false;
			accumulated_delta = 0;
		}
	} else {
		double pv = position_control->get_value(); // 0..1.0 ; 0 = left
		position_control->set_value (pv + delta, Controllable::NoGroup);
	}

	last_drag_x = ev->x;
	return true;
}

bool
MonoPanner::on_key_press_event (GdkEventKey* ev)
{
	double one_degree = 1.0/180.0;
	double pv = position_control->get_value(); // 0..1.0 ; 0 = left
	double step;

	if (_panner_shell->bypassed()) {
		return false;
	}

	if (Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier)) {
		step = one_degree;
	} else {
		step = one_degree * 5.0;
	}

	switch (ev->keyval) {
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
			position_control->set_value (0.0, Controllable::NoGroup);
			break;
		default:
			return false;
	}

	return true;
}

void
MonoPanner::set_colors ()
{
	colors.fill = UIConfiguration::instance().color_mod ("mono panner fill", "panner fill");
	colors.outline = UIConfiguration::instance().color ("mono panner outline");
	colors.text = UIConfiguration::instance().color ("mono panner text");
	colors.background = UIConfiguration::instance().color ("mono panner bg");
	colors.pos_outline = UIConfiguration::instance().color ("mono panner position outline");
	colors.pos_fill = UIConfiguration::instance().color_mod ("mono panner position fill", "mono panner position fill");
}

void
MonoPanner::color_handler ()
{
	set_colors ();
	queue_draw ();
}

void
MonoPanner::bypass_handler ()
{
	queue_draw ();
}

void
MonoPanner::pannable_handler ()
{
	panvalue_connections.drop_connections();
	position_control = _panner->pannable()->pan_azimuth_control;
	position_binder.set_controllable(position_control);
	position_control->Changed.connect (panvalue_connections, invalidator(*this), boost::bind (&MonoPanner::value_change, this), gui_context());
	queue_draw ();
}

PannerEditor*
MonoPanner::editor ()
{
	return new MonoPannerEditor (this);
}
