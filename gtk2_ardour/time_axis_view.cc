/*
 * Copyright (C) 2005-2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2005-2008 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2005-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2008-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2015 Ben Loftis <ben@harrisonconsoles.com>
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

#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <string>
#include <list>

#include <boost/smart_ptr/scoped_ptr.hpp>

#include <gtkmm/separator.h>

#include "pbd/error.h"
#include "pbd/convert.h"
#include "pbd/unwind.h"

#include "ardour/profile.h"

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/doi.h"
#include "gtkmm2ext/utils.h"

#include "canvas/canvas.h"
#include "canvas/rectangle.h"
#include "canvas/debug.h"
#include "canvas/utils.h"

#include "widgets/tooltips.h"

#include "ardour_dialog.h"
#include "audio_time_axis.h"
#include "floating_text_entry.h"
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
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace ArdourCanvas;
using namespace ArdourWidgets;
using Gtkmm2ext::Keyboard;

#define TOP_LEVEL_WIDGET controls_ebox

const double trim_handle_size = 6.0; /* pixels */
uint32_t TimeAxisView::button_height = 0;
uint32_t TimeAxisView::extra_height = 0;
int const TimeAxisView::_max_order = 512;
unsigned int TimeAxisView::name_width_px = 100;
PBD::Signal1<void,TimeAxisView*> TimeAxisView::CatchDeletion;
Glib::RefPtr<Gtk::SizeGroup> TimeAxisView::controls_meters_size_group = Glib::RefPtr<Gtk::SizeGroup>();
Glib::RefPtr<Gtk::SizeGroup> TimeAxisView::midi_scroomer_size_group = Glib::RefPtr<Gtk::SizeGroup>();

void
TimeAxisView::setup_sizes()
{
	name_width_px = ceilf (100.f * UIConfiguration::instance().get_ui_scale());
}

TimeAxisView::TimeAxisView (ARDOUR::Session* sess, PublicEditor& ed, TimeAxisView* rent, Canvas& /*canvas*/)
	: controls_table (5, 4)
	, controls_button_size_group (Gtk::SizeGroup::create (Gtk::SIZE_GROUP_BOTH))
	, _name_editing (false)
	, height (0)
	, display_menu (0)
	, parent (rent)
	, selection_group (0)
	, _ghost_group (0)
	, _hidden (true)
	, in_destructor (false)
	, _size_menu (0)
	, _canvas_display (0)
	, _y_position (0)
	, _editor (ed)
	, control_parent (0)
	, _order (0)
	, _effective_height (0)
	, _resize_drag_start (-1)
	, _did_resize (false)
	, _preresize_cursor (0)
	, _have_preresize_cursor (false)
	, _ebox_release_can_act (true)
{
	if (!controls_meters_size_group) {
		controls_meters_size_group = SizeGroup::create (SIZE_GROUP_HORIZONTAL);
	}
	if (!midi_scroomer_size_group) {
		midi_scroomer_size_group = SizeGroup::create (SIZE_GROUP_HORIZONTAL);
	}
	if (extra_height == 0) {
		compute_heights ();
	}

	_canvas_display = new ArdourCanvas::Container (ed.get_trackview_group ());
	CANVAS_DEBUG_NAME (_canvas_display, "main for TAV");
	_canvas_display->hide(); // reveal as needed

	_canvas_separator = new ArdourCanvas::Line(_canvas_display);
	CANVAS_DEBUG_NAME (_canvas_separator, "separator for TAV");
	_canvas_separator->set (ArdourCanvas::Duple(0.0, 0.0), ArdourCanvas::Duple(ArdourCanvas::COORD_MAX, 0.0));
	_canvas_separator->set_outline_color(Gtkmm2ext::rgba_to_color (0, 0, 0, 1.0));
	_canvas_separator->set_outline_width(1.0);
	_canvas_separator->hide();

	selection_group = new ArdourCanvas::Container (_canvas_display);
	CANVAS_DEBUG_NAME (selection_group, "selection for TAV");
	selection_group->set_data (X_("timeselection"), (void *) 1);
	selection_group->hide();

	_ghost_group = new ArdourCanvas::Container (_canvas_display);
	CANVAS_DEBUG_NAME (_ghost_group, "ghost for TAV");
	_ghost_group->lower_to_bottom();
	_ghost_group->show();

	name_label.set_name (X_("TrackNameEditor"));
	name_label.set_alignment (0.0, 0.5);
	name_label.set_width_chars (12);
	set_tooltip (name_label, _("Track/Bus name (double click to edit)"));

	inactive_label.set_name (X_("TrackNameEditor"));
	inactive_label.set_alignment (0.0, 0.5);
	set_tooltip (inactive_label, _("This track is inactive. (right-click to activate)"));

	{
		boost::scoped_ptr<Gtk::Entry> an_entry (new FocusEntry);
		an_entry->set_name (X_("TrackNameEditor"));
		Gtk::Requisition req = an_entry->size_request ();

		name_label.set_size_request (-1, req.height);
		set_name_ellipsize_mode ();
	}

	// set min. track-header width if fader is not visible
	name_label.set_size_request(name_width_px, -1);

	name_label.show ();
	inactive_label.show ();

	controls_table.set_row_spacings (2);
	controls_table.set_col_spacings (2);
	controls_table.set_border_width (2);

	if (ARDOUR::Profile->get_mixbus() ) {
		controls_table.attach (name_label, 4, 5, 0, 1,  Gtk::FILL|Gtk::EXPAND, Gtk::SHRINK, 0, 0);
	} else {
		controls_table.attach (name_label, 1, 2, 0, 1,  Gtk::FILL|Gtk::EXPAND, Gtk::SHRINK, 0, 0);
	}

	controls_table.show_all ();
	controls_table.set_no_show_all ();

	inactive_table.set_no_show_all ();
	inactive_table.set_border_width (4);  //try to match the offset of the label on an "active" track
	inactive_table.attach (inactive_label, 1, 2, 0, 1,  Gtk::FILL|Gtk::EXPAND, Gtk::SHRINK, 0, 0);
	controls_vbox.pack_start (inactive_table, false, false);

	controls_vbox.pack_start (controls_table, false, false);
	controls_vbox.show ();

	top_hbox.pack_start (controls_vbox, true, true);
	top_hbox.show ();

	controls_ebox.add (time_axis_hbox);
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

	time_axis_frame.set_shadow_type (Gtk::SHADOW_NONE);
	time_axis_frame.add(top_hbox);
	time_axis_frame.show();

	HSeparator* separator = manage (new HSeparator());
	separator->set_name("TrackSeparator");
	separator->set_size_request(-1, 1);
	separator->show();

	scroomer_placeholder.set_size_request (-1, -1);
	scroomer_placeholder.show();
	midi_scroomer_size_group->add_widget (scroomer_placeholder);

	time_axis_vbox.pack_start (*separator, false, false);
	time_axis_vbox.pack_start (time_axis_frame, true, true);
	time_axis_vbox.show();
	time_axis_hbox.pack_start (time_axis_vbox, true, true);
	time_axis_hbox.show();
	top_hbox.pack_start (scroomer_placeholder, false, false); // OR pack_end to move after meters ?

	UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &TimeAxisView::color_handler));
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &TimeAxisView::parameter_changed));
}

TimeAxisView::~TimeAxisView()
{
	in_destructor = true;

	for (list<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		delete *i;
	}

	for (list<SelectionRect*>::iterator i = free_selection_rects.begin(); i != free_selection_rects.end(); ++i) {
		delete (*i)->rect; (*i)->rect=0;
		delete (*i)->start_trim; (*i)->start_trim = 0;
		delete (*i)->end_trim; (*i)->end_trim = 0;

	}

	for (list<SelectionRect*>::iterator i = used_selection_rects.begin(); i != used_selection_rects.end(); ++i) {
		delete (*i)->rect; (*i)->rect = 0;
		delete (*i)->start_trim; (*i)->start_trim = 0;
		delete (*i)->end_trim; (*i)->end_trim = 0;
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
	_canvas_separator->hide ();

	if (control_parent) {
		control_parent->remove (TOP_LEVEL_WIDGET);
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
*
* @return height of this TimeAxisView.
*/
guint32
TimeAxisView::show_at (double y, int& nth, VBox *parent)
{
	if (control_parent) {
		control_parent->reorder_child (TOP_LEVEL_WIDGET, nth);
	} else {
		control_parent = parent;
		parent->pack_start (TOP_LEVEL_WIDGET, false, false);
		parent->reorder_child (TOP_LEVEL_WIDGET, nth);
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

	for (list<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		(*i)->set_height ();
	}

	/* put separator at the bottom of this time axis view */

	_canvas_separator->set (ArdourCanvas::Duple(0, height), ArdourCanvas::Duple(ArdourCanvas::COORD_MAX, height));
	_canvas_separator->lower_to_bottom ();
	_canvas_separator->show ();

	return _effective_height;
}

bool
TimeAxisView::controls_ebox_scroll (GdkEventScroll* ev)
{
	switch (ev->direction) {
	case GDK_SCROLL_UP:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::ScrollZoomVerticalModifier)) {
			/* See Editor::_stepping_axis_view for notes on this hack */
			Editor& e = dynamic_cast<Editor&> (_editor);
			if (!e.stepping_axis_view ()) {
				e.set_stepping_axis_view (this);
			}
			e.stepping_axis_view()->step_height (false);
			return true;
		}
		break;

	case GDK_SCROLL_DOWN:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::ScrollZoomVerticalModifier)) {
			/* See Editor::_stepping_axis_view for notes on this hack */
			Editor& e = dynamic_cast<Editor&> (_editor);
			if (!e.stepping_axis_view ()) {
				e.set_stepping_axis_view (this);
			}
			e.stepping_axis_view()->step_height (true);
			return true;
		}
		break;

	default:
		/* no handling for left/right, yet */
		break;
	}

	/* Just forward to the normal canvas scroll method. The coordinate
	   systems are different but since the canvas is always larger than the
	   track headers, and aligned with the trackview area, this will work.

	   In the not too distant future this layout is going away anyway and
	   headers will be on the canvas.
	*/
	return _editor.canvas_scroll_event (ev, false);
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

	if (event->button == 1 && event->type == GDK_2BUTTON_PRESS) {
		if (_effective_height < preset_height (HeightLargest)) {
			set_height_enum (HeightLargest);
		} else {
			set_height_enum (HeightNormal);
		}
	}

	_ebox_release_can_act = true;

	if (maybe_set_cursor (event->y) > 0) {
		_resize_drag_start = event->y_root;
	}

	return true;
}

void
TimeAxisView::idle_resize (int32_t h)
{
	set_height (std::max(0, h));
}


bool
TimeAxisView::controls_ebox_motion (GdkEventMotion* ev)
{
	if (_resize_drag_start >= 0) {

		/* (ab)use the DragManager to do autoscrolling - basically we
		 * are pretending that the drag is taking place over the canvas
		 * (which perhaps in the glorious future, when track headers
		 * and the canvas are unified, will actually be true.)
		 */

		_editor.maybe_autoscroll (false, true, true);

		/* now schedule the actual TAV resize */
		int32_t const delta = (int32_t) floor (ev->y_root - _resize_drag_start);
		_editor.add_to_idle_resize (this, delta);
		_resize_drag_start = ev->y_root;
		_did_resize = true;
	} else {
		/* not dragging but ... */
		maybe_set_cursor (ev->y);
	}

	gdk_event_request_motions(ev);
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
		if (_did_resize) {
			_did_resize = false;
			// don't change selection
			return true;
		}
	}

	if (!_ebox_release_can_act) {
		return true;
	}

	switch (ev->button) {
	case 1:
		if (selectable()) {
			selection_click (ev);
		}
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
TimeAxisView::set_height (uint32_t h, TrackHeightMode m)
{
	uint32_t lanes = 0;
	if (m == TotalHeight) {
		for (Children::iterator i = children.begin(); i != children.end(); ++i) {
			if (!(*i)->hidden()) {
				++lanes;
			}
		}
	}
	h /= (lanes + 1);

	if (h < preset_height (HeightSmall)) {
		h = preset_height (HeightSmall);
	}

	TOP_LEVEL_WIDGET.property_height_request () = h;
	height = h;

	set_gui_property ("height", height);

	for (list<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		(*i)->set_height ();
	}

	if (selection_group->visible ()) {
		/* resize the selection rect */
		show_selection (_editor.get_selection().time);
	}

	if (m != OnlySelf) {
		for (Children::iterator i = children.begin(); i != children.end(); ++i) {
			(*i)->set_height(h, OnlySelf);
		}
	}

	_editor.override_visible_track_count ();
}

void
TimeAxisView::begin_name_edit ()
{
	if (!can_edit_name()) {
		return;
	}

	Gtk::Window* toplevel = (Gtk::Window*) control_parent->get_toplevel();
	FloatingTextEntry* fte = new FloatingTextEntry (toplevel, name ());

	fte->set_name ("TrackNameEditor");
	fte->use_text.connect (sigc::mem_fun (*this, &TimeAxisView::end_name_edit));

	/* We want to new toplevel window to overlay the name label, so
	 * translate the coordinates of the upper left corner of the name label
	 * into the coordinate space of the top level window.
	 */

	int x, y;
	int wx, wy;

	name_label.translate_coordinates (*toplevel, 0, 0, x, y);
	toplevel->get_window()->get_origin (wx, wy);

	fte->move (wx + x, wy + y);
	fte->present ();
}

void
TimeAxisView::end_name_edit (std::string str, int next_dir)
{
	if (!name_entry_changed (str)) {
		next_dir = 0;
	}

	if (next_dir > 0) {

		TrackViewList const & allviews = _editor.get_track_views ();
		TrackViewList::const_iterator i = find (allviews.begin(), allviews.end(), this);

		if (i != allviews.end()) {

			do {
				if (++i == allviews.end()) {
					return;
				}

				RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*>(*i);

				if (rtav && rtav->is_track() && rtav->track()->rec_enable_control()->get_value()) {
					continue;
				}

				if (!(*i)->hidden()) {
					break;
				}

			} while (true);
		}

		if ((i != allviews.end()) && (*i != this) && !(*i)->hidden()) {
			_editor.ensure_time_axis_view_is_visible (**i, false);
			(*i)->begin_name_edit ();
		}

	} else if (next_dir < 0) {

		TrackViewList const & allviews = _editor.get_track_views ();
		TrackViewList::const_iterator i = find (allviews.begin(), allviews.end(), this);

		if (i != allviews.begin()) {
			do {
				if (i == allviews.begin()) {
					return;
				}

				--i;

				RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*>(*i);

				if (rtav && rtav->is_track() && rtav->track()->rec_enable_control()->get_value()) {
					continue;
				}

				if (!(*i)->hidden()) {
					break;
				}

			} while (true);
		}

		if ((i != allviews.end()) && (*i != this) && !(*i)->hidden()) {
			_editor.ensure_time_axis_view_is_visible (**i, false);
			(*i)->begin_name_edit ();
		}
	}
}

bool
TimeAxisView::name_entry_changed (string const&)
{
	return true;
}

bool
TimeAxisView::can_edit_name () const
{
	return true;
}

void
TimeAxisView::conditionally_add_to_selection ()
{
	if (!selectable()) {
		return;
	}

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

	if (!display_menu->items().empty()) {
		display_menu->popup (1, when);
	}
}

void
TimeAxisView::set_selected (bool yn)
{
	if (yn == selected()) {
		return;
	}

	AxisView::set_selected (yn);

	if (_selected) {
		time_axis_frame.set_shadow_type (Gtk::SHADOW_IN);
		time_axis_frame.set_name ("MixerStripSelectedFrame");
		controls_ebox.set_name (controls_base_selected_name);
		controls_vbox.set_name (controls_base_selected_name);
		time_axis_vbox.set_name (controls_base_selected_name);
	} else {
		time_axis_frame.set_shadow_type (Gtk::SHADOW_NONE);
		time_axis_frame.set_name (controls_base_unselected_name);
		controls_ebox.set_name (controls_base_unselected_name);
		controls_vbox.set_name (controls_base_unselected_name);
		time_axis_vbox.set_name (controls_base_unselected_name);

		hide_selection ();
	}

	time_axis_frame.show();
}

void
TimeAxisView::build_display_menu ()
{
	using namespace Menu_Helpers;

	if (_size_menu) {
		Gtkmm2ext::detach_menu (*_size_menu);
	}

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
TimeAxisView::show_timestretch (timepos_t const & start, timepos_t const & end, int layers, int layer)
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

	time_axis_frame.show();

	for (Children::iterator i = children.begin(); i != children.end(); ++i) {
		if (!(*i)->selected () && !(*i)->propagate_time_selection ()) {
			continue;
		}
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

	uint32_t gap = UIConfiguration::instance().get_vertical_region_gap ();
	float ui_scale = UIConfiguration::instance().get_ui_scale ();
	if (gap > 0 && ui_scale > 0) {
		gap = ceil (gap * ui_scale);
	}

	for (list<TimelineRange>::iterator i = ts.begin(); i != ts.end(); ++i) {
		timepos_t start, end;
		timecnt_t cnt;

		start = (*i).start();
		end = (*i).end();
		cnt = start.distance (end); /* XXX NUTEMPO used to add 1 here */

		rect = get_selection_rect ((*i).id);

		x1 = _editor.time_to_pixel (start);
		x2 = _editor.time_to_pixel (end.decrement());
		y2 = current_height() - 1;

		if (dynamic_cast<AudioTimeAxisView*>(this)) {
			if (y2 > gap) {
				y2 -= gap;
			} else {
				y2 = 1;
			}
		}

		rect->rect->set (ArdourCanvas::Rect (x1, 0, x2, y2));

		// trim boxes are at the top for selections

		if (x2 > x1) {
			rect->start_trim->set (ArdourCanvas::Rect (x1, 0, x1 + trim_handle_size, y2));
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
		if (!(*i)->selected () && !(*i)->propagate_time_selection ()) {
			continue;
		}
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

// retuned rect is pushed back into the used_selection_rects list
// in TimeAxisView::show_selection() which is the only caller.
SelectionRect *
TimeAxisView::get_selection_rect (uint32_t id)
{
	SelectionRect *rect;

	/* check to see if we already have a visible rect for this particular selection ID */

	for (list<SelectionRect*>::iterator i = used_selection_rects.begin(); i != used_selection_rects.end(); ++i) {
		if ((*i)->id == id) {
			SelectionRect* ret = (*i);
			used_selection_rects.erase (i);
			return ret;
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
		CANVAS_DEBUG_NAME (rect->rect, "selection rect");
		rect->rect->set_outline (true);
		rect->rect->set_outline_width (1.0);
		rect->rect->set_outline_color (UIConfiguration::instance().color ("selection"));
		rect->rect->set_fill_color (UIConfiguration::instance().color_mod ("selection rect", "selection rect"));

		rect->start_trim = new ArdourCanvas::Rectangle (selection_group);
		CANVAS_DEBUG_NAME (rect->start_trim, "selection rect start trim");
		rect->start_trim->set_outline (false);
		rect->start_trim->set_fill (false);

		rect->end_trim = new ArdourCanvas::Rectangle (selection_group);
		CANVAS_DEBUG_NAME (rect->end_trim, "selection rect end trim");
		rect->end_trim->set_outline (false);
		rect->end_trim->set_fill (false);

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
 *  @param start Start time in session samples.
 *  @param end End time in session samples.
 *  @param top Top y range, in trackview coordinates (ie 0 is the top of the track view)
 *  @param bot Bottom y range, in trackview coordinates (ie 0 is the top of the track view)
 *  @param result Filled in with selectable things.
 */
void
TimeAxisView::get_selectables (timepos_t const & start, timepos_t const & end, double top, double bot, list<Selectable*>& results, bool within)
{
	for (Children::iterator i = children.begin(); i != children.end(); ++i) {
		if (!(*i)->hidden()) {
			(*i)->get_selectables (start, end, top, bot, results, within);
		}
	}
}

void
TimeAxisView::set_selected_points (PointSelection& points)
{
	for (Children::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->set_selected_points (points);
	}
}

void
TimeAxisView::get_inverted_selectables (Selection& sel, list<Selectable*>& results)
{
	for (Children::iterator i = children.begin(); i != children.end(); ++i) {
		if (!(*i)->hidden()) {
			(*i)->get_inverted_selectables (sel, results);
		}
	}
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
	// TODO this function should be re-evaluated when font-scaling changes (!)
	Gtk::Window window (Gtk::WINDOW_TOPLEVEL);
	Gtk::Table one_row_table (1, 1);
	ArdourButton* test_button = manage (new ArdourButton);
	const int border_width = 2;
	const int frame_height = 2;
	extra_height = (2 * border_width) + frame_height;

	window.add (one_row_table);
	test_button->set_name ("mute button");
	test_button->set_text (S_("Mute|M"));
	test_button->set_tweaks (ArdourButton::TrackHeader);

	one_row_table.set_border_width (border_width);
	one_row_table.set_row_spacings (2);
	one_row_table.set_col_spacings (2);

	one_row_table.attach (*test_button, 0, 1, 0, 1, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
	one_row_table.show_all ();

	Gtk::Requisition req(one_row_table.size_request ());
	button_height = req.height;
}

void
TimeAxisView::color_handler ()
{
	for (list<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); i++) {
		(*i)->set_colors();
	}

	for (list<SelectionRect*>::iterator i = used_selection_rects.begin(); i != used_selection_rects.end(); ++i) {

		(*i)->rect->set_fill_color (UIConfiguration::instance().color_mod ("selection rect", "selection rect"));
		(*i)->rect->set_outline_color (UIConfiguration::instance().color ("selection"));

		(*i)->start_trim->set_fill_color (UIConfiguration::instance().color ("selection"));
		(*i)->start_trim->set_outline_color (UIConfiguration::instance().color ("selection"));

		(*i)->end_trim->set_fill_color (UIConfiguration::instance().color ("selection"));
		(*i)->end_trim->set_outline_color (UIConfiguration::instance().color ("selection"));
	}

	for (list<SelectionRect*>::iterator i = free_selection_rects.begin(); i != free_selection_rects.end(); ++i) {

		(*i)->rect->set_fill_color (UIConfiguration::instance().color_mod ("selection rect", "selection rect"));
		(*i)->rect->set_outline_color (UIConfiguration::instance().color ("selection"));

		(*i)->start_trim->set_fill_color (UIConfiguration::instance().color ("selection"));
		(*i)->start_trim->set_outline_color (UIConfiguration::instance().color ("selection"));

		(*i)->end_trim->set_fill_color (UIConfiguration::instance().color ("selection"));
		(*i)->end_trim->set_outline_color (UIConfiguration::instance().color ("selection"));
	}
}

void
TimeAxisView::parameter_changed (string const & what_changed)
{
	if (what_changed == "vertical-region-gap") {
		if (selected ()) {
			show_selection (_editor.get_selection().time);
		}
	} else if (what_changed == "time-axis-name-ellipsize-mode") {
		set_name_ellipsize_mode ();
	}

	if (view()) {
		view()->parameter_changed (what_changed);
	}
}

/** @return Pair: TimeAxisView, layer index.
 * TimeAxisView is non-0 if this object covers @param y, or one of its children
 * does. @param y is an offset from the top of the trackview area.
 *
 * If the covering object is a child axis, then the child is returned.
 * TimeAxisView is 0 otherwise.
 *
 * Layer index is the layer number (possibly fractional) if the TimeAxisView is valid
 * and is in stacked or expanded * region display mode, otherwise 0.
 */
std::pair<TimeAxisView*, double>
TimeAxisView::covers_y_position (double y) const
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

		return std::make_pair (const_cast<TimeAxisView*>(this), l);
	}

	for (Children::const_iterator i = children.begin(); i != children.end(); ++i) {

		std::pair<TimeAxisView*, int> const r = (*i)->covers_y_position (y);
		if (r.first) {
			return r;
		}
	}

	return std::make_pair ((TimeAxisView *) 0, 0);
}

bool
TimeAxisView::covered_by_y_range (double y0, double y1) const
{
	if (hidden()) {
		return false;
	}

	/* if either the top or bottom of the axisview is in the vertical
	 * range, we cover it.
	 */

	if ((y0 < _y_position && y1 < _y_position) ||
	    (y0 >= _y_position + height && y1 >= _y_position + height)) {
		return false;
	}

	for (Children::const_iterator i = children.begin(); i != children.end(); ++i) {
		if ((*i)->covered_by_y_range (y0, y1)) {
			return true;
		}
	}

	return true;
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

	abort(); /* NOTREACHED */
	return 0;
}

/** @return Child time axis views that are not hidden */
TimeAxisView::Children
TimeAxisView::get_child_list () const
{
	Children c;

	for (Children::const_iterator i = children.begin(); i != children.end(); ++i) {
		if (!(*i)->hidden()) {
			c.push_back(*i);
		}
	}

	return c;
}

void
TimeAxisView::build_size_menu ()
{
	if (_size_menu) {
		return;
	}

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

	uint32_t height;
	if (get_gui_property ("height", height)) {
		set_height (height);
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
