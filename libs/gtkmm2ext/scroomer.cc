/*
    Copyright (C) 2008 Paul Davis 

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

#include <iostream>

#include "gtkmm2ext/scroomer.h"
#include "gtkmm2ext/keyboard.h"

using namespace Gtkmm2ext;
using namespace Gtk;
using namespace Gdk;
using namespace std;

Scroomer::Scroomer(Gtk::Adjustment& adjustment)
	: adj(adjustment)
	, handle_size(0)
	, grab_comp(None)
{
	position[TopBase] = 0;
	position[Handle1] = 0;
	position[Slider] = 0;
	position[Handle2] = 0;
	position[BottomBase] = 0;
	position[Total] = 0;

	add_events (Gdk::BUTTON_PRESS_MASK |
		    Gdk::BUTTON_RELEASE_MASK |
		    Gdk::POINTER_MOTION_MASK |
		    Gdk::SCROLL_MASK);

	adjustment.signal_value_changed().connect (mem_fun (*this, &Scroomer::adjustment_changed));
	//adjustment.signal_changed().connect (mem_fun (*this, &Scroomer::adjustment_changed));
}

Scroomer::~Scroomer()
{
}

bool
Scroomer::on_motion_notify_event (GdkEventMotion* ev)
{
	double range = adj.get_upper() - adj.get_lower();
	double pixel2val = range / get_height();
	double val_at_pointer = ((get_height() - ev->y) * pixel2val) + adj.get_lower();
	double delta_y = ev->y - grab_y;
	double half_min_page = min_page_size / 2;
	double fract = delta_y / position[Total];
	double scale, temp, zoom;
	double val, page;

	if (grab_comp == None || grab_comp == Total) {
		return true;
	}

	if (ev->window != grab_window) {
		grab_y = ev->y;
		grab_window = ev->window;
		return true;
	}

	if (ev->y < 0 || ev->y > get_height ()) {
		return true;
	}

	grab_y = ev->y;

	if (ev->state & Keyboard::PrimaryModifier) {
		if (ev->state & Keyboard::SecondaryModifier) {
			scale = 0.05;
		} else {
			scale = 0.1;
		}
	} else {
		scale = 1.0;
	}

	fract = min (1.0, fract);
	fract = max (-1.0, fract);
	fract = -fract;

	switch (grab_comp) {
	case TopBase:
	case BottomBase:
		unzoomed_val += scale * fract * range;
		unzoomed_val = min(unzoomed_val, adj.get_upper() - unzoomed_page);
		unzoomed_val = max(unzoomed_val, adj.get_lower());
		break;
	case Slider:
		unzoomed_val += scale * fract * range;
		unzoomed_val = min(unzoomed_val, adj.get_upper() - unzoomed_page);
		unzoomed_val = max(unzoomed_val, adj.get_lower());
		break;
	case Handle1:

		unzoomed_page += scale * fract * range;
		unzoomed_page = min(unzoomed_page, adj.get_upper() - unzoomed_val);
		unzoomed_page = max(unzoomed_page, min_page_size);
		
		if (pinch){
			temp = unzoomed_val + unzoomed_page;
			unzoomed_val -= scale * fract * range * 0.5;
			unzoomed_val = min(unzoomed_val, temp - min_page_size);
			unzoomed_val = max(unzoomed_val, adj.get_lower());
		}
		
		break;
	case Handle2:
		temp = unzoomed_val + unzoomed_page;
		unzoomed_val += scale * fract * range;
		unzoomed_val = min(unzoomed_val, temp - min_page_size);
		unzoomed_val = max(unzoomed_val, adj.get_lower());
		
		unzoomed_page = temp - unzoomed_val;
		
		if (pinch){
			
			unzoomed_page -= scale * fract * range;
		}
		
		unzoomed_page = min(unzoomed_page, adj.get_upper() - unzoomed_val);		
		unzoomed_page = max(unzoomed_page, min_page_size);
		break;
	default:
		break;
	}

	/* Then we handle zoom, which is dragging horizontally. We zoom around the area that is
	 * the current y pointer value, not from the area that was the start of the drag.
	 * We don't start doing zoom until we are at least one scroomer width outside the scroomer's
	 * area.
	 */
	
	if (ev->x > (get_width() * 2)) {
		zoom = ev->x - get_width();
		
		double higher = unzoomed_val + unzoomed_page - half_min_page - val_at_pointer;
		double lower = val_at_pointer - (unzoomed_val + half_min_page);

		higher *= zoom / 128;
		lower *= zoom / 128;

		val = unzoomed_val + lower;
		page = unzoomed_page - higher - lower;

		page = max(page, min_page_size);

		if (lower < 0) {
			val = max(val, val_at_pointer - half_min_page);
		} else if (lower > 0) {
			val = min(val, val_at_pointer - half_min_page);
		}

		val = min(val, adj.get_upper() - min_page_size);
		page = min(page, adj.get_upper() - val);
	} else if (ev->x < 0) {
		/* on zoom out increase the page size as well as moving the range towards the mouse pos*/
		zoom = abs(ev->x);

		/*double higher = unzoomed_val + unzoomed_page - half_min_page - val_at_pointer;
		double lower = val_at_pointer - (unzoomed_val + half_min_page);

		higher *= zoom / 128;
		lower *= zoom / 128;

		val = unzoomed_val + lower;
		page = unzoomed_page - higher - lower;

		page = max(page, min_page_size);

		if (lower < 0) {
			val = max(val, val_at_pointer - half_min_page);
		}
		else if (lower > 0) {
			val = min(val, val_at_pointer - half_min_page);
		}

		val = min(val, adj.get_upper() - min_page_size);
		page = min(page, adj.get_upper() - val);*/

		val = unzoomed_val;
		page = unzoomed_page;
	} else {
		val = unzoomed_val;
		page = unzoomed_page;
	}

	/* Round these values to stop the scroomer handlers quivering about during drags */
	adj.set_page_size (rint (page));
	adj.set_value (rint (val));
	adj.value_changed();
	
	return true;
}

bool
Scroomer::on_scroll_event (GdkEventScroll* ev)
{
	switch (ev->direction) {
	case GDK_SCROLL_UP:
		adj.set_value (adj.get_value() + adj.get_page_size() / 10.0);
		break;
	case GDK_SCROLL_DOWN:
		adj.set_value (adj.get_value() - adj.get_page_size() / 10.0);
		break;
	default:
		return false;
	}

	return true;
}

bool
Scroomer::on_button_press_event (GdkEventButton* ev)
{
	if (ev->button == 1 || ev->button == 3) {
		Component comp = point_in(ev->y);

		if (comp == Total || comp == None) {
			return false;
		}

		add_modal_grab();
		grab_comp = comp;
		grab_y = ev->y;
		unzoomed_val = adj.get_value();
		unzoomed_page = adj.get_page_size();
		grab_window = ev->window;
		
		if (ev->button == 3){
			pinch = true;
		} else {
                        pinch = false;
                }

		DragStarting (); /* EMIT SIGNAL */
	}
	
	if (ev->type == GDK_2BUTTON_PRESS && ev->button == 1) {
		DoubleClicked();
	}

	return false;
}

bool
Scroomer::on_button_release_event (GdkEventButton* ev)
{
	if (grab_comp == None || grab_comp == Total) {
		return true;
	}

	if (ev->window != grab_window) {
		grab_y = ev->y;
		grab_window = ev->window;
		return true;
	}

	if (ev->button != 1 && ev->button != 3) {
		return true;
	}

	switch (grab_comp) {
	case TopBase:
		break;
	case Handle1:
		break;
	case Slider:
		break;
	case Handle2:
		break;
	case BottomBase:
		break;
	default:
		break;
	}
	
	grab_comp = None;

	remove_modal_grab();
	DragFinishing (); /* EMIT SIGNAL */
	return true;
}

void
Scroomer::on_size_allocate (Allocation& a)
{
	Gtk::DrawingArea::on_size_allocate(a);

	position[Total] = a.get_height();
	set_min_page_size(min_page_size);
	update();
}

/** Assumes that x and width are correct, and they will not be altered.
 */
void
Scroomer::set_comp_rect(GdkRectangle& r, Component c) const
{
	int index = (int) c;

	switch (c) {
	case None:
		return;
	case Total:
		r.y = 0;
		r.height = position[Total];
		break;
	default:
		r.y = position[index];
		r.height = position[index+1] - position[index];
		break;
	}
}

Scroomer::Component
Scroomer::point_in(double point) const
{
	for (int i = 0; i < Total; ++i) {
		if (position[i+1] >= point) {
			return (Component) i;
		}
	}

	return None;
}

void
Scroomer::set_min_page_size(double ps)
{
	double coeff = ((double)position[Total]) / (adj.get_upper() - adj.get_lower());

	min_page_size = ps;
	handle_size = (int) floor((ps * coeff) / 2);
}

void
Scroomer::update()
{
	double range = adj.get_upper() - adj.get_lower();
	//double value = adj.get_value() - adj.get_lower();
	int height = position[Total];
	double coeff = ((double) height) / range;

	/* save the old positions to calculate update regions later*/
	for (int i = Handle1; i < Total; ++i) {
		old_pos[i] = position[i];
	}

	position[BottomBase] = (int) floor(height - (adj.get_value() * coeff));
	position[Handle2] = position[BottomBase] - handle_size;

	position[Handle1] = (int) floor(height - ((adj.get_value() + adj.get_page_size()) * coeff));
	position[Slider] = position[Handle1] + handle_size;
}

void
Scroomer::adjustment_changed()
{
	//cerr << floor(adj.get_value()) << " " << floor(adj.get_value() + adj.get_page_size()) << endl;
	Gdk::Rectangle rect;
	Glib::RefPtr<Gdk::Window> win = get_window();

	update();

	if (!win) {
		return;
	}

	rect.set_x(0);
	rect.set_width(get_width());

	if (position[Handle1] < old_pos[Handle1]) {
		rect.set_y(position[Handle1]);
		rect.set_height(old_pos[Slider] - position[Handle1]);
		win->invalidate_rect(rect, false);
	} else if (position[Handle1] > old_pos[Handle1]) {
		rect.set_y(old_pos[Handle1]);
		rect.set_height(position[Slider] - old_pos[Handle1]);
		win->invalidate_rect(rect, false);
	}

	if (position[Handle2] < old_pos[Handle2]) {
		rect.set_y(position[Handle2]);
		rect.set_height(old_pos[BottomBase] - position[Handle2]);
		win->invalidate_rect(rect, false);
	} else if (position[Handle2] > old_pos[Handle2]) {
		rect.set_y(old_pos[Handle2]);
		rect.set_height(position[BottomBase] - old_pos[Handle2]);
		win->invalidate_rect(rect, false);
	}
}

