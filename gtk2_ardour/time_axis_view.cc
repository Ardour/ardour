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

    $Id$
*/

#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <string>
#include <list>

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
#include "canvas-simplerect.h"
#include "selection.h"
#include "keyboard.h"
#include "rgb_macros.h"

#include "i18n.h"

using namespace Gtk;
using namespace sigc;
using namespace ARDOUR;
using namespace Editing;

const double trim_handle_size = 6.0; /* pixels */

TimeAxisView::TimeAxisView(ARDOUR::Session& sess, PublicEditor& ed, TimeAxisView* rent, Gtk::Widget *canvas) 
	: AxisView(sess), 
	  editor(ed),
	  controls_table (2, 9)
{
	canvas_display = gnome_canvas_item_new (gnome_canvas_root(GNOME_CANVAS(canvas->gobj())),
					      gnome_canvas_group_get_type(),
					      "x", 0.0,
					      "y", 0.0,
					      NULL);

	selection_group = gnome_canvas_item_new (GNOME_CANVAS_GROUP(canvas_display), 
					       gnome_canvas_group_get_type (), 
					       NULL);
	gnome_canvas_item_hide (selection_group);
	
   	control_parent = 0;
	display_menu = 0;
	size_menu = 0;
	_marked_for_display = false;
	_hidden = false;
	height = 0;
	effective_height = 0;
	parent = rent;
	_has_state = false;

	/*
	  Create the standard LHS Controls
	  We create the top-level container and name add the name label here,
	  subclasses can add to the layout as required
	*/

	name_entry.set_name ("EditorTrackNameDisplay");
	name_entry.signal_button_release_event().connect (mem_fun (*this, &TimeAxisView::name_entry_button_release));
	name_entry.signal_button_press_event().connect (mem_fun (*this, &TimeAxisView::name_entry_button_press));
	
	name_entry.signal_focus_in_event()().connect (ptr_fun (ARDOUR_UI::generic_focus_in_event));
	name_entry.signal_focus_out_event()().connect (ptr_fun (ARDOUR_UI::generic_focus_out_event));

	Gtkmm2ext::set_size_request_to_display_given_text (name_entry, N_("gTortnam"), 10, 10); // just represents a short name

	name_label.set_name ("TrackLabel");
	name_label.set_alignment (0.0, 0.5);

	// name_hbox.set_border_width (2);
	// name_hbox.set_spacing (5);

	/* typically, either name_label OR name_entry are visible,
	   but not both. its up to derived classes to show/hide them as they
	   wish.
	*/

	name_hbox.pack_start (name_label, true, true);
	name_hbox.pack_start (name_entry, true, true);
	name_hbox.show ();

	controls_table.set_border_width (2);
	controls_table.set_row_spacings (0);
	controls_table.set_col_spacings (0);
	controls_table.set_homogeneous (true);
	controls_table.show ();

	controls_table.attach (name_hbox, 0, 5, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);

	controls_table.show ();

	controls_vbox.pack_start (controls_table, false, false);
	controls_vbox.show ();

	controls_ebox.set_name ("TimeAxisViewControlsBaseUnselected");
	controls_ebox.add (controls_vbox);
	controls_ebox.signal_add_event()s (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	controls_ebox.set_flags (Gtk::CAN_FOCUS);

	controls_ebox.signal_button_release_event().connect (mem_fun (*this, &TimeAxisView::controls_ebox_button_release));
	
	controls_lhs_pad.set_name ("TimeAxisViewControlsPadding");
	controls_hbox.pack_start (controls_lhs_pad,false,false);
	controls_hbox.pack_start (controls_ebox,true,true);
	controls_hbox.show ();

	controls_frame.add (controls_hbox);
	controls_frame.set_name ("TimeAxisViewControlsBaseUnselected");
	controls_frame.set_shadow_type (Gtk::SHADOW_OUT);
}

TimeAxisView::~TimeAxisView()
{
	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		delete *i;
	}

	for (list<SelectionRect*>::iterator i = free_selection_rects.begin(); i != free_selection_rects.end(); ++i) {
		gtk_object_destroy (GTK_OBJECT((*i)->rect));
		gtk_object_destroy (GTK_OBJECT((*i)->start_trim));
		gtk_object_destroy (GTK_OBJECT((*i)->end_trim));
	}

	for (list<SelectionRect*>::iterator i = used_selection_rects.begin(); i != used_selection_rects.end(); ++i) {
		gtk_object_destroy (GTK_OBJECT((*i)->rect));
		gtk_object_destroy (GTK_OBJECT((*i)->start_trim));
		gtk_object_destroy (GTK_OBJECT((*i)->end_trim));
	}

	if (selection_group) {
		gtk_object_destroy (GTK_OBJECT (selection_group));
		selection_group = 0;
	}

	if (canvas_display) {
		gtk_object_destroy (GTK_OBJECT (canvas_display));	
		canvas_display = 0;
	}

	if (display_menu) {
		delete display_menu;
		display_menu = 0;
	}

	if (size_menu) {
		delete size_menu;
		size_menu = 0;
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

	gnome_canvas_item_get_bounds (canvas_display, &ix1, &iy1, &ix2, &iy2);
	gnome_canvas_item_i2w (canvas_display->parent, &ix1, &iy1);
	if (iy1 < 0) {
		iy1 = 0;
	}
	gnome_canvas_item_move (canvas_display, 0.0, y - iy1);
	gnome_canvas_item_show (canvas_display); /* XXX not necessary */

	y_position = y;
	order = nth;
	_hidden = false;
	
	effective_height = height;

	/* now show children */
	
	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		
		if ((*i)->marked_for_display()) {
			gnome_canvas_item_show ((*i)->canvas_display);
		}
		
		if (GTK_OBJECT_FLAGS(GTK_OBJECT((*i)->canvas_display)) & GNOME_CANVAS_ITEM_VISIBLE) {
			++nth;
			effective_height += (*i)->show_at (y + effective_height, nth, parent);
		}
	}

	return effective_height;
}

gint
TimeAxisView::controls_ebox_button_release (GdkEventButton* ev)
{
	switch (ev->button) {
	case 1:
		selection_click (ev);
		break;

	case 3:
		popup_display_menu (ev->time);
		break;

	case 4:
		if (Keyboard::no_modifier_keys_pressed (ev)) {
			editor.scroll_tracks_up_line ();
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::Control)) {
			step_height (true);
		}
		break;

	case 5:
		if (Keyboard::no_modifier_keys_pressed (ev)) {
			editor.scroll_tracks_down_line ();
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::Control)) {
			step_height (false);
		}
		break;
		

	}

	return TRUE;
}

void
TimeAxisView::selection_click (GdkEventButton* ev)
{
	if (Keyboard::modifier_state_contains (ev->state, Keyboard::Shift)) {

		if (editor.get_selection().selected (this)) {
			editor.get_selection().remove (this);
		} else {
			editor.get_selection().add (this);
		}

	} else {

		editor.get_selection().set (this);
	}
}

void
TimeAxisView::hide ()
{
	if (_hidden) {
		return;
	}

	gnome_canvas_item_hide (canvas_display);
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
	switch (height) {
	case Largest:
		if (!bigger) set_height (Large);
		break;
	case Large:
		if (bigger) set_height (Largest);
		else set_height (Larger);
		break;
	case Larger:
		if (bigger) set_height (Large);
		else set_height (Normal);
		break;
	case Normal:
		if (bigger) set_height (Larger);
		else set_height (Smaller);
		break;
	case Smaller:
		if (bigger) set_height (Normal);
		else set_height (Small);
		break;
	case Small:
		if (bigger) set_height (Smaller);
		break;
	}
}


void
TimeAxisView::set_height (TrackHeight h)
{
	height = (guint32) h;
	controls_frame.set_size_request (-1, height);

 	if (GTK_OBJECT_FLAGS(GTK_OBJECT(selection_group)) & GNOME_CANVAS_ITEM_VISIBLE) {
		/* resize the selection rect */
		show_selection (editor.get_selection().time);
	}

//	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
//		(*i)->set_height (h);
//	}

}


gint
TimeAxisView::name_entry_button_press (GdkEventButton *ev)
{
	if (ev->button == 3) {
		return do_not_propagate (ev);
	}
	return FALSE;
}

gint
TimeAxisView::name_entry_button_release (GdkEventButton *ev)
{
	if (ev->button == 3) {
		popup_display_menu (ev->time);
		return stop_signal (name_entry, "button_release_event");
	}
	return FALSE;
}

void
TimeAxisView::popup_display_menu (guint32 when)
{
	if (display_menu == 0) {
		build_display_menu ();
	}
	display_menu->popup (1, when);	
}

gint
TimeAxisView::size_click (GdkEventButton *ev)
{
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
	items.push_back (MenuElem (_("Smaller"), bind (mem_fun (*this, &TimeAxisView::set_height), Smaller)));
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
TimeAxisView::show_timestretch (jack_nframes_t start, jack_nframes_t end)
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

	if (GTK_OBJECT_FLAGS(GTK_OBJECT(selection_group)) & GNOME_CANVAS_ITEM_VISIBLE) {
		while (!used_selection_rects.empty()) {
			free_selection_rects.push_front (used_selection_rects.front());
			used_selection_rects.pop_front();
			gnome_canvas_item_hide (free_selection_rects.front()->rect);
			gnome_canvas_item_hide (free_selection_rects.front()->start_trim);
			gnome_canvas_item_hide (free_selection_rects.front()->end_trim);
		}
		gnome_canvas_item_hide (selection_group);
	}

	gnome_canvas_item_show (selection_group);
	gnome_canvas_item_raise_to_top (selection_group);
	
	for (list<AudioRange>::iterator i = ts.begin(); i != ts.end(); ++i) {
		jack_nframes_t start, end, cnt;

		start = (*i).start;
		end = (*i).end;
		cnt = end - start + 1;

		rect = get_selection_rect ((*i).id);
		
		x1 = start / editor.get_current_zoom();
		x2 = (start + cnt - 1) / editor.get_current_zoom();
		y2 = height;

		gtk_object_set (GTK_OBJECT(rect->rect), 
				"x1", x1,
				"y1", 1.0,
				"x2", x2,
				"y2", y2,
				NULL);
		
		// trim boxes are at the top for selections
		
		if (x2 > x1) {
			gtk_object_set (GTK_OBJECT(rect->start_trim), 
					"x1", x1,
					"y1", 1.0,
					"x2", x1 + trim_handle_size,
					"y2", 1.0 + trim_handle_size,
					NULL);
			gtk_object_set (GTK_OBJECT(rect->end_trim), 
					"x1", x2 - trim_handle_size,
					"y1", 1.0,
					"x2", x2,
					"y2", 1.0 + trim_handle_size,
					NULL);
			gnome_canvas_item_show (rect->start_trim);
			gnome_canvas_item_show (rect->end_trim);
		} else {
			gnome_canvas_item_hide (rect->start_trim);
			gnome_canvas_item_hide (rect->end_trim);
		}

		gnome_canvas_item_show (rect->rect);
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
	if (GTK_OBJECT_FLAGS(GTK_OBJECT(selection_group)) & GNOME_CANVAS_ITEM_VISIBLE) {
		while (!used_selection_rects.empty()) {
			free_selection_rects.push_front (used_selection_rects.front());
			used_selection_rects.pop_front();
			gnome_canvas_item_hide (free_selection_rects.front()->rect);
			gnome_canvas_item_hide (free_selection_rects.front()->start_trim);
			gnome_canvas_item_hide (free_selection_rects.front()->end_trim);
		}
		gnome_canvas_item_hide (selection_group);
	}
	
	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->hide_selection ();
	}
}

void
TimeAxisView::order_selection_trims (GnomeCanvasItem *item, bool put_start_on_top)
{
	/* find the selection rect this is for. we have the item corresponding to one
	   of the trim handles.
	 */

	for (list<SelectionRect*>::iterator i = used_selection_rects.begin(); i != used_selection_rects.end(); ++i) {
		if ((*i)->start_trim == item || (*i)->end_trim == item) {

			/* make one trim handle be "above" the other so that if they overlap,
			   the top one is the one last used.
			*/

			gnome_canvas_item_raise_to_top ((*i)->rect);
			gnome_canvas_item_raise_to_top (put_start_on_top ? (*i)->start_trim : (*i)->end_trim);
			gnome_canvas_item_raise_to_top (put_start_on_top ? (*i)->end_trim : (*i)->start_trim);

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
			free_selection_rects.erase (i);
			return (*i);
		}
	}

	/* no existing matching rect, so go get a new one from the free list, or create one if there are none */
	
	if (free_selection_rects.empty()) {

		rect = new SelectionRect;

		rect->rect = gnome_canvas_item_new (GNOME_CANVAS_GROUP(selection_group),
						  gnome_canvas_simplerect_get_type(),
						  "x1", 0.0,
						  "y1", 0.0,
						  "x2", 0.0,
						  "y2", 0.0,
						  "fill_color_rgba", color_map[cSelectionRectFill],
						  "outline_color_rgba" , color_map[cSelectionRectOutline],
						  NULL);
		
		
		rect->start_trim = gnome_canvas_item_new (GNOME_CANVAS_GROUP(selection_group),
							    gnome_canvas_simplerect_get_type(),
							    "x1", (gdouble) 0.0,
							    "x2", (gdouble) 0.0,
							    "fill_color_rgba" , color_map[cSelectionStartFill],
							    "outline_color_rgba" , color_map[cSelectionStartOutline],
							    NULL);
		
		rect->end_trim = gnome_canvas_item_new (GNOME_CANVAS_GROUP(selection_group),
							  gnome_canvas_simplerect_get_type(),
							  "x1", 0.0,
							  "x2", 0.0,
							  "fill_color_rgba" , color_map[cSelectionEndFill],
							  "outline_color_rgba" , color_map[cSelectionEndOutline],
							  NULL);
		free_selection_rects.push_front (rect);

		gtk_signal_connect (GTK_OBJECT(rect->rect), "event",
				    (GtkSignalFunc) PublicEditor::canvas_selection_rect_event,
				    &editor);
		gtk_signal_connect (GTK_OBJECT(rect->start_trim), "event",
				    (GtkSignalFunc) PublicEditor::canvas_selection_start_trim_event,
				    &editor);
		gtk_signal_connect (GTK_OBJECT(rect->end_trim), "event",
				    (GtkSignalFunc) PublicEditor::canvas_selection_end_trim_event,
				    &editor);

		gtk_object_set_data(GTK_OBJECT(rect->rect), "rect", rect);
		gtk_object_set_data(GTK_OBJECT(rect->start_trim), "rect", rect);
		gtk_object_set_data(GTK_OBJECT(rect->end_trim), "rect", rect);
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
TimeAxisView::get_selectables (jack_nframes_t start, jack_nframes_t end, double top, double bot, list<Selectable*>& result)
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
			error << compose(_("unknown track height name \"%1\" in XML GUI information"), prop->value()) << endmsg;
			set_height (Normal);
		}

	} else {
		set_height (Normal);
	}
}

void
TimeAxisView::reset_height()
{
	set_height ((TrackHeight) height);

	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->set_height ((TrackHeight)(*i)->height);
	}
}
	
