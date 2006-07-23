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
#include <cassert>

#include <algorithm>
#include <string>
#include <vector>

#include <sigc++/bind.h>

#include <pbd/error.h>
#include <pbd/stl_delete.h>
#include <pbd/whitespace.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/bindable_button.h>
#include <gtkmm2ext/utils.h>

#include <ardour/playlist.h>
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
#include "canvas_impl.h"
#include "crossfade_view.h"
#include "enums.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "playlist_selector.h"
#include "plugin_selector.h"
#include "plugin_ui.h"
#include "point_selection.h"
#include "prompter.h"
#include "public_editor.h"
#include "regionview.h"
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


RouteTimeAxisView::RouteTimeAxisView (PublicEditor& ed, Session& sess, Route& rt, Canvas& canvas)
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

	//view = new AudioStreamView (*this);

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

	hide_button.add (*(manage (new Image (get_xpm("small_x.xpm")))));

	solo_button->signal_button_press_event().connect (mem_fun (*this, &RouteTimeAxisView::select_me), false);
	mute_button->signal_button_press_event().connect (mem_fun (*this, &RouteTimeAxisView::select_me), false);
	playlist_button.signal_button_press_event().connect (mem_fun (*this, &RouteTimeAxisView::select_me), false);
	automation_button.signal_button_press_event().connect (mem_fun (*this, &RouteTimeAxisView::select_me), false);
	size_button.signal_button_press_event().connect (mem_fun (*this, &RouteTimeAxisView::select_me), false);
	visual_button.signal_button_press_event().connect (mem_fun (*this, &RouteTimeAxisView::select_me), false);
	hide_button.signal_button_press_event().connect (mem_fun (*this, &RouteTimeAxisView::select_me), false);

	solo_button->signal_button_press_event().connect (mem_fun(*this, &RouteUI::solo_press), false);
	solo_button->signal_button_release_event().connect (mem_fun(*this, &RouteUI::solo_release), false);
	mute_button->signal_button_press_event().connect (mem_fun(*this, &RouteUI::mute_press), false);
	mute_button->signal_button_release_event().connect (mem_fun(*this, &RouteUI::mute_release), false);
 	edit_group_button.signal_button_release_event().connect (mem_fun(*this, &RouteTimeAxisView::edit_click), false);
	playlist_button.signal_clicked().connect (mem_fun(*this, &RouteTimeAxisView::playlist_click));
	automation_button.signal_clicked().connect (mem_fun(*this, &RouteTimeAxisView::automation_click));
	size_button.signal_button_release_event().connect (mem_fun(*this, &RouteTimeAxisView::size_click), false);
	visual_button.signal_clicked().connect (mem_fun(*this, &RouteTimeAxisView::visual_click));
	hide_button.signal_clicked().connect (mem_fun(*this, &RouteTimeAxisView::hide_click));

	if (is_track()) {
		rec_enable_button->set_active (false);
		rec_enable_button->set_name ("TrackRecordEnableButton");
		rec_enable_button->signal_button_press_event().connect (mem_fun (*this, &RouteTimeAxisView::select_me), false);
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

	/* map current state of the route */

	update_diskstream_display ();
	solo_changed(0);
	mute_changed(0);
	//redirects_changed (0);
	//reset_redirect_automation_curves ();
	y_position = -1;

	_route.mute_changed.connect (mem_fun(*this, &RouteUI::mute_changed));
	_route.solo_changed.connect (mem_fun(*this, &RouteUI::solo_changed));
	//_route.redirects_changed.connect (mem_fun(*this, &RouteTimeAxisView::redirects_changed));
	_route.name_changed.connect (mem_fun(*this, &RouteTimeAxisView::route_name_changed));
	_route.solo_safe_changed.connect (mem_fun(*this, &RouteUI::solo_changed));

	if (is_track()) {

		track()->FreezeChange.connect (mem_fun(*this, &RouteTimeAxisView::map_frozen));
		track()->DiskstreamChanged.connect (mem_fun(*this, &RouteTimeAxisView::diskstream_changed));
		get_diskstream()->SpeedChanged.connect (mem_fun(*this, &RouteTimeAxisView::speed_changed));

		/* ask for notifications of any new RegionViews */
		// FIXME: _view is NULL, but it would be nice to attach this here :/
		//_view->RegionViewAdded.connect (mem_fun(*this, &RouteTimeAxisView::region_view_added));
		//_view->attach ();

		/* pick up the correct freeze state */
		map_frozen ();

	}

	editor.ZoomChanged.connect (mem_fun(*this, &RouteTimeAxisView::reset_samples_per_unit));
	ColorChanged.connect (mem_fun (*this, &RouteTimeAxisView::color_handler));
}

RouteTimeAxisView::~RouteTimeAxisView ()
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

	if (_view) {
		delete _view;
		_view = 0;
	}
}

void
RouteTimeAxisView::set_playlist (Playlist *newplaylist)
{
	Playlist *pl = playlist();
	assert(pl);

	modified_connection.disconnect ();
	state_changed_connection.disconnect ();
	
	state_changed_connection = pl->StateChanged.connect (mem_fun(*this, &RouteTimeAxisView::playlist_state_changed));
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
	        _route.set_edit_group (0, this);
		return FALSE;
	} 

	using namespace Menu_Helpers;

	MenuList& items = edit_group_menu.items ();
	RadioMenuItem::Group group;

	items.clear ();
	items.push_back (RadioMenuElem (group, _("No group"), 
					bind (mem_fun(*this, &RouteTimeAxisView::set_edit_group_from_menu), (RouteGroup *) 0)));
	
	if (_route.edit_group() == 0) {
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

	cerr << "adding edit group " << eg->name() << endl;

	items.push_back (RadioMenuElem (*group, eg->name(), bind (mem_fun(*this, &RouteTimeAxisView::set_edit_group_from_menu), eg)));
	if (_route.edit_group() == eg) {
		static_cast<RadioMenuItem*>(&items.back())->set_active ();
	}
}

void
RouteTimeAxisView::set_edit_group_from_menu (RouteGroup *eg)

{
	_route.set_edit_group (eg, this);
}

void
RouteTimeAxisView::playlist_state_changed (Change ignored)
{
	// ENSURE_GUI_THREAD (bind (mem_fun(*this, &RouteTimeAxisView::playlist_state_changed), ignored));
	// why are we here ?
}

void
RouteTimeAxisView::playlist_changed ()

{
	label_view ();

	if (is_track()) {
		set_playlist (dynamic_cast<Playlist*>(get_diskstream()->playlist()));
	}
}

void
RouteTimeAxisView::label_view ()
{
	string x = _route.name();

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
	
	if (playlist_action_menu == 0) {
		playlist_action_menu = new Menu;
		playlist_action_menu->set_name ("ArdourContextMenu");
	}
	
 	build_playlist_menu(playlist_action_menu);

	playlist_action_menu->popup (1, 0);
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
	automation_action_menu->popup (1, 0);
}

void
RouteTimeAxisView::show_timestretch (jack_nframes_t start, jack_nframes_t end)
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

	if (((ts.track != this && !is_child (ts.track)) && _route.edit_group() && !_route.edit_group()->is_active()) ||
	    (!(ts.track == this || is_child (ts.track) || (ts.group != 0 && ts.group == _route.edit_group())))) {
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

	_view->set_height ((double) height);

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
	get_diskstream()->set_align_style (style);
}

void
RouteTimeAxisView::rename_current_playlist ()
{
	ArdourPrompter prompter (true);
	string name;

	Diskstream *const ds = get_diskstream();
	if (!ds || ds->destructive())
		return;

	Playlist *const pl = ds->playlist();
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
	
	Diskstream *const ds = get_diskstream();
	if (!ds || ds->destructive())
		return;

	Playlist *const pl = ds->playlist();
	if (!pl)
		return;

	name = Playlist::bump_name (pl->name(), _session);

	if (prompt) {

		ArdourPrompter prompter (true);
		
		prompter.set_prompt (_("Name for Playlist"));
		prompter.set_initial_text (name);
		prompter.add_button (Gtk::Stock::NEW, Gtk::RESPONSE_ACCEPT);
		prompter.set_response_sensitive (Gtk::RESPONSE_ACCEPT, false);
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
		pl->set_name (name);
	}
}

void
RouteTimeAxisView::use_new_playlist (bool prompt)
{
	string name;
	
	Diskstream *const ds = get_diskstream();
	if (!ds || ds->destructive())
		return;

	Playlist *const pl = ds->playlist();
	if (!pl)
		return;

	name = Playlist::bump_name (pl->name(), _session);

	if (prompt) {
		
		ArdourPrompter prompter (true);
		
		prompter.set_prompt (_("Name for Playlist"));
		prompter.set_initial_text (name);
		prompter.add_button (Gtk::Stock::NEW, Gtk::RESPONSE_ACCEPT);
		prompter.set_response_sensitive (Gtk::RESPONSE_ACCEPT, false);
		
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
		pl->set_name (name);
	}
}

void
RouteTimeAxisView::clear_playlist ()
{
	Diskstream *const ds = get_diskstream();
	if (!ds || ds->destructive())
		return;

	Playlist *const pl = ds->playlist();
	if (!pl)
		return;

	editor.clear_playlist (*pl);
}

void
RouteTimeAxisView::speed_changed ()
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun(*this, &RouteTimeAxisView::reset_samples_per_unit));
}

void
RouteTimeAxisView::diskstream_changed (void *src)
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
RouteTimeAxisView::set_selected_points (PointSelection& points)
{
	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->set_selected_points (points);
	}
}

void
RouteTimeAxisView::get_selectables (jack_nframes_t start, jack_nframes_t end, double top, double bot, list<Selectable*>& results)
{
	double speed = 1.0;
	
	if (get_diskstream() != 0) {
		speed = get_diskstream()->speed();
	}
	
	jack_nframes_t start_adjusted = session_frame_to_track_frame(start, speed);
	jack_nframes_t end_adjusted   = session_frame_to_track_frame(end, speed);

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
	return _route.edit_group();
}

string
RouteTimeAxisView::name() const
{
	return _route.name();
}

Playlist *
RouteTimeAxisView::playlist () const 
{
	Diskstream *ds;

	if ((ds = get_diskstream()) != 0) {
		return ds->playlist(); 
	} else {
		return 0; 
	}
}

void
RouteTimeAxisView::name_entry_changed ()
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
RouteTimeAxisView::visual_click ()
{
	popup_display_menu (0);
}

void
RouteTimeAxisView::hide_click ()
{
	editor.hide_track_in_display (*this);
}

Region*
RouteTimeAxisView::find_next_region (jack_nframes_t pos, RegionPoint point, int32_t dir)
{
	Diskstream *stream;
	Playlist *playlist;

	if ((stream = get_diskstream()) != 0 && (playlist = stream->playlist()) != 0) {
		return playlist->find_next_region (pos, point, dir);
	}

	return 0;
}

bool
RouteTimeAxisView::cut_copy_clear (Selection& selection, CutCopyOp op)
{
	Playlist* what_we_got;
	Diskstream* ds = get_diskstream();
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
RouteTimeAxisView::paste (jack_nframes_t pos, float times, Selection& selection, size_t nth)
{
	if (!is_track()) {
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

	playlist_items.push_back (MenuElem (string_compose (_("Current: %1"), get_diskstream()->playlist()->name())));
	playlist_items.push_back (SeparatorElem());
	
	playlist_items.push_back (MenuElem (_("Rename"), mem_fun(*this, &RouteTimeAxisView::rename_current_playlist)));
	playlist_items.push_back (SeparatorElem());

	playlist_items.push_back (MenuElem (_("New"), mem_fun(editor, &PublicEditor::new_playlists)));
	playlist_items.push_back (MenuElem (_("New Copy"), mem_fun(editor, &PublicEditor::copy_playlists)));
	playlist_items.push_back (SeparatorElem());
	playlist_items.push_back (MenuElem (_("Clear Current"), mem_fun(editor, &PublicEditor::clear_playlists)));
	playlist_items.push_back (SeparatorElem());
	playlist_items.push_back (MenuElem(_("Select"), mem_fun(*this, &RouteTimeAxisView::show_playlist_selector)));

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

bool
RouteTimeAxisView::select_me (GdkEventButton* ev)
{
	editor.get_selection().add (this);
	return false;
}

