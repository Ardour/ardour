/*
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2013-2015 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2016-2018 Len Ovens <len@ovenwerks.net>
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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <algorithm>
#include <map>
#include <sigc++/bind.h>

#include <boost/foreach.hpp>

#include <glibmm/threads.h>

#include <gtkmm/accelmap.h>
#include <gtkmm/offscreenwindow.h>
#include <gtkmm/stock.h>

#include "pbd/convert.h"
#include "pbd/unwind.h"

#include "ardour/amp.h"
#include "ardour/debug.h"
#include "ardour/audio_port.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"
#include "ardour/monitor_control.h"
#include "ardour/panner_shell.h"
#include "ardour/plugin_manager.h"
#include "ardour/route_group.h"
#include "ardour/selection.h"
#include "ardour/session.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/window_title.h"
#include "gtkmm2ext/doi.h"

#include "widgets/tearoff.h"

#include "foldback_strip.h"
#include "keyboard.h"
#include "mixer_ui.h"
#include "mixer_strip.h"
#include "monitor_section.h"
#include "plugin_selector.h"
#include "public_editor.h"
#include "mouse_cursors.h"
#include "ardour_ui.h"
#include "utils.h"
#include "route_sorter.h"
#include "actions.h"
#include "gui_thread.h"
#include "mixer_group_tabs.h"
#include "plugin_utils.h"
#include "route_sorter.h"
#include "timers.h"
#include "ui_config.h"
#include "vca_master_strip.h"

#include "pbd/i18n.h"

#define PX_SCALE(px) std::max ((float)px, rintf ((float)px* UIConfiguration::instance ().get_ui_scale ()))

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace ARDOUR_PLUGIN_UTILS;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace std;

using PBD::atoi;
using PBD::Unwinder;

Mixer_UI* Mixer_UI::_instance = 0;

Mixer_UI*
Mixer_UI::instance ()
{
	if (!_instance) {
		_instance  = new Mixer_UI;
	}

	return _instance;
}

Mixer_UI::Mixer_UI ()
	: Tabbable (_content, _("Mixer"), X_("mixer"))
	, plugin_search_clear_button (Stock::CLEAR)
	, no_track_list_redisplay (false)
	, in_group_row_change (false)
	, track_menu (0)
	, _plugin_selector (0)
	, foldback_strip (0)
	, _show_foldback_strip (true)
	, _strip_width (UIConfiguration::instance().get_default_narrow_ms() ? Narrow : Wide)
	, _spill_scroll_position (0)
	, ignore_track_reorder (false)
	, ignore_plugin_refill (false)
	, ignore_plugin_reorder (false)
	, _in_group_rebuild_or_clear (false)
	, _route_deletion_in_progress (false)
	, _maximised (false)
	, _strip_selection_change_without_scroll (false)
	, _selection (*this, *this)
{
	load_bindings ();
	register_actions ();
	Glib::RefPtr<ToggleAction> fb_act = ActionManager::get_toggle_action ("Mixer", "ToggleFoldbackStrip");
	fb_act->set_sensitive (false);

	_content.set_data ("ardour-bindings", bindings);

	PresentationInfo::Change.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::presentation_info_changed, this, _1), gui_context());
	Route::FanOut.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::fan_out, this, _1, false, true), gui_context());

	scroller.set_can_default (true);
	// set_default (scroller);

	scroller_base.set_flags (Gtk::CAN_FOCUS);
	scroller_base.add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	scroller_base.set_name ("MixerWindow");
	scroller_base.signal_button_press_event().connect (sigc::mem_fun(*this, &Mixer_UI::strip_scroller_button_event));
	scroller_base.signal_button_release_event().connect (sigc::mem_fun(*this, &Mixer_UI::strip_scroller_button_event));

	/* set up drag-n-drop */
	vector<TargetEntry> target_table;
	target_table.push_back (TargetEntry ("x-ardour/plugin.favorite", Gtk::TARGET_SAME_APP));
	scroller_base.drag_dest_set (target_table);
	scroller_base.signal_drag_data_received().connect (sigc::mem_fun(*this, &Mixer_UI::scroller_drag_data_received));

	/* add as last item of strip packer */
	strip_packer.pack_end (scroller_base, true, true);
	scroller_base.set_size_request (PX_SCALE (20), -1);
	scroller_base.signal_expose_event ().connect (sigc::bind (sigc::ptr_fun (&ArdourWidgets::ArdourIcon::expose_with_text), &scroller_base, ArdourWidgets::ArdourIcon::ShadedPlusSign,
			_("Right-click or Double-click here\nto add Track, Bus, or VCA channels")));

#ifdef MIXBUS
	/* create a drop-shadow at the end of the mixer strips */
	mb_shadow.set_size_request( 4, -1 );
	mb_shadow.set_name("EditorWindow");
	mb_shadow.show();
	strip_packer.pack_end (mb_shadow, false, false);
#endif

	_group_tabs = new MixerGroupTabs (this);
	strip_group_box.set_spacing (0);
	strip_group_box.set_border_width (0);
	strip_group_box.pack_start (*_group_tabs, PACK_SHRINK);
	strip_group_box.pack_start (strip_packer);
	strip_group_box.show_all ();
	strip_group_box.signal_scroll_event().connect (sigc::mem_fun (*this, &Mixer_UI::on_scroll_event), false);

	scroller.add (strip_group_box);
	scroller.set_policy (Gtk::POLICY_ALWAYS, Gtk::POLICY_AUTOMATIC);

	setup_track_display ();

	group_model = ListStore::create (group_columns);
	group_display.set_model (group_model);
	group_display.append_column (_("Show"), group_columns.visible);
	group_display.append_column (_("Group"), group_columns.text);
	group_display.get_column (0)->set_data (X_("colnum"), GUINT_TO_POINTER(0));
	group_display.get_column (1)->set_data (X_("colnum"), GUINT_TO_POINTER(1));
	group_display.get_column (0)->set_expand(false);
	group_display.get_column (1)->set_expand(true);
	group_display.get_column (1)->set_sizing (Gtk::TREE_VIEW_COLUMN_FIXED);
	group_display.set_name ("EditGroupList");
	group_display.get_selection()->set_mode (Gtk::SELECTION_SINGLE);
	group_display.set_reorderable (true);
	group_display.set_headers_visible (true);
	group_display.set_rules_hint (true);
	group_display.set_can_focus(false);

	/* name is directly editable */

	CellRendererText* name_cell = dynamic_cast<CellRendererText*>(group_display.get_column_cell_renderer (1));
	name_cell->property_editable() = true;
	name_cell->signal_edited().connect (sigc::mem_fun (*this, &Mixer_UI::route_group_name_edit));

	/* use checkbox for the active column */

	CellRendererToggle* active_cell = dynamic_cast<CellRendererToggle*>(group_display.get_column_cell_renderer (0));
	active_cell->property_activatable() = true;
	active_cell->property_radio() = false;

	group_model->signal_row_changed().connect (sigc::mem_fun (*this, &Mixer_UI::route_group_row_change));
	/* We use this to notice drag-and-drop reorders of the group list */
	group_model->signal_row_deleted().connect (sigc::mem_fun (*this, &Mixer_UI::route_group_row_deleted));
	group_display.signal_button_press_event().connect (sigc::mem_fun (*this, &Mixer_UI::group_display_button_press), false);

	group_display_scroller.add (group_display);
	group_display_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);


	group_display_vbox.pack_start (group_display_scroller, true, true);

	group_display_frame.set_name ("BaseFrame");
	group_display_frame.set_shadow_type (Gtk::SHADOW_IN);
	group_display_frame.add (group_display_vbox);

	list<TargetEntry> target_list;
	target_list.push_back (TargetEntry ("x-ardour/plugin.preset", Gtk::TARGET_SAME_APP));

	favorite_plugins_model = PluginTreeStore::create (favorite_plugins_columns);
	favorite_plugins_display.set_model (favorite_plugins_model);
	favorite_plugins_display.append_column (_("Favorite Plugins"), favorite_plugins_columns.name);
	favorite_plugins_display.set_name ("EditGroupList");
	favorite_plugins_display.get_selection()->set_mode (Gtk::SELECTION_SINGLE);
	favorite_plugins_display.set_reorderable (false);
	favorite_plugins_display.set_headers_visible (false);
	favorite_plugins_display.set_rules_hint (true);
	favorite_plugins_display.set_can_focus (false);
	favorite_plugins_display.add_object_drag (favorite_plugins_columns.plugin.index(), "x-ardour/plugin.favorite", Gtk::TARGET_SAME_APP);
	favorite_plugins_display.set_drag_column (favorite_plugins_columns.name.index());
	favorite_plugins_display.add_drop_targets (target_list);
	favorite_plugins_display.signal_row_activated().connect (sigc::mem_fun (*this, &Mixer_UI::plugin_row_activated));
	favorite_plugins_display.signal_button_press_event().connect (sigc::mem_fun (*this, &Mixer_UI::plugin_row_button_press), false);
	favorite_plugins_display.signal_drop.connect (sigc::mem_fun (*this, &Mixer_UI::plugin_drop));
	favorite_plugins_display.signal_motion.connect (sigc::mem_fun (*this, &Mixer_UI::plugin_drag_motion));
	favorite_plugins_display.signal_row_expanded().connect (sigc::mem_fun (*this, &Mixer_UI::save_favorite_ui_state));
	favorite_plugins_display.signal_row_collapsed().connect (sigc::mem_fun (*this, &Mixer_UI::save_favorite_ui_state));
	if (UIConfiguration::instance().get_use_tooltips()) {
		favorite_plugins_display.set_tooltip_column (0);
	}
	favorite_plugins_model->signal_row_has_child_toggled().connect (sigc::mem_fun (*this, &Mixer_UI::sync_treeview_favorite_ui_state));
	favorite_plugins_model->signal_row_deleted().connect (sigc::mem_fun (*this, &Mixer_UI::favorite_plugins_deleted));

	favorite_plugins_mode_combo.append_text (_("Favorite Plugins"));
	favorite_plugins_mode_combo.append_text (_("Recent Plugins"));
	favorite_plugins_mode_combo.append_text (_("Top-10 Plugins"));
	favorite_plugins_mode_combo.set_active_text (_("Favorite Plugins"));
	favorite_plugins_mode_combo.signal_changed().connect (sigc::mem_fun (*this, &Mixer_UI::plugin_list_mode_changed));

	plugin_search_entry.signal_changed().connect (sigc::mem_fun (*this, &Mixer_UI::plugin_search_entry_changed));
	plugin_search_clear_button.signal_clicked().connect (sigc::mem_fun (*this, &Mixer_UI::plugin_search_clear_button_clicked));

	favorite_plugins_scroller.add (favorite_plugins_display);
	favorite_plugins_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	favorite_plugins_search_hbox.pack_start (plugin_search_entry, true, true);
	favorite_plugins_search_hbox.pack_start (plugin_search_clear_button, false, false);

	favorite_plugins_frame.set_name ("BaseFrame");
	favorite_plugins_frame.set_shadow_type (Gtk::SHADOW_IN);
	favorite_plugins_frame.add (favorite_plugins_vbox);

	favorite_plugins_vbox.pack_start (favorite_plugins_mode_combo, false, false);
	favorite_plugins_vbox.pack_start (favorite_plugins_scroller, true, true);
	favorite_plugins_vbox.pack_start (favorite_plugins_search_hbox, false, false);

	rhs_pane1.add (favorite_plugins_frame);
	rhs_pane1.add (track_display_frame);

	rhs_pane2.add (rhs_pane1);
	rhs_pane2.add (group_display_frame);

	list_vpacker.pack_start (rhs_pane2, true, true);

	vca_label_bar.set_size_request (-1, 16 + 1); /* must match height in GroupTabs::set_size_request()  + 1 border px*/
	vca_vpacker.pack_start (vca_label_bar, false, false);

	vca_scroller_base.add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	vca_scroller_base.set_name (X_("MixerWindow"));
	vca_scroller_base.signal_button_release_event().connect (sigc::mem_fun(*this, &Mixer_UI::masters_scroller_button_release), false);

	vca_hpacker.signal_scroll_event().connect (sigc::mem_fun (*this, &Mixer_UI::on_vca_scroll_event), false);
	vca_scroller.add (vca_hpacker);
	vca_scroller.set_policy (Gtk::POLICY_ALWAYS, Gtk::POLICY_AUTOMATIC);
	vca_scroller.signal_button_press_event().connect (sigc::mem_fun(*this, &Mixer_UI::strip_scroller_button_event));
	vca_scroller.signal_button_release_event().connect (sigc::mem_fun(*this, &Mixer_UI::strip_scroller_button_event));

	vca_vpacker.pack_start (vca_scroller, true, true);

	inner_pane.add (scroller);
	inner_pane.add (vca_vpacker);

	global_hpacker.pack_start (inner_pane, true, true);
	global_hpacker.pack_start (out_packer, false, false);

	list_hpane.set_check_divider_position (true);
	list_hpane.add (list_vpacker);
	list_hpane.add (global_hpacker);
	list_hpane.set_child_minsize (list_vpacker, 30);

	XMLNode const * settings = ARDOUR_UI::instance()->mixer_settings();
	float fract;

	if (!settings || !settings->get_property ("mixer-rhs-pane1-pos", fract) || fract > 1.0) {
		fract = 0.6f;
	}
	rhs_pane1.set_divider (0, fract);

	if (!settings || !settings->get_property ("mixer-rhs-pane2-pos", fract) || fract > 1.0) {
		fract = 0.7f;
	}
	rhs_pane2.set_divider (0, fract);

	if (!settings || !settings->get_property ("mixer-list-hpane-pos", fract) || fract > 1.0) {
		fract = 0.2f;
	}
	list_hpane.set_divider (0, fract);

	if (!settings || !settings->get_property ("mixer-inner-pane-pos", fract) || fract > 1.0) {
		fract = 0.8f;
	}
	inner_pane.set_divider (0, fract);

	rhs_pane1.set_drag_cursor (*PublicEditor::instance().cursors()->expand_up_down);
	rhs_pane2.set_drag_cursor (*PublicEditor::instance().cursors()->expand_up_down);
	list_hpane.set_drag_cursor (*PublicEditor::instance().cursors()->expand_left_right);
	inner_pane.set_drag_cursor (*PublicEditor::instance().cursors()->expand_left_right);

	_content.pack_start (list_hpane, true, true);

	update_title ();

	_content.show ();
	_content.set_name ("MixerWindow");

	global_hpacker.show();
	scroller.show();
	scroller_base.show();
	scroller_hpacker.show();
	mixer_scroller_vpacker.show();
	list_vpacker.show();
	group_display_button_label.show();
	group_display_scroller.show();
	favorite_plugins_scroller.show();
	group_display_vbox.show();
	group_display_frame.show();
	favorite_plugins_frame.show();
	rhs_pane1.show();
	rhs_pane2.show();
	strip_packer.show();
	inner_pane.show();
	vca_scroller.show();
	vca_vpacker.show();
	vca_hpacker.show();
	vca_label_bar.show();
	vca_label.show();
	vca_scroller_base.show();
	out_packer.show();
	list_hpane.show();
	group_display.show();
	favorite_plugins_display.show();

	XMLNode* mnode = ARDOUR_UI::instance()->tearoff_settings (X_("monitor-section"));
	if (mnode) {
		_monitor_section.tearoff().set_state (*mnode);
	}

	MixerStrip::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::remove_strip, this, _1), gui_context());
	VCAMasterStrip::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::remove_master, this, _1), gui_context());
	FoldbackStrip::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::remove_foldback, this, _1), gui_context());

	/* handle escape */

	ARDOUR_UI::instance()->Escape.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::escape, this), gui_context());

#ifndef DEFER_PLUGIN_SELECTOR_LOAD
	_plugin_selector = new PluginSelector (PluginManager::instance ());
#else
#error implement deferred Plugin-Favorite list
#endif

	PluginManager::instance ().PluginListChanged.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::refill_favorite_plugins, this), gui_context());
	ARDOUR::Plugin::PresetsChanged.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::refill_favorite_plugins, this), gui_context());

	PluginManager::instance ().PluginStatusChanged.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::maybe_refill_favorite_plugins, this, PLM_Favorite), gui_context());
	PluginManager::instance ().PluginStatsChanged.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::maybe_refill_favorite_plugins, this, PLM_Recent), gui_context());
}

Mixer_UI::~Mixer_UI ()
{
	monitor_section_detached ();

	delete foldback_strip;
	foldback_strip = 0;
	delete _plugin_selector;
	delete track_menu;
	delete _group_tabs;
}

struct MixerStripSorter {
	bool operator() (const MixerStrip* ms_a, const MixerStrip* ms_b)
	{
		boost::shared_ptr<ARDOUR::Stripable> const& a = ms_a->stripable ();
		boost::shared_ptr<ARDOUR::Stripable> const& b = ms_b->stripable ();
		return ARDOUR::Stripable::Sorter(true)(a, b);
	}
};


void
Mixer_UI::escape ()
{
	select_none ();
}

Gtk::Window*
Mixer_UI::use_own_window (bool and_fill_it)
{
	bool new_window = !own_window();

	Gtk::Window* win = Tabbable::use_own_window (and_fill_it);

	if (win && new_window) {
		win->set_name ("MixerWindow");
		ARDOUR_UI::instance()->setup_toplevel_window (*win, _("Mixer"), this);
		win->signal_event().connect (sigc::bind (sigc::ptr_fun (&Keyboard::catch_user_event_for_pre_dialog_focus), win));
		win->set_data ("ardour-bindings", bindings);
		update_title ();
		if (!win->get_focus()) {
			/* set focus widget to something, anything */
			win->set_focus (scroller);
		}
	}

	return win;
}

void
Mixer_UI::show_window ()
{
	Tabbable::show_window ();

	/* show/hide group tabs as required */
	parameter_changed ("show-group-tabs");

	/* now reset each strips width so the right widgets are shown */

	TreeModel::Children rows = track_model->children();
	TreeModel::Children::iterator ri;

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		AxisView* av = (*ri)[stripable_columns.strip];
		MixerStrip* ms = dynamic_cast<MixerStrip*> (av);
		if (!ms) {
			continue;
		}
		ms->set_width_enum (ms->get_width_enum (), ms->width_owner());
		/* Fix visibility of mixer strip stuff */
		ms->parameter_changed (X_("mixer-element-visibility"));
	}

	/* force focus into main area */
	scroller_base.grab_focus ();
}

void
Mixer_UI::remove_master (VCAMasterStrip* vms)
{
	if (_session && _session->deletion_in_progress()) {
		/* its all being taken care of */
		return;
	}

	TreeModel::Children rows = track_model->children();
	TreeModel::Children::iterator ri;

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		if ((*ri)[stripable_columns.strip] == vms) {
			PBD::Unwinder<bool> uw (_route_deletion_in_progress, true);
			track_model->erase (ri);
			break;
		}
	}
}

bool
Mixer_UI::masters_scroller_button_release (GdkEventButton* ev)
{
	using namespace Menu_Helpers;

	if (Keyboard::is_context_menu_event (ev)) {
		ARDOUR_UI::instance()->add_route ();
		return true;
	}

	return false;
}

void
Mixer_UI::new_masters_created ()
{
	ActionManager::get_toggle_action ("Mixer", "ToggleVCAPane")->set_active (true);
}

void
Mixer_UI::add_masters (VCAList& vlist)
{
	StripableList sl;

	for (VCAList::iterator v = vlist.begin(); v != vlist.end(); ++v) {
		sl.push_back (boost::dynamic_pointer_cast<Stripable> (*v));
	}

	add_stripables (sl);
}

void
Mixer_UI::add_routes (RouteList& rlist)
{
	StripableList sl;

	for (RouteList::iterator r = rlist.begin(); r != rlist.end(); ++r) {
		sl.push_back (*r);
	}

	add_stripables (sl);
}

void
Mixer_UI::add_stripables (StripableList& slist)
{
	Gtk::TreeModel::Children::iterator insert_iter = track_model->children().end();
	bool from_scratch = (track_model->children().size() == 0);
	uint32_t nroutes = 0;

	slist.sort (Stripable::Sorter());

	for (Gtk::TreeModel::Children::iterator it = track_model->children().begin(); it != track_model->children().end(); ++it) {
		boost::shared_ptr<Stripable> s = (*it)[stripable_columns.stripable];

		if (!s) {
			continue;
		}

		nroutes++;

		// XXX what does this special case do?
		if (s->presentation_info().order() == (slist.front()->presentation_info().order() + slist.size())) {
			insert_iter = it;
			break;
		}
	}

	MixerStrip* strip;

	try {
		PBD::Unwinder<bool> uw (no_track_list_redisplay, true);

		track_display.set_model (Glib::RefPtr<ListStore>());

		for (StripableList::iterator s = slist.begin(); s != slist.end(); ++s) {

			boost::shared_ptr<Route> route;
			boost::shared_ptr<VCA> vca;

			if ((vca  = boost::dynamic_pointer_cast<VCA> (*s))) {

				VCAMasterStrip* vms = new VCAMasterStrip (_session, vca);

				TreeModel::Row row = *(track_model->append());

				row[stripable_columns.text] = vca->name();
				row[stripable_columns.visible] = vms->marked_for_display ();
				row[stripable_columns.strip] = vms;
				row[stripable_columns.stripable] = vca;

				vms->signal_button_release_event().connect (sigc::bind (sigc::mem_fun(*this, &Mixer_UI::vca_button_release_event), vms));

			} else if ((route = boost::dynamic_pointer_cast<Route> (*s))) {

				if (route->is_auditioner()) {
					continue;
				}

				if (route->is_monitor()) {

					out_packer.pack_end (_monitor_section.tearoff(), false, false);
					_monitor_section.set_session (_session);
					_monitor_section.tearoff().show_all ();

					_monitor_section.tearoff().Detach.connect (sigc::mem_fun(*this, &Mixer_UI::monitor_section_detached));
					_monitor_section.tearoff().Attach.connect (sigc::mem_fun(*this, &Mixer_UI::monitor_section_attached));

					if (_monitor_section.tearoff().torn_off()) {
						monitor_section_detached ();
					} else {
						monitor_section_attached ();
					}

					route->DropReferences.connect (*this, invalidator(*this), boost::bind (&Mixer_UI::monitor_section_going_away, this), gui_context());

					/* no regular strip shown for control out */

					continue;
				}
				if (route->is_foldbackbus ()) {
					if (foldback_strip) {
						// last strip created is shown
						foldback_strip->set_route (route);
					} else {
						foldback_strip = new FoldbackStrip (*this, _session, route);
						out_packer.pack_start (*foldback_strip, false, false);
						// change 0 to 1 below for foldback to right of master
						out_packer.reorder_child (*foldback_strip, 0);
					}
					/* config from last run is set before there are any foldback strips
					 * this takes that setting and applies it after at least one foldback
					 * strip exists */
					bool yn = _show_foldback_strip;
					Glib::RefPtr<ToggleAction> act = ActionManager::get_toggle_action ("Mixer", "ToggleFoldbackStrip");
					act->set_sensitive (true);
					act->set_active(!yn);
					act->set_active(yn);
					continue;
				}

				strip = new MixerStrip (*this, _session, route);
				strips.push_back (strip);

				UIConfiguration::instance().get_default_narrow_ms() ? _strip_width = Narrow : _strip_width = Wide;

				if (strip->width_owner() != strip) {
					strip->set_width_enum (_strip_width, this);
				}

				show_strip (strip);

				if (route->is_master()) {

					out_packer.pack_start (*strip, false, false);
					strip->set_packed (true);

				} else {

					TreeModel::Row row = *(track_model->insert (insert_iter));

					row[stripable_columns.text] = route->name();
					row[stripable_columns.visible] = strip->marked_for_display();
					row[stripable_columns.stripable] = route;
					row[stripable_columns.strip] = strip;
				}

				strip->WidthChanged.connect (sigc::mem_fun(*this, &Mixer_UI::strip_width_changed));
				strip->signal_button_release_event().connect (sigc::bind (sigc::mem_fun(*this, &Mixer_UI::strip_button_release_event), strip));
			}

			(*s)->presentation_info().PropertyChanged.connect (*this, invalidator(*this), boost::bind (&Mixer_UI::stripable_property_changed, this, _1, boost::weak_ptr<Stripable>(*s)), gui_context());
			(*s)->PropertyChanged.connect (*this, invalidator(*this), boost::bind (&Mixer_UI::stripable_property_changed, this, _1, boost::weak_ptr<Stripable>(*s)), gui_context());
		}

	} catch (const std::exception& e) {
		error << string_compose (_("Error adding GUI elements for new tracks/busses %1"), e.what()) << endmsg;
	}

	track_display.set_model (track_model);

	/* catch up on selection state, which we left to the editor to set */
	sync_treeview_from_presentation_info (PropertyChange (Properties::selected));

	if (!from_scratch) {
		sync_presentation_info_from_treeview ();
	}

	redisplay_track_list ();
}

void
Mixer_UI::deselect_all_strip_processors ()
{
	for (list<MixerStrip *>::iterator i = strips.begin(); i != strips.end(); ++i) {
		(*i)->deselect_all_processors();
	}
	if (foldback_strip) {
		foldback_strip->deselect_all_processors ();
	}
}

void
Mixer_UI::select_none ()
{
	_selection.clear_routes();
	deselect_all_strip_processors();
}

void
Mixer_UI::select_next_strip ()
{
	deselect_all_strip_processors();
	_session->selection().select_next_stripable (true, false);
}

void
Mixer_UI::select_prev_strip ()
{
	deselect_all_strip_processors();
	_session->selection().select_prev_stripable (true, false);
}

void
Mixer_UI::delete_processors ()
{
	for (list<MixerStrip *>::iterator i = strips.begin(); i != strips.end(); ++i) {
		(*i)->delete_processors();
	}
}


void
Mixer_UI::remove_strip (MixerStrip* strip)
{
	if (_session && _session->deletion_in_progress()) {
		/* its all being taken care of */
		return;
	}

	TreeModel::Children rows = track_model->children();
	TreeModel::Children::iterator ri;
	list<MixerStrip *>::iterator i;

	if ((i = find (strips.begin(), strips.end(), strip)) != strips.end()) {
		strips.erase (i);
	}

	PBD::Unwinder<bool> uwi (ignore_track_reorder, true);

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		if ((*ri)[stripable_columns.strip] == strip) {
			PBD::Unwinder<bool> uw (_route_deletion_in_progress, true);
			track_model->erase (ri);
			break;
		}
	}
}

void
Mixer_UI::remove_foldback (FoldbackStrip* strip)
{
	if (_session && _session->deletion_in_progress()) {
		/* its all being taken care of */
		return;
	}
	assert (strip == foldback_strip);
	Glib::RefPtr<ToggleAction> act = ActionManager::get_toggle_action ("Mixer", "ToggleFoldbackStrip");
	act->set_sensitive (false);
	foldback_strip = 0;
}

void
Mixer_UI::presentation_info_changed (PropertyChange const & what_changed)
{
	if (what_changed.contains (Properties::selected)) {
		_selection.presentation_info_changed (what_changed);
	}

	PropertyChange soh;
	soh.add (Properties::selected);
	soh.add (Properties::order);
	soh.add (Properties::hidden);

	if (what_changed.contains (soh)) {
		sync_treeview_from_presentation_info (what_changed);
	}
}

void
Mixer_UI::sync_presentation_info_from_treeview ()
{
	if (ignore_track_reorder || !_session || _session->deletion_in_progress()) {
		return;
	}

	TreeModel::Children rows = track_model->children();

	if (rows.empty()) {
		return;
	}

	DEBUG_TRACE (DEBUG::OrderKeys, "mixer sync presentation info from treeview\n");

	TreeModel::Children::iterator ri;

	PresentationInfo::order_t master_key = _session->master_order_key ();
	PresentationInfo::order_t order = 0;

	PresentationInfo::ChangeSuspender cs;

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		bool visible = (*ri)[stripable_columns.visible];
		boost::shared_ptr<Stripable> stripable = (*ri)[stripable_columns.stripable];

#ifndef NDEBUG // these should not exist in the mixer's treeview
		if (!stripable) {
			assert (0);
			continue;
		}
		if (stripable->is_monitor() || stripable->is_auditioner()) {
			assert (0);
			continue;
		}
		if (stripable->is_master()) {
			assert (0);
			continue;
		}
#endif

		// leave master where it is.
		if (order == master_key) {
			++order;
		}

		stripable->presentation_info().set_hidden (!visible);
		stripable->set_presentation_order (order);
		++order;
	}

}

void
Mixer_UI::sync_treeview_from_presentation_info (PropertyChange const & what_changed)
{
	if (!_session || _session->deletion_in_progress()) {
		return;
	}

	DEBUG_TRACE (DEBUG::OrderKeys, "mixer sync model from presentation info.\n");

	/* we could get here after either a change in the Mixer or Editor sort
	 * order, but either way, the mixer order keys reflect the intended
	 * order for the GUI, so reorder the treeview model to match it.
	 */

	vector<int> neworder;
	TreeModel::Children rows = track_model->children();
	uint32_t old_order = 0;
	bool changed = false;

	if (rows.empty()) {
		return;
	}

	TreeOrderKeys sorted;
	for (TreeModel::Children::iterator ri = rows.begin(); ri != rows.end(); ++ri, ++old_order) {
		boost::shared_ptr<Stripable> stripable = (*ri)[stripable_columns.stripable];
		sorted.push_back (TreeOrderKey (old_order, stripable));
	}

	TreeOrderKeySorter cmp;

	sort (sorted.begin(), sorted.end(), cmp);
	neworder.assign (sorted.size(), 0);

	uint32_t n = 0;

	for (TreeOrderKeys::iterator sr = sorted.begin(); sr != sorted.end(); ++sr, ++n) {

		neworder[n] = sr->old_display_order;

		if (sr->old_display_order != n) {
			changed = true;
		}
	}

	if (changed) {
		Unwinder<bool> uw (ignore_track_reorder, true);
		track_model->reorder (neworder);
	}

	if (what_changed.contains (Properties::selected)) {

		PresentationInfo::ChangeSuspender cs;

		for (list<MixerStrip *>::const_iterator i = strips.begin(); i != strips.end(); ++i) {
			boost::shared_ptr<Stripable> stripable = (*i)->stripable();
			if (stripable && stripable->is_selected()) {
				_selection.add (*i);
			} else {
				_selection.remove (*i);
			}
		}

		if (!_selection.axes.empty() && !PublicEditor::instance().track_selection_change_without_scroll () && !_strip_selection_change_without_scroll) {
			move_stripable_into_view ((*_selection.axes.begin())->stripable());
		}

		TreeModel::Children rows = track_model->children();
		for (TreeModel::Children::const_iterator i = rows.begin(); i != rows.end(); ++i) {
			AxisView* av = (*i)[stripable_columns.strip];
			VCAMasterStrip* vms = dynamic_cast<VCAMasterStrip*> (av);
			if (!vms) {
				continue;
			}
			if (vms->vca() && vms->vca()->is_selected()) {
				_selection.add (vms);
			} else {
				_selection.remove (vms);
			}
		}
	}

	redisplay_track_list ();
}

void
Mixer_UI::fan_out (boost::weak_ptr<Route> wr, bool to_busses, bool group)
{
	boost::shared_ptr<ARDOUR::Route> route = wr.lock ();

	if (!ARDOUR_UI_UTILS::engine_is_running () || ! route) {
		return;
	}

	DisplaySuspender ds;
	boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (route->the_instrument ());
	assert (pi);

	const uint32_t n_outputs = pi->output_streams ().n_audio ();
	if (route->n_outputs ().n_audio () != n_outputs) {
		MessageDialog msg (string_compose (
					_("The Plugin's number of audio outputs ports (%1) does not match the Tracks's number of audio outputs (%2). Cannot fan out."),
					n_outputs, route->n_outputs ().n_audio ()));
		msg.run ();
		return;
	}

#define BUSNAME  pd.group_name + "(" + route->name () + ")"

	/* count busses and channels/bus */
	boost::shared_ptr<Plugin> plugin = pi->plugin ();
	std::map<std::string, uint32_t> busnames;
	for (uint32_t p = 0; p < n_outputs; ++p) {
		const Plugin::IOPortDescription& pd (plugin->describe_io_port (DataType::AUDIO, false, p));
		std::string bn = BUSNAME;
		busnames[bn]++;
	}

	if (busnames.size () < 2) {
		MessageDialog msg (_("Instrument has only 1 output bus. Nothing to fan out."));
		msg.run ();
		return;
	}

	uint32_t outputs = 2;
	if (_session->master_out ()) {
		outputs = std::max (outputs, _session->master_out ()->n_inputs ().n_audio ());
	}

	route->output ()->disconnect (this);
	route->panner_shell ()->set_bypassed (true);

	boost::shared_ptr<AutomationControl> msac = route->master_send_enable_controllable ();
	if (msac) {
		msac->start_touch (timepos_t (msac->session().transport_sample()));
		msac->set_value (0, PBD::Controllable::NoGroup);
	}

	RouteList to_group;
	for (uint32_t p = 0; p < n_outputs; ++p) {
		const Plugin::IOPortDescription& pd (plugin->describe_io_port (DataType::AUDIO, false, p));
		std::string bn = BUSNAME;
		boost::shared_ptr<Route> r = _session->route_by_name (bn);
		if (!r) {
			try {
				if (to_busses) {
					RouteList rl = _session->new_audio_route (busnames[bn], outputs, NULL, 1, bn, PresentationInfo::AudioBus, PresentationInfo::max_order);
					r = rl.front ();
					assert (r);
				} else {
					list<boost::shared_ptr<AudioTrack> > tl = _session->new_audio_track (busnames[bn], outputs, NULL, 1, bn, PresentationInfo::max_order, Normal, false);
					r = tl.front ();
					assert (r);

					boost::shared_ptr<ControlList> cl (new ControlList);
					cl->push_back (r->monitoring_control ());
					_session->set_controls (cl, (double) MonitorInput, Controllable::NoGroup);
				}
			} catch (...) {
				if (!to_group.empty()) {
					boost::shared_ptr<RouteList> rl (&to_group);
					_session->remove_routes (rl);
				}
				return;
			}
		}
		to_group.push_back (r);
		route->output ()->audio (p)->connect (r->input ()->audio (pd.group_channel).get());
	}
#undef BUSNAME

	if (group) {
		RouteGroup* rg = NULL;
		const std::list<RouteGroup*>& rgs (_session->route_groups ());
		for (std::list<RouteGroup*>::const_iterator i = rgs.begin (); i != rgs.end (); ++i) {
			if ((*i)->name () == pi->name ()) {
				rg = *i;
				break;
			}
		}
		if (!rg) {
			rg = new RouteGroup (*_session, pi->name ());
			_session->add_route_group (rg);
			rg->set_gain (false);
		}

		GroupTabs::set_group_color (rg, route->presentation_info().color());
		for (RouteList::const_iterator i = to_group.begin(); i != to_group.end(); ++i) {
			rg->add (*i);
		}
	}
}

MixerStrip*
Mixer_UI::strip_by_route (boost::shared_ptr<Route> r) const
{
	for (list<MixerStrip *>::const_iterator i = strips.begin(); i != strips.end(); ++i) {
		if ((*i)->route() == r) {
			return (*i);
		}
	}

	return 0;
}

MixerStrip*
Mixer_UI::strip_by_stripable (boost::shared_ptr<Stripable> s) const
{
	for (list<MixerStrip *>::const_iterator i = strips.begin(); i != strips.end(); ++i) {
		if ((*i)->stripable() == s) {
			return (*i);
		}
	}

	return 0;
}

AxisView*
Mixer_UI::axis_view_by_stripable (boost::shared_ptr<Stripable> s) const
{
	for (list<MixerStrip *>::const_iterator i = strips.begin(); i != strips.end(); ++i) {
		if ((*i)->stripable() == s) {
			return (*i);
		}
	}

	TreeModel::Children rows = track_model->children();
	for (TreeModel::Children::const_iterator i = rows.begin(); i != rows.end(); ++i) {
		AxisView* av = (*i)[stripable_columns.strip];
		VCAMasterStrip* vms = dynamic_cast<VCAMasterStrip*> (av);
		if (vms && vms->stripable () == s) {
			return av;
		}
	}

	return 0;
}

AxisView*
Mixer_UI::axis_view_by_control (boost::shared_ptr<AutomationControl> c) const
{
	for (list<MixerStrip *>::const_iterator i = strips.begin(); i != strips.end(); ++i) {
		if ((*i)->control() == c) {
			return (*i);
		}
	}

	return 0;
}

bool
Mixer_UI::strip_button_release_event (GdkEventButton *ev, MixerStrip *strip)
{
	/* Selecting a mixer-strip may also select grouped-tracks, and
	 * presentation_info_changed() being emitted and
	 * _selection.axes.begin() is being moved into view. This may
	 * effectively move the track that was clicked-on out of view.
	 *
	 * So here only the track that is actually clicked-on is moved into
	 * view (in case it's partially visible)
	 */
	PBD::Unwinder<bool> uw (_strip_selection_change_without_scroll, true);
	move_stripable_into_view (strip->stripable());

	if (ev->button == 1) {
		if (_selection.selected (strip)) {
			/* primary-click: toggle selection state of strip */
			if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
				_selection.remove (strip, true);
			} else if (_selection.axes.size() > 1) {
				/* de-select others */
				_selection.set (strip);
			}
			PublicEditor& pe = PublicEditor::instance();
			TimeAxisView* tav = pe.time_axis_view_from_stripable (strip->stripable());
			if (tav) {
				pe.set_selected_mixer_strip (*tav);
			}
		} else {
			if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
				_selection.add (strip, true);
			} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::RangeSelectModifier)) {

				/* extend selection */

				vector<MixerStrip*> tmp;
				bool accumulate = false;
				bool found_another = false;

				strips.sort (MixerStripSorter());

				for (list<MixerStrip*>::iterator i = strips.begin(); i != strips.end(); ++i) {
					MixerStrip* ms = *i;
					assert (ms);

					if (ms == strip) {
						/* hit clicked strip, start accumulating till we hit the first
						   selected strip
						*/
						if (accumulate) {
							/* done */
							break;
						} else {
							accumulate = true;
						}
					} else if (_selection.selected (ms)) {
						/* hit selected strip. if currently accumulating others,
						   we're done. if not accumulating others, start doing so.
						*/
						found_another = true;
						if (accumulate) {
							/* done */
							break;
						} else {
							accumulate = true;
						}
					} else {
						if (accumulate) {
							tmp.push_back (ms);
						}
					}
				}

				tmp.push_back (strip);

				if (found_another) {
					PresentationInfo::ChangeSuspender cs;
					for (vector<MixerStrip*>::iterator i = tmp.begin(); i != tmp.end(); ++i) {
						_selection.add (*i, true);
					}
				} else {
					_selection.set (strip);  //user wants to start a range selection, but there aren't any others selected yet
				}
			} else {
				_selection.set (strip);
			}
		}
	}

	return true;
}

bool
Mixer_UI::vca_button_release_event (GdkEventButton *ev, VCAMasterStrip *strip)
{
	_selection.set (strip);
	return true;
}

void
Mixer_UI::set_session (Session* sess)
{
	SessionHandlePtr::set_session (sess);
	_monitor_section.set_session (sess);

	if (_plugin_selector) {
		_plugin_selector->set_session (_session);
	}

	_group_tabs->set_session (sess);

	if (!_session) {
		favorite_plugins_model->clear ();
		_selection.clear ();
		return;
	}

	refill_favorite_plugins();

	XMLNode* node = ARDOUR_UI::instance()->mixer_settings();
	set_state (*node, 0);

	update_title ();

#if 0
	/* skip mapping all session-config vars, we only need one */
	boost::function<void (string)> pc (boost::bind (&Mixer_UI::parameter_changed, this, _1));
	_session->config.map_parameters (pc);
#else
	parameter_changed ("show-group-tabs");
#endif

	initial_track_display ();

	_session->RouteAdded.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::add_routes, this, _1), gui_context());
	_session->route_group_added.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::add_route_group, this, _1), gui_context());
	_session->route_group_removed.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::route_groups_changed, this), gui_context());
	_session->route_groups_reordered.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::route_groups_changed, this), gui_context());
	_session->config.ParameterChanged.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::parameter_changed, this, _1), gui_context());
	_session->DirtyChanged.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::update_title, this), gui_context());
	_session->StateSaved.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::update_title, this), gui_context());

	_session->vca_manager().VCAAdded.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::add_masters, this, _1), gui_context());
	_session->vca_manager().VCACreated.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::new_masters_created, this), gui_context());

	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::parameter_changed, this, _1), gui_context ());

	route_groups_changed ();

	if (_visible) {
		show_window();
	}

	/* catch up on selection state, etc. */

	PropertyChange sc;
	sc.add (Properties::selected);
	_selection.presentation_info_changed (sc);

	start_updating ();
}

void
Mixer_UI::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &Mixer_UI::session_going_away);

	_in_group_rebuild_or_clear = true;
	group_model->clear ();
	_in_group_rebuild_or_clear = false;

	_selection.clear ();
	track_model->clear ();

	for (list<MixerStrip *>::iterator i = strips.begin(); i != strips.end(); ++i) {
		delete (*i);
	}

	_monitor_section.tearoff().hide_visible ();
	StripableList fb;
	_session->get_stripables (fb, PresentationInfo::FoldbackBus);
	if (fb.size()) {
		if (foldback_strip) {
			delete foldback_strip;
			foldback_strip = 0;
		}
	}

	monitor_section_detached ();

	strips.clear ();

	stop_updating ();

	SessionHandlePtr::session_going_away ();

	_session = 0;
	update_title ();
}

void
Mixer_UI::track_visibility_changed (std::string const & path)
{
	if (_session && _session->deletion_in_progress()) {
		return;
	}

	TreeIter iter;

	if ((iter = track_model->get_iter (path))) {

		AxisView* av = (*iter)[stripable_columns.strip];
		bool visible = (*iter)[stripable_columns.visible];

		if (av->set_marked_for_display (!visible)) {
			update_track_visibility ();
		}
	}
}

void
Mixer_UI::update_track_visibility ()
{
	TreeModel::Children rows = track_model->children();
	TreeModel::Children::iterator i;

	{
		Unwinder<bool> uw (no_track_list_redisplay, true);

		for (i = rows.begin(); i != rows.end(); ++i) {
			AxisView* av = (*i)[stripable_columns.strip];
			(*i)[stripable_columns.visible] = av->marked_for_display ();
		}

		/* force presentation to catch up with visibility changes */
		sync_presentation_info_from_treeview ();
	}

	redisplay_track_list ();
}

void
Mixer_UI::show_strip (MixerStrip* ms)
{
	TreeModel::Children rows = track_model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {

		AxisView* av = (*i)[stripable_columns.strip];
		MixerStrip* strip = dynamic_cast<MixerStrip*> (av);
		if (strip == ms) {
			(*i)[stripable_columns.visible] = true;
			av->set_marked_for_display (true);
			update_track_visibility ();
			break;
		}
	}
}

void
Mixer_UI::hide_strip (MixerStrip* ms)
{
	TreeModel::Children rows = track_model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {

		AxisView* av = (*i)[stripable_columns.strip];
		MixerStrip* strip = dynamic_cast<MixerStrip*> (av);
		if (strip == ms) {
			(*i)[stripable_columns.visible] = false;
			av->set_marked_for_display (false);
			update_track_visibility ();
			break;
		}
	}
}

gint
Mixer_UI::start_updating ()
{
	fast_screen_update_connection = Timers::super_rapid_connect (sigc::mem_fun(*this, &Mixer_UI::fast_update_strips));
	return 0;
}

gint
Mixer_UI::stop_updating ()
{
	fast_screen_update_connection.disconnect();
	return 0;
}

void
Mixer_UI::fast_update_strips ()
{
	if (_content.is_mapped () && _session) {
		for (list<MixerStrip *>::iterator i = strips.begin(); i != strips.end(); ++i) {
			(*i)->fast_update ();
		}
		if (foldback_strip) {
			foldback_strip->fast_update ();
		}
	}
}

void
Mixer_UI::set_all_strips_visibility (bool yn)
{
	TreeModel::Children rows = track_model->children();
	TreeModel::Children::iterator i;

	{
		Unwinder<bool> uw (no_track_list_redisplay, true);

		for (i = rows.begin(); i != rows.end(); ++i) {

			AxisView* av = (*i)[stripable_columns.strip];
			MixerStrip* strip = dynamic_cast<MixerStrip*> (av);

			if (!strip) {
				continue;
			}

			if (strip->route()->is_master() || strip->route()->is_monitor()) {
				continue;
			}

			(*i)[stripable_columns.visible] = yn;
		}

		/* force presentation to catch up with visibility changes */
		sync_presentation_info_from_treeview ();
	}

	redisplay_track_list ();
}

void
Mixer_UI::set_all_audio_midi_visibility (int tracks, bool yn)
{
	TreeModel::Children rows = track_model->children();
	TreeModel::Children::iterator i;

	{
		Unwinder<bool> uw (no_track_list_redisplay, true);

		for (i = rows.begin(); i != rows.end(); ++i) {

			AxisView* av = (*i)[stripable_columns.strip];
			MixerStrip* strip = dynamic_cast<MixerStrip*> (av);

			if (!strip) {
				continue;
			}

			if (strip->route()->is_master() || strip->route()->is_monitor()) {
				continue;
			}

			boost::shared_ptr<AudioTrack> at = strip->audio_track();
			boost::shared_ptr<MidiTrack> mt = strip->midi_track();

			switch (tracks) {
			case 0:
				(*i)[stripable_columns.visible] = yn;
				break;

			case 1:
				if (at) { /* track */
					(*i)[stripable_columns.visible] = yn;
				}
				break;

			case 2:
				if (!at && !mt) { /* bus */
					(*i)[stripable_columns.visible] = yn;
				}
				break;

			case 3:
				if (mt) { /* midi-track */
					(*i)[stripable_columns.visible] = yn;
				}
				break;
			}
		}

		/* force presentation to catch up with visibility changes */
		sync_presentation_info_from_treeview ();
	}

	redisplay_track_list ();
}

void
Mixer_UI::hide_all_routes ()
{
	set_all_strips_visibility (false);
}

void
Mixer_UI::show_all_routes ()
{
	set_all_strips_visibility (true);
}

void
Mixer_UI::show_all_audiobus ()
{
	set_all_audio_midi_visibility (2, true);
}
void
Mixer_UI::hide_all_audiobus ()
{
	set_all_audio_midi_visibility (2, false);
}

void
Mixer_UI::show_all_audiotracks()
{
	set_all_audio_midi_visibility (1, true);
}
void
Mixer_UI::hide_all_audiotracks ()
{
	set_all_audio_midi_visibility (1, false);
}

void
Mixer_UI::show_all_miditracks()
{
	set_all_audio_midi_visibility (3, true);
}
void
Mixer_UI::hide_all_miditracks ()
{
	set_all_audio_midi_visibility (3, false);
}

void
Mixer_UI::track_list_reorder (const TreeModel::Path&, const TreeModel::iterator&, int* /*new_order*/)
{
	DEBUG_TRACE (DEBUG::OrderKeys, "mixer UI treeview reordered\n");
	sync_presentation_info_from_treeview ();
}

void
Mixer_UI::track_list_delete (const Gtk::TreeModel::Path&)
{
	/* this happens as the second step of a DnD within the treeview as well
	   as when a row/route is actually deleted.

           if it was a deletion then we have to force a redisplay because
           order keys may not have changed.
	*/

	DEBUG_TRACE (DEBUG::OrderKeys, "mixer UI treeview row deleted\n");

	if (_route_deletion_in_progress) {
		redisplay_track_list ();
	} else {
		sync_presentation_info_from_treeview ();
	}

}

void
Mixer_UI::spill_redisplay (boost::shared_ptr<Stripable> s)
{

	boost::shared_ptr<VCA> vca = boost::dynamic_pointer_cast<VCA> (s);
	boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route> (s);

	TreeModel::Children rows = track_model->children();
	std::list<boost::shared_ptr<VCA> > vcas;

	if (vca) {
		vcas.push_back (vca);

		for (TreeModel::Children::const_iterator i = rows.begin(); i != rows.end(); ++i) {
			AxisView* av = (*i)[stripable_columns.strip];
			VCAMasterStrip* vms = dynamic_cast<VCAMasterStrip*> (av);
			if (vms && vms->vca()->slaved_to (vca)) {
				vcas.push_back (vms->vca());
			}
		}
	}

	for (TreeModel::Children::const_iterator i = rows.begin(); i != rows.end(); ++i) {

		AxisView* av = (*i)[stripable_columns.strip];
		MixerStrip* strip = dynamic_cast<MixerStrip*> (av);
		bool const visible = (*i)[stripable_columns.visible];
		bool slaved = false;
		bool feeds = false;

		if (!strip) {
			/* we're in the middle of changing a row, don't worry */
			continue;
		}

		if (!strip->route()) {
			/* non-route element */
			continue;
		}

		if (strip->route()->is_master() || strip->route()->is_monitor()) {
			continue;
		}

		if (vca) {
			for (std::list<boost::shared_ptr<VCA> >::const_iterator m = vcas.begin(); m != vcas.end(); ++m) {
				if (strip->route()->slaved_to (*m)) {
					slaved = true;
					break;
				}
			}
		}

#ifdef MIXBUS
		if (r && r->mixbus()) {
			feeds = strip->route()->mb_feeds (r);
		} else
#endif
		if (r) {
			feeds = strip->route()->direct_feeds_according_to_graph (r);
		}

		bool should_show = visible && (slaved || feeds);
		should_show |= (strip->route() == r);  //the spilled aux should itself be shown...

		if (should_show) {

			if (strip->packed()) {
				strip_packer.reorder_child (*strip, -1); /* put at end */
			} else {
				strip_packer.pack_start (*strip, false, false);
				strip->set_packed (true);
			}

		} else {

			if (strip->packed()) {
				strip_packer.remove (*strip);
				strip->set_packed (false);
			}
		}
	}
}

void
Mixer_UI::redisplay_track_list ()
{
	if (no_track_list_redisplay) {
		return;
	}

	boost::shared_ptr<Stripable> ss = spilled_strip.lock ();
	if (ss) {
		boost::shared_ptr<VCA> sv = boost::dynamic_pointer_cast<VCA> (ss);
		if (sv) {
			if (_spill_scroll_position <= 0 && scroller.get_hscrollbar()) {
				_spill_scroll_position = scroller.get_hscrollbar()->get_adjustment()->get_value();
			}
			spill_redisplay (sv);
			return;
		} else {
			if (_spill_scroll_position <= 0 && scroller.get_hscrollbar()) {
				_spill_scroll_position = scroller.get_hscrollbar()->get_adjustment()->get_value();
			}
			spill_redisplay (ss);
			return;
		}
	}

	TreeModel::Children rows = track_model->children();
	TreeModel::Children::iterator i;
	uint32_t n_masters = 0;

	container_clear (vca_hpacker);

	vca_hpacker.pack_end (vca_scroller_base, true, true);
	vca_scroller_base.set_size_request (PX_SCALE (20), -1);
	vca_scroller_base.signal_expose_event ().connect (sigc::bind (sigc::ptr_fun (&ArdourWidgets::ArdourIcon::expose_with_text), &vca_scroller_base, ArdourWidgets::ArdourIcon::ShadedPlusSign,
			_("Right-click or Double-click here\nto add Track, Bus, or VCA channels")));
	vca_scroller_base.show();

	for (i = rows.begin(); i != rows.end(); ++i) {

		AxisView* s = (*i)[stripable_columns.strip];
		bool const visible = (*i)[stripable_columns.visible];
		boost::shared_ptr<Stripable> stripable = (*i)[stripable_columns.stripable];

		if (!s) {
			/* we're in the middle of changing a row, don't worry */
			continue;
		}

		VCAMasterStrip* vms;

		if ((vms = dynamic_cast<VCAMasterStrip*> (s))) {
			if (visible) {
				vca_hpacker.pack_start (*vms, false, false);
				vms->show ();
				n_masters++;
			}
			continue;
		}

		MixerStrip* strip = dynamic_cast<MixerStrip*> (s);

		if (!strip) {
			continue;
		}

		if (visible) {

			if (strip->packed()) {
				strip_packer.reorder_child (*strip, -1); /* put at end */
			} else {
				strip_packer.pack_start (*strip, false, false);
				strip->set_packed (true);
			}

		} else {

			if (stripable->is_master() || stripable->is_monitor()) {
				/* do nothing, these cannot be hidden */
			} else {
				if (strip->packed()) {
					strip_packer.remove (*strip);
					strip->set_packed (false);
				}
			}
		}
	}

	/* update visibility of VCA assign buttons */

	if (n_masters == 0) {
		//show/hide the channelstrip VCA assign buttons on channelstrips:
		UIConfiguration::instance().set_mixer_strip_visibility (VisibilityGroup::remove_element (UIConfiguration::instance().get_mixer_strip_visibility(), X_("VCA")));

		Glib::RefPtr<Action> act = ActionManager::get_action ("Mixer", "ToggleVCAPane");
		if (act) {
			act->set_sensitive (false);
		}

		//remove the VCA packer, but don't change our prior setting for show/hide:
		vca_vpacker.hide ();
	} else {
		//show/hide the channelstrip VCA assign buttons on channelstrips:
		UIConfiguration::instance().set_mixer_strip_visibility (VisibilityGroup::add_element (UIConfiguration::instance().get_mixer_strip_visibility(), X_("VCA")));

		Glib::RefPtr<ToggleAction> act = ActionManager::get_toggle_action ("Mixer", "ToggleVCAPane");
		act->set_sensitive (true);

		//if we were showing VCAs before, show them now:
		showhide_vcas (act->get_active ());
	}

	_group_tabs->set_dirty ();

	if (_spill_scroll_position > 0 && scroller.get_hscrollbar()) {
		Adjustment* adj = scroller.get_hscrollbar()->get_adjustment();
		adj->set_value (max (adj->get_lower(), min (adj->get_upper(), _spill_scroll_position)));
	}
	_spill_scroll_position = 0;

}

void
Mixer_UI::strip_width_changed ()
{
	_group_tabs->set_dirty ();

#ifdef __APPLE__
	TreeModel::Children rows = track_model->children();
	TreeModel::Children::iterator i;
	long order;

	for (order = 0, i = rows.begin(); i != rows.end(); ++i, ++order) {
		AxisView* av = (*i)[stripable_columns.strip];
		MixerStrip* strip = dynamic_cast<MixerStrip*> (av);

		if (strip == 0) {
			continue;
		}

		bool visible = (*i)[stripable_columns.visible];

		if (visible) {
			strip->queue_draw();
		}
	}
#endif

}

struct PresentationInfoMixerSorter
{
	bool operator() (boost::shared_ptr<Stripable> a, boost::shared_ptr<Stripable> b) {
		if (a->is_master()) {
			/* master after everything else */
			return false;
		} else if (b->is_master()) {
			/* everything else before master */
			return true;
		}
		return a->presentation_info().order () < b->presentation_info().order ();
	}
};

void
Mixer_UI::initial_track_display ()
{
	StripableList sl;
	StripableList fb;
	_session->get_stripables (sl);
	_session->get_stripables (fb, PresentationInfo::FoldbackBus);
	if (fb.size()) {
		boost::shared_ptr<ARDOUR::Stripable> _current_foldback = *(fb.begin());
		sl.push_back (_current_foldback);
	}

	sl.sort (PresentationInfoMixerSorter());

	{
		/* These are also used inside ::add_stripables() but we need
		 *  them here because we're going to clear the track_model also.
		 */
		Unwinder<bool> uw1 (no_track_list_redisplay, true);
		Unwinder<bool> uw2 (ignore_track_reorder, true);

		track_model->clear ();
		add_stripables (sl);
	}

	sync_treeview_from_presentation_info (Properties::order);
}

bool
Mixer_UI::track_display_button_press (GdkEventButton* ev)
{
	if (Keyboard::is_context_menu_event (ev)) {
		if (track_menu == 0) {
			build_track_menu ();
		}
		track_menu->popup (ev->button, ev->time);
		return true;
	}
	if ((ev->type == GDK_BUTTON_PRESS) && (ev->button == 1)) {
		TreeModel::Path path;
		TreeViewColumn* column;
		int cellx, celly;
		if (track_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
			TreeIter iter = track_model->get_iter (path);
			if ((*iter)[stripable_columns.visible]) {
				boost::shared_ptr<ARDOUR::Stripable> s = (*iter)[stripable_columns.stripable];
				move_stripable_into_view (s);
			}
		}
	}

	return false;
}

void
Mixer_UI::move_vca_into_view (boost::shared_ptr<ARDOUR::Stripable> s)
{
	if (!vca_scroller.get_hscrollbar()) {
		return;
	}

	bool found = false;
	int x0 = 0;
	Gtk::Allocation alloc;

	TreeModel::Children rows = track_model->children();
	for (TreeModel::Children::const_iterator i = rows.begin(); i != rows.end(); ++i) {
		AxisView* av = (*i)[stripable_columns.strip];
		VCAMasterStrip* vms = dynamic_cast<VCAMasterStrip*> (av);
		if (vms && vms->stripable () == s) {
			int y;
			found = true;
			vms->translate_coordinates (vca_hpacker, 0, 0, x0, y);
			alloc = vms->get_allocation ();
			break;
		}
	}

	if (!found) {
		return;
	}

	Adjustment* adj = vca_scroller.get_hscrollbar()->get_adjustment();

	if (x0 < adj->get_value()) {
		adj->set_value (max (adj->get_lower(), min (adj->get_upper(), (double) x0)));
	} else if (x0 + alloc.get_width() >= adj->get_value() + adj->get_page_size()) {
		int x1 = x0 + alloc.get_width() - adj->get_page_size();
		adj->set_value (max (adj->get_lower(), min (adj->get_upper(), (double) x1)));
	}
}

void
Mixer_UI::move_stripable_into_view (boost::shared_ptr<ARDOUR::Stripable> s)
{
	if (!scroller.get_hscrollbar()) {
		return;
	}
	if (s->presentation_info().special ()) {
		return;
	}
	if (s->presentation_info().flag_match (PresentationInfo::VCA)) {
		move_vca_into_view (s);
	}
#ifdef MIXBUS
	if (s->mixbus ()) {
		return;
	}
#endif
	bool found = false;
	int x0 = 0;
	Gtk::Allocation alloc;
	for (list<MixerStrip *>::const_iterator i = strips.begin(); i != strips.end(); ++i) {
		if ((*i)->route() == s) {
			int y;
			found = true;
			(*i)->translate_coordinates (strip_packer, 0, 0, x0, y);
			alloc = (*i)->get_allocation ();
			break;
		}
	}
	if (!found) {
		return;
	}

	Adjustment* adj = scroller.get_hscrollbar()->get_adjustment();

	if (x0 < adj->get_value()) {
		adj->set_value (max (adj->get_lower(), min (adj->get_upper(), (double) x0)));
	} else if (x0 + alloc.get_width() >= adj->get_value() + adj->get_page_size()) {
		int x1 = x0 + alloc.get_width() - adj->get_page_size();
		adj->set_value (max (adj->get_lower(), min (adj->get_upper(), (double) x1)));
	}
}

void
Mixer_UI::build_track_menu ()
{
	using namespace Menu_Helpers;
	using namespace Gtk;

	track_menu = new Menu;
	track_menu->set_name ("ArdourContextMenu");
	MenuList& items = track_menu->items();

	items.push_back (MenuElem (_("Show All"), sigc::mem_fun(*this, &Mixer_UI::show_all_routes)));
	items.push_back (MenuElem (_("Hide All"), sigc::mem_fun(*this, &Mixer_UI::hide_all_routes)));
	items.push_back (MenuElem (_("Show All Audio Tracks"), sigc::mem_fun(*this, &Mixer_UI::show_all_audiotracks)));
	items.push_back (MenuElem (_("Hide All Audio Tracks"), sigc::mem_fun(*this, &Mixer_UI::hide_all_audiotracks)));
	items.push_back (MenuElem (_("Show All Midi Tracks"), sigc::mem_fun (*this, &Mixer_UI::show_all_miditracks)));
	items.push_back (MenuElem (_("Hide All Midi Tracks"), sigc::mem_fun (*this, &Mixer_UI::hide_all_miditracks)));
	items.push_back (MenuElem (_("Show All Busses"), sigc::mem_fun(*this, &Mixer_UI::show_all_audiobus)));
	items.push_back (MenuElem (_("Hide All Busses"), sigc::mem_fun(*this, &Mixer_UI::hide_all_audiobus)));

}

void
Mixer_UI::stripable_property_changed (const PropertyChange& what_changed, boost::weak_ptr<Stripable> ws)
{
	if (!what_changed.contains (ARDOUR::Properties::hidden) && !what_changed.contains (ARDOUR::Properties::name)) {
		return;
	}

	boost::shared_ptr<Stripable> s = ws.lock ();

	if (!s) {
		return;
	}

	TreeModel::Children rows = track_model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		boost::shared_ptr<Stripable> ss = (*i)[stripable_columns.stripable];

		if (s == ss) {

			if (what_changed.contains (ARDOUR::Properties::name)) {
				(*i)[stripable_columns.text] = s->name();
			}

			if (what_changed.contains (ARDOUR::Properties::hidden)) {
				(*i)[stripable_columns.visible] = !s->presentation_info().hidden();
				redisplay_track_list ();
			}

			return;
		}
	}

	if (s->is_master ()) {
		return;
	}

	error << _("track display list item for renamed strip not found!") << endmsg;
}

bool
Mixer_UI::group_display_button_press (GdkEventButton* ev)
{
	TreeModel::Path path;
	TreeViewColumn* column;
	int cellx;
	int celly;

	if (!group_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		if (ev->button == 3) {
			_group_tabs->get_menu(0)->popup (ev->button, ev->time);
		}
		return true;
	}

	TreeIter iter = group_model->get_iter (path);
	if (!iter) {
		if (ev->button == 3) {
			_group_tabs->get_menu(0)->popup (ev->button, ev->time);
		}
		return true;
	}

	RouteGroup* group = (*iter)[group_columns.group];

	if (Keyboard::is_context_menu_event (ev)) {
		_group_tabs->get_menu(group)->popup (1, ev->time);
		return true;
	}

	switch (GPOINTER_TO_UINT (column->get_data (X_("colnum")))) {
	case 1:
		if (Keyboard::is_edit_event (ev)) {
			if (group) {
				// edit_route_group (group);
#ifdef __APPLE__
				group_display.queue_draw();
#endif
				return true;
			}
		}
		break;

	case 0:
	{
		bool visible = (*iter)[group_columns.visible];
		(*iter)[group_columns.visible] = !visible;
#ifdef __APPLE__
		group_display.queue_draw();
#endif
		return true;
	}

	default:
		break;
	}

	return false;
 }

void
Mixer_UI::activate_all_route_groups ()
{
	_session->foreach_route_group (sigc::bind (sigc::mem_fun (*this, &Mixer_UI::set_route_group_activation), true));
}

void
Mixer_UI::disable_all_route_groups ()
{
	_session->foreach_route_group (sigc::bind (sigc::mem_fun (*this, &Mixer_UI::set_route_group_activation), false));
}

void
Mixer_UI::route_groups_changed ()
{
	ENSURE_GUI_THREAD (*this, &Mixer_UI::route_groups_changed);

	_in_group_rebuild_or_clear = true;

	/* just rebuild the while thing */

	group_model->clear ();

#if 0
	/* this is currently not used,
	 * Mixer_UI::group_display_button_press() has a case for it,
	 * and a commented edit_route_group() but that's n/a since 2011.
	 *
	 * This code is left as reminder that
	 * row[group_columns.group] = 0 has special meaning.
	 */
	{
		TreeModel::Row row;
		row = *(group_model->append());
		row[group_columns.visible] = true;
		row[group_columns.text] = (_("-all-"));
		row[group_columns.group] = 0;
	}
#endif

	_session->foreach_route_group (sigc::mem_fun (*this, &Mixer_UI::add_route_group));

	_group_tabs->set_dirty ();
	_in_group_rebuild_or_clear = false;
}

void
Mixer_UI::new_route_group ()
{
	_group_tabs->run_new_group_dialog (0, false);
}

void
Mixer_UI::remove_selected_route_group ()
{
	Glib::RefPtr<TreeSelection> selection = group_display.get_selection();
	TreeView::Selection::ListHandle_Path rows = selection->get_selected_rows ();

	if (rows.empty()) {
		return;
	}

	TreeView::Selection::ListHandle_Path::iterator i = rows.begin();
	TreeIter iter;

	/* selection mode is single, so rows.begin() is it */

	if ((iter = group_model->get_iter (*i))) {

		RouteGroup* rg = (*iter)[group_columns.group];

		if (rg) {
			_session->remove_route_group (*rg);
		}
	}
}

void
Mixer_UI::route_group_property_changed (RouteGroup* group, const PropertyChange& change)
{
	if (in_group_row_change) {
		return;
	}

	/* force an update of any mixer strips that are using this group,
	   otherwise mix group names don't change in mixer strips
	*/

	for (list<MixerStrip *>::iterator i = strips.begin(); i != strips.end(); ++i) {
		if ((*i)->route_group() == group) {
			(*i)->route_group_changed();
		}
	}

	TreeModel::iterator i;
	TreeModel::Children rows = group_model->children();
	Glib::RefPtr<TreeSelection> selection = group_display.get_selection();

	in_group_row_change = true;

	for (i = rows.begin(); i != rows.end(); ++i) {
		if ((*i)[group_columns.group] == group) {
			(*i)[group_columns.visible] = !group->is_hidden ();
			(*i)[group_columns.text] = group->name ();
			break;
		}
	}

	in_group_row_change = false;

	if (change.contains (Properties::name)) {
		_group_tabs->set_dirty ();
	}

	for (list<MixerStrip*>::iterator j = strips.begin(); j != strips.end(); ++j) {
		if ((*j)->route_group() == group) {
			if (group->is_hidden ()) {
				hide_strip (*j);
			} else {
				show_strip (*j);
			}
		}
	}
}

void
Mixer_UI::toggle_mixer_list ()
{
	Glib::RefPtr<ToggleAction> act = ActionManager::get_toggle_action ("Mixer", "ToggleMixerList");
	showhide_mixer_list (act->get_active());
}

void
Mixer_UI::showhide_mixer_list (bool yn)
{
	if (yn) {
		list_vpacker.show ();
	} else {
		list_vpacker.hide ();
	}
}

void
Mixer_UI::toggle_monitor_section ()
{
	Glib::RefPtr<ToggleAction> act = ActionManager::get_toggle_action ("Mixer", "ToggleMonitorSection");
	showhide_monitor_section (act->get_active());
}


void
Mixer_UI::showhide_monitor_section (bool yn)
{
	if (monitor_section().tearoff().torn_off()) {
		return;
	}

	if (yn) {
		monitor_section().tearoff().show();
	} else {
		monitor_section().tearoff().hide();
	}
}

void
Mixer_UI::toggle_foldback_strip ()
{
	Glib::RefPtr<ToggleAction> act = ActionManager::get_toggle_action ("Mixer", "ToggleFoldbackStrip");
	showhide_foldback_strip (act->get_active());
}


void
Mixer_UI::showhide_foldback_strip (bool yn)
{
	_show_foldback_strip = yn;

	if (foldback_strip) {
		if (yn) {
			foldback_strip->show();
		} else {
			foldback_strip->hide();
		}
	}
}

void
Mixer_UI::toggle_vcas ()
{
	Glib::RefPtr<ToggleAction> act = ActionManager::get_toggle_action ("Mixer", "ToggleVCAPane");
	showhide_vcas (act->get_active());
}

void
Mixer_UI::showhide_vcas (bool yn)
{
	if (yn) {
		vca_vpacker.show();
	} else {
		vca_vpacker.hide();
	}
}

#ifdef MIXBUS
void
Mixer_UI::toggle_mixbuses ()
{
	Glib::RefPtr<ToggleAction> act = ActionManager::get_toggle_action ("Mixer", "ToggleMixbusPane");
	showhide_mixbuses (act->get_active());
}

void
Mixer_UI::showhide_mixbuses (bool on)
{
	if (on) {
		mb_vpacker.show();
	} else {
		mb_vpacker.hide();
	}
}
#endif


void
Mixer_UI::route_group_name_edit (const std::string& path, const std::string& new_text)
{
	RouteGroup* group;
	TreeIter iter;

	if ((iter = group_model->get_iter (path))) {

		if ((group = (*iter)[group_columns.group]) == 0) {
			return;
		}

		if (new_text != group->name()) {
			group->set_name (new_text);
		}
	}
}

void
Mixer_UI::route_group_row_change (const Gtk::TreeModel::Path&, const Gtk::TreeModel::iterator& iter)
{
	RouteGroup* group;

	if (in_group_row_change) {
		return;
	}

	if ((group = (*iter)[group_columns.group]) == 0) {
		return;
	}

	std::string name = (*iter)[group_columns.text];

	if (name != group->name()) {
		group->set_name (name);
	}

	bool hidden = !(*iter)[group_columns.visible];

	if (hidden != group->is_hidden ()) {
		group->set_hidden (hidden, this);
	}
}

/** Called when a group model row is deleted, but also when the model is
 *  reordered by a user drag-and-drop; the latter is what we are
 *  interested in here.
 */
void
Mixer_UI::route_group_row_deleted (Gtk::TreeModel::Path const &)
{
	if (_in_group_rebuild_or_clear) {
		return;
	}

	/* Re-write the session's route group list so that the new order is preserved */

	list<RouteGroup*> new_list;

	Gtk::TreeModel::Children children = group_model->children();
	for (Gtk::TreeModel::Children::iterator i = children.begin(); i != children.end(); ++i) {
		RouteGroup* g = (*i)[group_columns.group];
		if (g) {
			new_list.push_back (g);
		}
	}

	_session->reorder_route_groups (new_list);
}


void
Mixer_UI::add_route_group (RouteGroup* group)
{
	ENSURE_GUI_THREAD (*this, &Mixer_UI::add_route_group, group)
	bool focus = false;

	in_group_row_change = true;

	TreeModel::Row row = *(group_model->append());
	row[group_columns.visible] = !group->is_hidden ();
	row[group_columns.group] = group;
	if (!group->name().empty()) {
		row[group_columns.text] = group->name();
	} else {
		row[group_columns.text] = _("unnamed");
		focus = true;
	}

	group->PropertyChanged.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::route_group_property_changed, this, group, _1), gui_context());

	if (focus) {
		TreeViewColumn* col = group_display.get_column (0);
		CellRendererText* name_cell = dynamic_cast<CellRendererText*>(group_display.get_column_cell_renderer (1));
		group_display.set_cursor (group_model->get_path (row), *col, *name_cell, true);
	}

	_group_tabs->set_dirty ();

	in_group_row_change = false;
}

bool
Mixer_UI::strip_scroller_button_event (GdkEventButton* ev)
{
	if ((ev->type == GDK_2BUTTON_PRESS && ev->button == 1) || (ev->type == GDK_BUTTON_RELEASE && Keyboard::is_context_menu_event (ev))) {
		ARDOUR_UI::instance()->add_route ();
		return true;
	}
	return false;
}

void
Mixer_UI::scroller_drag_data_received (const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, const Gtk::SelectionData& data, guint info, guint time)
{
	if (data.get_target() != "x-ardour/plugin.favorite") {
		context->drag_finish (false, false, time);
		return;
	}

	const void * d = data.get_data();
	const Gtkmm2ext::DnDTreeView<ARDOUR::PluginPresetPtr>* tv = reinterpret_cast<const Gtkmm2ext::DnDTreeView<ARDOUR::PluginPresetPtr>*>(d);

	PluginPresetList nfos;
	TreeView* source;
	tv->get_object_drag_data (nfos, &source);

	Route::ProcessorList pl;
	bool ok = false;

	for (list<PluginPresetPtr>::const_iterator i = nfos.begin(); i != nfos.end(); ++i) {
		PluginPresetPtr ppp = (*i);
		PluginInfoPtr pip = ppp->_pip;
		if (!pip->is_instrument ()) {
			continue;
		}
		ARDOUR_UI::instance()->session_add_midi_route (true, (RouteGroup*) 0, 1, _("MIDI"), Config->get_strict_io (), pip, ppp->_preset.valid ? &ppp->_preset : 0, PresentationInfo::max_order, false);
		ok = true;
	}

	context->drag_finish (ok, false, time);
}

void
Mixer_UI::set_strip_width (Width w, bool save)
{
	_strip_width = w;

	for (list<MixerStrip*>::iterator i = strips.begin(); i != strips.end(); ++i) {
		(*i)->set_width_enum (w, save ? (*i)->width_owner() : this);
	}
}

int
Mixer_UI::set_state (const XMLNode& node, int version)
{
	bool yn;

	Tabbable::set_state (node, version);

	if (node.get_property ("narrow-strips", yn)) {
		if (yn) {
			set_strip_width (Narrow);
		} else {
			set_strip_width (Wide);
		}
	}

	node.get_property ("show-mixer", _visible);

	yn = false;
	node.get_property ("maximised", yn);
	{
		Glib::RefPtr<ToggleAction> act = ActionManager::get_toggle_action (X_("Common"), X_("ToggleMaximalMixer"));
		bool fs = act && act->get_active();
		if (yn ^ fs) {
			ActionManager::do_action ("Common", "ToggleMaximalMixer");
		}
	}

	yn = true;
	node.get_property ("show-mixer-list", yn);
	{
		Glib::RefPtr<ToggleAction> act = ActionManager::get_toggle_action (X_("Mixer"), X_("ToggleMixerList"));
		/* do it twice to force the change */
		act->set_active (!yn);
		act->set_active (yn);
	}

	yn = true;
	node.get_property ("monitor-section-visible", yn);
	{
		Glib::RefPtr<ToggleAction> act = ActionManager::get_toggle_action (X_("Mixer"), X_("ToggleMonitorSection"));
		/* do it twice to force the change */
		act->set_active (!yn);
		act->set_active (yn);
	}

	yn = true;
	node.get_property ("foldback-strip-visible", yn);
	{
		Glib::RefPtr<ToggleAction> act = ActionManager::get_toggle_action (X_("Mixer"), X_("ToggleFoldbackStrip"));
		/* do it twice to force the change */
		act->set_active (!yn);
		act->set_active (yn);
	}

	yn = true;
	node.get_property ("show-vca-pane", yn);
	{
		Glib::RefPtr<ToggleAction> act = ActionManager::get_toggle_action (X_("Mixer"), X_("ToggleVCAPane"));
		/* do it twice to force the change */
		act->set_active (!yn);
		act->set_active (yn);
	}

#ifdef MIXBUS
	yn = true;
	node.get_property ("show-mixbus-pane", yn);
	{
		Glib::RefPtr<ToggleAction> act = ActionManager::get_toggle_action (X_("Mixer"), X_("ToggleMixbusPane"));
		/* do it twice to force the change */
		act->set_active (!yn);
		act->set_active (yn);
	}
#endif

	XMLNode plugin_order (X_("PO"));
	if (PluginManager::instance().load_plugin_order_file (plugin_order)) {
		favorite_ui_order.clear ();
		const XMLNodeList& kids = plugin_order.children("PluginInfo");
		XMLNodeConstIterator i;
		for (i = kids.begin(); i != kids.end(); ++i) {
			std::string unique_id;
			if ((*i)->get_property ("unique-id", unique_id)) {
				favorite_ui_order.push_back (unique_id);
				if ((*i)->get_property ("expanded", yn)) {
					favorite_ui_state[unique_id] = yn;
				}
			}
		}
		sync_treeview_from_favorite_order ();
	}

	return 0;
}

void
Mixer_UI::favorite_plugins_deleted (const TreeModel::Path&)
{
	if (ignore_plugin_reorder) {
		return;
	}
	/* re-order is implemented by insert; delete */
	save_plugin_order_file ();
}

void
Mixer_UI::save_plugin_order_file ()
{
	store_current_favorite_order ();

	XMLNode plugin_order ("PluginOrder");
	uint32_t cnt = 0;
	for (std::list<std::string>::const_iterator i = favorite_ui_order.begin(); i != favorite_ui_order.end(); ++i, ++cnt) {
		XMLNode* p = new XMLNode ("PluginInfo");
		p->set_property ("sort", cnt);
		p->set_property ("unique-id", *i);
		if (favorite_ui_state.find (*i) != favorite_ui_state.end ()) {
			p->set_property ("expanded", favorite_ui_state[*i]);
		}
		plugin_order.add_child_nocopy (*p);
	}
	PluginManager::instance().save_plugin_order_file (plugin_order);
}

XMLNode&
Mixer_UI::get_state ()
{
	XMLNode* node = new XMLNode (X_("Mixer"));

	node->add_child_nocopy (Tabbable::get_state());

	node->set_property (X_("mixer-rhs-pane1-pos"), rhs_pane1.get_divider());
	node->set_property (X_("mixer-rhs_pane2-pos"), rhs_pane2.get_divider());
	node->set_property (X_("mixer-list-hpane-pos"), list_hpane.get_divider());
	node->set_property (X_("mixer-inner-pane-pos"),  inner_pane.get_divider());

	node->set_property ("narrow-strips", (_strip_width == Narrow));
	node->set_property ("show-mixer", _visible);
	node->set_property ("maximised", _maximised);

	Glib::RefPtr<ToggleAction> act = ActionManager::get_toggle_action ("Mixer", "ToggleMixerList");
	node->set_property ("show-mixer-list", act->get_active ());

	act = ActionManager::get_toggle_action ("Mixer", "ToggleMonitorSection");
	node->set_property ("monitor-section-visible", act->get_active ());

	act = ActionManager::get_toggle_action ("Mixer", "ToggleFoldbackStrip");
	node->set_property ("foldback-strip-visible", act->get_active ());

	act = ActionManager::get_toggle_action ("Mixer", "ToggleVCAPane");
	node->set_property ("show-vca-pane", act->get_active ());

#ifdef MIXBUS
	act = ActionManager::get_toggle_action ("Mixer", "ToggleMixbusPane");
	node->set_property ("show-mixbus-pane", act->get_active ());
#endif

	return *node;
}

void
Mixer_UI::scroll_left ()
{
	if (!scroller.get_hscrollbar()) return;
	Adjustment* adj = scroller.get_hscrollbar()->get_adjustment();
	int sc_w = scroller.get_width();
	int sp_w = strip_packer.get_width();
	if (sp_w <= sc_w) {
		return;
	}
	int lp = adj->get_value();
	int lm = 0;
	using namespace Gtk::Box_Helpers;
	const BoxList& strips = strip_packer.children();
	for (BoxList::const_iterator i = strips.begin(); i != strips.end(); ++i) {
		if (i->get_widget() == &scroller_base) {
			continue;
		}
#ifdef MIXBUS
		if (i->get_widget() == &mb_shadow) {
			continue;
		}
#endif
		lm += i->get_widget()->get_width ();
		if (lm >= lp) {
			lm -= i->get_widget()->get_width ();
			break;
		}
	}
	scroller.get_hscrollbar()->set_value (max (adj->get_lower(), min (adj->get_upper(), lm - 1.0)));
}

void
Mixer_UI::scroll_right ()
{
	if (!scroller.get_hscrollbar()) return;
	Adjustment* adj = scroller.get_hscrollbar()->get_adjustment();
	int sc_w = scroller.get_width();
	int sp_w = strip_packer.get_width();
	if (sp_w <= sc_w) {
		return;
	}
	int lp = adj->get_value();
	int lm = 0;
	using namespace Gtk::Box_Helpers;
	const BoxList& strips = strip_packer.children();
	for (BoxList::const_iterator i = strips.begin(); i != strips.end(); ++i) {
		if (i->get_widget() == &scroller_base) {
			continue;
		}
#ifdef MIXBUS
		if (i->get_widget() == &mb_shadow) {
			continue;
		}
#endif
		lm += i->get_widget()->get_width ();
		if (lm > lp + 1) {
			break;
		}
	}
	scroller.get_hscrollbar()->set_value (max (adj->get_lower(), min (adj->get_upper(), lm - 1.0)));
}

bool
Mixer_UI::on_scroll_event (GdkEventScroll* ev)
{
	switch (ev->direction) {
	case GDK_SCROLL_LEFT:
		scroll_left ();
		return true;
	case GDK_SCROLL_UP:
		if (ev->state & Keyboard::TertiaryModifier) {
			scroll_left ();
			return true;
		}
		return false;

	case GDK_SCROLL_RIGHT:
		scroll_right ();
		return true;

	case GDK_SCROLL_DOWN:
		if (ev->state & Keyboard::TertiaryModifier) {
			scroll_right ();
			return true;
		}
		return false;
	}

	return false;
}

void
Mixer_UI::vca_scroll_left ()
{
	if (!vca_scroller.get_hscrollbar()) return;
	Adjustment* adj = vca_scroller.get_hscrollbar()->get_adjustment();
	int sc_w = vca_scroller.get_width();
	int sp_w = strip_packer.get_width();
	if (sp_w <= sc_w) {
		return;
	}
	int lp = adj->get_value();
	int lm = 0;
	using namespace Gtk::Box_Helpers;
	const BoxList& strips = vca_hpacker.children();
	for (BoxList::const_iterator i = strips.begin(); i != strips.end(); ++i) {
		if (i->get_widget() == &vca_scroller_base) {
			continue;
		}
		lm += i->get_widget()->get_width ();
		if (lm >= lp) {
			lm -= i->get_widget()->get_width ();
			break;
		}
	}
	vca_scroller.get_hscrollbar()->set_value (max (adj->get_lower(), min (adj->get_upper(), lm - 1.0)));
}

void
Mixer_UI::vca_scroll_right ()
{
	if (!vca_scroller.get_hscrollbar()) return;
	Adjustment* adj = vca_scroller.get_hscrollbar()->get_adjustment();
	int sc_w = vca_scroller.get_width();
	int sp_w = strip_packer.get_width();
	if (sp_w <= sc_w) {
		return;
	}
	int lp = adj->get_value();
	int lm = 0;
	using namespace Gtk::Box_Helpers;
	const BoxList& strips = vca_hpacker.children();
	for (BoxList::const_iterator i = strips.begin(); i != strips.end(); ++i) {
		if (i->get_widget() == &vca_scroller_base) {
			continue;
		}
		lm += i->get_widget()->get_width ();
		if (lm > lp + 1) {
			break;
		}
	}
	vca_scroller.get_hscrollbar()->set_value (max (adj->get_lower(), min (adj->get_upper(), lm - 1.0)));
}

bool
Mixer_UI::on_vca_scroll_event (GdkEventScroll* ev)
{
	switch (ev->direction) {
	case GDK_SCROLL_LEFT:
		vca_scroll_left ();
		return true;
	case GDK_SCROLL_UP:
		if (ev->state & Keyboard::TertiaryModifier) {
			vca_scroll_left ();
			return true;
		}
		return false;

	case GDK_SCROLL_RIGHT:
		vca_scroll_right ();
		return true;

	case GDK_SCROLL_DOWN:
		if (ev->state & Keyboard::TertiaryModifier) {
			vca_scroll_right ();
			return true;
		}
		return false;
	}

	return false;
}

void
Mixer_UI::parameter_changed (string const & p)
{
	if (p == "show-group-tabs") {
		bool const s = _session ? _session->config.get_show_group_tabs () : true;
		if (s) {
			_group_tabs->show ();
		} else {
			_group_tabs->hide ();
		}
	} else if (p == "default-narrow_ms") {
		bool const s = UIConfiguration::instance().get_default_narrow_ms ();
		for (list<MixerStrip*>::iterator i = strips.begin(); i != strips.end(); ++i) {
			(*i)->set_width_enum (s ? Narrow : Wide, this);
		}
	}
}

void
Mixer_UI::set_route_group_activation (RouteGroup* g, bool a)
{
	g->set_active (a, this);
}

PluginSelector*
Mixer_UI::plugin_selector()
{
#ifdef DEFER_PLUGIN_SELECTOR_LOAD
	if (!_plugin_selector)
		_plugin_selector = new PluginSelector (PluginManager::instance());
#endif

	return _plugin_selector;
}

void
Mixer_UI::setup_track_display ()
{
	track_model = ListStore::create (stripable_columns);
	track_display.set_model (track_model);
	track_display.append_column (_("Show"), stripable_columns.visible);
	track_display.append_column (_("Strips"), stripable_columns.text);
	track_display.get_column (0)->set_data (X_("colnum"), GUINT_TO_POINTER(0));
	track_display.get_column (1)->set_data (X_("colnum"), GUINT_TO_POINTER(1));
	track_display.get_column (0)->set_expand(false);
	track_display.get_column (1)->set_expand(true);
	track_display.get_column (1)->set_sizing (Gtk::TREE_VIEW_COLUMN_FIXED);
	track_display.set_name (X_("EditGroupList"));
	track_display.get_selection()->set_mode (Gtk::SELECTION_NONE);
	track_display.set_reorderable (true);
	track_display.set_headers_visible (true);
	track_display.set_can_focus(false);

	track_model->signal_row_deleted().connect (sigc::mem_fun (*this, &Mixer_UI::track_list_delete));
	track_model->signal_rows_reordered().connect (sigc::mem_fun (*this, &Mixer_UI::track_list_reorder));

	CellRendererToggle* track_list_visible_cell = dynamic_cast<CellRendererToggle*>(track_display.get_column_cell_renderer (0));
	track_list_visible_cell->property_activatable() = true;
	track_list_visible_cell->property_radio() = false;
	track_list_visible_cell->signal_toggled().connect (sigc::mem_fun (*this, &Mixer_UI::track_visibility_changed));

	track_display.signal_button_press_event().connect (sigc::mem_fun (*this, &Mixer_UI::track_display_button_press), false);

	track_display_scroller.add (track_display);
	track_display_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	track_display_frame.set_name("BaseFrame");
	track_display_frame.set_shadow_type (Gtk::SHADOW_IN);
	track_display_frame.add (track_display_scroller);

	track_display_scroller.show();
	track_display_frame.show();
	track_display.show();
}

void
Mixer_UI::new_track_or_bus ()
{
	ARDOUR_UI::instance()->add_route ();
}

void
Mixer_UI::update_title ()
{
	if (!own_window()) {
		return;
	}

	if (_session) {
		string n;

		if (_session->snap_name() != _session->name()) {
			n = _session->snap_name ();
		} else {
			n = _session->name ();
		}

		if (_session->dirty ()) {
			n = "*" + n;
		}

		WindowTitle title (n);
		title += S_("Window|Mixer");
		title += Glib::get_application_name ();
		own_window()->set_title (title.get_string());

	} else {

		WindowTitle title (S_("Window|Mixer"));
		title += Glib::get_application_name ();
		own_window()->set_title (title.get_string());
	}
}

MixerStrip*
Mixer_UI::strip_by_x (int x)
{
	for (list<MixerStrip*>::iterator i = strips.begin(); i != strips.end(); ++i) {
		int x1, x2, y;

		(*i)->translate_coordinates (_content, 0, 0, x1, y);
		x2 = x1 + (*i)->get_width();

		if (x >= x1 && x <= x2) {
			return (*i);
		}
	}

	return 0;
}

void
Mixer_UI::set_axis_targets_for_operation ()
{
	_axis_targets.clear ();

	if (!_selection.empty()) {
		_axis_targets = _selection.axes;
		return;
	}

//  removed "implicit" selections of strips, after discussion on IRC

}

void
Mixer_UI::monitor_section_going_away ()
{
	XMLNode* ui_node = Config->extra_xml(X_("UI"));

	/* immediate state save.
	 *
	 * Tearoff settings are otherwise only stored during
	 * save_ardour_state(). The mon-section may or may not
	 * exist at that point.
	 */

	if (ui_node) {
		XMLNode* tearoff_node = ui_node->child (X_("Tearoffs"));
		if (tearoff_node) {
			tearoff_node->remove_nodes_and_delete (X_("monitor-section"));
			XMLNode* t = new XMLNode (X_("monitor-section"));
			_monitor_section.tearoff().add_state (*t);
			tearoff_node->add_child_nocopy (*t);
		}
	}

	monitor_section_detached ();
	out_packer.remove (_monitor_section.tearoff());
}

void
Mixer_UI::toggle_midi_input_active (bool flip_others)
{
	boost::shared_ptr<RouteList> rl (new RouteList);
	bool onoff = false;

	set_axis_targets_for_operation ();

	for (AxisViewSelection::iterator r = _axis_targets.begin(); r != _axis_targets.end(); ++r) {
		boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack> ((*r)->stripable());

		if (mt) {
			rl->push_back (mt);
			onoff = !mt->input_active();
		}
	}

	_session->set_exclusive_input_active (rl, onoff, flip_others);
}

void
Mixer_UI::maximise_mixer_space ()
{
	if (!own_window()) {
		return;
	}

	if (_maximised) {
		return;
	}

	_window->fullscreen ();
	_maximised = true;
}

void
Mixer_UI::restore_mixer_space ()
{
	if (!own_window()) {
		return;
	}

	if (!_maximised) {
		return;
	}

	own_window()->unfullscreen();
	_maximised = false;
}

void
Mixer_UI::monitor_section_attached ()
{
	Glib::RefPtr<ToggleAction> act = ActionManager::get_toggle_action ("Mixer", "ToggleMonitorSection");
	act->set_sensitive (true);
	showhide_monitor_section (act->get_active ());
}

void
Mixer_UI::monitor_section_detached ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action ("Mixer", "ToggleMonitorSection");
	act->set_sensitive (false);
}

Mixer_UI::PluginListMode
Mixer_UI::plugin_list_mode () const
{
	if (favorite_plugins_mode_combo.get_active_text() == _("Top-10 Plugins")) {
		return PLM_TopHits;
	} else if (favorite_plugins_mode_combo.get_active_text() == _("Recent Plugins")) {
		return PLM_Recent;
	} else {
		return PLM_Favorite;
	}
}

void
Mixer_UI::store_current_favorite_order ()
{
	if (plugin_list_mode () != PLM_Favorite || !plugin_search_entry.get_text ().empty()) {
		return;
	}

	typedef Gtk::TreeModel::Children type_children;
	type_children children = favorite_plugins_model->children();
	favorite_ui_order.clear();
	for(type_children::iterator iter = children.begin(); iter != children.end(); ++iter)
	{
		Gtk::TreeModel::Row row = *iter;
		ARDOUR::PluginPresetPtr ppp = row[favorite_plugins_columns.plugin];
		favorite_ui_order.push_back ((*ppp->_pip).unique_id);
		favorite_ui_state[(*ppp->_pip).unique_id] = favorite_plugins_display.row_expanded (favorite_plugins_model->get_path(iter));
	}
}

void
Mixer_UI::save_favorite_ui_state (const TreeModel::iterator& iter, const TreeModel::Path& path)
{
	Gtk::TreeModel::Row row = *iter;
	ARDOUR::PluginPresetPtr ppp = row[favorite_plugins_columns.plugin];
	assert (ppp);
	favorite_ui_state[(*ppp->_pip).unique_id] = favorite_plugins_display.row_expanded (favorite_plugins_model->get_path(iter));
}

void
Mixer_UI::plugin_list_mode_changed ()
{
	if (plugin_list_mode () == PLM_Favorite) {
		PBD::Unwinder<bool> uw (ignore_plugin_refill, true);
		favorite_plugins_search_hbox.show ();
		plugin_search_entry.set_text ("");
	} else {
		favorite_plugins_search_hbox.hide ();
	}
	refill_favorite_plugins ();
}

void
Mixer_UI::plugin_search_entry_changed ()
{
	if (plugin_list_mode () == PLM_Favorite) {
		refill_favorite_plugins ();
	}
}

void
Mixer_UI::plugin_search_clear_button_clicked ()
{
	plugin_search_entry.set_text ("");
}

void
Mixer_UI::refiller (PluginInfoList& result, const PluginInfoList& plugs)
{
	PluginManager& manager (PluginManager::instance());
	PluginListMode plm = plugin_list_mode ();

	std::string searchstr = plugin_search_entry.get_text ();
	setup_search_string (searchstr);

	for (PluginInfoList::const_iterator i = plugs.begin(); i != plugs.end(); ++i) {
		bool maybe_show = true;

		if (plm == PLM_Favorite) {
			if (manager.get_status (*i) != PluginManager::Favorite) {
				maybe_show = false;
			}

			if (maybe_show && !searchstr.empty()) {
				maybe_show = false;
				/* check name */
				std::string compstr = (*i)->name;
				setup_search_string (compstr);
				maybe_show |= match_search_strings (compstr, searchstr);
				/* check tags */
				std::string tags = manager.get_tags_as_string (*i);
				setup_search_string (tags);
				maybe_show |= match_search_strings (tags, searchstr);
			}
		} else {
			int64_t lru;
			uint64_t use_count;
			if (!manager.stats (*i, lru, use_count)) {
				maybe_show = false;
			}
			if (plm == PLM_Recent && lru == 0) {
				maybe_show = false;
			}
		}

		if (!maybe_show) {
			continue;
		}
		result.push_back (*i);
	}
}

void
Mixer_UI::refill_favorite_plugins ()
{
	if (ignore_plugin_refill) {
		return;
	}

	PluginInfoList plugs;
	PluginManager& mgr (PluginManager::instance());

#ifdef WINDOWS_VST_SUPPORT
	refiller (plugs, mgr.windows_vst_plugin_info ());
#endif
#ifdef LXVST_SUPPORT
	refiller (plugs, mgr.lxvst_plugin_info ());
#endif
#ifdef MACVST_SUPPORT
	refiller (plugs, mgr.mac_vst_plugin_info ());
#endif
#ifdef VST3_SUPPORT
	refiller (plugs, mgr.vst3_plugin_info ());
#endif
#ifdef AUDIOUNIT_SUPPORT
	refiller (plugs, mgr.au_plugin_info ());
#endif
	refiller (plugs, mgr.ladspa_plugin_info ());
	refiller (plugs, mgr.lv2_plugin_info ());
	refiller (plugs, mgr.lua_plugin_info ());

	switch (plugin_list_mode ()) {
		default:
			/* use favorites as-is */
			break;
		case PLM_TopHits:
			{
				PluginChartsSorter cmp;
				plugs.sort (cmp);
				plugs.resize (std::min (plugs.size(), size_t(UIConfiguration::instance().get_max_plugin_chart())));
			}
			break;
		case PLM_Recent:
			{
				PluginRecentSorter cmp;
				plugs.sort (cmp);
				plugs.resize (std::min (plugs.size(), size_t(10)));
				plugs.resize (std::min (plugs.size(), size_t(UIConfiguration::instance().get_max_plugin_recent())));
			}
			break;
	}
	plugin_list = plugs;

	sync_treeview_from_favorite_order ();
	//store_current_favorite_order ();
}

void
Mixer_UI::maybe_refill_favorite_plugins (PluginListMode plm)
{
	switch (plm) {
		case PLM_Favorite:
			if (plugin_list_mode () == PLM_Favorite) {
				refill_favorite_plugins();
			}
			break;
		default:
			if (plugin_list_mode () != PLM_Favorite) {
				refill_favorite_plugins();
			}
			break;
	}
}

void
Mixer_UI::sync_treeview_favorite_ui_state (const TreeModel::Path& path, const TreeModel::iterator&)
{
	TreeIter iter;
	if (!(iter = favorite_plugins_model->get_iter (path))) {
		return;
	}
	ARDOUR::PluginPresetPtr ppp = (*iter)[favorite_plugins_columns.plugin];
	if (!ppp) {
		return;
	}
	PluginInfoPtr pip = ppp->_pip;
	if (favorite_ui_state.find (pip->unique_id) != favorite_ui_state.end ()) {
		if (favorite_ui_state[pip->unique_id]) {
			favorite_plugins_display.expand_row (path, true);
		}
	}
}

void
Mixer_UI::sync_treeview_from_favorite_order ()
{
	PBD::Unwinder<bool> uw (ignore_plugin_reorder, true);
	switch (plugin_list_mode ()) {
		case PLM_Favorite:
			{
				PluginUIOrderSorter cmp (favorite_ui_order);
				plugin_list.sort (cmp);
			}
			break;
		case PLM_TopHits:
			{
				PluginABCSorter cmp;
				plugin_list.sort (cmp);
			}
		case PLM_Recent:
			break;
	}

	favorite_plugins_model->clear ();
	for (PluginInfoList::const_iterator i = plugin_list.begin(); i != plugin_list.end(); ++i) {
		PluginInfoPtr pip = (*i);

		TreeModel::Row newrow = *(favorite_plugins_model->append());
		newrow[favorite_plugins_columns.name] = (*i)->name;
		newrow[favorite_plugins_columns.plugin] = PluginPresetPtr (new PluginPreset(pip));
		if (!_session) {
			continue;
		}

		vector<ARDOUR::Plugin::PresetRecord> presets = (*i)->get_presets (true);
		for (vector<ARDOUR::Plugin::PresetRecord>::const_iterator j = presets.begin(); j != presets.end(); ++j) {
			if (!(*j).user) {
				continue;
			}
			Gtk::TreeModel::Row child_row = *(favorite_plugins_model->append (newrow.children()));
			child_row[favorite_plugins_columns.name] = (*j).label;
			child_row[favorite_plugins_columns.plugin] = PluginPresetPtr (new PluginPreset(pip, &(*j)));
		}
		if (favorite_ui_state.find (pip->unique_id) != favorite_ui_state.end ()) {
			if (favorite_ui_state[pip->unique_id]) {
				favorite_plugins_display.expand_row (favorite_plugins_model->get_path(newrow), true);
			}
		}
	}
}

void
Mixer_UI::popup_note_context_menu (GdkEventButton *ev)
{
	using namespace Gtk::Menu_Helpers;

	Gtk::Menu* m = ARDOUR_UI::instance()->shared_popup_menu ();
	MenuList& items = m->items ();

	if (_selection.axes.empty()) {
		items.push_back (MenuElem (_("No Track/Bus is selected.")));
	} else {
		items.push_back (MenuElem (_("Add at the top"),
					sigc::bind (sigc::mem_fun (*this, &Mixer_UI::add_selected_processor), AddTop)));
		items.push_back (MenuElem (_("Add Pre-Fader"),
					sigc::bind (sigc::mem_fun (*this, &Mixer_UI::add_selected_processor), AddPreFader)));
		items.push_back (MenuElem (_("Add Post-Fader"),
					sigc::bind (sigc::mem_fun (*this, &Mixer_UI::add_selected_processor), AddPostFader)));
		items.push_back (MenuElem (_("Add at the end"),
					sigc::bind (sigc::mem_fun (*this, &Mixer_UI::add_selected_processor), AddBottom)));
	}

	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Remove from favorites"), sigc::mem_fun (*this, &Mixer_UI::remove_selected_from_favorites)));

	ARDOUR::PluginPresetPtr ppp = selected_plugin();
	if (ppp && ppp->_preset.valid && ppp->_preset.user) {
		// we cannot currently delete AU presets
		if (!ppp->_pip || ppp->_pip->type != AudioUnit) {
			items.push_back (MenuElem (_("Delete Preset"), sigc::mem_fun (*this, &Mixer_UI::delete_selected_preset)));
		}
	}

	m->popup (ev->button, ev->time);
}

bool
Mixer_UI::plugin_row_button_press (GdkEventButton *ev)
{
	if ((ev->type == GDK_BUTTON_PRESS) && (ev->button == 3)) {
		TreeModel::Path path;
		TreeViewColumn* column;
		int cellx, celly;
		if (favorite_plugins_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
			Glib::RefPtr<Gtk::TreeView::Selection> selection = favorite_plugins_display.get_selection();
			if (selection) {
				selection->unselect_all();
				selection->select(path);
			}
		}
		ARDOUR::PluginPresetPtr ppp = selected_plugin();
		if (ppp) {
			popup_note_context_menu (ev);
		}
	}
	return false;
}


PluginPresetPtr
Mixer_UI::selected_plugin ()
{
	Glib::RefPtr<Gtk::TreeView::Selection> selection = favorite_plugins_display.get_selection();
	if (!selection) {
		return PluginPresetPtr();
	}
	Gtk::TreeModel::iterator iter = selection->get_selected();
	if (!iter) {
		return PluginPresetPtr();
	}
	return (*iter)[favorite_plugins_columns.plugin];
}

void
Mixer_UI::add_selected_processor (ProcessorPosition pos)
{
	ARDOUR::PluginPresetPtr ppp = selected_plugin();
	if (ppp) {
		add_favorite_processor (ppp, pos);
	}
}

void
Mixer_UI::delete_selected_preset ()
{
	if (!_session) {
		return;
	}
	ARDOUR::PluginPresetPtr ppp = selected_plugin();
	if (!ppp || !ppp->_preset.valid || !ppp->_preset.user) {
		return;
	}
	PluginPtr plugin = ppp->_pip->load (*_session);
	plugin->get_presets();
	plugin->remove_preset (ppp->_preset.label);
}

void
Mixer_UI::remove_selected_from_favorites ()
{
	ARDOUR::PluginPresetPtr ppp = selected_plugin();
	if (!ppp) {
		return;
	}
	PluginManager::PluginStatusType status = PluginManager::Normal;
	PluginManager& manager (PluginManager::instance());

	manager.set_status (ppp->_pip->type, ppp->_pip->unique_id, status);
	manager.save_statuses ();
}

void
Mixer_UI::plugin_row_activated (const TreeModel::Path& path, TreeViewColumn* column)
{
	TreeIter iter;
	if (!(iter = favorite_plugins_model->get_iter (path))) {
		return;
	}
	ARDOUR::PluginPresetPtr ppp = (*iter)[favorite_plugins_columns.plugin];
	add_favorite_processor (ppp, AddPreFader); // TODO: preference?!
}

void
Mixer_UI::add_favorite_processor (ARDOUR::PluginPresetPtr ppp, ProcessorPosition pos)
{
	if (!_session || _selection.axes.empty()) {
		return;
	}

	PluginInfoPtr pip = ppp->_pip;
	for (AxisViewSelection::iterator i = _selection.axes.begin(); i != _selection.axes.end(); ++i) {
		boost::shared_ptr<ARDOUR::Route> rt = boost::dynamic_pointer_cast<ARDOUR::Route> ((*i)->stripable());

		if (!rt) {
			continue;
		}

		PluginPtr p = pip->load (*_session);

		if (!p) {
			continue;
		}

		if (ppp->_preset.valid) {
			p->load_preset (ppp->_preset);
		}

		Route::ProcessorStreams err;
		boost::shared_ptr<Processor> processor (new PluginInsert (*_session, rt->time_domain(), p));

		switch (pos) {
			case AddTop:
				rt->add_processor_by_index (processor, 0, &err, Config->get_new_plugins_active ());
				break;
			case AddPreFader:
				rt->add_processor (processor, PreFader, &err, Config->get_new_plugins_active ());
				break;
			case AddPostFader:
				{
					int idx = 0;
					int pos = 0;
					for (;;++idx) {
						boost::shared_ptr<Processor> np = rt->nth_processor (idx);
						if (!np) {
							break;
						}
						if (!np->display_to_user()) {
							continue;
						}
						if (boost::dynamic_pointer_cast<Amp> (np) && // Fader, not Trim
								boost::dynamic_pointer_cast<Amp> (np)->gain_control()->parameter().type() == GainAutomation) {
							break;
						}
						++pos;
					}
					rt->add_processor_by_index (processor, ++pos, &err, Config->get_new_plugins_active ());
				}
				break;
			case AddBottom:
				rt->add_processor_by_index (processor, -1, &err, Config->get_new_plugins_active ());
				break;
		}
	}
}

bool
PluginTreeStore::row_drop_possible_vfunc(const Gtk::TreeModel::Path& dest, const Gtk::SelectionData& data) const
{
	if (data.get_target() != "GTK_TREE_MODEL_ROW") {
		return false;
	}

	// only allow to re-order top-level items
	TreePath src;
	if (TreePath::get_from_selection_data (data, src)) {
		if (src.up() && src.up()) {
			return false;
		}
	}

	// don't allow to drop as child-rows.
	Gtk::TreeModel::Path _dest = dest; // un const
	const bool is_child = _dest.up (); // explicit bool for clang
	if (!is_child || _dest.empty ()) {
		return true;
	}
	return false;
}

bool
Mixer_UI::plugin_drag_motion (const Glib::RefPtr<Gdk::DragContext>& ctx, int x, int y, guint time)
{
	std::string target = favorite_plugins_display.drag_dest_find_target (ctx, favorite_plugins_display.drag_dest_get_target_list());

  if (target.empty()) {
		ctx->drag_status (Gdk::DragAction (0), time);
    return false;
	}

	if (target == "GTK_TREE_MODEL_ROW") {
		if (plugin_list_mode () == PLM_Favorite && plugin_search_entry.get_text ().empty()) {
			/* re-order rows */
			ctx->drag_status (Gdk::ACTION_MOVE, time);
			return true;
		}
	} else if (target == "x-ardour/plugin.preset") {
		ctx->drag_status (Gdk::ACTION_COPY, time);
		//favorite_plugins_mode_combo.set_active_text (_("Favorite Plugins"));
		return true;
	}

	ctx->drag_status (Gdk::DragAction (0), time);
	return false;
}

void
Mixer_UI::plugin_drop (const Glib::RefPtr<Gdk::DragContext>&, const Gtk::SelectionData& data)
{
	if (data.get_target() != "x-ardour/plugin.preset") {
		return;
	}
	if (data.get_length() != sizeof (PluginPresetPtr)) {
		return;
	}

	const void *d = data.get_data();
	const PluginPresetPtr ppp = *(static_cast<const PluginPresetPtr*> (d));

	PluginManager::PluginStatusType status = PluginManager::Favorite;
	PluginManager& manager (PluginManager::instance());

	manager.set_status (ppp->_pip->type, ppp->_pip->unique_id, status);
	manager.save_statuses ();
}

void
Mixer_UI::do_vca_assign (boost::shared_ptr<VCA> vca)
{
	/* call protected MixerActor:: method */
	vca_assign (vca);
}

void
Mixer_UI::do_vca_unassign (boost::shared_ptr<VCA> vca)
{
	/* call protected MixerActor:: method */
	vca_unassign (vca);
}

void
Mixer_UI::show_spill (boost::shared_ptr<Stripable> s)
{
	boost::shared_ptr<Stripable> ss = spilled_strip.lock();
	if (ss == s) {
		return;
	}

	spilled_strip = s;
	_spill_gone_connection.disconnect ();
	show_spill_change (s); /* EMIT SIGNAL */

	if (s) {
		s->DropReferences.connect (_spill_gone_connection, invalidator (*this), boost::bind (&Mixer_UI::spill_nothing, this), gui_context());
		_group_tabs->set_sensitive (false);
	} else {
		_group_tabs->set_sensitive (true);
	}
	redisplay_track_list ();
}

void
Mixer_UI::spill_nothing ()
{
	show_spill (boost::shared_ptr<Stripable> ());
}

bool
Mixer_UI::showing_spill_for (boost::shared_ptr<Stripable> s) const
{
	return s == spilled_strip.lock();
}

void
Mixer_UI::register_actions ()
{
	Glib::RefPtr<ActionGroup> group = ActionManager::create_action_group (bindings, X_("Mixer"));

	ActionManager::register_action (group, "solo", _("Toggle Solo on Mixer-Selected Tracks/Busses"), sigc::mem_fun (*this, &Mixer_UI::solo_action));
	ActionManager::register_action (group, "mute", _("Toggle Mute on Mixer-Selected Tracks/Busses"), sigc::mem_fun (*this, &Mixer_UI::mute_action));
	ActionManager::register_action (group, "recenable", _("Toggle Rec-enable on Mixer-Selected Tracks/Busses"), sigc::mem_fun (*this, &Mixer_UI::rec_enable_action));
	ActionManager::register_action (group, "increment-gain", _("Decrease Gain on Mixer-Selected Tracks/Busses"), sigc::mem_fun (*this, &Mixer_UI::step_gain_up_action));
	ActionManager::register_action (group, "decrement-gain", _("Increase Gain on Mixer-Selected Tracks/Busses"), sigc::mem_fun (*this, &Mixer_UI::step_gain_down_action));
	ActionManager::register_action (group, "unity-gain", _("Set Gain to 0dB on Mixer-Selected Tracks/Busses"), sigc::mem_fun (*this, &Mixer_UI::unity_gain_action));


	ActionManager::register_action (group, "copy-processors", _("Copy Selected Processors"), sigc::mem_fun (*this, &Mixer_UI::copy_processors));
	ActionManager::register_action (group, "cut-processors", _("Cut Selected Processors"), sigc::mem_fun (*this, &Mixer_UI::cut_processors));
	ActionManager::register_action (group, "paste-processors", _("Paste Selected Processors"), sigc::mem_fun (*this, &Mixer_UI::paste_processors));
	ActionManager::register_action (group, "delete-processors", _("Delete Selected Processors"), sigc::mem_fun (*this, &Mixer_UI::delete_processors));
	ActionManager::register_action (group, "select-all-processors", _("Select All (visible) Processors"), sigc::mem_fun (*this, &Mixer_UI::select_all_processors));
	ActionManager::register_action (group, "toggle-processors", _("Toggle Selected Processors"), sigc::mem_fun (*this, &Mixer_UI::toggle_processors));
	ActionManager::register_action (group, "ab-plugins", _("Toggle Selected Plugins"), sigc::mem_fun (*this, &Mixer_UI::ab_plugins));
	ActionManager::register_action (group, "select-none", _("Deselect all strips and processors"), sigc::mem_fun (*this, &Mixer_UI::select_none));

	ActionManager::register_action (group, "select-next-stripable", _("Select Next Mixer Strip"), sigc::mem_fun (*this, &Mixer_UI::select_next_strip));
	ActionManager::register_action (group, "select-prev-stripable", _("Select Previous Mixer Strip"), sigc::mem_fun (*this, &Mixer_UI::select_prev_strip));

	ActionManager::register_action (group, "scroll-left", _("Scroll Mixer Window to the left"), sigc::mem_fun (*this, &Mixer_UI::scroll_left));
	ActionManager::register_action (group, "scroll-right", _("Scroll Mixer Window to the right"), sigc::mem_fun (*this, &Mixer_UI::scroll_right));

	ActionManager::register_action (group, "toggle-midi-input-active", _("Toggle MIDI Input Active for Mixer-Selected Tracks/Busses"),
	                           sigc::bind (sigc::mem_fun (*this, &Mixer_UI::toggle_midi_input_active), false));

	ActionManager::register_toggle_action (group, X_("ToggleMixerList"), _("Mixer: Show Mixer List"), sigc::mem_fun (*this, &Mixer_UI::toggle_mixer_list));

	ActionManager::register_toggle_action (group, X_("ToggleVCAPane"), _("Mixer: Show VCAs"), sigc::mem_fun (*this, &Mixer_UI::toggle_vcas));

#ifdef MIXBUS
	ActionManager::register_toggle_action (group, X_("ToggleMixbusPane"), _("Mixer: Show Mixbusses"), sigc::mem_fun (*this, &Mixer_UI::toggle_mixbuses));
#endif

	ActionManager::register_toggle_action (group, X_("ToggleMonitorSection"), _("Mixer: Show Monitor Section"), sigc::mem_fun (*this, &Mixer_UI::toggle_monitor_section));

	ActionManager::register_toggle_action (group, X_("ToggleFoldbackStrip"), _("Mixer: Show Foldback Strip"), sigc::mem_fun (*this, &Mixer_UI::toggle_foldback_strip));

	ActionManager::register_toggle_action (group, X_("toggle-disk-monitor"), _("Toggle Disk Monitoring"), sigc::bind (sigc::mem_fun (*this, &Mixer_UI::toggle_monitor_action), MonitorDisk, false, false));
	ActionManager::register_toggle_action (group, X_("toggle-input-monitor"), _("Toggle Input Monitoring"), sigc::bind (sigc::mem_fun (*this, &Mixer_UI::toggle_monitor_action), MonitorInput, false, false));
}

void
Mixer_UI::load_bindings ()
{
	bindings = Bindings::get_bindings (X_("Mixer"));
}

template<class T> void
Mixer_UI::control_action (boost::shared_ptr<T> (Stripable::*get_control)() const)
{
	boost::shared_ptr<ControlList> cl (new ControlList);
	boost::shared_ptr<AutomationControl> ac;
	bool val = false;
	bool have_val = false;

	set_axis_targets_for_operation ();

	BOOST_FOREACH(AxisView* r, _axis_targets) {
		boost::shared_ptr<Stripable> s = r->stripable();
		if (s) {
			ac = (s.get()->*get_control)();
			if (ac) {
				ac->start_touch (timepos_t (_session->audible_sample ()));
				cl->push_back (ac);
				if (!have_val) {
					val = !ac->get_value();
					have_val = true;
				}
			}
		}
	}

	_session->set_controls (cl,  val, Controllable::UseGroup);
}

void
Mixer_UI::solo_action ()
{
	control_action (&Stripable::solo_control);
}

void
Mixer_UI::mute_action ()
{
	control_action (&Stripable::mute_control);
}

void
Mixer_UI::rec_enable_action ()
{
	control_action (&Stripable::rec_enable_control);
}

AutomationControlSet
Mixer_UI::selected_gaincontrols ()
{
	set_axis_targets_for_operation ();
	AutomationControlSet rv;
	BOOST_FOREACH(AxisView* r, _axis_targets) {
		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);
		if (ms) {
			boost::shared_ptr<GainControl> ac (ms->route()->gain_control());
			ControlList cl (ac->grouped_controls());
			for (ControlList::const_iterator c = cl.begin(); c != cl.end (); ++c) {
				rv.insert (*c);
			}
			rv.insert (ac);
		}
	}
	return rv;
}

void
Mixer_UI::step_gain_up_action ()
{
	AutomationControlSet acs = selected_gaincontrols ();
	for (AutomationControlSet::const_iterator i = acs.begin(); i != acs.end (); ++i) {
		boost::shared_ptr<GainControl> ac = boost::dynamic_pointer_cast<GainControl> (*i);
		assert (ac);
		ac->set_value (dB_to_coefficient (accurate_coefficient_to_dB (ac->get_value()) + 0.1), Controllable::NoGroup);
	}
}

void
Mixer_UI::step_gain_down_action ()
{
	AutomationControlSet acs = selected_gaincontrols ();
	for (AutomationControlSet::const_iterator i = acs.begin(); i != acs.end (); ++i) {
		boost::shared_ptr<GainControl> ac = boost::dynamic_pointer_cast<GainControl> (*i);
		assert (ac);
		ac->set_value (dB_to_coefficient (accurate_coefficient_to_dB (ac->get_value()) - 0.1), Controllable::NoGroup);
	}
}

void
Mixer_UI::unity_gain_action ()
{
	set_axis_targets_for_operation ();

	BOOST_FOREACH(AxisView* r, _axis_targets) {
		boost::shared_ptr<Stripable> s = r->stripable();
		if (s) {
			boost::shared_ptr<AutomationControl> ac = s->gain_control();
			if (ac) {
				ac->set_value (1.0, Controllable::UseGroup);
			}
		}
	}
}

void
Mixer_UI::copy_processors ()
{
	set_axis_targets_for_operation ();

	BOOST_FOREACH(AxisView* r, _axis_targets) {
		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);
		if (ms) {
			ms->copy_processors ();
		}
	}
}
void
Mixer_UI::cut_processors ()
{
	set_axis_targets_for_operation ();

	BOOST_FOREACH(AxisView* r, _axis_targets) {
		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);
		if (ms) {
			ms->cut_processors ();
		}
	}
}
void
Mixer_UI::paste_processors ()
{
	set_axis_targets_for_operation ();

	BOOST_FOREACH(AxisView* r, _axis_targets) {
		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);
		if (ms) {
			ms->paste_processors ();
		}
	}
}
void
Mixer_UI::select_all_processors ()
{
	set_axis_targets_for_operation ();

	BOOST_FOREACH(AxisView* r, _axis_targets) {
		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);
		if (ms) {
			ms->select_all_processors ();
		}
	}
}
void
Mixer_UI::toggle_processors ()
{
	set_axis_targets_for_operation ();

	BOOST_FOREACH(AxisView* r, _axis_targets) {
		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);
		if (ms) {
			ms->toggle_processors ();
		}
	}
}
void
Mixer_UI::ab_plugins ()
{
	set_axis_targets_for_operation ();

	BOOST_FOREACH(AxisView* r, _axis_targets) {
		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);
		if (ms) {
			ms->ab_plugins ();
		}
	}
}

void
Mixer_UI::vca_assign (boost::shared_ptr<VCA> vca)
{
	set_axis_targets_for_operation ();
	BOOST_FOREACH(AxisView* r, _axis_targets) {
		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);
		if (ms) {
			ms->vca_assign (vca);
		}
	}
}

void
Mixer_UI::vca_unassign (boost::shared_ptr<VCA> vca)
{
	set_axis_targets_for_operation ();
	BOOST_FOREACH(AxisView* r, _axis_targets) {
		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);
		if (ms) {
			ms->vca_unassign (vca);
		}
	}
}

bool
Mixer_UI::screenshot (std::string const& filename)
{
	if (!_session) {
		return false;
	}

	int height = strip_packer.get_height();
	bool with_vca = vca_vpacker.is_visible ();
	MixerStrip* master = strip_by_route (_session->master_out ());

	Gtk::OffscreenWindow osw;
	Gtk::HBox b;
	osw.add (b);
	b.show ();

	/* unpack widgets, add to OffscreenWindow */

	strip_group_box.remove (strip_packer);
	b.pack_start (strip_packer, false, false);
	/* hide extra elements inside strip_packer */
	scroller_base.hide ();
#ifdef MIXBUS
	mb_shadow.hide();
#endif

	if (with_vca) {
		/* work around Gtk::ScrolledWindow */
		Gtk::Viewport* viewport = (Gtk::Viewport*) vca_scroller.get_child();
		viewport->remove (); // << vca_hpacker
		b.pack_start (vca_hpacker, false, false);
		/* hide some growing widgets */
		vca_scroller_base.hide();
	}

	if (master) {
		out_packer.remove (*master);
		b.pack_start (*master, false, false);
		master->hide_master_spacer (true);
	}

	/* prepare the OffscreenWindow for rendering */
	osw.set_size_request (-1, height);
	osw.show ();
	osw.queue_resize ();
	osw.queue_draw ();
	osw.get_window()->process_updates (true);

	/* create screenshot */
	Glib::RefPtr<Gdk::Pixbuf> pb = osw.get_pixbuf ();
	pb->save (filename, "png");

	/* unpack elements before destroying the Box & OffscreenWindow */
	list<Gtk::Widget*> children = b.get_children();
	for (list<Gtk::Widget*>::iterator child = children.begin(); child != children.end(); ++child) {
		b.remove (**child);
	}
	osw.remove ();

	/* now re-pack the widgets into the main mixer window */
	scroller_base.show ();
#ifdef MIXBUS
	mb_shadow.show();
#endif
	strip_group_box.pack_start (strip_packer);
	if (with_vca) {
		vca_scroller_base.show();
		vca_scroller.add (vca_hpacker);
	}
	if (master) {
		master->hide_master_spacer (false);
		out_packer.pack_start (*master, false, false);
	}
	return true;
}

void
Mixer_UI::toggle_monitor_action (MonitorChoice monitor_choice, bool group_override, bool all)
{
	MonitorChoice mc;
	boost::shared_ptr<RouteList> rl;

	for (AxisViewSelection::iterator i = _selection.axes.begin(); i != _selection.axes.end(); ++i) {
		boost::shared_ptr<ARDOUR::Route> rt = boost::dynamic_pointer_cast<ARDOUR::Route> ((*i)->stripable());

		if (!rt->monitoring_control ()) {
			/* skip busses */
			continue;
		}

		if (rt->monitoring_control()->monitoring_choice() & monitor_choice) {
			mc = MonitorChoice (rt->monitoring_control()->monitoring_choice() & ~monitor_choice);
		} else {
			mc = MonitorChoice (rt->monitoring_control()->monitoring_choice() | monitor_choice);
		}

		if (all) {
			/* Primary-Tertiary-click applies change to all routes */
			rl = _session->get_routes ();
			_session->set_controls (route_list_to_control_list (rl, &Stripable::monitoring_control), (double) mc, Controllable::NoGroup);
		} else if (group_override) {
			rl.reset (new RouteList);
			rl->push_back (rt);
			_session->set_controls (route_list_to_control_list (rl, &Stripable::monitoring_control), (double) mc, Controllable::InverseGroup);
		} else {
			rl.reset (new RouteList);
			rl->push_back (rt);
			_session->set_controls (route_list_to_control_list (rl, &Stripable::monitoring_control), (double) mc, Controllable::UseGroup);
		}

	}
}
