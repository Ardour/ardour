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

    $Id$
*/

#include <cmath>
#include <climits>
#include <string.h>

#include <gtkmm/menu.h>
#include <gtkmm/checkmenuitem.h>

#include <pbd/error.h>
#include <ardour/panner.h>
#include <gtkmm2ext/gtk_ui.h>

#include "panner2d.h"
#include "keyboard.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace sigc;
using namespace ARDOUR;

Panner2d::Target::Target (float xa, float ya, const char *txt)
	: x (xa), y (ya), text (txt ? strdup (txt) : 0)
{
	if (text) {
		textlen = strlen (txt);
	} else {
		textlen = 0;
	}
}

Panner2d::Target::~Target ()
{ 
	if (text) {
		free (text);
	}
}

Panner2d::Panner2d (Panner& p, int32_t w, int32_t h)
	: panner (p), width (w), height (h)
{
	context_menu = 0;
	bypass_menu_item = 0;

	allow_x = false;
	allow_y = false;
	allow_target = false;

	panner.StateChanged.connect (mem_fun(*this, &Panner2d::handle_state_change));
	
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
	/* add pucks */
	
	drop_pucks ();
	
	switch (n_inputs) {
	case 0:
		break;
		
	case 1:
		add_puck ("", 0.0f, 0.5f);
		break;
		
	case 2:
		add_puck ("L", 0.5f, 0.25f);
		add_puck ("R", 0.25f, 0.5f);
		show_puck (0);
		show_puck (1);
		break;
		
	default:
		for (uint32_t i = 0; i < n_inputs; ++i) {
			char buf[64];
			snprintf (buf, sizeof (buf), "%" PRIu32, i);
			add_puck (buf, 0.0f, 0.5f);
			show_puck (i);
		}
		break;
	}
	
	/* add all outputs */
	
	drop_targets ();
	
	for (uint32_t n = 0; n < panner.nouts(); ++n) {
		add_target (panner.output (n).x, panner.output (n).y);
	}
	
	allow_x_motion (true);
	allow_y_motion (true);
	allow_target_motion (true);
}

void
Panner2d::on_size_allocate (Gtk::Allocation& alloc)
{
  	width = alloc.get_width();
  	height = alloc.get_height();

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
Panner2d::drop_pucks ()
{
	for (Targets::iterator i = pucks.begin(); i != pucks.end(); ) {

		Targets::iterator tmp;

		tmp = i;
		++tmp;

		delete i->second;
		pucks.erase (i);

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
Panner2d::move_target (int which, float x, float y)
{
	Targets::iterator i = targets.find (which);
	Target *target;

	if (!allow_target) {
		return;
	}

	if (i != targets.end()) {
		target = i->second;
		target->x = x;
		target->y = y;
		
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
		target->x = x;
		target->y = y;
		
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

		distance = sqrt ((candidate->x - efx) * (candidate->x - efx) +
				 (candidate->y - efy) * (candidate->y - efy));

		if (distance < best_distance) {
			closest = candidate;
			best_distance = distance;
		}
	}

	for (Targets::const_iterator i = pucks.begin(); i != pucks.end(); ++i, ++pwhich) {
		candidate = i->second;

		distance = sqrt ((candidate->x - efx) * (candidate->x - efx) +
				 (candidate->y - efy) * (candidate->y - efy));

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
gint
Panner2d::handle_motion (gint evx, gint evy, GdkModifierType state)
{
	if (drag_target == 0 || (state & GDK_BUTTON1_MASK) == 0) {
		return FALSE;
	}

	int x, y;
	bool need_move = false;

	if (!drag_is_puck && !allow_target) {
		return TRUE;
	}

	if (allow_x || !drag_is_puck) {
		float new_x;
		x = min (evx, width - 1);
		x = max (x, 0);
		new_x = (float) x / (width - 1);
		if (new_x != drag_target->x) {
			drag_target->x = new_x;
			need_move = true;
		}
	}

	if (allow_y || drag_is_puck) {
		float new_y;
		y = min (evy, height - 1);
		y = max (y, 0);
		new_y = (float) y / (height - 1);
		if (new_y != drag_target->y) {
			drag_target->y = new_y;
			need_move = true;
		}
	}

	if (need_move) {
		queue_draw ();

		if (drag_is_puck) {
			
			panner[drag_index]->set_position (drag_target->x, drag_target->y);

		} else {

			TargetMoved (drag_index);
		}
	}

	return TRUE;
}

bool
Panner2d::on_expose_event (GdkEventExpose *event)
{
	gint x, y;
	float fx, fy;

	if (layout == 0) {
		layout = create_pango_layout ("");
		layout->set_font_description (get_style()->get_font());
	}

	/* redraw the background */

	get_window()->draw_rectangle (get_style()->get_bg_gc(get_state()),
				     true,
				     event->area.x, event->area.y,
				     event->area.width, event->area.height);
	

	if (!panner.bypassed()) {

		for (Targets::iterator i = pucks.begin(); i != pucks.end(); ++i) {

			Target* puck = i->second;

			if (puck->visible) {
				/* redraw puck */
				
				fx = min (puck->x, 1.0f);
				fx = max (fx, -1.0f);
				x = (gint) floor (width * fx - 4);
				
				fy = min (puck->y, 1.0f);
				fy = max (fy, -1.0f);
				y = (gint) floor (height * fy - 4);
				
				get_window()->draw_arc (get_style()->get_fg_gc(Gtk::STATE_NORMAL),
						       true,
						       x, y,
						       8, 8,
						       0, 360 * 64);

				layout->set_text (puck->text);

				get_window()->draw_layout (get_style()->get_fg_gc (STATE_NORMAL), x+6, y+6, layout);
			}
		}

		/* redraw any visible targets */

		for (Targets::iterator i = targets.begin(); i != targets.end(); ++i) {
			Target *target = i->second;

			if (target->visible) {
				
				/* why -8 ??? why is this necessary ? */
				
				fx = min (target->x, 1.0f);
				fx = max (fx, -1.0f);
				x = (gint) floor ((width - 8) * fx);
			
				fy = min (target->y, 1.0f);
				fy = max (fy, -1.0f);
				y = (gint) floor ((height - 8) * fy);

				get_window()->draw_rectangle (get_style()->get_fg_gc(Gtk::STATE_ACTIVE),
							     true,
							     x, y,
							     4, 4);
			}
		}
	}

	return TRUE;
}

bool
Panner2d::on_button_press_event (GdkEventButton *ev)
{
	switch (ev->button) {
	case 1:
		gint x, y;
		GdkModifierType state;

		drag_target = find_closest_object (ev->x, ev->y, drag_index, drag_is_puck);
		
		x = (int) floor (ev->x);
		y = (int) floor (ev->y);
		state = (GdkModifierType) ev->state;

		return handle_motion (x, y, state);
		break;
	default:
		break;
	}
	
	return FALSE;
}

bool
Panner2d::on_button_release_event (GdkEventButton *ev)
{
	switch (ev->button) {
	case 1:
		gint x, y;
		int ret;
		GdkModifierType state;

		x = (int) floor (ev->x);
		y = (int) floor (ev->y);
		state = (GdkModifierType) ev->state;

		if (drag_is_puck && (Keyboard::modifier_state_contains (state, Keyboard::Shift))) {
			
			for (Targets::iterator i = pucks.begin(); i != pucks.end(); ++i) {
				Target* puck = i->second;
				puck->x = 0.5;
				puck->y = 0.5;
			}

			queue_draw ();
			PuckMoved (-1);
		        ret = TRUE;

		} else {
			ret = handle_motion (x, y, state);
		}
		
		drag_target = 0;

		return ret;
		break;
	case 2:
		toggle_bypass ();
		return TRUE;

	case 3:
		show_context_menu ();
		break;

	}

	return FALSE;
}

void
Panner2d::toggle_bypass ()
{
	if (bypass_menu_item && (panner.bypassed() != bypass_menu_item->get_active())) {
		panner.set_bypassed (!panner.bypassed());
	}
}

void
Panner2d::show_context_menu ()
{
	using namespace Menu_Helpers;

	if (context_menu == 0) {
		context_menu = manage (new Menu);
		context_menu->set_name ("ArdourContextMenu");
		MenuList& items = context_menu->items();

		items.push_back (CheckMenuElem (_("Bypass")));
		bypass_menu_item = static_cast<CheckMenuItem*> (&items.back());
		bypass_menu_item->signal_toggled().connect (mem_fun(*this, &Panner2d::toggle_bypass));

	} 

	bypass_menu_item->set_active (panner.bypassed());
	context_menu->popup (1, 0);
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

int
Panner2d::puck_position (int which, float& x, float& y)
{
	Targets::iterator i;

	if ((i = pucks.find (which)) != pucks.end()) {
		x = i->second->x;
		y = i->second->y;
		return 0;
	}

	return -1;
}

int
Panner2d::target_position (int which, float& x, float& y)
{
	Targets::iterator i;

	if ((i = targets.find (which)) != targets.end()) {
		x = i->second->x;
		y = i->second->y;
		return 0;
	}

	return -1;
}
	
