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

#include <cstdio> // for sprintf, grrr 
#include <cmath>

#include <string>

#include <ardour/tempo.h>
#include <gtkmm2ext/gtk_ui.h>

#include "editor.h"
#include "editing.h"
#include "gtk-custom-hruler.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace Gtk;
using namespace Editing;

Editor *Editor::ruler_editor;

/* the order here must match the "metric" enums in editor.h */

GtkCustomMetric Editor::ruler_metrics[4] = {
	{1, Editor::_metric_get_smpte },
	{1, Editor::_metric_get_bbt },
	{1, Editor::_metric_get_frames },
	{1, Editor::_metric_get_minsec }
};

void
Editor::initialize_rulers ()
{
	ruler_editor = this;
	ruler_grabbed_widget = 0;
	
	_smpte_ruler = gtk_custom_hruler_new ();
	smpte_ruler = Glib::wrap (_smpte_ruler);
	smpte_ruler->set_name ("SMPTERuler");
	smpte_ruler->set_size_request (-1, (int)timebar_height);
	gtk_custom_ruler_set_metric (GTK_CUSTOM_RULER(_smpte_ruler), &ruler_metrics[ruler_metric_smpte]);
	ruler_shown[ruler_metric_smpte] = true;
	
	_bbt_ruler = gtk_custom_hruler_new ();
	bbt_ruler = Glib::wrap (_bbt_ruler);
	bbt_ruler->set_name ("BBTRuler");
	bbt_ruler->set_size_request (-1, (int)timebar_height);
	gtk_custom_ruler_set_metric (GTK_CUSTOM_RULER(_bbt_ruler), &ruler_metrics[ruler_metric_bbt]);
	ruler_shown[ruler_metric_bbt] = true;

	_frames_ruler = gtk_custom_hruler_new ();
	frames_ruler = Glib::wrap (_frames_ruler);
	frames_ruler->set_name ("FramesRuler");
	frames_ruler->set_size_request (-1, (int)timebar_height);
	gtk_custom_ruler_set_metric (GTK_CUSTOM_RULER(_frames_ruler), &ruler_metrics[ruler_metric_frames]);

	_minsec_ruler = gtk_custom_hruler_new ();
	minsec_ruler = Glib::wrap (_minsec_ruler);
	minsec_ruler->set_name ("MinSecRuler");
	minsec_ruler->set_size_request (-1, (int)timebar_height);
	gtk_custom_ruler_set_metric (GTK_CUSTOM_RULER(_minsec_ruler), &ruler_metrics[ruler_metric_minsec]);

	ruler_shown[ruler_time_meter] = true;
	ruler_shown[ruler_time_tempo] = true;
	ruler_shown[ruler_time_marker] = true;
	ruler_shown[ruler_time_range_marker] = true;
	ruler_shown[ruler_time_transport_marker] = true;
	ruler_shown[ruler_metric_frames] = false;
	ruler_shown[ruler_metric_minsec] = false;
	
	smpte_ruler->set_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	bbt_ruler->set_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	frames_ruler->set_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	minsec_ruler->set_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);

	smpte_ruler->signal_button_release_event().connect (mem_fun(*this, &Editor::ruler_button_release));
	bbt_ruler->signal_button_release_event().connect (mem_fun(*this, &Editor::ruler_button_release));
	frames_ruler->signal_button_release_event().connect (mem_fun(*this, &Editor::ruler_button_release));
	minsec_ruler->signal_button_release_event().connect (mem_fun(*this, &Editor::ruler_button_release));

	smpte_ruler->signal_button_press_event().connect (mem_fun(*this, &Editor::ruler_button_press));
	bbt_ruler->signal_button_press_event().connect (mem_fun(*this, &Editor::ruler_button_press));
	frames_ruler->signal_button_press_event().connect (mem_fun(*this, &Editor::ruler_button_press));
	minsec_ruler->signal_button_press_event().connect (mem_fun(*this, &Editor::ruler_button_press));
	
	smpte_ruler->signal_motion_notify_event().connect (mem_fun(*this, &Editor::ruler_mouse_motion));
	bbt_ruler->signal_motion_notify_event().connect (mem_fun(*this, &Editor::ruler_mouse_motion));
	frames_ruler->signal_motion_notify_event().connect (mem_fun(*this, &Editor::ruler_mouse_motion));
	minsec_ruler->signal_motion_notify_event().connect (mem_fun(*this, &Editor::ruler_mouse_motion));
	
	visible_timebars = 7; /* 4 here, 3 in time_canvas */
	ruler_pressed_button = 0;
}


gint
Editor::ruler_button_press (GdkEventButton* ev)
{
	if (session == 0) {
		return FALSE;
	}

	ruler_pressed_button = ev->button;

	// jlc: grab ev->window ?
	//Gtk::Main::grab_add (*minsec_ruler);
	Widget * grab_widget = 0;

	if (smpte_ruler->is_realized() && ev->window == smpte_ruler->get_window()->gobj()) grab_widget = smpte_ruler;
	else if (bbt_ruler->is_realized() && ev->window == bbt_ruler->get_window()->gobj()) grab_widget = bbt_ruler;
	else if (frames_ruler->is_realized() && ev->window == frames_ruler->get_window()->gobj()) grab_widget = frames_ruler;
	else if (minsec_ruler->is_realized() && ev->window == minsec_ruler->get_window()->gobj()) grab_widget = minsec_ruler;

	if (grab_widget) {
		Gtk::Main::grab_add (*grab_widget);
		ruler_grabbed_widget = grab_widget;
	}

	return TRUE;
}

gint
Editor::ruler_button_release (GdkEventButton* ev)
{
	gint x,y;
	Gdk::ModifierType state;

	/* need to use the correct x,y, the event lies */
	time_canvas_event_box.get_window()->get_pointer (x, y, state);


	ruler_pressed_button = 0;
	
	if (session == 0) {
		return FALSE;
	}

	hide_verbose_canvas_cursor();
	stop_canvas_autoscroll();
	
	jack_nframes_t where = leftmost_frame + pixel_to_frame (x);

	switch (ev->button) {
	case 1:
		/* transport playhead */
		snap_to (where);
		session->request_locate (where);
		break;

	case 2:
		/* edit cursor */
		if (snap_type != Editing::SnapToEditCursor) {
			snap_to (where);
		}
		edit_cursor->set_position (where);
		edit_cursor_clock.set (where);
		break;

	case 3:
		/* popup menu */
		snap_to (where);
		popup_ruler_menu (where);
		
		break;
	default:
		break;
	}


	if (ruler_grabbed_widget) {
		Gtk::Main::grab_remove (*ruler_grabbed_widget);
		ruler_grabbed_widget = 0;
	}

	return TRUE;
}

gint
Editor::ruler_label_button_release (GdkEventButton* ev)
{
	if (ev->button == 3)
	{
		popup_ruler_menu();
	}
	
	return TRUE;
}


gint
Editor::ruler_mouse_motion (GdkEventMotion* ev)
{
	if (session == 0 || !ruler_pressed_button) {
		return FALSE;
	}

	double wcx=0,wcy=0;
	double cx=0,cy=0;

	gint x,y;
	Gdk::ModifierType state;

	/* need to use the correct x,y, the event lies */
	time_canvas_event_box.get_window()->get_pointer (x, y, state);

	
	track_canvas.c2w (x, y, wcx, wcy);
	track_canvas.w2c (wcx, wcy, cx, cy);
	
	jack_nframes_t where = leftmost_frame + pixel_to_frame (x);

	/// ripped from maybe_autoscroll
	jack_nframes_t one_page = (jack_nframes_t) rint (canvas_width * frames_per_unit);
	jack_nframes_t rightmost_frame = leftmost_frame + one_page;

	jack_nframes_t frame = pixel_to_frame (cx);

	if (autoscroll_timeout_tag < 0) {
		if (frame > rightmost_frame) {
			if (rightmost_frame < max_frames) {
				start_canvas_autoscroll (1);
			}
		} else if (frame < leftmost_frame) {
			if (leftmost_frame > 0) {
				start_canvas_autoscroll (-1);
			}
		} 
	} else {
		if (frame >= leftmost_frame && frame < rightmost_frame) {
			stop_canvas_autoscroll ();
		}
	}
	//////	
	
	snap_to (where);

	Cursor* cursor = 0;
	
	switch (ruler_pressed_button) {
	case 1:
		/* transport playhead */
		cursor = playhead_cursor;
		break;

	case 2:
		/* edit cursor */
		cursor = edit_cursor;
		break;

	default:
		break;
	}

	if (cursor)
	{
		cursor->set_position (where);
		
		if (cursor == edit_cursor) {
			edit_cursor_clock.set (where);
		}
		
		show_verbose_time_cursor (where, 10, cx, 0);
	}
	
	return TRUE;
}


void
Editor::popup_ruler_menu (jack_nframes_t where, ItemType t)
{
	using namespace Menu_Helpers;

	if (editor_ruler_menu == 0) {
		editor_ruler_menu = new Menu;
		editor_ruler_menu->set_name ("ArdourContextMenu");
	}

	// always build from scratch
	MenuList& ruler_items = editor_ruler_menu->items();
	editor_ruler_menu->set_name ("ArdourContextMenu");
	ruler_items.clear();

	CheckMenuItem * mitem;

	no_ruler_shown_update = true;

	switch (t) {
	case MarkerBarItem:
		ruler_items.push_back (MenuElem (_("New location marker"), bind ( mem_fun(*this, &Editor::mouse_add_new_marker), where)));
		ruler_items.push_back (MenuElem (_("Clear all locations"), mem_fun(*this, &Editor::clear_markers)));
		ruler_items.push_back (SeparatorElem ());
		break;
	case RangeMarkerBarItem:
		//ruler_items.push_back (MenuElem (_("New Range")));
		ruler_items.push_back (MenuElem (_("Clear all ranges"), mem_fun(*this, &Editor::clear_ranges)));
		ruler_items.push_back (SeparatorElem ());

		break;
	case TransportMarkerBarItem:

		break;
		
	case TempoBarItem:
		ruler_items.push_back (MenuElem (_("New Tempo"), bind ( mem_fun(*this, &Editor::mouse_add_new_tempo_event), where)));
		ruler_items.push_back (MenuElem (_("Clear tempo")));
		ruler_items.push_back (SeparatorElem ());
		break;

	case MeterBarItem:
		ruler_items.push_back (MenuElem (_("New Meter"), bind ( mem_fun(*this, &Editor::mouse_add_new_meter_event), where)));
		ruler_items.push_back (MenuElem (_("Clear meter")));
		ruler_items.push_back (SeparatorElem ());
		break;

	default:
		break;
	}
	
	ruler_items.push_back (CheckMenuElem (_("Min:Secs"), bind (mem_fun(*this, &Editor::ruler_toggled), (int)ruler_metric_minsec)));
	mitem = (CheckMenuItem *) &ruler_items.back(); 
	if (ruler_shown[ruler_metric_minsec]) {
		mitem->set_active(true);
	}

	ruler_items.push_back (CheckMenuElem (X_("SMPTE"), bind (mem_fun(*this, &Editor::ruler_toggled), (int)ruler_metric_smpte)));
	mitem = (CheckMenuItem *) &ruler_items.back(); 
	if (ruler_shown[ruler_metric_smpte]) {
		mitem->set_active(true);
	}

	ruler_items.push_back (CheckMenuElem (_("Frames"), bind (mem_fun(*this, &Editor::ruler_toggled), (int)ruler_metric_frames)));
	mitem = (CheckMenuItem *) &ruler_items.back(); 
	if (ruler_shown[ruler_metric_frames]) {
		mitem->set_active(true);
	}

	ruler_items.push_back (CheckMenuElem (_("Bars:Beats"), bind (mem_fun(*this, &Editor::ruler_toggled), (int)ruler_metric_bbt)));
	mitem = (CheckMenuItem *) &ruler_items.back(); 
	if (ruler_shown[ruler_metric_bbt]) {
		mitem->set_active(true);
	}

	ruler_items.push_back (SeparatorElem ());

	ruler_items.push_back (CheckMenuElem (_("Meter"), bind (mem_fun(*this, &Editor::ruler_toggled), (int)ruler_time_meter)));
	mitem = (CheckMenuItem *) &ruler_items.back(); 
	if (ruler_shown[ruler_time_meter]) {
		mitem->set_active(true);
	}

	ruler_items.push_back (CheckMenuElem (_("Tempo"), bind (mem_fun(*this, &Editor::ruler_toggled), (int)ruler_time_tempo)));
	mitem = (CheckMenuItem *) &ruler_items.back(); 
	if (ruler_shown[ruler_time_tempo]) {
		mitem->set_active(true);
	}

	ruler_items.push_back (CheckMenuElem (_("Location Markers"), bind (mem_fun(*this, &Editor::ruler_toggled), (int)ruler_time_marker)));
	mitem = (CheckMenuItem *) &ruler_items.back(); 
	if (ruler_shown[ruler_time_marker]) {
		mitem->set_active(true);
	}

 	ruler_items.push_back (CheckMenuElem (_("Range Markers"), bind (mem_fun(*this, &Editor::ruler_toggled), (int)ruler_time_range_marker)));
 	mitem = (CheckMenuItem *) &ruler_items.back(); 
 	if (ruler_shown[ruler_time_range_marker]) {
 		mitem->set_active(true);
 	}

 	ruler_items.push_back (CheckMenuElem (_("Loop/Punch Ranges"), bind (mem_fun(*this, &Editor::ruler_toggled), (int)ruler_time_transport_marker)));
 	mitem = (CheckMenuItem *) &ruler_items.back(); 
 	if (ruler_shown[ruler_time_transport_marker]) {
 		mitem->set_active(true);
 	}
	
        editor_ruler_menu->popup (1, 0);

	no_ruler_shown_update = false;
}

void
Editor::ruler_toggled (int ruler)
{
	if (!session) return;
	if (ruler < 0 || ruler >= (int) sizeof(ruler_shown)) return;

	if (no_ruler_shown_update) return;

	if (ruler_shown[ruler]) {
		if (visible_timebars <= 1) {
			// must always have 1 visible
			return;
		}
	}
	
	ruler_shown[ruler] = !ruler_shown[ruler];

	update_ruler_visibility ();

	// update session extra RulerVisibility
	store_ruler_visibility ();
}

void
Editor::store_ruler_visibility ()
{
	XMLNode* node = new XMLNode(X_("RulerVisibility"));

	node->add_property (X_("smpte"), ruler_shown[ruler_metric_smpte] ? "yes": "no");
	node->add_property (X_("bbt"), ruler_shown[ruler_metric_bbt] ? "yes": "no");
	node->add_property (X_("frames"), ruler_shown[ruler_metric_frames] ? "yes": "no");
	node->add_property (X_("minsec"), ruler_shown[ruler_metric_minsec] ? "yes": "no");
	node->add_property (X_("tempo"), ruler_shown[ruler_time_tempo] ? "yes": "no");
	node->add_property (X_("meter"), ruler_shown[ruler_time_meter] ? "yes": "no");
	node->add_property (X_("marker"), ruler_shown[ruler_time_marker] ? "yes": "no");
	node->add_property (X_("rangemarker"), ruler_shown[ruler_time_range_marker] ? "yes": "no");
	node->add_property (X_("transportmarker"), ruler_shown[ruler_time_transport_marker] ? "yes": "no");

	session->add_extra_xml (*node);
	session->set_dirty ();
}
 
void 
Editor::restore_ruler_visibility ()
{
	XMLProperty* prop;
	XMLNode * node = session->extra_xml (X_("RulerVisibility"));

	if (node) {
		if ((prop = node->property ("smpte")) != 0) {
			if (prop->value() == "yes") 
				ruler_shown[ruler_metric_smpte] = true;
			else 
				ruler_shown[ruler_metric_smpte] = false;
		}
		if ((prop = node->property ("bbt")) != 0) {
			if (prop->value() == "yes") 
				ruler_shown[ruler_metric_bbt] = true;
			else 
				ruler_shown[ruler_metric_bbt] = false;
		}
		if ((prop = node->property ("frames")) != 0) {
			if (prop->value() == "yes") 
				ruler_shown[ruler_metric_frames] = true;
			else 
				ruler_shown[ruler_metric_frames] = false;
		}
		if ((prop = node->property ("minsec")) != 0) {
			if (prop->value() == "yes") 
				ruler_shown[ruler_metric_minsec] = true;
			else 
				ruler_shown[ruler_metric_minsec] = false;
		}
		if ((prop = node->property ("tempo")) != 0) {
			if (prop->value() == "yes") 
				ruler_shown[ruler_time_tempo] = true;
			else 
				ruler_shown[ruler_time_tempo] = false;
		}
		if ((prop = node->property ("meter")) != 0) {
			if (prop->value() == "yes") 
				ruler_shown[ruler_time_meter] = true;
			else 
				ruler_shown[ruler_time_meter] = false;
		}
		if ((prop = node->property ("marker")) != 0) {
			if (prop->value() == "yes") 
				ruler_shown[ruler_time_marker] = true;
			else 
				ruler_shown[ruler_time_marker] = false;
		}
		if ((prop = node->property ("rangemarker")) != 0) {
			if (prop->value() == "yes") 
				ruler_shown[ruler_time_range_marker] = true;
			else 
				ruler_shown[ruler_time_range_marker] = false;
		}
		if ((prop = node->property ("transportmarker")) != 0) {
			if (prop->value() == "yes") 
				ruler_shown[ruler_time_transport_marker] = true;
			else 
				ruler_shown[ruler_time_transport_marker] = false;
		}

	}

	update_ruler_visibility ();
}


void
Editor::update_ruler_visibility ()
{
	using namespace Box_Helpers;
	BoxList & lab_children =  time_button_vbox.children();
	BoxList & ruler_children =  time_canvas_vbox.children();

	visible_timebars = 0;

	lab_children.clear();

	// leave the last one (the time_canvas_scroller) intact
	while (ruler_children.size() > 1) {
		ruler_children.pop_front();
	}

	BoxList::iterator canvaspos = ruler_children.begin();
	
	_smpte_ruler = gtk_custom_hruler_new ();
	smpte_ruler = Glib::wrap (_smpte_ruler);
	smpte_ruler->set_name ("SMPTERuler");
	smpte_ruler->set_size_request (-1, (int)timebar_height);
	gtk_custom_ruler_set_metric (GTK_CUSTOM_RULER(_smpte_ruler), &ruler_metrics[ruler_metric_smpte]);
	
	_bbt_ruler = gtk_custom_hruler_new ();
	bbt_ruler = Glib::wrap (_bbt_ruler);
	bbt_ruler->set_name ("BBTRuler");
	bbt_ruler->set_size_request (-1, (int)timebar_height);
	gtk_custom_ruler_set_metric (GTK_CUSTOM_RULER(_bbt_ruler), &ruler_metrics[ruler_metric_bbt]);

	_frames_ruler = gtk_custom_hruler_new ();
	frames_ruler = Glib::wrap (_frames_ruler);
	frames_ruler->set_name ("FramesRuler");
	frames_ruler->set_size_request (-1, (int)timebar_height);
	gtk_custom_ruler_set_metric (GTK_CUSTOM_RULER(_frames_ruler), &ruler_metrics[ruler_metric_frames]);

	_minsec_ruler = gtk_custom_hruler_new ();
	minsec_ruler = Glib::wrap (_minsec_ruler);
	minsec_ruler->set_name ("MinSecRuler");
	minsec_ruler->set_size_request (-1, (int)timebar_height);
	gtk_custom_ruler_set_metric (GTK_CUSTOM_RULER(_minsec_ruler), &ruler_metrics[ruler_metric_minsec]);

	
	smpte_ruler->set_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	bbt_ruler->set_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	frames_ruler->set_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	minsec_ruler->set_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);

	smpte_ruler->signal_button_release_event().connect (mem_fun(*this, &Editor::ruler_button_release));
	bbt_ruler->signal_button_release_event().connect (mem_fun(*this, &Editor::ruler_button_release));
	frames_ruler->signal_button_release_event().connect (mem_fun(*this, &Editor::ruler_button_release));
	minsec_ruler->signal_button_release_event().connect (mem_fun(*this, &Editor::ruler_button_release));

	smpte_ruler->signal_button_press_event().connect (mem_fun(*this, &Editor::ruler_button_press));
	bbt_ruler->signal_button_press_event().connect (mem_fun(*this, &Editor::ruler_button_press));
	frames_ruler->signal_button_press_event().connect (mem_fun(*this, &Editor::ruler_button_press));
	minsec_ruler->signal_button_press_event().connect (mem_fun(*this, &Editor::ruler_button_press));
	
	smpte_ruler->signal_motion_notify_event().connect (mem_fun(*this, &Editor::ruler_mouse_motion));
	bbt_ruler->signal_motion_notify_event().connect (mem_fun(*this, &Editor::ruler_mouse_motion));
	frames_ruler->signal_motion_notify_event().connect (mem_fun(*this, &Editor::ruler_mouse_motion));
	minsec_ruler->signal_motion_notify_event().connect (mem_fun(*this, &Editor::ruler_mouse_motion));

	
	if (ruler_shown[ruler_metric_minsec]) {
		lab_children.push_back (Element(minsec_label, PACK_SHRINK, PACK_START));
		ruler_children.insert (canvaspos, Element(*minsec_ruler, PACK_SHRINK, PACK_START));
		visible_timebars++;
	}

	if (ruler_shown[ruler_metric_smpte]) {
		lab_children.push_back (Element(smpte_label, PACK_SHRINK, PACK_START));
		ruler_children.insert (canvaspos, Element(*smpte_ruler, PACK_SHRINK, PACK_START));
		visible_timebars++;
	}

	if (ruler_shown[ruler_metric_frames]) {
		lab_children.push_back (Element(frame_label, PACK_SHRINK, PACK_START));
		ruler_children.insert (canvaspos, Element(*frames_ruler, PACK_SHRINK, PACK_START));
		visible_timebars++;
	}

	if (ruler_shown[ruler_metric_bbt]) {
		lab_children.push_back (Element(bbt_label, PACK_SHRINK, PACK_START));
		ruler_children.insert (canvaspos, Element(*bbt_ruler, PACK_SHRINK, PACK_START));
		visible_timebars++;
	}

	double tbpos = 0.0;
	double old_unit_pos ;
	GtkArg args[1] ;
	args[0].name = "y";
	
	if (ruler_shown[ruler_time_meter]) {
		lab_children.push_back (Element(meter_label, PACK_SHRINK, PACK_START));

		gtk_object_getv (GTK_OBJECT(meter_group), 1, args) ;
		old_unit_pos = GTK_VALUE_DOUBLE (args[0]) ;
		if (tbpos != old_unit_pos) {
			meter_group->move ( 0.0, tbpos - old_unit_pos);
		}

		//gnome_canvas_item_set (meter_group, "y", tbpos, NULL);
		meter_group->show();
		tbpos += timebar_height;
		visible_timebars++;
	}
	else {
		meter_group->hide();
	}
	
	if (ruler_shown[ruler_time_tempo]) {
		lab_children.push_back (Element(tempo_label, PACK_SHRINK, PACK_START));
		gtk_object_getv (GTK_OBJECT(tempo_group), 1, args) ;
		old_unit_pos = GTK_VALUE_DOUBLE (args[0]) ;
		if (tbpos != old_unit_pos) {
			tempo_group->move(0.0, tbpos - old_unit_pos);
		}
		//gnome_canvas_item_set (tempo_group, "y", tbpos, NULL);
		tempo_group->show();
		tbpos += timebar_height;
		visible_timebars++;
	}
	else {
		tempo_group->hide();
	}
	
	if (ruler_shown[ruler_time_marker]) {
		lab_children.push_back (Element(mark_label, PACK_SHRINK, PACK_START));
		gtk_object_getv (GTK_OBJECT(marker_group), 1, args) ;
		old_unit_pos = GTK_VALUE_DOUBLE (args[0]) ;
		if (tbpos != old_unit_pos) {
			marker_group->move ( 0.0, tbpos - old_unit_pos);
		}
		//gnome_canvas_item_set (marker_group, "y", tbpos, NULL);
		marker_group->show();
		tbpos += timebar_height;
		visible_timebars++;
	}
	else {
		marker_group->hide();
	}
	
	if (ruler_shown[ruler_time_range_marker]) {
		lab_children.push_back (Element(range_mark_label, PACK_SHRINK, PACK_START));
		gtk_object_getv (GTK_OBJECT(range_marker_group), 1, args) ;
		old_unit_pos = GTK_VALUE_DOUBLE (args[0]) ;
		if (tbpos != old_unit_pos) {
			range_marker_group->move (0.0, tbpos - old_unit_pos);
		}
		//gnome_canvas_item_set (marker_group, "y", tbpos, NULL);
		range_marker_group->show();
		tbpos += timebar_height;
		visible_timebars++;
	}
	else {
		range_marker_group->hide();
	}

	if (ruler_shown[ruler_time_transport_marker]) {
		lab_children.push_back (Element(transport_mark_label, PACK_SHRINK, PACK_START));
		gtk_object_getv (GTK_OBJECT(transport_marker_group), 1, args) ;
		old_unit_pos = GTK_VALUE_DOUBLE (args[0]) ;
		if (tbpos != old_unit_pos) {
			transport_marker_group->move ( 0.0, tbpos - old_unit_pos);
		}
		//gnome_canvas_item_set (marker_group, "y", tbpos, NULL);
		transport_marker_group->show();
		tbpos += timebar_height;
		visible_timebars++;
	}
	else {
		transport_marker_group->hide();
	}
	
	time_canvas_vbox.set_size_request (-1, (int)(timebar_height * visible_timebars));
	time_canvas_event_box.queue_resize();
	
	update_fixed_rulers();
	//update_tempo_based_rulers();
	tempo_map_changed(Change (0));

	time_canvas_event_box.show_all();
	time_button_event_box.show_all();
}

void
Editor::update_just_smpte ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &Editor::update_just_smpte));
	
	if (session == 0) {
		return;
	}

	/* XXX Note the potential loss of accuracy here as we convert from
	   an uint32_t (or larger) to a float ... what to do ?
	*/

	jack_nframes_t page = (jack_nframes_t) floor (canvas_width * frames_per_unit);
	jack_nframes_t rightmost_frame = leftmost_frame + page;

	if (ruler_shown[ruler_metric_smpte]) {
		gtk_custom_ruler_set_range (GTK_CUSTOM_RULER(_smpte_ruler), leftmost_frame, rightmost_frame,
					    leftmost_frame, session->current_end_frame());
	}
}

void
Editor::update_fixed_rulers ()
{
	jack_nframes_t rightmost_frame;

	if (session == 0) {
		return;
	}

	/* XXX Note the potential loss of accuracy here as we convert from
	   an uint32_t (or larger) to a float ... what to do ?
	*/

	jack_nframes_t page = (jack_nframes_t) floor (canvas_width * frames_per_unit);

	ruler_metrics[ruler_metric_smpte].units_per_pixel = frames_per_unit;
	ruler_metrics[ruler_metric_frames].units_per_pixel = frames_per_unit;
	ruler_metrics[ruler_metric_minsec].units_per_pixel = frames_per_unit;

	rightmost_frame = leftmost_frame + page;

	/* these force a redraw, which in turn will force execution of the metric callbacks
	   to compute the relevant ticks to display.
	*/

	if (ruler_shown[ruler_metric_smpte]) {
		gtk_custom_ruler_set_range (GTK_CUSTOM_RULER(_smpte_ruler), leftmost_frame, rightmost_frame,
					    leftmost_frame, session->current_end_frame());
	}
	
	if (ruler_shown[ruler_metric_frames]) {
		gtk_custom_ruler_set_range (GTK_CUSTOM_RULER(_frames_ruler), leftmost_frame, rightmost_frame,
					    leftmost_frame, session->current_end_frame());
	}
	
	if (ruler_shown[ruler_metric_minsec]) {
		gtk_custom_ruler_set_range (GTK_CUSTOM_RULER(_minsec_ruler), leftmost_frame, rightmost_frame,
					    leftmost_frame, session->current_end_frame());
	}
}		

void
Editor::update_tempo_based_rulers ()
{
	if (session == 0) {
		return;
	}

	/* XXX Note the potential loss of accuracy here as we convert from
	   an uint32_t (or larger) to a float ... what to do ?
	*/

	jack_nframes_t page = (jack_nframes_t) floor (canvas_width * frames_per_unit);
	ruler_metrics[ruler_metric_bbt].units_per_pixel = frames_per_unit;

	if (ruler_shown[ruler_metric_bbt]) {
		gtk_custom_ruler_set_range (GTK_CUSTOM_RULER(_bbt_ruler), leftmost_frame, leftmost_frame+page, 
					    leftmost_frame, session->current_end_frame());
	}
}

/* Mark generation */

gint
Editor::_metric_get_smpte (GtkCustomRulerMark **marks, gulong lower, gulong upper, gint maxchars)
{
	return ruler_editor->metric_get_smpte (marks, lower, upper, maxchars);
}

gint
Editor::_metric_get_bbt (GtkCustomRulerMark **marks, gulong lower, gulong upper, gint maxchars)
{
	return ruler_editor->metric_get_bbt (marks, lower, upper, maxchars);
}

gint
Editor::_metric_get_frames (GtkCustomRulerMark **marks, gulong lower, gulong upper, gint maxchars)
{
	return ruler_editor->metric_get_frames (marks, lower, upper, maxchars);
}

gint
Editor::_metric_get_minsec (GtkCustomRulerMark **marks, gulong lower, gulong upper, gint maxchars)
{
	return ruler_editor->metric_get_minsec (marks, lower, upper, maxchars);
}

gint
Editor::metric_get_smpte (GtkCustomRulerMark **marks, gulong lower, gulong upper, gint maxchars)
{
	jack_nframes_t range;
	jack_nframes_t pos;
	jack_nframes_t spacer;
	jack_nframes_t fr;
	SMPTE_Time smpte;
	gchar buf[16];
	gint nmarks = 0;
	gint n;
	bool show_bits = false;
	bool show_frames = false;
	bool show_seconds = false;
	bool show_minutes = false;
	bool show_hours = false;
	int mark_modulo;

	if (session == 0) {
		return 0;
	}

	fr = session->frame_rate();

	if (lower > (spacer = (jack_nframes_t)(128 * Editor::get_current_zoom ()))) {
		lower = lower - spacer;
	} else {
		upper = upper + spacer - lower;
		lower = 0;
	}
	range = upper - lower;

	if (range < (2 * session->frames_per_smpte_frame())) { /* 0 - 2 frames */
		show_bits = true;
		mark_modulo = 20;
		nmarks = 1 + 160;
	} else if (range <= (fr / 4)) { /* 2 frames - 0.250 second */
		show_frames = true;
		mark_modulo = 1;
		nmarks = 1 + (range / (jack_nframes_t)session->frames_per_smpte_frame());
	} else if (range <= (fr / 2)) { /* 0.25-0.5 second */
		show_frames = true;
		mark_modulo = 2;
		nmarks = 1 + (range / (jack_nframes_t)session->frames_per_smpte_frame());
	} else if (range <= fr) { /* 0.5-1 second */
		show_frames = true;
		mark_modulo = 5;
		nmarks = 1 + (range / (jack_nframes_t)session->frames_per_smpte_frame());
	} else if (range <= 2 * fr) { /* 1-2 seconds */
		show_frames = true;
		mark_modulo = 10;
		nmarks = 1 + (range / (jack_nframes_t)session->frames_per_smpte_frame());
	} else if (range <= 8 * fr) { /* 2-8 seconds */
		show_seconds = true;
		mark_modulo = 1;
		nmarks = 1 + (range / fr);
	} else if (range <= 16 * fr) { /* 8-16 seconds */
		show_seconds = true;
		mark_modulo = 2;
		nmarks = 1 + (range / fr);
	} else if (range <= 30 * fr) { /* 16-30 seconds */
		show_seconds = true;
		mark_modulo = 5;
		nmarks = 1 + (range / fr);
	} else if (range <= 60 * fr) { /* 30-60 seconds */
		show_seconds = true;
		mark_modulo = 5;
		nmarks = 1 + (range / fr);
	} else if (range <= 2 * 60 * fr) { /* 1-2 minutes */
		show_seconds = true;
		mark_modulo = 20;
		nmarks = 1 + (range / fr);
	} else if (range <= 4 * 60 * fr) { /* 2-4 minutes */
		show_seconds = true;
		mark_modulo = 30;
		nmarks = 1 + (range / fr);
	} else if (range <= 10 * 60 * fr) { /* 4-10 minutes */
		show_minutes = true;
		mark_modulo = 2;
		nmarks = 1 + 10;
	} else if (range <= 30 * 60 * fr) { /* 10-30 minutes */
		show_minutes = true;
		mark_modulo = 5;
		nmarks = 1 + 30;
	} else if (range <= 60 * 60 * fr) { /* 30 minutes - 1hr */
		show_minutes = true;
		mark_modulo = 10;
		nmarks = 1 + 60;
	} else if (range <= 4 * 60 * 60 * fr) { /* 1 - 4 hrs*/
		show_minutes = true;
		mark_modulo = 30;
		nmarks = 1 + (60 * 4);
	} else if (range <= 8 * 60 * 60 * fr) { /* 4 - 8 hrs*/
		show_hours = true;
		mark_modulo = 1;
		nmarks = 1 + 8;
	} else if (range <= 16 * 60 * 60 * fr) { /* 16-24 hrs*/
		show_hours = true;
		mark_modulo = 1;
		nmarks = 1 + 24;
	} else {
    
		/* not possible if jack_nframes_t is a 32 bit quantity */
    
		show_hours = true;
		mark_modulo = 4;
		nmarks = 1 + 24;
	}
  
	pos = lower;

	*marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * nmarks);  
	
	if (show_bits) {
		// Find smpte time of this sample (pos) with subframe accuracy
		session->sample_to_smpte(pos, smpte, true /* use_offset */, true /* use_subframes */ );
    
		for (n = 0; n < nmarks; n++) {
			session->smpte_to_sample(smpte, pos, true /* use_offset */, true /* use_subframes */ );
			if ((smpte.subframes % mark_modulo) == 0) {
				if (smpte.subframes == 0) {
					(*marks)[n].style = GtkCustomRulerMarkMajor;
					snprintf (buf, sizeof(buf), "%s%02ld:%02ld:%02ld:%02ld", smpte.negative ? "-" : "", smpte.hours, smpte.minutes, smpte.seconds, smpte.frames);
				} else {
					(*marks)[n].style = GtkCustomRulerMarkMinor;
					snprintf (buf, sizeof(buf), ".%02ld", smpte.subframes);
				}
			} else {
				snprintf (buf, sizeof(buf)," ");
				(*marks)[n].style = GtkCustomRulerMarkMicro;
        
			}
			(*marks)[n].label = g_strdup (buf);
			(*marks)[n].position = pos;

			// Increment subframes by one
			session->smpte_increment_subframes( smpte );
		}
	} else if (show_seconds) {
		// Find smpte time of this sample (pos)
		session->sample_to_smpte(pos, smpte, true /* use_offset */, false /* use_subframes */ );
		// Go to next whole second down
		session->smpte_seconds_floor( smpte );

		for (n = 0; n < nmarks; n++) {
			session->smpte_to_sample(smpte, pos, true /* use_offset */, false /* use_subframes */ );
			if ((smpte.seconds % mark_modulo) == 0) {
				if (smpte.seconds == 0) {
					(*marks)[n].style = GtkCustomRulerMarkMajor;
					(*marks)[n].position = pos;
				} else {
					(*marks)[n].style = GtkCustomRulerMarkMinor;
					(*marks)[n].position = pos;
				}
				snprintf (buf, sizeof(buf), "%s%02ld:%02ld:%02ld:%02ld", smpte.negative ? "-" : "", smpte.hours, smpte.minutes, smpte.seconds, smpte.frames);
			} else {
				snprintf (buf, sizeof(buf)," ");
				(*marks)[n].style = GtkCustomRulerMarkMicro;
				(*marks)[n].position = pos;
        
			}
			(*marks)[n].label = g_strdup (buf);
			session->smpte_increment_seconds( smpte );
		}
	} else if (show_minutes) {
		// Find smpte time of this sample (pos)
		session->sample_to_smpte(pos, smpte, true /* use_offset */, false /* use_subframes */ );
		// Go to next whole minute down
		session->smpte_minutes_floor( smpte );

		for (n = 0; n < nmarks; n++) {
			session->smpte_to_sample(smpte, pos, true /* use_offset */, false /* use_subframes */ );
			if ((smpte.minutes % mark_modulo) == 0) {
				if (smpte.minutes == 0) {
					(*marks)[n].style = GtkCustomRulerMarkMajor;
				} else {
					(*marks)[n].style = GtkCustomRulerMarkMinor;
				}
				snprintf (buf, sizeof(buf), "%s%02ld:%02ld:%02ld:%02ld", smpte.negative ? "-" : "", smpte.hours, smpte.minutes, smpte.seconds, smpte.frames);
			} else {
				snprintf (buf, sizeof(buf)," ");
				(*marks)[n].style = GtkCustomRulerMarkMicro;
        
			}
			(*marks)[n].label = g_strdup (buf);
			(*marks)[n].position = pos;
			session->smpte_increment_minutes( smpte );
		}
	} else if (show_hours) {
		// Find smpte time of this sample (pos)
		session->sample_to_smpte(pos, smpte, true /* use_offset */, false /* use_subframes */ );
		// Go to next whole hour down
		session->smpte_hours_floor( smpte );

		for (n = 0; n < nmarks; n++) {
			session->smpte_to_sample(smpte, pos, true /* use_offset */, false /* use_subframes */ );
			if ((smpte.hours % mark_modulo) == 0) {
				(*marks)[n].style = GtkCustomRulerMarkMajor;
				snprintf (buf, sizeof(buf), "%s%02ld:%02ld:%02ld:%02ld", smpte.negative ? "-" : "", smpte.hours, smpte.minutes, smpte.seconds, smpte.frames);
			} else {
				snprintf (buf, sizeof(buf)," ");
				(*marks)[n].style = GtkCustomRulerMarkMicro;
        
			}
			(*marks)[n].label = g_strdup (buf);
			(*marks)[n].position = pos;

			session->smpte_increment_hours( smpte );
		}
	} else { // show_frames
		// Find smpte time of this sample (pos)
		session->sample_to_smpte(pos, smpte, true /* use_offset */, false /* use_subframes */ );
		// Go to next whole frame down
		session->smpte_frames_floor( smpte );

		for (n = 0; n < nmarks; n++) {
			session->smpte_to_sample(smpte, pos, true /* use_offset */, false /* use_subframes */ );
			if ((smpte.frames % mark_modulo) == 0)  {
				(*marks)[n].style = GtkCustomRulerMarkMajor;
				(*marks)[n].position = pos;
				snprintf (buf, sizeof(buf), "%s%02ld:%02ld:%02ld:%02ld", smpte.negative ? "-" : "", smpte.hours, smpte.minutes, smpte.seconds, smpte.frames);
			} else {
				snprintf (buf, sizeof(buf)," ");
				(*marks)[n].style = GtkCustomRulerMarkMicro;
				(*marks)[n].position = pos;
        
			}
			(*marks)[n].label = g_strdup (buf);
			session->smpte_increment( smpte );
		}
	}
  
	return nmarks;
}


gint
Editor::metric_get_bbt (GtkCustomRulerMark **marks, gulong lower, gulong upper, gint maxchars)
{
        if (session == 0) {
                return 0;
        }

        TempoMap::BBTPointList::iterator i;
	TempoMap::BBTPointList *zoomed_bbt_points;
        uint32_t beats = 0;
        uint32_t bars = 0;
	uint32_t tick = 0;
	uint32_t skip;
	uint32_t t;
        uint32_t zoomed_beats = 0;
        uint32_t zoomed_bars = 0;
        uint32_t desirable_marks;
	uint32_t magic_accent_number = 1;
	gint nmarks;
        char buf[64];
        gint n;
	jack_nframes_t pos;
        jack_nframes_t frame_one_beats_worth;
        jack_nframes_t frame_skip;
	double frame_skip_error;
	double accumulated_error;
        bool bar_helper_on = true;

       
	BBT_Time previous_beat;
	BBT_Time next_beat;
	jack_nframes_t next_beat_pos;

      	if ((desirable_marks = maxchars / 6) == 0) {
               return 0;
        }

        /* align the tick marks to whatever we're snapping to... */
                                                                                                             
        if (snap_type == SnapToAThirdBeat) {
                bbt_beat_subdivision = 3;
        } else if (snap_type == SnapToAQuarterBeat) {
                bbt_beat_subdivision = 4;
        } else if (snap_type == SnapToAEighthBeat) {
                bbt_beat_subdivision = 8;
		magic_accent_number = 2;
        } else if (snap_type == SnapToASixteenthBeat) {
                bbt_beat_subdivision = 16;
		magic_accent_number = 4;
        } else if (snap_type == SnapToAThirtysecondBeat) {
                bbt_beat_subdivision = 32;
		magic_accent_number = 8;
        } else {
	       bbt_beat_subdivision = 4;
	}

	/* First find what a beat's distance is, so we can start plotting stuff before the beginning of the ruler */

	session->bbt_time(lower,previous_beat);
	previous_beat.ticks = 0;
	next_beat = previous_beat;

	if (session->tempo_map().meter_at(lower).beats_per_bar() < (next_beat.beats + 1)) {
	       next_beat.bars += 1;
	       next_beat.beats = 1;
	} else {
	       next_beat.beats += 1;
	}

	frame_one_beats_worth = session->tempo_map().frame_time(next_beat) - session->tempo_map().frame_time(previous_beat);


	zoomed_bbt_points = session->tempo_map().get_points((lower >= frame_one_beats_worth) ? lower - frame_one_beats_worth : 0, upper);

	if (current_bbt_points == 0 || zoomed_bbt_points == 0 || zoomed_bbt_points->empty()) {
		return 0;
	}

	for (i = current_bbt_points->begin(); i != current_bbt_points->end(); i++) {
        	if ((*i).type == TempoMap::Beat) {
			beats++;
		} else if ((*i).type == TempoMap::Bar) {
			bars++;
		}
	}
	/*Only show the bar helper if there aren't many bars on the screen */
	if (bars > 1) {
	        bar_helper_on = false;
	}

	for (i = zoomed_bbt_points->begin(); i != zoomed_bbt_points->end(); i++) {
               	if ((*i).type == TempoMap::Beat) {
                       	zoomed_beats++;
               	} else if ((*i).type == TempoMap::Bar) {
                       	zoomed_bars++;
               	}
       	}

	if (desirable_marks > (beats / 4)) {

		/* we're in beat land...*/

		double position_of_helper;
		bool i_am_accented = false;
		bool we_need_ticks = false;
	
		position_of_helper = lower + (30 * Editor::get_current_zoom ());

		if (desirable_marks >= (beats * 2)) {
               		nmarks = (zoomed_beats * bbt_beat_subdivision) + 1;
			we_need_ticks = true;
		} else {
			nmarks = zoomed_beats + 1;
		}

		*marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * nmarks);

		(*marks)[0].label = g_strdup(" ");
		(*marks)[0].position = lower;
		(*marks)[0].style = GtkCustomRulerMarkMicro;
		
		for (n = 1, i = zoomed_bbt_points->begin(); i != zoomed_bbt_points->end() && n < nmarks; ++i) {
		
			if ((*i).frame <= lower && (bar_helper_on)) {
			
					snprintf (buf, sizeof(buf), "<%" PRIu32 "|%" PRIu32, (*i).bar, (*i).beat);
					(*marks)[0].label = g_strdup (buf); 
			} else {

	       
			  if ((*i).type == TempoMap::Bar)  {
			    tick = 0;
			    (((*i).frame < position_of_helper) && bar_helper_on) ?
			      snprintf (buf, sizeof(buf), " ") : snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bar);
			    (*marks)[n].label = g_strdup (buf);
			    (*marks)[n].position = (*i).frame;
			    (*marks)[n].style = GtkCustomRulerMarkMajor;
			    n++;
			    
			  } else if (((*i).type == TempoMap::Beat) && ((*i).beat > 1)) {
			    tick = 0;
			    ((((*i).frame < position_of_helper) && bar_helper_on) || !we_need_ticks) ?
			      snprintf (buf, sizeof(buf), " ") : snprintf (buf, sizeof(buf), "%" PRIu32, (*i).beat);
			    if (((*i).beat % 2 == 1) || we_need_ticks) {
			      (*marks)[n].style = GtkCustomRulerMarkMinor;
			    } else {
			      (*marks)[n].style = GtkCustomRulerMarkMicro;
			    }
			    (*marks)[n].label =  g_strdup (buf);
			    (*marks)[n].position = (*i).frame;
			    n++;
			  }

			}
			/* Find the next beat */
			
			session->bbt_time((*i).frame, next_beat);

			if (session->tempo_map().meter_at((*i).frame).beats_per_bar() > (next_beat.beats + 1)) {
			  next_beat.beats += 1;
			} else {
			  next_beat.bars += 1;
			  next_beat.beats = 1;
			}
		
			next_beat_pos = session->tempo_map().frame_time(next_beat);

			/* Add the tick marks */

			if (we_need_ticks) {

      			        frame_skip = (jack_nframes_t) floor ((session->frame_rate() *  60) / (bbt_beat_subdivision * (*i).tempo->beats_per_minute()));
			        frame_skip_error =  ((session->frame_rate() *  60.0f) / (bbt_beat_subdivision * (*i).tempo->beats_per_minute())) - frame_skip;
			        skip = (uint32_t) (Meter::ticks_per_beat / bbt_beat_subdivision);

				pos = (*i).frame + frame_skip;
				accumulated_error = frame_skip_error;

				tick += skip;

				for (t = 0; tick < Meter::ticks_per_beat && pos <= next_beat_pos ; pos += frame_skip, tick += skip, ++t) {

					if (t % magic_accent_number == (magic_accent_number - 1)) {
						i_am_accented = true;
					}
					if (Editor::get_current_zoom () > 32) {
						snprintf (buf, sizeof(buf), " ");
					} else if ((Editor::get_current_zoom () > 8) && !i_am_accented) {
						snprintf (buf, sizeof(buf), " ");
					} else  if (bar_helper_on && (pos < position_of_helper)) {
						snprintf (buf, sizeof(buf), " ");
					} else {
						snprintf (buf, sizeof(buf), "%" PRIu32, tick);
					}

					(*marks)[n].label = g_strdup (buf);

					/* Error compensation for float to jack_nframes_t*/
					accumulated_error += frame_skip_error;
				        if (accumulated_error > 1) {
					        pos += 1;
						accumulated_error -= 1.0f;
					}

					(*marks)[n].position = pos;

					if ((bbt_beat_subdivision > 4) && i_am_accented) {
						(*marks)[n].style = GtkCustomRulerMarkMinor;
					} else {
						(*marks)[n].style = GtkCustomRulerMarkMicro;
					}
					i_am_accented = false;
					n++;
				
				}
			}
		}
		delete zoomed_bbt_points;
		return n; //return the actual number of marks made, since we might have skipped some fro fractional time signatures 

       } else {

		/* we're in bar land */

		if (desirable_marks < (uint32_t) (zoomed_bars / 256)) {
        		nmarks = 1;
			*marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * nmarks);
			snprintf (buf, sizeof(buf), "too many bars... (currently %" PRIu32 ")", zoomed_bars );
        		(*marks)[0].style = GtkCustomRulerMarkMajor;
        		(*marks)[0].label = g_strdup (buf);
			(*marks)[0].position = lower;
		} else if (desirable_marks < (uint32_t) (nmarks = (gint) (zoomed_bars / 64))) {
			*marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * nmarks);
			for (n = 0, i = zoomed_bbt_points->begin(); i != zoomed_bbt_points->end() && n < nmarks; i++) {
				if ((*i).type == TempoMap::Bar)  {
					if ((*i).bar % 64 == 1) {
						if ((*i).bar % 256 == 1) {
							snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bar);
							(*marks)[n].style = GtkCustomRulerMarkMajor;
						} else {
							snprintf (buf, sizeof(buf), " ");
							if ((*i).bar % 256 == 129)  {
								(*marks)[n].style = GtkCustomRulerMarkMinor;
							} else {
								(*marks)[n].style = GtkCustomRulerMarkMicro;
							}
						}
						(*marks)[n].label = g_strdup (buf);
						(*marks)[n].position = (*i).frame;
						n++;
					}
				}
			}
		} else if (desirable_marks < (uint32_t) (nmarks = (gint)(zoomed_bars / 16))) {
			*marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * nmarks);
			for (n = 0, i = zoomed_bbt_points->begin(); i != zoomed_bbt_points->end() && n < nmarks; i++) {
				if ((*i).type == TempoMap::Bar)  {
					if ((*i).bar % 16 == 1) {
						if ((*i).bar % 64 == 1) {
							snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bar);
							(*marks)[n].style = GtkCustomRulerMarkMajor;
						} else {
							snprintf (buf, sizeof(buf), " ");
							if ((*i).bar % 64 == 33)  {
								(*marks)[n].style = GtkCustomRulerMarkMinor;
							} else {
								(*marks)[n].style = GtkCustomRulerMarkMicro;
							}
						}
						(*marks)[n].label = g_strdup (buf);
						(*marks)[n].position = (*i).frame;
						n++;
					}
				}
			}
		} else if (desirable_marks < (uint32_t) (nmarks = (gint)(zoomed_bars / 4))){
			*marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * nmarks);
			for (n = 0, i = zoomed_bbt_points->begin(); i != zoomed_bbt_points->end() && n < nmarks; ++i) {
				if ((*i).type == TempoMap::Bar)  {
					if ((*i).bar % 4 == 1) {
						if ((*i).bar % 16 == 1) {
							snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bar);
							(*marks)[n].style = GtkCustomRulerMarkMajor;
						} else {
							snprintf (buf, sizeof(buf), " ");
							if ((*i).bar % 16 == 9)  {
								(*marks)[n].style = GtkCustomRulerMarkMinor;
							} else {
								(*marks)[n].style = GtkCustomRulerMarkMicro;
							}
						}
						(*marks)[n].label = g_strdup (buf);
						(*marks)[n].position = (*i).frame;
						n++;
					}
				}
			}
		} else {
			nmarks = zoomed_bars;
			*marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * nmarks);
                	for (n = 0, i = zoomed_bbt_points->begin(); i != zoomed_bbt_points->end() && n < nmarks; i++) {
                        	if ((*i).type == TempoMap::Bar)  {
                                	if ((*i).bar % 4 == 1) {
                                        	snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bar);
                                        	(*marks)[n].style = GtkCustomRulerMarkMajor;
                                	} else {
                                        	snprintf (buf, sizeof(buf), " ");
						if ((*i).bar % 4 == 3)  {
							(*marks)[n].style = GtkCustomRulerMarkMinor;
						} else {
                                        		(*marks)[n].style = GtkCustomRulerMarkMicro;
						}
                                	}
                                	(*marks)[n].label = g_strdup (buf);
                                	(*marks)[n].position = (*i).frame;
                                	n++;
                        	}
                	}
        	}
		delete zoomed_bbt_points;
		return nmarks;
	}
}

gint
Editor::metric_get_frames (GtkCustomRulerMark **marks, gulong lower, gulong upper, gint maxchars)
{
	jack_nframes_t mark_interval;
	jack_nframes_t pos;
	gchar buf[16];
	gint nmarks;
	gint n;

	if (session == 0) {
		return 0;
	}

	mark_interval = (upper - lower) / 5;
	if (mark_interval > session->frame_rate()) {
		mark_interval -= mark_interval % session->frame_rate();
	} else {
		mark_interval = session->frame_rate() / (session->frame_rate() / mark_interval ) ;
	}
	nmarks = 5;
	*marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * nmarks);
	for (n = 0, pos = lower; n < nmarks; pos += mark_interval, ++n) {
		snprintf (buf, sizeof(buf), "%u", pos);
		(*marks)[n].label = g_strdup (buf);
		(*marks)[n].position = pos;
		(*marks)[n].style = GtkCustomRulerMarkMajor;
	}
	
	return nmarks;
}

static void
sample_to_clock_parts ( jack_nframes_t sample,
			jack_nframes_t sample_rate, 
			long *hrs_p,
			long *mins_p,
			long *secs_p,
			long *millisecs_p)

{
	jack_nframes_t left;
	long hrs;
	long mins;
	long secs;
	long millisecs;
	
	left = sample;
	hrs = left / (sample_rate * 60 * 60);
	left -= hrs * sample_rate * 60 * 60;
	mins = left / (sample_rate * 60);
	left -= mins * sample_rate * 60;
	secs = left / sample_rate;
	left -= secs * sample_rate;
	millisecs = left * 1000 / sample_rate;

	*millisecs_p = millisecs;
	*secs_p = secs;
	*mins_p = mins;
	*hrs_p = hrs;

	return;
}

gint
Editor::metric_get_minsec (GtkCustomRulerMark **marks, gulong lower, gulong upper, gint maxchars)
{
	jack_nframes_t range;
	jack_nframes_t fr;
	jack_nframes_t mark_interval;
	jack_nframes_t pos;
	jack_nframes_t spacer;
	long hrs, mins, secs, millisecs;
	gchar buf[16];
	gint nmarks;
	gint n;
	gint mark_modulo = 100;
	bool show_seconds = false;
	bool show_minutes = false;
	bool show_hours = false;

	if (session == 0) {
		return 0;
	}

	fr = session->frame_rate();

	/* to prevent 'flashing' */
	if (lower > (spacer = (jack_nframes_t)(128 * Editor::get_current_zoom ()))) {
		lower = lower - spacer;
	} else {
		upper = upper + spacer;
		lower = 0;
	}
	range = upper - lower;

	if (range <  (fr / 50)) {
		mark_interval =  fr / 100; /* show 1/100 seconds */
		mark_modulo = 10;
	} else if (range <= (fr / 10)) { /* 0-0.1 second */
		mark_interval = fr / 50; /* show 1/50 seconds */
		mark_modulo = 20;
	} else if (range <= (fr / 2)) { /* 0-0.5 second */
		mark_interval = fr / 20;  /* show 1/20 seconds */
		mark_modulo = 100;
	} else if (range <= fr) { /* 0-1 second */
		mark_interval = fr / 10;  /* show 1/10 seconds */
		mark_modulo = 200;
	} else if (range <= 2 * fr) { /* 1-2 seconds */
		mark_interval = fr / 2; /* show 1/2 seconds */
		mark_modulo = 500;
	} else if (range <= 8 * fr) { /* 2-5 seconds */
		mark_interval =  fr / 5; /* show 2 seconds */
		mark_modulo = 1000;
	} else if (range <= 16 * fr) { /* 8-16 seconds */
		mark_interval =  fr; /* show 1 seconds */
		show_seconds = true;
		mark_modulo = 5;
	} else if (range <= 30 * fr) { /* 10-30 seconds */
		mark_interval =  fr; /* show 10 seconds */
		show_seconds = true;
                mark_modulo = 5;
	} else if (range <= 60 * fr) { /* 30-60 seconds */
                mark_interval = 5 * fr; /* show 5 seconds */
                show_seconds = true;
                mark_modulo = 3;
        } else if (range <= 2 * 60 * fr) { /* 1-2 minutes */
                mark_interval = 5 * fr; /* show 5 seconds */
                show_seconds = true;
                mark_modulo = 3;
        } else if (range <= 4 * 60 * fr) { /* 4 minutes */
                mark_interval = 10 * fr; /* show 10 seconds */
                show_seconds = true;
                mark_modulo = 30;
        } else if (range <= 10 * 60 * fr) { /* 10 minutes */
                mark_interval = 30 * fr; /* show 30 seconds */
                show_seconds = true;
                mark_modulo = 60;
        } else if (range <= 30 * 60 * fr) { /* 10-30 minutes */
                mark_interval =  60 * fr; /* show 1 minute */
                show_minutes = true;
		mark_modulo = 5;
        } else if (range <= 60 * 60 * fr) { /* 30 minutes - 1hr */
                mark_interval = 2 * 60 * fr; /* show 2 minutes */
                show_minutes = true;
                mark_modulo = 10;
        } else if (range <= 4 * 60 * 60 * fr) { /* 1 - 4 hrs*/
                mark_interval = 5 * 60 * fr; /* show 10 minutes */
                show_minutes = true;
                mark_modulo = 30;
        } else if (range <= 8 * 60 * 60 * fr) { /* 4 - 8 hrs*/
                mark_interval = 20 * 60 * fr; /* show 20 minutes */
                show_minutes = true;
                mark_modulo = 60;
        } else if (range <= 16 * 60 * 60 * fr) { /* 16-24 hrs*/
                mark_interval =  60 * 60 * fr; /* show 60 minutes */
                show_hours = true;
		mark_modulo = 2;
        } else {
                                                                                                                   
                /* not possible if jack_nframes_t is a 32 bit quantity */
                                                                                                                   
                mark_interval = 4 * 60 * 60 * fr; /* show 4 hrs */
        }

	nmarks = 1 + (range / mark_interval);
	*marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * nmarks);
	pos = ((lower + (mark_interval/2))/mark_interval) * mark_interval;
	
	if (show_seconds) {
		for (n = 0; n < nmarks; pos += mark_interval, ++n) {
                	sample_to_clock_parts (pos, fr, &hrs, &mins, &secs, &millisecs);
              	  	if (secs % mark_modulo == 0) {
				if (secs == 0) {
					(*marks)[n].style = GtkCustomRulerMarkMajor;
				} else {
					(*marks)[n].style = GtkCustomRulerMarkMinor;
				}
				snprintf (buf, sizeof(buf), "%02ld:%02ld:%02ld.%03ld", hrs, mins, secs, millisecs);
                	} else {
                        	snprintf (buf, sizeof(buf), " ");
	                        (*marks)[n].style = GtkCustomRulerMarkMicro;
        	        }
                	(*marks)[n].label = g_strdup (buf);
              		(*marks)[n].position = pos;
		}
        } else if (show_minutes) {
		for (n = 0; n < nmarks; pos += mark_interval, ++n) {
                        sample_to_clock_parts (pos, fr, &hrs, &mins, &secs, &millisecs);
                        if (mins % mark_modulo == 0) {
                                if (mins == 0) {
                                        (*marks)[n].style = GtkCustomRulerMarkMajor;
                                } else {
                                        (*marks)[n].style = GtkCustomRulerMarkMinor;
                                }
				snprintf (buf, sizeof(buf), "%02ld:%02ld:%02ld.%03ld", hrs, mins, secs, millisecs);
                        } else {
                                snprintf (buf, sizeof(buf), " ");
                                (*marks)[n].style = GtkCustomRulerMarkMicro;
                        }
                        (*marks)[n].label = g_strdup (buf);
                        (*marks)[n].position = pos;
                }
        } else if (show_hours) {
		 for (n = 0; n < nmarks; pos += mark_interval, ++n) {
                        sample_to_clock_parts (pos, fr, &hrs, &mins, &secs, &millisecs);
                        if (hrs % mark_modulo == 0) {
                                (*marks)[n].style = GtkCustomRulerMarkMajor;
                                snprintf (buf, sizeof(buf), "%02ld:%02ld:%02ld.%03ld", hrs, mins, secs, millisecs);
                        } else {
                                snprintf (buf, sizeof(buf), " ");
                                (*marks)[n].style = GtkCustomRulerMarkMicro;
                        }
                        (*marks)[n].label = g_strdup (buf);
                        (*marks)[n].position = pos;
                }
        } else {
		for (n = 0; n < nmarks; pos += mark_interval, ++n) {
			sample_to_clock_parts (pos, fr, &hrs, &mins, &secs, &millisecs);
			if (millisecs % mark_modulo == 0) {
				if (millisecs == 0) {
					(*marks)[n].style = GtkCustomRulerMarkMajor;
				} else {
					(*marks)[n].style = GtkCustomRulerMarkMinor;
				}
				snprintf (buf, sizeof(buf), "%02ld:%02ld:%02ld.%03ld", hrs, mins, secs, millisecs);
			} else {
				snprintf (buf, sizeof(buf), " ");
				(*marks)[n].style = GtkCustomRulerMarkMicro;
			}
			(*marks)[n].label = g_strdup (buf);
			(*marks)[n].position = pos;
		}
	}

	return nmarks;
}
