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
#include <map>
#include <utility>

#include <sigc++/bind.h>

#include "pbd/error.h"
#include "pbd/stl_delete.h"
#include "pbd/whitespace.h"
#include "pbd/memento_command.h"
#include "pbd/enumwriter.h"
#include "pbd/stateful_diff_command.h"

#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/bindable_button.h>
#include <gtkmm2ext/utils.h>

#include "ardour/amp.h"
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
#include "ardour/debug.h"
#include "ardour/utils.h"
#include "evoral/Parameter.hpp"

#include "ardour_ui.h"
#include "debug.h"
#include "global_signals.h"
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
#include "route_group_menu.h"

#include "ardour/track.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace Editing;
using namespace std;

Glib::RefPtr<Gdk::Pixbuf> RouteTimeAxisView::slider;

void
RouteTimeAxisView::setup_slider_pix ()
{
	if ((slider = ::get_icon ("fader_belt_h")) == 0) {
		throw failed_constructor ();
	}
}

RouteTimeAxisView::RouteTimeAxisView (PublicEditor& ed, Session* sess, boost::shared_ptr<Route> rt, Canvas& canvas)
	: AxisView(sess)
	, RouteUI(rt, sess)
	, TimeAxisView(sess,ed,(TimeAxisView*) 0, canvas)
	, parent_canvas (canvas)
	, button_table (3, 3)
	, route_group_button (_("g"))
	, playlist_button (_("p"))
	, automation_button (_("a"))
	, gm (sess, slider, true, 115)
	, _ignore_track_mode_change (false)
{
	gm.set_controls (_route, _route->shared_peak_meter(), _route->amp());
	gm.get_level_meter().set_no_show_all();
	gm.get_level_meter().setup_meters(50);

	_has_state = true;
	playlist_action_menu = 0;
	automation_action_menu = 0;
	plugins_submenu_item = 0;
	mode_menu = 0;
	_view = 0;

	if (!_route->is_hidden()) {
		_marked_for_display = true;
	}

	mute_changed (0);
        update_solo_display ();

	timestretch_rect = 0;
	no_redraw = false;
	destructive_track_mode_item = 0;
	normal_track_mode_item = 0;
	non_layered_track_mode_item = 0;

	ignore_toggle = false;

	route_group_button.set_name ("TrackGroupButton");
	playlist_button.set_name ("TrackPlaylistButton");
	automation_button.set_name ("TrackAutomationButton");

	route_group_button.unset_flags (Gtk::CAN_FOCUS);
	playlist_button.unset_flags (Gtk::CAN_FOCUS);
	automation_button.unset_flags (Gtk::CAN_FOCUS);

 	route_group_button.signal_button_release_event().connect (sigc::mem_fun(*this, &RouteTimeAxisView::route_group_click), false);
	playlist_button.signal_clicked().connect (sigc::mem_fun(*this, &RouteTimeAxisView::playlist_click));
	automation_button.signal_clicked().connect (sigc::mem_fun(*this, &RouteTimeAxisView::automation_click));

	if (is_track()) {

		/* use icon */

		rec_enable_button->remove ();

		switch (track()->mode()) {
		case ARDOUR::Normal:
		case ARDOUR::NonLayered:
			rec_enable_button->add (*(manage (new Image (::get_icon (X_("record_normal_red"))))));
			break;
		case ARDOUR::Destructive:
			rec_enable_button->add (*(manage (new Image (::get_icon (X_("record_tape_red"))))));
			break;
		}
		rec_enable_button->show_all ();

		controls_table.attach (*rec_enable_button, 5, 6, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);

                if (is_midi_track()) {
                        ARDOUR_UI::instance()->set_tip(*rec_enable_button, _("Record (Right-click for Step Edit)"));
                } else {
                        ARDOUR_UI::instance()->set_tip(*rec_enable_button, _("Record"));
                }

		rec_enable_button->set_sensitive (_session->writable());
	}

	controls_hbox.pack_start(gm.get_level_meter(), false, false);
	_route->meter_change.connect (*this, invalidator (*this), bind (&RouteTimeAxisView::meter_changed, this), gui_context());
	_route->input()->changed.connect (*this, invalidator (*this), ui_bind (&RouteTimeAxisView::io_changed, this, _1, _2), gui_context());
	_route->output()->changed.connect (*this, invalidator (*this), ui_bind (&RouteTimeAxisView::io_changed, this, _1, _2), gui_context());

	controls_table.attach (*mute_button, 6, 7, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);

        if (!_route->is_master()) {
                controls_table.attach (*solo_button, 7, 8, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);
        }

	controls_table.attach (route_group_button, 7, 8, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);
	controls_table.attach (gm.get_gain_slider(), 0, 5, 1, 2, Gtk::SHRINK, Gtk::SHRINK, 0, 0);

	ARDOUR_UI::instance()->set_tip(*solo_button,_("Solo"));
	ARDOUR_UI::instance()->set_tip(*mute_button,_("Mute"));
	ARDOUR_UI::instance()->set_tip(route_group_button, _("Route Group"));
	ARDOUR_UI::instance()->set_tip(playlist_button,_("Playlist"));
	ARDOUR_UI::instance()->set_tip(automation_button, _("Automation"));

	label_view ();

	controls_table.attach (automation_button, 6, 7, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);

	if (is_track() && track()->mode() == ARDOUR::Normal) {
		controls_table.attach (playlist_button, 5, 6, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	}

	_y_position = -1;

	_route->processors_changed.connect (*this, invalidator (*this), ui_bind (&RouteTimeAxisView::processors_changed, this, _1), gui_context());
	_route->PropertyChanged.connect (*this, invalidator (*this), ui_bind (&RouteTimeAxisView::route_property_changed, this, _1), gui_context());

	if (is_track()) {

		track()->TrackModeChanged.connect (*this, invalidator (*this), boost::bind (&RouteTimeAxisView::track_mode_changed, this), gui_context());
		track()->FreezeChange.connect (*this, invalidator (*this), boost::bind (&RouteTimeAxisView::map_frozen, this), gui_context());
		track()->SpeedChanged.connect (*this, invalidator (*this), boost::bind (&RouteTimeAxisView::speed_changed, this), gui_context());

		/* pick up the correct freeze state */
		map_frozen ();

	}

	_editor.ZoomChanged.connect (sigc::mem_fun(*this, &RouteTimeAxisView::reset_samples_per_unit));
	_editor.HorizontalPositionChanged.connect (sigc::mem_fun (*this, &RouteTimeAxisView::horizontal_position_changed));
	ColorsChanged.connect (sigc::mem_fun (*this, &RouteTimeAxisView::color_handler));

	PropertyList* plist = new PropertyList();
	
	plist->add (ARDOUR::Properties::edit, true);
	plist->add (ARDOUR::Properties::mute, true);
	plist->add (ARDOUR::Properties::solo, true);
	
	route_group_menu = new RouteGroupMenu (_session, plist);
	route_group_menu->GroupSelected.connect (sigc::mem_fun (*this, &RouteTimeAxisView::set_route_group_from_menu));

	gm.get_gain_slider().signal_scroll_event().connect(sigc::mem_fun(*this, &RouteTimeAxisView::controls_ebox_scroll), false);
	gm.get_gain_slider().set_name ("TrackGainFader");
}

RouteTimeAxisView::~RouteTimeAxisView ()
{
	CatchDeletion (this);

	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {
		delete *i;
	}

	delete playlist_action_menu;
	playlist_action_menu = 0;

	delete _view;
	_view = 0;

	_automation_tracks.clear ();

	delete route_group_menu;
}

void
RouteTimeAxisView::post_construct ()
{
	/* map current state of the route */

	update_diskstream_display ();

	_subplugin_menu_map.clear ();
	subplugin_menu.items().clear ();
	_route->foreach_processor (sigc::mem_fun (*this, &RouteTimeAxisView::add_processor_to_subplugin_menu));
	_route->foreach_processor (sigc::mem_fun (*this, &RouteTimeAxisView::add_existing_processor_automation_curves));
	reset_processor_automation_curves ();
}

gint
RouteTimeAxisView::route_group_click (GdkEventButton *ev)
{
	if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
		if (_route->route_group()) {
			_route->route_group()->remove (_route);
		}
		return false;
	}

	route_group_menu->build (_route->route_group ());
	route_group_menu->menu()->popup (ev->button, ev->time);

	return false;
}

void
RouteTimeAxisView::set_route_group_from_menu (RouteGroup *eg)
{
	if (eg) {
		eg->add (_route);
	} else {
		if (_route->route_group()) {
			_route->route_group()->remove (_route);
		}
	}
}

void
RouteTimeAxisView::playlist_changed ()
{
	label_view ();
}

void
RouteTimeAxisView::label_view ()
{
	string x = _route->name();

	if (x != name_entry.get_text()) {
		name_entry.set_text (x);
	}

	if (x != name_label.get_text()) {
		name_label.set_text (x);
	}

	ARDOUR_UI::instance()->set_tip (name_entry, x);
}

void
RouteTimeAxisView::route_property_changed (const PropertyChange& what_changed)
{
	if (what_changed.contains (ARDOUR::Properties::name)) {
		label_view ();
	}
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
 	build_playlist_menu ();
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
RouteTimeAxisView::set_state (const XMLNode& node, int version)
{
	TimeAxisView::set_state (node, version);

	XMLNodeList kids = node.children();
	XMLNodeConstIterator iter;
	const XMLProperty* prop;

	if (_view && (prop = node.property ("layer-display"))) {
		set_layer_display (LayerDisplay (string_2_enum (prop->value(), _view->layer_display ())));
	}

	for (iter = kids.begin(); iter != kids.end(); ++iter) {
		if ((*iter)->name() == AutomationTimeAxisView::state_node_name) {
			if ((prop = (*iter)->property ("automation-id")) != 0) {

				Evoral::Parameter param = ARDOUR::EventTypeMap::instance().new_parameter(prop->value());
				bool show = ((prop = (*iter)->property ("shown")) != 0) && string_is_affirmative (prop->value());
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

	/* detach subplugin_menu from automation_action_menu before we delete automation_action_menu,
	   otherwise bad things happen (see comment for similar case in MidiTimeAxisView::build_automation_action_menu)
	*/

	detach_menu (subplugin_menu);

	_main_automation_menu_map.clear ();
	delete automation_action_menu;
	automation_action_menu = new Menu;

	MenuList& items = automation_action_menu->items();

	automation_action_menu->set_name ("ArdourContextMenu");
	
	items.push_back (MenuElem (_("Show All Automation"),
				   sigc::mem_fun(*this, &RouteTimeAxisView::show_all_automation)));
	
	items.push_back (MenuElem (_("Show Existing Automation"),
				   sigc::mem_fun(*this, &RouteTimeAxisView::show_existing_automation)));
	
	items.push_back (MenuElem (_("Hide All Automation"),
				   sigc::mem_fun(*this, &RouteTimeAxisView::hide_all_automation)));

	items.push_back (SeparatorElem ());
	
	/* Attach the plugin submenu. It may have previously been used elsewhere,
	   so it was detached above */

	items.push_back (MenuElem (_("Plugins"), subplugin_menu));
	items.back().set_sensitive (!subplugin_menu.items().empty());
}

void
RouteTimeAxisView::build_display_menu ()
{
	using namespace Menu_Helpers;

	/* prepare it */

	TimeAxisView::build_display_menu ();

	/* now fill it with our stuff */

	MenuList& items = display_menu->items();
	display_menu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Color..."), sigc::mem_fun (*this, &RouteUI::choose_color)));

	if (_size_menu) {
		detach_menu (*_size_menu);
	}
	build_size_menu ();
	items.push_back (MenuElem (_("Height"), *_size_menu));

	items.push_back (SeparatorElem());

	if (!Profile->get_sae()) {
		items.push_back (MenuElem (_("Remote Control ID..."), sigc::mem_fun (*this, &RouteUI::open_remote_control_id_dialog)));
		items.back().set_sensitive (_editor.get_selection().tracks.size() <= 1);
		items.push_back (SeparatorElem());
	}

	// Hook for derived classes to add type specific stuff
	append_extra_display_menu_items ();

	if (is_track()) {

		Menu* layers_menu = manage (new Menu);
		MenuList &layers_items = layers_menu->items();
		layers_menu->set_name("ArdourContextMenu");

		RadioMenuItem::Group layers_group;

		/* Find out how many overlaid/stacked tracks we have in the selection */

		int overlaid = 0;
		int stacked = 0;
		TrackSelection const & s = _editor.get_selection().tracks;
		for (TrackSelection::const_iterator i = s.begin(); i != s.end(); ++i) {
			StreamView* v = (*i)->view ();
			if (!v) {
				continue;
			}

			switch (v->layer_display ()) {
			case Overlaid:
				++overlaid;
				break;
			case Stacked:
				++stacked;
				break;
			}
		}

		/* We're not connecting to signal_toggled() here; in the case where these two items are
		   set to be in the `inconsistent' state, it seems that one or other will end up active
		   as well as inconsistent (presumably due to the RadioMenuItem::Group).  Then when you
		   select the active one, no toggled signal is emitted so nothing happens.
		*/
		
		layers_items.push_back (RadioMenuElem (layers_group, _("Overlaid")));
		RadioMenuItem* i = dynamic_cast<RadioMenuItem*> (&layers_items.back ());
		i->set_active (overlaid != 0 && stacked == 0);
		i->set_inconsistent (overlaid != 0 && stacked != 0);
		i->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::set_layer_display), Overlaid, true));

		layers_items.push_back (
			RadioMenuElem (layers_group, _("Stacked"),
				       sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::set_layer_display), Stacked, true))
			);

		i = dynamic_cast<RadioMenuItem*> (&layers_items.back ());
		i->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::set_layer_display), Stacked, true));
		i->set_active (overlaid == 0 && stacked != 0);
		i->set_inconsistent (overlaid != 0 && stacked != 0);
		
		items.push_back (MenuElem (_("Layers"), *layers_menu));

		if (!Profile->get_sae()) {

			Menu* alignment_menu = manage (new Menu);
			MenuList& alignment_items = alignment_menu->items();
			alignment_menu->set_name ("ArdourContextMenu");
			
			RadioMenuItem::Group align_group;

			/* Same verbose hacks as for the layering options above */

			int existing = 0;
			int capture = 0;
			TrackSelection const & s = _editor.get_selection().tracks;
			for (TrackSelection::const_iterator i = s.begin(); i != s.end(); ++i) {
				RouteTimeAxisView* r = dynamic_cast<RouteTimeAxisView*> (*i);
				if (!r || !r->is_track ()) {
					continue;
				}
				
				switch (r->track()->alignment_style()) {
				case ExistingMaterial:
					++existing;
					break;
				case CaptureTime:
					++capture;
					break;
				}
			}
			
			alignment_items.push_back (RadioMenuElem (align_group, _("Align With Existing Material")));
			RadioMenuItem* i = dynamic_cast<RadioMenuItem*> (&alignment_items.back());
			i->signal_activate().connect (sigc::bind (sigc::mem_fun(*this, &RouteTimeAxisView::set_align_style), ExistingMaterial, true));
			i->set_active (existing != 0 && capture == 0);
			i->set_inconsistent (existing != 0 && capture != 0);
			
			alignment_items.push_back (RadioMenuElem (align_group, _("Align With Capture Time")));
			i = dynamic_cast<RadioMenuItem*> (&alignment_items.back());
			i->signal_activate().connect (sigc::bind (sigc::mem_fun(*this, &RouteTimeAxisView::set_align_style), CaptureTime, true));
			i->set_active (existing == 0 && capture != 0);
			i->set_inconsistent (existing != 0 && capture != 0);
			
			items.push_back (MenuElem (_("Alignment"), *alignment_menu));

			Menu* mode_menu = manage (new Menu);
			MenuList& mode_items = mode_menu->items ();
			mode_menu->set_name ("ArdourContextMenu");

			RadioMenuItem::Group mode_group;

			mode_items.push_back (RadioMenuElem (mode_group, _("Normal Mode"), sigc::bind (
					sigc::mem_fun (*this, &RouteTimeAxisView::set_track_mode),
					ARDOUR::Normal)));
			normal_track_mode_item = dynamic_cast<RadioMenuItem*>(&mode_items.back());

			mode_items.push_back (RadioMenuElem (mode_group, _("Tape Mode"), sigc::bind (
					sigc::mem_fun (*this, &RouteTimeAxisView::set_track_mode),
					ARDOUR::Destructive)));
			destructive_track_mode_item = dynamic_cast<RadioMenuItem*>(&mode_items.back());

 			mode_items.push_back (RadioMenuElem (mode_group, _("Non-Layered Mode"),
 							sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::set_track_mode), ARDOUR::NonLayered)));
 			non_layered_track_mode_item = dynamic_cast<RadioMenuItem*>(&mode_items.back());


			_ignore_track_mode_change = true;
			
			switch (track()->mode()) {
			case ARDOUR::Destructive:
				destructive_track_mode_item->set_active ();
				break;
			case ARDOUR::Normal:
				normal_track_mode_item->set_active ();
				break;
			case ARDOUR::NonLayered:
				non_layered_track_mode_item->set_active ();
				break;
			}
			
			_ignore_track_mode_change = false;

			items.push_back (MenuElem (_("Mode"), *mode_menu));
		}

		color_mode_menu = build_color_mode_menu();
		if (color_mode_menu) {
			items.push_back (MenuElem (_("Color Mode"), *color_mode_menu));
		}

		items.push_back (SeparatorElem());

		build_playlist_menu ();
		items.push_back (MenuElem (_("Playlist"), *playlist_action_menu));

		route_group_menu->detach ();
		route_group_menu->build (_route->route_group ());
		items.push_back (MenuElem (_("Route Group"), *route_group_menu->menu ()));

		build_automation_action_menu ();
		items.push_back (MenuElem (_("Automation"), *automation_action_menu));

		items.push_back (SeparatorElem());
	}

	items.push_back (CheckMenuElem (_("Active"), sigc::mem_fun(*this, &RouteUI::toggle_route_active)));
	route_active_menu_item = dynamic_cast<CheckMenuItem *> (&items.back());
	route_active_menu_item->set_active (_route->active());

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Hide"), sigc::bind (sigc::mem_fun(_editor, &PublicEditor::hide_track_in_display), this, false)));
	if (!Profile->get_sae()) {
		items.push_back (MenuElem (_("Remove"), sigc::mem_fun(*this, &RouteUI::remove_this_route)));
	} else {
		items.push_front (SeparatorElem());
		items.push_front (MenuElem (_("Delete"), sigc::mem_fun(*this, &RouteUI::remove_this_route)));
	}
}

static bool __reset_item (RadioMenuItem* item, RadioMenuItem* item_2)
{
	item->set_active ();
	item_2->set_active ();
	return false;
}

void
RouteTimeAxisView::set_track_mode (TrackMode mode)
{
	if (_ignore_track_mode_change) {
		return;
	}
	
	RadioMenuItem* item;
	RadioMenuItem* other_item;
	RadioMenuItem* other_item_2;

	switch (mode) {
	case ARDOUR::Normal:
		item = normal_track_mode_item;
		other_item = non_layered_track_mode_item;
		other_item_2 = destructive_track_mode_item;
		break;
	case ARDOUR::NonLayered:
		item = non_layered_track_mode_item;
		other_item = normal_track_mode_item;
		other_item_2 = destructive_track_mode_item;
		break;
	case ARDOUR::Destructive:
		item = destructive_track_mode_item;
		other_item = normal_track_mode_item;
		other_item_2 = non_layered_track_mode_item;
		break;
	default:
		fatal << string_compose (_("programming error: %1 %2"), "illegal track mode in RouteTimeAxisView::set_track_mode", mode) << endmsg;
		/*NOTREACHED*/
		return;
	}

	if (item && other_item && other_item_2 && track()->mode() != mode) {
		_set_track_mode (track().get(), mode, other_item, other_item_2);
	}
}

void
RouteTimeAxisView::_set_track_mode (Track* track, TrackMode mode, RadioMenuItem* reset_item, RadioMenuItem* reset_item_2)
{
	bool needs_bounce;

	if (!track->can_use_mode (mode, needs_bounce)) {

		if (!needs_bounce) {
			/* cannot be done */
			Glib::signal_idle().connect (sigc::bind (sigc::ptr_fun (__reset_item), reset_item, reset_item_2));
			return;
		} else {
			cerr << "would bounce this one\n";
			/* XXX: radio menu item becomes inconsistent with track state in this case */
			return;
		}
	}

	track->set_mode (mode);

	rec_enable_button->remove ();

	switch (mode) {
	case ARDOUR::NonLayered:
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
	case ARDOUR::NonLayered:
		item = non_layered_track_mode_item;
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
RouteTimeAxisView::show_timestretch (framepos_t start, framepos_t end)
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


	/* check that the time selection was made in our route, or our route group.
	   remember that route_group() == 0 implies the route is *not* in a edit group.
	*/

	if (!(ts.track == this || (ts.group != 0 && ts.group == _route->route_group()))) {
		/* this doesn't apply to us */
		return;
	}

	/* ignore it if our edit group is not active */

	if ((ts.track != this) && _route->route_group() && !_route->route_group()->is_active()) {
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
	   in some other track or route group (remember that route_group() == 0 means
	   that the track is not in an route group).
	*/

	if (((ts.track != this && !is_child (ts.track)) && _route->route_group() && !_route->route_group()->is_active()) ||
	    (!(ts.track == this || is_child (ts.track) || (ts.group != 0 && ts.group == _route->route_group())))) {
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

	if (height >= preset_height (HeightNormal)) {
		reset_meter();
		show_name_entry ();
		hide_name_label ();

		gm.get_gain_slider().show();
		mute_button->show();
		if (!_route || _route->is_monitor()) {
			solo_button->hide();
		} else {
			solo_button->show();
		}
		if (rec_enable_button)
			rec_enable_button->show();

		route_group_button.show();
		automation_button.show();

		if (is_track() && track()->mode() == ARDOUR::Normal) {
			playlist_button.show();
		}

	} else if (height >= preset_height (HeightSmaller)) {

		reset_meter();
		show_name_entry ();
		hide_name_label ();

		gm.get_gain_slider().hide();
		mute_button->show();
		if (!_route || _route->is_monitor()) {
			solo_button->hide();
		} else {
			solo_button->show();
		}
		if (rec_enable_button)
			rec_enable_button->show();

		route_group_button.hide ();
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

		route_group_button.hide ();
		automation_button.hide ();
		playlist_button.hide ();
		name_label.set_text (_route->name());
	}

	if (height_changed && !no_redraw) {
		/* only emit the signal if the height really changed */
		 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
	}
}

void
RouteTimeAxisView::set_color (Gdk::Color const & c)
{
	RouteUI::set_color (c);
	
	if (_view) {
		_view->apply_color (_color, StreamView::RegionColor);
	}
}

void
RouteTimeAxisView::reset_samples_per_unit ()
{
	set_samples_per_unit (_editor.get_current_zoom());
}

void
RouteTimeAxisView::horizontal_position_changed ()
{
	if (_view) {
		_view->horizontal_position_changed ();
	}
}

void
RouteTimeAxisView::set_samples_per_unit (double spu)
{
	double speed = 1.0;

	if (track()) {
		speed = track()->speed();
	}

	if (_view) {
		_view->set_samples_per_unit (spu * speed);
	}

	TimeAxisView::set_samples_per_unit (spu * speed);
}

void
RouteTimeAxisView::set_align_style (AlignStyle style, bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_route_time_axis (boost::bind (&RouteTimeAxisView::set_align_style, _1, style, false));
	} else {
		if (track ()) {
			track()->set_align_style (style);
		}
	}
}

void
RouteTimeAxisView::rename_current_playlist ()
{
	ArdourPrompter prompter (true);
	string name;

	boost::shared_ptr<Track> tr = track();
	if (!tr || tr->destructive()) {
		return;
	}

	boost::shared_ptr<Playlist> pl = tr->playlist();
	if (!pl) {
		return;
	}

	prompter.set_title (_("Rename Playlist"));
	prompter.set_prompt (_("New name for playlist:"));
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
	std::string ret (basename);

	std::string const group_string = "." + route_group()->name() + ".";

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

	ret = this->name() + "." + route_group()->name () + "." + buf;

	return ret;
}

void
RouteTimeAxisView::use_copy_playlist (bool prompt, vector<boost::shared_ptr<Playlist> > const & playlists_before_op)
{
	string name;

	boost::shared_ptr<Track> tr = track ();
	if (!tr || tr->destructive()) {
		return;
	}

	boost::shared_ptr<const Playlist> pl = tr->playlist();
	if (!pl) {
		return;
	}

	name = pl->name();

	if (route_group() && route_group()->is_active()) {
		name = resolve_new_group_playlist_name(name, playlists_before_op);
	}

	while (_session->playlists->by_name(name)) {
		name = Playlist::bump_name (name, *_session);
	}

	// TODO: The prompter "new" button should be de-activated if the user
	// specifies a playlist name which already exists in the session.

	if (prompt) {

		ArdourPrompter prompter (true);

		prompter.set_title (_("New Copy Playlist"));
		prompter.set_prompt (_("Name for new playlist:"));
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
		tr->use_copy_playlist ();
		tr->playlist()->set_name (name);
	}
}

void
RouteTimeAxisView::use_new_playlist (bool prompt, vector<boost::shared_ptr<Playlist> > const & playlists_before_op)
{
	string name;

	boost::shared_ptr<Track> tr = track ();
	if (!tr || tr->destructive()) {
		return;
	}

	boost::shared_ptr<const Playlist> pl = tr->playlist();
	if (!pl) {
		return;
	}

	name = pl->name();

	if (route_group() && route_group()->is_active()) {
		name = resolve_new_group_playlist_name(name,playlists_before_op);
	}

	while (_session->playlists->by_name(name)) {
		name = Playlist::bump_name (name, *_session);
	}


	if (prompt) {

		ArdourPrompter prompter (true);

		prompter.set_title (_("New Playlist"));
		prompter.set_prompt (_("Name for new playlist:"));
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
		tr->use_new_playlist ();
		tr->playlist()->set_name (name);
	}
}

void
RouteTimeAxisView::clear_playlist ()
{
	boost::shared_ptr<Track> tr = track ();
	if (!tr || tr->destructive()) {
		return;
	}

	boost::shared_ptr<Playlist> pl = tr->playlist();
	if (!pl) {
		return;
	}

	_editor.clear_playlist (pl);
}

void
RouteTimeAxisView::speed_changed ()
{
	Gtkmm2ext::UI::instance()->call_slot (invalidator (*this), boost::bind (&RouteTimeAxisView::reset_samples_per_unit, this));
}

void
RouteTimeAxisView::update_diskstream_display ()
{
	if (!track()) {
		return;
	}

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

	switch (ArdourKeyboard::selection_type (ev->state)) {
	case Selection::Toggle:
		_editor.get_selection().toggle (this);
		break;

	case Selection::Set:
		_editor.get_selection().set (this);
		break;

	case Selection::Extend:
		_editor.extend_selection_to_track (*this);
		break;

	case Selection::Add:
		_editor.get_selection().add (this);
		break;
	}
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
RouteTimeAxisView::get_selectables (framepos_t start, framepos_t end, double top, double bot, list<Selectable*>& results)
{
	double speed = 1.0;

	if (track() != 0) {
		speed = track()->speed();
	}

	framepos_t const start_adjusted = session_frame_to_track_frame(start, speed);
	framepos_t const end_adjusted   = session_frame_to_track_frame(end, speed);

	if ((_view && ((top < 0.0 && bot < 0.0))) || touched (top, bot)) {
		_view->get_selectables (start_adjusted, end_adjusted, top, bot, results);
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

RouteGroup*
RouteTimeAxisView::route_group () const
{
	return _route->route_group();
}

string
RouteTimeAxisView::name() const
{
	return _route->name();
}

boost::shared_ptr<Playlist>
RouteTimeAxisView::playlist () const
{
	boost::shared_ptr<Track> tr;

	if ((tr = track()) != 0) {
		return tr->playlist();
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

	if (!_session->route_name_unique (x)) {
		ARDOUR_UI::instance()->popup_error (_("A track already exists with that name"));
		name_entry.set_text (_route->name());
	} else if (_session->route_name_internal (x)) {
		ARDOUR_UI::instance()->popup_error (string_compose (_("You cannot create a track with that name as it is reserved for %1"),
                                                                    PROGRAM_NAME));
		name_entry.set_text (_route->name());
	} else {
		_route->set_name (x);
	}
}

boost::shared_ptr<Region>
RouteTimeAxisView::find_next_region (framepos_t pos, RegionPoint point, int32_t dir)
{
	boost::shared_ptr<Playlist> pl = playlist ();

	if (pl) {
		return pl->find_next_region (pos, point, dir);
	}

	return boost::shared_ptr<Region> ();
}

framepos_t
RouteTimeAxisView::find_next_region_boundary (framepos_t pos, int32_t dir)
{
	boost::shared_ptr<Playlist> pl = playlist ();

	if (pl) {
		return pl->find_next_region_boundary (pos, dir);
	}

	return -1;
}

void
RouteTimeAxisView::cut_copy_clear (Selection& selection, CutCopyOp op)
{
	boost::shared_ptr<Playlist> what_we_got;
	boost::shared_ptr<Track> tr = track ();
	boost::shared_ptr<Playlist> playlist;

	if (tr == 0) {
		/* route is a bus, not a track */
		return;
	}

	playlist = tr->playlist();

	TimeSelection time (selection.time);
	float const speed = tr->speed();
	if (speed != 1.0f) {
		for (TimeSelection::iterator i = time.begin(); i != time.end(); ++i) {
			(*i).start = session_frame_to_track_frame((*i).start, speed);
			(*i).end   = session_frame_to_track_frame((*i).end,   speed);
		}
	}

        playlist->clear_changes ();
        playlist->clear_owned_changes ();

	switch (op) {
	case Cut:
		if ((what_we_got = playlist->cut (time)) != 0) {
			_editor.get_cut_buffer().add (what_we_got);

                        vector<Command*> cmds;
                        playlist->rdiff (cmds);
                        _session->add_commands (cmds);
			
                        _session->add_command (new StatefulDiffCommand (playlist));
		}
		break;
	case Copy:
		if ((what_we_got = playlist->copy (time)) != 0) {
			_editor.get_cut_buffer().add (what_we_got);
		}
		break;

	case Clear:
		if ((what_we_got = playlist->cut (time)) != 0) {

                        vector<Command*> cmds;
                        playlist->rdiff (cmds);
			_session->add_commands (cmds);
                        _session->add_command (new StatefulDiffCommand (playlist));
			what_we_got->release ();
		}
		break;
	}
}

bool
RouteTimeAxisView::paste (framepos_t pos, float times, Selection& selection, size_t nth)
{
	if (!is_track()) {
		return false;
	}

	boost::shared_ptr<Playlist> pl = playlist ();
	PlaylistSelection::iterator p;

	for (p = selection.playlists.begin(); p != selection.playlists.end() && nth; ++p, --nth) {}

	if (p == selection.playlists.end()) {
		return false;
	}

        DEBUG_TRACE (DEBUG::CutNPaste, string_compose ("paste to %1\n", pos));

	if (track()->speed() != 1.0f) {
		pos = session_frame_to_track_frame (pos, track()->speed());
                DEBUG_TRACE (DEBUG::CutNPaste, string_compose ("modified paste to %1\n", pos));
	}

        pl->clear_changes ();
	pl->paste (*p, pos, times);
	_session->add_command (new StatefulDiffCommand (pl));

	return true;
}


struct PlaylistSorter {
    bool operator() (boost::shared_ptr<Playlist> a, boost::shared_ptr<Playlist> b) const {
            return a->sort_id() < b->sort_id();
    }
};

void
RouteTimeAxisView::build_playlist_menu ()
{
	using namespace Menu_Helpers;

	if (!is_track()) {
		return;
	}

	delete playlist_action_menu;
	playlist_action_menu = new Menu;
	playlist_action_menu->set_name ("ArdourContextMenu");

	MenuList& playlist_items = playlist_action_menu->items();
	playlist_action_menu->set_name ("ArdourContextMenu");
	playlist_items.clear();

        vector<boost::shared_ptr<Playlist> > playlists, playlists_tr;
	boost::shared_ptr<Track> tr = track();
	RadioMenuItem::Group playlist_group;

	_session->playlists->get (playlists);

        /* find the playlists for this diskstream */
        for (vector<boost::shared_ptr<Playlist> >::iterator i = playlists.begin(); i != playlists.end(); ++i) {
                if (((*i)->get_orig_diskstream_id() == tr->diskstream_id()) || (tr->playlist()->id() == (*i)->id())) {
                        playlists_tr.push_back(*i);
                }
        }

        /* sort the playlists */
        PlaylistSorter cmp;
        sort (playlists_tr.begin(), playlists_tr.end(), cmp);
        
        /* add the playlists to the menu */
        for (vector<boost::shared_ptr<Playlist> >::iterator i = playlists_tr.begin(); i != playlists_tr.end(); ++i) {
                playlist_items.push_back (RadioMenuElem (playlist_group, (*i)->name()));
                RadioMenuItem *item = static_cast<RadioMenuItem*>(&playlist_items.back());
                item->signal_toggled().connect(sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::use_playlist), item, boost::weak_ptr<Playlist> (*i)));
                
                if (tr->playlist()->id() == (*i)->id()) {
                        item->set_active();
                        
		}
	}
        
	playlist_items.push_back (SeparatorElem());
	playlist_items.push_back (MenuElem (_("Rename..."), sigc::mem_fun(*this, &RouteTimeAxisView::rename_current_playlist)));
	playlist_items.push_back (SeparatorElem());

	if (!route_group() || !route_group()->is_active()) {
		playlist_items.push_back (MenuElem (_("New..."), sigc::bind(sigc::mem_fun(_editor, &PublicEditor::new_playlists), this)));
		playlist_items.push_back (MenuElem (_("New Copy..."), sigc::bind(sigc::mem_fun(_editor, &PublicEditor::copy_playlists), this)));

	} else {
		// Use a label which tells the user what is happening
		playlist_items.push_back (MenuElem (_("New Take"), sigc::bind(sigc::mem_fun(_editor, &PublicEditor::new_playlists), this)));
		playlist_items.push_back (MenuElem (_("Copy Take"), sigc::bind(sigc::mem_fun(_editor, &PublicEditor::copy_playlists), this)));

	}

	playlist_items.push_back (SeparatorElem());
	playlist_items.push_back (MenuElem (_("Clear Current"), sigc::bind(sigc::mem_fun(_editor, &PublicEditor::clear_playlists), this)));
	playlist_items.push_back (SeparatorElem());

	playlist_items.push_back (MenuElem(_("Select from all..."), sigc::mem_fun(*this, &RouteTimeAxisView::show_playlist_selector)));
}

void
RouteTimeAxisView::use_playlist (RadioMenuItem *item, boost::weak_ptr<Playlist> wpl)
{
	assert (is_track());

        // exit if we were triggered by deactivating the old playlist
        if (!item->get_active()) {
                return;
        }

	boost::shared_ptr<Playlist> pl (wpl.lock());

	if (!pl) {
		return;
	}

	boost::shared_ptr<AudioPlaylist> apl = boost::dynamic_pointer_cast<AudioPlaylist> (pl);

	if (apl) {
		if (track()->playlist() == apl) {
                        // exit when use_playlist is called by the creation of the playlist menu
                        // or the playlist choice is unchanged
			return;
		}
		track()->use_playlist (apl);

		if (route_group() && route_group()->is_active()) {
			std::string group_string = "."+route_group()->name()+".";

			std::string take_name = apl->name();
			std::string::size_type idx = take_name.find(group_string);

			if (idx == std::string::npos)
				return;

			take_name = take_name.substr(idx + group_string.length()); // find the bit containing the take number / name

			boost::shared_ptr<RouteList> rl (route_group()->route_list());

			for (RouteList::const_iterator i = rl->begin(); i != rl->end(); ++i) {
				if ( (*i) == this->route()) {
					continue;
				}

				std::string playlist_name = (*i)->name()+group_string+take_name;

				boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track>(*i);
				if (!track) {
					std::cerr << "route " << (*i)->name() << " is not a Track" << std::endl;
					continue;
				}

				boost::shared_ptr<Playlist> ipl = session()->playlists->by_name(playlist_name);
				if (!ipl) {
					// No playlist for this track for this take yet, make it
					track->use_new_playlist();
					track->playlist()->set_name(playlist_name);
				} else {
					track->use_playlist(ipl);
				}
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

	ENSURE_GUI_THREAD (*this, &RouteTimeAxisView::map_frozen)

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

/** Toggle an automation track for a fully-specified Parameter (type,channel,id)
 *  Will add track if necessary.
 */
void
RouteTimeAxisView::toggle_automation_track (const Evoral::Parameter& param)
{
	boost::shared_ptr<AutomationTimeAxisView> track = automation_child (param);
	Gtk::CheckMenuItem* menu = automation_child_menu_item (param);
	
	if (!track) {
		/* it doesn't exist yet, so we don't care about the button state: just add it */
		create_automation_child (param, true);
	} else {
		assert (menu);
		bool yn = menu->get_active();
		if (track->set_visibility (menu->get_active()) && yn) {
			
			/* we made it visible, now trigger a redisplay. if it was hidden, then automation_track_hidden()
			   will have done that for us.
			*/
			
			if (!no_redraw) {
				_route->gui_changed (X_("track_height"), (void *) 0); /* EMIT_SIGNAL */
			} 
		}
	}
}

void
RouteTimeAxisView::automation_track_hidden (Evoral::Parameter param)
{
	boost::shared_ptr<AutomationTimeAxisView> track = automation_child (param);

	if (!track) {
		return;
	}

	Gtk::CheckMenuItem* menu = automation_child_menu_item (param);

	// if Evoral::Parameter::operator< doesn't obey strict weak ordering, we may crash here....
	track->get_state_node()->add_property (X_("shown"), X_("no"));

	if (menu && !_hidden) {
		ignore_toggle = true;
		menu->set_active (false);
		ignore_toggle = false;
	}

	if (_route && !no_redraw) {
		_route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
	}
}


void
RouteTimeAxisView::show_all_automation ()
{
	no_redraw = true;

	/* Show our automation */

	for (AutomationTracks::iterator i = _automation_tracks.begin(); i != _automation_tracks.end(); ++i) {
		i->second->set_marked_for_display (true);
		i->second->canvas_display()->show();
		i->second->get_state_node()->add_property ("shown", X_("yes"));

		Gtk::CheckMenuItem* menu = automation_child_menu_item (i->first);
		
		if (menu) {
			menu->set_active(true);
		}
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

	 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

void
RouteTimeAxisView::show_existing_automation ()
{
	no_redraw = true;

	/* Show our automation */

	for (AutomationTracks::iterator i = _automation_tracks.begin(); i != _automation_tracks.end(); ++i) {
		if (i->second->has_automation()) {
			i->second->set_marked_for_display (true);
			i->second->canvas_display()->show();
			i->second->get_state_node()->add_property ("shown", X_("yes"));

			Gtk::CheckMenuItem* menu = automation_child_menu_item (i->first);
			if (menu) {
				menu->set_active(true);
			}
		}
	}


	/* Show processor automation */

	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {
		for (vector<ProcessorAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
			if ((*ii)->view != 0 && (*i)->processor->control((*ii)->what)->list()->size() > 0) {
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

	/* Hide our automation */

	for (AutomationTracks::iterator i = _automation_tracks.begin(); i != _automation_tracks.end(); ++i) {
		i->second->set_marked_for_display (false);
		i->second->hide ();
		i->second->get_state_node()->add_property ("shown", X_("no"));

		Gtk::CheckMenuItem* menu = automation_child_menu_item (i->first);
		
		if (menu) {
			menu->set_active (false);
		}
	}

	/* Hide processor automation */

	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {
		for (vector<ProcessorAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
			(*ii)->menu_item->set_active (false);
		}
	}

	no_redraw = false;
	 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}


void
RouteTimeAxisView::region_view_added (RegionView* rv)
{
	/* XXX need to find out if automation children have automationstreamviews. If yes, no ghosts */
	for (Children::iterator i = children.begin(); i != children.end(); ++i) {
		boost::shared_ptr<AutomationTimeAxisView> atv;
		
		if ((atv = boost::dynamic_pointer_cast<AutomationTimeAxisView> (*i)) != 0) {
			atv->add_ghost(rv);
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
                /* session state may never have been saved with new plugin */
                error << _("programming error: ")
		      << string_compose (X_("processor automation curve for %1:%2/%3/%4 not registered with track!"),
                                         processor->name(), what.type(), (int) what.channel(), what.id() )
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
	snprintf (state_name, sizeof (state_name), "%s-%" PRIu32, legalize_for_xml_node (processor->name()).c_str(), what.id());

	boost::shared_ptr<AutomationControl> control
			= boost::dynamic_pointer_cast<AutomationControl>(processor->control(what, true));

	pan->view = boost::shared_ptr<AutomationTimeAxisView>(
		new AutomationTimeAxisView (_session, _route, processor, control, control->parameter (),
					    _editor, *this, false, parent_canvas, name, state_name));

	pan->view->Hiding.connect (sigc::bind (sigc::mem_fun(*this, &RouteTimeAxisView::processor_automation_track_hidden), pan, processor));

	if (!pan->view->marked_for_display()) {
		pan->view->hide ();
	} else {
		pan->menu_item->set_active (true);
	}

	add_child (pan->view);

	if (_view) {
		_view->foreach_regionview (sigc::mem_fun(*pan->view.get(), &TimeAxisView::add_ghost));
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

	if (!no_redraw) {
		_route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
	}
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
RouteTimeAxisView::add_automation_child (Evoral::Parameter param, boost::shared_ptr<AutomationTimeAxisView> track, bool show)
{
	using namespace Menu_Helpers;

	XMLProperty* prop;
	XMLNode* node;

	add_child (track);

	track->Hiding.connect (sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::automation_track_hidden), param));

	bool hideit = (!show);

	if ((node = track->get_state_node()) != 0) {
		if  ((prop = node->property ("shown")) != 0) {
			if (string_is_affirmative (prop->value())) {
				hideit = false;
			}
		}
	}

	_automation_tracks[param] = track;

	track->set_visibility (!hideit);

	if (!no_redraw) {
		_route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
	}

	if (!EventTypeMap::instance().is_midi_parameter(param)) {
		/* MIDI-related parameters are always in the menu, there's no
		   reason to rebuild the menu just because we added a automation
		   lane for one of them. But if we add a non-MIDI automation
		   lane, then we need to invalidate the display menu.
		*/
		delete display_menu;
		display_menu = 0;
	}
}

void
RouteTimeAxisView::add_processor_to_subplugin_menu (boost::weak_ptr<Processor> p)
{
	boost::shared_ptr<Processor> processor (p.lock ());

	if (!processor || !processor->display_to_user ()) {
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

		_subplugin_menu_map[*i] = mitem;

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

		mitem->signal_toggled().connect (sigc::bind (sigc::mem_fun(*this, &RouteTimeAxisView::processor_menu_item_toggled), rai, pan));
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
		 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */

	}
}

void
RouteTimeAxisView::processors_changed (RouteProcessorChange c)
{
	if (c.type == RouteProcessorChange::MeterPointChange) {
		/* nothing to do if only the meter point has changed */
		return;
	}

	using namespace Menu_Helpers;

	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {
		(*i)->valid = false;
	}

	_subplugin_menu_map.clear ();
	subplugin_menu.items().clear ();

	_route->foreach_processor (sigc::mem_fun (*this, &RouteTimeAxisView::add_processor_to_subplugin_menu));
	_route->foreach_processor (sigc::mem_fun (*this, &RouteTimeAxisView::add_existing_processor_automation_curves));

	bool deleted_processor_automation = false;

	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ) {

		list<ProcessorAutomationInfo*>::iterator tmp;

		tmp = i;
		++tmp;

		if (!(*i)->valid) {

			delete *i;
			processor_automation.erase (i);
			deleted_processor_automation = true;

		}

		i = tmp;
	}

	if (deleted_processor_automation && !no_redraw) {
		_route->gui_changed ("track_height", this);
	}
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
RouteTimeAxisView::set_layer_display (LayerDisplay d, bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_route_time_axis (boost::bind (&RouteTimeAxisView::set_layer_display, _1, d, false));
	} else {
		
		if (_view) {
			_view->set_layer_display (d);
		}
		
		ensure_xml_node ();
		xml_node->add_property (N_("layer-display"), enum_2_string (d));
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
	if (i != _automation_tracks.end()) {
		return i->second;
	} else {
		return boost::shared_ptr<AutomationTimeAxisView>();
	}
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
RouteTimeAxisView::meter_changed ()
{
	ENSURE_GUI_THREAD (*this, &RouteTimeAxisView::meter_changed)
	reset_meter();
}

void
RouteTimeAxisView::io_changed (IOChange /*change*/, void */*src*/)
{
	reset_meter ();
}

void
RouteTimeAxisView::build_underlay_menu(Gtk::Menu* parent_menu)
{
	using namespace Menu_Helpers;

	if (!_underlay_streams.empty()) {
		MenuList& parent_items = parent_menu->items();
		Menu* gs_menu = manage (new Menu);
		gs_menu->set_name ("ArdourContextMenu");
		MenuList& gs_items = gs_menu->items();

		parent_items.push_back (MenuElem (_("Underlays"), *gs_menu));

		for(UnderlayList::iterator it = _underlay_streams.begin(); it != _underlay_streams.end(); ++it) {
			gs_items.push_back(MenuElem(string_compose(_("Remove \"%1\""), (*it)->trackview().name()),
						    sigc::bind(sigc::mem_fun(*this, &RouteTimeAxisView::remove_underlay), *it)));
		}
	}
}

bool
RouteTimeAxisView::set_underlay_state()
{
	if (!underlay_xml_node) {
		return false;
	}

	XMLNodeList nlist = underlay_xml_node->children();
	XMLNodeConstIterator niter;
	XMLNode *child_node;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		child_node = *niter;

		if (child_node->name() != "Underlay") {
			continue;
		}

		XMLProperty* prop = child_node->property ("id");
		if (prop) {
			PBD::ID id (prop->value());

			RouteTimeAxisView* v = _editor.get_route_view_by_route_id (id);

			if (v) {
				add_underlay(v->view(), false);
			}
		}
	}

	return false;
}

void
RouteTimeAxisView::add_underlay (StreamView* v, bool update_xml)
{
	if (!v) {
		return;
	}

	RouteTimeAxisView& other = v->trackview();

	if (find(_underlay_streams.begin(), _underlay_streams.end(), v) == _underlay_streams.end()) {
		if (find(other._underlay_mirrors.begin(), other._underlay_mirrors.end(), this) != other._underlay_mirrors.end()) {
			fatal << _("programming error: underlay reference pointer pairs are inconsistent!") << endmsg;
			/*NOTREACHED*/
		}

		_underlay_streams.push_back(v);
		other._underlay_mirrors.push_back(this);

		v->foreach_regionview(sigc::mem_fun(*this, &RouteTimeAxisView::add_ghost));

		if (update_xml) {
			if (!underlay_xml_node) {
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
RouteTimeAxisView::remove_underlay (StreamView* v)
{
	if (!v) {
		return;
	}

	UnderlayList::iterator it = find(_underlay_streams.begin(), _underlay_streams.end(), v);
	RouteTimeAxisView& other = v->trackview();

	if (it != _underlay_streams.end()) {
		UnderlayMirrorList::iterator gm = find(other._underlay_mirrors.begin(), other._underlay_mirrors.end(), this);

		if (gm == other._underlay_mirrors.end()) {
			fatal << _("programming error: underlay reference pointer pairs are inconsistent!") << endmsg;
			/*NOTREACHED*/
		}

		v->foreach_regionview(sigc::mem_fun(*this, &RouteTimeAxisView::remove_ghost));

		_underlay_streams.erase(it);
		other._underlay_mirrors.erase(gm);

		if (underlay_xml_node) {
			underlay_xml_node->remove_nodes_and_delete("id", v->trackview().route()->id().to_s());
		}
	}
}

void
RouteTimeAxisView::set_button_names ()
{
	rec_enable_button_label.set_text (_("r"));

        if (_route && _route->solo_safe()) {
                solo_button_label.set_text (X_("!"));
        } else {
                if (Config->get_solo_control_is_listen_control()) {
                        switch (Config->get_listen_position()) {
                        case AfterFaderListen:
                                solo_button_label.set_text (_("A"));
                                break;
                        case PreFaderListen:
                                solo_button_label.set_text (_("P"));
                                break;
                        }
                } else {
                        solo_button_label.set_text (_("s"));
                }
        }
	mute_button_label.set_text (_("m"));
}

Gtk::CheckMenuItem*
RouteTimeAxisView::automation_child_menu_item (Evoral::Parameter param)
{
	ParameterMenuMap::iterator i = _main_automation_menu_map.find (param);
	if (i != _main_automation_menu_map.end()) {
		return i->second;
	}
	
	i = _subplugin_menu_map.find (param);
	if (i != _subplugin_menu_map.end()) {
		return i->second;
	}

	return 0;
}

void
RouteTimeAxisView::create_gain_automation_child (const Evoral::Parameter& param, bool show)
{
	boost::shared_ptr<AutomationControl> c = _route->gain_control();
	if (!c) {
		error << "Route has no gain automation, unable to add automation track view." << endmsg;
		return;
	}

	gain_track.reset (new AutomationTimeAxisView (_session,
						      _route, _route->amp(), c, param,
						      _editor,
						      *this,
						      false,
						      parent_canvas,
						      _route->amp()->describe_parameter(param)));
	
	add_automation_child (Evoral::Parameter(GainAutomation), gain_track, show);
}
