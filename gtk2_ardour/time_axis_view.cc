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


#include "pbd/error.h"
#include "pbd/convert.h"

#include <gtkmm2ext/doi.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/selector.h>

#include "canvas/rectangle.h"

#include "ardour_ui.h"
#include "ardour_dialog.h"
#include "global_signals.h"
#include "gui_thread.h"
#include "public_editor.h"
#include "time_axis_view.h"
#include "region_view.h"
#include "ghostregion.h"
#include "selection.h"
#include "keyboard.h"
#include "rgb_macros.h"
#include "utils.h"
#include "streamview.h"
#include "editor_drag.h"
#include "editor.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace ArdourCanvas;
using Gtkmm2ext::Keyboard;

const double trim_handle_size = 6.0; /* pixels */
uint32_t TimeAxisView::button_height = 0;
uint32_t TimeAxisView::extra_height = 0;
int const TimeAxisView::_max_order = 512;
PBD::Signal1<void,TimeAxisView*> TimeAxisView::CatchDeletion;

TimeAxisView::TimeAxisView (ARDOUR::Session* sess, PublicEditor& ed, TimeAxisView* rent, Canvas& /*canvas*/)
	: AxisView (sess)
	, controls_table (2, 8)
	, _name_editing (false)
	, height (0)
	, display_menu (0)
	, parent (rent)
	, selection_group (0)
	, _hidden (false)
	, in_destructor (false)
	, _size_menu (0)
	, _canvas_display (0)
	, _y_position (0)
	, _editor (ed)
	, name_entry (0)
	, control_parent (0)
	, _order (0)
	, _effective_height (0)
	, _resize_drag_start (-1)
	, _preresize_cursor (0)
	, _have_preresize_cursor (false)
	, _ghost_group (0)
	, _ebox_release_can_act (true)
{
	if (extra_height == 0) {
		compute_heights ();
	}

	_canvas_display = new Group (ed.get_trackview_group (), ArdourCanvas::Duple (0.0, 0.0));
	_canvas_display->hide(); // reveal as needed

	selection_group = new Group (_canvas_display);
	selection_group->set_data (X_("timeselection"), (void *) 1);
	selection_group->hide();

	_ghost_group = new Group (_canvas_display);
	_ghost_group->lower_to_bottom();
	_ghost_group->show();

	name_label.set_name ("TrackLabel");
	name_label.set_alignment (0.0, 0.5);
	ARDOUR_UI::instance()->set_tip (name_label, _("Track/Bus name (double click to edit)"));

	Gtk::Entry* an_entry = new Gtk::Entry;
	Gtk::Requisition req;
	an_entry->size_request (req);
	name_label.set_size_request (-1, req.height);
	delete an_entry;

	name_hbox.pack_start (name_label, true, true);
	name_hbox.show ();
	name_label.show ();
	
	controls_table.set_size_request (200);
	controls_table.set_row_spacings (2);
	controls_table.set_col_spacings (2);
	controls_table.set_border_width (2);
	controls_table.set_homogeneous (true);

	controls_table.attach (name_hbox, 0, 5, 0, 1,  Gtk::FILL|Gtk::EXPAND,  Gtk::FILL|Gtk::EXPAND, 3, 0);
	controls_table.show_all ();
	controls_table.set_no_show_all ();

	HSeparator* separator = manage (new HSeparator());

	controls_vbox.pack_start (controls_table, false, false);
	controls_vbox.show ();

	//controls_ebox.set_name ("TimeAxisViewControlsBaseUnselected");
	controls_ebox.add (controls_vbox);
	controls_ebox.add_events (Gdk::BUTTON_PRESS_MASK|
				  Gdk::BUTTON_RELEASE_MASK|
				  Gdk::POINTER_MOTION_MASK|
				  Gdk::ENTER_NOTIFY_MASK|
				  Gdk::LEAVE_NOTIFY_MASK|
				  Gdk::SCROLL_MASK);
	controls_ebox.set_flags (CAN_FOCUS);

	/* note that this handler connects *before* the default handler */
	controls_ebox.signal_scroll_event().connect (sigc::mem_fun (*this, &TimeAxisView::controls_ebox_scroll), true);
	controls_ebox.signal_button_press_event().connect (sigc::mem_fun (*this, &TimeAxisView::controls_ebox_button_press));
	controls_ebox.signal_button_release_event().connect (sigc::mem_fun (*this, &TimeAxisView::controls_ebox_button_release));
	controls_ebox.signal_motion_notify_event().connect (sigc::mem_fun (*this, &TimeAxisView::controls_ebox_motion));
	controls_ebox.signal_leave_notify_event().connect (sigc::mem_fun (*this, &TimeAxisView::controls_ebox_leave));
	controls_ebox.show ();

	controls_hbox.pack_start (controls_ebox, true, true);
	controls_hbox.show ();

	time_axis_vbox.pack_start (controls_hbox, true, true);
	time_axis_vbox.pack_end (*separator, false, false);
	time_axis_vbox.show();

	ColorsChanged.connect (sigc::mem_fun (*this, &TimeAxisView::color_handler));

	GhostRegion::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&TimeAxisView::erase_ghost, this, _1), gui_context());
}

TimeAxisView::~TimeAxisView()
{
	in_destructor = true;

	for (list<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
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

	delete selection_group;
	selection_group = 0;

	delete _canvas_display;
	_canvas_display = 0;

	delete display_menu;
	display_menu = 0;

	delete _size_menu;
}

void
TimeAxisView::hide ()
{
	if (_hidden) {
		return;
	}

	_canvas_display->hide ();

	if (control_parent) {
		control_parent->remove (time_axis_vbox);
		control_parent = 0;
	}

	_y_position = -1;
	_hidden = true;

	/* now hide children */

	for (Children::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->hide ();
	}

	/* if its hidden, it cannot be selected */
	_editor.get_selection().remove (this);
	/* and neither can its regions */
	_editor.get_selection().remove_regions (this);

	Hiding ();
}

/** Display this TimeAxisView as the nth component of the parent box, at y.
*
* @param y y position.
* @param nth index for this TimeAxisView, increased if this view has children.
* @param parent parent component.
* @return height of this TimeAxisView.
*/
guint32
TimeAxisView::show_at (double y, int& nth, VBox *parent)
{
	if (control_parent) {
		control_parent->reorder_child (time_axis_vbox, nth);
	} else {
		control_parent = parent;
		parent->pack_start (time_axis_vbox, false, false);
		parent->reorder_child (time_axis_vbox, nth);
	}

	_order = nth;

	if (_y_position != y) {
		_canvas_display->set_y_position (y);
		_y_position = y;

	}

	_canvas_display->raise_to_top ();
	_canvas_display->show ();

	_hidden = false;

	_effective_height = current_height ();

	/* now show relevant children */

	for (Children::iterator i = children.begin(); i != children.end(); ++i) {
		if ((*i)->marked_for_display()) {
			++nth;
			_effective_height += (*i)->show_at (y + _effective_height, nth, parent);
		} else {
			(*i)->hide ();
		}
	}

	return _effective_height;
}

bool
TimeAxisView::controls_ebox_scroll (GdkEventScroll* ev)
{
	switch (ev->direction) {
	case GDK_SCROLL_UP:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
			/* See Editor::_stepping_axis_view for notes on this hack */
			Editor& e = dynamic_cast<Editor&> (_editor);
			if (!e.stepping_axis_view ()) {
				e.set_stepping_axis_view (this);
			}
			e.stepping_axis_view()->step_height (false);
			return true;
		} else if (Keyboard::no_modifiers_active (ev->state)) {
			_editor.scroll_tracks_up_line();
			return true;
		}
		break;

	case GDK_SCROLL_DOWN:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
			/* See Editor::_stepping_axis_view for notes on this hack */
			Editor& e = dynamic_cast<Editor&> (_editor);
			if (!e.stepping_axis_view ()) {
				e.set_stepping_axis_view (this);
			}
			e.stepping_axis_view()->step_height (true);
			return true;
		} else if (Keyboard::no_modifiers_active (ev->state)) {
			_editor.scroll_tracks_down_line();
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
TimeAxisView::controls_ebox_button_press (GdkEventButton* event)
{
	if ((event->button == 1 && event->type == GDK_2BUTTON_PRESS) || Keyboard::is_edit_event (event)) {
		/* see if it is inside the name label */
		if (name_label.is_ancestor (controls_ebox)) {
			int nlx;
			int nly;
			controls_ebox.translate_coordinates (name_label, event->x, event->y, nlx, nly);
			Gtk::Allocation a = name_label.get_allocation ();
			if (nlx > 0 && nlx < a.get_width() && nly > 0 && nly < a.get_height()) {
				begin_name_edit ();
				_ebox_release_can_act = false;
				return true;
			}
		}

	}

	_ebox_release_can_act = true;
			
	if (maybe_set_cursor (event->y) > 0) {
		_resize_drag_start = event->y_root;
	}

	return true;
}

void
TimeAxisView::idle_resize (uint32_t h)
{
	set_height (h);
}


bool
TimeAxisView::controls_ebox_motion (GdkEventMotion* ev)
{
	if (_resize_drag_start >= 0) {
		/* (ab)use the DragManager to do autoscrolling; adjust the event coordinates
		   into the world coordinate space that DragManager::motion_handler is expecting,
		   and then fake a DragManager motion event so that when maybe_autoscroll
		   asks DragManager for the current pointer position it will get the correct
		   answers.
		*/
		int tx, ty;
		controls_ebox.translate_coordinates (*control_parent, ev->x, ev->y, tx, ty);
		ev->y = ty;
		_editor.drags()->motion_handler ((GdkEvent *) ev, false);
		_editor.maybe_autoscroll (false, true, false, ev->y_root < _resize_drag_start);

		/* now do the actual TAV resize */
                int32_t const delta = (int32_t) floor (ev->y_root - _resize_drag_start);
                _editor.add_to_idle_resize (this, delta);
                _resize_drag_start = ev->y_root;
        } else {
		/* not dragging but ... */
		maybe_set_cursor (ev->y);
	}

	return true;
}

bool
TimeAxisView::controls_ebox_leave (GdkEventCrossing*)
{
	if (_have_preresize_cursor) {
		gdk_window_set_cursor (controls_ebox.get_window()->gobj(), _preresize_cursor);
		_have_preresize_cursor = false;
	}
	return true;
}

bool
TimeAxisView::maybe_set_cursor (int y)
{
	/* XXX no Gtkmm Gdk::Window::get_cursor() */
	Glib::RefPtr<Gdk::Window> win = controls_ebox.get_window();

	if (y > (gint) floor (controls_ebox.get_height() * 0.75)) {

		/* y-coordinate in lower 25% */

		if (!_have_preresize_cursor) {
			_preresize_cursor = gdk_window_get_cursor (win->gobj());
			_have_preresize_cursor = true;
			win->set_cursor (Gdk::Cursor(Gdk::SB_V_DOUBLE_ARROW));
		}

		return 1;

	} else if (_have_preresize_cursor) {
		gdk_window_set_cursor (win->gobj(), _preresize_cursor);
		_have_preresize_cursor = false;

		return -1;
	}

	return 0;
}

bool
TimeAxisView::controls_ebox_button_release (GdkEventButton* ev)
{
	if (_resize_drag_start >= 0) {
		if (_have_preresize_cursor) {
			gdk_window_set_cursor (controls_ebox.get_window()->gobj(), _preresize_cursor);
			_preresize_cursor = 0;
			_have_preresize_cursor = false;
		}
		_editor.stop_canvas_autoscroll ();
		_resize_drag_start = -1;
	}

	if (!_ebox_release_can_act) {
		return true;
	}

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
	Selection::Operation op = ArdourKeyboard::selection_type (ev->state);
	_editor.set_selected_track (*this, op, false);
}


/** Steps through the defined heights for this TrackView.
 *  @param coarser true if stepping should decrease in size, otherwise false.
 */
void
TimeAxisView::step_height (bool coarser)
{
	static const uint32_t step = 25;

	if (coarser) {

		if (height <= preset_height (HeightSmall)) {
			return;
		} else if (height <= preset_height (HeightNormal) && height > preset_height (HeightSmall)) {
			set_height_enum (HeightSmall);
		} else {
			set_height (height - step);
		}

	} else {

		if (height <= preset_height(HeightSmall)) {
			set_height_enum (HeightNormal);
		} else {
			set_height (height + step);
		}

	}
}

void
TimeAxisView::set_height_enum (Height h, bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_time_axis (boost::bind (&TimeAxisView::set_height_enum, _1, h, false));
	} else {
		set_height (preset_height (h));
	}
}

void
TimeAxisView::set_height (uint32_t h)
{
	if (h < preset_height (HeightSmall)) {
		h = preset_height (HeightSmall);
	}

	time_axis_vbox.property_height_request () = h;
	height = h;

	char buf[32];
	snprintf (buf, sizeof (buf), "%u", height);
	set_gui_property ("height", buf);

	for (list<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		(*i)->set_height ();
	}

	if (selection_group->visible ()) {
		/* resize the selection rect */
		show_selection (_editor.get_selection().time);
	}
}

bool
TimeAxisView::name_entry_key_press (GdkEventKey* ev)
{
	/* steal escape, tabs from GTK */

	switch (ev->keyval) {
	case GDK_Escape:
	case GDK_ISO_Left_Tab:
	case GDK_Tab:
		return true;
	}
	return false;
}

bool
TimeAxisView::name_entry_key_release (GdkEventKey* ev)
{
	TrackViewList::iterator i;

	switch (ev->keyval) {
	case GDK_Escape:
		end_name_edit (RESPONSE_CANCEL);
		return true;

	/* Shift+Tab Keys Pressed. Note that for Shift+Tab, GDK actually
	 * generates a different ev->keyval, rather than setting
	 * ev->state.
	 */
	case GDK_ISO_Left_Tab:
		end_name_edit (RESPONSE_APPLY);
		return true;

	case GDK_Tab:
		end_name_edit (RESPONSE_ACCEPT);
		return true;
	default:
		break;
	}

	return false;
}

bool
TimeAxisView::name_entry_focus_out (GdkEventFocus*)
{
	end_name_edit (RESPONSE_OK);
	return false;
}

void
TimeAxisView::begin_name_edit ()
{
	if (name_entry) {
		return;
	}

	if (can_edit_name()) {

		name_entry = manage (new Gtkmm2ext::FocusEntry);
		
		name_entry->set_name ("EditorTrackNameDisplay");
		name_entry->signal_key_press_event().connect (sigc::mem_fun (*this, &TimeAxisView::name_entry_key_press), false);
		name_entry->signal_key_release_event().connect (sigc::mem_fun (*this, &TimeAxisView::name_entry_key_release), false);
		name_entry->signal_focus_out_event().connect (sigc::mem_fun (*this, &TimeAxisView::name_entry_focus_out));
		name_entry->set_text (name_label.get_text());
		name_entry->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &TimeAxisView::end_name_edit), RESPONSE_OK));

		if (name_label.is_ancestor (name_hbox)) {
			name_hbox.remove (name_label);
		}
		
		name_hbox.pack_start (*name_entry);
		name_entry->show ();

		name_entry->select_region (0, -1);
		name_entry->set_state (STATE_SELECTED);
		name_entry->grab_focus ();
		name_entry->start_editing (0);
	}
}

void
TimeAxisView::end_name_edit (int response)
{
	if (!name_entry) {
		return;
	}
	
	bool edit_next = false;
	bool edit_prev = false;

	switch (response) {
	case RESPONSE_CANCEL:
		break;
	case RESPONSE_OK:
		name_entry_changed ();
		break;
	case RESPONSE_ACCEPT:
		name_entry_changed ();
		edit_next = true;
	case RESPONSE_APPLY:
		name_entry_changed ();
		edit_prev = true;
	}

	/* this will delete the name_entry. but it will also drop focus, which
	 * will cause another callback to this function, so set name_entry = 0
	 * first to ensure we don't double-remove etc. etc.
	 */

	Gtk::Entry* tmp = name_entry;
	name_entry = 0;
	name_hbox.remove (*tmp);

	/* put the name label back */

	name_hbox.pack_start (name_label);
	name_label.show ();

	if (edit_next) {

		TrackViewList const & allviews = _editor.get_track_views ();
		TrackViewList::const_iterator i = find (allviews.begin(), allviews.end(), this);
		
		if (i != allviews.end()) {
			
			do {
				if (++i == allviews.end()) {
					return;
				}
				
				RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*>(*i);
			
				if (rtav && rtav->route()->record_enabled()) {
					continue;
				}
				
				if (!(*i)->hidden()) {
					break;
				}
				
			} while (true);
		}

		if ((i != allviews.end()) && (*i != this) && !(*i)->hidden()) {
			_editor.ensure_time_axis_view_is_visible (**i);
			(*i)->begin_name_edit ();
		} 

	} else if (edit_prev) {

		TrackViewList const & allviews = _editor.get_track_views ();
		TrackViewList::const_iterator i = find (allviews.begin(), allviews.end(), this);
		
		if (i != allviews.begin()) {
			do {
				if (i == allviews.begin()) {
					return;
				}
				
				--i;
				
				RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*>(*i);
				
				if (rtav && rtav->route()->record_enabled()) {
					continue;
				}
				
				if (!(*i)->hidden()) {
					break;
				}
				
			} while (true);
		}
		
		if ((i != allviews.end()) && (*i != this) && !(*i)->hidden()) {
			_editor.ensure_time_axis_view_is_visible (**i);
			(*i)->begin_name_edit ();
		} 
	}
}

void
TimeAxisView::name_entry_changed ()
{
}

bool
TimeAxisView::can_edit_name () const
{
	return true;
}

void
TimeAxisView::conditionally_add_to_selection ()
{
	Selection& s (_editor.get_selection ());

	if (!s.selected (this)) {
		_editor.set_selected_track (*this, Selection::Set);
	}
}

void
TimeAxisView::popup_display_menu (guint32 when)
{
	conditionally_add_to_selection ();

	build_display_menu ();
	display_menu->popup (1, when);
}

void
TimeAxisView::set_selected (bool yn)
{
	if (yn == _selected) {
		return;
	}

	Selectable::set_selected (yn);

	if (_selected) {
		controls_ebox.set_name (controls_base_selected_name);
		time_axis_vbox.set_name (controls_base_selected_name);
		controls_vbox.set_name (controls_base_selected_name);
	} else {
		controls_ebox.set_name (controls_base_unselected_name);
		time_axis_vbox.set_name (controls_base_unselected_name);
		controls_vbox.set_name (controls_base_unselected_name);
		hide_selection ();

		/* children will be set for the yn=true case. but when deselecting
		   the editor only has a list of top-level trackviews, so we
		   have to do this here.
		*/

		for (Children::iterator i = children.begin(); i != children.end(); ++i) {
			(*i)->set_selected (false);
		}
	}
}

void
TimeAxisView::build_display_menu ()
{
	using namespace Menu_Helpers;

	delete display_menu;

	display_menu = new Menu;
	display_menu->set_name ("ArdourContextMenu");

	// Just let implementing classes define what goes into the manu
}

void
TimeAxisView::set_samples_per_pixel (double fpp)
{
	for (Children::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->set_samples_per_pixel (fpp);
	}
}

void
TimeAxisView::show_timestretch (framepos_t start, framepos_t end, int layers, int layer)
{
	for (Children::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->show_timestretch (start, end, layers, layer);
	}
}

void
TimeAxisView::hide_timestretch ()
{
	for (Children::iterator i = children.begin(); i != children.end(); ++i) {
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

	for (Children::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->show_selection (ts);
	}

	if (selection_group->visible ()) {
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
		framepos_t start, end;
		framecnt_t cnt;

		start = (*i).start;
		end = (*i).end;
		cnt = end - start + 1;

		rect = get_selection_rect ((*i).id);

		x1 = _editor.sample_to_pixel (start);
		x2 = _editor.sample_to_pixel (start + cnt - 1);
		y2 = current_height();

		rect->rect->set (ArdourCanvas::Rect (x1, 1, x2, y2));

		// trim boxes are at the top for selections

		if (x2 > x1) {
			rect->start_trim->set (ArdourCanvas::Rect (x1, 1, x1 + trim_handle_size, y2));
			rect->end_trim->set (ArdourCanvas::Rect (x2 - trim_handle_size, 1, x2, y2));

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

	for (Children::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->show_selection (ts);
	}
}

void
TimeAxisView::hide_selection ()
{
	if (selection_group->visible ()) {
		while (!used_selection_rects.empty()) {
			free_selection_rects.push_front (used_selection_rects.front());
			used_selection_rects.pop_front();
			free_selection_rects.front()->rect->hide();
			free_selection_rects.front()->start_trim->hide();
			free_selection_rects.front()->end_trim->hide();
		}
		selection_group->hide();
	}

	for (Children::iterator i = children.begin(); i != children.end(); ++i) {
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

		rect->rect = new ArdourCanvas::Rectangle (selection_group);
		rect->rect->set_outline_what (0);
		rect->rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_SelectionRect());

		rect->start_trim = new ArdourCanvas::Rectangle (selection_group);
		rect->start_trim->set_outline_what (0);

		rect->end_trim = new ArdourCanvas::Rectangle (selection_group);
		rect->end_trim->set_outline_what (0);

		free_selection_rects.push_front (rect);

		rect->rect->Event.connect (sigc::bind (sigc::mem_fun (_editor, &PublicEditor::canvas_selection_rect_event), rect->rect, rect));
		rect->start_trim->Event.connect (sigc::bind (sigc::mem_fun (_editor, &PublicEditor::canvas_selection_start_trim_event), rect->rect, rect));
		rect->end_trim->Event.connect (sigc::bind (sigc::mem_fun (_editor, &PublicEditor::canvas_selection_end_trim_event), rect->rect, rect));
	}

	rect = free_selection_rects.front();
	rect->id = id;
	free_selection_rects.pop_front();
	return rect;
}

struct null_deleter { void operator()(void const *) const {} };

bool
TimeAxisView::is_child (TimeAxisView* tav)
{
	return find (children.begin(), children.end(), boost::shared_ptr<TimeAxisView>(tav, null_deleter())) != children.end();
}

void
TimeAxisView::add_child (boost::shared_ptr<TimeAxisView> child)
{
	children.push_back (child);
}

void
TimeAxisView::remove_child (boost::shared_ptr<TimeAxisView> child)
{
	Children::iterator i;

	if ((i = find (children.begin(), children.end(), child)) != children.end()) {
		children.erase (i);
	}
}

/** Get selectable things within a given range.
 *  @param start Start time in session frames.
 *  @param end End time in session frames.
 *  @param top Top y range, in trackview coordinates (ie 0 is the top of the track view)
 *  @param bot Bottom y range, in trackview coordinates (ie 0 is the top of the track view)
 *  @param result Filled in with selectable things.
 */
void
TimeAxisView::get_selectables (framepos_t /*start*/, framepos_t /*end*/, double /*top*/, double /*bot*/, list<Selectable*>& /*result*/)
{
	return;
}

void
TimeAxisView::get_inverted_selectables (Selection& /*sel*/, list<Selectable*>& /*result*/)
{
	return;
}

void
TimeAxisView::add_ghost (RegionView* rv)
{
	GhostRegion* gr = rv->add_ghost (*this);

	if (gr) {
		ghosts.push_back(gr);
	}
}

void
TimeAxisView::remove_ghost (RegionView* rv)
{
	rv->remove_ghost_in (*this);
}

void
TimeAxisView::erase_ghost (GhostRegion* gr)
{
	if (in_destructor) {
		return;
	}

	for (list<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		if ((*i) == gr) {
			ghosts.erase (i);
			break;
		}
	}
}

bool
TimeAxisView::touched (double top, double bot)
{
	/* remember: this is X Window - coordinate space starts in upper left and moves down.
	  y_position is the "origin" or "top" of the track.
	*/

	double mybot = _y_position + current_height();

	return ((_y_position <= bot && _y_position >= top) ||
		((mybot <= bot) && (top < mybot)) ||
		(mybot >= bot && _y_position < top));
}

void
TimeAxisView::set_parent (TimeAxisView& p)
{
	parent = &p;
}

void
TimeAxisView::reset_height ()
{
	set_height (height);

	for (Children::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->set_height ((*i)->height);
	}
}

void
TimeAxisView::compute_heights ()
{
	Gtk::Window window (Gtk::WINDOW_TOPLEVEL);
	Gtk::Table two_row_table (2, 8);
	Gtk::Table one_row_table (1, 8);
	Button* buttons[5];
	const int border_width = 2;

	const int separator_height = 2;
	extra_height = (2 * border_width) + separator_height;

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
	button_height = req.height;
}

void
TimeAxisView::color_handler ()
{
	for (list<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); i++) {
		(*i)->set_colors();
	}

	for (list<SelectionRect*>::iterator i = used_selection_rects.begin(); i != used_selection_rects.end(); ++i) {

		(*i)->rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_SelectionRect());
		(*i)->rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_Selection());

		(*i)->start_trim->set_fill_color (ARDOUR_UI::config()->get_canvasvar_Selection());
		(*i)->start_trim->set_outline_color (ARDOUR_UI::config()->get_canvasvar_Selection());
		
		(*i)->end_trim->set_fill_color (ARDOUR_UI::config()->get_canvasvar_Selection());
		(*i)->end_trim->set_outline_color (ARDOUR_UI::config()->get_canvasvar_Selection());
	}
	
	for (list<SelectionRect*>::iterator i = free_selection_rects.begin(); i != free_selection_rects.end(); ++i) {
		
		(*i)->rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_SelectionRect());
		(*i)->rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_Selection());
		
		(*i)->start_trim->set_fill_color (ARDOUR_UI::config()->get_canvasvar_Selection());
		(*i)->start_trim->set_outline_color (ARDOUR_UI::config()->get_canvasvar_Selection());
		
		(*i)->end_trim->set_fill_color (ARDOUR_UI::config()->get_canvasvar_Selection());
		(*i)->end_trim->set_outline_color (ARDOUR_UI::config()->get_canvasvar_Selection());
	}
}

/** @return Pair: TimeAxisView, layer index.
 * TimeAxisView is non-0 if this object covers y, or one of its children does.
 * If the covering object is a child axis, then the child is returned.
 * TimeAxisView is 0 otherwise.
 * Layer index is the layer number (possibly fractional) if the TimeAxisView is valid
 * and is in stacked or expanded * region display mode, otherwise 0.
 */
std::pair<TimeAxisView*, double>
TimeAxisView::covers_y_position (double y)
{
	if (hidden()) {
		return std::make_pair ((TimeAxisView *) 0, 0);
	}

	if (_y_position <= y && y < (_y_position + height)) {

		/* work out the layer index if appropriate */
		double l = 0;
		switch (layer_display ()) {
		case Overlaid:
			break;
		case Stacked:
			if (view ()) {
				/* compute layer */
				l = layer_t ((_y_position + height - y) / (view()->child_height ()));
				/* clamp to max layers to be on the safe side; sometimes the above calculation
				   returns a too-high value */
				if (l >= view()->layers ()) {
					l = view()->layers() - 1;
				}
			}
			break;
		case Expanded:
			if (view ()) {
				int const n = floor ((_y_position + height - y) / (view()->child_height ()));
				l = n * 0.5 - 0.5;
				if (l >= (view()->layers() - 0.5)) {
					l = view()->layers() - 0.5;
				}
			}
			break;
		}

		return std::make_pair (this, l);
	}

	for (Children::const_iterator i = children.begin(); i != children.end(); ++i) {

		std::pair<TimeAxisView*, int> const r = (*i)->covers_y_position (y);
		if (r.first) {
			return r;
		}
	}

	return std::make_pair ((TimeAxisView *) 0, 0);
}


uint32_t
TimeAxisView::preset_height (Height h)
{
	switch (h) {
	case HeightLargest:
		return (button_height * 2) + extra_height + 260;
	case HeightLarger:
		return (button_height * 2) + extra_height + 160;
	case HeightLarge:
		return (button_height * 2) + extra_height + 60;
	case HeightNormal:
		return (button_height * 2) + extra_height + 10;
	case HeightSmall:
		return button_height + extra_height;
	}

	/* NOTREACHED */
	return 0;
}

/** @return Child time axis views that are not hidden */
TimeAxisView::Children
TimeAxisView::get_child_list ()
{
	Children c;

	for (Children::iterator i = children.begin(); i != children.end(); ++i) {
		if (!(*i)->hidden()) {
			c.push_back(*i);
		}
	}

	return c;
}

void
TimeAxisView::build_size_menu ()
{
	if (_size_menu && _size_menu->gobj ()) {
		return;
	}

	delete _size_menu;

	using namespace Menu_Helpers;

	_size_menu = new Menu;
	_size_menu->set_name ("ArdourContextMenu");
	MenuList& items = _size_menu->items();

	items.push_back (MenuElem (_("Largest"), sigc::bind (sigc::mem_fun (*this, &TimeAxisView::set_height_enum), HeightLargest, true)));
	items.push_back (MenuElem (_("Larger"),  sigc::bind (sigc::mem_fun (*this, &TimeAxisView::set_height_enum), HeightLarger, true)));
	items.push_back (MenuElem (_("Large"),   sigc::bind (sigc::mem_fun (*this, &TimeAxisView::set_height_enum), HeightLarge, true)));
	items.push_back (MenuElem (_("Normal"),  sigc::bind (sigc::mem_fun (*this, &TimeAxisView::set_height_enum), HeightNormal, true)));
	items.push_back (MenuElem (_("Small"),   sigc::bind (sigc::mem_fun (*this, &TimeAxisView::set_height_enum), HeightSmall, true)));
}

void
TimeAxisView::reset_visual_state ()
{
	/* this method is not required to trigger a global redraw */

	string str = gui_property ("height");
	
	if (!str.empty()) {
		set_height (atoi (str));
	} else {
		set_height (preset_height (HeightNormal));
	}
}

TrackViewList
TrackViewList::filter_to_unique_playlists ()
{
	std::set<boost::shared_ptr<ARDOUR::Playlist> > playlists;
	TrackViewList ts;

	for (iterator i = begin(); i != end(); ++i) {
		RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (*i);
		if (!rtav) {
			/* not a route: include it anyway */
			ts.push_back (*i);
		} else {
			boost::shared_ptr<ARDOUR::Track> t = rtav->track();
			if (t) {
				if (playlists.insert (t->playlist()).second) {
					/* playlist not seen yet */
					ts.push_back (*i);
				}
			} else {
				/* not a track: include it anyway */
				ts.push_back (*i);
			}
		}
	}
	return ts;
}
