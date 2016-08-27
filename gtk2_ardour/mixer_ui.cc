/*
    Copyright (C) 2000-2004 Paul Davis

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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <algorithm>
#include <map>
#include <sigc++/bind.h>

#include <boost/foreach.hpp>

#include <gtkmm/accelmap.h>

#include "pbd/convert.h"
#include "pbd/stacktrace.h"
#include "pbd/unwind.h"

#include <glibmm/threads.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/keyboard.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/tearoff.h>
#include <gtkmm2ext/window_title.h>
#include <gtkmm2ext/doi.h>

#include "ardour/amp.h"
#include "ardour/debug.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"
#include "ardour/plugin_manager.h"
#include "ardour/route_group.h"
#include "ardour/session.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "keyboard.h"
#include "mixer_ui.h"
#include "mixer_strip.h"
#include "monitor_section.h"
#include "plugin_selector.h"
#include "public_editor.h"
#include "mouse_cursors.h"
#include "ardour_ui.h"
#include "prompter.h"
#include "utils.h"
#include "route_sorter.h"
#include "actions.h"
#include "gui_thread.h"
#include "mixer_group_tabs.h"
#include "route_sorter.h"
#include "timers.h"
#include "ui_config.h"
#include "vca_master_strip.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
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
	: Tabbable (_content, _("Mixer"))
	, no_track_list_redisplay (false)
	, in_group_row_change (false)
	, track_menu (0)
	, _monitor_section (0)
	, _plugin_selector (0)
	, _strip_width (UIConfiguration::instance().get_default_narrow_ms() ? Narrow : Wide)
	, ignore_reorder (false)
        , _in_group_rebuild_or_clear (false)
        , _route_deletion_in_progress (false)
	, _maximised (false)
	, _show_mixer_list (true)
	, myactions (X_("mixer"))
{
	register_actions ();
	load_bindings ();
	_content.set_data ("ardour-bindings", bindings);

	PresentationInfo::Change.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::presentation_info_changed, this, _1), gui_context());

	scroller.set_can_default (true);
	// set_default (scroller);

	scroller_base.set_flags (Gtk::CAN_FOCUS);
	scroller_base.add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	scroller_base.set_name ("MixerWindow");
	scroller_base.signal_button_release_event().connect (sigc::mem_fun(*this, &Mixer_UI::strip_scroller_button_release));

	/* set up drag-n-drop */
	vector<TargetEntry> target_table;
	target_table.push_back (TargetEntry ("PluginFavoritePtr"));
	scroller_base.drag_dest_set (target_table);
	scroller_base.signal_drag_data_received().connect (sigc::mem_fun(*this, &Mixer_UI::scroller_drag_data_received));

	// add as last item of strip packer
	strip_packer.pack_end (scroller_base, true, true);

	_group_tabs = new MixerGroupTabs (this);
	VBox* b = manage (new VBox);
	b->set_spacing (0);
	b->set_border_width (0);
	b->pack_start (*_group_tabs, PACK_SHRINK);
	b->pack_start (strip_packer);
	b->show_all ();
	b->signal_scroll_event().connect (sigc::mem_fun (*this, &Mixer_UI::on_scroll_event), false);

	scroller.add (*b);
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

	HBox* route_group_display_button_box = manage (new HBox());

	Button* route_group_add_button = manage (new Button ());
	Button* route_group_remove_button = manage (new Button ());

	Widget* w;

	w = manage (new Image (Stock::ADD, ICON_SIZE_BUTTON));
	w->show();
	route_group_add_button->add (*w);

	w = manage (new Image (Stock::REMOVE, ICON_SIZE_BUTTON));
	w->show();
	route_group_remove_button->add (*w);

	route_group_display_button_box->set_homogeneous (true);

	route_group_add_button->signal_clicked().connect (sigc::mem_fun (*this, &Mixer_UI::new_route_group));
	route_group_remove_button->signal_clicked().connect (sigc::mem_fun (*this, &Mixer_UI::remove_selected_route_group));

	route_group_display_button_box->add (*route_group_add_button);
	route_group_display_button_box->add (*route_group_remove_button);

	group_display_vbox.pack_start (group_display_scroller, true, true);
	group_display_vbox.pack_start (*route_group_display_button_box, false, false);

	group_display_frame.set_name ("BaseFrame");
	group_display_frame.set_shadow_type (Gtk::SHADOW_IN);
	group_display_frame.add (group_display_vbox);


	list<TargetEntry> target_list;
	target_list.push_back (TargetEntry ("PluginPresetPtr"));

	favorite_plugins_model = PluginTreeStore::create (favorite_plugins_columns);
	favorite_plugins_display.set_model (favorite_plugins_model);
	favorite_plugins_display.append_column (_("Favorite Plugins"), favorite_plugins_columns.name);
	favorite_plugins_display.set_name ("EditGroupList");
	favorite_plugins_display.get_selection()->set_mode (Gtk::SELECTION_SINGLE);
	favorite_plugins_display.set_reorderable (false);
	favorite_plugins_display.set_headers_visible (true);
	favorite_plugins_display.set_rules_hint (true);
	favorite_plugins_display.set_can_focus (false);
	favorite_plugins_display.set_tooltip_column (0);
	favorite_plugins_display.add_object_drag (favorite_plugins_columns.plugin.index(), "PluginFavoritePtr");
	favorite_plugins_display.set_drag_column (favorite_plugins_columns.name.index());
	favorite_plugins_display.add_drop_targets (target_list);
	favorite_plugins_display.signal_row_activated().connect (sigc::mem_fun (*this, &Mixer_UI::plugin_row_activated));
	favorite_plugins_display.signal_button_press_event().connect (sigc::mem_fun (*this, &Mixer_UI::plugin_row_button_press), false);
	favorite_plugins_display.signal_drop.connect (sigc::mem_fun (*this, &Mixer_UI::plugin_drop));
	favorite_plugins_display.signal_row_expanded().connect (sigc::mem_fun (*this, &Mixer_UI::save_favorite_ui_state));
	favorite_plugins_display.signal_row_collapsed().connect (sigc::mem_fun (*this, &Mixer_UI::save_favorite_ui_state));
	favorite_plugins_model->signal_row_has_child_toggled().connect (sigc::mem_fun (*this, &Mixer_UI::sync_treeview_favorite_ui_state));

	favorite_plugins_scroller.add (favorite_plugins_display);
	favorite_plugins_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	favorite_plugins_frame.set_name ("BaseFrame");
	favorite_plugins_frame.set_shadow_type (Gtk::SHADOW_IN);
	favorite_plugins_frame.add (favorite_plugins_scroller);

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
	vca_hpacker.pack_end (vca_scroller_base, true, true);

	vca_scroller.add (vca_hpacker);
	vca_scroller.set_policy (Gtk::POLICY_ALWAYS, Gtk::POLICY_AUTOMATIC);
	vca_scroller.signal_button_release_event().connect (sigc::mem_fun(*this, &Mixer_UI::strip_scroller_button_release));

	vca_vpacker.pack_start (vca_scroller, true, true);

	inner_pane.add (scroller);
	inner_pane.add (vca_vpacker);

	global_hpacker.pack_start (inner_pane, true, true);
	global_hpacker.pack_start (out_packer, false, false);

	list_hpane.set_check_divider_position (true);
	list_hpane.add (list_vpacker);
	list_hpane.add (global_hpacker);
	list_hpane.set_child_minsize (list_vpacker, 1);


	XMLNode const * settings = ARDOUR_UI::instance()->mixer_settings();
	float fract;

	{
		LocaleGuard lg;

		if (!settings || !settings->get_property ("mixer-rhs-pane1-pos", fract) || fract  > 1.0) {
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
	}

	rhs_pane1.set_drag_cursor (*PublicEditor::instance().cursors()->expand_up_down);
	rhs_pane2.set_drag_cursor (*PublicEditor::instance().cursors()->expand_up_down);
	list_hpane.set_drag_cursor (*PublicEditor::instance().cursors()->expand_left_right);
	inner_pane.set_drag_cursor (*PublicEditor::instance().cursors()->expand_left_right);

	_content.pack_start (list_hpane, true, true);

	update_title ();

	route_group_display_button_box->show();
	route_group_add_button->show();
	route_group_remove_button->show();

	_content.show ();
	_content.set_name ("MixerWindow");

	global_hpacker.show();
	scroller.show();
	scroller_base.show();
	scroller_hpacker.show();
	mixer_scroller_vpacker.show();
	list_vpacker.show();
	group_display_button_label.show();
	group_display_button.show();
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

	MixerStrip::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::remove_strip, this, _1), gui_context());

	/* handle escape */

	ARDOUR_UI::instance()->Escape.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::escape, this), gui_context());

#ifndef DEFER_PLUGIN_SELECTOR_LOAD
	_plugin_selector = new PluginSelector (PluginManager::instance ());
#else
#error implement deferred Plugin-Favorite list
#endif
	PluginManager::instance ().PluginListChanged.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::refill_favorite_plugins, this), gui_context());
	PluginManager::instance ().PluginStatusesChanged.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::refill_favorite_plugins, this), gui_context());
	ARDOUR::Plugin::PresetsChanged.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::refill_favorite_plugins, this), gui_context());
}

Mixer_UI::~Mixer_UI ()
{
	if (_monitor_section) {
		monitor_section_detached ();
		delete _monitor_section;
	}
	delete _plugin_selector;
	delete track_menu;
}

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

	slist.sort (StripablePresentationInfoSorter());

	for (Gtk::TreeModel::Children::iterator it = track_model->children().begin(); it != track_model->children().end(); ++it) {
		boost::shared_ptr<Stripable> s = (*it)[stripable_columns.stripable];

		if (!s) {
			continue;
		}

		nroutes++;

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

				vms->CatchDeletion.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::remove_master, this, _1), gui_context());

			} else if ((route = boost::dynamic_pointer_cast<Route> (*s))) {

				if (route->is_auditioner()) {
					continue;
				}

				if (route->is_monitor()) {

					if (!_monitor_section) {
						_monitor_section = new MonitorSection (_session);

						XMLNode* mnode = ARDOUR_UI::instance()->tearoff_settings (X_("monitor-section"));
						if (mnode) {
							_monitor_section->tearoff().set_state (*mnode);
						}
					}

					out_packer.pack_end (_monitor_section->tearoff(), false, false);
					_monitor_section->set_session (_session);
					_monitor_section->tearoff().show_all ();

					_monitor_section->tearoff().Detach.connect (sigc::mem_fun(*this, &Mixer_UI::monitor_section_detached));
					_monitor_section->tearoff().Attach.connect (sigc::mem_fun(*this, &Mixer_UI::monitor_section_attached));

					monitor_section_attached ();

					route->DropReferences.connect (*this, invalidator(*this), boost::bind (&Mixer_UI::monitor_section_going_away, this), gui_context());

					/* no regular strip shown for control out */

					continue;
				}

				strip = new MixerStrip (*this, _session, route);
				strips.push_back (strip);

				UIConfiguration::instance().get_default_narrow_ms() ? _strip_width = Narrow : _strip_width = Wide;

				if (strip->width_owner() != strip) {
					strip->set_width_enum (_strip_width, this);
				}

				show_strip (strip);

				if (!route->is_master()) {

					TreeModel::Row row = *(track_model->insert (insert_iter));

					row[stripable_columns.text] = route->name();
					row[stripable_columns.visible] = strip->marked_for_display();
					row[stripable_columns.stripable] = route;
					row[stripable_columns.strip] = strip;

				} else {

					out_packer.pack_start (*strip, false, false);
					strip->set_packed (true);
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
}

void
Mixer_UI::select_strip (MixerStrip& ms, bool add)
{
	if (add) {
		_selection.add (&ms);
	} else {
		_selection.set (&ms);
	}
}

void
Mixer_UI::select_none ()
{
	_selection.clear_routes();
	deselect_all_strip_processors();
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

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		if ((*ri)[stripable_columns.strip] == strip) {
                        PBD::Unwinder<bool> uw (_route_deletion_in_progress, true);
			track_model->erase (ri);
			break;
		}
	}
}

void
Mixer_UI::presentation_info_changed (PropertyChange const & what_changed)
{
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
	if (ignore_reorder || !_session || _session->deletion_in_progress()) {
		return;
	}

	TreeModel::Children rows = track_model->children();

	if (rows.empty()) {
		return;
	}

	DEBUG_TRACE (DEBUG::OrderKeys, "mixer sync presentation info from treeview\n");

	TreeModel::Children::iterator ri;
	bool change = false;
	uint32_t order = 0;

	OrderingKeys sorted;
	const size_t cmp_max = rows.size ();

	// special case master if it's got PI order 0 lets keep it there
	if (_session->master_out() && (_session->master_out()->presentation_info().order() == 0)) {
		order++;
	}

	PresentationInfo::ChangeSuspender cs;

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		bool visible = (*ri)[stripable_columns.visible];
		boost::shared_ptr<Stripable> stripable = (*ri)[stripable_columns.stripable];

		if (!stripable) {
			continue;
		}

		/* Monitor and Auditioner do not get their presentation
		 * info reset here.
		 */

		if (stripable->is_monitor() || stripable->is_auditioner()) {
			continue;
		}

		/* Master also doesn't get set here but since the editor allows
		 * it to be reordered, we need to preserve its ordering.
		 */

		stripable->presentation_info().set_hidden (!visible);

		// master may not get set here, but if it is zero keep it there
		if (stripable->is_master() && (stripable->presentation_info().order() == 0)) {
			continue;
		}

		if (order != stripable->presentation_info().order()) {
			stripable->set_presentation_order (order);
			change = true;
		}

		sorted.push_back (OrderKeys (order, stripable, cmp_max));

		++order;
	}

	if (!change) {
		// VCA (and Mixbus) special cases according to SortByNewDisplayOrder
		uint32_t n = 0;
		SortByNewDisplayOrder cmp;
		sort (sorted.begin(), sorted.end(), cmp);
		for (OrderingKeys::iterator sr = sorted.begin(); sr != sorted.end(); ++sr, ++n) {
			if (_session->master_out() && (_session->master_out()->presentation_info().order() == n)) {
				++n;
			}
			if (sr->old_display_order != n) {
				change = true;
				break;
			}
		}
		if (change) {
			n = 0;
			for (OrderingKeys::iterator sr = sorted.begin(); sr != sorted.end(); ++sr, ++n) {
				if (_session->master_out() && (_session->master_out()->presentation_info().order() == n)) {
					++n;
				}
				if (sr->stripable->presentation_info().order() != n) {
					sr->stripable->set_presentation_order (n);
				}
			}
		}
	}

	if (change) {
		DEBUG_TRACE (DEBUG::OrderKeys, "... notify PI change from mixer GUI\n");
		_session->set_dirty();
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

	OrderingKeys sorted;
	const size_t cmp_max = rows.size ();

	for (TreeModel::Children::iterator ri = rows.begin(); ri != rows.end(); ++ri, ++old_order) {
		boost::shared_ptr<Stripable> stripable = (*ri)[stripable_columns.stripable];
		sorted.push_back (OrderKeys (old_order, stripable, cmp_max));
	}

	SortByNewDisplayOrder cmp;

	sort (sorted.begin(), sorted.end(), cmp);
	neworder.assign (sorted.size(), 0);

	uint32_t n = 0;

	for (OrderingKeys::iterator sr = sorted.begin(); sr != sorted.end(); ++sr, ++n) {

		neworder[n] = sr->old_display_order;

		if (sr->old_display_order != n) {
			changed = true;
		}
	}

	if (changed) {
		Unwinder<bool> uw (ignore_reorder, true);
		track_model->reorder (neworder);
	}

	if (what_changed.contains (Properties::selected)) {

		PresentationInfo::ChangeSuspender cs;

		for (list<MixerStrip *>::const_iterator i = strips.begin(); i != strips.end(); ++i) {
			boost::shared_ptr<Stripable> stripable = (*i)->stripable();
			if (stripable && stripable->presentation_info().selected()) {
				_selection.add (*i);
			} else {
				_selection.remove (*i);
			}
		}

		if (!_selection.axes.empty() && !PublicEditor::instance().track_selection_change_without_scroll ()) {
			move_stripable_into_view ((*_selection.axes.begin())->stripable());
		}
	}

	redisplay_track_list ();
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
Mixer_UI::axis_by_stripable (boost::shared_ptr<Stripable> s) const
{
	for (list<MixerStrip *>::const_iterator i = strips.begin(); i != strips.end(); ++i) {
		if ((*i)->stripable() == s) {
			return (*i);
		}
	}

	return 0;
}

bool
Mixer_UI::strip_button_release_event (GdkEventButton *ev, MixerStrip *strip)
{
	if (ev->button == 1) {
		if (_selection.selected (strip)) {
			/* primary-click: toggle selection state of strip */
			if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
				_selection.remove (strip);
			} else if (_selection.axes.size() > 1) {
				/* de-select others */
				_selection.set (strip);
			}
		} else {
			if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
				_selection.add (strip);
			} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::RangeSelectModifier)) {

				/* extend selection */

				vector<MixerStrip*> tmp;
				bool accumulate = false;
				bool found_another = false;

				OrderingKeys sorted;
				const size_t cmp_max = strips.size ();
				for (list<MixerStrip*>::iterator i = strips.begin(); i != strips.end(); ++i) {
					sorted.push_back (OrderKeys (-1, (*i)->stripable(), cmp_max));
				}
				SortByNewDisplayOrder cmp;
				sort (sorted.begin(), sorted.end(), cmp);

				for (OrderingKeys::iterator sr = sorted.begin(); sr != sorted.end(); ++sr) {
					MixerStrip* ms = strip_by_stripable (sr->stripable);
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
						_selection.add (*i);
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

void
Mixer_UI::set_session (Session* sess)
{
	SessionHandlePtr::set_session (sess);

	if (_plugin_selector) {
		_plugin_selector->set_session (_session);
	}

	_group_tabs->set_session (sess);

	if (_monitor_section) {
		_monitor_section->set_session (_session);
	}

	if (!_session) {
		return;
	}

	refill_favorite_plugins();

	XMLNode* node = ARDOUR_UI::instance()->mixer_settings();
	set_state (*node, 0);

	update_title ();

	initial_track_display ();

	_session->RouteAdded.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::add_routes, this, _1), gui_context());
	_session->route_group_added.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::add_route_group, this, _1), gui_context());
	_session->route_group_removed.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::route_groups_changed, this), gui_context());
	_session->route_groups_reordered.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::route_groups_changed, this), gui_context());
	_session->config.ParameterChanged.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::parameter_changed, this, _1), gui_context());
	_session->DirtyChanged.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::update_title, this), gui_context());
	_session->StateSaved.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::update_title, this), gui_context());

	_session->vca_manager().VCAAdded.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::add_masters, this, _1), gui_context());

	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::parameter_changed, this, _1), gui_context ());

	route_groups_changed ();

	if (_visible) {
		show_window();
	}
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

	if (_monitor_section) {
		_monitor_section->tearoff().hide_visible ();
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

		/* force presentation catch up with visibility changes
		 */

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
	sync_presentation_info_from_treeview ();

        if (_route_deletion_in_progress) {
                redisplay_track_list ();
        }
}

void
Mixer_UI::spill_redisplay (boost::shared_ptr<VCA> vca)
{
	TreeModel::Children rows = track_model->children();
	std::list<boost::shared_ptr<VCA> > vcas;
	vcas.push_back (vca);

	for (TreeModel::Children::const_iterator i = rows.begin(); i != rows.end(); ++i) {
		AxisView* av = (*i)[stripable_columns.strip];
		VCAMasterStrip* vms = dynamic_cast<VCAMasterStrip*> (av);
		if (vms && vms->vca()->slaved_to (vca)) {
			vcas.push_back (vms->vca());
		}
	}

	for (TreeModel::Children::const_iterator i = rows.begin(); i != rows.end(); ++i) {

		AxisView* av = (*i)[stripable_columns.strip];
		MixerStrip* strip = dynamic_cast<MixerStrip*> (av);
		bool const visible = (*i)[stripable_columns.visible];

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

		bool slaved = false;
		for (std::list<boost::shared_ptr<VCA> >::const_iterator m = vcas.begin(); m != vcas.end(); ++m) {
			if (strip->route()->slaved_to (*m)) {
				slaved = true;
				break;
			}
		}

		if (slaved && visible) {

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
			spill_redisplay (sv);
			return;
		}
	}

	TreeModel::Children rows = track_model->children();
	TreeModel::Children::iterator i;
	uint32_t n_masters = 0;

	container_clear (vca_hpacker);
	vca_hpacker.pack_end (vca_scroller_base, true, true);

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
		UIConfiguration::instance().set_mixer_strip_visibility (VisibilityGroup::remove_element (UIConfiguration::instance().get_mixer_strip_visibility(), X_("VCA")));
		vca_vpacker.hide ();
	} else {
		UIConfiguration::instance().set_mixer_strip_visibility (VisibilityGroup::add_element (UIConfiguration::instance().get_mixer_strip_visibility(), X_("VCA")));
		vca_vpacker.show ();
	}

	_group_tabs->set_dirty ();
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

	boost::shared_ptr<RouteList> routes = _session->get_routes();

	for (RouteList::iterator r = routes->begin(); r != routes->end(); ++r) {
		sl.push_back (*r);
	}

	VCAList vcas = _session->vca_manager().vcas();

	for (VCAList::iterator v = vcas.begin(); v != vcas.end(); ++v) {
		sl.push_back (boost::dynamic_pointer_cast<Stripable> (*v));
	}

	sl.sort (PresentationInfoMixerSorter());

	{
		/* These are also used inside ::add_stripables() but we need
		 *  them here because we're going to clear the track_model also.
		 */
		Unwinder<bool> uw1 (no_track_list_redisplay, true);
		Unwinder<bool> uw2 (ignore_reorder, true);

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
Mixer_UI::move_stripable_into_view (boost::shared_ptr<ARDOUR::Stripable> s)
{
	if (!scroller.get_hscrollbar()) {
		return;
	}
	if (s->presentation_info().special () || s->presentation_info().flag_match (PresentationInfo::VCA)) {
		return;
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
	items.push_back (MenuElem (_("Show All Audio Busses"), sigc::mem_fun(*this, &Mixer_UI::show_all_audiobus)));
	items.push_back (MenuElem (_("Hide All Audio Busses"), sigc::mem_fun(*this, &Mixer_UI::hide_all_audiobus)));
	items.push_back (MenuElem (_("Show All Midi Tracks"), sigc::mem_fun (*this, &Mixer_UI::show_all_miditracks)));
	items.push_back (MenuElem (_("Hide All Midi Tracks"), sigc::mem_fun (*this, &Mixer_UI::hide_all_miditracks)));

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
Mixer_UI::show_mixer_list (bool yn)
{
	if (yn) {
		list_vpacker.show ();
	} else {
		list_vpacker.hide ();
	}

	_show_mixer_list = yn;
}

void
Mixer_UI::show_monitor_section (bool yn)
{
	if (!monitor_section()) {
		return;
	}
	if (monitor_section()->tearoff().torn_off()) {
		return;
	}

	if (yn) {
		monitor_section()->tearoff().show();
	} else {
		monitor_section()->tearoff().hide();
	}
}

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
Mixer_UI::strip_scroller_button_release (GdkEventButton* ev)
{
	using namespace Menu_Helpers;

	if (Keyboard::is_context_menu_event (ev)) {
		ARDOUR_UI::instance()->add_route ();
		return true;
	}

	return false;
}

void
Mixer_UI::scroller_drag_data_received (const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, const Gtk::SelectionData& data, guint info, guint time)
{
	printf ("Mixer_UI::scroller_drag_data_received\n");
	if (data.get_target() != "PluginFavoritePtr") {
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
		ARDOUR_UI::instance()->session_add_midi_track ((RouteGroup*) 0, 1, _("MIDI"), Config->get_strict_io (), pip, ppp->_preset.valid ? &ppp->_preset : 0, PresentationInfo::max_order);
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


struct PluginStateSorter {
public:
	bool operator() (PluginInfoPtr a, PluginInfoPtr b) const {
		std::list<std::string>::const_iterator aiter = std::find(_user.begin(), _user.end(), (*a).unique_id);
		std::list<std::string>::const_iterator biter = std::find(_user.begin(), _user.end(), (*b).unique_id);
		if (aiter != _user.end() && biter != _user.end()) {
			return std::distance (_user.begin(), aiter)  < std::distance (_user.begin(), biter);
		}
		if (aiter != _user.end()) {
			return true;
		}
		if (biter != _user.end()) {
			return false;
		}
		return ARDOUR::cmp_nocase((*a).name, (*b).name) == -1;
	}

	PluginStateSorter(std::list<std::string> user) : _user (user)  {}
private:
	std::list<std::string> _user;
};

int
Mixer_UI::set_state (const XMLNode& node, int version)
{
	LocaleGuard lg;
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

	if (node.get_property ("maximised", yn)) {
		Glib::RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("ToggleMaximalMixer"));
		assert (act);
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		bool fs = tact && tact->get_active();
		if (yn ^ fs) {
			ActionManager::do_action ("Common", "ToggleMaximalMixer");
		}
	}

	if (node.get_property ("show-mixer-list", yn)) {
		Glib::RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("ToggleMixerList"));
		assert (act);
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

		/* do it twice to force the change */
		tact->set_active (!yn);
		tact->set_active (yn);
	}


	XMLNode* plugin_order;
	if ((plugin_order = find_named_node (node, "PluginOrder")) != 0) {
		store_current_favorite_order ();
		std::list<string> order;
		const XMLNodeList& kids = plugin_order->children("PluginInfo");
		XMLNodeConstIterator i;
		for (i = kids.begin(); i != kids.end(); ++i) {
			std::string unique_id;
			if ((*i)->get_property ("unique-id", unique_id)) {
				order.push_back (unique_id);
				if ((*i)->get_property ("expanded", yn)) {
					favorite_ui_state[unique_id] = yn;
				}
			}
		}
		PluginStateSorter cmp (order);
		favorite_order.sort (cmp);
		sync_treeview_from_favorite_order ();
	}
	return 0;
}

XMLNode&
Mixer_UI::get_state ()
{
	XMLNode* node = new XMLNode (X_("Mixer"));
	LocaleGuard lg;

	node->add_child_nocopy (Tabbable::get_state());

	node->set_property (X_("mixer-rhs-pane1-pos"), rhs_pane1.get_divider());
	node->set_property (X_("mixer-rhs_pane2-pos"), rhs_pane2.get_divider());
	node->set_property (X_("mixer-list-hpane-pos"), list_hpane.get_divider());
	node->set_property (X_("mixer-inner-pane-pos"),  inner_pane.get_divider());

	node->set_property ("narrow-strips", (_strip_width == Narrow));
	node->set_property ("show-mixer", _visible);
	node->set_property ("show-mixer-list", _show_mixer_list);
	node->set_property ("maximised", _maximised);

	store_current_favorite_order ();
	XMLNode* plugin_order = new XMLNode ("PluginOrder");
	uint32_t cnt = 0;
	for (PluginInfoList::const_iterator i = favorite_order.begin(); i != favorite_order.end(); ++i, ++cnt) {
		XMLNode* p = new XMLNode ("PluginInfo");
		p->set_property ("sort", cnt);
		p->set_property ("unique-id", (*i)->unique_id);
		if (favorite_ui_state.find ((*i)->unique_id) != favorite_ui_state.end ()) {
			p->set_property ("expanded", favorite_ui_state[(*i)->unique_id]);
		}
		plugin_order->add_child_nocopy (*p);
	}
	node->add_child_nocopy (*plugin_order);

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
Mixer_UI::parameter_changed (string const & p)
{
	if (p == "show-group-tabs") {
		bool const s = _session->config.get_show_group_tabs ();
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
	} else if (p == "use-monitor-bus") {
		if (_session && !_session->monitor_out()) {
			monitor_section_detached ();
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

	VBox* v = manage (new VBox);
	v->show ();
	v->pack_start (track_display_scroller, true, true);

	Button* b = manage (new Button);
	b->show ();
	Widget* w = manage (new Image (Stock::ADD, ICON_SIZE_BUTTON));
	w->show ();
	b->add (*w);

	b->signal_clicked().connect (sigc::mem_fun (*this, &Mixer_UI::new_track_or_bus));

	v->pack_start (*b, false, false);

	track_display_frame.set_name("BaseFrame");
	track_display_frame.set_shadow_type (Gtk::SHADOW_IN);
	track_display_frame.add (*v);

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
	if (_monitor_section) {
		monitor_section_detached ();
		out_packer.remove (_monitor_section->tearoff());
		_monitor_section->set_session (0);
		delete _monitor_section;
		_monitor_section = 0;
	}
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
	Glib::RefPtr<Action> act = ActionManager::get_action ("Common", "ToggleMonitorSection");
	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
	act->set_sensitive (true);
	tact->set_active ();
}

void
Mixer_UI::monitor_section_detached ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action ("Common", "ToggleMonitorSection");
	act->set_sensitive (false);
}

void
Mixer_UI::store_current_favorite_order ()
{
	typedef Gtk::TreeModel::Children type_children;
	type_children children = favorite_plugins_model->children();
	favorite_order.clear();
	for(type_children::iterator iter = children.begin(); iter != children.end(); ++iter)
	{
		Gtk::TreeModel::Row row = *iter;
		ARDOUR::PluginPresetPtr ppp = row[favorite_plugins_columns.plugin];
		favorite_order.push_back (ppp->_pip);
		std::string name = row[favorite_plugins_columns.name];
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
Mixer_UI::refiller (PluginInfoList& result, const PluginInfoList& plugs)
{
	PluginManager& manager (PluginManager::instance());
	for (PluginInfoList::const_iterator i = plugs.begin(); i != plugs.end(); ++i) {
		if (manager.get_status (*i) != PluginManager::Favorite) {
			continue;
		}
		result.push_back (*i);
	}
}

struct PluginCustomSorter {
public:
	bool operator() (PluginInfoPtr a, PluginInfoPtr b) const {
		PluginInfoList::const_iterator aiter = _user.begin();
		PluginInfoList::const_iterator biter = _user.begin();
		while (aiter != _user.end()) { if ((*aiter)->unique_id == a->unique_id) { break; } ++aiter; }
		while (biter != _user.end()) { if ((*biter)->unique_id == b->unique_id) { break; } ++biter; }

		if (aiter != _user.end() && biter != _user.end()) {
			return std::distance (_user.begin(), aiter) < std::distance (_user.begin(), biter);
		}
		if (aiter != _user.end()) {
			return true;
		}
		if (biter != _user.end()) {
			return false;
		}
		return ARDOUR::cmp_nocase((*a).name, (*b).name) == -1;
	}
	PluginCustomSorter(PluginInfoList user) : _user (user)  {}
private:
	PluginInfoList _user;
};

void
Mixer_UI::refill_favorite_plugins ()
{
	PluginInfoList plugs;
	PluginManager& mgr (PluginManager::instance());

#ifdef LV2_SUPPORT
	refiller (plugs, mgr.lv2_plugin_info ());
#endif
#ifdef WINDOWS_VST_SUPPORT
	refiller (plugs, mgr.windows_vst_plugin_info ());
#endif
#ifdef LXVST_SUPPORT
	refiller (plugs, mgr.lxvst_plugin_info ());
#endif
#ifdef MACVST_SUPPORT
	refiller (plugs, mgr.mac_vst_plugin_info ());
#endif
#ifdef AUDIOUNIT_SUPPORT
	refiller (plugs, mgr.au_plugin_info ());
#endif
	refiller (plugs, mgr.ladspa_plugin_info ());
	refiller (plugs, mgr.lua_plugin_info ());

	store_current_favorite_order ();

	PluginCustomSorter cmp (favorite_order);
	plugs.sort (cmp);

	favorite_order = plugs;

	sync_treeview_from_favorite_order ();
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
	favorite_plugins_model->clear ();
	for (PluginInfoList::const_iterator i = favorite_order.begin(); i != favorite_order.end(); ++i) {
		PluginInfoPtr pip = (*i);

		TreeModel::Row newrow = *(favorite_plugins_model->append());
		newrow[favorite_plugins_columns.name] = (*i)->name;
		newrow[favorite_plugins_columns.plugin] = PluginPresetPtr (new PluginPreset(pip));
		if (!_session) {
			continue;
		}

		vector<ARDOUR::Plugin::PresetRecord> presets = (*i)->get_presets (true);
		for (vector<ARDOUR::Plugin::PresetRecord>::const_iterator j = presets.begin(); j != presets.end(); ++j) {
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

	Gtk::Menu* m = manage (new Menu);
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
	if ((ev->type == GDK_BUTTON_PRESS) && (ev->button == 3) ) {
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
		boost::shared_ptr<Processor> processor (new PluginInsert (*_session, p));

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

void
Mixer_UI::plugin_drop (const Glib::RefPtr<Gdk::DragContext>&, const Gtk::SelectionData& data)
{
	if (data.get_target() != "PluginPresetPtr") {
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
	if (ss != s) {
		spilled_strip = s;
		show_spill_change (s); /* EMIT SIGNAL */
		if (s) {
			_group_tabs->hide ();
		} else {
			_group_tabs->show ();
		}
		redisplay_track_list ();
	}
}

bool
Mixer_UI::showing_spill_for (boost::shared_ptr<Stripable> s) const
{
	return s == spilled_strip.lock();
}

void
Mixer_UI::show_editor_window () const
{
	PublicEditor::instance().make_visible ();
}

void
Mixer_UI::register_actions ()
{
	Glib::RefPtr<ActionGroup> group = myactions.create_action_group (X_("Mixer"));

	myactions.register_action (group, "show-editor", _("Show Editor"), sigc::mem_fun (*this, &Mixer_UI::show_editor_window));

	myactions.register_action (group, "solo", _("Toggle Solo on Mixer-Selected Tracks/Busses"), sigc::mem_fun (*this, &Mixer_UI::solo_action));
	myactions.register_action (group, "mute", _("Toggle Mute on Mixer-Selected Tracks/Busses"), sigc::mem_fun (*this, &Mixer_UI::mute_action));
	myactions.register_action (group, "recenable", _("Toggle Rec-enable on Mixer-Selected Tracks/Busses"), sigc::mem_fun (*this, &Mixer_UI::rec_enable_action));
	myactions.register_action (group, "increment-gain", _("Decrease Gain on Mixer-Selected Tracks/Busses"), sigc::mem_fun (*this, &Mixer_UI::step_gain_up_action));
	myactions.register_action (group, "decrement-gain", _("Increase Gain on Mixer-Selected Tracks/Busses"), sigc::mem_fun (*this, &Mixer_UI::step_gain_down_action));
	myactions.register_action (group, "unity-gain", _("Set Gain to 0dB on Mixer-Selected Tracks/Busses"), sigc::mem_fun (*this, &Mixer_UI::unity_gain_action));


	myactions.register_action (group, "copy-processors", _("Copy Selected Processors"), sigc::mem_fun (*this, &Mixer_UI::copy_processors));
	myactions.register_action (group, "cut-processors", _("Cut Selected Processors"), sigc::mem_fun (*this, &Mixer_UI::cut_processors));
	myactions.register_action (group, "paste-processors", _("Paste Selected Processors"), sigc::mem_fun (*this, &Mixer_UI::paste_processors));
	myactions.register_action (group, "delete-processors", _("Delete Selected Processors"), sigc::mem_fun (*this, &Mixer_UI::delete_processors));
	myactions.register_action (group, "select-all-processors", _("Select All (visible) Processors"), sigc::mem_fun (*this, &Mixer_UI::select_all_processors));
	myactions.register_action (group, "toggle-processors", _("Toggle Selected Processors"), sigc::mem_fun (*this, &Mixer_UI::toggle_processors));
	myactions.register_action (group, "ab-plugins", _("Toggle Selected Plugins"), sigc::mem_fun (*this, &Mixer_UI::ab_plugins));
	myactions.register_action (group, "select-none", _("Deselect all strips and processors"), sigc::mem_fun (*this, &Mixer_UI::select_none));

	myactions.register_action (group, "scroll-left", _("Scroll Mixer Window to the left"), sigc::mem_fun (*this, &Mixer_UI::scroll_left));
	myactions.register_action (group, "scroll-right", _("Scroll Mixer Window to the right"), sigc::mem_fun (*this, &Mixer_UI::scroll_right));

	myactions.register_action (group, "toggle-midi-input-active", _("Toggle MIDI Input Active for Mixer-Selected Tracks/Busses"),
	                           sigc::bind (sigc::mem_fun (*this, &Mixer_UI::toggle_midi_input_active), false));
}

void
Mixer_UI::load_bindings ()
{
	bindings = Bindings::get_bindings (X_("Mixer"), myactions);
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

void
Mixer_UI::step_gain_up_action ()
{
	set_axis_targets_for_operation ();

	BOOST_FOREACH(AxisView* r, _axis_targets) {
		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);
		if (ms) {
			ms->step_gain_up ();
		}
	}
}

void
Mixer_UI::step_gain_down_action ()
{
	set_axis_targets_for_operation ();

	BOOST_FOREACH(AxisView* r, _axis_targets) {
		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);
		if (ms) {
			ms->step_gain_down ();
		}
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
