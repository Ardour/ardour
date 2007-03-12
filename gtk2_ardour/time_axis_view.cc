/*
    Copyright (C) 2000 Paul Davis 

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

#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <string>
#include <list>

#include <libgnomecanvasmm.h>
#include <libgnomecanvasmm/canvas.h>
#include <libgnomecanvasmm/item.h>

#include <pbd/error.h>

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/stop_signal.h>

#include <ardour/session.h>
#include <ardour/utils.h>
#include <ardour/ladspa_plugin.h>
#include <ardour/insert.h>
#include <ardour/location.h>

#include "ardour_ui.h"
#include "public_editor.h"
#include "time_axis_view.h"
#include "simplerect.h"
#include "selection.h"
#include "keyboard.h"
#include "rgb_macros.h"
#include "utils.h"

#include "i18n.h"

using namespace Gtk;
using namespace Gdk;
using namespace sigc; 
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace ArdourCanvas;

const double trim_handle_size = 6.0; /* pixels */

uint32_t TimeAxisView::hLargest = 0;
uint32_t TimeAxisView::hLarge = 0;
uint32_t TimeAxisView::hLarger = 0;
uint32_t TimeAxisView::hNormal = 0;
uint32_t TimeAxisView::hSmaller = 0;
uint32_t TimeAxisView::hSmall = 0;
bool TimeAxisView::need_size_info = true;

TimeAxisView::TimeAxisView (ARDOUR::Session& sess, PublicEditor& ed, TimeAxisView* rent, Canvas& canvas) 
	: AxisView (sess), 
	  editor (ed),
	  controls_table (2, 8)
{
	if (need_size_info) {
		compute_controls_size_info ();
		need_size_info = false;
	}

	canvas_display = new Group (*canvas.root(), 0.0, 0.0);
	
	selection_group = new Group (*canvas_display);
	selection_group->hide();
	
   	control_parent = 0;
	display_menu = 0;
	size_menu = 0;
	_marked_for_display = false;
	_hidden = false;
	height = 0;
	effective_height = 0;
	parent = rent;
	_has_state = false;
	last_name_entry_key_press_event = 0;
	name_packing = NamePackingBits (0);

	/*
	  Create the standard LHS Controls
	  We create the top-level container and name add the name label here,
	  subclasses can add to the layout as required
	*/

	name_entry.set_name ("EditorTrackNameDisplay");
	name_entry.signal_button_release_event().connect (mem_fun (*this, &TimeAxisView::name_entry_button_release));
	name_entry.signal_button_press_event().connect (mem_fun (*this, &TimeAxisView::name_entry_button_press));
	name_entry.signal_key_release_event().connect (mem_fun (*this, &TimeAxisView::name_entry_key_release));
	name_entry.signal_activate().connect (mem_fun(*this, &TimeAxisView::name_entry_activated));
	name_entry.signal_focus_in_event().connect (mem_fun (*this, &TimeAxisView::name_entry_focus_in));
	name_entry.signal_focus_out_event().connect (mem_fun (*this, &TimeAxisView::name_entry_focus_out));
	Gtkmm2ext::set_size_request_to_display_given_text (name_entry, N_("gTortnam"), 10, 10); // just represents a short name

	name_label.set_name ("TrackLabel");
	name_label.set_alignment (0.0, 0.5);

	/* typically, either name_label OR name_entry are visible,
	   but not both. its up to derived classes to show/hide them as they
	   wish.
	*/

	name_hbox.show ();

	controls_table.set_border_width (2);
	controls_table.set_row_spacings (0);
	controls_table.set_col_spacings (0);
	controls_table.set_homogeneous (true);

	controls_table.attach (name_hbox, 0, 4, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	controls_table.show_all ();
	controls_table.set_no_show_all ();

	controls_vbox.pack_start (controls_table, false, false);
	controls_vbox.show ();

	controls_ebox.set_name ("TimeAxisViewControlsBaseUnselected");
	controls_ebox.add (controls_vbox);
	controls_ebox.add_events (BUTTON_PRESS_MASK|BUTTON_RELEASE_MASK|SCROLL_MASK);
	controls_ebox.set_flags (CAN_FOCUS);

	controls_ebox.signal_button_release_event().connect (mem_fun (*this, &TimeAxisView::controls_ebox_button_release));
	controls_ebox.signal_scroll_event().connect (mem_fun (*this, &TimeAxisView::controls_ebox_scroll), true);

	controls_lhs_pad.set_name ("TimeAxisViewControlsPadding");
	controls_hbox.pack_start (controls_lhs_pad,false,false);
	controls_hbox.pack_start (controls_ebox,true,true);
	controls_hbox.show ();

	controls_frame.add (controls_hbox);
	controls_frame.set_name ("TimeAxisViewControlsBaseUnselected");
	controls_frame.set_shadow_type (Gtk::SHADOW_OUT);

	ColorChanged.connect (mem_fun (*this, &TimeAxisView::color_handler));
}

TimeAxisView::~TimeAxisView()
{
	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		delete *i;
	}

	for (list<SelectionRect*>::iterator i = free_selection_rects.begin(); i != free_selection_rects.end(); ++i) {
		delete (*i)->rect;
		delete (*i)->start_trim;
		delete (*i)->end_trim;

	}

	for (list<SelectionRect*>::iterator i = used_selection_rects.begin(); i != used_selection_rects.end(); ++i) {
		delete (*i)->rect;
		delete (*i)->start_trim;
		delete (*i)->end_trim;
	}

	if (selection_group) {
		delete selection_group;
		selection_group = 0;
	}

	if (canvas_display) {
		delete canvas_display;
		canvas_display = 0;
	}

	if (display_menu) {
		delete display_menu;
		display_menu = 0;
	}
}

guint32
TimeAxisView::show_at (double y, int& nth, VBox *parent)
{
	gdouble ix1, ix2, iy1, iy2;
	effective_height = 0;
	
	if (control_parent) {
		control_parent->reorder_child (controls_frame, nth);
	} else {
		control_parent = parent;
		parent->pack_start (controls_frame, false, false);
		parent->reorder_child (controls_frame, nth);
	}
	controls_frame.show ();
	controls_ebox.show ();
	
	/* the coordinates used here are in the system of the
	   item's parent ...
	*/

	canvas_display->get_bounds (ix1, iy1, ix2, iy2);
	Group* pg = canvas_display->property_parent();
	pg->i2w (ix1, iy1);

	if (iy1 < 0) {
		iy1 = 0;
	}

	canvas_display->move (0.0, y - iy1);
	canvas_display->show();/* XXX not necessary */
	y_position = y;
	order = nth;
	_hidden = false;
	
	effective_height = height;

	/* now show children */
	
	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		
		if ((*i)->marked_for_display()) {
			(*i)->canvas_display->show();
		}
		
		if (canvas_item_visible ((*i)->canvas_display)) {
			++nth;
			effective_height += (*i)->show_at (y + 1 + effective_height, nth, parent);
		}
	}

	return effective_height;
}

bool
TimeAxisView::controls_ebox_scroll (GdkEventScroll* ev)
{
	switch (ev->direction) {
	case GDK_SCROLL_UP:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::Shift)) {
			step_height (false);
			return true;
		}
		break;
		
	case GDK_SCROLL_DOWN:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::Shift)) {
			step_height (true);
			return true;
		}
		break;

	default:
		/* no handling for left/right, yet */
		break;
	}

	return false;
}

bool
TimeAxisView::controls_ebox_button_release (GdkEventButton* ev)
{
	switch (ev->button) {
	case 1:
		selection_click (ev);
		break;

	case 3:
		popup_display_menu (ev->time);
		break;
	}

	return true;
}

void
TimeAxisView::selection_click (GdkEventButton* ev)
{
	Selection::Operation op = Keyboard::selection_type (ev->state);
	editor.set_selected_track (*this, op, false);
}

void
TimeAxisView::hide ()
{
	if (_hidden) {
		return;
	}

	canvas_display->hide();
	controls_frame.hide ();

	if (control_parent) {
		control_parent->remove (controls_frame);
		control_parent = 0;
	}

	y_position = -1;
	_hidden = true;
	
	/* now hide children */
	
	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->hide ();
	}
	
	Hiding ();
}

void
TimeAxisView::step_height (bool bigger)
{
  
       if (height == hLargest) {
               if (!bigger) set_height (Large);
               return;
       }
       if (height == hLarge) {
               if (bigger) set_height (Largest);
                else set_height (Larger);
               return;
       }
       if (height == hLarger) {
                if (bigger) set_height (Large);
                else set_height (Normal);
               return;
       }
       if (height == hNormal) {
                if (bigger) set_height (Larger);
                else set_height (Smaller);
               return;
       }
       if (height == hSmaller) {
                if (bigger) set_height (Normal);
                else set_height (Small);
               return;
       }
       if (height == hSmall) {
                if (bigger) set_height (Smaller);
               return;
       }
}

void
TimeAxisView::set_height (TrackHeight h)
{
	height_style = h;
	set_height_pixels (height_to_pixels (h));
}

void
TimeAxisView::set_height_pixels (uint32_t h)
{
	height = h;
	controls_frame.set_size_request (-1, height);

 	if (canvas_item_visible (selection_group)) {
		/* resize the selection rect */
		show_selection (editor.get_selection().time);
	}
}

bool
TimeAxisView::name_entry_key_release (GdkEventKey* ev)
{
	PublicEditor::TrackViewList *allviews = 0;
	PublicEditor::TrackViewList::iterator i;

	switch (ev->keyval) {
	case GDK_Escape:
		name_entry.select_region (0,0);
		controls_ebox.grab_focus ();
		name_entry_changed ();
		return true;

	/* Shift+Tab Keys Pressed. Note that for Shift+Tab, GDK actually
	 * generates a different ev->keyval, rather than setting 
	 * ev->state.
	 */
	case GDK_ISO_Left_Tab:
	case GDK_Tab:
		name_entry_changed ();
		allviews = editor.get_valid_views (0);
		if (allviews != 0) {
			i = find (allviews->begin(), allviews->end(), this);
			if (ev->keyval == GDK_Tab) {
				if (i != allviews->end()) {
					do {
						if (++i == allviews->end()) { return true; }
					} while((*i)->hidden());
				}
			} else {
				if (i != allviews->begin()) {
					do {
						if (--i == allviews->begin()) { return true; }
					} while ((*i)->hidden());
				}
			}

			if ((*i)->height_style == Small) {
				(*i)->set_height(Smaller);
			}
			
			(*i)->name_entry.grab_focus();
		}
		return true;

	case GDK_Up:
	case GDK_Down:
		name_entry_changed ();
		return true;

	default:
		break;
	}

#ifdef TIMEOUT_NAME_EDIT	
	/* adapt the timeout to reflect the user's typing speed */

	guint32 name_entry_timeout;

	if (last_name_entry_key_press_event) {
		/* timeout is 1/2 second or 5 times their current inter-char typing speed */
		name_entry_timeout = std::max (500U, (5 * (ev->time - last_name_entry_key_press_event)));
	} else {
		/* start with a 1 second timeout */
		name_entry_timeout = 1000;
	}

	last_name_entry_key_press_event = ev->time;

	/* wait 1 seconds and if no more keys are pressed, act as if they pressed enter */

	name_entry_key_timeout.disconnect();
	name_entry_key_timeout = Glib::signal_timeout().connect (mem_fun (*this, &TimeAxisView::name_entry_key_timed_out), name_entry_timeout);
#endif

	return false;
}

bool
TimeAxisView::name_entry_focus_in (GdkEventFocus* ev)
{
	name_entry.select_region (0, -1);
	name_entry.set_name ("EditorActiveTrackNameDisplay");
	return false;
}

bool
TimeAxisView::name_entry_focus_out (GdkEventFocus* ev)
{
	/* clean up */

	last_name_entry_key_press_event = 0;
	name_entry_key_timeout.disconnect ();
	name_entry.set_name ("EditorTrackNameDisplay");
	name_entry.select_region (0,0);
	
	/* do the real stuff */

	name_entry_changed ();

	return false;
}

bool
TimeAxisView::name_entry_key_timed_out ()
{
	name_entry_activated();
	return false;
}

void
TimeAxisView::name_entry_activated ()
{
	controls_ebox.grab_focus();
}

void
TimeAxisView::name_entry_changed ()
{
}

bool
TimeAxisView::name_entry_button_press (GdkEventButton *ev)
{
	if (ev->button == 3) {
		return true;
	}
	return false;
}

bool
TimeAxisView::name_entry_button_release (GdkEventButton *ev)
{
	if (ev->button == 3) {
		popup_display_menu (ev->time);
		return true;
	}
	return false;
}

void
TimeAxisView::popup_display_menu (guint32 when)
{
	if (display_menu == 0) {
		build_display_menu ();
	}
	editor.set_selected_track (*this, Selection::Add);
	display_menu->popup (1, when);	
}

gint
TimeAxisView::size_click (GdkEventButton *ev)
{
	editor.set_selected_track (*this, Selection::Add);
	popup_size_menu (ev->time);
	return TRUE;
}

void
TimeAxisView::popup_size_menu (guint32 when)
{
	if (size_menu == 0) {
		build_size_menu ();
	}
	size_menu->popup (1, when);
}

void
TimeAxisView::set_selected (bool yn)
{
	AxisView::set_selected (yn);

	if (_selected) {
		controls_ebox.set_name (controls_base_selected_name);
		controls_frame.set_name (controls_base_selected_name);

		/* propagate any existing selection, if the mode is right */

		if (editor.current_mouse_mode() == Editing::MouseRange && !editor.get_selection().time.empty()) {
			show_selection (editor.get_selection().time);
		}

	} else {
		controls_ebox.set_name (controls_base_unselected_name);
		controls_frame.set_name (controls_base_unselected_name);

		hide_selection ();

		/* children will be set for the yn=true case. but when deselecting
		   the editor only has a list of top-level trackviews, so we
		   have to do this here.
		*/

		for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
			(*i)->set_selected (false);
		}

		
	}
}

void
TimeAxisView::build_size_menu ()
{
	using namespace Menu_Helpers;

	size_menu = new Menu;
	size_menu->set_name ("ArdourContextMenu");
	MenuList& items = size_menu->items();
	
	items.push_back (MenuElem (_("Largest"), bind (mem_fun (*this, &TimeAxisView::set_height), Largest)));
	items.push_back (MenuElem (_("Large"), bind (mem_fun (*this, &TimeAxisView::set_height), Large)));
	items.push_back (MenuElem (_("Larger"), bind (mem_fun (*this, &TimeAxisView::set_height), Larger)));
	items.push_back (MenuElem (_("Normal"), bind (mem_fun (*this, &TimeAxisView::set_height), Normal)));
	items.push_back (MenuElem (_("Smaller"), bind (mem_fun (*this, &TimeAxisView::set_height),Smaller)));
	items.push_back (MenuElem (_("Small"), bind (mem_fun (*this, &TimeAxisView::set_height), Small)));
}

void
TimeAxisView::build_display_menu ()
{
	using namespace Menu_Helpers;

	display_menu = new Menu;
	display_menu->set_name ("ArdourContextMenu");

	// Just let implementing classes define what goes into the manu
}

void
TimeAxisView::set_samples_per_unit (double spu)
{
	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->set_samples_per_unit (spu);
	}
}

void
TimeAxisView::show_timestretch (nframes_t start, nframes_t end)
{
	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->show_timestretch (start, end);
	}
}

void
TimeAxisView::hide_timestretch ()
{
	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->hide_timestretch ();
	}
}

void
TimeAxisView::show_selection (TimeSelection& ts)
{
	double x1;
	double x2;
	double y2;
	SelectionRect *rect;

	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->show_selection (ts);
	}

	if (canvas_item_visible (selection_group)) {
		while (!used_selection_rects.empty()) {
			free_selection_rects.push_front (used_selection_rects.front());
			used_selection_rects.pop_front();
			free_selection_rects.front()->rect->hide();
			free_selection_rects.front()->start_trim->hide();
			free_selection_rects.front()->end_trim->hide();
		}
		selection_group->hide();
	}

	selection_group->show();
	selection_group->raise_to_top();
	
	for (list<AudioRange>::iterator i = ts.begin(); i != ts.end(); ++i) {
		nframes_t start, end, cnt;

		start = (*i).start;
		end = (*i).end;
		cnt = end - start + 1;

		rect = get_selection_rect ((*i).id);
		
		x1 = editor.frame_to_unit (start);
		x2 = editor.frame_to_unit (start + cnt - 1);
		y2 = height;

		rect->rect->property_x1() = x1;
		rect->rect->property_y1() = 1.0;
		rect->rect->property_x2() = x2;
		rect->rect->property_y2() = y2;
		
		// trim boxes are at the top for selections
		
		if (x2 > x1) {
			rect->start_trim->property_x1() = x1;
			rect->start_trim->property_y1() = 1.0;
			rect->start_trim->property_x2() = x1 + trim_handle_size;
			rect->start_trim->property_y2() = 1.0 + trim_handle_size;

			rect->end_trim->property_x1() = x2 - trim_handle_size;
			rect->end_trim->property_y1() = 1.0;
			rect->end_trim->property_x2() = x2;
			rect->end_trim->property_y2() = 1.0 + trim_handle_size;

			rect->start_trim->show();
			rect->end_trim->show();
		} else {
			rect->start_trim->hide();
			rect->end_trim->hide();
		}

		rect->rect->show ();
		used_selection_rects.push_back (rect);
	}
}

void
TimeAxisView::reshow_selection (TimeSelection& ts)
{
	show_selection (ts);

	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->show_selection (ts);
	}
}

void
TimeAxisView::hide_selection ()
{
	if (canvas_item_visible (selection_group)) {
		while (!used_selection_rects.empty()) {
			free_selection_rects.push_front (used_selection_rects.front());
			used_selection_rects.pop_front();
			free_selection_rects.front()->rect->hide();
			free_selection_rects.front()->start_trim->hide();
			free_selection_rects.front()->end_trim->hide();
		}
		selection_group->hide();
	}
	
	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->hide_selection ();
	}
}

void
TimeAxisView::order_selection_trims (ArdourCanvas::Item *item, bool put_start_on_top)
{
	/* find the selection rect this is for. we have the item corresponding to one
	   of the trim handles.
	 */

	for (list<SelectionRect*>::iterator i = used_selection_rects.begin(); i != used_selection_rects.end(); ++i) {
		if ((*i)->start_trim == item || (*i)->end_trim == item) {
			
			/* make one trim handle be "above" the other so that if they overlap,
			   the top one is the one last used.
			*/
			
			(*i)->rect->raise_to_top ();
			(put_start_on_top ? (*i)->start_trim : (*i)->end_trim)->raise_to_top ();
			(put_start_on_top ? (*i)->end_trim : (*i)->start_trim)->raise_to_top ();
			
			break;
		}
	}
}

SelectionRect *
TimeAxisView::get_selection_rect (uint32_t id)
{
	SelectionRect *rect;

	/* check to see if we already have a visible rect for this particular selection ID */

	for (list<SelectionRect*>::iterator i = used_selection_rects.begin(); i != used_selection_rects.end(); ++i) {
		if ((*i)->id == id) {
			return (*i);
		}
	}

	/* ditto for the free rect list */

	for (list<SelectionRect*>::iterator i = free_selection_rects.begin(); i != free_selection_rects.end(); ++i) {
		if ((*i)->id == id) {
			SelectionRect* ret = (*i);
			free_selection_rects.erase (i);
			return ret;
		}
	}

	/* no existing matching rect, so go get a new one from the free list, or create one if there are none */
	
	if (free_selection_rects.empty()) {

		rect = new SelectionRect;

		rect->rect = new SimpleRect (*selection_group);
		rect->rect->property_x1() = 0.0;
		rect->rect->property_y1() = 0.0;
		rect->rect->property_x2() = 0.0;
		rect->rect->property_y2() = 0.0;
		rect->rect->property_fill_color_rgba() = color_map[cSelectionRectFill];
		rect->rect->property_outline_color_rgba() = color_map[cSelectionRectOutline];
		
		rect->start_trim = new SimpleRect (*selection_group);
		rect->start_trim->property_x1() = 0.0;
		rect->start_trim->property_x2() = 0.0;
		rect->start_trim->property_fill_color_rgba() = color_map[cSelectionStartFill];
		rect->start_trim->property_outline_color_rgba() = color_map[cSelectionStartOutline];
		
		rect->end_trim = new SimpleRect (*selection_group);
		rect->end_trim->property_x1() = 0.0;
		rect->end_trim->property_x2() = 0.0;
		rect->end_trim->property_fill_color_rgba() = color_map[cSelectionEndFill];
		rect->end_trim->property_outline_color_rgba() = color_map[cSelectionEndOutline];

		free_selection_rects.push_front (rect);

		rect->rect->signal_event().connect (bind (mem_fun (editor, &PublicEditor::canvas_selection_rect_event), rect->rect, rect));
		rect->start_trim->signal_event().connect (bind (mem_fun (editor, &PublicEditor::canvas_selection_start_trim_event), rect->rect, rect));
		rect->end_trim->signal_event().connect (bind (mem_fun (editor, &PublicEditor::canvas_selection_end_trim_event), rect->rect, rect));
	} 

	rect = free_selection_rects.front();
	rect->id = id;
	free_selection_rects.pop_front();
	return rect;
}

bool
TimeAxisView::is_child (TimeAxisView* tav)
{
	return find (children.begin(), children.end(), tav) != children.end();
}

void
TimeAxisView::add_child (TimeAxisView* child)
{
	children.push_back (child);
}

void
TimeAxisView::remove_child (TimeAxisView* child)
{
	vector<TimeAxisView*>::iterator i;

	if ((i = find (children.begin(), children.end(), child)) != children.end()) {
		children.erase (i);
	}
}

void
TimeAxisView::get_selectables (nframes_t start, nframes_t end, double top, double bot, list<Selectable*>& result)
{
	return;
}

void
TimeAxisView::get_inverted_selectables (Selection& sel, list<Selectable*>& result)
{
	return;
}

bool
TimeAxisView::touched (double top, double bot)
{
	/* remember: this is X Window - coordinate space starts in upper left and moves down.
	   y_position is the "origin" or "top" of the track.
	 */

	double mybot = y_position + height; // XXX need to include Editor::track_spacing; 
	
	return ((y_position <= bot && y_position >= top) || 
		((mybot <= bot) && (top < mybot)) || 
		(mybot >= bot && y_position < top));
}		

void
TimeAxisView::set_parent (TimeAxisView& p)
{
	parent = &p;
}

bool
TimeAxisView::has_state () const
{
	return _has_state;
}

TimeAxisView*
TimeAxisView::get_parent_with_state ()
{
	if (parent == 0) {
		return 0;
	}

	if (parent->has_state()) {
		return parent;
	} 

	return parent->get_parent_with_state ();
}		

void
TimeAxisView::set_state (const XMLNode& node)
{
	const XMLProperty *prop;

	if ((prop = node.property ("track_height")) != 0) {

		if (prop->value() == "largest") {
			set_height (Largest);
		} else if (prop->value() == "large") {
			set_height (Large);
		} else if (prop->value() == "larger") {
			set_height (Larger);
		} else if (prop->value() == "normal") {
			set_height (Normal);
		} else if (prop->value() == "smaller") {
			set_height (Smaller);
		} else if (prop->value() == "small") {
			set_height (Small);
		} else {
			error << string_compose(_("unknown track height name \"%1\" in XML GUI information"), prop->value()) << endmsg;
			set_height (Normal);
		}

	} else {
		set_height (Normal);
	}
}

void
TimeAxisView::reset_height()
{
	set_height_pixels (height);

	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->set_height_pixels ((TrackHeight)(*i)->height);
	}
}
	
uint32_t
TimeAxisView::height_to_pixels (TrackHeight h)
{
	switch (h) {
	case Largest:
		return hLargest;
	case Large:
		return hLarge;
	case Larger:
		return hLarger;
	case Normal:
		return hNormal;
	case Smaller:
		return hSmaller;
	case Small:
		return hSmall;
	}
	
	// what is wrong with gcc ?
	
	return hNormal;
}
			
void
TimeAxisView::compute_controls_size_info ()
{
	Gtk::Window window (Gtk::WINDOW_TOPLEVEL);
	Gtk::Table two_row_table (2, 8);
	Gtk::Table one_row_table (1, 8);
	Button* buttons[5];
	const int border_width = 2;
	const int extra_height = (2 * border_width) + 2; // 2 pixels for the controls frame

	window.add (one_row_table);

	one_row_table.set_border_width (border_width);
	one_row_table.set_row_spacings (0);
	one_row_table.set_col_spacings (0);
	one_row_table.set_homogeneous (true);

	two_row_table.set_border_width (border_width);
	two_row_table.set_row_spacings (0);
	two_row_table.set_col_spacings (0);
	two_row_table.set_homogeneous (true);

	for (int i = 0; i < 5; ++i) {
		buttons[i] = manage (new Button (X_("f")));
		buttons[i]->set_name ("TrackMuteButton");
	}

	one_row_table.attach (*buttons[0], 6, 7, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);
	
	one_row_table.show_all ();
	Gtk::Requisition req(one_row_table.size_request ());


	// height required to show 1 row of buttons

	hSmaller = req.height + extra_height;

	window.remove ();
	window.add (two_row_table);

	two_row_table.attach (*buttons[1], 5, 6, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);
	two_row_table.attach (*buttons[2], 6, 7, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);
	two_row_table.attach (*buttons[3], 7, 8, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);
	two_row_table.attach (*buttons[4], 8, 9, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);

	two_row_table.show_all ();
	req = two_row_table.size_request ();

	// height required to show all normal buttons 

	hNormal = req.height + extra_height;

	// these heights are all just larger than normal. no more 
	// elements are visible (yet).

	hLarger = hNormal + 50;
	hLarge = hNormal + 150;
	hLargest = hNormal + 250;

	// height required to show track name

	hSmall = 27;
}

void
TimeAxisView::show_name_label ()
{
	if (!(name_packing & NameLabelPacked)) {
		name_hbox.pack_start (name_label, true, true);
		name_packing = NamePackingBits (name_packing | NameLabelPacked);
		name_hbox.show ();
		name_label.show ();
	}
}

void
TimeAxisView::show_name_entry ()
{
	if (!(name_packing & NameEntryPacked)) {
		name_hbox.pack_start (name_entry, true, true);
		name_packing = NamePackingBits (name_packing | NameEntryPacked);
		name_hbox.show ();
		name_entry.show ();
	}
}

void
TimeAxisView::hide_name_label ()
{
	if (name_packing & NameLabelPacked) {
		name_hbox.remove (name_label);
		name_packing = NamePackingBits (name_packing & ~NameLabelPacked);
	}
}

void
TimeAxisView::hide_name_entry ()
{
	if (name_packing & NameEntryPacked) {
		name_hbox.remove (name_entry);
		name_packing = NamePackingBits (name_packing & ~NameEntryPacked);
	}
}

void
TimeAxisView::color_handler (ColorID id, uint32_t val)
{
	switch (id) {
	case cSelectionRectFill:
		break;
	case cSelectionRectOutline:
		break;
	case cSelectionStartFill:
		break;
	case cSelectionStartOutline:
		break;
	case cSelectionEndFill:
		break;
	case cSelectionEndOutline:
		break;
	default:
		break;
	}
}

TimeAxisView*
TimeAxisView::covers_y_position (double y)
{
	if (hidden()) {
		return 0;
	}

	if (y_position <= y && y < (y_position + height)) {
		return this;
	}

	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		TimeAxisView* tv;

		if ((tv = (*i)->covers_y_position (y)) != 0) {
			return tv;
		}
	}

	return 0;
}
