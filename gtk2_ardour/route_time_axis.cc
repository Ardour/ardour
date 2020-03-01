/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2008-2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2014 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2014-2017 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2016 Julien "_FrnchFrgg_" RIVAUD <frnchfrgg@free.fr>
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
#include <cassert>

#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <utility>

#include <sigc++/bind.h>

#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/stock.h>

#include "pbd/error.h"
#include "pbd/stl_delete.h"
#include "pbd/whitespace.h"
#include "pbd/memento_command.h"
#include "pbd/enumwriter.h"
#include "pbd/stateful_diff_command.h"

#include "evoral/Parameter.h"

#include "ardour/amp.h"
#include "ardour/meter.h"
#include "ardour/event_type_map.h"
#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/plugin_insert.h"
#include "ardour/processor.h"
#include "ardour/profile.h"
#include "ardour/route_group.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"
#include "ardour/track.h"

#include "canvas/debug.h"

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/utils.h"

#include "widgets/ardour_button.h"
#include "widgets/prompter.h"
#include "widgets/tooltips.h"

#include "ardour_ui.h"
#include "audio_streamview.h"
#include "debug.h"
#include "enums_convert.h"
#include "route_time_axis.h"
#include "automation_time_axis.h"
#include "enums.h"
#include "gui_thread.h"
#include "item_counts.h"
#include "keyboard.h"
#include "paste_context.h"
#include "patch_change_widget.h"
#include "playlist_selector.h"
#include "point_selection.h"
#include "public_editor.h"
#include "region_view.h"
#include "rgb_macros.h"
#include "selection.h"
#include "streamview.h"
#include "ui_config.h"
#include "utils.h"
#include "route_group_menu.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace Editing;
using namespace std;
using std::list;

RouteTimeAxisView::RouteTimeAxisView (PublicEditor& ed, Session* sess, ArdourCanvas::Canvas& canvas)
	: RouteUI(sess)
	, StripableTimeAxisView(ed, sess, canvas)
	, _view (0)
	, button_table (3, 3)
	, route_group_button (S_("RTAV|G"))
	, playlist_button (S_("RTAV|P"))
	, automation_button (S_("RTAV|A"))
	, automation_action_menu (0)
	, plugins_submenu_item (0)
	, route_group_menu (0)
	, playlist_action_menu (0)
	, gm (sess, true, 75, 14)
	, _ignore_set_layer_display (false)
	, pan_automation_item(NULL)
{
	number_label.set_name("tracknumber label");
	number_label.set_elements((ArdourButton::Element)(ArdourButton::Edge|ArdourButton::Body|ArdourButton::Text|ArdourButton::Inactive));
	number_label.set_alignment(.5, .5);
	number_label.set_fallthrough_to_parent (true);

	sess->config.ParameterChanged.connect (*this, invalidator (*this), boost::bind (&RouteTimeAxisView::parameter_changed, this, _1), gui_context());
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &RouteTimeAxisView::parameter_changed));

	parameter_changed ("editor-stereo-only-meters");
}

void
RouteTimeAxisView::route_property_changed (const PBD::PropertyChange& what_changed)
{
	if (what_changed.contains (ARDOUR::Properties::name)) {
		label_view ();
	}
}

void
RouteTimeAxisView::set_route (boost::shared_ptr<Route> rt)
{
	RouteUI::set_route (rt);
	StripableTimeAxisView::set_stripable (rt);

	CANVAS_DEBUG_NAME (_canvas_display, string_compose ("main for %1", rt->name()));
	CANVAS_DEBUG_NAME (selection_group, string_compose ("selections for %1", rt->name()));
	CANVAS_DEBUG_NAME (_ghost_group, string_compose ("ghosts for %1", rt->name()));

	int meter_width = 3;
	if (_route && _route->shared_peak_meter()->input_streams().n_total() == 1) {
		meter_width = 6;
	}
	gm.set_controls (_route, _route->shared_peak_meter(), _route->amp(), _route->gain_control());
	gm.get_level_meter().set_no_show_all();
	gm.get_level_meter().setup_meters(50, meter_width);
	gm.update_gain_sensitive ();

	uint32_t height;
	if (get_gui_property ("height", height)) {
		set_height (height);
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

	timestretch_rect = 0;
	no_redraw = false;

	ignore_toggle = false;

	route_group_button.set_name ("route button");
	playlist_button.set_name ("route button");
	automation_button.set_name ("route button");
	
	route_group_button.signal_button_press_event().connect (sigc::mem_fun(*this, &RouteTimeAxisView::route_group_click), false);
	playlist_button.signal_button_press_event().connect (sigc::mem_fun(*this, &RouteTimeAxisView::playlist_click), false);
	automation_button.signal_button_press_event().connect (sigc::mem_fun(*this, &RouteTimeAxisView::automation_click), false);

	if (is_track()) {

		if (ARDOUR::Profile->get_mixbus()) {
			controls_table.attach (*rec_enable_button, 0, 1, 0, 1, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
		} else {
			controls_table.attach (*rec_enable_button, 2, 3, 0, 1, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
		}

		if (is_midi_track()) {
			set_tooltip(*rec_enable_button, _("Record (Right-click for Step Edit)"));
			gm.set_fader_name ("MidiTrackFader");
		} else {
			set_tooltip(*rec_enable_button, _("Record"));
			gm.set_fader_name ("AudioTrackFader");
		}

		/* set playlist button tip to the current playlist, and make it update when it changes */
		update_playlist_tip ();
		track()->PlaylistChanged.connect (*this, invalidator (*this), ui_bind(&RouteTimeAxisView::update_playlist_tip, this), gui_context());

	} else {
		gm.set_fader_name ("AudioBusFader");
		Gtk::Fixed *blank = manage(new Gtk::Fixed());
		controls_button_size_group->add_widget(*blank);
		if (ARDOUR::Profile->get_mixbus() ) {
			controls_table.attach (*blank, 0, 1, 0, 1, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
		} else {
			controls_table.attach (*blank, 2, 3, 0, 1, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
		}
		blank->show();
	}

	top_hbox.pack_end(gm.get_level_meter(), false, false, 2);

	if (!ARDOUR::Profile->get_mixbus()) {
		controls_meters_size_group->add_widget (gm.get_level_meter());
	}

	if (_route->is_master()) {
		route_group_button.set_sensitive(false);
	}

	_route->meter_change.connect (*this, invalidator (*this), bind (&RouteTimeAxisView::meter_changed, this), gui_context());
	_route->input()->changed.connect (*this, invalidator (*this), boost::bind (&RouteTimeAxisView::io_changed, this, _1, _2), gui_context());
	_route->output()->changed.connect (*this, invalidator (*this), boost::bind (&RouteTimeAxisView::io_changed, this, _1, _2), gui_context());
	_route->track_number_changed.connect (*this, invalidator (*this), boost::bind (&RouteTimeAxisView::label_view, this), gui_context());

	if (ARDOUR::Profile->get_mixbus()) {
		controls_table.attach (*mute_button, 1, 2, 0, 1, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
	} else {
		controls_table.attach (*mute_button, 3, 4, 0, 1, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
	}
	// mute button is always present, it is used to
	// force the 'blank' placeholders to the proper size
	controls_button_size_group->add_widget(*mute_button);

	if (!_route->is_master()) {
		if (ARDOUR::Profile->get_mixbus()) {
			controls_table.attach (*solo_button, 2, 3, 0, 1, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
		} else {
			controls_table.attach (*solo_button, 4, 5, 0, 1, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
		}
	} else {
		Gtk::Fixed *blank = manage(new Gtk::Fixed());
		controls_button_size_group->add_widget(*blank);
		if (ARDOUR::Profile->get_mixbus()) {
			controls_table.attach (*blank, 2, 3, 0, 1, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
		} else {
			controls_table.attach (*blank, 4, 5, 0, 1, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
		}
		blank->show();
	}

	if (ARDOUR::Profile->get_mixbus()) {
		controls_table.attach (route_group_button, 2, 3, 2, 3, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
		controls_table.attach (gm.get_gain_slider(), 3, 5, 2, 3, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 1, 0);
	} else {
		controls_table.attach (route_group_button, 4, 5, 2, 3, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
		controls_table.attach (gm.get_gain_slider(), 0, 2, 2, 3, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 1, 0);
	}

	set_tooltip(*solo_button,_("Solo"));
	set_tooltip(*mute_button,_("Mute"));
	set_tooltip(route_group_button, _("Route Group"));

	mute_button->set_tweaks(ArdourButton::TrackHeader);
	solo_button->set_tweaks(ArdourButton::TrackHeader);
	rec_enable_button->set_tweaks(ArdourButton::TrackHeader);
	playlist_button.set_tweaks(ArdourButton::TrackHeader);
	automation_button.set_tweaks(ArdourButton::TrackHeader);
	route_group_button.set_tweaks(ArdourButton::TrackHeader);

	if (is_midi_track()) {
		set_tooltip(automation_button, _("MIDI Controllers and Automation"));
	} else {
		set_tooltip(automation_button, _("Automation"));
	}

	update_track_number_visibility();
	route_active_changed();
	label_view ();

	if (ARDOUR::Profile->get_mixbus()) {
		controls_table.attach (automation_button, 1, 2, 2, 3, Gtk::SHRINK, Gtk::SHRINK);
	} else {
		controls_table.attach (automation_button, 3, 4, 2, 3, Gtk::SHRINK, Gtk::SHRINK);
	}

	if (is_track() && track()->mode() == ARDOUR::Normal) {
		if (ARDOUR::Profile->get_mixbus()) {
			controls_table.attach (playlist_button, 0, 1, 2, 3, Gtk::SHRINK, Gtk::SHRINK);
		} else {
			controls_table.attach (playlist_button, 2, 3, 2, 3, Gtk::SHRINK, Gtk::SHRINK);
		}
	}

	_y_position = -1;

	_route->processors_changed.connect (*this, invalidator (*this), boost::bind (&RouteTimeAxisView::processors_changed, this, _1), gui_context());

	if (is_track()) {

		LayerDisplay layer_display;
		if (get_gui_property ("layer-display", layer_display)) {
			set_layer_display (layer_display);
		}

		track()->FreezeChange.connect (*this, invalidator (*this), boost::bind (&RouteTimeAxisView::map_frozen, this), gui_context());
		track()->SpeedChanged.connect (*this, invalidator (*this), boost::bind (&RouteTimeAxisView::speed_changed, this), gui_context());

		/* pick up the correct freeze state */
		map_frozen ();

	}

	UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &RouteTimeAxisView::color_handler));

	PropertyList* plist = new PropertyList();

	plist->add (ARDOUR::Properties::group_mute, true);
	plist->add (ARDOUR::Properties::group_solo, true);

	route_group_menu = new RouteGroupMenu (_session, plist);

	gm.get_level_meter().signal_scroll_event().connect (sigc::mem_fun (*this, &RouteTimeAxisView::controls_ebox_scroll), false);
}

RouteTimeAxisView::~RouteTimeAxisView ()
{
	cleanup_gui_properties ();

	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {
		delete *i;
	}

	delete playlist_action_menu;
	delete automation_action_menu;

	delete _view;
	_view = 0;

	_automation_tracks.clear ();

	delete route_group_menu;
	CatchDeletion (this);
}

string
RouteTimeAxisView::name() const
{
	if (_route) {
		return _route->name();
	}
	return string();
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

bool
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
	if (ev->button == 1) {
		Gtkmm2ext::anchored_menu_popup(route_group_menu->menu(),
		                               &route_group_button,
		                               "", 1, ev->time);
	} else {
		route_group_menu->menu()->popup (ev->button, ev->time);
	}

	return true;
}

void
RouteTimeAxisView::playlist_changed ()
{
	label_view ();
}

void
RouteTimeAxisView::label_view ()
{
	string x = _route->name ();
	if (x != name_label.get_text ()) {
		name_label.set_text (x);
	}

	inactive_label.set_text (string_compose("(%1)", x));

	const int64_t track_number = _route->track_number ();
	if (track_number == 0) {
		number_label.set_text ("");
	} else {
		number_label.set_text (PBD::to_string (abs(_route->track_number ())));
	}
}

void
RouteTimeAxisView::update_track_number_visibility ()
{
	DisplaySuspender ds;
	bool show_label = _session->config.get_track_name_number();

	if (_route && _route->is_master()) {
		show_label = false;
	}

	if (number_label.get_parent()) {
		number_label.get_parent()->remove (number_label);
	}
	if (show_label) {
		if (!_route->active()) {
			inactive_table.attach (number_label, 0, 1, 0, 1, Gtk::SHRINK, Gtk::EXPAND|Gtk::FILL, 1, 0);
		} else if (ARDOUR::Profile->get_mixbus()) {
			controls_table.attach (number_label, 3, 4, 0, 1, Gtk::SHRINK, Gtk::EXPAND|Gtk::FILL, 1, 0);
		} else {
			controls_table.attach (number_label, 0, 1, 0, 1, Gtk::SHRINK, Gtk::EXPAND|Gtk::FILL, 1, 0);
		}
		// see ArdourButton::on_size_request(), we should probably use a global size-group here instead.
		// except the width of the number label is subtracted from the name-hbox, so we
		// need to explictly calculate it anyway until the name-label & entry become ArdourWidgets.
		int tnw = (2 + std::max(2u, _session->track_number_decimals())) * number_label.char_pixel_width();
		if (tnw & 1) --tnw;
		number_label.set_size_request(tnw, -1);
		number_label.show ();
	} else {
		number_label.hide ();
	}
}

void
RouteTimeAxisView::route_active_changed ()
{
	RouteUI::route_active_changed ();
	update_track_number_visibility ();
}

void
RouteTimeAxisView::parameter_changed (string const & p)
{
	if (p == "track-name-number") {
		update_track_number_visibility();
	} else if (p == "editor-stereo-only-meters") {
		if (UIConfiguration::instance().get_editor_stereo_only_meters()) {
			gm.get_level_meter().set_max_audio_meter_count (2);
		} else {
			gm.get_level_meter().set_max_audio_meter_count (0);
		}
	}
}

void
RouteTimeAxisView::take_name_changed (void *src)
{
	if (src != this) {
		label_view ();
	}
}

bool
RouteTimeAxisView::playlist_click (GdkEventButton *ev)
{
	if (ev->button != 1) {
		return true;
	}

	build_playlist_menu ();
	conditionally_add_to_selection ();
	Gtkmm2ext::anchored_menu_popup(playlist_action_menu, &playlist_button,
	                               "", 1, ev->time);
	return true;
}

bool
RouteTimeAxisView::automation_click (GdkEventButton *ev)
{
	if (ev->button != 1) {
		return true;
	}

	conditionally_add_to_selection ();
	build_automation_action_menu (false);
	Gtkmm2ext::anchored_menu_popup(automation_action_menu, &automation_button,
	                               "", 1, ev->time);
	return true;
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

	bool single_track_selected = (!for_selection || _editor.get_selection().tracks.size() == 1);

	if (!subplugin_menu.items().empty()) {
		items.push_back (SeparatorElem ());
		items.push_back (MenuElem (_("Processor automation"), subplugin_menu));
		items.back().set_sensitive (single_track_selected);
	}

	/* Add any route automation */

	if (gain_track) {
		items.push_back (CheckMenuElem (_("Fader"), sigc::mem_fun (*this, &RouteTimeAxisView::update_gain_track_visibility)));
		gain_automation_item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
		gain_automation_item->set_active (single_track_selected &&
		                                  string_to<bool>(gain_track->gui_property ("visible")));

		_main_automation_menu_map[Evoral::Parameter(GainAutomation)] = gain_automation_item;
	}

	if (trim_track) {
		items.push_back (CheckMenuElem (_("Trim"), sigc::mem_fun (*this, &RouteTimeAxisView::update_trim_track_visibility)));
		trim_automation_item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
		trim_automation_item->set_active (single_track_selected &&
		                                  string_to<bool>(trim_track->gui_property ("visible")));

		_main_automation_menu_map[Evoral::Parameter(TrimAutomation)] = trim_automation_item;
	}

	if (mute_track) {
		items.push_back (CheckMenuElem (_("Mute"), sigc::mem_fun (*this, &RouteTimeAxisView::update_mute_track_visibility)));
		mute_automation_item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
		mute_automation_item->set_active (single_track_selected &&
		                                  string_to<bool>(mute_track->gui_property ("visible")));

		_main_automation_menu_map[Evoral::Parameter(MuteAutomation)] = mute_automation_item;
	}

	if (!pan_tracks.empty() && !ARDOUR::Profile->get_mixbus()) {
		items.push_back (CheckMenuElem (_("Pan"), sigc::mem_fun (*this, &RouteTimeAxisView::update_pan_track_visibility)));
		pan_automation_item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
		pan_automation_item->set_active (single_track_selected &&
		                                 string_to<bool>(pan_tracks.front()->gui_property ("visible")));

		set<Evoral::Parameter> const & params = _route->pannable()->what_can_be_automated ();
		for (set<Evoral::Parameter>::const_iterator p = params.begin(); p != params.end(); ++p) {
			_main_automation_menu_map[*p] = pan_automation_item;
		}
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

	items.push_back (MenuElem (_("Comments..."), sigc::mem_fun (*this, &RouteUI::open_comment_editor)));

	items.push_back (MenuElem (_("Inputs..."), sigc::mem_fun (*this, &RouteUI::edit_input_configuration)));

	items.push_back (MenuElem (_("Outputs..."), sigc::mem_fun (*this, &RouteUI::edit_output_configuration)));

	items.push_back (SeparatorElem());

	if (_size_menu) {
		detach_menu (*_size_menu);
	}
	build_size_menu ();
	items.push_back (MenuElem (_("Height"), *_size_menu));
	items.push_back (SeparatorElem());

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
		int unchangeable = 0;
		TrackSelection const & s = _editor.get_selection().tracks;

		for (TrackSelection::const_iterator i = s.begin(); i != s.end(); ++i) {
			StreamView* v = (*i)->view ();
			if (!v) {
				continue;
			}

			if (v->can_change_layer_display()) {
				switch (v->layer_display ()) {
				case Overlaid:
					++overlaid;
					break;
				case Stacked:
				case Expanded:
					++stacked;
					break;
				}
			} else {
				unchangeable++;
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

		if (unchangeable) {
			i->set_sensitive (false);
		}

		layers_items.push_back (RadioMenuElem (layers_group, _("Stacked")));
		i = dynamic_cast<RadioMenuItem*> (&layers_items.back ());
		i->set_active (overlaid == 0 && stacked != 0);
		i->set_inconsistent (overlaid != 0 && stacked != 0);
		i->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::set_layer_display), Stacked, true));

		if (unchangeable) {
			i->set_sensitive (false);
		}

		_ignore_set_layer_display = false;

		items.push_back (MenuElem (_("Layers"), *layers_menu));

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

		for (TrackSelection::const_iterator t = s.begin(); t != s.end(); ++t) {
			RouteTimeAxisView* r = dynamic_cast<RouteTimeAxisView*> (*t);
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
			delete alignment_menu;
		}

		items.push_back (SeparatorElem());

		build_playlist_menu ();
		items.push_back (MenuElem (_("Playlist"), *playlist_action_menu));
		items.back().set_sensitive (_editor.get_selection().tracks.size() <= 1);
	}

	if (!is_midi_track () && _route->the_instrument ()) {
		/* MIDI Bus */
		items.push_back (MenuElem (_("Patch Selector..."),
					sigc::mem_fun(*this, &RouteUI::select_midi_patch)));
		items.push_back (SeparatorElem());
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

	if (!_route->is_master()) {
		route_group_menu->build (r);
		items.push_back (MenuElem (_("Group"), *route_group_menu->menu ()));
	}

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
	Gtk::CheckMenuItem* i = dynamic_cast<Gtk::CheckMenuItem *> (&items.back());
	bool click_sets_active = true;
	if (active > 0 && inactive == 0) {
		i->set_active (true);
		click_sets_active = false;
	} else if (active > 0 && inactive > 0) {
		i->set_inconsistent (true);
	}
	i->set_sensitive(! _session->transport_rolling());
	i->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &RouteUI::set_route_active), click_sets_active, true));

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Hide"), sigc::bind (sigc::mem_fun(_editor, &PublicEditor::hide_track_in_display), this, true)));
	if (_route && !_route->is_master()) {
		items.push_back (SeparatorElem());
		items.push_back (MenuElem (_("Duplicate..."), boost::bind (&ARDOUR_UI::start_duplicate_routes, ARDOUR_UI::instance())));
	}
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Remove"), sigc::mem_fun(_editor, &PublicEditor::remove_tracks)));
}

void
RouteTimeAxisView::show_timestretch (samplepos_t start, samplepos_t end, int layers, int layer)
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
		timestretch_rect->set_fill_color (Gtkmm2ext::HSV (UIConfiguration::instance().color ("time stretch fill")).mod (UIConfiguration::instance().modifier ("time stretch fill")).color());
		timestretch_rect->set_outline_color (UIConfiguration::instance().color ("time stretch outline"));
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
RouteTimeAxisView::set_height (uint32_t h, TrackHeightMode m)
{
	int gmlen = h - 9;
	bool height_changed = (height == 0) || (h != height);

	int meter_width = 3;
	if (_route && _route->shared_peak_meter()->input_streams().n_total() == 1) {
		meter_width = 6;
	}
	gm.get_level_meter().setup_meters (gmlen, meter_width);

	TimeAxisView::set_height (h, m);

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
	using namespace ARDOUR_UI_UTILS;
	if (_view) {
		_view->apply_color (color(), StreamView::RegionColor);
	}
	number_label.set_fixed_colors (gdk_color_to_rgba (color()), gdk_color_to_rgba (color()));
}

void
RouteTimeAxisView::set_samples_per_pixel (double fpp)
{
	if (_view) {
		_view->set_samples_per_pixel (fpp);
	}

	StripableTimeAxisView::set_samples_per_pixel (fpp);
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
	Prompter prompter (true);
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
	prompter.add_button (_("Rename"), Gtk::RESPONSE_ACCEPT);
	prompter.set_initial_text (pl->name());
	prompter.set_response_sensitive (Gtk::RESPONSE_ACCEPT, false);

	while (true) {
		if (prompter.run () != Gtk::RESPONSE_ACCEPT) {
			break;
		}
		prompter.get_result (name);
		if (name.length()) {
			if (_session->playlists()->by_name (name)) {
				MessageDialog msg (_("Given playlist name is not unique."));
				msg.run ();
				prompter.set_initial_text (Playlist::bump_name (name, *_session));
			} else {
				pl->set_name (name);
				break;
			}
		}
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
RouteTimeAxisView::use_new_playlist (bool prompt, vector<boost::shared_ptr<Playlist> > const & playlists_before_op, bool copy)
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

	if (route_group() && route_group()->is_active() && route_group()->enabled_property (ARDOUR::Properties::group_select.property_id)) {
		name = resolve_new_group_playlist_name(name,playlists_before_op);
	}

	while (_session->playlists()->by_name(name)) {
		name = Playlist::bump_name (name, *_session);
	}

	if (prompt) {
		// TODO: The prompter "new" button should be de-activated if the user
		// specifies a playlist name which already exists in the session.

		Prompter prompter (true);

		if (copy) {
			prompter.set_title (_("New Copy Playlist"));
			prompter.set_prompt (_("Name for playlist copy:"));
		} else {
			prompter.set_title (_("New Playlist"));
			prompter.set_prompt (_("Name for new playlist:"));
		}
		prompter.set_initial_text (name);
		prompter.add_button (Gtk::Stock::NEW, Gtk::RESPONSE_ACCEPT);
		prompter.set_response_sensitive (Gtk::RESPONSE_ACCEPT, true);
		prompter.show_all ();

		while (true) {
			if (prompter.run () != Gtk::RESPONSE_ACCEPT) {
				return;
			}
			prompter.get_result (name);
			if (name.length()) {
				if (_session->playlists()->by_name (name)) {
					MessageDialog msg (_("Given playlist name is not unique."));
					msg.run ();
					prompter.set_initial_text (Playlist::bump_name (name, *_session));
				} else {
					break;
				}
			}
		}
	}

	if (name.length()) {
		if (copy) {
			tr->use_copy_playlist ();
		} else {
			tr->use_default_new_playlist ();
		}
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

		_editor.begin_reversible_selection_op (X_("Selection Click"));

		if (_editor.get_selection().selected (this)) {
			_editor.get_selection().clear_tracks ();
		} else {
			_editor.select_all_visible_lanes ();
		}

		_editor.commit_reversible_selection_op ();

		return;
	}

	_editor.begin_reversible_selection_op (X_("Selection Click"));

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

	_editor.commit_reversible_selection_op ();

	_editor.set_selected_mixer_strip (*this);
}

void
RouteTimeAxisView::set_selected_points (PointSelection& points)
{
	StripableTimeAxisView::set_selected_points (points);
	AudioStreamView* asv = dynamic_cast<AudioStreamView*>(_view);
	if (asv) {
		asv->set_selected_points (points);
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
RouteTimeAxisView::get_selectables (samplepos_t start, samplepos_t end, double top, double bot, list<Selectable*>& results, bool within)
{
	if ((_view && ((top < 0.0 && bot < 0.0))) || touched (top, bot)) {
		_view->get_selectables (start, end, top, bot, results, within);
	}

	/* pick up visible automation tracks */
	for (Children::iterator i = children.begin(); i != children.end(); ++i) {
		if (!(*i)->hidden()) {
			(*i)->get_selectables (start, end, top, bot, results, within);
		}
	}
}

void
RouteTimeAxisView::get_inverted_selectables (Selection& sel, list<Selectable*>& results)
{
	if (_view) {
		_view->get_inverted_selectables (sel, results);
	}
	StripableTimeAxisView::get_inverted_selectables (sel, results);
}

RouteGroup*
RouteTimeAxisView::route_group () const
{
	return _route->route_group();
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

bool
RouteTimeAxisView::name_entry_changed (string const& str)
{
	if (str == _route->name()) {
		return true;
	}

	string x = str;

	strip_whitespace_edges (x);

	if (x.empty()) {
		return false;
	}

	if (_session->route_name_internal (x)) {
		ARDOUR_UI::instance()->popup_error (string_compose (_("The name \"%1\" is reserved for %2"), x, PROGRAM_NAME));
		return false;
	} else if (RouteUI::verify_new_route_name (x)) {
		_route->set_name (x);
		return true;
	}

	return false;
}

boost::shared_ptr<Region>
RouteTimeAxisView::find_next_region (samplepos_t pos, RegionPoint point, int32_t dir)
{
	boost::shared_ptr<Playlist> pl = playlist ();

	if (pl) {
		return pl->find_next_region (pos, point, dir);
	}

	return boost::shared_ptr<Region> ();
}

samplepos_t
RouteTimeAxisView::find_next_region_boundary (samplepos_t pos, int32_t dir)
{
	boost::shared_ptr<Playlist> pl = playlist ();

	if (pl) {
		return pl->find_next_region_boundary (pos, dir);
	}

	return -1;
}

void
RouteTimeAxisView::fade_range (TimeSelection& selection)
{
	boost::shared_ptr<Playlist> what_we_got;
	boost::shared_ptr<Track> tr = track ();
	boost::shared_ptr<Playlist> playlist;

	if (tr == 0) {
		/* route is a bus, not a track */
		return;
	}

	playlist = tr->playlist();

	TimeSelection time (selection);

	playlist->clear_changes ();
	playlist->clear_owned_changes ();

	playlist->fade_range (time);

	vector<Command*> cmds;
	playlist->rdiff (cmds);
	_session->add_commands (cmds);
	_session->add_command (new StatefulDiffCommand (playlist));

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

	playlist->clear_changes ();
	playlist->clear_owned_changes ();

	switch (op) {
	case Delete:
		if (playlist->cut (time) != 0) {
			if (Config->get_edit_mode() == Ripple) {
				playlist->ripple(time.start(), -time.length(), NULL);
			}
			// no need to exclude any regions from rippling here

			vector<Command*> cmds;
			playlist->rdiff (cmds);
			_session->add_commands (cmds);

			_session->add_command (new StatefulDiffCommand (playlist));
		}
		break;

	case Cut:
		if ((what_we_got = playlist->cut (time)) != 0) {
			_editor.get_cut_buffer().add (what_we_got);
			if (Config->get_edit_mode() == Ripple) {
				playlist->ripple(time.start(), -time.length(), NULL);
			}
			// no need to exclude any regions from rippling here

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
			if (Config->get_edit_mode() == Ripple) {
				playlist->ripple(time.start(), -time.length(), NULL);
			}
			// no need to exclude any regions from rippling here

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
RouteTimeAxisView::paste (samplepos_t pos, const Selection& selection, PasteContext& ctx, const int32_t sub_num)
{
	if (!is_track()) {
		return false;
	}

	boost::shared_ptr<Playlist>       pl   = playlist ();
	const ARDOUR::DataType            type = pl->data_type();
	PlaylistSelection::const_iterator p    = selection.playlists.get_nth(type, ctx.counts.n_playlists(type));

	if (p == selection.playlists.end()) {
		return false;
	}
	ctx.counts.increase_n_playlists(type);

	DEBUG_TRACE (DEBUG::CutNPaste, string_compose ("paste to %1\n", pos));

	/* add multi-paste offset if applicable */
	std::pair<samplepos_t, samplepos_t> extent  = (*p)->get_extent();
	const samplecnt_t                  duration = extent.second - extent.first;
	pos += _editor.get_paste_offset(pos, ctx.count, duration);

	pl->clear_changes ();
	pl->clear_owned_changes ();
	if (Config->get_edit_mode() == Ripple) {
		std::pair<samplepos_t, samplepos_t> extent = (*p)->get_extent_with_endspace();
		samplecnt_t amount = extent.second - extent.first;
		pl->ripple(pos, amount * ctx.times, boost::shared_ptr<Region>());
	}
	pl->paste (*p, pos, ctx.times, sub_num);

	vector<Command*> cmds;
	pl->rdiff (cmds);
	_session->add_commands (cmds);

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

	vector<boost::shared_ptr<Playlist> > playlists_tr = _session->playlists()->playlists_for_track (tr);

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

	if (!route_group() || !route_group()->is_active() || !route_group()->enabled_property (ARDOUR::Properties::group_select.property_id)) {
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

	playlist_items.push_back (MenuElem(_("Select from All..."), sigc::mem_fun(*this, &RouteTimeAxisView::show_playlist_selector)));
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

	track()->use_playlist (track()->data_type(), pl);

	RouteGroup* rg = route_group();

	if (rg && rg->is_active() && rg->enabled_property (ARDOUR::Properties::group_select.property_id)) {
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

			boost::shared_ptr<Playlist> ipl = session()->playlists()->by_name(playlist_name);
			if (!ipl) {
				// No playlist for this track for this take yet, make it
				track->use_default_new_playlist();
				track->playlist()->set_name(playlist_name);
			} else {
				track->use_playlist(track->data_type(), ipl);
			}
		}
	}
}

void
RouteTimeAxisView::update_playlist_tip ()
{
	RouteGroup* rg = route_group ();
	if (rg && rg->is_active() && rg->enabled_property (ARDOUR::Properties::group_select.property_id)) {
		string group_string = "." + rg->name() + ".";

		string take_name = track()->playlist()->name();
		string::size_type idx = take_name.find(group_string);

		if (idx != string::npos) {
			/* find the bit containing the take number / name */
			take_name = take_name.substr (idx + group_string.length());

			/* set the playlist button tooltip to the take name */
			set_tooltip (
				playlist_button,
				string_compose(_("Take: %1.%2"),
					Gtkmm2ext::markup_escape_text (rg->name()),
					Gtkmm2ext::markup_escape_text (take_name))
				);

			return;
		}
	}

	/* set the playlist button tooltip to the playlist name */
	set_tooltip (playlist_button, _("Playlist") + std::string(": ") + Gtkmm2ext::markup_escape_text (track()->playlist()->name()));
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
		break;
	default:
		playlist_button.set_sensitive (true);
		break;
	}
	RouteUI::map_frozen ();
}

void
RouteTimeAxisView::color_handler ()
{
	//case cTimeStretchOutline:
	if (timestretch_rect) {
		timestretch_rect->set_outline_color (UIConfiguration::instance().color ("time stretch outline"));
	}
	//case cTimeStretchFill:
	if (timestretch_rect) {
		timestretch_rect->set_fill_color (UIConfiguration::instance().color ("time stretch fill"));
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
RouteTimeAxisView::update_pan_track_visibility ()
{
	bool const showit = pan_automation_item->get_active();
	bool changed = false;

	for (list<boost::shared_ptr<AutomationTimeAxisView> >::iterator i = pan_tracks.begin(); i != pan_tracks.end(); ++i) {
		if ((*i)->set_marked_for_display (showit)) {
			changed = true;
		}
	}

	if (changed) {
		_route->gui_changed (X_("visible_tracks"), (void *) 0); /* EMIT_SIGNAL */
	}
}

void
RouteTimeAxisView::ensure_pan_views (bool show)
{
	bool changed = false;
	for (list<boost::shared_ptr<AutomationTimeAxisView> >::iterator i = pan_tracks.begin(); i != pan_tracks.end(); ++i) {
		changed = true;
		(*i)->set_marked_for_display (false);
	}
	if (changed) {
		_route->gui_changed (X_("visible_tracks"), (void *) 0); /* EMIT_SIGNAL */
	}
	pan_tracks.clear();

	if (!_route->panner()) {
		return;
	}

	set<Evoral::Parameter> params = _route->panner()->what_can_be_automated();
	set<Evoral::Parameter>::iterator p;

	for (p = params.begin(); p != params.end(); ++p) {
		boost::shared_ptr<ARDOUR::AutomationControl> pan_control = _route->pannable()->automation_control(*p);

		if (pan_control->parameter().type() == NullAutomation) {
			error << "Pan control has NULL automation type!" << endmsg;
			continue;
		}

		if (automation_child (pan_control->parameter ()).get () == 0) {

			/* we don't already have an AutomationTimeAxisView for this parameter */

			std::string const name = _route->panner()->describe_parameter (pan_control->parameter ());

			boost::shared_ptr<AutomationTimeAxisView> t (
					new AutomationTimeAxisView (_session,
						_route,
						_route->pannable(),
						pan_control,
						pan_control->parameter (),
						_editor,
						*this,
						false,
						parent_canvas,
						name)
					);

			pan_tracks.push_back (t);
			add_automation_child (*p, t, show);
		} else {
			pan_tracks.push_back (automation_child (pan_control->parameter ()));
		}
	}
}


void
RouteTimeAxisView::show_all_automation (bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_route_time_axis (boost::bind (&RouteTimeAxisView::show_all_automation, _1, false));
	} else {
		no_redraw = true;

		StripableTimeAxisView::show_all_automation ();

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

		StripableTimeAxisView::show_existing_automation ();

		/* Show processor automation */
		for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {
			for (vector<ProcessorAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
				if ((*i)->processor->control((*ii)->what)->list()->size() > 0) {
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
		StripableTimeAxisView::hide_all_automation ();

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
		abort(); /*NOTREACHED*/
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

	/* Also add explicitly visible */
	const std::set<Evoral::Parameter>& automatable = processor->what_can_be_automated ();
	for (std::set<Evoral::Parameter>::const_iterator i = automatable.begin(); i != automatable.end(); ++i) {
		boost::shared_ptr<AutomationControl> control = boost::dynamic_pointer_cast<AutomationControl>(processor->control(*i, false));
		if (!control) {
			continue;
		}
		/* see also AutomationTimeAxisView::state_id() */
		std::string ctrl_state_id = std::string("automation ") + control->id().to_s();
		bool visible;
		if (get_gui_property (ctrl_state_id, "visible", visible) && visible) {
			existing.insert (*i);
		}
	}

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

	for (std::set<Evoral::Parameter>::const_iterator i = automatable.begin(); i != automatable.end(); ++i) {

		ProcessorAutomationNode* pan;
		Gtk::CheckMenuItem* mitem;

		string name = processor->describe_parameter (*i);

		if (name == X_("hidden")) {
			continue;
		}

		items.push_back (CheckMenuElem (name));
		mitem = dynamic_cast<Gtk::CheckMenuItem*> (&items.back());

		_subplugin_menu_map[*i] = mitem;

		if ((pan = find_processor_automation_node (processor, *i)) == 0) {

			/* new item */

			pan = new ProcessorAutomationNode (*i, mitem, *this);

			rai->lines.push_back (pan);

		} else {

			pan->menu_item = mitem;

		}

		boost::shared_ptr<AutomationTimeAxisView> atav = automation_child (*i);
		bool visible;
		if (atav && atav->get_gui_property ("visible", visible)) {
			mitem->set_active(true);
		}

		mitem->signal_toggled().connect (sigc::bind (sigc::mem_fun(*this, &RouteTimeAxisView::processor_menu_item_toggled), rai, pan));
	}

	if (items.size() == 0) {
		return;
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
RouteTimeAxisView::reread_midnam ()
{
	boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (_route->the_instrument ());
	assert (pi);
	bool rv = pi->plugin ()->read_midnam();

	if (rv && patch_change_dialog ()) {
		patch_change_dialog ()->refresh ();
	}
}

void
RouteTimeAxisView::drop_instrument_ref ()
{
	midnam_connection.drop_connections ();
}

void
RouteTimeAxisView::processors_changed (RouteProcessorChange c)
{
	if (_route) {
		boost::shared_ptr<Processor> the_instrument (_route->the_instrument());
		boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (the_instrument);
		if (pi && pi->plugin ()->has_midnam ()) {
			midnam_connection.drop_connections ();
			the_instrument->DropReferences.connect (midnam_connection, invalidator (*this),
					boost::bind (&RouteTimeAxisView::drop_instrument_ref, this),
					gui_context());
			pi->plugin()->UpdateMidnam.connect (midnam_connection, invalidator (*this),
					boost::bind (&RouteTimeAxisView::reread_midnam, this),
					gui_context());

			reread_midnam ();
		}
	}

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
			return pan->view->line();
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
	boost::shared_ptr<Track> trk (boost::dynamic_pointer_cast<Track> (_route));
	if (!trk) {
		return true;
	}
	return !trk->rec_enable_control()->get_value();
}

void
RouteTimeAxisView::blink_rec_display (bool onoff)
{
	RouteUI::blink_rec_display (onoff);
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

		set_gui_property (X_("layer-display"), d);
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
	if (UIConfiguration::instance().get_show_track_meters()) {
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
	if (_route && !no_redraw && UIConfiguration::instance().get_show_track_meters()) {
		request_redraw ();
	}
	// reset peak when meter point changes
	gm.reset_peak_display();
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

		XMLProperty const * prop = child_node->property ("id");
		if (prop) {
			PBD::ID id (prop->value());

			StripableTimeAxisView* v = _editor.get_stripable_time_axis_by_id (id);

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
			abort(); /*NOTREACHED*/
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
			XMLProperty const * prop = node->add_property("id");
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
			abort(); /*NOTREACHED*/
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
	if (_route && _route->solo_safe_control()->solo_safe()) {
		solo_button->set_visual_state (Gtkmm2ext::VisualState (solo_button->visual_state() | Gtkmm2ext::Insensitive));
	} else {
		solo_button->set_visual_state (Gtkmm2ext::VisualState (solo_button->visual_state() & ~Gtkmm2ext::Insensitive));
	}
	if (Config->get_solo_control_is_listen_control()) {
		switch (Config->get_listen_position()) {
			case AfterFaderListen:
				solo_button->set_text (S_("AfterFader|A"));
				set_tooltip (*solo_button, _("After-fade listen (AFL)"));
				break;
			case PreFaderListen:
				solo_button->set_text (S_("PreFader|P"));
				set_tooltip (*solo_button, _("Pre-fade listen (PFL)"));
			break;
		}
	} else {
		solo_button->set_text (S_("Solo|S"));
		set_tooltip (*solo_button, _("Solo"));
	}
	mute_button->set_text (S_("Mute|M"));
}

Gtk::CheckMenuItem*
RouteTimeAxisView::automation_child_menu_item (Evoral::Parameter param)
{
	Gtk::CheckMenuItem* rv = StripableTimeAxisView::automation_child_menu_item (param);
	if (rv) {
		return rv;
	}

	ParameterMenuMap::iterator i = _subplugin_menu_map.find (param);
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

void
RouteTimeAxisView::create_trim_automation_child (const Evoral::Parameter& param, bool show)
{
	boost::shared_ptr<AutomationControl> c = _route->trim()->gain_control();
	if (!c || ! _route->trim()->active()) {
		return;
	}

	trim_track.reset (new AutomationTimeAxisView (_session,
						      _route, _route->trim(), c, param,
						      _editor,
						      *this,
						      false,
						      parent_canvas,
						      _route->trim()->describe_parameter(param)));

	if (_view) {
		_view->foreach_regionview (sigc::mem_fun (*trim_track.get(), &TimeAxisView::add_ghost));
	}

	add_automation_child (Evoral::Parameter(TrimAutomation), trim_track, show);
}

void
RouteTimeAxisView::create_mute_automation_child (const Evoral::Parameter& param, bool show)
{
	boost::shared_ptr<AutomationControl> c = _route->mute_control();
	if (!c) {
		error << "Route has no mute automation, unable to add automation track view." << endmsg;
		return;
	}

	mute_track.reset (new AutomationTimeAxisView (_session,
						      _route, _route, c, param,
						      _editor,
						      *this,
						      false,
						      parent_canvas,
						      _route->describe_parameter(param)));

	if (_view) {
		_view->foreach_regionview (sigc::mem_fun (*mute_track.get(), &TimeAxisView::add_ghost));
	}

	add_automation_child (Evoral::Parameter(MuteAutomation), mute_track, show);
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

Gdk::Color
RouteTimeAxisView::color () const
{
	return route_color ();
}

bool
RouteTimeAxisView::marked_for_display () const
{
	return !_route->presentation_info().hidden();
}

bool
RouteTimeAxisView::set_marked_for_display (bool yn)
{
	return RouteUI::mark_hidden (!yn);
}
