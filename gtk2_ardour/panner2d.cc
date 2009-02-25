/*
    Copyright (C) 2002 Paul Davis

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

#include <cmath>
#include <climits>
#include <string.h>

#include <cairo.h>
#include <gtkmm/menu.h>

#include "pbd/error.h"
#include "ardour/panner.h"
#include <gtkmm2ext/gtk_ui.h>

#include "panner2d.h"
#include "keyboard.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace sigc;
using namespace ARDOUR;
using namespace PBD;

Panner2d::Target::Target (float xa, float ya, const char *txt)
	: x (xa, 0.0, 1.0, 0.01, 0.1)
	, y (ya, 0.0, 1.0, 0.01, 0.1)
	, azimuth (M_PI/2.0, 0.0, 2.0 * M_PI, 0.1, 0.5)
	, text (txt ? strdup (txt) : 0)
{
	azimuth.set_value ((random() / (double) INT_MAX) * (2.0 * M_PI));
}

Panner2d::Target::~Target ()
{ 
	if (text) {
		free (text);
	}
}

void
Panner2d::Target::set_text (const char* txt)
{
	if (text) {
		free (text);
	}
	text = strdup (txt);
}

Panner2d::Panner2d (Panner& p, int32_t h)
	: panner (p), width (0), height (h)
{
	allow_x = false;
	allow_y = false;
	allow_target = false;

	panner.StateChanged.connect (mem_fun(*this, &Panner2d::handle_state_change));
	panner.Changed.connect (mem_fun(*this, &Panner2d::handle_position_change));
	
	drag_target = 0;
	set_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::POINTER_MOTION_MASK);
}

Panner2d::~Panner2d()
{
	for (Targets::iterator i = targets.begin(); i != targets.end(); ++i) {
		delete i->second;
	}
}

void
Panner2d::reset (uint32_t n_inputs)
{
	Targets::size_type existing_pucks = pucks.size();

	/* pucks */

	while (pucks.size() < n_inputs) {
		add_puck ("", 0.0, 0.0);
	}
	
	while (pucks.size() > n_inputs) {
		pucks.erase (pucks.begin());
	}

	for (Targets::iterator x = pucks.begin(); x != pucks.end(); ++x) {
		(*x).second->visible = false;
	}

	switch (n_inputs) {
	case 0:
		break;
		
	case 1:
		pucks[0]->set_text ("");
		pucks[0]->x.set_value (0.0);
		pucks[0]->y.set_value (0.5);
		pucks[0]->visible = true;
		break;
		
	case 2:
		pucks[0]->set_text ("R");
		if (existing_pucks < 0) {
			pucks[0]->x.set_value (0.5f);
			pucks[1]->y.set_value (0.25f);
		}
		pucks[0]->visible = true;
		pucks[1]->set_text ("L");
		if (existing_pucks < 2) {
			pucks[1]->x.set_value (0.25f);
			pucks[1]->y.set_value (0.5f);
		}
		pucks[1]->visible = true;
		break;
		
	default:
		for (uint32_t i = 0; i < n_inputs; ++i) {
			char buf[64];
			snprintf (buf, sizeof (buf), "%" PRIu32, i);
			pucks[i]->set_text (buf);
			
			if (existing_pucks < i) {
				float x, y;
				panner.streampanner (i).get_position (x, y);
				pucks[i]->x.set_value (x);
				pucks[i]->y.set_value (y);
			}

			pucks[i]->visible = true;
		}
		break;
	}
	
	/* add all outputs */
	
	while (targets.size() < panner.nouts()) {
		add_target (0.0, 0.0);
	}
	
	while (targets.size() > panner.nouts()) {
		targets.erase (targets.begin());
	}

	for (Targets::iterator x = targets.begin(); x != targets.end(); ++x) {
		(*x).second->visible = false;
	}

	for (uint32_t n = 0; n < panner.nouts(); ++n) {
		char buf[16];

		snprintf (buf, sizeof (buf), "%d", n+1);
		targets[n]->set_text (buf);
		targets[n]->x.set_value (panner.output(n).x);
		targets[n]->y.set_value (panner.output(n).y);
		targets[n]->visible = true;
	}
	
	allow_x_motion (true);
	allow_y_motion (true);
	allow_target_motion (true);

	queue_draw ();
}

Gtk::Adjustment&
Panner2d::azimuth (uint32_t which)
{
	assert (which < pucks.size());
	return pucks[which]->azimuth;
}

void
Panner2d::on_size_allocate (Gtk::Allocation& alloc)
{
  	width = alloc.get_width();
  	height = alloc.get_height();

	if (height > 100) {
		width -= 20;
		height -= 20;
	}

	DrawingArea::on_size_allocate (alloc);
}

int
Panner2d::add_puck (const char* text, float x, float y)
{
	Target* puck = new Target (x, y, text);

	pair<int,Target *> newpair;
	newpair.first = pucks.size();
	newpair.second = puck;

	pucks.insert (newpair);
	puck->visible = true;
	
	return 0;
}

int
Panner2d::add_target (float x, float y)
{
	Target *target = new Target (x, y, "");

	pair<int,Target *> newpair;
	newpair.first = targets.size();
	newpair.second = target;

	targets.insert (newpair);
	target->visible = true;
	queue_draw ();

	return newpair.first;
}

void
Panner2d::drop_targets ()
{
	for (Targets::iterator i = targets.begin(); i != targets.end(); ) {

		Targets::iterator tmp;

		tmp = i;
		++tmp;

		delete i->second;
		targets.erase (i);

		i = tmp;
	}

	queue_draw ();
}

void
Panner2d::remove_target (int which)
{
	Targets::iterator i = targets.find (which);

	if (i != targets.end()) {
		delete i->second;
		targets.erase (i);
		queue_draw ();
	}
}		

void
Panner2d::handle_state_change ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &Panner2d::handle_state_change));

	queue_draw ();
}

void
Panner2d::handle_position_change ()
{
	uint32_t n;
	ENSURE_GUI_THREAD(mem_fun(*this, &Panner2d::handle_position_change));

	for (n = 0; n < pucks.size(); ++n) {
		float x, y;
		panner.streampanner(n).get_position (x, y);
		pucks[n]->x.set_value (x);
		pucks[n]->y.set_value (y);
	}

	for (n = 0; n < targets.size(); ++n) {
		targets[n]->x.set_value (panner.output(n).x);
		targets[n]->y.set_value (panner.output(n).y);
	}

	queue_draw ();
}

void
Panner2d::move_target (int which, float x, float y)
{
	Targets::iterator i = targets.find (which);
	Target *target;

	if (!allow_target) {
		return;
	}

	if (i != targets.end()) {
		target = i->second;
		target->x.set_value (x);
		target->y.set_value (y);
		
		queue_draw ();
	}
}		

void
Panner2d::move_puck (int which, float x, float y)
{
	Targets::iterator i = pucks.find (which);
	Target *target;

	if (i != pucks.end()) {
		target = i->second;
		target->x.set_value (x);
		target->y.set_value (y);
		
		queue_draw ();
	}
}		

void
Panner2d::show_puck (int which)
{
	Targets::iterator i = pucks.find (which);

	if (i != pucks.end()) {
		Target* puck = i->second;
		if (!puck->visible) {
			puck->visible = true;
			queue_draw ();
		}
	}
}

void
Panner2d::hide_puck (int which)
{
	Targets::iterator i = pucks.find (which);

	if (i != pucks.end()) {
		Target* puck = i->second;
		if (!puck->visible) {
			puck->visible = false;
			queue_draw ();
		}
	}
}

void
Panner2d::show_target (int which)
{
	Targets::iterator i = targets.find (which);
	if (i != targets.end()) {
		if (!i->second->visible) {
			i->second->visible = true;
			queue_draw ();
		}
	}
}

void
Panner2d::hide_target (int which)
{
	Targets::iterator i = targets.find (which);
	if (i != targets.end()) {
		if (i->second->visible) {
			i->second->visible = false;
			queue_draw ();
		}
	}
}

Panner2d::Target *
Panner2d::find_closest_object (gdouble x, gdouble y, int& which, bool& is_puck) const
{
	gdouble efx, efy;
	gdouble cx, cy;
	Target *closest = 0;
	Target *candidate;
	float distance;
	float best_distance = FLT_MAX;
	int pwhich;

	efx = x/width;
	efy = y/height;
	which = 0;
	pwhich = 0;
	is_puck = false;

	for (Targets::const_iterator i = targets.begin(); i != targets.end(); ++i, ++which) {
		candidate = i->second;

		cx = candidate->x.get_value();
		cy = candidate->y.get_value();

		distance = sqrt ((cx - efx) * (cx - efx) +
				 (cy - efy) * (cy - efy));

		if (distance < best_distance) {
			closest = candidate;
			best_distance = distance;
		}
	}

	for (Targets::const_iterator i = pucks.begin(); i != pucks.end(); ++i, ++pwhich) {
		candidate = i->second;

		cx = candidate->x.get_value();
		cy = candidate->y.get_value();

		distance = sqrt ((cx - efx) * (cx - efx) +
				 (cy - efy) * (cy - efy));

		if (distance < best_distance) {
			closest = candidate;
			best_distance = distance;
			is_puck = true;
			which = pwhich;
		}
	}
	
	return closest;
}		

bool
Panner2d::on_motion_notify_event (GdkEventMotion *ev)
{
	gint x, y;
	GdkModifierType state;

	if (ev->is_hint) {
		gdk_window_get_pointer (ev->window, &x, &y, &state);
	} else {
		x = (int) floor (ev->x);
		y = (int) floor (ev->y);
		state = (GdkModifierType) ev->state;
	}

	return handle_motion (x, y, state);
}
bool
Panner2d::on_expose_event (GdkEventExpose *event)
{
	gint x, y;
	float fx, fy;
	cairo_t* cr;

	cr = gdk_cairo_create (get_window()->gobj());

	cairo_set_line_width (cr, 1.0);

	cairo_rectangle (cr, event->area.x, event->area.y, event->area.width, event->area.height);
	if (!panner.bypassed()) {
		cairo_set_source_rgba (cr, 0.1, 0.1, 0.1, 1.0);
	} else {
		cairo_set_source_rgba (cr, 0.1, 0.1, 0.1, 0.2);
	}
	cairo_fill_preserve (cr);
	cairo_clip (cr);

	if (height > 100) {
		cairo_translate (cr, 10.0, 10.0);
	}

	cairo_set_source_rgb (cr, 0.0, 0.1, 0.7);
	cairo_move_to (cr, 0.5, height/2.0+0.5);
	cairo_line_to (cr, height+0.5, height/2+0.5);
	cairo_stroke (cr);

	cairo_move_to (cr, height/2+0.5, 0.5);
	cairo_line_to (cr, height/2+0.5, height+0.5);
	cairo_stroke (cr);

	cairo_arc (cr, height/2, height/2, height/2, 0, 2.0 * M_PI);
	cairo_stroke (cr);

	if (!panner.bypassed()) {
		float arc_radius;

		cairo_select_font_face (cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

		if (height < 100) {
			cairo_set_font_size (cr, 10);
			arc_radius = 2.0;
		} else {
			cairo_set_font_size (cr, 16);
			arc_radius = 4.0;
		}

		for (Targets::iterator i = pucks.begin(); i != pucks.end(); ++i) {

			Target* puck = i->second;

			if (puck->visible) {
				/* redraw puck */
				
				fx = min (puck->x.get_value(), 1.0);
				fx = max (fx, -1.0f);
				x = (gint) floor (width * fx - 4);
				
				fy = min (puck->y.get_value(), 1.0);
				fy = max (fy, -1.0f);
				y = (gint) floor (height * fy - 4);
				
				cairo_arc (cr, x, y, arc_radius, 0, 2.0 * M_PI);
				cairo_set_source_rgb (cr, 0.8, 0.2, 0.1);
				cairo_close_path (cr);
				cairo_fill (cr);

				/* arrow */
				
				if (height > 100.0f) {

					float endx, endy;
					endx = x;
					endy = y;
					
					cairo_save (cr);
					cairo_translate (cr, x, y);
					cairo_rotate (cr, puck->azimuth.get_value());

					/* horizontal left-to-right line (rotation will rotate it, duh) */

					endx = 30.0;
					endy = 0.0;

					/* stem */
					cairo_set_line_width (cr, 4.0);
					cairo_move_to (cr, 0.0, 0.0);
					cairo_line_to (cr, endx, endy);
					cairo_stroke (cr);
					
					/* arrow head */
					
					cairo_move_to (cr, endx - 10.0, endy + 10.0);
					cairo_line_to (cr, endx, endy);
					cairo_line_to (cr, endx - 10.0, endy - 10.0);
					cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
					cairo_stroke (cr);

					cairo_restore (cr);
				}

				cairo_move_to (cr, x + 6, y + 6);
				cairo_show_text (cr, puck->text);
			}
		}

		/* redraw any visible targets */
		
		int n = 0;

		for (Targets::iterator i = targets.begin(); i != targets.end(); ++i) {
			Target *target = i->second;
			char buf[256];
			++n;

			if (target->visible) {
				
				fx = min (target->x.get_value(), 1.0);
				fx = max (fx, -1.0f);
				x = (gint) floor (width  * fx);
			
				fy = min (target->y.get_value(), 1.0);
				fy = max (fy, -1.0f);
				y = (gint) floor (height * fy);

				snprintf (buf, sizeof (buf), "%d", n);

				cairo_set_source_rgb (cr, 0.0, 0.8, 0.1);
				cairo_rectangle (cr, x-2, y-2, 4, 4);
				cairo_fill (cr);
				cairo_move_to (cr, x+6, y+6);
				cairo_show_text (cr, buf);
			}
		}
	}

	cairo_destroy (cr);

	return TRUE;
}

bool
Panner2d::on_button_press_event (GdkEventButton *ev)
{
	GdkModifierType state;

	if (ev->type == GDK_2BUTTON_PRESS && ev->button == 1) {
		return false;
	}

	switch (ev->button) {
	case 1:
	case 2:
		drag_target = find_closest_object (ev->x, ev->y, drag_index, drag_is_puck);
		drag_x = (int) floor (ev->x);
		drag_y = (int) floor (ev->y);
		state = (GdkModifierType) ev->state;

		return handle_motion (drag_x, drag_y, state);
		break;

	default:
		break;
	}
	
	return FALSE;
}

bool
Panner2d::on_button_release_event (GdkEventButton *ev)
{
	gint x, y;
	GdkModifierType state;
	bool ret = false;

	switch (ev->button) {
	case 1:
		x = (int) floor (ev->x);
		y = (int) floor (ev->y);
		state = (GdkModifierType) ev->state;

		if (drag_is_puck && (Keyboard::modifier_state_contains (state, Keyboard::TertiaryModifier))) {


			for (Targets::iterator i = pucks.begin(); i != pucks.end(); ++i) {
				//Target* puck = i->second;

				/* XXX DO SOMETHING TO SET PUCK BACK TO "normal" */
			}

			queue_draw ();
			PuckMoved (-1);
		        ret = true;

		} else {
			ret = handle_motion (x, y, state);
		}
		
		drag_target = 0;
		break;

	case 2:
		x = (int) floor (ev->x);
		y = (int) floor (ev->y);
		state = (GdkModifierType) ev->state;

		if (drag_is_puck && (Keyboard::modifier_state_contains (state, Keyboard::TertiaryModifier))) {
			toggle_bypass ();
			ret = true;
		} else {
			ret = handle_motion (x, y, state);
		}
		
		drag_target = 0;
		break;

	case 3:
		break;

	}

	return ret;
}

gint
Panner2d::handle_motion (gint evx, gint evy, GdkModifierType state)
{
	if (drag_target == 0) {
		return false;
	}

	if ((state & (GDK_BUTTON1_MASK|GDK_BUTTON2_MASK)) == 0) {
		return false;
	}

	int x, y;
	bool need_move = false;

	if (!drag_is_puck && !allow_target) {
		cerr << "dip = " << drag_is_puck << " at = " << allow_target << endl;
		return true;
	}

	if (state & GDK_BUTTON1_MASK && !(state & GDK_BUTTON2_MASK)) {

		if (allow_x || !drag_is_puck) {
			float new_x;
			x = min (evx, width - 1);
			x = max (x, 0);
			new_x = (float) x / (width - 1);
			if (new_x != drag_target->x.get_value()) {
				drag_target->x.set_value (new_x);
				need_move = true;
			}
		}
		
		if (allow_y || drag_is_puck) {
			float new_y;
			y = min (evy, height - 1);
			y = max (y, 0);
			new_y = (float) y / (height - 1);
			if (new_y != drag_target->y.get_value()) {
				drag_target->y.set_value (new_y);
				need_move = true;
			}
		}
		
		if (need_move) {
			
			if (drag_is_puck) {
				
				panner.streampanner(drag_index).set_position (drag_target->x.get_value(), drag_target->y.get_value(), false);
				
			} else {
				
				TargetMoved (drag_index);
			}

			queue_draw ();
		}
		

	} else if ((state & GDK_BUTTON2_MASK) && !(state & GDK_BUTTON1_MASK)) {

		if (!drag_is_puck) {
			return false;
		}

		int xdelta = drag_x - evx;
		int ydelta = drag_x - evy;
		
		drag_target->azimuth.set_value (drag_target->azimuth.get_value() + (2 * M_PI) * ((float)ydelta)/height * ((float) -xdelta)/height);
		queue_draw ();
	}

	return true;
}

void
Panner2d::toggle_bypass ()
{
	panner.set_bypassed (!panner.bypassed());
}

void
Panner2d::allow_x_motion (bool yn)
{
	allow_x = yn;
}

void
Panner2d::allow_target_motion (bool yn)
{
	allow_target = yn;
}

void
Panner2d::allow_y_motion (bool yn)
{
	allow_y = yn;
}

Panner2dWindow::Panner2dWindow (Panner&p, int32_t h, uint32_t inputs)
	: widget (p, h)
	, reset_button (_("Reset"))
	, bypass_button (_("Bypass"))
	, mute_button (_("Mute"))
{
	widget.set_name ("MixerPanZone");

	set_title (_("Panner"));
	widget.set_size_request (h, h);
	
	button_box.set_spacing (6);
	button_box.pack_start (reset_button, false, false);
	button_box.pack_start (bypass_button, false, false);
	button_box.pack_start (mute_button, false, false);

	spinner_box.set_spacing (6);
	left_side.set_spacing (6);

	left_side.pack_start (button_box, false, false);
	left_side.pack_start (spinner_box, false, false);

	reset_button.show ();
	bypass_button.show ();
	mute_button.show ();
	button_box.show ();
	spinner_box.show ();
	left_side.show ();

	hpacker.set_spacing (6);
	hpacker.set_border_width (12);
	hpacker.pack_start (widget, false, false);
	hpacker.pack_start (left_side, false, false);
	hpacker.show ();
	
	add (hpacker);
	reset (inputs);
	widget.show ();
}

void
Panner2dWindow::reset (uint32_t n_inputs)
{
	widget.reset (n_inputs);

	while (spinners.size() < n_inputs) {
		spinners.push_back (new Gtk::SpinButton (widget.azimuth (spinners.size())));
		spinner_box.pack_start (*spinners.back(), false, false);
		spinners.back()->set_digits (4);
		spinners.back()->show ();
	}

	while (spinners.size() > n_inputs) {
		spinner_box.remove (*spinners.back());
		delete spinners.back();
		spinners.erase (--spinners.end());
	}
}
