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
using namespace PBD;
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
	
	_ruler_separator = new Gtk::HSeparator();
	_ruler_separator->set_size_request(-1, 2);
	_ruler_separator->set_name("TimebarPadding");
	_ruler_separator->show();
	
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
		grab_widget->add_modal_grab ();
		ruler_grabbed_widget = grab_widget;
	}

	gint x,y;
	Gdk::ModifierType state;

	/* need to use the correct x,y, the event lies */
	time_canvas_event_box.get_window()->get_pointer (x, y, state);

	nframes_t where = leftmost_frame + pixel_to_frame (x);

	switch (ev->button) {
	case 1:
		// Since we will locate the playhead on button release, cancel any running
		// auditions.
		if (session->is_auditioning()) {
			session->cancel_audition ();
		}
		/* playhead cursor */
		snap_to (where);
		playhead_cursor->set_position (where);
		_dragging_playhead = true;
		break;

	case 2:
		/* edit point */
		if (snap_type != Editing::SnapToEditPoint) {
			snap_to (where);
		}
		break;

	default:
		break;
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

	stop_canvas_autoscroll();
	
	nframes_t where = leftmost_frame + pixel_to_frame (x);

	switch (ev->button) {
	case 1:
		/* transport playhead */
		_dragging_playhead = false;
		snap_to (where);
		session->request_locate (where);
		break;

	case 2:
		/* edit point */
		if (snap_type != Editing::SnapToEditPoint) {
			snap_to (where);
		}
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
		ruler_grabbed_widget->remove_modal_grab();
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
	
	nframes_t where = leftmost_frame + pixel_to_frame (x);

	/// ripped from maybe_autoscroll, and adapted to work here
	nframes_t rightmost_frame = leftmost_frame + current_page_frames ();

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
		/* edit point */
		// EDIT CURSOR XXX do something useful
		break;

	default:
		break;
	}

	if (cursor) {
		cursor->set_position (where);
		
		if (cursor == playhead_cursor) {
			UpdateAllTransportClocks (cursor->current_frame);
		}
	}
	
	return TRUE;
}


void
Editor::popup_ruler_menu (nframes_t where, ItemType t)
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
		ruler_items.push_back (MenuElem (_("Unhide locations"), mem_fun(*this, &Editor::unhide_markers)));
		ruler_items.push_back (SeparatorElem ());
		break;
	case RangeMarkerBarItem:
		//ruler_items.push_back (MenuElem (_("New Range")));
		ruler_items.push_back (MenuElem (_("Clear all ranges"), mem_fun(*this, &Editor::clear_ranges)));
		ruler_items.push_back (MenuElem (_("Unhide ranges"), mem_fun(*this, &Editor::unhide_ranges)));
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

	ruler_items.push_back (CheckMenuElem (_("Timecode"), bind (mem_fun(*this, &Editor::ruler_toggled), (int)ruler_metric_smpte)));
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
	
        editor_ruler_menu->popup (1, gtk_get_current_event_time());

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

	// leave the last one (the time_canvas) intact
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
	
	ruler_children.insert (canvaspos, Element(*_ruler_separator, PACK_SHRINK, PACK_START));

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

	double tbpos = 1.0;
	double old_unit_pos ;
	
	if (ruler_shown[ruler_time_meter]) {
		lab_children.push_back (Element(meter_label, PACK_SHRINK, PACK_START));

		old_unit_pos = meter_group->property_y();
		if (tbpos != old_unit_pos) {
			meter_group->move ( 0.0, tbpos - old_unit_pos);
		}
		meter_group->show();
		tbpos += timebar_height;
		visible_timebars++;
	}
	else {
		meter_group->hide();
	}
	
	if (ruler_shown[ruler_time_tempo]) {
		lab_children.push_back (Element(tempo_label, PACK_SHRINK, PACK_START));
		old_unit_pos = tempo_group->property_y();
		if (tbpos != old_unit_pos) {
			tempo_group->move(0.0, tbpos - old_unit_pos);
		}
		tempo_group->show();
		tbpos += timebar_height;
		visible_timebars++;
	}
	else {
		tempo_group->hide();
	}
	
	if (ruler_shown[ruler_time_range_marker]) {
		lab_children.push_back (Element(range_mark_label, PACK_SHRINK, PACK_START));
		old_unit_pos = range_marker_group->property_y();
		if (tbpos != old_unit_pos) {
			range_marker_group->move (0.0, tbpos - old_unit_pos);
		}
		range_marker_group->show();
		tbpos += timebar_height;
		visible_timebars++;
	}
	else {
		range_marker_group->hide();
	}

	if (ruler_shown[ruler_time_transport_marker]) {
		lab_children.push_back (Element(transport_mark_label, PACK_SHRINK, PACK_START));
		old_unit_pos = transport_marker_group->property_y();
		if (tbpos != old_unit_pos) {
			transport_marker_group->move ( 0.0, tbpos - old_unit_pos);
		}
		transport_marker_group->show();
		tbpos += timebar_height;
		visible_timebars++;
	}
	else {
		transport_marker_group->hide();
	}
	
	if (ruler_shown[ruler_time_marker]) {
		lab_children.push_back (Element(mark_label, PACK_SHRINK, PACK_START));
		old_unit_pos = marker_group->property_y();
		if (tbpos != old_unit_pos) {
			marker_group->move ( 0.0, tbpos - old_unit_pos);
		}
		marker_group->show();
		tbpos += timebar_height;
		visible_timebars++;
	}
	else {
		marker_group->hide();
	}
	
	time_canvas_vbox.set_size_request (-1, (int)(timebar_height * visible_timebars));
	time_canvas_event_box.queue_resize();
	compute_fixed_ruler_scale();
	update_fixed_rulers();

	time_canvas_event_box.show_all();
	time_button_frame.show_all();

	compute_current_bbt_points (leftmost_frame, leftmost_frame + (nframes_t)(canvas_width * frames_per_unit));
	compute_bbt_ruler_scale (leftmost_frame, leftmost_frame + (nframes_t)(canvas_width * frames_per_unit));

	redisplay_tempo (false);
}

void
Editor::update_just_smpte ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &Editor::update_just_smpte));
	
	if (session == 0) {
		return;
	}

	nframes_t rightmost_frame = leftmost_frame + current_page_frames();

	if (ruler_shown[ruler_metric_smpte]) {
		gtk_custom_ruler_set_range (GTK_CUSTOM_RULER(_smpte_ruler), leftmost_frame, rightmost_frame,
					    leftmost_frame, session->current_end_frame());
	}
}

void
Editor::compute_fixed_ruler_scale ()
{
	if (session == 0) {
		return;
	}

	if (ruler_shown[ruler_metric_smpte]) {
		set_smpte_ruler_scale (leftmost_frame, leftmost_frame + (canvas_width * frames_per_unit) );
	}
	
	if (ruler_shown[ruler_metric_minsec]) {
		set_minsec_ruler_scale (leftmost_frame, leftmost_frame + (canvas_width * frames_per_unit) );
	}
}

void
Editor::update_fixed_rulers ()
{
	nframes_t rightmost_frame;

	if (session == 0) {
		return;
	}

	ruler_metrics[ruler_metric_smpte].units_per_pixel = frames_per_unit;
	ruler_metrics[ruler_metric_frames].units_per_pixel = frames_per_unit;
	ruler_metrics[ruler_metric_minsec].units_per_pixel = frames_per_unit;

	rightmost_frame = leftmost_frame + current_page_frames ();

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

	ruler_metrics[ruler_metric_bbt].units_per_pixel = frames_per_unit;
	
	if (ruler_shown[ruler_metric_bbt]) {
		gtk_custom_ruler_set_range (GTK_CUSTOM_RULER(_bbt_ruler), leftmost_frame, leftmost_frame+current_page_frames(),
					    leftmost_frame, session->current_end_frame());
	}
}

/* Mark generation */

gint
Editor::_metric_get_smpte (GtkCustomRulerMark **marks, gdouble lower, gdouble upper, gint maxchars)
{
	return ruler_editor->metric_get_smpte (marks, lower, upper, maxchars);
}

gint
Editor::_metric_get_bbt (GtkCustomRulerMark **marks, gdouble lower, gdouble upper, gint maxchars)
{
	return ruler_editor->metric_get_bbt (marks, lower, upper, maxchars);
}

gint
Editor::_metric_get_frames (GtkCustomRulerMark **marks, gdouble lower, gdouble upper, gint maxchars)
{
	return ruler_editor->metric_get_frames (marks, lower, upper, maxchars);
}

gint
Editor::_metric_get_minsec (GtkCustomRulerMark **marks, gdouble lower, gdouble upper, gint maxchars)
{
	return ruler_editor->metric_get_minsec (marks, lower, upper, maxchars);
}

void
Editor::set_smpte_ruler_scale (gdouble lower, gdouble upper)
{
	nframes_t range;
	nframes_t spacer;
	nframes_t fr;

	if (session == 0) {
		return;
	}

	fr = session->frame_rate();

	if (lower > (spacer = (nframes_t)(128 * Editor::get_current_zoom ()))) {
		lower = lower - spacer;
	} else {
		lower = 0;
	}
	upper = upper + spacer;
	range = (nframes_t) floor (upper - lower);

	if (range < (2 * session->frames_per_smpte_frame())) { /* 0 - 2 frames */
		smpte_ruler_scale = smpte_show_bits;
		smpte_mark_modulo = 20;
		smpte_nmarks = 2 + (2 * Config->get_subframes_per_frame());
	} else if (range <= (fr / 4)) { /* 2 frames - 0.250 second */
		smpte_ruler_scale = smpte_show_frames;
		smpte_mark_modulo = 1;
		smpte_nmarks = 2 + (range / (nframes_t)session->frames_per_smpte_frame());
	} else if (range <= (fr / 2)) { /* 0.25-0.5 second */
		smpte_ruler_scale = smpte_show_frames;
		smpte_mark_modulo = 2;
		smpte_nmarks = 2 + (range / (nframes_t)session->frames_per_smpte_frame());
	} else if (range <= fr) { /* 0.5-1 second */
		smpte_ruler_scale = smpte_show_frames;
		smpte_mark_modulo = 5;
		smpte_nmarks = 2 + (range / (nframes_t)session->frames_per_smpte_frame());
	} else if (range <= 2 * fr) { /* 1-2 seconds */
		smpte_ruler_scale = smpte_show_frames;
		smpte_mark_modulo = 10;
		smpte_nmarks = 2 + (range / (nframes_t)session->frames_per_smpte_frame());
	} else if (range <= 8 * fr) { /* 2-8 seconds */
		smpte_ruler_scale = smpte_show_seconds;
		smpte_mark_modulo = 1;
		smpte_nmarks = 2 + (range / fr);
	} else if (range <= 16 * fr) { /* 8-16 seconds */
		smpte_ruler_scale = smpte_show_seconds;
		smpte_mark_modulo = 2;
		smpte_nmarks = 2 + (range / fr);
	} else if (range <= 30 * fr) { /* 16-30 seconds */
		smpte_ruler_scale = smpte_show_seconds;
		smpte_mark_modulo = 5;
		smpte_nmarks = 2 + (range / fr);
	} else if (range <= 60 * fr) { /* 30-60 seconds */
		smpte_ruler_scale = smpte_show_seconds;
		smpte_mark_modulo = 5;
		smpte_nmarks = 2 + (range / fr);
	} else if (range <= 2 * 60 * fr) { /* 1-2 minutes */
		smpte_ruler_scale = smpte_show_seconds;
		smpte_mark_modulo = 15;
		smpte_nmarks = 2 + (range / fr);
	} else if (range <= 4 * 60 * fr) { /* 2-4 minutes */
		smpte_ruler_scale = smpte_show_seconds;
		smpte_mark_modulo = 30;
		smpte_nmarks = 2 + (range / fr);
	} else if (range <= 10 * 60 * fr) { /* 4-10 minutes */
		smpte_ruler_scale = smpte_show_minutes;
		smpte_mark_modulo = 2;
		smpte_nmarks = 2 + 10;
	} else if (range <= 30 * 60 * fr) { /* 10-30 minutes */
		smpte_ruler_scale = smpte_show_minutes;
		smpte_mark_modulo = 5;
		smpte_nmarks = 2 + 30;
	} else if (range <= 60 * 60 * fr) { /* 30 minutes - 1hr */
		smpte_ruler_scale = smpte_show_minutes;
		smpte_mark_modulo = 10;
		smpte_nmarks = 2 + 60;
	} else if (range <= 4 * 60 * 60 * fr) { /* 1 - 4 hrs*/
		smpte_ruler_scale = smpte_show_minutes;
		smpte_mark_modulo = 30;
		smpte_nmarks = 2 + (60 * 4);
	} else if (range <= 8 * 60 * 60 * fr) { /* 4 - 8 hrs*/
		smpte_ruler_scale = smpte_show_hours;
		smpte_mark_modulo = 1;
		smpte_nmarks = 2 + 8;
	} else if (range <= 16 * 60 * 60 * fr) { /* 16-24 hrs*/
		smpte_ruler_scale = smpte_show_hours;
		smpte_mark_modulo = 1;
		smpte_nmarks = 2 + 24;
	} else {
    
		/* not possible if nframes_t is a 32 bit quantity */
    
		smpte_ruler_scale = smpte_show_hours;
		smpte_mark_modulo = 4;
		smpte_nmarks = 2 + 24;
	}
  
}

gint
Editor::metric_get_smpte (GtkCustomRulerMark **marks, gdouble lower, gdouble upper, gint maxchars)
{
	nframes_t pos;
	nframes_t spacer;
	SMPTE::Time smpte;
	gchar buf[16];
	gint n;

	if (session == 0) {
		return 0;
	}

	if (lower > (spacer = (nframes_t)(128 * Editor::get_current_zoom ()))) {
		lower = lower - spacer;
	} else {
		lower = 0;
	}

	pos = (nframes_t) floor (lower);
	
	*marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * smpte_nmarks);  
	switch (smpte_ruler_scale) {
	case smpte_show_bits:

		// Find smpte time of this sample (pos) with subframe accuracy
		session->sample_to_smpte(pos, smpte, true /* use_offset */, true /* use_subframes */ );
    
		for (n = 0; n < smpte_nmarks; n++) {
			session->smpte_to_sample(smpte, pos, true /* use_offset */, true /* use_subframes */ );
			if ((smpte.subframes % smpte_mark_modulo) == 0) {
				if (smpte.subframes == 0) {
					(*marks)[n].style = GtkCustomRulerMarkMajor;
					snprintf (buf, sizeof(buf), "%s%02u:%02u:%02u:%02u", smpte.negative ? "-" : "", smpte.hours, smpte.minutes, smpte.seconds, smpte.frames);
				} else {
					(*marks)[n].style = GtkCustomRulerMarkMinor;
					snprintf (buf, sizeof(buf), ".%02u", smpte.subframes);
				}
			} else {
				snprintf (buf, sizeof(buf)," ");
				(*marks)[n].style = GtkCustomRulerMarkMicro;
        
			}
			(*marks)[n].label = g_strdup (buf);
			(*marks)[n].position = pos;

			// Increment subframes by one
			SMPTE::increment_subframes( smpte );
		}
	  break;
	case smpte_show_seconds:
		// Find smpte time of this sample (pos)
		session->sample_to_smpte(pos, smpte, true /* use_offset */, false /* use_subframes */ );
		// Go to next whole second down
		SMPTE::seconds_floor( smpte );

		for (n = 0; n < smpte_nmarks; n++) {
			session->smpte_to_sample(smpte, pos, true /* use_offset */, false /* use_subframes */ );
			if ((smpte.seconds % smpte_mark_modulo) == 0) {
				if (smpte.seconds == 0) {
					(*marks)[n].style = GtkCustomRulerMarkMajor;
					(*marks)[n].position = pos;
				} else {
					(*marks)[n].style = GtkCustomRulerMarkMinor;
					(*marks)[n].position = pos;
				}
				snprintf (buf, sizeof(buf), "%s%02u:%02u:%02u:%02u", smpte.negative ? "-" : "", smpte.hours, smpte.minutes, smpte.seconds, smpte.frames);
			} else {
				snprintf (buf, sizeof(buf)," ");
				(*marks)[n].style = GtkCustomRulerMarkMicro;
				(*marks)[n].position = pos;
        
			}
			(*marks)[n].label = g_strdup (buf);
			SMPTE::increment_seconds( smpte );
		}
	  break;
	case smpte_show_minutes:
		// Find smpte time of this sample (pos)
		session->sample_to_smpte(pos, smpte, true /* use_offset */, false /* use_subframes */ );
		// Go to next whole minute down
		SMPTE::minutes_floor( smpte );

		for (n = 0; n < smpte_nmarks; n++) {
			session->smpte_to_sample(smpte, pos, true /* use_offset */, false /* use_subframes */ );
			if ((smpte.minutes % smpte_mark_modulo) == 0) {
				if (smpte.minutes == 0) {
					(*marks)[n].style = GtkCustomRulerMarkMajor;
				} else {
					(*marks)[n].style = GtkCustomRulerMarkMinor;
				}
				snprintf (buf, sizeof(buf), "%s%02u:%02u:%02u:%02u", smpte.negative ? "-" : "", smpte.hours, smpte.minutes, smpte.seconds, smpte.frames);
			} else {
				snprintf (buf, sizeof(buf)," ");
				(*marks)[n].style = GtkCustomRulerMarkMicro;
        
			}
			(*marks)[n].label = g_strdup (buf);
			(*marks)[n].position = pos;
			SMPTE::increment_minutes( smpte );
		}

	  break;
	case smpte_show_hours:
		// Find smpte time of this sample (pos)
		session->sample_to_smpte(pos, smpte, true /* use_offset */, false /* use_subframes */ );
		// Go to next whole hour down
		SMPTE::hours_floor( smpte );

		for (n = 0; n < smpte_nmarks; n++) {
			session->smpte_to_sample(smpte, pos, true /* use_offset */, false /* use_subframes */ );
			if ((smpte.hours % smpte_mark_modulo) == 0) {
				(*marks)[n].style = GtkCustomRulerMarkMajor;
				snprintf (buf, sizeof(buf), "%s%02u:%02u:%02u:%02u", smpte.negative ? "-" : "", smpte.hours, smpte.minutes, smpte.seconds, smpte.frames);
			} else {
				snprintf (buf, sizeof(buf)," ");
				(*marks)[n].style = GtkCustomRulerMarkMicro;
        
			}
			(*marks)[n].label = g_strdup (buf);
			(*marks)[n].position = pos;

			SMPTE::increment_hours( smpte );
		}
	  break;
	case smpte_show_frames:
		// Find smpte time of this sample (pos)
		session->sample_to_smpte(pos, smpte, true /* use_offset */, false /* use_subframes */ );
		// Go to next whole frame down
		SMPTE::frames_floor( smpte );

		for (n = 0; n < smpte_nmarks; n++) {
			session->smpte_to_sample(smpte, pos, true /* use_offset */, false /* use_subframes */ );
			if ((smpte.frames % smpte_mark_modulo) == 0)  {
				if (smpte.frames == 0) {
				  (*marks)[n].style = GtkCustomRulerMarkMajor;
				} else {
				  (*marks)[n].style = GtkCustomRulerMarkMinor;
				}
				(*marks)[n].position = pos;
				snprintf (buf, sizeof(buf), "%s%02u:%02u:%02u:%02u", smpte.negative ? "-" : "", smpte.hours, smpte.minutes, smpte.seconds, smpte.frames);
			} else {
				snprintf (buf, sizeof(buf)," ");
				(*marks)[n].style = GtkCustomRulerMarkMicro;
				(*marks)[n].position = pos;
        
			}
			(*marks)[n].label = g_strdup (buf);
			SMPTE::increment( smpte );
		}

	  break;
	}
  
	return smpte_nmarks;
}


void
Editor::compute_bbt_ruler_scale (nframes_t lower, nframes_t upper)
{
        if (session == 0) {
                return;
        }
	TempoMap::BBTPointList::iterator i;
        BBT_Time lower_beat, upper_beat; // the beats at each end of the ruler

        session->bbt_time((jack_nframes_t) lower, lower_beat);
        session->bbt_time((jack_nframes_t) upper, upper_beat);
        uint32_t beats = 0;

	bbt_accent_modulo = 1;
	bbt_bar_helper_on = false;
        bbt_bars = 0;
        bbt_nmarks = 1;

	bbt_ruler_scale =  bbt_over;
  
	switch (snap_type) {
	case SnapToAThirdBeat:
                bbt_beat_subdivision = 3;
		break;
	case SnapToAQuarterBeat:
                bbt_beat_subdivision = 4;
		break;
	case SnapToAEighthBeat:
                bbt_beat_subdivision = 8;
		bbt_accent_modulo = 2;
		break;
	case SnapToASixteenthBeat:
                bbt_beat_subdivision = 16;
		bbt_accent_modulo = 4;
		break;
	case SnapToAThirtysecondBeat:
                bbt_beat_subdivision = 32;
		bbt_accent_modulo = 8;
		break;
	default:
                bbt_beat_subdivision = 4;
		break;
	}

	if (current_bbt_points == 0 || current_bbt_points->empty()) {
		return;
	}

	i = current_bbt_points->end();
	i--;
	if ((*i).beat >= (*current_bbt_points->begin()).beat) {
	  bbt_bars = (*i).bar - (*current_bbt_points->begin()).bar;
	} else {
	  bbt_bars = (*i).bar - (*current_bbt_points->begin()).bar - 1;
	}
	beats = current_bbt_points->size() - bbt_bars;

	/*Only show the bar helper if there aren't many bars on the screen */
	if ((bbt_bars < 2) || (beats < 5)) {
	        bbt_bar_helper_on = true;
	}

	if (bbt_bars > 8192) {
	  bbt_ruler_scale =  bbt_over;
	} else if (bbt_bars > 1024) {
	  bbt_ruler_scale = bbt_show_64;
	} else if (bbt_bars > 256) {
	  bbt_ruler_scale = bbt_show_16;
	} else if (bbt_bars > 64) {
	  bbt_ruler_scale = bbt_show_4;
	} else if (bbt_bars > 10) {
	  bbt_ruler_scale =  bbt_show_1;
	} else if (bbt_bars > 2) {
	  bbt_ruler_scale =  bbt_show_beats;
	} else  if (bbt_bars > 0) {
	  bbt_ruler_scale =  bbt_show_ticks;
	} else {
	  bbt_ruler_scale =  bbt_show_ticks_detail;
	} 

	if ((bbt_ruler_scale == bbt_show_ticks_detail) && (lower_beat.beats == upper_beat.beats) && (upper_beat.ticks - lower_beat.ticks <= Meter::ticks_per_beat / 4)) {
		bbt_ruler_scale =  bbt_show_ticks_super_detail;
	}
}

gint
Editor::metric_get_bbt (GtkCustomRulerMark **marks, gdouble lower, gdouble upper, gint maxchars)
{
        if (session == 0) {
                return 0;
        }

	TempoMap::BBTPointList::iterator i;

        char buf[64];
        gint  n = 0;
	nframes_t pos;
	BBT_Time next_beat;
	nframes_t next_beat_pos;
        uint32_t beats = 0;

	uint32_t tick = 0;
	uint32_t skip;
	uint32_t t;
	nframes_t frame_skip;
	double frame_skip_error;
	double bbt_position_of_helper;
	double accumulated_error;
	bool i_am_accented = false;
	bool helper_active = false;

	if (current_bbt_points == 0 || current_bbt_points->empty()) {
		return 0;
	}

	switch (bbt_ruler_scale) {

	case bbt_show_beats:
		beats = current_bbt_points->size();
		bbt_nmarks = beats + 2;

		*marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * bbt_nmarks);

		(*marks)[0].label = g_strdup(" ");
		(*marks)[0].position = lower;
		(*marks)[0].style = GtkCustomRulerMarkMicro;
		
		for (n = 1,   i = current_bbt_points->begin(); n < bbt_nmarks && i != current_bbt_points->end(); ++i) {
			if  ((*i).type != TempoMap::Beat) {
				continue;
			}
			if ((*i).frame < lower && (bbt_bar_helper_on)) {
				snprintf (buf, sizeof(buf), "<%" PRIu32 "|%" PRIu32, (*i).bar, (*i).beat);
				(*marks)[0].label = g_strdup (buf); 
				helper_active = true;
			} else {

				if ((*i).beat == 1) {
					(*marks)[n].style = GtkCustomRulerMarkMajor;
					snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bar);
				} else if (((*i).beat % 2 == 1)) {
					(*marks)[n].style = GtkCustomRulerMarkMinor;
					snprintf (buf, sizeof(buf), " ");
				} else {
					(*marks)[n].style = GtkCustomRulerMarkMicro;
					snprintf (buf, sizeof(buf), " ");
				}
				(*marks)[n].label =  g_strdup (buf);
				(*marks)[n].position = (*i).frame;
				n++;
			}
		}
		break;

	case bbt_show_ticks:

		beats = current_bbt_points->size();
		bbt_nmarks = (beats + 2) * bbt_beat_subdivision;

		bbt_position_of_helper = lower + (30 * Editor::get_current_zoom ());
		*marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * bbt_nmarks);

		(*marks)[0].label = g_strdup(" ");
		(*marks)[0].position = lower;
		(*marks)[0].style = GtkCustomRulerMarkMicro;
		
		for (n = 1,   i = current_bbt_points->begin(); n < bbt_nmarks && i != current_bbt_points->end(); ++i) {
			if  ((*i).type != TempoMap::Beat) {
				continue;
			}
			if ((*i).frame < lower && (bbt_bar_helper_on)) {
				snprintf (buf, sizeof(buf), "<%" PRIu32 "|%" PRIu32, (*i).bar, (*i).beat);
				(*marks)[0].label = g_strdup (buf); 
				helper_active = true;
			} else {

			        if ((*i).beat == 1) {
					(*marks)[n].style = GtkCustomRulerMarkMajor;
					snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bar);
				} else {
					(*marks)[n].style = GtkCustomRulerMarkMinor;
					snprintf (buf, sizeof(buf), "%" PRIu32, (*i).beat);
				}
				if (((*i).frame < bbt_position_of_helper) && helper_active) {
					snprintf (buf, sizeof(buf), " ");
				}
				(*marks)[n].label =  g_strdup (buf);
				(*marks)[n].position = (*i).frame;
				n++;
			}
			
			/* Add the tick marks */

			/* Find the next beat */
			next_beat.beats = (*i).beat;
			next_beat.bars = (*i).bar;
			next_beat.ticks = 0;
			
			if ((*i).meter->beats_per_bar() > (next_beat.beats + 1)) {
				  next_beat.beats += 1;
			} else {
				  next_beat.bars += 1;
				  next_beat.beats = 1;
			}
				
			next_beat_pos = session->tempo_map().frame_time(next_beat);
			
			frame_skip = (nframes_t) floor (frame_skip_error = (session->frame_rate() *  60) / (bbt_beat_subdivision * (*i).tempo->beats_per_minute()));
			frame_skip_error -= frame_skip;
			skip = (uint32_t) (Meter::ticks_per_beat / bbt_beat_subdivision);

			pos = (*i).frame + frame_skip;
			accumulated_error = frame_skip_error;

			tick = skip;
			
			for (t = 0; (tick < Meter::ticks_per_beat) && (n < bbt_nmarks) && (pos < next_beat_pos) ; pos += frame_skip, tick += skip, ++t) {

			        if (t % bbt_accent_modulo == (bbt_accent_modulo - 1)) {
					i_am_accented = true;
				}

				snprintf (buf, sizeof(buf), " ");
				(*marks)[n].label = g_strdup (buf);

				/* Error compensation for float to nframes_t*/
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

	  break;

	case bbt_show_ticks_detail:

		beats = current_bbt_points->size();
		bbt_nmarks = (beats + 2) * bbt_beat_subdivision;

		bbt_position_of_helper = lower + (30 * Editor::get_current_zoom ());
		*marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * bbt_nmarks);

		(*marks)[0].label = g_strdup(" ");
		(*marks)[0].position = lower;
		(*marks)[0].style = GtkCustomRulerMarkMicro;
		
		for (n = 1,   i = current_bbt_points->begin(); n < bbt_nmarks && i != current_bbt_points->end(); ++i) {
			if  ((*i).type != TempoMap::Beat) {
				continue;
			}
			if ((*i).frame < lower && (bbt_bar_helper_on)) {
			        snprintf (buf, sizeof(buf), "<%" PRIu32 "|%" PRIu32, (*i).bar, (*i).beat);
				(*marks)[0].label = g_strdup (buf); 
				helper_active = true;
			} else {

				if ((*i).beat == 1) {
					(*marks)[n].style = GtkCustomRulerMarkMajor;
					snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bar);
				} else {
					(*marks)[n].style = GtkCustomRulerMarkMinor;
					snprintf (buf, sizeof(buf), "%" PRIu32, (*i).beat);
				}
				if (((*i).frame < bbt_position_of_helper) && helper_active) {
					snprintf (buf, sizeof(buf), " ");
				}
				(*marks)[n].label =  g_strdup (buf);
				(*marks)[n].position = (*i).frame;
				n++;
			}
			
			/* Add the tick marks */

			/* Find the next beat */

			next_beat.beats = (*i).beat;
			next_beat.bars = (*i).bar;
			
			if ((*i).meter->beats_per_bar() > (next_beat.beats + 1)) {
				  next_beat.beats += 1;
			} else {
				  next_beat.bars += 1;
				  next_beat.beats = 1;
			}
				
			next_beat_pos = session->tempo_map().frame_time(next_beat);
			
			frame_skip = (nframes_t) floor (frame_skip_error = (session->frame_rate() *  60) / (bbt_beat_subdivision * (*i).tempo->beats_per_minute()));
			frame_skip_error -= frame_skip;
			skip = (uint32_t) (Meter::ticks_per_beat / bbt_beat_subdivision);

			pos = (*i).frame + frame_skip;
			accumulated_error = frame_skip_error;

			tick = skip;
			
			for (t = 0; (tick < Meter::ticks_per_beat) && (n < bbt_nmarks) && (pos < next_beat_pos) ; pos += frame_skip, tick += skip, ++t) {

			        if (t % bbt_accent_modulo == (bbt_accent_modulo - 1)) {
				        i_am_accented = true;
				}

				if (i_am_accented && (pos > bbt_position_of_helper)){
				        snprintf (buf, sizeof(buf), "%" PRIu32, tick);
				} else {
				        snprintf (buf, sizeof(buf), " ");
				}

				(*marks)[n].label = g_strdup (buf);

				/* Error compensation for float to nframes_t*/
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

	  break;

	case bbt_show_ticks_super_detail:

		beats = current_bbt_points->size();
		bbt_nmarks = (beats + 2) * bbt_beat_subdivision;

		bbt_position_of_helper = lower + (30 * Editor::get_current_zoom ());
		*marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * bbt_nmarks);

		(*marks)[0].label = g_strdup(" ");
		(*marks)[0].position = lower;
		(*marks)[0].style = GtkCustomRulerMarkMicro;
		
		for (n = 1,   i = current_bbt_points->begin(); n < bbt_nmarks && i != current_bbt_points->end(); ++i) {
			if  ((*i).type != TempoMap::Beat) {
				  continue;
			}
			if ((*i).frame < lower && (bbt_bar_helper_on)) {
				  snprintf (buf, sizeof(buf), "<%" PRIu32 "|%" PRIu32, (*i).bar, (*i).beat);
				  (*marks)[0].label = g_strdup (buf); 
				  helper_active = true;
			} else {

				  if ((*i).beat == 1) {
					  (*marks)[n].style = GtkCustomRulerMarkMajor;
					  snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bar);
				  } else {
					  (*marks)[n].style = GtkCustomRulerMarkMinor;
					  snprintf (buf, sizeof(buf), "%" PRIu32, (*i).beat);
				  }
				  if (((*i).frame < bbt_position_of_helper) && helper_active) {
					  snprintf (buf, sizeof(buf), " ");
				  }
				  (*marks)[n].label =  g_strdup (buf);
				  (*marks)[n].position = (*i).frame;
				  n++;
			}
			
			/* Add the tick marks */

			/* Find the next beat */

			next_beat.beats = (*i).beat;
			next_beat.bars = (*i).bar;
			
			if ((*i).meter->beats_per_bar() > (next_beat.beats + 1)) {
				  next_beat.beats += 1;
			} else {
				  next_beat.bars += 1;
				  next_beat.beats = 1;
			}
				
			next_beat_pos = session->tempo_map().frame_time(next_beat);
			
			frame_skip = (nframes_t) floor (frame_skip_error = (session->frame_rate() *  60) / (bbt_beat_subdivision * (*i).tempo->beats_per_minute()));
			frame_skip_error -= frame_skip;
			skip = (uint32_t) (Meter::ticks_per_beat / bbt_beat_subdivision);

			pos = (*i).frame + frame_skip;
			accumulated_error = frame_skip_error;

			tick = skip;
			
			for (t = 0; (tick < Meter::ticks_per_beat) && (n < bbt_nmarks) && (pos < next_beat_pos) ; pos += frame_skip, tick += skip, ++t) {

				  if (t % bbt_accent_modulo == (bbt_accent_modulo - 1)) {
					  i_am_accented = true;
				  }

				  if (pos > bbt_position_of_helper) {
 					  snprintf (buf, sizeof(buf), "%" PRIu32, tick);
				  } else {
					  snprintf (buf, sizeof(buf), " ");
				  }

				  (*marks)[n].label = g_strdup (buf);

				  /* Error compensation for float to nframes_t*/
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

	  break;

	case bbt_over:
	                bbt_nmarks = 1;
			*marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * bbt_nmarks);
			snprintf (buf, sizeof(buf), "cannot handle %" PRIu32 " bars", bbt_bars );
        		(*marks)[0].style = GtkCustomRulerMarkMajor;
        		(*marks)[0].label = g_strdup (buf);
			(*marks)[0].position = lower;
			n = 1;

	  break;

	case bbt_show_64:
        		bbt_nmarks = (gint) (bbt_bars / 64) + 1;
			*marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * bbt_nmarks);
			for (n = 0,   i = current_bbt_points->begin(); i != current_bbt_points->end() && n < bbt_nmarks; i++) {
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
			break;

	case bbt_show_16:
       		bbt_nmarks = (bbt_bars / 16) + 1;
	        *marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * bbt_nmarks);
		for (n = 0,  i = current_bbt_points->begin(); i != current_bbt_points->end() && n < bbt_nmarks; i++) {
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
	  break;

	case bbt_show_4:
		bbt_nmarks = (bbt_bars / 4) + 1;
 		*marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * bbt_nmarks);
		for (n = 0,   i = current_bbt_points->begin(); i != current_bbt_points->end() && n < bbt_nmarks; ++i) {
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
	  break;

	case bbt_show_1:
  //	default:
	        bbt_nmarks = bbt_bars + 2;
	        *marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * bbt_nmarks );
		for (n = 0,  i = current_bbt_points->begin(); i != current_bbt_points->end() && n < bbt_nmarks; i++) {
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
 
	break;

	}

	return n; //return the actual number of marks made, since we might have skipped some from fractional time signatures 

}

gint
Editor::metric_get_frames (GtkCustomRulerMark **marks, gdouble lower, gdouble upper, gint maxchars)
{
	nframes_t mark_interval;
	nframes_t pos;
	nframes_t ilower = (nframes_t) floor (lower);
	nframes_t iupper = (nframes_t) floor (upper);
	gchar buf[16];
	gint nmarks;
	gint n;

	if (session == 0) {
		return 0;
	}

	mark_interval = (iupper - ilower) / 5;
	if (mark_interval > session->frame_rate()) {
		mark_interval -= mark_interval % session->frame_rate();
	} else {
		mark_interval = session->frame_rate() / (session->frame_rate() / mark_interval ) ;
	}
	nmarks = 5;
	*marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * nmarks);
	for (n = 0, pos = ilower; n < nmarks; pos += mark_interval, ++n) {
		snprintf (buf, sizeof(buf), "%u", pos);
		(*marks)[n].label = g_strdup (buf);
		(*marks)[n].position = pos;
		(*marks)[n].style = GtkCustomRulerMarkMajor;
	}
	
	return nmarks;
}

static void
sample_to_clock_parts ( nframes_t sample,
			nframes_t sample_rate, 
			long *hrs_p,
			long *mins_p,
			long *secs_p,
			long *millisecs_p)

{
	nframes_t left;
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

void
Editor::set_minsec_ruler_scale (gdouble lower, gdouble upper)
{
	nframes_t range;
	nframes_t fr;
	nframes_t spacer;

	if (session == 0) {
		return;
	}

	fr = session->frame_rate();

	/* to prevent 'flashing' */
	if (lower > (spacer = (nframes_t)(128 * Editor::get_current_zoom ()))) {
		lower -= spacer;
	} else {
		lower = 0;
	}
	upper += spacer;
	range = (nframes_t) (upper - lower);

	if (range <  (fr / 50)) {
		minsec_mark_interval =  fr / 1000; /* show 1/1000 seconds */
		minsec_ruler_scale = minsec_show_frames;
		minsec_mark_modulo = 10;
	} else if (range <= (fr / 10)) { /* 0-0.1 second */
		minsec_mark_interval = fr / 1000; /* show 1/1000 seconds */
		minsec_ruler_scale = minsec_show_frames;
		minsec_mark_modulo = 10;
	} else if (range <= (fr / 2)) { /* 0-0.5 second */
		minsec_mark_interval = fr / 100;  /* show 1/100 seconds */
		minsec_ruler_scale = minsec_show_frames;
		minsec_mark_modulo = 100;
	} else if (range <= fr) { /* 0-1 second */
		minsec_mark_interval = fr / 10;  /* show 1/10 seconds */
		minsec_ruler_scale = minsec_show_frames;
		minsec_mark_modulo = 200;
	} else if (range <= 2 * fr) { /* 1-2 seconds */
		minsec_mark_interval = fr / 10; /* show 1/10 seconds */
		minsec_ruler_scale = minsec_show_frames;
		minsec_mark_modulo = 500;
	} else if (range <= 8 * fr) { /* 2-5 seconds */
		minsec_mark_interval =  fr / 5; /* show 2 seconds */
		minsec_ruler_scale = minsec_show_frames;
		minsec_mark_modulo = 1000;
	} else if (range <= 16 * fr) { /* 8-16 seconds */
		minsec_mark_interval =  fr; /* show 1 seconds */
		minsec_ruler_scale = minsec_show_seconds;
		minsec_mark_modulo = 2;
	} else if (range <= 30 * fr) { /* 10-30 seconds */
		minsec_mark_interval =  fr; /* show 1 seconds */
		minsec_ruler_scale = minsec_show_seconds;
                minsec_mark_modulo = 5;
	} else if (range <= 60 * fr) { /* 30-60 seconds */
                minsec_mark_interval = fr; /* show 1 seconds */
                minsec_ruler_scale = minsec_show_seconds;
                minsec_mark_modulo = 5;
        } else if (range <= 2 * 60 * fr) { /* 1-2 minutes */
                minsec_mark_interval = 5 * fr; /* show 5 seconds */
                minsec_ruler_scale = minsec_show_seconds;
                minsec_mark_modulo = 3;
        } else if (range <= 4 * 60 * fr) { /* 4 minutes */
                minsec_mark_interval = 5 * fr; /* show 10 seconds */
                minsec_ruler_scale = minsec_show_seconds;
                minsec_mark_modulo = 30;
        } else if (range <= 10 * 60 * fr) { /* 10 minutes */
                minsec_mark_interval = 30 * fr; /* show 30 seconds */
                minsec_ruler_scale = minsec_show_seconds;
                minsec_mark_modulo = 120;
        } else if (range <= 30 * 60 * fr) { /* 10-30 minutes */
                minsec_mark_interval =  60 * fr; /* show 1 minute */
                minsec_ruler_scale = minsec_show_minutes;
		minsec_mark_modulo = 5;
        } else if (range <= 60 * 60 * fr) { /* 30 minutes - 1hr */
                minsec_mark_interval = 2 * 60 * fr; /* show 2 minutes */
                minsec_ruler_scale = minsec_show_minutes;
                minsec_mark_modulo = 10;
        } else if (range <= 4 * 60 * 60 * fr) { /* 1 - 4 hrs*/
                minsec_mark_interval = 5 * 60 * fr; /* show 10 minutes */
                minsec_ruler_scale = minsec_show_minutes;
                minsec_mark_modulo = 30;
        } else if (range <= 8 * 60 * 60 * fr) { /* 4 - 8 hrs*/
                minsec_mark_interval = 20 * 60 * fr; /* show 20 minutes */
                minsec_ruler_scale = minsec_show_minutes;
                minsec_mark_modulo = 60;
        } else if (range <= 16 * 60 * 60 * fr) { /* 16-24 hrs*/
                minsec_mark_interval =  60 * 60 * fr; /* show 60 minutes */
                minsec_ruler_scale = minsec_show_hours;
		minsec_mark_modulo = 2;
        } else {
                                                                                                                   
                /* not possible if nframes_t is a 32 bit quantity */
                                                                                                                   
                minsec_mark_interval = 4 * 60 * 60 * fr; /* show 4 hrs */
        }
	minsec_nmarks = 2 + (range / minsec_mark_interval);
}

gint
Editor::metric_get_minsec (GtkCustomRulerMark **marks, gdouble lower, gdouble upper, gint maxchars)
{
	nframes_t pos;
	nframes_t spacer;
	long hrs, mins, secs, millisecs;
	gchar buf[16];
	gint n;

	if (session == 0) {
		return 0;
	}

	/* to prevent 'flashing' */
	if (lower > (spacer = (nframes_t)(128 * Editor::get_current_zoom ()))) {
		lower = lower - spacer;
	} else {
		lower = 0;
	}

	*marks = (GtkCustomRulerMark *) g_malloc (sizeof(GtkCustomRulerMark) * minsec_nmarks);
	pos = ((((nframes_t) floor(lower)) + (minsec_mark_interval/2))/minsec_mark_interval) * minsec_mark_interval;
	switch (minsec_ruler_scale) {
	case minsec_show_seconds:
		for (n = 0; n < minsec_nmarks; pos += minsec_mark_interval, ++n) {
                	sample_to_clock_parts (pos, session->frame_rate(), &hrs, &mins, &secs, &millisecs);
              	  	if (secs % minsec_mark_modulo == 0) {
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
	  break;
	case minsec_show_minutes:
		for (n = 0; n < minsec_nmarks; pos += minsec_mark_interval, ++n) {
                        sample_to_clock_parts (pos, session->frame_rate(), &hrs, &mins, &secs, &millisecs);
                        if (mins % minsec_mark_modulo == 0) {
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
	  break;
	case minsec_show_hours:
		 for (n = 0; n < minsec_nmarks; pos += minsec_mark_interval, ++n) {
                        sample_to_clock_parts (pos, session->frame_rate(), &hrs, &mins, &secs, &millisecs);
                        if (hrs % minsec_mark_modulo == 0) {
                                (*marks)[n].style = GtkCustomRulerMarkMajor;
                                snprintf (buf, sizeof(buf), "%02ld:%02ld:%02ld.%03ld", hrs, mins, secs, millisecs);
                        } else {
                                snprintf (buf, sizeof(buf), " ");
                                (*marks)[n].style = GtkCustomRulerMarkMicro;
                        }
                        (*marks)[n].label = g_strdup (buf);
                        (*marks)[n].position = pos;
                }
	      break;
	case minsec_show_frames:
		for (n = 0; n < minsec_nmarks; pos += minsec_mark_interval, ++n) {
			sample_to_clock_parts (pos, session->frame_rate(), &hrs, &mins, &secs, &millisecs);
			if (millisecs % minsec_mark_modulo == 0) {
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
	  break;
	}

	return minsec_nmarks;
}
