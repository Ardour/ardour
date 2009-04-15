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
#include <utility>

#include <sigc++/bind.h>

#include "pbd/error.h"
#include "pbd/stl_delete.h"
#include "pbd/whitespace.h"
#include "pbd/memento_command.h"

#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/bindable_button.h>
#include <gtkmm2ext/utils.h>

#include "ardour/audioplaylist.h"
#include "ardour/diskstream.h"
#include "ardour/event_type_map.h"
#include "ardour/ladspa_plugin.h"
#include "ardour/location.h"
#include "ardour/panner.h"
#include "ardour/playlist.h"
#include "ardour/playlist.h"
#include "ardour/processor.h"
#include "ardour/profile.h"
#include "ardour/route_group.h"
#include "ardour/session.h"
#include "ardour/session_playlist.h"
#include "ardour/utils.h"
#include "evoral/Parameter.hpp"

#include "ardour_ui.h"
#include "route_time_axis.h"
#include "automation_time_axis.h"
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

#include "ardour/track.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace Editing;
using namespace sigc;
using namespace std;

Glib::RefPtr<Gdk::Pixbuf> RouteTimeAxisView::slider;

void
RouteTimeAxisView::setup_slider_pix ()
{
	if ((slider = ::get_icon ("fader_belt_h")) == 0) {
		throw failed_constructor ();
	}
}

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
	  visual_button (_("v")),
	  gm (sess, slider, true)
{
	gm.set_io (rt);
	gm.get_level_meter().set_no_show_all();
	gm.get_level_meter().setup_meters(50);

	_has_state = true;
	playlist_menu = 0;
	playlist_action_menu = 0;
	automation_action_menu = 0;
	_view = 0;

	if (!_route->is_hidden()) {
		_marked_for_display = true;
	}

	timestretch_rect = 0;
	no_redraw = false;
	destructive_track_mode_item = 0;
	normal_track_mode_item = 0;

	ignore_toggle = false;

	edit_group_button.set_name ("TrackGroupButton");
	playlist_button.set_name ("TrackPlaylistButton");
	automation_button.set_name ("TrackAutomationButton");
	size_button.set_name ("TrackSizeButton");
	visual_button.set_name ("TrackVisualButton");
	hide_button.set_name ("TrackRemoveButton");

	edit_group_button.unset_flags (Gtk::CAN_FOCUS);
	playlist_button.unset_flags (Gtk::CAN_FOCUS);
	automation_button.unset_flags (Gtk::CAN_FOCUS);
	size_button.unset_flags (Gtk::CAN_FOCUS);
	visual_button.unset_flags (Gtk::CAN_FOCUS);
	hide_button.unset_flags (Gtk::CAN_FOCUS);

	hide_button.add (*(manage (new Image (::get_icon("hide")))));
	hide_button.show_all ();

 	edit_group_button.signal_button_release_event().connect (mem_fun(*this, &RouteTimeAxisView::edit_click), false);
	playlist_button.signal_clicked().connect (mem_fun(*this, &RouteTimeAxisView::playlist_click));
	automation_button.signal_clicked().connect (mem_fun(*this, &RouteTimeAxisView::automation_click));
	size_button.signal_button_release_event().connect (mem_fun(*this, &RouteTimeAxisView::size_click), false);
	visual_button.signal_clicked().connect (mem_fun(*this, &RouteTimeAxisView::visual_click));
	hide_button.signal_clicked().connect (mem_fun(*this, &RouteTimeAxisView::hide_click));

	solo_button->signal_button_press_event().connect (mem_fun(*this, &RouteUI::solo_press), false);
	solo_button->signal_button_release_event().connect (mem_fun(*this, &RouteUI::solo_release), false);
	mute_button->signal_button_press_event().connect (mem_fun(*this, &RouteUI::mute_press), false);
	mute_button->signal_button_release_event().connect (mem_fun(*this, &RouteUI::mute_release), false);

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

		rec_enable_button->signal_button_press_event().connect (mem_fun(*this, &RouteUI::rec_enable_press), false);
		rec_enable_button->signal_button_release_event().connect (mem_fun(*this, &RouteUI::rec_enable_release));
		controls_table.attach (*rec_enable_button, 5, 6, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);
		ARDOUR_UI::instance()->tooltips().set_tip(*rec_enable_button, _("Record"));

	}

	controls_hbox.pack_start(gm.get_level_meter(), false, false);
	_route->meter_change.connect (mem_fun(*this, &RouteTimeAxisView::meter_changed));
	_route->input_changed.connect (mem_fun(*this, &RouteTimeAxisView::io_changed));
	_route->output_changed.connect (mem_fun(*this, &RouteTimeAxisView::io_changed));

	controls_table.attach (*mute_button, 6, 7, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);
	controls_table.attach (*solo_button, 7, 8, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);

	controls_table.attach (edit_group_button, 7, 8, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);
	controls_table.attach (gm.get_gain_slider(), 0, 5, 1, 2, Gtk::SHRINK, Gtk::SHRINK, 0, 0);

	ARDOUR_UI::instance()->tooltips().set_tip(*solo_button,_("Solo"));
	ARDOUR_UI::instance()->tooltips().set_tip(*mute_button,_("Mute"));
	ARDOUR_UI::instance()->tooltips().set_tip(edit_group_button,_("Edit Group"));
	ARDOUR_UI::instance()->tooltips().set_tip(size_button,_("Display Height"));
	ARDOUR_UI::instance()->tooltips().set_tip(playlist_button,_("Playlist"));
	ARDOUR_UI::instance()->tooltips().set_tip(automation_button, _("Automation"));
	ARDOUR_UI::instance()->tooltips().set_tip(visual_button, _("Visual options"));
	ARDOUR_UI::instance()->tooltips().set_tip(hide_button, _("Hide this track"));
	
	label_view ();

	if (0) {

		/* old school - when we used to put an extra row of buttons in place */

		controls_table.attach (hide_button, 0, 1, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
		controls_table.attach (visual_button, 1, 2, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
		controls_table.attach (size_button, 2, 3, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
		controls_table.attach (automation_button, 3, 4, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);

	} else {

		controls_table.attach (automation_button, 6, 7, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	}

	if (is_track() && track()->mode() == ARDOUR::Normal) {
		controls_table.attach (playlist_button, 5, 6, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	}

	_y_position = -1;

	_route->mute_changed.connect (mem_fun(*this, &RouteUI::mute_changed));
	_route->solo_changed.connect (mem_fun(*this, &RouteUI::solo_changed));
	_route->processors_changed.connect (mem_fun(*this, &RouteTimeAxisView::processors_changed));
	_route->NameChanged.connect (mem_fun(*this, &RouteTimeAxisView::route_name_changed));
	_route->solo_safe_changed.connect (mem_fun(*this, &RouteUI::solo_changed));


	if (is_track()) {

		track()->TrackModeChanged.connect (mem_fun(*this, &RouteTimeAxisView::track_mode_changed));
		track()->FreezeChange.connect (mem_fun(*this, &RouteTimeAxisView::map_frozen));
		track()->DiskstreamChanged.connect (mem_fun(*this, &RouteTimeAxisView::diskstream_changed));
		get_diskstream()->SpeedChanged.connect (mem_fun(*this, &RouteTimeAxisView::speed_changed));

		/* pick up the correct freeze state */
		map_frozen ();

	}

	_editor.ZoomChanged.connect (mem_fun(*this, &RouteTimeAxisView::reset_samples_per_unit));
	ColorsChanged.connect (mem_fun (*this, &RouteTimeAxisView::color_handler));

	gm.get_gain_slider().signal_scroll_event().connect(mem_fun(*this, &RouteTimeAxisView::controls_ebox_scroll), false);
	gm.get_gain_slider().set_name ("TrackGainFader");
}

RouteTimeAxisView::~RouteTimeAxisView ()
{
	GoingAway (); /* EMIT_SIGNAL */

	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {
		delete *i;
	}

	delete playlist_menu;
	playlist_menu = 0;
  
	delete playlist_action_menu;
	playlist_action_menu = 0;

	delete _view;
	_view = 0;

	for (AutomationTracks::iterator i = _automation_tracks.begin(); i != _automation_tracks.end(); ++i) {
		delete i->second;
	}
	
	_automation_tracks.clear ();
}

void
RouteTimeAxisView::post_construct ()
{
	/* map current state of the route */

	update_diskstream_display ();

	subplugin_menu.items().clear ();
	_route->foreach_processor (mem_fun (*this, &RouteTimeAxisView::add_processor_to_subplugin_menu));
	_route->foreach_processor (mem_fun (*this, &RouteTimeAxisView::add_existing_processor_automation_curves));
	reset_processor_automation_curves ();
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
	if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
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
RouteTimeAxisView::route_name_changed ()
{
	_editor.route_name_changed (this);
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
  
	delete playlist_action_menu;

	playlist_action_menu = new Menu;
	playlist_action_menu->set_name ("ArdourContextMenu");
	
 	build_playlist_menu (playlist_action_menu);

	conditionally_add_to_selection ();
	playlist_action_menu->popup (1, gtk_get_current_event_time());
}

void
RouteTimeAxisView::automation_click ()
{
	conditionally_add_to_selection ();
	build_automation_action_menu ();
	automation_action_menu->popup (1, gtk_get_current_event_time());
}

int
RouteTimeAxisView::set_state (const XMLNode& node)
{
	TimeAxisView::set_state (node);

	XMLNodeList kids = node.children();
	XMLNodeConstIterator iter;
	const XMLProperty* prop;
	
	for (iter = kids.begin(); iter != kids.end(); ++iter) {
		if ((*iter)->name() == AutomationTimeAxisView::state_node_name) {
			if ((prop = (*iter)->property ("automation-id")) != 0) {

				Evoral::Parameter param = ARDOUR::EventTypeMap::instance().new_parameter(prop->value());
				bool show = ((prop = (*iter)->property ("shown")) != 0) && prop->value() == "yes";
				create_automation_child(param, show);
			} else {
				warning << "Automation child has no ID" << endmsg;
			}
		}
	}

	return 0;
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

	if (subplugin_menu.get_attach_widget())
		subplugin_menu.detach();

	automation_items.push_back (MenuElem (_("Plugins"), subplugin_menu));
	
	map<Evoral::Parameter, RouteAutomationNode*>::iterator i;
	for (i = _automation_tracks.begin(); i != _automation_tracks.end(); ++i) {

		automation_items.push_back (SeparatorElem());

		delete i->second->menu_item;

		automation_items.push_back(CheckMenuElem (_route->describe_parameter(i->second->param), 
				bind (mem_fun(*this, &RouteTimeAxisView::toggle_automation_track), i->second->param)));

		i->second->menu_item = static_cast<Gtk::CheckMenuItem*>(&automation_items.back());

		i->second->menu_item->set_active(show_automation(i->second->param));
		//i->second->menu_item->set_active(false);
	}
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

	if (!Profile->get_sae()) {
		build_remote_control_menu ();
		items.push_back (MenuElem (_("Remote Control ID"), *remote_control_menu));
		/* rebuild this every time */
		build_automation_action_menu ();
		items.push_back (MenuElem (_("Automation"), *automation_action_menu));
		items.push_back (SeparatorElem());
	}

	// Hook for derived classes to add type specific stuff
	append_extra_display_menu_items ();
	items.push_back (SeparatorElem());
	
	if (is_track()) {

		Menu *layers_menu = manage(new Menu);
		MenuList &layers_items = layers_menu->items();
		layers_menu->set_name("ArdourContextMenu");

		RadioMenuItem::Group layers_group;

		layers_items.push_back(RadioMenuElem (layers_group, _("Overlaid"),
				bind (mem_fun (*this, &RouteTimeAxisView::set_layer_display), Overlaid)));
		layers_items.push_back(RadioMenuElem (layers_group, _("Stacked"),
				bind (mem_fun (*this, &RouteTimeAxisView::set_layer_display), Stacked)));

		items.push_back (MenuElem (_("Layers"), *layers_menu));

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

		if (!Profile->get_sae()) {
			items.push_back (MenuElem (_("Alignment"), *alignment_menu));
			get_diskstream()->AlignmentStyleChanged.connect (
					mem_fun(*this, &RouteTimeAxisView::align_style_changed));
			
			RadioMenuItem::Group mode_group;
			items.push_back (RadioMenuElem (mode_group, _("Normal mode"), bind (
					mem_fun (*this, &RouteTimeAxisView::set_track_mode),
					ARDOUR::Normal)));
			normal_track_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());
			items.push_back (RadioMenuElem (mode_group, _("Tape mode"), bind (
					mem_fun (*this, &RouteTimeAxisView::set_track_mode),
					ARDOUR::Destructive)));
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

		get_diskstream()->AlignmentStyleChanged.connect (
				mem_fun(*this, &RouteTimeAxisView::align_style_changed));

		mode_menu = build_mode_menu();
		if (mode_menu)
			items.push_back (MenuElem (_("Mode"), *mode_menu));
			
		color_mode_menu = build_color_mode_menu();
		if (color_mode_menu)
			items.push_back (MenuElem (_("Color Mode"), *color_mode_menu));
			
		items.push_back (SeparatorElem());
	}

	items.push_back (CheckMenuElem (_("Active"), mem_fun(*this, &RouteUI::toggle_route_active)));
	route_active_menu_item = dynamic_cast<CheckMenuItem *> (&items.back());
	route_active_menu_item->set_active (_route->active());

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Hide"), mem_fun(*this, &RouteTimeAxisView::hide_click)));
	if (!Profile->get_sae()) {
		items.push_back (MenuElem (_("Remove"), mem_fun(*this, &RouteUI::remove_this_route)));
	} else {
		items.push_front (SeparatorElem());
		items.push_front (MenuElem (_("Delete"), mem_fun(*this, &RouteUI::remove_this_route)));
	}
}

static bool __reset_item (RadioMenuItem* item)
{
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
	
	if (item && other_item && item->get_active () && track()->mode() != mode) {
		_set_track_mode (track().get(), mode, other_item);
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
		timestretch_rect = new SimpleRect (*canvas_display ());
		timestretch_rect->property_x1() =  0.0;
		timestretch_rect->property_y1() =  0.0;
		timestretch_rect->property_x2() =  0.0;
		timestretch_rect->property_y2() =  0.0;
		timestretch_rect->property_fill_color_rgba() =  ARDOUR_UI::config()->canvasvar_TimeStretchFill.get();
		timestretch_rect->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_TimeStretchOutline.get();
	}

	timestretch_rect->show ();
	timestretch_rect->raise_to_top ();

	x1 = start / _editor.get_current_zoom();
	x2 = (end - 1) / _editor.get_current_zoom();
	y2 = current_height() - 2;
	
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
RouteTimeAxisView::set_height (uint32_t h)
{
	int gmlen = h - 5;
	bool height_changed = (height == 0) || (h != height);
	gm.get_level_meter().setup_meters (gmlen);

	TimeAxisView::set_height (h);

	ensure_xml_node ();

	if (_view) {
		_view->set_height ((double) current_height());
	}

	char buf[32];
	snprintf (buf, sizeof (buf), "%u", height);
	xml_node->add_property ("height", buf);

	if (height >= hNormal) {
		reset_meter();
		show_name_entry ();
		hide_name_label ();

		gm.get_gain_slider().show();
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

	} else if (height >= hSmaller) {

		reset_meter();
		show_name_entry ();
		hide_name_label ();

		gm.get_gain_slider().hide();
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

	} else {


		/* don't allow name_entry to be hidden while
		   it has focus, otherwise the GUI becomes unusable.
		*/

		if (name_entry.has_focus()) {
			if (name_entry.get_text() != _route->name()) {
				name_entry_changed ();
			}
			controls_ebox.grab_focus ();
		}

		hide_name_entry ();
		show_name_label ();
		
		gm.get_gain_slider().hide();
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
	set_samples_per_unit (_editor.get_current_zoom());
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

std::string 
RouteTimeAxisView::resolve_new_group_playlist_name(std::string &basename, vector<boost::shared_ptr<Playlist> > const & playlists)
{
	std::string ret(basename);

	std::string group_string = "."+edit_group()->name()+".";

	// iterate through all playlists
	int maxnumber = 0;
	for (vector<boost::shared_ptr<Playlist> >::const_iterator i = playlists.begin(); i != playlists.end(); ++i) {
		std::string tmp = (*i)->name();

		std::string::size_type idx = tmp.find(group_string);			
		// find those which belong to this group
		if (idx != string::npos) {
			tmp = tmp.substr(idx + group_string.length());

			// and find the largest current number
			int x = atoi(tmp.c_str());
			if (x > maxnumber) {
				maxnumber = x;
			}
		}
	}

	maxnumber++;

	char buf[32];
	snprintf (buf, sizeof(buf), "%d", maxnumber);
               
	ret = this->name()+"."+edit_group()->name()+"."+buf;

	return ret;
}

void
RouteTimeAxisView::use_copy_playlist (bool prompt, vector<boost::shared_ptr<Playlist> > const & playlists_before_op)
{
	string name;
	
	boost::shared_ptr<Diskstream> ds = get_diskstream();
	if (!ds || ds->destructive())
		return;

	boost::shared_ptr<const Playlist> pl = ds->playlist();
	if (!pl)
		return;

	name = pl->name();
	
	if (edit_group() && edit_group()->is_active()) {
		name = resolve_new_group_playlist_name(name, playlists_before_op);
	}

	while (_session.playlist_by_name(name)) {
		name = Playlist::bump_name (name, _session);
	}

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
RouteTimeAxisView::use_new_playlist (bool prompt, vector<boost::shared_ptr<Playlist> > const & playlists_before_op)
{
	string name;
	
	boost::shared_ptr<Diskstream> ds = get_diskstream();
	if (!ds || ds->destructive())
		return;

	boost::shared_ptr<const Playlist> pl = ds->playlist();
	if (!pl)
		return;

	name = pl->name();
	
	if (edit_group() && edit_group()->is_active()) {
		name = resolve_new_group_playlist_name(name,playlists_before_op);
	}

	while (_session.playlist_by_name(name)) {
		name = Playlist::bump_name (name, _session);
	}


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

	_editor.clear_playlist (pl);
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
	if (Keyboard::modifier_state_equals (ev->state, (Keyboard::TertiaryModifier|Keyboard::PrimaryModifier))) {

		/* special case: select/deselect all tracks */
		if (_editor.get_selection().selected (this)) {
			_editor.get_selection().clear_tracks ();
		} else {
			_editor.select_all_tracks ();
		}

		return;
	} 

	PublicEditor::TrackViewList* tracks = _editor.get_valid_views (this, _route->edit_group());

	switch (Keyboard::selection_type (ev->state)) {
	case Selection::Toggle:
		_editor.get_selection().toggle (*tracks);
		break;
		
	case Selection::Set:
		_editor.get_selection().set (*tracks);
		break;

	case Selection::Extend:
		if (tracks->size() > 1) {
			/* add each one, do not "extend" */
			_editor.get_selection().add (*tracks);
		} else {
			/* extend to the single track */
			_editor.extend_selection_to_track (*tracks->front());
		}
		break;

	case Selection::Add:
		_editor.get_selection().add (*tracks);
		break;
	}

	delete tracks;
}

void
RouteTimeAxisView::set_selected_points (PointSelection& points)
{
	for (Children::iterator i = children.begin(); i != children.end(); ++i) {
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

/** Add the selectable things that we have to a list.
 * @param results List to add things to.
 */
void
RouteTimeAxisView::get_selectables (nframes_t start, nframes_t end, double top, double bot, list<Selectable*>& results)
{
	double speed = 1.0;
	
	if (get_diskstream() != 0) {
		speed = get_diskstream()->speed();
	}
	
	nframes_t start_adjusted = session_frame_to_track_frame(start, speed);
	nframes_t end_adjusted   = session_frame_to_track_frame(end, speed);

	if ((_view && ((top < 0.0 && bot < 0.0))) || touched (top, bot)) {
		_view->get_selectables (start_adjusted, end_adjusted, results);
	}

	/* pick up visible automation tracks */
	
	for (Children::iterator i = children.begin(); i != children.end(); ++i) {
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

	for (Children::iterator i = children.begin(); i != children.end(); ++i) {
		if (!(*i)->hidden()) {
			(*i)->get_inverted_selectables (sel, results);
		}
	}

	return;
}

bool
RouteTimeAxisView::show_automation(Evoral::Parameter param)
{
	return (_show_automation.find(param) != _show_automation.end());
}

/** Retuns 0 if track for \a param doesn't exist.
 */
RouteTimeAxisView::RouteAutomationNode*
RouteTimeAxisView::automation_track (Evoral::Parameter param)
{
	map<Evoral::Parameter, RouteAutomationNode*>::iterator i = _automation_tracks.find (param);

	if (i != _automation_tracks.end()) {
		return i->second;
	} else {
		return 0;
	}
}

/** Shorthand for GainAutomation, etc.
 */	
RouteTimeAxisView::RouteAutomationNode*
RouteTimeAxisView::automation_track (AutomationType type)
{
	return automation_track (Evoral::Parameter(type));
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
		_route->set_name (x);
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
	
	_editor.hide_track_in_display (*this);
	
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

nframes64_t 
RouteTimeAxisView::find_next_region_boundary (nframes64_t pos, int32_t dir)
{
	boost::shared_ptr<Diskstream> stream;
	boost::shared_ptr<Playlist> playlist;

	if ((stream = get_diskstream()) != 0 && (playlist = stream->playlist()) != 0) {
		return playlist->find_next_region_boundary (pos, dir);
	}

	return -1;
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
			_editor.get_cut_buffer().add (what_we_got);
			_session.add_command( new MementoCommand<Playlist>(*playlist.get(), &before, &playlist->get_state()));
			ret = true;
		}
		break;
	case Copy:
		if ((what_we_got = playlist->copy (time)) != 0) {
			_editor.get_cut_buffer().add (what_we_got);
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
	
	for (p = selection.playlists.begin(); p != selection.playlists.end() && nth; ++p, --nth) {}

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


TimeAxisView::Children
RouteTimeAxisView::get_child_list()
{
	TimeAxisView::Children redirect_children;
	
	for (Children::iterator i = children.begin(); i != children.end(); ++i) {
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

	delete playlist_menu;

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

	if (!edit_group() || !edit_group()->is_active()) {
		playlist_items.push_back (MenuElem (_("New"), bind(mem_fun(_editor, &PublicEditor::new_playlists), this)));
		playlist_items.push_back (MenuElem (_("New Copy"), bind(mem_fun(_editor, &PublicEditor::copy_playlists), this)));

	} else {
		// Use a label which tells the user what is happening
		playlist_items.push_back (MenuElem (_("New Take"), bind(mem_fun(_editor, &PublicEditor::new_playlists), this)));
		playlist_items.push_back (MenuElem (_("Copy Take"), bind(mem_fun(_editor, &PublicEditor::copy_playlists), this)));
		
	}

	playlist_items.push_back (SeparatorElem());
	playlist_items.push_back (MenuElem (_("Clear Current"), bind(mem_fun(_editor, &PublicEditor::clear_playlists), this)));
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
		if (get_diskstream()->playlist() == apl) {
			// radio button cotnrols mean this function is called for both the 
			// old and new playlist
			return;
		}
		get_diskstream()->use_playlist (apl);


		if (edit_group() && edit_group()->is_active()) {
			//PBD::stacktrace(cerr, 20);
			std::string group_string = "."+edit_group()->name()+".";

			std::string take_name = apl->name();
			std::string::size_type idx = take_name.find(group_string);

			if (idx == std::string::npos)
				return;

			take_name = take_name.substr(idx + group_string.length()); // find the bit containing the take number / name
			
			for (list<Route*>::const_iterator i = edit_group()->route_list().begin(); i != edit_group()->route_list().end(); ++i) {
				if ( (*i) == this->route().get()) {
					continue;
				}
				
				std::string playlist_name = (*i)->name()+group_string+take_name;

				Track *track = dynamic_cast<Track *>(*i);
				if (!track) {
					std::cerr << "route " << (*i)->name() << " is not a Track" << std::endl;
					continue;
				}

				boost::shared_ptr<Playlist> ipl = session().playlist_by_name(playlist_name);
				if (!ipl) {
					// No playlist for this track for this take yet, make it
					track->diskstream()->use_new_playlist();
					track->diskstream()->playlist()->set_name(playlist_name);
				} else {
					track->diskstream()->use_playlist(ipl);
				}
				
				//(*i)->get_dis
			}
		}
	}
}

void
RouteTimeAxisView::show_playlist_selector ()
{
	_editor.playlist_selector().show_for (this);
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
RouteTimeAxisView::color_handler ()
{
	//case cTimeStretchOutline:
	if (timestretch_rect) {
		timestretch_rect->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_TimeStretchOutline.get();
	}
	//case cTimeStretchFill:
	if (timestretch_rect) {
		timestretch_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_TimeStretchFill.get();
	}

	reset_meter();
}

void
RouteTimeAxisView::toggle_automation_track (Evoral::Parameter param)
{
	RouteAutomationNode* node = automation_track(param);

	if (!node)
		return;

	bool showit = node->menu_item->get_active();

	if (showit != node->track->marked_for_display()) {
		if (showit) {
			node->track->set_marked_for_display (true);
			node->track->canvas_display()->show();
			node->track->get_state_node()->add_property ("shown", X_("yes"));
		} else {
			node->track->set_marked_for_display (false);
			node->track->hide ();
			node->track->get_state_node()->add_property ("shown", X_("no"));
		}

		/* now trigger a redisplay */
		
		if (!no_redraw) {
			 _route->gui_changed (X_("track_height"), (void *) 0); /* EMIT_SIGNAL */
		}
	}
}

void
RouteTimeAxisView::automation_track_hidden (Evoral::Parameter param)
{
	RouteAutomationNode* ran = automation_track(param);
	if (!ran) {
		return;
	}
	
	// if Evoral::Parameter::operator< doesn't obey strict weak ordering, we may crash here....
	_show_automation.erase(param);
	ran->track->get_state_node()->add_property (X_("shown"), X_("no"));

	if (ran->menu_item && !_hidden) {
		ran->menu_item->set_active (false);
	}

	 _route->gui_changed ("visible_tracks", (void *) 0); /* EMIT_SIGNAL */
}


void
RouteTimeAxisView::show_all_automation ()
{
	no_redraw = true;
	
	/* Show our automation */

	map<Evoral::Parameter, RouteAutomationNode*>::iterator i;
	for (i = _automation_tracks.begin(); i != _automation_tracks.end(); ++i) {
		i->second->track->set_marked_for_display (true);
		i->second->track->canvas_display()->show();
		i->second->track->get_state_node()->add_property ("shown", X_("yes"));
		i->second->menu_item->set_active(true);
	}


	/* Show processor automation */

	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {
		for (vector<ProcessorAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
			if ((*ii)->view == 0) {
				add_processor_automation_curve ((*i)->processor, (*ii)->what);
			} 

			(*ii)->menu_item->set_active (true);
		}
	}

	no_redraw = false;

	/* Redraw */

	 _route->gui_changed ("visible_tracks", (void *) 0); /* EMIT_SIGNAL */
}

void
RouteTimeAxisView::show_existing_automation ()
{
	no_redraw = true;
	
	/* Show our automation */

	map<Evoral::Parameter, RouteAutomationNode*>::iterator i;
	for (i = _automation_tracks.begin(); i != _automation_tracks.end(); ++i) {
		if (i->second->track->line() && i->second->track->line()->npoints() > 0) {
			i->second->track->set_marked_for_display (true);
			i->second->track->canvas_display()->show();
			i->second->track->get_state_node()->add_property ("shown", X_("yes"));
			i->second->menu_item->set_active(true);
		}
	}


	/* Show processor automation */

	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {
		for (vector<ProcessorAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
			if ((*ii)->view != 0 && (*i)->processor->data().control((*ii)->what)->list()->size() > 0) {
				(*ii)->menu_item->set_active (true);
			}
		}
	}

	no_redraw = false;
	
	_route->gui_changed ("visible_tracks", (void *) 0); /* EMIT_SIGNAL */
}

void
RouteTimeAxisView::hide_all_automation ()
{
	no_redraw = true;

	/* Hide our automation */

	for (map<Evoral::Parameter, RouteAutomationNode*>::iterator i = _automation_tracks.begin(); i != _automation_tracks.end(); ++i) {
		i->second->track->set_marked_for_display (false);
		i->second->track->hide ();
		i->second->track->get_state_node()->add_property ("shown", X_("no"));
		i->second->menu_item->set_active (false);
	}

	/* Hide processor automation */

	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {
		for (vector<ProcessorAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
			(*ii)->menu_item->set_active (false);
		}
	}

	_show_automation.clear();

	no_redraw = false;
	 _route->gui_changed ("visible_tracks", (void *) 0); /* EMIT_SIGNAL */
}


void
RouteTimeAxisView::region_view_added (RegionView* rv)
{
	/* XXX need to find out if automation children have automationstreamviews. If yes, no ghosts */
	if(is_audio_track()) {
		for (Children::iterator i = children.begin(); i != children.end(); ++i) {
			boost::shared_ptr<AutomationTimeAxisView> atv;
			
			if ((atv = boost::dynamic_pointer_cast<AutomationTimeAxisView> (*i)) != 0) {
				atv->add_ghost(rv);
			}
		}
	}

	for (UnderlayMirrorList::iterator i = _underlay_mirrors.begin(); i != _underlay_mirrors.end(); ++i) {
		(*i)->add_ghost(rv);
	}
}

RouteTimeAxisView::ProcessorAutomationInfo::~ProcessorAutomationInfo ()
{
	for (vector<ProcessorAutomationNode*>::iterator i = lines.begin(); i != lines.end(); ++i) {
		delete *i;
	}
}


RouteTimeAxisView::ProcessorAutomationNode::~ProcessorAutomationNode ()
{
	parent.remove_processor_automation_node (this);
}

void
RouteTimeAxisView::remove_processor_automation_node (ProcessorAutomationNode* pan)
{
	if (pan->view) {
		remove_child (pan->view);
	}
}

RouteTimeAxisView::ProcessorAutomationNode*
RouteTimeAxisView::find_processor_automation_node (boost::shared_ptr<Processor> processor, Evoral::Parameter what)
{
	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {

		if ((*i)->processor == processor) {

			for (vector<ProcessorAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
				if ((*ii)->what == what) {
					return *ii;
				}
			}
		}
	}

	return 0;
}

static string 
legalize_for_xml_node (string str)
{
	string::size_type pos;
	string legal_chars = "abcdefghijklmnopqrtsuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_=:";
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
RouteTimeAxisView::add_processor_automation_curve (boost::shared_ptr<Processor> processor, Evoral::Parameter what)
{
	string name;
	ProcessorAutomationNode* pan;

	if ((pan = find_processor_automation_node (processor, what)) == 0) {
		fatal << _("programming error: ")
		      << string_compose (X_("processor automation curve for %1:%2 not registered with track!"),
				  processor->name(), what)
		      << endmsg;
		/*NOTREACHED*/
		return;
	}

	if (pan->view) {
		return;
	}

	name = processor->describe_parameter (what);

	/* create a string that is a legal XML node name that can be used to refer to this redirect+port combination */

	/* FIXME: ew */

	char state_name[256];
	snprintf (state_name, sizeof (state_name), "Redirect-%s-%" PRIu32, legalize_for_xml_node (processor->name()).c_str(), what.id());

	boost::shared_ptr<AutomationControl> control
			= boost::dynamic_pointer_cast<AutomationControl>(processor->data().control(what, true));

	pan->view = boost::shared_ptr<AutomationTimeAxisView>(
			new AutomationTimeAxisView (_session, _route, processor, control,
				_editor, *this, false, parent_canvas, name, state_name));

	pan->view->Hiding.connect (bind (mem_fun(*this, &RouteTimeAxisView::processor_automation_track_hidden), pan, processor));

	if (!pan->view->marked_for_display()) {
		pan->view->hide ();
	} else {
		pan->menu_item->set_active (true);
	}

	add_child (pan->view);

	if (_view) {
		_view->foreach_regionview (mem_fun(*pan->view.get(), &TimeAxisView::add_ghost));
	}

	processor->mark_automation_visible (what, true);
}

void
RouteTimeAxisView::processor_automation_track_hidden (RouteTimeAxisView::ProcessorAutomationNode* pan, boost::shared_ptr<Processor> i)
{
	if (!_hidden) {
		pan->menu_item->set_active (false);
	}

	i->mark_automation_visible (pan->what, false);

	 _route->gui_changed ("visible_tracks", (void *) 0); /* EMIT_SIGNAL */
}

void
RouteTimeAxisView::add_existing_processor_automation_curves (boost::weak_ptr<Processor> p)
{
	boost::shared_ptr<Processor> processor (p.lock ());
	if (!processor) {
		return;
	}
	
	set<Evoral::Parameter> s;
	boost::shared_ptr<AutomationLine> al;

	processor->what_has_visible_data (s);

	for (set<Evoral::Parameter>::iterator i = s.begin(); i != s.end(); ++i) {
		
		if ((al = find_processor_automation_curve (processor, *i)) != 0) {
			al->queue_reset ();
		} else {
			add_processor_automation_curve (processor, (*i));
		}
	}
}

void
RouteTimeAxisView::add_automation_child(Evoral::Parameter param, boost::shared_ptr<AutomationTimeAxisView> track, bool show)
{
	using namespace Menu_Helpers;

	XMLProperty* prop;
	XMLNode* node;

	add_child (track);

	track->Hiding.connect (bind (mem_fun (*this, &RouteTimeAxisView::automation_track_hidden), param));

	bool hideit = (!show);

	if ((node = track->get_state_node()) != 0) {
		if  ((prop = node->property ("shown")) != 0) {
			if (prop->value() == "yes") {
				hideit = false;
			}
		} 
	}

	_automation_tracks.insert(std::make_pair(param, new RouteAutomationNode(param, NULL, track)));

	if (hideit) {
		track->hide ();
	} else {
		_show_automation.insert (param);


		if (!no_redraw) {
			_route->gui_changed ("visible_tracks", (void *) 0); /* EMIT_SIGNAL */
		}
	}

	build_display_menu();
}


void
RouteTimeAxisView::add_processor_to_subplugin_menu (boost::weak_ptr<Processor> p)
{
	boost::shared_ptr<Processor> processor (p.lock ());
	if (!processor) {
		return;
	}
	
	using namespace Menu_Helpers;
	ProcessorAutomationInfo *rai;
	list<ProcessorAutomationInfo*>::iterator x;
	
	const std::set<Evoral::Parameter>& automatable = processor->what_can_be_automated ();
	std::set<Evoral::Parameter> has_visible_automation;

	processor->what_has_visible_data(has_visible_automation);

	if (automatable.empty()) {
		return;
	}

	for (x = processor_automation.begin(); x != processor_automation.end(); ++x) {
		if ((*x)->processor == processor) {
			break;
		}
	}

	if (x == processor_automation.end()) {

		rai = new ProcessorAutomationInfo (processor);
		processor_automation.push_back (rai);

	} else {

		rai = *x;

	}

	/* any older menu was deleted at the top of processors_changed()
	   when we cleared the subplugin menu.
	*/

	rai->menu = manage (new Menu);
	MenuList& items = rai->menu->items();
	rai->menu->set_name ("ArdourContextMenu");

	items.clear ();

	for (std::set<Evoral::Parameter>::const_iterator i = automatable.begin(); i != automatable.end(); ++i) {

		ProcessorAutomationNode* pan;
		CheckMenuItem* mitem;
		
		string name = processor->describe_parameter (*i);
		
		items.push_back (CheckMenuElem (name));
		mitem = dynamic_cast<CheckMenuItem*> (&items.back());

		if (has_visible_automation.find((*i)) != has_visible_automation.end()) {
			mitem->set_active(true);
		}

		if ((pan = find_processor_automation_node (processor, *i)) == 0) {

			/* new item */
			
			pan = new ProcessorAutomationNode (*i, mitem, *this);
			
			rai->lines.push_back (pan);

		} else {

			pan->menu_item = mitem;

		}

		mitem->signal_toggled().connect (bind (mem_fun(*this, &RouteTimeAxisView::processor_menu_item_toggled), rai, pan));
	}

	/* add the menu for this processor, because the subplugin
	   menu is always cleared at the top of processors_changed().
	   this is the result of some poor design in gtkmm and/or
	   GTK+.
	*/

	subplugin_menu.items().push_back (MenuElem (processor->name(), *rai->menu));
	rai->valid = true;
}

void
RouteTimeAxisView::processor_menu_item_toggled (RouteTimeAxisView::ProcessorAutomationInfo* rai,
					       RouteTimeAxisView::ProcessorAutomationNode* pan)
{
	bool showit = pan->menu_item->get_active();
	bool redraw = false;

	if (pan->view == 0 && showit) {
		add_processor_automation_curve (rai->processor, pan->what);
		redraw = true;
	}

	if (pan->view && showit != pan->view->marked_for_display()) {

		if (showit) {
			pan->view->set_marked_for_display (true);
			pan->view->canvas_display()->show();
			pan->view->canvas_background()->show();
		} else {
			rai->processor->mark_automation_visible (pan->what, true);
			pan->view->set_marked_for_display (false);
			pan->view->hide ();
		}

		redraw = true;

	}

	if (redraw && !no_redraw) {

		/* now trigger a redisplay */
		
		 _route->gui_changed ("visible_tracks", (void *) 0); /* EMIT_SIGNAL */

	}
}

void
RouteTimeAxisView::processors_changed ()
{
	using namespace Menu_Helpers;
	
	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {
		(*i)->valid = false;
	}

	subplugin_menu.items().clear ();

	_route->foreach_processor (mem_fun (*this, &RouteTimeAxisView::add_processor_to_subplugin_menu));
	_route->foreach_processor (mem_fun (*this, &RouteTimeAxisView::add_existing_processor_automation_curves));

	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ) {

		list<ProcessorAutomationInfo*>::iterator tmp;

		tmp = i;
		++tmp;

		if (!(*i)->valid) {

			delete *i;
			processor_automation.erase (i);

		} 

		i = tmp;
	}

	/* change in visibility was possible */

	_route->gui_changed ("visible_tracks", this);
}

boost::shared_ptr<AutomationLine>
RouteTimeAxisView::find_processor_automation_curve (boost::shared_ptr<Processor> processor, Evoral::Parameter what)
{
	ProcessorAutomationNode* pan;

	if ((pan = find_processor_automation_node (processor, what)) != 0) {
		if (pan->view) {
			pan->view->line();
		} 
	}

	return boost::shared_ptr<AutomationLine>();
}

void
RouteTimeAxisView::reset_processor_automation_curves ()
{
	for (ProcessorAutomationCurves::iterator i = processor_automation_curves.begin(); i != processor_automation_curves.end(); ++i) {
		(*i)->reset();
	}
}

void
RouteTimeAxisView::update_rec_display ()
{
	RouteUI::update_rec_display ();
	name_entry.set_sensitive (!_route->record_enabled());
}
		
void
RouteTimeAxisView::set_layer_display (LayerDisplay d)
{
	if (_view) {
		_view->set_layer_display (d);
	}
}

LayerDisplay
RouteTimeAxisView::layer_display () const
{
	if (_view) {
		return _view->layer_display ();
	}

	/* we don't know, since we don't have a _view, so just return something */
	return Overlaid;
}

	

boost::shared_ptr<AutomationTimeAxisView>
RouteTimeAxisView::automation_child(Evoral::Parameter param)
{
	AutomationTracks::iterator i = _automation_tracks.find(param);
	if (i != _automation_tracks.end())
		return i->second->track;
	else
		return boost::shared_ptr<AutomationTimeAxisView>();
}

void
RouteTimeAxisView::fast_update ()
{
	gm.get_level_meter().update_meters ();
}

void
RouteTimeAxisView::hide_meter ()
{
	clear_meter ();
	gm.get_level_meter().hide_meters ();
}

void
RouteTimeAxisView::show_meter ()
{
	reset_meter ();
}

void
RouteTimeAxisView::reset_meter ()
{
	if (Config->get_show_track_meters()) {
		gm.get_level_meter().setup_meters (height-5);
	} else {
		hide_meter ();
	}
}

void
RouteTimeAxisView::clear_meter ()
{
	gm.get_level_meter().clear_meters ();
}

void
RouteTimeAxisView::meter_changed (void *src)
{
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &RouteTimeAxisView::meter_changed), src));
	reset_meter();
}

void
RouteTimeAxisView::io_changed (IOChange change, void *src)
{
	reset_meter ();
}

void
RouteTimeAxisView::build_underlay_menu(Gtk::Menu* parent_menu) {
	using namespace Menu_Helpers;

	if(!_underlay_streams.empty()) {
		MenuList& parent_items = parent_menu->items();
		Menu* gs_menu = manage (new Menu);
		gs_menu->set_name ("ArdourContextMenu");
		MenuList& gs_items = gs_menu->items();
		
		parent_items.push_back (MenuElem (_("Underlays"), *gs_menu));
		
		for(UnderlayList::iterator it = _underlay_streams.begin(); it != _underlay_streams.end(); ++it) {
			gs_items.push_back(MenuElem(string_compose(_("Remove \"%1\""), (*it)->trackview().name()),
						    bind(mem_fun(*this, &RouteTimeAxisView::remove_underlay), *it)));
		}
	}
}

bool
RouteTimeAxisView::set_underlay_state() 
{
	if(!underlay_xml_node) {
		return false;
	}

	XMLNodeList nlist = underlay_xml_node->children();
	XMLNodeConstIterator niter;
	XMLNode *child_node;
	
	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		child_node = *niter;

		if(child_node->name() != "Underlay") {
			continue;
		}

		XMLProperty* prop = child_node->property ("id");
		if (prop) {
			PBD::ID id (prop->value());

			RouteTimeAxisView* v = _editor.get_route_view_by_id (id);

			if (v) {
				add_underlay(v->view(), false);
			}
		}
	}

	return false;
}

void
RouteTimeAxisView::add_underlay(StreamView* v, bool update_xml) 
{
	if(!v) {
		return;
	}

	RouteTimeAxisView& other = v->trackview();

	if(find(_underlay_streams.begin(), _underlay_streams.end(), v) == _underlay_streams.end()) {
		if(find(other._underlay_mirrors.begin(), other._underlay_mirrors.end(), this) != other._underlay_mirrors.end()) {
			fatal << _("programming error: underlay reference pointer pairs are inconsistent!") << endmsg;
			/*NOTREACHED*/
		}

		_underlay_streams.push_back(v);
		other._underlay_mirrors.push_back(this);

		v->foreach_regionview(mem_fun(*this, &RouteTimeAxisView::add_ghost));

		if(update_xml) {
			if(!underlay_xml_node) {
				ensure_xml_node();
				underlay_xml_node = xml_node->add_child("Underlays");
			}

			XMLNode* node = underlay_xml_node->add_child("Underlay");
			XMLProperty* prop = node->add_property("id");
			prop->set_value(v->trackview().route()->id().to_s());
		}
	}
}

void
RouteTimeAxisView::remove_underlay(StreamView* v) 
{
	if(!v) {
		return;
	}

	UnderlayList::iterator it = find(_underlay_streams.begin(), _underlay_streams.end(), v);
	RouteTimeAxisView& other = v->trackview();

	if(it != _underlay_streams.end()) {
		UnderlayMirrorList::iterator gm = find(other._underlay_mirrors.begin(), other._underlay_mirrors.end(), this);

		if(gm == other._underlay_mirrors.end()) {
			fatal << _("programming error: underlay reference pointer pairs are inconsistent!") << endmsg;
			/*NOTREACHED*/
		}

		v->foreach_regionview(mem_fun(*this, &RouteTimeAxisView::remove_ghost));

		_underlay_streams.erase(it);
		other._underlay_mirrors.erase(gm);

		if(underlay_xml_node) {
			underlay_xml_node->remove_nodes_and_delete("id", v->trackview().route()->id().to_s());
		}
	}
}

