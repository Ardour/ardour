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
#include <vector>

#include <sigc++/bind.h>

#include <pbd/error.h>
#include <pbd/stl_delete.h>
#include <pbd/whitespace.h>

#include <gtkmm2ext/bindable_button.h>
#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/utils.h>

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
#include "audio_time_axis.h"
#include "automation_gain_line.h"
#include "automation_pan_line.h"
#include "automation_time_axis.h"
#include "canvas_impl.h"
#include "crossfade_view.h"
#include "enums.h"
#include "gain_automation_time_axis.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "pan_automation_time_axis.h"
#include "playlist_selector.h"
#include "plugin_selector.h"
#include "plugin_ui.h"
#include "point_selection.h"
#include "prompter.h"
#include "public_editor.h"
#include "redirect_automation_line.h"
#include "redirect_automation_time_axis.h"
#include "regionview.h"
#include "rgb_macros.h"
#include "selection.h"
#include "simplerect.h"
#include "streamview.h"
#include "utils.h"

#include <ardour/audio_track.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace LADSPA;
using namespace Gtk;
using namespace Editing;


AudioTimeAxisView::AudioTimeAxisView (PublicEditor& ed, Session& sess, Route& rt, Canvas& canvas)
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
	subplugin_menu.set_name ("ArdourContextMenu");
	playlist_menu = 0;
	playlist_action_menu = 0;
	automation_action_menu = 0;
	gain_track = 0;
	pan_track = 0;
	view = 0;
	timestretch_rect = 0;
	waveform_item = 0;
	pan_automation_item = 0;
	gain_automation_item = 0;
	no_redraw = false;

	view = new StreamView (*this);

	add_gain_automation_child ();
	add_pan_automation_child ();

	ignore_toggle = false;

	rec_enable_button->set_active (false);
	mute_button->set_active (false);
	solo_button->set_active (false);
	
	rec_enable_button->set_name ("TrackRecordEnableButton");
	mute_button->set_name ("TrackMuteButton");
	solo_button->set_name ("SoloButton");
	edit_group_button.set_name ("TrackGroupButton");
	playlist_button.set_name ("TrackPlaylistButton");
	automation_button.set_name ("TrackAutomationButton");
	size_button.set_name ("TrackSizeButton");
	visual_button.set_name ("TrackVisualButton");
	hide_button.set_name ("TrackRemoveButton");

	hide_button.add (*(manage (new Image (get_xpm("small_x.xpm")))));
	
	_route.mute_changed.connect (mem_fun(*this, &RouteUI::mute_changed));
	_route.solo_changed.connect (mem_fun(*this, &RouteUI::solo_changed));
	_route.solo_safe_changed.connect (mem_fun(*this, &RouteUI::solo_changed));

	_route.panner().Changed.connect (mem_fun(*this, &AudioTimeAxisView::update_pans));

	solo_button->signal_button_press_event().connect (mem_fun (*this, &AudioTimeAxisView::select_me), false);
	mute_button->signal_button_press_event().connect (mem_fun (*this, &AudioTimeAxisView::select_me), false);
	rec_enable_button->signal_button_press_event().connect (mem_fun (*this, &AudioTimeAxisView::select_me), false);
	playlist_button.signal_button_press_event().connect (mem_fun (*this, &AudioTimeAxisView::select_me), false);
	automation_button.signal_button_press_event().connect (mem_fun (*this, &AudioTimeAxisView::select_me), false);
	size_button.signal_button_press_event().connect (mem_fun (*this, &AudioTimeAxisView::select_me), false);
	visual_button.signal_button_press_event().connect (mem_fun (*this, &AudioTimeAxisView::select_me), false);
	hide_button.signal_button_press_event().connect (mem_fun (*this, &AudioTimeAxisView::select_me), false);

	solo_button->signal_button_press_event().connect (mem_fun(*this, &RouteUI::solo_press), false);
	solo_button->signal_button_release_event().connect (mem_fun(*this, &RouteUI::solo_release), false);
	mute_button->signal_button_press_event().connect (mem_fun(*this, &RouteUI::mute_press), false);
	mute_button->signal_button_release_event().connect (mem_fun(*this, &RouteUI::mute_release), false);
	rec_enable_button->signal_button_press_event().connect (mem_fun(*this, &RouteUI::rec_enable_press));
 	edit_group_button.signal_button_release_event().connect (mem_fun(*this, &AudioTimeAxisView::edit_click), false);
	playlist_button.signal_clicked().connect (mem_fun(*this, &AudioTimeAxisView::playlist_click));
	automation_button.signal_clicked().connect (mem_fun(*this, &AudioTimeAxisView::automation_click));
	size_button.signal_button_release_event().connect (mem_fun(*this, &AudioTimeAxisView::size_click), false);
	visual_button.signal_clicked().connect (mem_fun(*this, &AudioTimeAxisView::visual_click));
	hide_button.signal_clicked().connect (mem_fun(*this, &AudioTimeAxisView::hide_click));

	if (is_audio_track()) {
		controls_table.attach (*rec_enable_button, 5, 6, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);
	}
	controls_table.attach (*mute_button, 6, 7, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);
	controls_table.attach (*solo_button, 7, 8, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::FILL|Gtk::EXPAND, 0, 0);

	controls_table.attach (edit_group_button, 6, 7, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);

	ARDOUR_UI::instance()->tooltips().set_tip(*rec_enable_button, _("Record"));
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

	if (is_audio_track() && audio_track()->mode() == ARDOUR::Normal) {
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

	/* map current state of the route */

	update_diskstream_display ();
	solo_changed(0);
	mute_changed(0);
	redirects_changed (0);
	reset_redirect_automation_curves ();
	y_position = -1;

	ensure_xml_node ();

	set_state (*xml_node);
	
	_route.mute_changed.connect (mem_fun(*this, &RouteUI::mute_changed));
	_route.solo_changed.connect (mem_fun(*this, &RouteUI::solo_changed));
	_route.redirects_changed.connect (mem_fun(*this, &AudioTimeAxisView::redirects_changed));

	_route.name_changed.connect (mem_fun(*this, &AudioTimeAxisView::route_name_changed));

	if (is_audio_track()) {

		/* track */

		audio_track()->FreezeChange.connect (mem_fun(*this, &AudioTimeAxisView::map_frozen));

		audio_track()->diskstream_changed.connect (mem_fun(*this, &AudioTimeAxisView::diskstream_changed));
		get_diskstream()->speed_changed.connect (mem_fun(*this, &AudioTimeAxisView::speed_changed));

		controls_ebox.set_name ("AudioTrackControlsBaseUnselected");
		controls_base_selected_name = "AudioTrackControlsBaseSelected";
		controls_base_unselected_name = "AudioTrackControlsBaseUnselected";

		/* ask for notifications of any new RegionViews */

		view->AudioRegionViewAdded.connect (mem_fun(*this, &AudioTimeAxisView::region_view_added));

		view->attach ();

		/* pick up the correct freeze state */

		map_frozen ();

	} else {

		/* bus */

		controls_ebox.set_name ("BusControlsBaseUnselected");
		controls_base_selected_name = "BusControlsBaseSelected";
		controls_base_unselected_name = "BusControlsBaseUnselected";
	}

	editor.ZoomChanged.connect (mem_fun(*this, &AudioTimeAxisView::reset_samples_per_unit));
	ColorChanged.connect (mem_fun (*this, &AudioTimeAxisView::color_handler));
}

AudioTimeAxisView::~AudioTimeAxisView ()
{
	GoingAway (); /* EMIT_SIGNAL */

 	if (playlist_menu) {
 		delete playlist_menu;
 		playlist_menu = 0;
 	}
  
	if (playlist_action_menu) {
		delete playlist_action_menu;
		playlist_action_menu = 0;
	}

	vector_delete (&redirect_automation_curves);

	for (list<RedirectAutomationInfo*>::iterator i = redirect_automation.begin(); i != redirect_automation.end(); ++i) {
		delete *i;
	}

	if (view) {
		delete view;
		view = 0;
	}
}

guint32
AudioTimeAxisView::show_at (double y, int& nth, Gtk::VBox *parent)
{
	ensure_xml_node ();
	xml_node->add_property ("shown_editor", "yes");
		
	return TimeAxisView::show_at (y, nth, parent);
}

void
AudioTimeAxisView::hide ()
{
	ensure_xml_node ();
	xml_node->add_property ("shown_editor", "no");

	TimeAxisView::hide ();
}

void
AudioTimeAxisView::set_playlist (AudioPlaylist *newplaylist)
{
	AudioPlaylist *pl;

	modified_connection.disconnect ();
	state_changed_connection.disconnect ();
	
	if ((pl = dynamic_cast<AudioPlaylist*> (playlist())) != 0) {
		state_changed_connection = pl->StateChanged.connect (mem_fun(*this, &AudioTimeAxisView::playlist_state_changed));
		modified_connection = pl->Modified.connect (mem_fun(*this, &AudioTimeAxisView::playlist_modified));
	}
}

void
AudioTimeAxisView::playlist_modified ()
{
}

gint
AudioTimeAxisView::edit_click (GdkEventButton *ev)
{
	if (Keyboard::modifier_state_equals (ev->state, Keyboard::Control)) {
	        _route.set_edit_group (0, this);
		return FALSE;
	} 

	using namespace Menu_Helpers;

	MenuList& items = edit_group_menu.items ();
	RadioMenuItem::Group group;

	items.clear ();
	items.push_back (RadioMenuElem (group, _("No group"), 
					bind (mem_fun(*this, &AudioTimeAxisView::set_edit_group_from_menu), (RouteGroup *) 0)));
	
	if (_route.edit_group() == 0) {
		static_cast<RadioMenuItem*>(&items.back())->set_active ();
	}
	
	_session.foreach_edit_group (bind (mem_fun (*this, &AudioTimeAxisView::add_edit_group_menu_item), &group));
	edit_group_menu.popup (ev->button, ev->time);

	return FALSE;
}

void
AudioTimeAxisView::add_edit_group_menu_item (RouteGroup *eg, RadioMenuItem::Group* group)
{
	using namespace Menu_Helpers;

	MenuList &items = edit_group_menu.items();

	items.push_back (RadioMenuElem (*group, eg->name(), bind (mem_fun(*this, &AudioTimeAxisView::set_edit_group_from_menu), eg)));
	if (_route.edit_group() == eg) {
		static_cast<RadioMenuItem*>(&items.back())->set_active ();
	}
}

void
AudioTimeAxisView::set_edit_group_from_menu (RouteGroup *eg)

{
	_route.set_edit_group (eg, this);
}

void
AudioTimeAxisView::playlist_state_changed (Change ignored)
{
	// ENSURE_GUI_THREAD (bind (mem_fun(*this, &AudioTimeAxisView::playlist_state_changed), ignored));
	// why are we here ?
}

void
AudioTimeAxisView::playlist_changed ()

{
	label_view ();

	if (is_audio_track()) {
		set_playlist (get_diskstream()->playlist());
	}
}

void
AudioTimeAxisView::label_view ()
{
	string x = _route.name();

	if (x != name_entry.get_text()) {
		name_entry.set_text (x);
	}

	ARDOUR_UI::instance()->tooltips().set_tip (name_entry, x);
}

void
AudioTimeAxisView::route_name_changed (void *src)
{
	editor.route_name_changed (this);
	label_view ();
}

void
AudioTimeAxisView::take_name_changed (void *src)

{
	if (src != this) {
		label_view ();
	}
}

void
AudioTimeAxisView::playlist_click ()
{
	// always build a new action menu
	
	if (playlist_action_menu == 0) {
		playlist_action_menu = new Menu;
		playlist_action_menu->set_name ("ArdourContextMenu");
	}
	
 	build_playlist_menu(playlist_action_menu);

	playlist_action_menu->popup (1, 0);
}

void
AudioTimeAxisView::automation_click ()
{
	if (automation_action_menu == 0) {
		/* this seems odd, but the automation action
		   menu is built as part of the display menu.
		*/
		build_display_menu ();
	}
	automation_action_menu->popup (1, 0);
}

void
AudioTimeAxisView::show_timestretch (jack_nframes_t start, jack_nframes_t end)
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

	if (!(ts.track == this || (ts.group != 0 && ts.group == _route.edit_group()))) {
		/* this doesn't apply to us */
		return;
	}

	/* ignore it if our edit group is not active */
	
	if ((ts.track != this) && _route.edit_group() && !_route.edit_group()->is_active()) {
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
AudioTimeAxisView::hide_timestretch ()
{
	TimeAxisView::hide_timestretch ();

	if (timestretch_rect) {
		timestretch_rect->hide ();
	}
}

void
AudioTimeAxisView::show_selection (TimeSelection& ts)
{

#if 0
	/* ignore it if our edit group is not active or if the selection was started
	   in some other track or edit group (remember that edit_group() == 0 means
	   that the track is not in an edit group).
	*/

	if (((ts.track != this && !is_child (ts.track)) && _route.edit_group() && !_route.edit_group()->is_active()) ||
	    (!(ts.track == this || is_child (ts.track) || (ts.group != 0 && ts.group == _route.edit_group())))) {
		hide_selection ();
		return;
	}
#endif

	TimeAxisView::show_selection (ts);
}

void
AudioTimeAxisView::set_state (const XMLNode& node)
{
	const XMLProperty *prop;
	
	TimeAxisView::set_state (node);
	
	if ((prop = node.property ("shown_editor")) != 0) {
		if (prop->value() == "no") {
			_marked_for_display = false;
		} else {
			_marked_for_display = true;
		}
	} else {
		_marked_for_display = true;
	}
	
	XMLNodeList nlist = node.children();
	XMLNodeConstIterator niter;
	XMLNode *child_node;
	
	
	show_gain_automation = false;
	show_pan_automation  = false;
	
	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		child_node = *niter;

		if (child_node->name() == "gain") {
			XMLProperty *prop=child_node->property ("shown");
			
			if (prop != 0) {
				if (prop->value() == "yes") {
					show_gain_automation = true;
				}
			}
			continue;
		}
		
		if (child_node->name() == "pan") {
			XMLProperty *prop=child_node->property ("shown");
			
			if (prop != 0) {
				if (prop->value() == "yes") {
					show_pan_automation = true;
				}			
			}
			continue;
		}
	}
}

void
AudioTimeAxisView::set_height (TrackHeight h)
{
	bool height_changed = (h != height_style);

	TimeAxisView::set_height (h);

	ensure_xml_node ();

	view->set_height ((double) height);

	switch (height_style) {
	case Largest:
		xml_node->add_property ("track_height", "largest");
		show_name_entry ();
		hide_name_label ();
		controls_table.show_all();
		break;
	case Large:
		xml_node->add_property ("track_height", "large");
		show_name_entry ();
		hide_name_label ();
		controls_table.show_all();
		break;
	case Larger:
		xml_node->add_property ("track_height", "larger");
		show_name_entry ();
		hide_name_label ();
		controls_table.show_all();
		break;
	case Normal:
		xml_node->add_property ("track_height", "normal");
		show_name_entry ();
		hide_name_label ();
		controls_table.show_all();
		break;
	case Smaller:
		xml_node->add_property ("track_height", "smaller");
		controls_table.show_all ();
		show_name_entry ();
		hide_name_label ();
		edit_group_button.hide ();
		hide_button.hide ();
		visual_button.hide ();
		size_button.hide ();
		automation_button.hide ();
		playlist_button.hide ();
		break;
	case Small:
		xml_node->add_property ("track_height", "small");
		controls_table.hide_all ();
		controls_table.show ();
		hide_name_entry ();
		show_name_label ();
		name_label.set_text (_route.name());
		break;
	}

	if (height_changed) {
		/* only emit the signal if the height really changed */
		 _route.gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
	}
}

void
AudioTimeAxisView::select_track_color ()
{
	if (RouteUI::choose_color ()) {

		if (view) {
			view->apply_color (_color, StreamView::RegionColor);
		}
	}
}

void
AudioTimeAxisView::reset_redirect_automation_curves ()
{
	for (vector<RedirectAutomationLine*>::iterator i = redirect_automation_curves.begin(); i != redirect_automation_curves.end(); ++i) {
		(*i)->reset();
	}
}

void
AudioTimeAxisView::reset_samples_per_unit ()
{
	set_samples_per_unit (editor.get_current_zoom());
}

void
AudioTimeAxisView::set_samples_per_unit (double spu)
{
	double speed = 1.0;

	if (get_diskstream() != 0) {
		speed = get_diskstream()->speed();
	}
	
	if (view) {
		view->set_samples_per_unit (spu * speed);
	}

	TimeAxisView::set_samples_per_unit (spu * speed);
}

void
AudioTimeAxisView::build_display_menu ()
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
	items.push_back (MenuElem (_("Color"), mem_fun(*this, &AudioTimeAxisView::select_track_color)));


	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Hide all crossfades"), mem_fun(*this, &AudioTimeAxisView::hide_all_xfades)));
	items.push_back (MenuElem (_("Show all crossfades"), mem_fun(*this, &AudioTimeAxisView::show_all_xfades)));
	items.push_back (SeparatorElem());

	build_remote_control_menu ();
	items.push_back (MenuElem (_("Remote Control ID"), *remote_control_menu));

	automation_action_menu = manage (new Menu);
	MenuList& automation_items = automation_action_menu->items();
	automation_action_menu->set_name ("ArdourContextMenu");
	
	automation_items.push_back (MenuElem (_("show all automation"),
					      mem_fun(*this, &AudioTimeAxisView::show_all_automation)));

	automation_items.push_back (MenuElem (_("show existing automation"),
					      mem_fun(*this, &AudioTimeAxisView::show_existing_automation)));

	automation_items.push_back (MenuElem (_("hide all automation"),
					      mem_fun(*this, &AudioTimeAxisView::hide_all_automation)));

	automation_items.push_back (SeparatorElem());

	automation_items.push_back (CheckMenuElem (_("gain"), 
						   mem_fun(*this, &AudioTimeAxisView::toggle_gain_track)));
	gain_automation_item = static_cast<CheckMenuItem*> (&automation_items.back());
	gain_automation_item->set_active(show_gain_automation);

	automation_items.push_back (CheckMenuElem (_("pan"),
						   mem_fun(*this, &AudioTimeAxisView::toggle_pan_track)));
	pan_automation_item = static_cast<CheckMenuItem*> (&automation_items.back());
	pan_automation_item->set_active(show_pan_automation);

	automation_items.push_back (MenuElem (_("Plugins"), subplugin_menu));

	items.push_back (MenuElem (_("Automation"), *automation_action_menu));

	Menu *waveform_menu = manage(new Menu);
	MenuList& waveform_items = waveform_menu->items();
	waveform_menu->set_name ("ArdourContextMenu");
	
	waveform_items.push_back (CheckMenuElem (_("Show waveforms"), mem_fun(*this, &AudioTimeAxisView::toggle_waveforms)));
	waveform_item = static_cast<CheckMenuItem *> (&waveform_items.back());
	ignore_toggle = true;
	waveform_item->set_active (editor.show_waveforms());
	ignore_toggle = false;

	RadioMenuItem::Group group;

	waveform_items.push_back (RadioMenuElem (group, _("Traditional"), bind (mem_fun(*this, &AudioTimeAxisView::set_waveform_shape), Traditional)));
	traditional_item = static_cast<RadioMenuItem *> (&waveform_items.back());

	waveform_items.push_back (RadioMenuElem (group, _("Rectified"), bind (mem_fun(*this, &AudioTimeAxisView::set_waveform_shape), Rectified)));
	rectified_item = static_cast<RadioMenuItem *> (&waveform_items.back());

	items.push_back (MenuElem (_("Waveform"), *waveform_menu));

	if (is_audio_track()) {

		Menu* alignment_menu = manage (new Menu);
		MenuList& alignment_items = alignment_menu->items();
		alignment_menu->set_name ("ArdourContextMenu");

		RadioMenuItem::Group align_group;
		
		alignment_items.push_back (RadioMenuElem (align_group, _("align with existing material"), bind (mem_fun(*this, &AudioTimeAxisView::set_align_style), ExistingMaterial)));
		align_existing_item = dynamic_cast<RadioMenuItem*>(&alignment_items.back());
		if (get_diskstream()->alignment_style() == ExistingMaterial) {
			align_existing_item->set_active();
		}
		alignment_items.push_back (RadioMenuElem (align_group, _("align with capture time"), bind (mem_fun(*this, &AudioTimeAxisView::set_align_style), CaptureTime)));
		align_capture_item = dynamic_cast<RadioMenuItem*>(&alignment_items.back());
		if (get_diskstream()->alignment_style() == CaptureTime) {
			align_capture_item->set_active();
		}
		
		items.push_back (MenuElem (_("Alignment"), *alignment_menu));

		get_diskstream()->AlignmentStyleChanged.connect (mem_fun(*this, &AudioTimeAxisView::align_style_changed));
	}

	items.push_back (SeparatorElem());
	items.push_back (CheckMenuElem (_("Active"), mem_fun(*this, &RouteUI::toggle_route_active)));
	route_active_menu_item = dynamic_cast<CheckMenuItem *> (&items.back());
	route_active_menu_item->set_active (_route.active());

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Remove"), mem_fun(*this, &RouteUI::remove_this_route)));

}

void
AudioTimeAxisView::align_style_changed ()
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
AudioTimeAxisView::set_align_style (AlignStyle style)
{
	get_diskstream()->set_align_style (style);
}

void
AudioTimeAxisView::rename_current_playlist ()
{
	ArdourPrompter prompter (true);
	string name;

	AudioPlaylist *pl;
	DiskStream *ds;

	if (((ds = get_diskstream()) == 0) || ds->destructive() || ((pl = ds->playlist()) == 0)) {
		return;
	}

	prompter.set_prompt (_("Name for playlist"));
	prompter.set_initial_text (pl->name());
	prompter.add_button (_("Rename"), Gtk::RESPONSE_ACCEPT);

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
AudioTimeAxisView::use_copy_playlist (bool prompt)
{
	AudioPlaylist *pl;
	DiskStream *ds;
	string name;

	if (((ds = get_diskstream()) == 0) || ds->destructive() || ((pl = ds->playlist()) == 0)) {
		return;
	}
	
	name = Playlist::bump_name (pl->name(), _session);

	if (prompt) {

		ArdourPrompter prompter (true);
		
		prompter.set_prompt (_("Name for Playlist"));
		prompter.set_initial_text (name);
		prompter.add_button (Gtk::Stock::NEW, Gtk::RESPONSE_ACCEPT);
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
		pl = ds->playlist();
		pl->set_name (name);
	}
}

void
AudioTimeAxisView::use_new_playlist (bool prompt)
{
	AudioPlaylist *pl;
	DiskStream *ds;
	string name;

	if (((ds = get_diskstream()) == 0) || ds->destructive() || ((pl = ds->playlist()) == 0)) {
		return;
	}
	
	name = Playlist::bump_name (pl->name(), _session);

	if (prompt) {
		
		ArdourPrompter prompter (true);
		
		prompter.set_prompt (_("Name for Playlist"));
		prompter.set_initial_text (name);
		prompter.add_button (Gtk::Stock::NEW, Gtk::RESPONSE_ACCEPT);
		
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
		pl = ds->playlist();
		pl->set_name (name);
	}
}

void
AudioTimeAxisView::clear_playlist ()
{
	AudioPlaylist *pl;
	DiskStream *ds;
	
	if ((ds = get_diskstream()) != 0) {
		if ((pl = ds->playlist()) != 0) {
			editor.clear_playlist (*pl);
		}
	}
}

void
AudioTimeAxisView::toggle_waveforms ()
{
	if (view && waveform_item && !ignore_toggle) {
		view->set_show_waveforms (waveform_item->get_active());
	}
}

void
AudioTimeAxisView::set_show_waveforms (bool yn)
{
	if (waveform_item) {
		waveform_item->set_active (yn);
	} else {
		view->set_show_waveforms (yn);
	}
}

void
AudioTimeAxisView::set_show_waveforms_recording (bool yn)
{
	if (view) {
		view->set_show_waveforms_recording (yn);
	}
}

void
AudioTimeAxisView::set_waveform_shape (WaveformShape shape)
{
	if (view) {
		view->set_waveform_shape (shape);
	}
}

void
AudioTimeAxisView::speed_changed ()
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun(*this, &AudioTimeAxisView::reset_samples_per_unit));
}

void
AudioTimeAxisView::diskstream_changed (void *src)
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun(*this, &AudioTimeAxisView::update_diskstream_display));
}	

void
AudioTimeAxisView::update_diskstream_display ()
{
	DiskStream *ds;

	if ((ds = get_diskstream()) != 0) {
		set_playlist (ds->playlist ());
	}

	map_frozen ();
}	

void
AudioTimeAxisView::selection_click (GdkEventButton* ev)
{
	PublicEditor::TrackViewList* tracks = editor.get_valid_views (this, _route.edit_group());

	switch (Keyboard::selection_type (ev->state)) {
	case Selection::Toggle:
		/* XXX this is not right */
		editor.get_selection().add (*tracks);
		break;
		
	case Selection::Set:
		editor.get_selection().set (*tracks);
		break;

	case Selection::Extend:
		/* not defined yet */
		break;
	}

	delete tracks;
}

void
AudioTimeAxisView::set_selected_regionviews (AudioRegionSelection& regions)
{
	if (view) {
		view->set_selected_regionviews (regions);
	}
}

void
AudioTimeAxisView::set_selected_points (PointSelection& points)
{
	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->set_selected_points (points);
	}
}

void
AudioTimeAxisView::get_selectables (jack_nframes_t start, jack_nframes_t end, double top, double bot, list<Selectable*>& results)
{
	double speed = 1.0;
	
	if (get_diskstream() != 0) {
		speed = get_diskstream()->speed();
	}
	
	jack_nframes_t start_adjusted = session_frame_to_track_frame(start, speed);
	jack_nframes_t end_adjusted   = session_frame_to_track_frame(end, speed);

	if (view && touched (top, bot)) {
		view->get_selectables (start_adjusted, end_adjusted, results);
	}

	/* pick up visible automation tracks */
	
	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		if (!(*i)->hidden()) {
			(*i)->get_selectables (start_adjusted, end_adjusted, top, bot, results);
		}
	}
}

void
AudioTimeAxisView::get_inverted_selectables (Selection& sel, list<Selectable*>& results)
{
	if (view) {
		view->get_inverted_selectables (sel, results);
	}

	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		if (!(*i)->hidden()) {
			(*i)->get_inverted_selectables (sel, results);
		}
	}

	return;
}

RouteGroup*
AudioTimeAxisView::edit_group() const
{
	return _route.edit_group();
}

string
AudioTimeAxisView::name() const
{
	return _route.name();
}

Playlist *
AudioTimeAxisView::playlist () const 
{
	DiskStream *ds;

	if ((ds = get_diskstream()) != 0) {
		return ds->playlist(); 
	} else {
		return 0; 
	}
}

void
AudioTimeAxisView::name_entry_changed ()
{
	string x;

	x = name_entry.get_text ();
	
	if (x == _route.name()) {
		return;
	}

	if (x.length() == 0) {
		name_entry.set_text (_route.name());
		return;
	}

	strip_whitespace_edges(x);

	if (_session.route_name_unique (x)) {
		_route.set_name (x, this);
	} else {
		ARDOUR_UI::instance()->popup_error (_("a track already exists with that name"));
		name_entry.set_text (_route.name());
	}
}

void
AudioTimeAxisView::visual_click ()
{
	popup_display_menu (0);
}

void
AudioTimeAxisView::hide_click ()
{
	editor.hide_track_in_display (*this);
}

Region*
AudioTimeAxisView::find_next_region (jack_nframes_t pos, RegionPoint point, int32_t dir)
{
	DiskStream *stream;
	AudioPlaylist *playlist;

	if ((stream = get_diskstream()) != 0 && (playlist = stream->playlist()) != 0) {
		return playlist->find_next_region (pos, point, dir);
	}

	return 0;
}

void
AudioTimeAxisView::add_gain_automation_child ()
{
	XMLProperty* prop;
	AutomationLine* line;

	gain_track = new GainAutomationTimeAxisView (_session,
						     _route,
						     editor,
						     *this,
						     parent_canvas,
						     _("gain"),
						     _route.gain_automation_curve());
	
	line = new AutomationGainLine ("automation gain",
				       _session,
				       *gain_track,
				       *gain_track->canvas_display,
				       _route.gain_automation_curve());

	line->set_line_color (color_map[cAutomationLine]);
	

	gain_track->add_line (*line);

	add_child (gain_track);

	gain_track->Hiding.connect (mem_fun(*this, &AudioTimeAxisView::gain_hidden));

	bool hideit = true;
	
	XMLNode* node;

	if ((node = gain_track->get_state_node()) != 0) {
		if  ((prop = node->property ("shown")) != 0) {
			if (prop->value() == "yes") {
				hideit = false;
			}
		} 
	}

	if (hideit) {
		gain_track->hide ();
	}
}

void
AudioTimeAxisView::add_pan_automation_child ()
{
	XMLProperty* prop;

	pan_track = new PanAutomationTimeAxisView (_session, _route, editor, *this, parent_canvas, _("pan"));

	update_pans ();
	
	add_child (pan_track);

	pan_track->Hiding.connect (mem_fun(*this, &AudioTimeAxisView::pan_hidden));

	ensure_xml_node ();
	bool hideit = true;
	
	XMLNode* node;

	if ((node = pan_track->get_state_node()) != 0) {
		if ((prop = node->property ("shown")) != 0) {
			if (prop->value() == "yes") {
				hideit = false;
			}
		} 
	}

	if (hideit) {
		pan_track->hide ();
	}
}

void
AudioTimeAxisView::update_pans ()
{
	Panner::iterator p;
	
	pan_track->clear_lines ();
	
	/* we don't draw lines for "greater than stereo" panning.
	 */

	if (_route.n_outputs() > 2) {
		return;
	}

	for (p = _route.panner().begin(); p != _route.panner().end(); ++p) {

		AutomationLine* line;

		line = new AutomationPanLine ("automation pan", _session, *pan_track,
					      *pan_track->canvas_display, 
					      (*p)->automation());

		if (p == _route.panner().begin()) {
			/* first line is a nice orange */
			line->set_line_color (color_map[cLeftPanAutomationLine]);
		} else {
			/* second line is a nice blue */
			line->set_line_color (color_map[cRightPanAutomationLine]);
		}

		pan_track->add_line (*line);
	}
}
		
void
AudioTimeAxisView::toggle_gain_track ()
{

	bool showit = gain_automation_item->get_active();

	if (showit != gain_track->marked_for_display()) {
		if (showit) {
			gain_track->set_marked_for_display (true);
			gain_track->canvas_display->show();
			gain_track->get_state_node()->add_property ("shown", X_("yes"));
		} else {
			gain_track->set_marked_for_display (false);
			gain_track->hide ();
			gain_track->get_state_node()->add_property ("shown", X_("no"));
		}

		/* now trigger a redisplay */
		
		if (!no_redraw) {
			 _route.gui_changed (X_("track_height"), (void *) 0); /* EMIT_SIGNAL */
		}
	}
}

void
AudioTimeAxisView::gain_hidden ()
{
	gain_track->get_state_node()->add_property (X_("shown"), X_("no"));

	if (gain_automation_item && !_hidden) {
		gain_automation_item->set_active (false);
	}

	 _route.gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

void
AudioTimeAxisView::toggle_pan_track ()
{
	bool showit = pan_automation_item->get_active();

	if (showit != pan_track->marked_for_display()) {
		if (showit) {
			pan_track->set_marked_for_display (true);
			pan_track->canvas_display->show();
			pan_track->get_state_node()->add_property ("shown", X_("yes"));
		} else {
			pan_track->set_marked_for_display (false);
			pan_track->hide ();
			pan_track->get_state_node()->add_property ("shown", X_("no"));
		}

		/* now trigger a redisplay */
		
		if (!no_redraw) {
			 _route.gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
		}
	}
}

void
AudioTimeAxisView::pan_hidden ()
{
	pan_track->get_state_node()->add_property ("shown", "no");

	if (pan_automation_item && !_hidden) {
		pan_automation_item->set_active (false);
	}

	 _route.gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

AudioTimeAxisView::RedirectAutomationInfo::~RedirectAutomationInfo ()
{
	for (vector<RedirectAutomationNode*>::iterator i = lines.begin(); i != lines.end(); ++i) {
		delete *i;
	}
}


AudioTimeAxisView::RedirectAutomationNode::~RedirectAutomationNode ()
{
	parent.remove_ran (this);

	if (view) {
		delete view;
	}
}

void
AudioTimeAxisView::remove_ran (RedirectAutomationNode* ran)
{
	if (ran->view) {
		remove_child (ran->view);
	}
}

AudioTimeAxisView::RedirectAutomationNode*
AudioTimeAxisView::find_redirect_automation_node (Redirect *redirect, uint32_t what)
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
AudioTimeAxisView::add_redirect_automation_curve (Redirect *redirect, uint32_t what)
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

	ran->view->Hiding.connect (bind (mem_fun(*this, &AudioTimeAxisView::redirect_automation_track_hidden), ran, redirect));

	if (!ran->view->marked_for_display()) {
		ran->view->hide ();
	} else {
		ran->menu_item->set_active (true);
	}

	add_child (ran->view);

	view->foreach_regionview (bind (mem_fun(*this, &AudioTimeAxisView::add_ghost_to_redirect), ran->view));

	redirect->mark_automation_visible (what, true);
}

void
AudioTimeAxisView::redirect_automation_track_hidden (AudioTimeAxisView::RedirectAutomationNode* ran, Redirect* r)
{
	if (!_hidden) {
		ran->menu_item->set_active (false);
	}

	r->mark_automation_visible (ran->what, false);

	 _route.gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

void
AudioTimeAxisView::add_existing_redirect_automation_curves (Redirect *redirect)
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
AudioTimeAxisView::add_redirect_to_subplugin_menu (Redirect* r)
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

		mitem->signal_toggled().connect (bind (mem_fun(*this, &AudioTimeAxisView::redirect_menu_item_toggled), rai, ran));
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
AudioTimeAxisView::redirect_menu_item_toggled (AudioTimeAxisView::RedirectAutomationInfo* rai,
					       AudioTimeAxisView::RedirectAutomationNode* ran)
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
		
		 _route.gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */

	}
}

void
AudioTimeAxisView::redirects_changed (void *src)
{
	using namespace Menu_Helpers;

	for (list<RedirectAutomationInfo*>::iterator i = redirect_automation.begin(); i != redirect_automation.end(); ++i) {
		(*i)->valid = false;
	}

	subplugin_menu.items().clear ();

	_route.foreach_redirect (this, &AudioTimeAxisView::add_redirect_to_subplugin_menu);
	_route.foreach_redirect (this, &AudioTimeAxisView::add_existing_redirect_automation_curves);

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

	_route.gui_changed ("track_height", this);
}

RedirectAutomationLine *
AudioTimeAxisView::find_redirect_automation_curve (Redirect *redirect, uint32_t what)
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
AudioTimeAxisView::show_all_automation ()
{
	no_redraw = true;

	pan_automation_item->set_active (true);
	gain_automation_item->set_active (true);
	
	for (list<RedirectAutomationInfo*>::iterator i = redirect_automation.begin(); i != redirect_automation.end(); ++i) {
		for (vector<RedirectAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
			if ((*ii)->view == 0) {
				add_redirect_automation_curve ((*i)->redirect, (*ii)->what);
			} 

			(*ii)->menu_item->set_active (true);
		}
	}

	no_redraw = false;

	 _route.gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

void
AudioTimeAxisView::show_existing_automation ()
{
	no_redraw = true;

	pan_automation_item->set_active (true);
	gain_automation_item->set_active (true);

	for (list<RedirectAutomationInfo*>::iterator i = redirect_automation.begin(); i != redirect_automation.end(); ++i) {
		for (vector<RedirectAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
			if ((*ii)->view != 0) {
				(*ii)->menu_item->set_active (true);
			}
		}
	}

	no_redraw = false;

	 _route.gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

void
AudioTimeAxisView::hide_all_automation ()
{
	no_redraw = true;

	pan_automation_item->set_active (false);
	gain_automation_item->set_active (false);

	for (list<RedirectAutomationInfo*>::iterator i = redirect_automation.begin(); i != redirect_automation.end(); ++i) {
		for (vector<RedirectAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
			(*ii)->menu_item->set_active (false);
		}
	}

	no_redraw = false;
	 _route.gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

bool
AudioTimeAxisView::cut_copy_clear (Selection& selection, CutCopyOp op)
{
	Playlist* what_we_got;
	DiskStream* ds = get_diskstream();
	Playlist* playlist;
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
	
	switch (op) {
	case Cut:
		_session.add_undo (playlist->get_memento());
		if ((what_we_got = playlist->cut (time)) != 0) {
			editor.get_cut_buffer().add (what_we_got);
			_session.add_redo_no_execute (playlist->get_memento());
			ret = true;
		}
		break;
	case Copy:
		if ((what_we_got = playlist->copy (time)) != 0) {
			editor.get_cut_buffer().add (what_we_got);
		}
		break;

	case Clear:
		_session.add_undo (playlist->get_memento());
		if ((what_we_got = playlist->cut (time)) != 0) {
			_session.add_redo_no_execute (playlist->get_memento());
			what_we_got->unref ();
			ret = true;
		}
		break;
	}

	return ret;
}

bool
AudioTimeAxisView::paste (jack_nframes_t pos, float times, Selection& selection, size_t nth)
{
	if (!is_audio_track()) {
		return false;
	}

	Playlist* playlist = get_diskstream()->playlist();
	PlaylistSelection::iterator p;
	
	for (p = selection.playlists.begin(); p != selection.playlists.end() && nth; ++p, --nth);

	if (p == selection.playlists.end()) {
		return false;
	}

	if (get_diskstream()->speed() != 1.0f)
		pos = session_frame_to_track_frame(pos, get_diskstream()->speed() );
	
	_session.add_undo (playlist->get_memento());
	playlist->paste (**p, pos, times);
	_session.add_redo_no_execute (playlist->get_memento());

	return true;
}

void
AudioTimeAxisView::region_view_added (AudioRegionView* arv)
{
	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		AutomationTimeAxisView* atv;

		if ((atv = dynamic_cast<AutomationTimeAxisView*> (*i)) != 0) {
			arv->add_ghost (*atv);
		}
	}
}

void
AudioTimeAxisView::add_ghost_to_redirect (AudioRegionView* arv, AutomationTimeAxisView* atv)
{
	arv->add_ghost (*atv);
}

list<TimeAxisView*>
AudioTimeAxisView::get_child_list()
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
AudioTimeAxisView::build_playlist_menu (Gtk::Menu * menu)
{
	using namespace Menu_Helpers;

	if (!menu || !is_audio_track()) {
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

	playlist_items.push_back (MenuElem (string_compose (_("Current: %1"), get_diskstream()->playlist()->name())));
	playlist_items.push_back (SeparatorElem());
	
	playlist_items.push_back (MenuElem (_("Rename"), mem_fun(*this, &AudioTimeAxisView::rename_current_playlist)));
	playlist_items.push_back (SeparatorElem());

	playlist_items.push_back (MenuElem (_("New"), mem_fun(editor, &PublicEditor::new_playlists)));
	playlist_items.push_back (MenuElem (_("New Copy"), mem_fun(editor, &PublicEditor::copy_playlists)));
	playlist_items.push_back (SeparatorElem());
	playlist_items.push_back (MenuElem (_("Clear Current"), mem_fun(editor, &PublicEditor::clear_playlists)));
	playlist_items.push_back (SeparatorElem());
	playlist_items.push_back (MenuElem(_("Select"), mem_fun(*this, &AudioTimeAxisView::show_playlist_selector)));

}

void
AudioTimeAxisView::show_playlist_selector ()
{
	editor.playlist_selector().show_for (this);
}


void
AudioTimeAxisView::map_frozen ()
{
	if (!is_audio_track()) {
		return;
	}

	ENSURE_GUI_THREAD (mem_fun(*this, &AudioTimeAxisView::map_frozen));


	switch (audio_track()->freeze_state()) {
	case AudioTrack::Frozen:
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
AudioTimeAxisView::show_all_xfades ()
{
	if (view) {
		view->show_all_xfades ();
	}
}

void
AudioTimeAxisView::hide_all_xfades ()
{
	if (view) {
		view->hide_all_xfades ();
	}
}

void
AudioTimeAxisView::hide_dependent_views (TimeAxisViewItem& tavi)
{
	AudioRegionView* rv;

	if (view && (rv = dynamic_cast<AudioRegionView*>(&tavi)) != 0) {
		view->hide_xfades_involving (*rv);
	}
}

void
AudioTimeAxisView::reveal_dependent_views (TimeAxisViewItem& tavi)
{
	AudioRegionView* rv;

	if (view && (rv = dynamic_cast<AudioRegionView*>(&tavi)) != 0) {
		view->reveal_xfades_involving (*rv);
	}
}

void
AudioTimeAxisView::route_active_changed ()
{
	RouteUI::route_active_changed ();

	if (is_audio_track()) {
		if (_route.active()) {
			controls_ebox.set_name ("AudioTrackControlsBaseUnselected");
			controls_base_selected_name = "AudioTrackControlsBaseSelected";
			controls_base_unselected_name = "AudioTrackControlsBaseUnselected";
		} else {
			controls_ebox.set_name ("AudioTrackControlsBaseInactiveUnselected");
			controls_base_selected_name = "AudioTrackControlsBaseInactiveSelected";
			controls_base_unselected_name = "AudioTrackControlsBaseInactiveUnselected";
		}
	} else {
		if (_route.active()) {
			controls_ebox.set_name ("BusControlsBaseUnselected");
			controls_base_selected_name = "BusControlsBaseSelected";
			controls_base_unselected_name = "BusControlsBaseUnselected";
		} else {
			controls_ebox.set_name ("BusControlsBaseInactiveUnselected");
			controls_base_selected_name = "BusControlsBaseInactiveSelected";
			controls_base_unselected_name = "BusControlsBaseInactiveUnselected";
		}
	}
}

XMLNode* 
AudioTimeAxisView::get_child_xml_node (const string & childname)
{
	return RouteUI::get_child_xml_node (childname);
}

void
AudioTimeAxisView::color_handler (ColorID id, uint32_t val)
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

bool
AudioTimeAxisView::select_me (GdkEventButton* ev)
{
	editor.get_selection().add (this);
	return false;
}
