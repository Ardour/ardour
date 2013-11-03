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
#include "ardour/meter.h"
#include "ardour/event_type_map.h"
#include "ardour/processor.h"
#include "ardour/profile.h"
#include "ardour/route_group.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"

#include "evoral/Parameter.hpp"

#include "canvas/debug.h"

#include "ardour_ui.h"
#include "ardour_button.h"
#include "debug.h"
#include "global_signals.h"
#include "route_time_axis.h"
#include "automation_time_axis.h"
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
using std::list;

RouteTimeAxisView::RouteTimeAxisView (PublicEditor& ed, Session* sess, ArdourCanvas::Canvas& canvas)
	: AxisView(sess)
	, RouteUI(sess)
	, TimeAxisView(sess,ed,(TimeAxisView*) 0, canvas)
	, _view (0)
	, parent_canvas (canvas)
	, no_redraw (false)
	, button_table (3, 3)
	, route_group_button (_("g"))
	, playlist_button (_("p"))
	, automation_button (_("a"))
	, automation_action_menu (0)
	, plugins_submenu_item (0)
	, route_group_menu (0)
	, playlist_action_menu (0)
	, mode_menu (0)
	, color_mode_menu (0)
	, gm (sess, true, 125, 18)
	, _ignore_set_layer_display (false)
{
}

void
RouteTimeAxisView::set_route (boost::shared_ptr<Route> rt)
{
	RouteUI::set_route (rt);

	CANVAS_DEBUG_NAME (_canvas_display, string_compose ("main for %1", rt->name()));
	CANVAS_DEBUG_NAME (selection_group, string_compose ("selections for %1", rt->name()));
	CANVAS_DEBUG_NAME (_ghost_group, string_compose ("ghosts for %1", rt->name()));

	int meter_width = 3;
	if (_route && _route->shared_peak_meter()->input_streams().n_total() == 1) {
		meter_width = 6;
	}
	gm.set_controls (_route, _route->shared_peak_meter(), _route->amp());
	gm.get_level_meter().set_no_show_all();
	gm.get_level_meter().setup_meters(50, meter_width);
	gm.update_gain_sensitive ();

	string str = gui_property ("height");
	if (!str.empty()) {
		set_height (atoi (str));
	} else {
		set_height (preset_height (HeightNormal));
	}

	if (!_route->is_auditioner()) {
		if (gui_property ("visible").empty()) {
			set_gui_property ("visible", true);
		}
	} else {
		set_gui_property ("visible", false);
	}

	mute_changed (0);
        update_solo_display ();

	timestretch_rect = 0;
	no_redraw = false;

	ignore_toggle = false;

	route_group_button.set_name ("route button");
	playlist_button.set_name ("route button");
	automation_button.set_name ("route button");

 	route_group_button.signal_button_release_event().connect (sigc::mem_fun(*this, &RouteTimeAxisView::route_group_click), false);
	playlist_button.signal_clicked.connect (sigc::mem_fun(*this, &RouteTimeAxisView::playlist_click));
	automation_button.signal_clicked.connect (sigc::mem_fun(*this, &RouteTimeAxisView::automation_click));

	if (is_track()) {

		/* use icon */
		
		switch (track()->mode()) {
		case ARDOUR::Normal:
		case ARDOUR::NonLayered:
			rec_enable_button->set_image (::get_icon (X_("record_normal_red")));
			break;
		case ARDOUR::Destructive:
			rec_enable_button->set_image (::get_icon (X_("record_tape_red")));
			break;
		}

		controls_table.attach (*rec_enable_button, 5, 6, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);

                if (is_midi_track()) {
                        ARDOUR_UI::instance()->set_tip(*rec_enable_button, _("Record (Right-click for Step Edit)"));
			gm.set_fader_name ("MidiTrackFader");
                } else {
                        ARDOUR_UI::instance()->set_tip(*rec_enable_button, _("Record"));
			gm.set_fader_name ("AudioTrackFader");
                }

		rec_enable_button->set_sensitive (_session->writable());
		
		/* set playlist button tip to the current playlist, and make it update when it changes */
		update_playlist_tip ();
		track()->PlaylistChanged.connect (*this, invalidator (*this), ui_bind(&RouteTimeAxisView::update_playlist_tip, this), gui_context());

	} else {
		gm.set_fader_name ("AudioBusFader");
	}

	Gtk::VBox *mtrbox = manage(new Gtk::VBox());
	mtrbox->pack_start(gm.get_level_meter(), false, false, 2);
	controls_hbox.pack_start(*mtrbox, false, false, 4);
	mtrbox->show();

	_route->meter_change.connect (*this, invalidator (*this), bind (&RouteTimeAxisView::meter_changed, this), gui_context());
	_route->input()->changed.connect (*this, invalidator (*this), boost::bind (&RouteTimeAxisView::io_changed, this, _1, _2), gui_context());
	_route->output()->changed.connect (*this, invalidator (*this), boost::bind (&RouteTimeAxisView::io_changed, this, _1, _2), gui_context());

	controls_table.attach (*mute_button, 6, 7, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);

        if (!_route->is_master()) {
                controls_table.attach (*solo_button, 7, 8, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);
        }

	controls_table.attach (route_group_button, 7, 8, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);
	controls_table.attach (gm.get_gain_slider(), 0, 5, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::AttachOptions (0), 3, 0);

	ARDOUR_UI::instance()->set_tip(*solo_button,_("Solo"));
	ARDOUR_UI::instance()->set_tip(*mute_button,_("Mute"));
	ARDOUR_UI::instance()->set_tip(route_group_button, _("Route Group"));

	if (is_midi_track()) {
		ARDOUR_UI::instance()->set_tip(automation_button, _("MIDI Controllers and Automation"));
	} else {
		ARDOUR_UI::instance()->set_tip(automation_button, _("Automation"));
	}

	label_view ();

	controls_table.attach (automation_button, 6, 7, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);

	if (is_track() && track()->mode() == ARDOUR::Normal) {
		controls_table.attach (playlist_button, 5, 6, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	}

	_y_position = -1;

	_route->processors_changed.connect (*this, invalidator (*this), boost::bind (&RouteTimeAxisView::processors_changed, this, _1), gui_context());
	_route->PropertyChanged.connect (*this, invalidator (*this), boost::bind (&RouteTimeAxisView::route_property_changed, this, _1), gui_context());

	if (is_track()) {

		str = gui_property ("layer-display");
		if (!str.empty()) {
			set_layer_display (LayerDisplay (string_2_enum (str, _view->layer_display ())));
		}

		track()->FreezeChange.connect (*this, invalidator (*this), boost::bind (&RouteTimeAxisView::map_frozen, this), gui_context());
		track()->SpeedChanged.connect (*this, invalidator (*this), boost::bind (&RouteTimeAxisView::speed_changed, this), gui_context());

		/* pick up the correct freeze state */
		map_frozen ();

	} 

	_editor.ZoomChanged.connect (sigc::mem_fun(*this, &RouteTimeAxisView::reset_samples_per_pixel));
	ColorsChanged.connect (sigc::mem_fun (*this, &RouteTimeAxisView::color_handler));

	PropertyList* plist = new PropertyList();

	plist->add (ARDOUR::Properties::mute, true);
	plist->add (ARDOUR::Properties::solo, true);

	route_group_menu = new RouteGroupMenu (_session, plist);

	gm.get_gain_slider().signal_scroll_event().connect(sigc::mem_fun(*this, &RouteTimeAxisView::controls_ebox_scroll), false);

	gm.get_level_meter().signal_scroll_event().connect (sigc::mem_fun (*this, &RouteTimeAxisView::controls_ebox_scroll), false);
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
	setup_processor_menu_and_curves ();
	reset_processor_automation_curves ();
}

/** Set up the processor menu for the current set of processors, and
 *  display automation curves for any parameters which have data.
 */
void
RouteTimeAxisView::setup_processor_menu_and_curves ()
{
	_subplugin_menu_map.clear ();
	subplugin_menu.items().clear ();
	_route->foreach_processor (sigc::mem_fun (*this, &RouteTimeAxisView::add_processor_to_subplugin_menu));
	_route->foreach_processor (sigc::mem_fun (*this, &RouteTimeAxisView::add_existing_processor_automation_curves));
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

	WeakRouteList r;
	r.push_back (route ());

	route_group_menu->build (r);
	route_group_menu->menu()->popup (ev->button, ev->time);

	return false;
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

	if (x != name_label.get_text()) {
		name_label.set_text (x);
	}

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
	build_automation_action_menu (false);
	automation_action_menu->popup (1, gtk_get_current_event_time());
}

void
RouteTimeAxisView::build_automation_action_menu (bool for_selection)
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
				   sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::show_all_automation), for_selection)));

	items.push_back (MenuElem (_("Show Existing Automation"),
				   sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::show_existing_automation), for_selection)));

	items.push_back (MenuElem (_("Hide All Automation"),
				   sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::hide_all_automation), for_selection)));

	/* Attach the plugin submenu. It may have previously been used elsewhere,
	   so it was detached above 
	*/
	
	if (!subplugin_menu.items().empty()) {
		items.push_back (SeparatorElem ());
		items.push_back (MenuElem (_("Processor automation"), subplugin_menu));
		items.back().set_sensitive (!for_selection || _editor.get_selection().tracks.size() == 1);;
	}
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
			case Expanded:
				++stacked;
				break;
			}
		}

		/* We're not connecting to signal_toggled() here; in the case where these two items are
		   set to be in the `inconsistent' state, it seems that one or other will end up active
		   as well as inconsistent (presumably due to the RadioMenuItem::Group).  Then when you
		   select the active one, no toggled signal is emitted so nothing happens.
		*/

		_ignore_set_layer_display = true;
		
		layers_items.push_back (RadioMenuElem (layers_group, _("Overlaid")));
		RadioMenuItem* i = dynamic_cast<RadioMenuItem*> (&layers_items.back ());
		i->set_active (overlaid != 0 && stacked == 0);
		i->set_inconsistent (overlaid != 0 && stacked != 0);
		i->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::set_layer_display), Overlaid, true));

		layers_items.push_back (RadioMenuElem (layers_group, _("Stacked")));
		i = dynamic_cast<RadioMenuItem*> (&layers_items.back ());
		i->set_active (overlaid == 0 && stacked != 0);
		i->set_inconsistent (overlaid != 0 && stacked != 0);
		i->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::set_layer_display), Stacked, true));
		
		_ignore_set_layer_display = false;

		items.push_back (MenuElem (_("Layers"), *layers_menu));

		if (!Profile->get_sae()) {

			Menu* alignment_menu = manage (new Menu);
			MenuList& alignment_items = alignment_menu->items();
			alignment_menu->set_name ("ArdourContextMenu");

			RadioMenuItem::Group align_group;

			/* Same verbose hacks as for the layering options above */

			int existing = 0;
			int capture = 0;
                        int automatic = 0;
                        int styles = 0;
                        boost::shared_ptr<Track> first_track;

			TrackSelection const & s = _editor.get_selection().tracks;
			for (TrackSelection::const_iterator i = s.begin(); i != s.end(); ++i) {
				RouteTimeAxisView* r = dynamic_cast<RouteTimeAxisView*> (*i);
				if (!r || !r->is_track ()) {
					continue;
				}

                                if (!first_track) {
                                        first_track = r->track();
                                }

                                switch (r->track()->alignment_choice()) {
                                case Automatic:
                                        ++automatic;
                                        styles |= 0x1;
                                        switch (r->track()->alignment_style()) {
                                        case ExistingMaterial:
                                                ++existing;
                                                break;
                                        case CaptureTime:
                                                ++capture;
                                                break;
                                        }
                                        break;
                                case UseExistingMaterial:
                                        ++existing;
                                        styles |= 0x2;
                                        break;
                                case UseCaptureTime:
                                        ++capture;
                                        styles |= 0x4;
                                        break;
                                }
			}

                        bool inconsistent;
                        switch (styles) {
                        case 1:
                        case 2:
                        case 4:
                                inconsistent = false;
                                break;
                        default:
                                inconsistent = true;
                                break;
                        }

                        RadioMenuItem* i;

                        if (!inconsistent && first_track) {

                                alignment_items.push_back (RadioMenuElem (align_group, _("Automatic (based on I/O connections)")));
                                i = dynamic_cast<RadioMenuItem*> (&alignment_items.back());
                                i->set_active (automatic != 0 && existing == 0 && capture == 0);
                                i->signal_activate().connect (sigc::bind (sigc::mem_fun(*this, &RouteTimeAxisView::set_align_choice), i, Automatic, true));

                                switch (first_track->alignment_choice()) {
                                case Automatic:
                                        switch (first_track->alignment_style()) {
                                        case ExistingMaterial:
                                                alignment_items.push_back (MenuElem (_("(Currently: Existing Material)")));
                                                break;
                                        case CaptureTime:
                                                alignment_items.push_back (MenuElem (_("(Currently: Capture Time)")));
                                                break;
                                        }
                                        break;
                                default:
                                        break;
                                }

                                alignment_items.push_back (RadioMenuElem (align_group, _("Align With Existing Material")));
                                i = dynamic_cast<RadioMenuItem*> (&alignment_items.back());
                                i->set_active (existing != 0 && capture == 0 && automatic == 0);
                                i->signal_activate().connect (sigc::bind (sigc::mem_fun(*this, &RouteTimeAxisView::set_align_choice), i, UseExistingMaterial, true));

                                alignment_items.push_back (RadioMenuElem (align_group, _("Align With Capture Time")));
                                i = dynamic_cast<RadioMenuItem*> (&alignment_items.back());
                                i->set_active (existing == 0 && capture != 0 && automatic == 0);
                                i->signal_activate().connect (sigc::bind (sigc::mem_fun(*this, &RouteTimeAxisView::set_align_choice), i, UseCaptureTime, true));

                                items.push_back (MenuElem (_("Alignment"), *alignment_menu));

                        } else {
                                /* show nothing */
                        }

			Menu* mode_menu = manage (new Menu);
			MenuList& mode_items = mode_menu->items ();
			mode_menu->set_name ("ArdourContextMenu");

			RadioMenuItem::Group mode_group;

			int normal = 0;
			int tape = 0;
			int non_layered = 0;

			for (TrackSelection::const_iterator i = s.begin(); i != s.end(); ++i) {
				RouteTimeAxisView* r = dynamic_cast<RouteTimeAxisView*> (*i);
				if (!r || !r->is_track ()) {
					continue;
				}

				switch (r->track()->mode()) {
				case Normal:
					++normal;
					break;
				case Destructive:
					++tape;
					break;
				case NonLayered:
					++non_layered;
					break;
				}
			}

			mode_items.push_back (RadioMenuElem (mode_group, _("Normal Mode")));
			i = dynamic_cast<RadioMenuItem*> (&mode_items.back ());
			i->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::set_track_mode), ARDOUR::Normal, true));
			i->set_active (normal != 0 && tape == 0 && non_layered == 0);
			i->set_inconsistent (normal != 0 && (tape != 0 || non_layered != 0));

			mode_items.push_back (RadioMenuElem (mode_group, _("Tape Mode")));
			i = dynamic_cast<RadioMenuItem*> (&mode_items.back ());
			i->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::set_track_mode), ARDOUR::Destructive, true));
			i->set_active (normal == 0 && tape != 0 && non_layered == 0);
			i->set_inconsistent (tape != 0 && (normal != 0 || non_layered != 0));

 			mode_items.push_back (RadioMenuElem (mode_group, _("Non-Layered Mode")));
			i = dynamic_cast<RadioMenuItem*> (&mode_items.back ());
			i->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::set_track_mode), ARDOUR::NonLayered, true));
			i->set_active (normal == 0 && tape == 0 && non_layered != 0);
			i->set_inconsistent (non_layered != 0 && (normal != 0 || tape != 0));

			items.push_back (MenuElem (_("Mode"), *mode_menu));
		}


		items.push_back (SeparatorElem());

		build_playlist_menu ();
		items.push_back (MenuElem (_("Playlist"), *playlist_action_menu));
		items.back().set_sensitive (_editor.get_selection().tracks.size() <= 1);
	}

	route_group_menu->detach ();
	
	WeakRouteList r;
	for (TrackSelection::iterator i = _editor.get_selection().tracks.begin(); i != _editor.get_selection().tracks.end(); ++i) {
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*i);
		if (rtv) {
			r.push_back (rtv->route ());
		}
	}
	
	if (r.empty ()) {
		r.push_back (route ());
	}

	route_group_menu->build (r);
	items.push_back (MenuElem (_("Group"), *route_group_menu->menu ()));
	
	build_automation_action_menu (true);
	items.push_back (MenuElem (_("Automation"), *automation_action_menu));
	
	items.push_back (SeparatorElem());
	
	int active = 0;
	int inactive = 0;
	TrackSelection const & s = _editor.get_selection().tracks;
	for (TrackSelection::const_iterator i = s.begin(); i != s.end(); ++i) {
		RouteTimeAxisView* r = dynamic_cast<RouteTimeAxisView*> (*i);
		if (!r) {
			continue;
		}

		if (r->route()->active()) {
			++active;
		} else {
			++inactive;
		}
	}

	items.push_back (CheckMenuElem (_("Active")));
	CheckMenuItem* i = dynamic_cast<CheckMenuItem *> (&items.back());
	bool click_sets_active = true;
	if (active > 0 && inactive == 0) {
		i->set_active (true);
		click_sets_active = false;
	} else if (active > 0 && inactive > 0) {
		i->set_inconsistent (true);
	}
	i->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &RouteUI::set_route_active), click_sets_active, true));

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Hide"), sigc::bind (sigc::mem_fun(_editor, &PublicEditor::hide_track_in_display), this, true)));
	if (!Profile->get_sae()) {
		items.push_back (MenuElem (_("Remove"), sigc::bind (sigc::mem_fun(*this, &RouteUI::remove_this_route), true)));
	} else {
		items.push_front (SeparatorElem());
		items.push_front (MenuElem (_("Delete"), sigc::bind (sigc::mem_fun(*this, &RouteUI::remove_this_route), true)));
	}
}

void
RouteTimeAxisView::set_track_mode (TrackMode mode, bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_route_time_axis (boost::bind (&RouteTimeAxisView::set_track_mode, _1, mode, false));
	} else {

		bool needs_bounce;

		if (!track()->can_use_mode (mode, needs_bounce)) {

			if (!needs_bounce) {
				/* cannot be done */
				return;
			} else {
				cerr << "would bounce this one\n";
				return;
			}
		}

		track()->set_mode (mode);

		rec_enable_button->remove ();

		switch (mode) {
		case ARDOUR::NonLayered:
		case ARDOUR::Normal:
			rec_enable_button->set_image (::get_icon (X_("record_normal_red")));
			rec_enable_button->set_text (string());
			break;
		case ARDOUR::Destructive:
			rec_enable_button->set_image (::get_icon (X_("record_tape_red")));
			rec_enable_button->set_text (string());
			break;
		}

		rec_enable_button->show_all ();
	}
}

void
RouteTimeAxisView::show_timestretch (framepos_t start, framepos_t end, int layers, int layer)
{
	TimeAxisView::show_timestretch (start, end, layers, layer);

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
		timestretch_rect = new ArdourCanvas::Rectangle (canvas_display ());
		timestretch_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_TimeStretchFill());
		timestretch_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_TimeStretchOutline());
	}

	timestretch_rect->show ();
	timestretch_rect->raise_to_top ();

	double const x1 = start / _editor.get_current_zoom();
	double const x2 = (end - 1) / _editor.get_current_zoom();

	timestretch_rect->set (ArdourCanvas::Rect (x1, current_height() * (layers - layer - 1) / layers,
						   x2, current_height() * (layers - layer) / layers));
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
	int gmlen = h - 9;
	bool height_changed = (height == 0) || (h != height);

	int meter_width = 3;
	if (_route && _route->shared_peak_meter()->input_streams().n_total() == 1) {
		meter_width = 6;
	}
	gm.get_level_meter().setup_meters (gmlen, meter_width);

	TimeAxisView::set_height (h);

	if (_view) {
		_view->set_height ((double) current_height());
	}

	if (height >= preset_height (HeightNormal)) {

		reset_meter();

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

	} else {

		reset_meter();

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

	}

	if (height_changed && !no_redraw) {
		/* only emit the signal if the height really changed */
		request_redraw ();
	}
}

void
RouteTimeAxisView::route_color_changed ()
{
	if (_view) {
		_view->apply_color (color(), StreamView::RegionColor);
	}
}

void
RouteTimeAxisView::reset_samples_per_pixel ()
{
	set_samples_per_pixel (_editor.get_current_zoom());
}

void
RouteTimeAxisView::set_samples_per_pixel (double fpp)
{
	double speed = 1.0;

	if (track()) {
		speed = track()->speed();
	}

	if (_view) {
		_view->set_samples_per_pixel (fpp * speed);
	}

	TimeAxisView::set_samples_per_pixel (fpp * speed);
}

void
RouteTimeAxisView::set_align_choice (RadioMenuItem* mitem, AlignChoice choice, bool apply_to_selection)
{
        if (!mitem->get_active()) {
                /* this is one of the two calls made when these radio menu items change status. this one
                   is for the item that became inactive, and we want to ignore it.
                */
                return;
        }

	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_route_time_axis (boost::bind (&RouteTimeAxisView::set_align_choice, _1, mitem, choice, false));
	} else {
		if (track ()) {
			track()->set_align_choice (choice);
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
			int x = atoi(tmp);
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

	if (route_group() && route_group()->is_active() && route_group()->enabled_property (ARDOUR::Properties::select.property_id)) {
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

	if (route_group() && route_group()->is_active() && route_group()->enabled_property (ARDOUR::Properties::select.property_id)) {
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
	Gtkmm2ext::UI::instance()->call_slot (invalidator (*this), boost::bind (&RouteTimeAxisView::reset_samples_per_pixel, this));
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
	TimeAxisView::name_entry_changed ();

	string x = name_entry->get_text ();

	if (x == _route->name()) {
		return;
	}

	strip_whitespace_edges (x);

	if (x.length() == 0) {
		name_entry->set_text (_route->name());
		return;
	}

	if (_session->route_name_internal (x)) {
		ARDOUR_UI::instance()->popup_error (string_compose (_("You cannot create a track with that name as it is reserved for %1"),
								    PROGRAM_NAME));
		name_entry->grab_focus ();
	} else if (RouteUI::verify_new_route_name (x)) {
		_route->set_name (x);
	} else {
		name_entry->grab_focus ();
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
	case Delete:
		if (playlist->cut (time) != 0) {
                        vector<Command*> cmds;
                        playlist->rdiff (cmds);
                        _session->add_commands (cmds);
			
                        _session->add_command (new StatefulDiffCommand (playlist));
		}
		break;
		
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

	RadioMenuItem::Group playlist_group;
	boost::shared_ptr<Track> tr = track ();

        vector<boost::shared_ptr<Playlist> > playlists_tr = _session->playlists->playlists_for_track (tr);

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

	if (!route_group() || !route_group()->is_active() || !route_group()->enabled_property (ARDOUR::Properties::select.property_id)) {
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

	playlist_items.push_back (MenuElem(_("Select From All..."), sigc::mem_fun(*this, &RouteTimeAxisView::show_playlist_selector)));
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

	if (track()->playlist() == pl) {
		// exit when use_playlist is called by the creation of the playlist menu
		// or the playlist choice is unchanged
		return;
	}

	track()->use_playlist (pl);
	
	RouteGroup* rg = route_group();
	
	if (rg && rg->is_active() && rg->enabled_property (ARDOUR::Properties::select.property_id)) {
		std::string group_string = "." + rg->name() + ".";
		
		std::string take_name = pl->name();
		std::string::size_type idx = take_name.find(group_string);
		
		if (idx == std::string::npos)
			return;
		
		take_name = take_name.substr(idx + group_string.length()); // find the bit containing the take number / name
		
		boost::shared_ptr<RouteList> rl (rg->route_list());
		
		for (RouteList::const_iterator i = rl->begin(); i != rl->end(); ++i) {
			if ((*i) == this->route()) {
				continue;
			}

			std::string playlist_name = (*i)->name()+group_string+take_name;
			
			boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track>(*i);
			if (!track) {
				continue;
			}

			if (track->freeze_state() == Track::Frozen) {
				/* Don't change playlists of frozen tracks */
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

void
RouteTimeAxisView::update_playlist_tip ()
{
	RouteGroup* rg = route_group ();
	if (rg && rg->is_active() && rg->enabled_property (ARDOUR::Properties::select.property_id)) {
		string group_string = "." + rg->name() + ".";
		
		string take_name = track()->playlist()->name();
		string::size_type idx = take_name.find(group_string);
		
		if (idx != string::npos) {
			/* find the bit containing the take number / name */
			take_name = take_name.substr (idx + group_string.length());

			/* set the playlist button tooltip to the take name */
			ARDOUR_UI::instance()->set_tip (
				playlist_button,
				string_compose(_("Take: %1.%2"),
					Glib::Markup::escape_text(rg->name()),
					Glib::Markup::escape_text(take_name))
				);
			
			return;
		}
	}

	/* set the playlist button tooltip to the playlist name */
	ARDOUR_UI::instance()->set_tip (playlist_button, _("Playlist") + std::string(": ") + Glib::Markup::escape_text(track()->playlist()->name()));
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
		timestretch_rect->set_outline_color (ARDOUR_UI::config()->get_canvasvar_TimeStretchOutline());
	}
	//case cTimeStretchFill:
	if (timestretch_rect) {
		timestretch_rect->set_fill_color (ARDOUR_UI::config()->get_canvasvar_TimeStretchFill());
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
		bool changed = false;

		if ((changed = track->set_marked_for_display (menu->get_active())) && yn) {

			/* we made it visible, now trigger a redisplay. if it was hidden, then automation_track_hidden()
			   will have done that for us.
			*/

			if (changed && !no_redraw) {
				request_redraw ();
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

	if (menu && !_hidden) {
		ignore_toggle = true;
		menu->set_active (false);
		ignore_toggle = false;
	}

	if (_route && !no_redraw) {
		request_redraw ();
	}
}


void
RouteTimeAxisView::show_all_automation (bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_route_time_axis (boost::bind (&RouteTimeAxisView::show_all_automation, _1, false));
	} else {
		no_redraw = true;

		/* Show our automation */

		for (AutomationTracks::iterator i = _automation_tracks.begin(); i != _automation_tracks.end(); ++i) {
			i->second->set_marked_for_display (true);

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

		request_redraw ();
	}
}

void
RouteTimeAxisView::show_existing_automation (bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_route_time_axis (boost::bind (&RouteTimeAxisView::show_existing_automation, _1, false));
	} else {
		no_redraw = true;

		/* Show our automation */

		for (AutomationTracks::iterator i = _automation_tracks.begin(); i != _automation_tracks.end(); ++i) {
			if (i->second->has_automation()) {
				i->second->set_marked_for_display (true);

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

		request_redraw ();
	}
}

void
RouteTimeAxisView::hide_all_automation (bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_route_time_axis (boost::bind (&RouteTimeAxisView::hide_all_automation, _1, false));
	} else {
		no_redraw = true;

		/* Hide our automation */

		for (AutomationTracks::iterator i = _automation_tracks.begin(); i != _automation_tracks.end(); ++i) {
			i->second->set_marked_for_display (false);

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
		request_redraw ();
	}
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

/** Add an AutomationTimeAxisView to display automation for a processor's parameter */
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

	boost::shared_ptr<AutomationControl> control
		= boost::dynamic_pointer_cast<AutomationControl>(processor->control(what, true));
	
	pan->view = boost::shared_ptr<AutomationTimeAxisView>(
		new AutomationTimeAxisView (_session, _route, processor, control, control->parameter (),
					    _editor, *this, false, parent_canvas, 
					    processor->describe_parameter (what), processor->name()));

	pan->view->Hiding.connect (sigc::bind (sigc::mem_fun(*this, &RouteTimeAxisView::processor_automation_track_hidden), pan, processor));

	add_automation_child (control->parameter(), pan->view, pan->view->marked_for_display ());

	if (_view) {
		_view->foreach_regionview (sigc::mem_fun(*pan->view.get(), &TimeAxisView::add_ghost));
	}
}

void
RouteTimeAxisView::processor_automation_track_hidden (RouteTimeAxisView::ProcessorAutomationNode* pan, boost::shared_ptr<Processor>)
{
	if (!_hidden) {
		pan->menu_item->set_active (false);
	}

	if (!no_redraw) {
		request_redraw ();
	}
}

void
RouteTimeAxisView::add_existing_processor_automation_curves (boost::weak_ptr<Processor> p)
{
	boost::shared_ptr<Processor> processor (p.lock ());

	if (!processor || boost::dynamic_pointer_cast<Amp> (processor)) {
		/* The Amp processor is a special case and is dealt with separately */
		return;
	}

	set<Evoral::Parameter> existing;

	processor->what_has_data (existing);

	for (set<Evoral::Parameter>::iterator i = existing.begin(); i != existing.end(); ++i) {
		
		Evoral::Parameter param (*i);
		boost::shared_ptr<AutomationLine> al;

		if ((al = find_processor_automation_curve (processor, param)) != 0) {
			al->queue_reset ();
		} else {
			add_processor_automation_curve (processor, param);
		}
	}
}

void
RouteTimeAxisView::add_automation_child (Evoral::Parameter param, boost::shared_ptr<AutomationTimeAxisView> track, bool show)
{
	using namespace Menu_Helpers;

	add_child (track);

	track->Hiding.connect (sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::automation_track_hidden), param));

	_automation_tracks[param] = track;

	/* existing state overrides "show" argument */
	string s = track->gui_property ("visible");
	if (!s.empty()) { 
		show = string_is_affirmative (s);
	}

	/* this might or might not change the visibility status, so don't rely on it */
	track->set_marked_for_display (show);

	if (show && !no_redraw) {
		request_redraw ();
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

	/* we use this override to veto the Amp processor from the plugin menu,
	   as its automation lane can be accessed using the special "Fader" menu
	   option
	*/

	if (boost::dynamic_pointer_cast<Amp> (processor) != 0) {
		return;
	}

	using namespace Menu_Helpers;
	ProcessorAutomationInfo *rai;
	list<ProcessorAutomationInfo*>::iterator x;

	const std::set<Evoral::Parameter>& automatable = processor->what_can_be_automated ();

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

	std::set<Evoral::Parameter> has_visible_automation;
	AutomationTimeAxisView::what_has_visible_automation (processor, has_visible_automation);

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

	if (pan->view && pan->view->set_marked_for_display (showit)) {
		redraw = true;
	}
	
	if (redraw && !no_redraw) {
		request_redraw ();
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

	setup_processor_menu_and_curves ();

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
		request_redraw ();
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

bool
RouteTimeAxisView::can_edit_name () const
{
	/* we do not allow track name changes if it is record enabled
	 */
	return !_route->record_enabled();
}

void
RouteTimeAxisView::update_rec_display ()
{
	RouteUI::update_rec_display ();
}

void
RouteTimeAxisView::set_layer_display (LayerDisplay d, bool apply_to_selection)
{
	if (_ignore_set_layer_display) {
		return;
	}
	
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_route_time_axis (boost::bind (&RouteTimeAxisView::set_layer_display, _1, d, false));
	} else {

		if (_view) {
			_view->set_layer_display (d);
		}

		set_gui_property (X_("layer-display"), enum_2_string (d));
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
		int meter_width = 3;
		if (_route && _route->shared_peak_meter()->input_streams().n_total() == 1) {
			meter_width = 6;
		}
		gm.get_level_meter().setup_meters (height - 9, meter_width);
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
	if (_route && !no_redraw) {
		request_redraw ();
	}
}

void
RouteTimeAxisView::io_changed (IOChange /*change*/, void */*src*/)
{
	reset_meter ();
	if (_route && !no_redraw) {
		request_redraw ();
	}
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
RouteTimeAxisView::add_underlay (StreamView* v, bool /*update_xml*/)
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

#ifdef GUI_OBJECT_STATE_FIX_REQUIRED
		if (update_xml) {
			if (!underlay_xml_node) {
				underlay_xml_node = xml_node->add_child("Underlays");
			}

			XMLNode* node = underlay_xml_node->add_child("Underlay");
			XMLProperty* prop = node->add_property("id");
			prop->set_value(v->trackview().route()->id().to_s());
		}
#endif
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
	if (_route && _route->solo_safe()) {
		solo_button->set_visual_state (Gtkmm2ext::VisualState (solo_button->visual_state() | Gtkmm2ext::Insensitive));
	} else {
		solo_button->set_visual_state (Gtkmm2ext::VisualState (solo_button->visual_state() & ~Gtkmm2ext::Insensitive));
	}
	if (Config->get_solo_control_is_listen_control()) {
		switch (Config->get_listen_position()) {
			case AfterFaderListen:
				solo_button->set_text (_("A"));
				ARDOUR_UI::instance()->set_tip (*solo_button, _("After-fade listen (AFL)"));
				break;
			case PreFaderListen:
				solo_button->set_text (_("P"));
				ARDOUR_UI::instance()->set_tip (*solo_button, _("Pre-fade listen (PFL)"));
			break;
		}
	} else {
		solo_button->set_text (_("s"));
		ARDOUR_UI::instance()->set_tip (*solo_button, _("Solo"));
	}
	mute_button->set_text (_("m"));
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

	if (_view) {
		_view->foreach_regionview (sigc::mem_fun (*gain_track.get(), &TimeAxisView::add_ghost));
	}

	add_automation_child (Evoral::Parameter(GainAutomation), gain_track, show);
}

static
void add_region_to_list (RegionView* rv, RegionList* l)
{
	l->push_back (rv->region());
}

RegionView*
RouteTimeAxisView::combine_regions ()
{
	/* as of may 2011, we do not offer uncombine for MIDI tracks
	 */

	if (!is_audio_track()) {
		return 0;
	}

	if (!_view) {
		return 0;
	}

	RegionList selected_regions;
	boost::shared_ptr<Playlist> playlist = track()->playlist();

	_view->foreach_selected_regionview (sigc::bind (sigc::ptr_fun (add_region_to_list), &selected_regions));

	if (selected_regions.size() < 2) {
		return 0;
	}

	playlist->clear_changes ();
	boost::shared_ptr<Region> compound_region = playlist->combine (selected_regions);

	_session->add_command (new StatefulDiffCommand (playlist));
	/* make the new region be selected */

	return _view->find_view (compound_region);
}

void
RouteTimeAxisView::uncombine_regions ()
{
	/* as of may 2011, we do not offer uncombine for MIDI tracks
	 */
	if (!is_audio_track()) {
		return;
	}

	if (!_view) {
		return;
	}

	RegionList selected_regions;
	boost::shared_ptr<Playlist> playlist = track()->playlist();

	/* have to grab selected regions first because the uncombine is going
	 * to change that in the middle of the list traverse
	 */

	_view->foreach_selected_regionview (sigc::bind (sigc::ptr_fun (add_region_to_list), &selected_regions));

	playlist->clear_changes ();

	for (RegionList::iterator i = selected_regions.begin(); i != selected_regions.end(); ++i) {
		playlist->uncombine (*i);
	}

	_session->add_command (new StatefulDiffCommand (playlist));
}

string
RouteTimeAxisView::state_id() const
{
	return string_compose ("rtav %1", _route->id().to_s());
}


void
RouteTimeAxisView::remove_child (boost::shared_ptr<TimeAxisView> c)
{
	TimeAxisView::remove_child (c);
	
	boost::shared_ptr<AutomationTimeAxisView> a = boost::dynamic_pointer_cast<AutomationTimeAxisView> (c);
	if (a) {
		for (AutomationTracks::iterator i = _automation_tracks.begin(); i != _automation_tracks.end(); ++i) {
			if (i->second == a) {
				_automation_tracks.erase (i);
				return;
			}
		}
	}
}
