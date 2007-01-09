/*
    Copyright (C) 2006 Paul Davis 

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
#include <cassert>

#include <algorithm>
#include <string>
#include <vector>

#include <sigc++/bind.h>

#include <pbd/error.h>
#include <pbd/stl_delete.h>
#include <pbd/whitespace.h>
#include <pbd/memento_command.h>

#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/bindable_button.h>
#include <gtkmm2ext/utils.h>

#include <ardour/playlist.h>
#include <ardour/audioplaylist.h>
#include <ardour/diskstream.h>
#include <ardour/insert.h>
#include <ardour/ladspa_plugin.h>
#include <ardour/location.h>
#include <ardour/panner.h>
#include <ardour/playlist.h>
#include <ardour/session.h>
#include <ardour/session_playlist.h>
#include <ardour/utils.h>

#include "ardour_ui.h"
#include "route_time_axis.h"
#include "automation_time_axis.h"
#include "redirect_automation_time_axis.h"
#include "redirect_automation_line.h"
#include "canvas_impl.h"
#include "crossfade_view.h"
#include "enums.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "playlist_selector.h"
#include "point_selection.h"
#include "prompter.h"
#include "public_editor.h"
#include "region_view.h"
#include "rgb_macros.h"
#include "selection.h"
#include "simplerect.h"
#include "streamview.h"
#include "utils.h"

#include <ardour/track.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Editing;


RouteTimeAxisView::RouteTimeAxisView (PublicEditor& ed, Session& sess, boost::shared_ptr<Route> rt, Canvas& canvas)
	: AxisView(sess),
	  RouteUI(rt, sess, _("m"), _("s"), _("r")), // mute, solo, and record
	  TimeAxisView(sess,ed,(TimeAxisView*) 0, canvas),
	  parent_canvas (canvas),
	  button_table (3, 3),
	  edit_group_button (_("g")), // group
	  playlist_button (_("p")), 
	  size_button (_("h")), // height
	  automation_button (_("a")),
	  visual_button (_("v"))

{
	_has_state = true;
	playlist_menu = 0;
	playlist_action_menu = 0;
	automation_action_menu = 0;
	_view = 0;
	timestretch_rect = 0;
	no_redraw = false;

	ignore_toggle = false;

	mute_button->set_active (false);
	solo_button->set_active (false);
	
	mute_button->set_name ("TrackMuteButton");
	solo_button->set_name ("SoloButton");
	edit_group_button.set_name ("TrackGroupButton");
	playlist_button.set_name ("TrackPlaylistButton");
	automation_button.set_name ("TrackAutomationButton");
	size_button.set_name ("TrackSizeButton");
	visual_button.set_name ("TrackVisualButton");
	hide_button.set_name ("TrackRemoveButton");

	hide_button.add (*(manage (new Image (::get_icon("hide")))));
	hide_button.show_all ();

 	edit_group_button.signal_button_release_event().connect (mem_fun(*this, &RouteTimeAxisView::edit_click), false);
	playlist_button.signal_clicked().connect (mem_fun(*this, &RouteTimeAxisView::playlist_click));
	automation_button.signal_clicked().connect (mem_fun(*this, &RouteTimeAxisView::automation_click));
	size_button.signal_button_release_event().connect (mem_fun(*this, &RouteTimeAxisView::size_click), false);
	visual_button.signal_clicked().connect (mem_fun(*this, &RouteTimeAxisView::visual_click));
	hide_button.signal_clicked().connect (mem_fun(*this, &RouteTimeAxisView::hide_click));

	solo_button->signal_button_press_event().connect (mem_fun(*this, &RouteUI::solo_press));
	solo_button->signal_button_release_event().connect (mem_fun(*this, &RouteUI::solo_release));
	mute_button->signal_button_press_event().connect (mem_fun(*this, &RouteUI::mute_press));
	mute_button->signal_button_release_event().connect (mem_fun(*this, &RouteUI::mute_release));

	if (is_track()) {

		/* use icon */

		rec_enable_button->remove ();
		switch (track()->mode()) {
		case ARDOUR::Normal:
			rec_enable_button->add (*(manage (new Image (::get_icon (X_("record_normal_red"))))));
			break;
		case ARDOUR::Destructive:
			rec_enable_button->add (*(manage (new Image (::get_icon (X_("record_tape_red"))))));
			break;
		}
		rec_enable_button->show_all ();

		rec_enable_button->set_name ("TrackRecordEnableButton");
		rec_enable_button->signal_button_press_event().connect (mem_fun(*this, &RouteUI::rec_enable_press));
		controls_table.attach (*rec_enable_button, 5, 6, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);
		ARDOUR_UI::instance()->tooltips().set_tip(*rec_enable_button, _("Record"));
	}

	controls_table.attach (*mute_button, 6, 7, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);
	controls_table.attach (*solo_button, 7, 8, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::FILL|Gtk::EXPAND, 0, 0);

	controls_table.attach (edit_group_button, 6, 7, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);

	ARDOUR_UI::instance()->tooltips().set_tip(*solo_button,_("Solo"));
	ARDOUR_UI::instance()->tooltips().set_tip(*mute_button,_("Mute"));
	ARDOUR_UI::instance()->tooltips().set_tip(edit_group_button,_("Edit Group"));
	ARDOUR_UI::instance()->tooltips().set_tip(size_button,_("Display Height"));
	ARDOUR_UI::instance()->tooltips().set_tip(playlist_button,_("Playlist"));
	ARDOUR_UI::instance()->tooltips().set_tip(automation_button, _("Automation"));
	ARDOUR_UI::instance()->tooltips().set_tip(visual_button, _("Visual options"));
	ARDOUR_UI::instance()->tooltips().set_tip(hide_button, _("Hide this track"));
	
	label_view ();

	controls_table.attach (hide_button, 0, 1, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	controls_table.attach (visual_button, 1, 2, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	controls_table.attach (size_button, 2, 3, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	controls_table.attach (automation_button, 3, 4, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);

	if (is_track() && track()->mode() == ARDOUR::Normal) {
		controls_table.attach (playlist_button, 5, 6, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	}

	/* remove focus from the buttons */
	
	automation_button.unset_flags (Gtk::CAN_FOCUS);
	solo_button->unset_flags (Gtk::CAN_FOCUS);
	mute_button->unset_flags (Gtk::CAN_FOCUS);
	edit_group_button.unset_flags (Gtk::CAN_FOCUS);
	size_button.unset_flags (Gtk::CAN_FOCUS);
	playlist_button.unset_flags (Gtk::CAN_FOCUS);
	hide_button.unset_flags (Gtk::CAN_FOCUS);
	visual_button.unset_flags (Gtk::CAN_FOCUS);

	y_position = -1;

	_route->mute_changed.connect (mem_fun(*this, &RouteUI::mute_changed));
	_route->solo_changed.connect (mem_fun(*this, &RouteUI::solo_changed));
	_route->redirects_changed.connect (mem_fun(*this, &RouteTimeAxisView::redirects_changed));
	_route->name_changed.connect (mem_fun(*this, &RouteTimeAxisView::route_name_changed));
	_route->solo_safe_changed.connect (mem_fun(*this, &RouteUI::solo_changed));


	if (is_track()) {

		track()->TrackModeChanged.connect (mem_fun(*this, &RouteTimeAxisView::track_mode_changed));
		track()->FreezeChange.connect (mem_fun(*this, &RouteTimeAxisView::map_frozen));
		track()->DiskstreamChanged.connect (mem_fun(*this, &RouteTimeAxisView::diskstream_changed));
		get_diskstream()->SpeedChanged.connect (mem_fun(*this, &RouteTimeAxisView::speed_changed));

		/* pick up the correct freeze state */
		map_frozen ();

	}

	editor.ZoomChanged.connect (mem_fun(*this, &RouteTimeAxisView::reset_samples_per_unit));
	ColorChanged.connect (mem_fun (*this, &RouteTimeAxisView::color_handler));
}

RouteTimeAxisView::~RouteTimeAxisView ()
{
	GoingAway (); /* EMIT_SIGNAL */

	vector_delete (&redirect_automation_curves);

	for (list<RedirectAutomationInfo*>::iterator i = redirect_automation.begin(); i != redirect_automation.end(); ++i) {
		delete *i;
	}

 	if (playlist_menu) {
 		delete playlist_menu;
 		playlist_menu = 0;
 	}
  
	if (playlist_action_menu) {
		delete playlist_action_menu;
		playlist_action_menu = 0;
	}

	if (_view) {
		delete _view;
		_view = 0;
	}
}

void
RouteTimeAxisView::post_construct ()
{
	/* map current state of the route */

	update_diskstream_display ();
	_route->foreach_redirect (this, &RouteTimeAxisView::add_redirect_to_subplugin_menu);
	_route->foreach_redirect (this, &RouteTimeAxisView::add_existing_redirect_automation_curves);
	reset_redirect_automation_curves ();
}

void
RouteTimeAxisView::set_playlist (boost::shared_ptr<Playlist> newplaylist)
{
	boost::shared_ptr<Playlist> pl = playlist();
	assert(pl);

	modified_connection.disconnect ();
	modified_connection = pl->Modified.connect (mem_fun(*this, &RouteTimeAxisView::playlist_modified));
}

void
RouteTimeAxisView::playlist_modified ()
{
}

gint
RouteTimeAxisView::edit_click (GdkEventButton *ev)
{
	if (Keyboard::modifier_state_equals (ev->state, Keyboard::Control)) {
	        _route->set_edit_group (0, this);
		return FALSE;
	} 

	using namespace Menu_Helpers;

	MenuList& items = edit_group_menu.items ();
	RadioMenuItem::Group group;

	items.clear ();
	items.push_back (RadioMenuElem (group, _("No group"), 
					bind (mem_fun(*this, &RouteTimeAxisView::set_edit_group_from_menu), (RouteGroup *) 0)));
	
	if (_route->edit_group() == 0) {
		static_cast<RadioMenuItem*>(&items.back())->set_active ();
	}
	
	_session.foreach_edit_group (bind (mem_fun (*this, &RouteTimeAxisView::add_edit_group_menu_item), &group));
	edit_group_menu.popup (ev->button, ev->time);

	return FALSE;
}

void
RouteTimeAxisView::add_edit_group_menu_item (RouteGroup *eg, RadioMenuItem::Group* group)
{
	using namespace Menu_Helpers;

	MenuList &items = edit_group_menu.items();

	items.push_back (RadioMenuElem (*group, eg->name(), bind (mem_fun(*this, &RouteTimeAxisView::set_edit_group_from_menu), eg)));
	if (_route->edit_group() == eg) {
		static_cast<RadioMenuItem*>(&items.back())->set_active ();
	}
}

void
RouteTimeAxisView::set_edit_group_from_menu (RouteGroup *eg)
{
	_route->set_edit_group (eg, this);
}

void
RouteTimeAxisView::playlist_changed ()

{
	label_view ();

	if (is_track()) {
		set_playlist (get_diskstream()->playlist());
	}
}

void
RouteTimeAxisView::label_view ()
{
	string x = _route->name();

	if (x != name_entry.get_text()) {
		name_entry.set_text (x);
	}

	ARDOUR_UI::instance()->tooltips().set_tip (name_entry, x);
}

void
RouteTimeAxisView::route_name_changed (void *src)
{
	editor.route_name_changed (this);
	label_view ();
}

void
RouteTimeAxisView::take_name_changed (void *src)

{
	if (src != this) {
		label_view ();
	}
}

void
RouteTimeAxisView::playlist_click ()
{
	// always build a new action menu
	
	if (playlist_action_menu != 0) {
		delete playlist_action_menu;
	} 

	playlist_action_menu = new Menu;
	playlist_action_menu->set_name ("ArdourContextMenu");
	
 	build_playlist_menu (playlist_action_menu);
	editor.set_selected_track (*this, Selection::Add);
	playlist_action_menu->popup (1, gtk_get_current_event_time());
}

void
RouteTimeAxisView::automation_click ()
{
	if (automation_action_menu == 0) {
		/* this seems odd, but the automation action
		   menu is built as part of the display menu.
		*/
		build_display_menu ();
	}
	editor.set_selected_track (*this, Selection::Add);
	automation_action_menu->popup (1, gtk_get_current_event_time());
}

void
RouteTimeAxisView::build_automation_action_menu ()
{
	using namespace Menu_Helpers;

	automation_action_menu = manage (new Menu);
	MenuList& automation_items = automation_action_menu->items();
	automation_action_menu->set_name ("ArdourContextMenu");
	
	automation_items.push_back (MenuElem (_("Show all automation"),
					      mem_fun(*this, &RouteTimeAxisView::show_all_automation)));

	automation_items.push_back (MenuElem (_("Show existing automation"),
					      mem_fun(*this, &RouteTimeAxisView::show_existing_automation)));

	automation_items.push_back (MenuElem (_("Hide all automation"),
					      mem_fun(*this, &RouteTimeAxisView::hide_all_automation)));

	automation_items.push_back (MenuElem (_("Plugins"), subplugin_menu));
}

void
RouteTimeAxisView::build_display_menu ()
{
	using namespace Menu_Helpers;

	/* get the size menu ready */

	build_size_menu ();

	/* prepare it */

	TimeAxisView::build_display_menu ();

	/* now fill it with our stuff */

	MenuList& items = display_menu->items();
	display_menu->set_name ("ArdourContextMenu");
	
	items.push_back (MenuElem (_("Height"), *size_menu));
	items.push_back (MenuElem (_("Color"), mem_fun(*this, &RouteTimeAxisView::select_track_color)));

	items.push_back (SeparatorElem());

	build_remote_control_menu ();
	items.push_back (MenuElem (_("Remote Control ID"), *remote_control_menu));

	build_automation_action_menu ();
	items.push_back (MenuElem (_("Automation"), *automation_action_menu));

	// Hook for derived classes to add type specific stuff
	items.push_back (SeparatorElem());
	append_extra_display_menu_items ();
	items.push_back (SeparatorElem());
	
	if (is_track()) {

		Menu* alignment_menu = manage (new Menu);
		MenuList& alignment_items = alignment_menu->items();
		alignment_menu->set_name ("ArdourContextMenu");

		RadioMenuItem::Group align_group;
		
		alignment_items.push_back (RadioMenuElem (align_group, _("Align with existing material"),
			bind (mem_fun(*this, &RouteTimeAxisView::set_align_style), ExistingMaterial)));
		align_existing_item = dynamic_cast<RadioMenuItem*>(&alignment_items.back());
		if (get_diskstream()->alignment_style() == ExistingMaterial)
			align_existing_item->set_active();
		
		alignment_items.push_back (RadioMenuElem (align_group, _("Align with capture time"),
			bind (mem_fun(*this, &RouteTimeAxisView::set_align_style), CaptureTime)));
		align_capture_item = dynamic_cast<RadioMenuItem*>(&alignment_items.back());
		if (get_diskstream()->alignment_style() == CaptureTime)
			align_capture_item->set_active();
		
		items.push_back (MenuElem (_("Alignment"), *alignment_menu));

		get_diskstream()->AlignmentStyleChanged.connect (
			mem_fun(*this, &RouteTimeAxisView::align_style_changed));

		RadioMenuItem::Group mode_group;
		items.push_back (RadioMenuElem (mode_group, _("Normal mode"),
						bind (mem_fun (*this, &RouteTimeAxisView::set_track_mode), ARDOUR::Normal)));
		normal_track_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());
		items.push_back (RadioMenuElem (mode_group, _("Tape mode"),
						bind (mem_fun (*this, &RouteTimeAxisView::set_track_mode), ARDOUR::Destructive)));
		destructive_track_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());
				 
		
		switch (track()->mode()) {
		case ARDOUR::Destructive:
			destructive_track_mode_item->set_active ();
			break;
		case ARDOUR::Normal:
			normal_track_mode_item->set_active ();
			break;
		}
	}

	items.push_back (SeparatorElem());
	items.push_back (CheckMenuElem (_("Active"), mem_fun(*this, &RouteUI::toggle_route_active)));
	route_active_menu_item = dynamic_cast<CheckMenuItem *> (&items.back());
	route_active_menu_item->set_active (_route->active());

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Remove"), mem_fun(*this, &RouteUI::remove_this_route)));
}

static bool __reset_item (RadioMenuItem* item)
{
	cerr << "reset item to true\n";
	item->set_active ();
	return false;
}

void
RouteTimeAxisView::set_track_mode (TrackMode mode)
{
	RadioMenuItem* item;
	RadioMenuItem* other_item;

	switch (mode) {
	case ARDOUR::Normal:
		item = normal_track_mode_item;
		other_item = destructive_track_mode_item;
		break;
	case ARDOUR::Destructive:
		item = destructive_track_mode_item;
		other_item = normal_track_mode_item;
		break;
	default:
		fatal << string_compose (_("programming error: %1 %2"), "illegal track mode in RouteTimeAxisView::set_track_mode", mode) << endmsg;
		/*NOTREACHED*/
		return;
	}

	if (item->get_active () && track()->mode() != mode) {
		_set_track_mode (track(), mode, other_item);
	}
}

void
RouteTimeAxisView::_set_track_mode (Track* track, TrackMode mode, RadioMenuItem* reset_item)
{
	bool needs_bounce;

	if (!track->can_use_mode (mode, needs_bounce)) {

		if (!needs_bounce) {
			/* cannot be done */
			Glib::signal_idle().connect (bind (sigc::ptr_fun (__reset_item), reset_item));
			return;
		} else {
			cerr << "would bounce this one\n";
			return;
		}
	}

	track->set_mode (mode);

	rec_enable_button->remove ();
	switch (mode) {
	case ARDOUR::Normal:
		rec_enable_button->add (*(manage (new Image (::get_icon (X_("record_normal_red"))))));
		break;
	case ARDOUR::Destructive:
		rec_enable_button->add (*(manage (new Image (::get_icon (X_("record_tape_red"))))));
		break;
	}
	rec_enable_button->show_all ();

}

void
RouteTimeAxisView::track_mode_changed ()
{
	RadioMenuItem* item;
	
	switch (track()->mode()) {
	case ARDOUR::Normal:
		item = normal_track_mode_item;
		break;
	case ARDOUR::Destructive:
		item = destructive_track_mode_item;
		break;
	default:
		fatal << string_compose (_("programming error: %1 %2"), "illegal track mode in RouteTimeAxisView::set_track_mode", track()->mode()) << endmsg;
		/*NOTREACHED*/
		return;
	}

	item->set_active ();
}

void
RouteTimeAxisView::show_timestretch (nframes_t start, nframes_t end)
{
	double x1;
	double x2;
	double y2;
	
	TimeAxisView::show_timestretch (start, end);

	hide_timestretch ();

#if 0	
	if (ts.empty()) {
		return;
	}


	/* check that the time selection was made in our route, or our edit group.
	   remember that edit_group() == 0 implies the route is *not* in a edit group.
	*/

	if (!(ts.track == this || (ts.group != 0 && ts.group == _route->edit_group()))) {
		/* this doesn't apply to us */
		return;
	}

	/* ignore it if our edit group is not active */
	
	if ((ts.track != this) && _route->edit_group() && !_route->edit_group()->is_active()) {
		return;
	}
#endif

	if (timestretch_rect == 0) {
		timestretch_rect = new SimpleRect (*canvas_display);
		timestretch_rect->property_x1() =  0.0;
		timestretch_rect->property_y1() =  0.0;
		timestretch_rect->property_x2() =  0.0;
		timestretch_rect->property_y2() =  0.0;
		timestretch_rect->property_fill_color_rgba() =  color_map[cTimeStretchFill];
		timestretch_rect->property_outline_color_rgba() = color_map[cTimeStretchOutline];
	}

	timestretch_rect->show ();
	timestretch_rect->raise_to_top ();

	x1 = start / editor.get_current_zoom();
	x2 = (end - 1) / editor.get_current_zoom();
	y2 = height - 2;
	
	timestretch_rect->property_x1() = x1;
	timestretch_rect->property_y1() = 1.0;
	timestretch_rect->property_x2() = x2;
	timestretch_rect->property_y2() = y2;
}

void
RouteTimeAxisView::hide_timestretch ()
{
	TimeAxisView::hide_timestretch ();

	if (timestretch_rect) {
		timestretch_rect->hide ();
	}
}

void
RouteTimeAxisView::show_selection (TimeSelection& ts)
{

#if 0
	/* ignore it if our edit group is not active or if the selection was started
	   in some other track or edit group (remember that edit_group() == 0 means
	   that the track is not in an edit group).
	*/

	if (((ts.track != this && !is_child (ts.track)) && _route->edit_group() && !_route->edit_group()->is_active()) ||
	    (!(ts.track == this || is_child (ts.track) || (ts.group != 0 && ts.group == _route->edit_group())))) {
		hide_selection ();
		return;
	}
#endif

	TimeAxisView::show_selection (ts);
}

void
RouteTimeAxisView::set_height (TrackHeight h)
{
	bool height_changed = (height == 0) || (h != height_style);

	TimeAxisView::set_height (h);

	ensure_xml_node ();

	if (_view) {
		_view->set_height ((double) height);
	}

	switch (height_style) {
	case Largest:
		xml_node->add_property ("track_height", "largest");
		break;

	case Large:
		xml_node->add_property ("track_height", "large");
		break;

	case Larger:
		xml_node->add_property ("track_height", "larger");
		break;

	case Normal:
		xml_node->add_property ("track_height", "normal");
		break;

	case Smaller:
		xml_node->add_property ("track_height", "smaller");
		break;

	case Small:
		xml_node->add_property ("track_height", "small");
		break;
	}

	switch (height_style) {
	case Largest:
	case Large:
	case Larger:
	case Normal:
		show_name_entry ();
		hide_name_label ();

		mute_button->show();
		solo_button->show();
		if (rec_enable_button)
			rec_enable_button->show();

		edit_group_button.show();
		hide_button.show();
		visual_button.show();
		size_button.show();
		automation_button.show();
		
		if (is_track() && track()->mode() == ARDOUR::Normal) {
			playlist_button.show();
		}
		break;

	case Smaller:
		show_name_entry ();
		hide_name_label ();

		mute_button->show();
		solo_button->show();
		if (rec_enable_button)
			rec_enable_button->show();

		edit_group_button.hide ();
		hide_button.hide ();
		visual_button.hide ();
		size_button.hide ();
		automation_button.hide ();
		
		if (is_track() && track()->mode() == ARDOUR::Normal) {
			playlist_button.hide ();
		}
		break;

	case Small:
		hide_name_entry ();
		show_name_label ();

		mute_button->hide();
		solo_button->hide();
		if (rec_enable_button)
			rec_enable_button->hide();

		edit_group_button.hide ();
		hide_button.hide ();
		visual_button.hide ();
		size_button.hide ();
		automation_button.hide ();
		playlist_button.hide ();
		name_label.set_text (_route->name());
		break;
	}

	if (height_changed) {
		/* only emit the signal if the height really changed */
		 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
	}
}

void
RouteTimeAxisView::select_track_color ()
{
	if (RouteUI::choose_color ()) {

		if (_view) {
			_view->apply_color (_color, StreamView::RegionColor);
		}
	}
}

void
RouteTimeAxisView::reset_samples_per_unit ()
{
	set_samples_per_unit (editor.get_current_zoom());
}

void
RouteTimeAxisView::set_samples_per_unit (double spu)
{
	double speed = 1.0;

	if (get_diskstream() != 0) {
		speed = get_diskstream()->speed();
	}
	
	if (_view) {
		_view->set_samples_per_unit (spu * speed);
	}

	TimeAxisView::set_samples_per_unit (spu * speed);
}

void
RouteTimeAxisView::align_style_changed ()
{
	switch (get_diskstream()->alignment_style()) {
	case ExistingMaterial:
		if (!align_existing_item->get_active()) {
			align_existing_item->set_active();
		}
		break;
	case CaptureTime:
		if (!align_capture_item->get_active()) {
			align_capture_item->set_active();
		}
		break;
	}
}

void
RouteTimeAxisView::set_align_style (AlignStyle style)
{
	RadioMenuItem* item;

	switch (style) {
	case ExistingMaterial:
		item = align_existing_item;
		break;
	case CaptureTime:
		item = align_capture_item;
		break;
	default:
		fatal << string_compose (_("programming error: %1 %2"), "illegal align style in RouteTimeAxisView::set_align_style", style) << endmsg;
		/*NOTREACHED*/
		return;
	}

	if (item->get_active()) {
		get_diskstream()->set_align_style (style);
	}
}

void
RouteTimeAxisView::rename_current_playlist ()
{
	ArdourPrompter prompter (true);
	string name;

	boost::shared_ptr<Diskstream> ds = get_diskstream();
	if (!ds || ds->destructive())
		return;

	boost::shared_ptr<Playlist> pl = ds->playlist();
	if (!pl)
		return;

	prompter.set_prompt (_("Name for playlist"));
	prompter.set_initial_text (pl->name());
	prompter.add_button (_("Rename"), Gtk::RESPONSE_ACCEPT);
	prompter.set_response_sensitive (Gtk::RESPONSE_ACCEPT, false);

	switch (prompter.run ()) {
	case Gtk::RESPONSE_ACCEPT:
		prompter.get_result (name);
		if (name.length()) {
			pl->set_name (name);
		}
		break;

	default:
		break;
	}
}

void
RouteTimeAxisView::use_copy_playlist (bool prompt)
{
	string name;
	
	boost::shared_ptr<Diskstream> ds = get_diskstream();
	if (!ds || ds->destructive())
		return;

	boost::shared_ptr<const Playlist> pl = ds->playlist();
	if (!pl)
		return;

	name = pl->name();

	do {
		name = Playlist::bump_name (name, _session);
	} while (_session.playlist_by_name(name));

	// TODO: The prompter "new" button should be de-activated if the user
	// specifies a playlist name which already exists in the session.

	if (prompt) {

		ArdourPrompter prompter (true);
		
		prompter.set_prompt (_("Name for Playlist"));
		prompter.set_initial_text (name);
		prompter.add_button (Gtk::Stock::NEW, Gtk::RESPONSE_ACCEPT);
		prompter.set_response_sensitive (Gtk::RESPONSE_ACCEPT, true);
		prompter.show_all ();
		
		switch (prompter.run ()) {
		case Gtk::RESPONSE_ACCEPT:
			prompter.get_result (name);
			break;
			
		default:
			return;
		}
	}

	if (name.length()) {
		ds->use_copy_playlist ();
		ds->playlist()->set_name (name);
	}
}

void
RouteTimeAxisView::use_new_playlist (bool prompt)
{
	string name;
	
	boost::shared_ptr<Diskstream> ds = get_diskstream();
	if (!ds || ds->destructive())
		return;

	boost::shared_ptr<const Playlist> pl = ds->playlist();
	if (!pl)
		return;

	name = pl->name();

	do {
		name = Playlist::bump_name (name, _session);
	} while (_session.playlist_by_name(name));


	if (prompt) {
		
		ArdourPrompter prompter (true);
		
		prompter.set_prompt (_("Name for Playlist"));
		prompter.set_initial_text (name);
		prompter.add_button (Gtk::Stock::NEW, Gtk::RESPONSE_ACCEPT);
		prompter.set_response_sensitive (Gtk::RESPONSE_ACCEPT, true);

		switch (prompter.run ()) {
		case Gtk::RESPONSE_ACCEPT:
			prompter.get_result (name);
			break;
			
		default:
			return;
		}
	}

	if (name.length()) {
		ds->use_new_playlist ();
		ds->playlist()->set_name (name);
	}
}

void
RouteTimeAxisView::clear_playlist ()
{
	boost::shared_ptr<Diskstream> ds = get_diskstream();
	if (!ds || ds->destructive())
		return;

	boost::shared_ptr<Playlist> pl = ds->playlist();
	if (!pl)
		return;

	editor.clear_playlist (pl);
}

void
RouteTimeAxisView::speed_changed ()
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun(*this, &RouteTimeAxisView::reset_samples_per_unit));
}

void
RouteTimeAxisView::diskstream_changed ()
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun(*this, &RouteTimeAxisView::update_diskstream_display));
}	

void
RouteTimeAxisView::update_diskstream_display ()
{
	if (!get_diskstream()) // bus
		return;

	set_playlist (get_diskstream()->playlist());
	map_frozen ();
}	

void
RouteTimeAxisView::selection_click (GdkEventButton* ev)
{
	PublicEditor::TrackViewList* tracks = editor.get_valid_views (this, _route->edit_group());

	switch (Keyboard::selection_type (ev->state)) {
	case Selection::Toggle:
		editor.get_selection().toggle (*tracks);
		break;
		
	case Selection::Set:
		editor.get_selection().set (*tracks);
		break;

	case Selection::Extend:
		if (tracks->size() > 1) {
			/* add each one, do not "extend" */
			editor.get_selection().add (*tracks);
		} else {
			/* extend to the single track */
			editor.extend_selection_to_track (*tracks->front());
		}
		break;

	case Selection::Add:
		editor.get_selection().add (*tracks);
		break;
	}

	delete tracks;
}

void
RouteTimeAxisView::set_selected_points (PointSelection& points)
{
	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->set_selected_points (points);
	}
}

void
RouteTimeAxisView::set_selected_regionviews (RegionSelection& regions)
{
	if (_view) {
		_view->set_selected_regionviews (regions);
	}
}

void
RouteTimeAxisView::get_selectables (nframes_t start, nframes_t end, double top, double bot, list<Selectable*>& results)
{
	double speed = 1.0;
	
	if (get_diskstream() != 0) {
		speed = get_diskstream()->speed();
	}
	
	nframes_t start_adjusted = session_frame_to_track_frame(start, speed);
	nframes_t end_adjusted   = session_frame_to_track_frame(end, speed);

	if (_view && ((top < 0.0 && bot < 0.0)) || touched (top, bot)) {
		_view->get_selectables (start_adjusted, end_adjusted, results);
	}

	/* pick up visible automation tracks */
	
	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		if (!(*i)->hidden()) {
			(*i)->get_selectables (start_adjusted, end_adjusted, top, bot, results);
		}
	}
}

void
RouteTimeAxisView::get_inverted_selectables (Selection& sel, list<Selectable*>& results)
{
	if (_view) {
		_view->get_inverted_selectables (sel, results);
	}

	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		if (!(*i)->hidden()) {
			(*i)->get_inverted_selectables (sel, results);
		}
	}

	return;
}

RouteGroup*
RouteTimeAxisView::edit_group() const
{
	return _route->edit_group();
}

string
RouteTimeAxisView::name() const
{
	return _route->name();
}

boost::shared_ptr<Playlist>
RouteTimeAxisView::playlist () const 
{
	boost::shared_ptr<Diskstream> ds;

	if ((ds = get_diskstream()) != 0) {
		return ds->playlist(); 
	} else {
		return boost::shared_ptr<Playlist> ();
	}
}

void
RouteTimeAxisView::name_entry_changed ()
{
	string x;

	x = name_entry.get_text ();
	
	if (x == _route->name()) {
		return;
	}

	strip_whitespace_edges(x);

	if (x.length() == 0) {
		name_entry.set_text (_route->name());
		return;
	}

	if (_session.route_name_unique (x)) {
		_route->set_name (x, this);
	} else {
		ARDOUR_UI::instance()->popup_error (_("A track already exists with that name"));
		name_entry.set_text (_route->name());
	}
}

void
RouteTimeAxisView::visual_click ()
{
	popup_display_menu (0);
}

void
RouteTimeAxisView::hide_click ()
{
	// LAME fix for hide_button refresh fix
	hide_button.set_sensitive(false);
	
	editor.hide_track_in_display (*this);
	
	hide_button.set_sensitive(true);
}

boost::shared_ptr<Region>
RouteTimeAxisView::find_next_region (nframes_t pos, RegionPoint point, int32_t dir)
{
	boost::shared_ptr<Diskstream> stream;
	boost::shared_ptr<Playlist> playlist;

	if ((stream = get_diskstream()) != 0 && (playlist = stream->playlist()) != 0) {
		return playlist->find_next_region (pos, point, dir);
	}

	return boost::shared_ptr<Region> ();
}

bool
RouteTimeAxisView::cut_copy_clear (Selection& selection, CutCopyOp op)
{
	boost::shared_ptr<Playlist> what_we_got;
	boost::shared_ptr<Diskstream> ds = get_diskstream();
	boost::shared_ptr<Playlist> playlist;
	bool ret = false;

	if (ds == 0) {
		/* route is a bus, not a track */
		return false;
	}

	playlist = ds->playlist();

	TimeSelection time (selection.time);
	float speed = ds->speed();
	if (speed != 1.0f) {
		for (TimeSelection::iterator i = time.begin(); i != time.end(); ++i) {
			(*i).start = session_frame_to_track_frame((*i).start, speed);
			(*i).end   = session_frame_to_track_frame((*i).end,   speed);
		}
	}
	
	XMLNode &before = playlist->get_state();
	switch (op) {
	case Cut:
		if ((what_we_got = playlist->cut (time)) != 0) {
			editor.get_cut_buffer().add (what_we_got);
			_session.add_command( new MementoCommand<Playlist>(*playlist.get(), &before, &playlist->get_state()));
			ret = true;
		}
		break;
	case Copy:
		if ((what_we_got = playlist->copy (time)) != 0) {
			editor.get_cut_buffer().add (what_we_got);
		}
		break;

	case Clear:
		if ((what_we_got = playlist->cut (time)) != 0) {
			_session.add_command( new MementoCommand<Playlist>(*playlist, &before, &playlist->get_state()));
			what_we_got->release ();
			ret = true;
		}
		break;
	}

	return ret;
}

bool
RouteTimeAxisView::paste (nframes_t pos, float times, Selection& selection, size_t nth)
{
	if (!is_track()) {
		return false;
	}

	boost::shared_ptr<Playlist> playlist = get_diskstream()->playlist();
	PlaylistSelection::iterator p;
	
	for (p = selection.playlists.begin(); p != selection.playlists.end() && nth; ++p, --nth);

	if (p == selection.playlists.end()) {
		return false;
	}

	if (get_diskstream()->speed() != 1.0f)
		pos = session_frame_to_track_frame(pos, get_diskstream()->speed() );
	
	XMLNode &before = playlist->get_state();
	playlist->paste (*p, pos, times);
	_session.add_command( new MementoCommand<Playlist>(*playlist, &before, &playlist->get_state()));

	return true;
}


list<TimeAxisView*>
RouteTimeAxisView::get_child_list()
{
  
	list<TimeAxisView*>redirect_children;
	
	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		if (!(*i)->hidden()) {
			redirect_children.push_back(*i);
		}
	}
	return redirect_children;
}


void
RouteTimeAxisView::build_playlist_menu (Gtk::Menu * menu)
{
	using namespace Menu_Helpers;

	if (!menu || !is_track()) {
		return;
	}

	MenuList& playlist_items = menu->items();
	menu->set_name ("ArdourContextMenu");
	playlist_items.clear();

	if (playlist_menu) {
		delete playlist_menu;
	}

	playlist_menu = new Menu;
	playlist_menu->set_name ("ArdourContextMenu");

	vector<boost::shared_ptr<Playlist> > playlists;
	boost::shared_ptr<Diskstream> ds = get_diskstream();
	RadioMenuItem::Group playlist_group;

	_session.get_playlists (playlists);
	
	for (vector<boost::shared_ptr<Playlist> >::iterator i = playlists.begin(); i != playlists.end(); ++i) {

		if ((*i)->get_orig_diskstream_id() == ds->id()) {
			playlist_items.push_back (RadioMenuElem (playlist_group, (*i)->name(), bind (mem_fun (*this, &RouteTimeAxisView::use_playlist),
												     boost::weak_ptr<Playlist> (*i))));

			if (ds->playlist()->id() == (*i)->id()) {
				static_cast<RadioMenuItem*>(&playlist_items.back())->set_active();
			}
		} else if (ds->playlist()->id() == (*i)->id()) {
			playlist_items.push_back (RadioMenuElem (playlist_group, (*i)->name(), bind (mem_fun (*this, &RouteTimeAxisView::use_playlist), 
												     boost::weak_ptr<Playlist>(*i))));
			static_cast<RadioMenuItem*>(&playlist_items.back())->set_active();
			
		}
	}

	playlist_items.push_back (SeparatorElem());
	playlist_items.push_back (MenuElem (_("Rename"), mem_fun(*this, &RouteTimeAxisView::rename_current_playlist)));
	playlist_items.push_back (SeparatorElem());

	playlist_items.push_back (MenuElem (_("New"), mem_fun(editor, &PublicEditor::new_playlists)));
	playlist_items.push_back (MenuElem (_("New Copy"), mem_fun(editor, &PublicEditor::copy_playlists)));
	playlist_items.push_back (SeparatorElem());
	playlist_items.push_back (MenuElem (_("Clear Current"), mem_fun(editor, &PublicEditor::clear_playlists)));
	playlist_items.push_back (SeparatorElem());

	playlist_items.push_back (MenuElem(_("Select from all ..."), mem_fun(*this, &RouteTimeAxisView::show_playlist_selector)));
}

void
RouteTimeAxisView::use_playlist (boost::weak_ptr<Playlist> wpl)
{
	assert (is_track());

	boost::shared_ptr<Playlist> pl (wpl.lock());

	if (!pl) {
		return;
	}

	boost::shared_ptr<AudioPlaylist> apl = boost::dynamic_pointer_cast<AudioPlaylist> (pl);
	
	if (apl) {
		get_diskstream()->use_playlist (apl);
	}
}

void
RouteTimeAxisView::show_playlist_selector ()
{
	editor.playlist_selector().show_for (this);
}

void
RouteTimeAxisView::map_frozen ()
{
	if (!is_track()) {
		return;
	}

	ENSURE_GUI_THREAD (mem_fun(*this, &RouteTimeAxisView::map_frozen));

	switch (track()->freeze_state()) {
	case Track::Frozen:
		playlist_button.set_sensitive (false);
		rec_enable_button->set_sensitive (false);
		break;
	default:
		playlist_button.set_sensitive (true);
		rec_enable_button->set_sensitive (true);
		break;
	}
}

void
RouteTimeAxisView::color_handler (ColorID id, uint32_t val)
{
	switch (id) {
	case cTimeStretchOutline:
		timestretch_rect->property_outline_color_rgba() = val;
		break;
	case cTimeStretchFill:
		timestretch_rect->property_fill_color_rgba() = val;
		break;
	default:
		break;
	}
}

void
RouteTimeAxisView::show_all_automation ()
{
	no_redraw = true;

	for (list<RedirectAutomationInfo*>::iterator i = redirect_automation.begin(); i != redirect_automation.end(); ++i) {
		for (vector<RedirectAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
			if ((*ii)->view == 0) {
				add_redirect_automation_curve ((*i)->redirect, (*ii)->what);
			} 

			(*ii)->menu_item->set_active (true);
		}
	}

	no_redraw = false;

	 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

void
RouteTimeAxisView::show_existing_automation ()
{
	no_redraw = true;

	for (list<RedirectAutomationInfo*>::iterator i = redirect_automation.begin(); i != redirect_automation.end(); ++i) {
		for (vector<RedirectAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
			if ((*ii)->view != 0) {
				(*ii)->menu_item->set_active (true);
			}
		}
	}

	no_redraw = false;

	 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

void
RouteTimeAxisView::hide_all_automation ()
{
	no_redraw = true;

	for (list<RedirectAutomationInfo*>::iterator i = redirect_automation.begin(); i != redirect_automation.end(); ++i) {
		for (vector<RedirectAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
			(*ii)->menu_item->set_active (false);
		}
	}

	no_redraw = false;
	 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}


void
RouteTimeAxisView::region_view_added (RegionView* rv)
{
	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		AutomationTimeAxisView* atv;

		if ((atv = dynamic_cast<AutomationTimeAxisView*> (*i)) != 0) {
			rv->add_ghost (*atv);
		}
	}
}

void
RouteTimeAxisView::add_ghost_to_redirect (RegionView* rv, AutomationTimeAxisView* atv)
{
	rv->add_ghost (*atv);
}

RouteTimeAxisView::RedirectAutomationInfo::~RedirectAutomationInfo ()
{
	for (vector<RedirectAutomationNode*>::iterator i = lines.begin(); i != lines.end(); ++i) {
		delete *i;
	}
}


RouteTimeAxisView::RedirectAutomationNode::~RedirectAutomationNode ()
{
	parent.remove_ran (this);

	if (view) {
		delete view;
	}
}

void
RouteTimeAxisView::remove_ran (RedirectAutomationNode* ran)
{
	if (ran->view) {
		remove_child (ran->view);
	}
}

RouteTimeAxisView::RedirectAutomationNode*
RouteTimeAxisView::find_redirect_automation_node (boost::shared_ptr<Redirect> redirect, uint32_t what)
{
	for (list<RedirectAutomationInfo*>::iterator i = redirect_automation.begin(); i != redirect_automation.end(); ++i) {

		if ((*i)->redirect == redirect) {

			for (vector<RedirectAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
				if ((*ii)->what == what) {
					return *ii;
				}
			}
		}
	}

	return 0;
}

// FIXME: duplicated in midi_time_axis.cc
static string 
legalize_for_xml_node (string str)
{
	string::size_type pos;
	string legal_chars = "abcdefghijklmnopqrtsuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_+=:";
	string legal;

	legal = str;
	pos = 0;

	while ((pos = legal.find_first_not_of (legal_chars, pos)) != string::npos) {
		legal.replace (pos, 1, "_");
		pos += 1;
	}

	return legal;
}


void
RouteTimeAxisView::add_redirect_automation_curve (boost::shared_ptr<Redirect> redirect, uint32_t what)
{
	RedirectAutomationLine* ral;
	string name;
	RedirectAutomationNode* ran;

	if ((ran = find_redirect_automation_node (redirect, what)) == 0) {
		fatal << _("programming error: ")
		      << string_compose (X_("redirect automation curve for %1:%2 not registered with audio track!"),
				  redirect->name(), what)
		      << endmsg;
		/*NOTREACHED*/
		return;
	}

	if (ran->view) {
		return;
	}

	name = redirect->describe_parameter (what);

	/* create a string that is a legal XML node name that can be used to refer to this redirect+port combination */

	char state_name[256];
	snprintf (state_name, sizeof (state_name), "Redirect-%s-%" PRIu32, legalize_for_xml_node (redirect->name()).c_str(), what);

	ran->view = new RedirectAutomationTimeAxisView (_session, _route, editor, *this, parent_canvas, name, what, *redirect, state_name);

	ral = new RedirectAutomationLine (name, 
					  *redirect, what, _session, *ran->view,
					  *ran->view->canvas_display, redirect->automation_list (what));
	
	ral->set_line_color (color_map[cRedirectAutomationLine]);
	ral->queue_reset ();

	ran->view->add_line (*ral);

	ran->view->Hiding.connect (bind (mem_fun(*this, &RouteTimeAxisView::redirect_automation_track_hidden), ran, redirect));

	if (!ran->view->marked_for_display()) {
		ran->view->hide ();
	} else {
		ran->menu_item->set_active (true);
	}

	add_child (ran->view);

	if (_view) {
		_view->foreach_regionview (bind (mem_fun(*this, &RouteTimeAxisView::add_ghost_to_redirect), ran->view));
	}

	redirect->mark_automation_visible (what, true);
}

void
RouteTimeAxisView::redirect_automation_track_hidden (RouteTimeAxisView::RedirectAutomationNode* ran, boost::shared_ptr<Redirect> r)
{
	if (!_hidden) {
		ran->menu_item->set_active (false);
	}

	r->mark_automation_visible (ran->what, false);

	 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

void
RouteTimeAxisView::add_existing_redirect_automation_curves (boost::shared_ptr<Redirect> redirect)
{
	set<uint32_t> s;
	RedirectAutomationLine *ral;

	redirect->what_has_visible_automation (s);

	for (set<uint32_t>::iterator i = s.begin(); i != s.end(); ++i) {
		
		if ((ral = find_redirect_automation_curve (redirect, *i)) != 0) {
			ral->queue_reset ();
		} else {
			add_redirect_automation_curve (redirect, (*i));
		}
	}
}

void
RouteTimeAxisView::add_redirect_to_subplugin_menu (boost::shared_ptr<Redirect> r)
{
	using namespace Menu_Helpers;
	RedirectAutomationInfo *rai;
	list<RedirectAutomationInfo*>::iterator x;
	
	const std::set<uint32_t>& automatable = r->what_can_be_automated ();
	std::set<uint32_t> has_visible_automation;

	r->what_has_visible_automation(has_visible_automation);

	if (automatable.empty()) {
		return;
	}

	for (x = redirect_automation.begin(); x != redirect_automation.end(); ++x) {
		if ((*x)->redirect == r) {
			break;
		}
	}

	if (x == redirect_automation.end()) {

		rai = new RedirectAutomationInfo (r);
		redirect_automation.push_back (rai);

	} else {

		rai = *x;

	}

	/* any older menu was deleted at the top of redirects_changed()
	   when we cleared the subplugin menu.
	*/

	rai->menu = manage (new Menu);
	MenuList& items = rai->menu->items();
	rai->menu->set_name ("ArdourContextMenu");

	items.clear ();

	for (std::set<uint32_t>::const_iterator i = automatable.begin(); i != automatable.end(); ++i) {

		RedirectAutomationNode* ran;
		CheckMenuItem* mitem;
		
		string name = r->describe_parameter (*i);
		
		items.push_back (CheckMenuElem (name));
		mitem = dynamic_cast<CheckMenuItem*> (&items.back());

		if (has_visible_automation.find((*i)) != has_visible_automation.end()) {
			mitem->set_active(true);
		}

		if ((ran = find_redirect_automation_node (r, *i)) == 0) {

			/* new item */
			
			ran = new RedirectAutomationNode (*i, mitem, *this);
			
			rai->lines.push_back (ran);

		} else {

			ran->menu_item = mitem;

		}

		mitem->signal_toggled().connect (bind (mem_fun(*this, &RouteTimeAxisView::redirect_menu_item_toggled), rai, ran));
	}

	/* add the menu for this redirect, because the subplugin
	   menu is always cleared at the top of redirects_changed().
	   this is the result of some poor design in gtkmm and/or
	   GTK+.
	*/

	subplugin_menu.items().push_back (MenuElem (r->name(), *rai->menu));
	rai->valid = true;
}

void
RouteTimeAxisView::redirect_menu_item_toggled (RouteTimeAxisView::RedirectAutomationInfo* rai,
					       RouteTimeAxisView::RedirectAutomationNode* ran)
{
	bool showit = ran->menu_item->get_active();
	bool redraw = false;

	if (ran->view == 0 && showit) {
		add_redirect_automation_curve (rai->redirect, ran->what);
		redraw = true;
	}

	if (showit != ran->view->marked_for_display()) {

		if (showit) {
			ran->view->set_marked_for_display (true);
			ran->view->canvas_display->show();
		} else {
			rai->redirect->mark_automation_visible (ran->what, true);
			ran->view->set_marked_for_display (false);
			ran->view->hide ();
		}

		redraw = true;

	}

	if (redraw && !no_redraw) {

		/* now trigger a redisplay */
		
		 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */

	}
}

void
RouteTimeAxisView::redirects_changed (void *src)
{
	using namespace Menu_Helpers;

	for (list<RedirectAutomationInfo*>::iterator i = redirect_automation.begin(); i != redirect_automation.end(); ++i) {
		(*i)->valid = false;
	}

	subplugin_menu.items().clear ();

	_route->foreach_redirect (this, &RouteTimeAxisView::add_redirect_to_subplugin_menu);
	_route->foreach_redirect (this, &RouteTimeAxisView::add_existing_redirect_automation_curves);

	for (list<RedirectAutomationInfo*>::iterator i = redirect_automation.begin(); i != redirect_automation.end(); ) {

		list<RedirectAutomationInfo*>::iterator tmp;

		tmp = i;
		++tmp;

		if (!(*i)->valid) {

			delete *i;
			redirect_automation.erase (i);

		} 

		i = tmp;
	}

	/* change in visibility was possible */

	_route->gui_changed ("track_height", this);
}

RedirectAutomationLine *
RouteTimeAxisView::find_redirect_automation_curve (boost::shared_ptr<Redirect> redirect, uint32_t what)
{
	RedirectAutomationNode* ran;

	if ((ran = find_redirect_automation_node (redirect, what)) != 0) {
		if (ran->view) {
			return dynamic_cast<RedirectAutomationLine*> (ran->view->lines.front());
		} 
	}

	return 0;
}

void
RouteTimeAxisView::reset_redirect_automation_curves ()
{
	for (vector<RedirectAutomationLine*>::iterator i = redirect_automation_curves.begin(); i != redirect_automation_curves.end(); ++i) {
		(*i)->reset();
	}
}

