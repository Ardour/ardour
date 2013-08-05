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

#include <gtkmm/accelmap.h>

#include "pbd/convert.h"
#include "pbd/unwind.h"

#include <glibmm/threads.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/tearoff.h>
#include <gtkmm2ext/window_title.h>

#include "ardour/debug.h"
#include "ardour/midi_track.h"
#include "ardour/plugin_manager.h"
#include "ardour/route_group.h"
#include "ardour/session.h"

#include "keyboard.h"
#include "mixer_ui.h"
#include "mixer_strip.h"
#include "monitor_section.h"
#include "plugin_selector.h"
#include "public_editor.h"
#include "ardour_ui.h"
#include "prompter.h"
#include "utils.h"
#include "route_sorter.h"
#include "actions.h"
#include "gui_thread.h"
#include "mixer_group_tabs.h"

#include "i18n.h"

using namespace ARDOUR;
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
	: Window (Gtk::WINDOW_TOPLEVEL)
	, VisibilityTracker (*((Gtk::Window*) this))
	, _visible (false)
	, no_track_list_redisplay (false)
	, in_group_row_change (false)
	, track_menu (0)
	, _monitor_section (0)
	, _strip_width (Config->get_default_narrow_ms() ? Narrow : Wide)
	, ignore_reorder (false)
	, _following_editor_selection (false)
{
	/* allow this window to become the key focus window */
	set_flags (CAN_FOCUS);

	Route::SyncOrderKeys.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::sync_treeview_from_order_keys, this, _1), gui_context());

	scroller.set_can_default (true);
	set_default (scroller);

	scroller_base.set_flags (Gtk::CAN_FOCUS);
	scroller_base.add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	scroller_base.set_name ("MixerWindow");
	scroller_base.signal_button_release_event().connect (sigc::mem_fun(*this, &Mixer_UI::strip_scroller_button_release));
	// add as last item of strip packer
	strip_packer.pack_end (scroller_base, true, true);

	_group_tabs = new MixerGroupTabs (this);
	VBox* b = manage (new VBox);
	b->pack_start (*_group_tabs, PACK_SHRINK);
	b->pack_start (strip_packer);
	b->show_all ();

	scroller.add (*b);
	scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

	setup_track_display ();

	group_model = ListStore::create (group_columns);
	group_display.set_model (group_model);
	group_display.append_column (_("Group"), group_columns.text);
	group_display.append_column (_("Show"), group_columns.visible);
	group_display.get_column (0)->set_data (X_("colnum"), GUINT_TO_POINTER(0));
	group_display.get_column (1)->set_data (X_("colnum"), GUINT_TO_POINTER(1));
	group_display.get_column (0)->set_expand(true);
	group_display.get_column (1)->set_expand(false);
	group_display.set_name ("EditGroupList");
	group_display.get_selection()->set_mode (Gtk::SELECTION_SINGLE);
	group_display.set_reorderable (true);
	group_display.set_headers_visible (true);
	group_display.set_rules_hint (true);

	/* name is directly editable */

	CellRendererText* name_cell = dynamic_cast<CellRendererText*>(group_display.get_column_cell_renderer (0));
	name_cell->property_editable() = true;
	name_cell->signal_edited().connect (sigc::mem_fun (*this, &Mixer_UI::route_group_name_edit));

	/* use checkbox for the active column */

	CellRendererToggle* active_cell = dynamic_cast<CellRendererToggle*>(group_display.get_column_cell_renderer (1));
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

	rhs_pane1.pack1 (track_display_frame);
	rhs_pane1.pack2 (group_display_frame);

	list_vpacker.pack_start (rhs_pane1, true, true);

	global_hpacker.pack_start (scroller, true, true);
#ifdef GTKOSX
	/* current gtk-quartz has dirty updates on borders like this one */
	global_hpacker.pack_start (out_packer, false, false, 0);
#else
	global_hpacker.pack_start (out_packer, false, false, 12);
#endif
	list_hpane.pack1(list_vpacker, true, true);
	list_hpane.pack2(global_hpacker, true, false);

	rhs_pane1.signal_size_allocate().connect (sigc::bind (sigc::mem_fun(*this, &Mixer_UI::pane_allocation_handler),
							static_cast<Gtk::Paned*> (&rhs_pane1)));
	list_hpane.signal_size_allocate().connect (sigc::bind (sigc::mem_fun(*this, &Mixer_UI::pane_allocation_handler),
							 static_cast<Gtk::Paned*> (&list_hpane)));

	global_vpacker.pack_start (list_hpane, true, true);

	add (global_vpacker);
	set_name ("MixerWindow");

	update_title ();

	set_wmclass (X_("ardour_mixer"), PROGRAM_NAME);

	signal_delete_event().connect (sigc::mem_fun (*this, &Mixer_UI::hide_window));
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);

	signal_configure_event().connect (sigc::mem_fun (*ARDOUR_UI::instance(), &ARDOUR_UI::configure_handler));

	route_group_display_button_box->show();
	route_group_add_button->show();
	route_group_remove_button->show();

	global_hpacker.show();
	global_vpacker.show();
	scroller.show();
	scroller_base.show();
	scroller_hpacker.show();
	mixer_scroller_vpacker.show();
	list_vpacker.show();
	group_display_button_label.show();
	group_display_button.show();
	group_display_scroller.show();
	group_display_vbox.show();
	group_display_frame.show();
	rhs_pane1.show();
	strip_packer.show();
	out_packer.show();
	list_hpane.show();
	group_display.show();

	_in_group_rebuild_or_clear = false;

	MixerStrip::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::remove_strip, this, _1), gui_context());

        MonitorSection::setup_knob_images ();

#ifndef DEFER_PLUGIN_SELECTOR_LOAD
	_plugin_selector = new PluginSelector (PluginManager::instance ());
#endif
}

Mixer_UI::~Mixer_UI ()
{
	if (_monitor_section) {
		delete _monitor_section;
	}
}

void
Mixer_UI::track_editor_selection ()
{
	PublicEditor::instance().get_selection().TracksChanged.connect (sigc::mem_fun (*this, &Mixer_UI::follow_editor_selection));
}


void
Mixer_UI::ensure_float (Window& win)
{
	win.set_transient_for (*this);
}

void
Mixer_UI::show_window ()
{
	present ();
	if (!_visible) {
		set_window_pos_and_size ();

		/* show/hide group tabs as required */
		parameter_changed ("show-group-tabs");

		/* now reset each strips width so the right widgets are shown */
		MixerStrip* ms;

		TreeModel::Children rows = track_model->children();
		TreeModel::Children::iterator ri;

		for (ri = rows.begin(); ri != rows.end(); ++ri) {
			ms = (*ri)[track_columns.strip];
			ms->set_width_enum (ms->get_width_enum (), ms->width_owner());
			/* Fix visibility of mixer strip stuff */
			ms->parameter_changed (X_("mixer-strip-visibility"));
		}
	}
	
	/* force focus into main area */
	scroller_base.grab_focus ();

	_visible = true;
}

bool
Mixer_UI::hide_window (GdkEventAny *ev)
{
	get_window_pos_and_size ();

	_visible = false;
	return just_hide_it(ev, static_cast<Gtk::Window *>(this));
}


void
Mixer_UI::add_strips (RouteList& routes)
{
	MixerStrip* strip;

	try {
		no_track_list_redisplay = true;
		track_display.set_model (Glib::RefPtr<ListStore>());

		for (RouteList::iterator x = routes.begin(); x != routes.end(); ++x) {
			boost::shared_ptr<Route> route = (*x);
			
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
				
				route->DropReferences.connect (*this, invalidator(*this), boost::bind (&Mixer_UI::monitor_section_going_away, this), gui_context());
				
				/* no regular strip shown for control out */
				
				continue;
			}
			
			strip = new MixerStrip (*this, _session, route);
			strips.push_back (strip);

			Config->get_default_narrow_ms() ? _strip_width = Narrow : _strip_width = Wide;
			
			if (strip->width_owner() != strip) {
				strip->set_width_enum (_strip_width, this);
			}
			
			show_strip (strip);
			
			TreeModel::Row row = *(track_model->append());
			row[track_columns.text] = route->name();
			row[track_columns.visible] = strip->route()->is_master() ? true : strip->marked_for_display();
			row[track_columns.route] = route;
			row[track_columns.strip] = strip;
			
			route->PropertyChanged.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::strip_property_changed, this, _1, strip), gui_context());
			
			strip->WidthChanged.connect (sigc::mem_fun(*this, &Mixer_UI::strip_width_changed));
			strip->signal_button_release_event().connect (sigc::bind (sigc::mem_fun(*this, &Mixer_UI::strip_button_release_event), strip));
		}

	} catch (...) {
	}

	no_track_list_redisplay = false;
	track_display.set_model (track_model);
	
	sync_order_keys_from_treeview ();
	redisplay_track_list ();
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
		if ((*ri)[track_columns.strip] == strip) {
			track_model->erase (ri);
			break;
		}
	}
}

void
Mixer_UI::reset_remote_control_ids ()
{
	if (Config->get_remote_model() != MixerOrdered || !_session || _session->deletion_in_progress()) {
		return;
	}

	TreeModel::Children rows = track_model->children();
	
	if (rows.empty()) {
		return;
	}

	DEBUG_TRACE (DEBUG::OrderKeys, "mixer resets remote control ids after remote model change\n");

	TreeModel::Children::iterator ri;
	bool rid_change = false;
	uint32_t rid = 1;
	uint32_t invisible_key = UINT32_MAX;

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		boost::shared_ptr<Route> route = (*ri)[track_columns.route];
		bool visible = (*ri)[track_columns.visible];

		if (!route->is_master() && !route->is_monitor()) {
			
			uint32_t new_rid = (visible ? rid : invisible_key--);
			
			if (new_rid != route->remote_control_id()) {
				route->set_remote_control_id_from_order_key (MixerSort, new_rid);	
				rid_change = true;
			}
			
			if (visible) {
				rid++;
			}
		}
	}

	if (rid_change) {
		/* tell the world that we changed the remote control IDs */
		_session->notify_remote_id_change ();
	}
}

void
Mixer_UI::sync_order_keys_from_treeview ()
{
	if (ignore_reorder || !_session || _session->deletion_in_progress()) {
		return;
	}

	TreeModel::Children rows = track_model->children();
	
	if (rows.empty()) {
		return;
	}

	DEBUG_TRACE (DEBUG::OrderKeys, "mixer sync order keys from model\n");

	TreeModel::Children::iterator ri;
	bool changed = false;
	bool rid_change = false;
	uint32_t order = 0;
	uint32_t rid = 1;
	uint32_t invisible_key = UINT32_MAX;

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		boost::shared_ptr<Route> route = (*ri)[track_columns.route];
		bool visible = (*ri)[track_columns.visible];

		uint32_t old_key = route->order_key (MixerSort);

		if (order != old_key) {
			route->set_order_key (MixerSort, order);
			changed = true;
		}

		if ((Config->get_remote_model() == MixerOrdered) && !route->is_master() && !route->is_monitor()) {

			uint32_t new_rid = (visible ? rid : invisible_key--);

			if (new_rid != route->remote_control_id()) {
				route->set_remote_control_id_from_order_key (MixerSort, new_rid);	
				rid_change = true;
			}
			
			if (visible) {
				rid++;
			}

		}

		++order;
	}

	if (changed) {
		/* tell everyone that we changed the mixer sort keys */
		_session->sync_order_keys (MixerSort);
	}

	if (rid_change) {
		/* tell the world that we changed the remote control IDs */
		_session->notify_remote_id_change ();
	}
}

void
Mixer_UI::sync_treeview_from_order_keys (RouteSortOrderKey src)
{
	if (!_session || _session->deletion_in_progress()) {
		return;
	}

	DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("mixer sync model from order keys, src = %1\n", enum_2_string (src)));

	if (src == EditorSort) {

		if (!Config->get_sync_all_route_ordering()) {
			/* editor sort keys changed - we don't care */
			return;
		}

		DEBUG_TRACE (DEBUG::OrderKeys, "reset mixer order key to match editor\n");

		/* editor sort keys were changed, update the mixer sort
		 * keys since "sync mixer+editor order" is enabled.
		 */

		boost::shared_ptr<RouteList> r = _session->get_routes ();
		
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			(*i)->sync_order_keys (src);
		}
	}

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

	OrderKeySortedRoutes sorted_routes;

	for (TreeModel::Children::iterator ri = rows.begin(); ri != rows.end(); ++ri, ++old_order) {
		boost::shared_ptr<Route> route = (*ri)[track_columns.route];
		sorted_routes.push_back (RoutePlusOrderKey (route, old_order, route->order_key (MixerSort)));
	}

	SortByNewDisplayOrder cmp;

	sort (sorted_routes.begin(), sorted_routes.end(), cmp);
	neworder.assign (sorted_routes.size(), 0);

	uint32_t n = 0;
	
	for (OrderKeySortedRoutes::iterator sr = sorted_routes.begin(); sr != sorted_routes.end(); ++sr, ++n) {

		neworder[n] = sr->old_display_order;

		if (sr->old_display_order != n) {
			changed = true;
		}

		DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("MIXER change order for %1 from %2 to %3\n",
							       sr->route->name(), sr->old_display_order, n));
	}

	if (changed) {
		Unwinder<bool> uw (ignore_reorder, true);
		track_model->reorder (neworder);
	}

	redisplay_track_list ();
}

void
Mixer_UI::follow_editor_selection ()
{
	if (!Config->get_link_editor_and_mixer_selection() || _following_editor_selection) {
		return;
	}

	_following_editor_selection = true;
	_selection.block_routes_changed (true);
	
	TrackSelection& s (PublicEditor::instance().get_selection().tracks);

	_selection.clear_routes ();

	for (TrackViewList::iterator i = s.begin(); i != s.end(); ++i) {
		RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (*i);
		if (rtav) {
			MixerStrip* ms = strip_by_route (rtav->route());
			if (ms) {
				_selection.add (ms);
			}
		}
	}

	_following_editor_selection = false;
	_selection.block_routes_changed (false);
}


MixerStrip*
Mixer_UI::strip_by_route (boost::shared_ptr<Route> r)
{
	for (list<MixerStrip *>::iterator i = strips.begin(); i != strips.end(); ++i) {
		if ((*i)->route() == r) {
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
			} 
		} else {
			if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
				_selection.add (strip);
			} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::RangeSelectModifier)) {

				if (!_selection.selected(strip)) {
				
					/* extend selection */
					
					vector<MixerStrip*> tmp;
					bool accumulate = false;
					
					tmp.push_back (strip);

					for (list<MixerStrip*>::iterator i = strips.begin(); i != strips.end(); ++i) {
						if ((*i) == strip) {
							/* hit clicked strip, start accumulating till we hit the first 
							   selected strip
							*/
							if (accumulate) {
								/* done */
								break;
							} else {
								accumulate = true;
							}
						} else if (_selection.selected (*i)) {
							/* hit selected strip. if currently accumulating others,
							   we're done. if not accumulating others, start doing so.
							*/
							if (accumulate) {
								/* done */
								break;
							} else {
								accumulate = true;
							}
						} else {
							if (accumulate) {
								tmp.push_back (*i);
							}
						}
					}

					for (vector<MixerStrip*>::iterator i = tmp.begin(); i != tmp.end(); ++i) {
						_selection.add (*i);
					}
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

	if (!_session) {
		return;
	}

	XMLNode* node = ARDOUR_UI::instance()->mixer_settings();
	set_state (*node);

	update_title ();

	initial_track_display ();

	_session->RouteAdded.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::add_strips, this, _1), gui_context());
	_session->route_group_added.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::add_route_group, this, _1), gui_context());
	_session->route_group_removed.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::route_groups_changed, this), gui_context());
	_session->route_groups_reordered.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::route_groups_changed, this), gui_context());
	_session->config.ParameterChanged.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::parameter_changed, this, _1), gui_context());
	_session->DirtyChanged.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::update_title, this), gui_context());
	_session->StateSaved.connect (_session_connections, invalidator (*this), boost::bind (&Mixer_UI::update_title, this), gui_context());

	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&Mixer_UI::parameter_changed, this, _1), gui_context ());

	route_groups_changed ();

	if (_visible) {
		show_window();

		/* Bit of a hack; if we're here, we're opening the mixer because of our
		   instant XML state having a show-mixer property.  Fix up the corresponding
		   action state.
		*/
		ActionManager::check_toggleaction ("<Actions>/Common/toggle-mixer");
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
		MixerStrip* strip = (*iter)[track_columns.strip];
		if (strip) {
			bool visible = (*iter)[track_columns.visible];

			if (strip->set_marked_for_display (!visible)) {
				update_track_visibility ();
			}
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
			MixerStrip *strip = (*i)[track_columns.strip];
			(*i)[track_columns.visible] = strip->marked_for_display ();
		}
		
		/* force route order keys catch up with visibility changes
		 */
		
		sync_order_keys_from_treeview ();
	}

	redisplay_track_list ();
}

void
Mixer_UI::show_strip (MixerStrip* ms)
{
	TreeModel::Children rows = track_model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {

		MixerStrip* strip = (*i)[track_columns.strip];
		if (strip == ms) {
			(*i)[track_columns.visible] = true;
			redisplay_track_list ();
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

		MixerStrip* strip = (*i)[track_columns.strip];
		if (strip == ms) {
			(*i)[track_columns.visible] = false;
			redisplay_track_list ();
			break;
		}
	}
}

gint
Mixer_UI::start_updating ()
{
    fast_screen_update_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect (sigc::mem_fun(*this, &Mixer_UI::fast_update_strips));
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
	if (is_mapped () && _session) {
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
			
			TreeModel::Row row = (*i);
			MixerStrip* strip = row[track_columns.strip];
			
			if (strip == 0) {
				continue;
			}
			
			if (strip->route()->is_master() || strip->route()->is_monitor()) {
				continue;
			}
			
			(*i)[track_columns.visible] = yn;
		}
	}

	redisplay_track_list ();
}


void
Mixer_UI::set_all_audio_visibility (int tracks, bool yn)
{
	TreeModel::Children rows = track_model->children();
	TreeModel::Children::iterator i;

	{
		Unwinder<bool> uw (no_track_list_redisplay, true);
		
		for (i = rows.begin(); i != rows.end(); ++i) {
			TreeModel::Row row = (*i);
			MixerStrip* strip = row[track_columns.strip];
			
			if (strip == 0) {
				continue;
			}
			
			if (strip->route()->is_master() || strip->route()->is_monitor()) {
				continue;
			}
			
			boost::shared_ptr<AudioTrack> at = strip->audio_track();
			
			switch (tracks) {
			case 0:
				(*i)[track_columns.visible] = yn;
				break;
				
			case 1:
				if (at) { /* track */
					(*i)[track_columns.visible] = yn;
				}
				break;
				
			case 2:
				if (!at) { /* bus */
					(*i)[track_columns.visible] = yn;
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
	set_all_audio_visibility (2, true);
}
void
Mixer_UI::hide_all_audiobus ()
{
	set_all_audio_visibility (2, false);
}

void
Mixer_UI::show_all_audiotracks()
{
	set_all_audio_visibility (1, true);
}
void
Mixer_UI::hide_all_audiotracks ()
{
	set_all_audio_visibility (1, false);
}

void
Mixer_UI::track_list_reorder (const TreeModel::Path&, const TreeModel::iterator&, int* /*new_order*/)
{
	DEBUG_TRACE (DEBUG::OrderKeys, "mixer UI treeview reordered\n");
	sync_order_keys_from_treeview ();
}

void
Mixer_UI::track_list_delete (const Gtk::TreeModel::Path&)
{
	/* this happens as the second step of a DnD within the treeview as well
	   as when a row/route is actually deleted.
	*/
	DEBUG_TRACE (DEBUG::OrderKeys, "mixer UI treeview row deleted\n");
	sync_order_keys_from_treeview ();
}

void
Mixer_UI::redisplay_track_list ()
{
	TreeModel::Children rows = track_model->children();
	TreeModel::Children::iterator i;
	
	if (no_track_list_redisplay) {
		return;
	}

	for (i = rows.begin(); i != rows.end(); ++i) {

		MixerStrip* strip = (*i)[track_columns.strip];

		if (strip == 0) {
			/* we're in the middle of changing a row, don't worry */
			continue;
		}

		bool const visible = (*i)[track_columns.visible];

		if (visible) {
			strip->set_gui_property ("visible", true);

			if (strip->packed()) {

				if (strip->route()->is_master() || strip->route()->is_monitor()) {
					out_packer.reorder_child (*strip, -1);

				} else {
					strip_packer.reorder_child (*strip, -1); /* put at end */
				}

			} else {

				if (strip->route()->is_master() || strip->route()->is_monitor()) {
					out_packer.pack_start (*strip, false, false);
				} else {
					strip_packer.pack_start (*strip, false, false);
				}
				strip->set_packed (true);
			}

		} else {

			strip->set_gui_property ("visible", false);

			if (strip->route()->is_master() || strip->route()->is_monitor()) {
				/* do nothing, these cannot be hidden */
			} else {
				if (strip->packed()) {
					strip_packer.remove (*strip);
					strip->set_packed (false);
				}
			}
		}
	}

	_group_tabs->set_dirty ();
}

void
Mixer_UI::strip_width_changed ()
{
	_group_tabs->set_dirty ();

#ifdef GTKOSX
	TreeModel::Children rows = track_model->children();
	TreeModel::Children::iterator i;
	long order;

	for (order = 0, i = rows.begin(); i != rows.end(); ++i, ++order) {
		MixerStrip* strip = (*i)[track_columns.strip];

		if (strip == 0) {
			continue;
		}

		bool visible = (*i)[track_columns.visible];

		if (visible) {
			strip->queue_draw();
		}
	}
#endif

}

struct SignalOrderRouteSorter {
    bool operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b) {
	    if (a->is_master() || a->is_monitor()) {
		    /* "a" is a special route (master, monitor, etc), and comes
		     * last in the mixer ordering
		     */
		    return false;
	    } else if (b->is_master() || b->is_monitor()) {
		    /* everything comes before b */
		    return true;
	    }
	    return a->order_key (MixerSort) < b->order_key (MixerSort);

    }
};

void
Mixer_UI::initial_track_display ()
{
	boost::shared_ptr<RouteList> routes = _session->get_routes();
	RouteList copy (*routes);
	SignalOrderRouteSorter sorter;

	copy.sort (sorter);

	{
		Unwinder<bool> uw1 (no_track_list_redisplay, true);
		Unwinder<bool> uw2 (ignore_reorder, true);

		track_model->clear ();
		add_strips (copy);
	}
	
	_session->sync_order_keys (MixerSort);

	redisplay_track_list ();
}

void
Mixer_UI::show_track_list_menu ()
{
	if (track_menu == 0) {
		build_track_menu ();
	}

	track_menu->popup (1, gtk_get_current_event_time());
}

bool
Mixer_UI::track_display_button_press (GdkEventButton* ev)
{
	if (Keyboard::is_context_menu_event (ev)) {
		show_track_list_menu ();
		return true;
	}

	return false;
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

}

void
Mixer_UI::strip_property_changed (const PropertyChange& what_changed, MixerStrip* mx)
{
	if (!what_changed.contains (ARDOUR::Properties::name)) {
		return;
	}

	ENSURE_GUI_THREAD (*this, &Mixer_UI::strip_name_changed, what_changed, mx)

	TreeModel::Children rows = track_model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		if ((*i)[track_columns.strip] == mx) {
			(*i)[track_columns.text] = mx->route()->name();
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
		return false;
	}

	TreeIter iter = group_model->get_iter (path);
	if (!iter) {
		return false;
	}

	RouteGroup* group = (*iter)[group_columns.group];

	if (Keyboard::is_context_menu_event (ev)) {
		_group_tabs->get_menu(group)->popup (1, ev->time);
		return true;
	}

	switch (GPOINTER_TO_UINT (column->get_data (X_("colnum")))) {
	case 0:
		if (Keyboard::is_edit_event (ev)) {
			if (group) {
				// edit_route_group (group);
#ifdef GTKOSX
				group_display.queue_draw();
#endif
				return true;
			}
		}
		break;

	case 1:
	{
		bool visible = (*iter)[group_columns.visible];
		(*iter)[group_columns.visible] = !visible;
#ifdef GTKOSX
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

	{
		TreeModel::Row row;
		row = *(group_model->append());
		row[group_columns.visible] = true;
		row[group_columns.text] = (_("-all-"));
		row[group_columns.group] = 0;
	}

	_session->foreach_route_group (sigc::mem_fun (*this, &Mixer_UI::add_route_group));

	_group_tabs->set_dirty ();
	_in_group_rebuild_or_clear = false;
}

void
Mixer_UI::new_route_group ()
{
	RouteList rl;

	_group_tabs->run_new_group_dialog (rl);
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
		CellRendererText* name_cell = dynamic_cast<CellRendererText*>(group_display.get_column_cell_renderer (0));
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
		ARDOUR_UI::instance()->add_route (this);
		return true;
	}

	return false;
}

void
Mixer_UI::set_strip_width (Width w, bool save)
{
	_strip_width = w;

	for (list<MixerStrip*>::iterator i = strips.begin(); i != strips.end(); ++i) {
		(*i)->set_width_enum (w, save ? (*i)->width_owner() : this);
	}
}

void
Mixer_UI::set_window_pos_and_size ()
{
	resize (m_width, m_height);
	move (m_root_x, m_root_y);
}

	void
Mixer_UI::get_window_pos_and_size ()
{
	get_position(m_root_x, m_root_y);
	get_size(m_width, m_height);
}

int
Mixer_UI::set_state (const XMLNode& node)
{
	const XMLProperty* prop;
	XMLNode* geometry;

	m_width = default_width;
	m_height = default_height;
	m_root_x = 1;
	m_root_y = 1;

	if ((geometry = find_named_node (node, "geometry")) != 0) {

		XMLProperty* prop;

		if ((prop = geometry->property("x_size")) == 0) {
			prop = geometry->property ("x-size");
		}
		if (prop) {
			m_width = atoi(prop->value());
		}
		if ((prop = geometry->property("y_size")) == 0) {
			prop = geometry->property ("y-size");
		}
		if (prop) {
			m_height = atoi(prop->value());
		}

		if ((prop = geometry->property ("x_pos")) == 0) {
			prop = geometry->property ("x-pos");
		}
		if (prop) {
			m_root_x = atoi (prop->value());

		}
		if ((prop = geometry->property ("y_pos")) == 0) {
			prop = geometry->property ("y-pos");
		}
		if (prop) {
			m_root_y = atoi (prop->value());
		}
	}

	set_window_pos_and_size ();

	if ((prop = node.property ("narrow-strips"))) {
		if (string_is_affirmative (prop->value())) {
			set_strip_width (Narrow);
		} else {
			set_strip_width (Wide);
		}
	}

	if ((prop = node.property ("show-mixer"))) {
		if (string_is_affirmative (prop->value())) {
		       _visible = true;
		}
	}

	return 0;
}

XMLNode&
Mixer_UI::get_state (void)
{
	XMLNode* node = new XMLNode ("Mixer");

	if (is_realized()) {
		Glib::RefPtr<Gdk::Window> win = get_window();

		get_window_pos_and_size ();

		XMLNode* geometry = new XMLNode ("geometry");
		char buf[32];
		snprintf(buf, sizeof(buf), "%d", m_width);
		geometry->add_property(X_("x_size"), string(buf));
		snprintf(buf, sizeof(buf), "%d", m_height);
		geometry->add_property(X_("y_size"), string(buf));
		snprintf(buf, sizeof(buf), "%d", m_root_x);
		geometry->add_property(X_("x_pos"), string(buf));
		snprintf(buf, sizeof(buf), "%d", m_root_y);
		geometry->add_property(X_("y_pos"), string(buf));

		// written only for compatibility, they are not used.
		snprintf(buf, sizeof(buf), "%d", 0);
		geometry->add_property(X_("x_off"), string(buf));
		snprintf(buf, sizeof(buf), "%d", 0);
		geometry->add_property(X_("y_off"), string(buf));

		snprintf(buf,sizeof(buf), "%d",gtk_paned_get_position (static_cast<Paned*>(&rhs_pane1)->gobj()));
		geometry->add_property(X_("mixer_rhs_pane1_pos"), string(buf));
		snprintf(buf,sizeof(buf), "%d",gtk_paned_get_position (static_cast<Paned*>(&list_hpane)->gobj()));
		geometry->add_property(X_("mixer_list_hpane_pos"), string(buf));

		node->add_child_nocopy (*geometry);
	}

	node->add_property ("narrow-strips", _strip_width == Narrow ? "yes" : "no");

	node->add_property ("show-mixer", _visible ? "yes" : "no");

	return *node;
}


void
Mixer_UI::pane_allocation_handler (Allocation&, Gtk::Paned* which)
{
	int pos;
	XMLProperty* prop = 0;
	char buf[32];
	XMLNode* node = ARDOUR_UI::instance()->mixer_settings();
	XMLNode* geometry;
	int height;
	static int32_t done[3] = { 0, 0, 0 };

	height = default_height;

	if ((geometry = find_named_node (*node, "geometry")) != 0) {

		if ((prop = geometry->property ("y_size")) == 0) {
			prop = geometry->property ("y-size");
		}
		if (prop) {
			height = atoi (prop->value());
		}
	}

	if (which == static_cast<Gtk::Paned*> (&rhs_pane1)) {

		if (done[0]) {
			return;
		}

		if (!geometry || (prop = geometry->property("mixer-rhs-pane1-pos")) == 0) {
			pos = height / 3;
			snprintf (buf, sizeof(buf), "%d", pos);
		} else {
			pos = atoi (prop->value());
		}

		if ((done[0] = GTK_WIDGET(rhs_pane1.gobj())->allocation.height > pos)) {
			rhs_pane1.set_position (pos);
		}

	} else if (which == static_cast<Gtk::Paned*> (&list_hpane)) {

		if (done[2]) {
			return;
		}

		if (!geometry || (prop = geometry->property("mixer-list-hpane-pos")) == 0) {
			pos = 75;
			snprintf (buf, sizeof(buf), "%d", pos);
		} else {
			pos = atoi (prop->value());
		}

		if ((done[2] = GTK_WIDGET(list_hpane.gobj())->allocation.width > pos)) {
			list_hpane.set_position (pos);
		}
	}
}
void
Mixer_UI::scroll_left ()
{
	if (!scroller.get_hscrollbar()) return;
	Adjustment* adj = scroller.get_hscrollbar()->get_adjustment();
	/* stupid GTK: can't rely on clamping across versions */
	scroller.get_hscrollbar()->set_value (max (adj->get_lower(), adj->get_value() - adj->get_step_increment()));
}

void
Mixer_UI::scroll_right ()
{
	if (!scroller.get_hscrollbar()) return;
	Adjustment* adj = scroller.get_hscrollbar()->get_adjustment();
	/* stupid GTK: can't rely on clamping across versions */
	scroller.get_hscrollbar()->set_value (min (adj->get_upper(), adj->get_value() + adj->get_step_increment()));
}

bool
Mixer_UI::on_key_press_event (GdkEventKey* ev)
{
        /* focus widget gets first shot, then bindings, otherwise
           forward to main window
        */

	if (gtk_window_propagate_key_event (GTK_WINDOW(gobj()), ev)) {
		return true;
	}

	KeyboardKey k (ev->state, ev->keyval);

	if (bindings.activate (k, Bindings::Press)) {
		return true;
	}

        return forward_key_press (ev);
}

bool
Mixer_UI::on_key_release_event (GdkEventKey* ev)
{
	if (gtk_window_propagate_key_event (GTK_WINDOW(gobj()), ev)) {
		return true;
	}

	KeyboardKey k (ev->state, ev->keyval);
	
	if (bindings.activate (k, Bindings::Release)) {
		return true;
	}

        /* don't forward releases */

        return true;
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
		bool const s = Config->get_default_narrow_ms ();
		for (list<MixerStrip*>::iterator i = strips.begin(); i != strips.end(); ++i) {
			(*i)->set_width_enum (s ? Narrow : Wide, this);
		}
	} else if (p == "remote-model") {
		reset_remote_control_ids ();
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
	track_model = ListStore::create (track_columns);
	track_display.set_model (track_model);
	track_display.append_column (_("Strips"), track_columns.text);
	track_display.append_column (_("Show"), track_columns.visible);
	track_display.get_column (0)->set_data (X_("colnum"), GUINT_TO_POINTER(0));
	track_display.get_column (1)->set_data (X_("colnum"), GUINT_TO_POINTER(1));
	track_display.get_column (0)->set_expand(true);
	track_display.get_column (1)->set_expand(false);
	track_display.set_name (X_("EditGroupList"));
	track_display.get_selection()->set_mode (Gtk::SELECTION_NONE);
	track_display.set_reorderable (true);
	track_display.set_headers_visible (true);

	track_model->signal_row_deleted().connect (sigc::mem_fun (*this, &Mixer_UI::track_list_delete));
	track_model->signal_rows_reordered().connect (sigc::mem_fun (*this, &Mixer_UI::track_list_reorder));

	CellRendererToggle* track_list_visible_cell = dynamic_cast<CellRendererToggle*>(track_display.get_column_cell_renderer (1));
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
	ARDOUR_UI::instance()->add_route (this);
}


void
Mixer_UI::update_title ()
{
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
		set_title (title.get_string());

	} else {
		
		WindowTitle title (S_("Window|Mixer"));
		title += Glib::get_application_name ();
		set_title (title.get_string());
	}
}

MixerStrip*
Mixer_UI::strip_by_x (int x)
{
	for (list<MixerStrip*>::iterator i = strips.begin(); i != strips.end(); ++i) {
		int x1, x2, y;

		(*i)->translate_coordinates (*this, 0, 0, x1, y);
		x2 = x1 + (*i)->get_width();

		if (x >= x1 && x <= x2) {
			return (*i);
		}
	}

	return 0;
}

void
Mixer_UI::set_route_targets_for_operation ()
{
	_route_targets.clear ();

	if (!_selection.empty()) {
		_route_targets = _selection.routes;
		return;
	}

	/* nothing selected ... try to get mixer strip at mouse */

	int x, y;
	get_pointer (x, y);
	
	MixerStrip* ms = strip_by_x (x);
	
	if (ms) {
		_route_targets.insert (ms);
	}
}

void
Mixer_UI::monitor_section_going_away ()
{
	if (_monitor_section) {
		out_packer.remove (_monitor_section->tearoff());
		_monitor_section->set_session (0);
	}
}

void
Mixer_UI::toggle_midi_input_active (bool flip_others)
{
	boost::shared_ptr<RouteList> rl (new RouteList);
	bool onoff = false;

	set_route_targets_for_operation ();

	for (RouteUISelection::iterator r = _route_targets.begin(); r != _route_targets.end(); ++r) {
		boost::shared_ptr<MidiTrack> mt = (*r)->midi_track();

		if (mt) {
			rl->push_back ((*r)->route());
			onoff = !mt->input_active();
		}
	}
	
	_session->set_exclusive_input_active (rl, onoff, flip_others);
}

