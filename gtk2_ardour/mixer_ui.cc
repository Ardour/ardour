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

    $Id$
*/

#include <algorithm>
#include <sigc++/bind.h>

#include <gtkmm/accelmap.h>

#include <pbd/lockmonitor.h>
#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/stop_signal.h>

#include <ardour/audioengine.h>
#include <ardour/session.h>
#include <ardour/audio_track.h>
#include <ardour/session_route.h>
#include <ardour/diskstream.h>
#include <ardour/plugin_manager.h>

#include "mixer_ui.h"
#include "mixer_strip.h"
#include "plugin_selector.h"
#include "ardour_ui.h"
#include "prompter.h"
#include "utils.h"
#include "actions.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace sigc;

Mixer_UI::Mixer_UI (AudioEngine& eng)
	: Window (Gtk::WINDOW_TOPLEVEL),
	  engine (eng)
{
	_strip_width = Wide;
	track_menu = 0;
	no_track_list_redisplay = false;

	XMLNode* node = ARDOUR_UI::instance()->mixer_settings();
	set_state (*node);

 	scroller_base.add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
 	scroller_base.set_name ("MixerWindow");
 	scroller_base.signal_button_release_event().connect (mem_fun(*this, &Mixer_UI::strip_scroller_button_release));
	// add as last item of strip packer
	strip_packer.pack_end (scroller_base, true, true);

	scroller.add (strip_packer);
	scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

	track_display_model = ListStore::create (track_display_columns);
	track_display.set_model (track_display_model);
	track_display.append_column (_("Strips"), track_display_columns.text);
	track_display.append_column (_("Visible"), track_display_columns.visible);
	track_display.get_column (0)->set_data (X_("colnum"), GUINT_TO_POINTER(0));
	track_display.get_column (1)->set_data (X_("colnum"), GUINT_TO_POINTER(1));
	track_display.set_name (X_("MixerTrackDisplayList"));
	track_display.get_selection()->set_mode (Gtk::SELECTION_NONE);
	track_display.set_reorderable (true);
	track_display.set_size_request (100, -1);
	track_display.set_headers_visible (true);

	track_display_model->signal_row_deleted().connect (mem_fun (*this, &Mixer_UI::track_list_delete));
	track_display_model->signal_row_changed().connect (mem_fun (*this, &Mixer_UI::track_list_change));

	CellRendererToggle* track_list_visible_cell = dynamic_cast<CellRendererToggle*>(track_display.get_column_cell_renderer (1));
	track_list_visible_cell->property_activatable() = true;
	track_list_visible_cell->property_radio() = false;

	track_display.signal_button_press_event().connect (mem_fun (*this, &Mixer_UI::track_display_button_press), false);

	track_display_scroller.add (track_display);
	track_display_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	group_display_model = ListStore::create (group_display_columns);
	group_display.set_model (group_display_model);
	group_display.append_column (_("active"), group_display_columns.active);
	group_display.append_column (_("groupname"), group_display_columns.text);
	group_display.get_column (0)->set_data (X_("colnum"), GUINT_TO_POINTER(0));
	group_display.get_column (1)->set_data (X_("colnum"), GUINT_TO_POINTER(1));

	/* use checkbox for the active column */

	CellRendererToggle *active_cell = dynamic_cast<CellRendererToggle*>(group_display.get_column_cell_renderer (0));
	active_cell->property_activatable() = true;
	active_cell->property_radio() = false;
	
	group_display.set_name ("MixerGroupList");
	group_display.set_reorderable (true);
	group_display.set_size_request (true);
	group_display_scroller.add (group_display);
	group_display_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	group_display_vbox.pack_start (group_display_button, false, false);
	group_display_vbox.pack_start (group_display_scroller, true, true);

	track_display_frame.set_name("BaseFrame");
	track_display_frame.set_shadow_type (Gtk::SHADOW_IN);
	track_display_frame.add(track_display_scroller);

	group_display_frame.set_name ("BaseFrame");
	group_display_frame.set_shadow_type (Gtk::SHADOW_IN);
	group_display_frame.add (group_display_vbox);

	rhs_pane1.add1 (track_display_frame);
	rhs_pane1.add2 (group_display_frame);

	list_vpacker.pack_start (rhs_pane1, true, true);

	global_hpacker.pack_start (scroller, true, true);
	global_hpacker.pack_start (out_packer, false, false);

	list_hpane.add1(list_vpacker);
	list_hpane.add2(global_hpacker);

	rhs_pane1.signal_size_allocate().connect (bind (mem_fun(*this, &Mixer_UI::pane_allocation_handler), 
							static_cast<Gtk::Paned*> (&rhs_pane1)));
	list_hpane.signal_size_allocate().connect (bind (mem_fun(*this, &Mixer_UI::pane_allocation_handler), 
							 static_cast<Gtk::Paned*> (&list_hpane)));
	

	rhs_pane1.set_data ("collapse-direction", (gpointer) 0);
	list_hpane.set_data ("collapse-direction", (gpointer) 1);

	rhs_pane1.signal_button_release_event().connect (bind (ptr_fun (pane_handler), static_cast<Paned*>(&rhs_pane1)));
	list_hpane.signal_button_release_event().connect (bind (ptr_fun (pane_handler), static_cast<Paned*>(&list_hpane)));
	
	global_vpacker.pack_start (list_hpane, true, true);

	add (global_vpacker);
	set_name ("MixerWindow");
	set_title (_("ardour: mixer"));
	set_wmclass (_("ardour_mixer"), "Ardour");

	add_accel_group (ActionManager::ui_manager->get_accel_group());

	signal_delete_event().connect (bind (ptr_fun (just_hide_it), static_cast<Gtk::Window *>(this)));
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);

	group_display.signal_button_press_event().connect (mem_fun (*this, &Mixer_UI::group_display_button_press));
	group_display.get_selection()->signal_changed().connect (mem_fun (*this, &Mixer_UI::group_display_selection_changed));

	_plugin_selector = new PluginSelector (PluginManager::the_manager());

	signal_configure_event().connect (mem_fun (*ARDOUR_UI::instance(), &ARDOUR_UI::configure_handler));

	_selection.RoutesChanged.connect (mem_fun(*this, &Mixer_UI::follow_strip_selection));
}

Mixer_UI::~Mixer_UI ()
{
}

void
Mixer_UI::ensure_float (Window& win)
{
	win.set_transient_for (*this);
}

void
Mixer_UI::show_window ()
{
	show_all ();

	/* now reset each strips width so the right widgets are shown */
	MixerStrip* ms;

	TreeModel::Children rows = track_display_model->children();
	TreeModel::Children::iterator ri;

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		ms = (*ri)[track_display_columns.strip];
		ms->set_width (ms->get_width());
	}
}

void
Mixer_UI::add_strip (Route* route)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Mixer_UI::add_strip), route));
	
	MixerStrip* strip;

	if (route->hidden()) {
		return;
	}

	strip = new MixerStrip (*this, *session, *route);
	strips.push_back (strip);

	strip->set_width (_strip_width);
	show_strip (strip);

	TreeModel::Row row = *(track_display_model->append());
	row[track_display_columns.text] = route->name();

	if (strip->route().master() || strip->route().control()) {
		row[track_display_columns.visible] = true;
	} else {
		row[track_display_columns.visible] = (strip->marked_for_display() || strip->packed());
	}
	row[track_display_columns.route] = route;
	row[track_display_columns.strip] = strip;

	route->name_changed.connect (bind (mem_fun(*this, &Mixer_UI::strip_name_changed), strip));
	strip->GoingAway.connect (bind (mem_fun(*this, &Mixer_UI::remove_strip), strip));

	strip->signal_button_release_event().connect (bind (mem_fun(*this, &Mixer_UI::strip_button_release_event), strip));

//	if (width() < gdk_screen_width()) {
//		set_size_request (width() + (_strip_width == Wide ? 75 : 50), height());
//	}
}

void
Mixer_UI::remove_strip (MixerStrip* strip)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Mixer_UI::remove_strip), strip));
	
	TreeModel::Children rows = track_display_model->children();
	TreeModel::Children::iterator ri;
	list<MixerStrip *>::iterator i;

	if ((i = find (strips.begin(), strips.end(), strip)) != strips.end()) {
		strips.erase (i);
	}

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		if ((*ri)[track_display_columns.strip] == strip) {
			track_display_model->erase (ri);
			break;
		}
	}
}

void
Mixer_UI::follow_strip_selection ()
{
	for (list<MixerStrip *>::iterator i = strips.begin(); i != strips.end(); ++i) {
		(*i)->set_selected (_selection.selected (&(*i)->route()));
	}
}

bool
Mixer_UI::strip_button_release_event (GdkEventButton *ev, MixerStrip *strip)
{
	if (ev->button == 1) {

		/* this allows the user to click on the strip to terminate comment
		   editing. XXX it needs improving so that we don't select the strip
		   at the same time.
		*/
		
		if (_selection.selected (&strip->route())) {
			_selection.remove (&strip->route());
		} else {
			if (Keyboard::modifier_state_equals (ev->state, Keyboard::Shift)) {
				_selection.add (&strip->route());
			} else {
				_selection.set (&strip->route());
			}
		}
	}

	return true;
}

void
Mixer_UI::connect_to_session (Session* sess)
{
	session = sess;

	string wintitle = _("ardour: mixer: ");
	wintitle += session->name();
	set_title (wintitle);

	initial_track_display ();

	session->going_away.connect (mem_fun(*this, &Mixer_UI::disconnect_from_session));
	session->RouteAdded.connect (mem_fun(*this, &Mixer_UI::add_strip));
	session->mix_group_added.connect (mem_fun(*this, &Mixer_UI::add_mix_group));

	session->foreach_mix_group(this, &Mixer_UI::add_mix_group);
	
	_plugin_selector->set_session (session);

	start_updating ();
}

void
Mixer_UI::disconnect_from_session ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &Mixer_UI::disconnect_from_session));
	
	group_display_model->clear ();
	set_title (_("ardour: mixer"));
	stop_updating ();
}

void
Mixer_UI::show_strip (MixerStrip* ms)
{
        TreeModel::Children rows = track_display_model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {

		MixerStrip* strip = (*i)[track_display_columns.strip];
		if (strip == ms) {
			(*i)[track_display_columns.visible] = true;
			break;
		}
	}
}

void
Mixer_UI::hide_strip (MixerStrip* ms)
{
        TreeModel::Children rows = track_display_model->children();
	TreeModel::Children::iterator i;
	
	for (i = rows.begin(); i != rows.end(); ++i) {
		
		MixerStrip* strip = (*i)[track_display_columns.strip];
		if (strip == ms) {
			(*i)[track_display_columns.visible] = false;
			break;
		}
	 }
 }

 gint
 Mixer_UI::start_updating ()
 {
	 screen_update_connection = ARDOUR_UI::instance()->RapidScreenUpdate.connect (mem_fun(*this, &Mixer_UI::update_strips));
	 fast_screen_update_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect (mem_fun(*this, &Mixer_UI::fast_update_strips));
	 return 0;
 }

 gint
 Mixer_UI::stop_updating ()
 {
	 screen_update_connection.disconnect();
	 fast_screen_update_connection.disconnect();
	 return 0;
 }

 void
 Mixer_UI::update_strips ()
 {
	 if (is_mapped () && session) {
		 for (list<MixerStrip *>::iterator i = strips.begin(); i != strips.end(); ++i) {
			 (*i)->update ();
		 }
	 }
 }

 void
 Mixer_UI::fast_update_strips ()
 {
	 if (is_mapped () && session) {
		 for (list<MixerStrip *>::iterator i = strips.begin(); i != strips.end(); ++i) {
			 (*i)->fast_update ();
		 }
	 }
 }

void
Mixer_UI::set_all_strips_visibility (bool yn)
{
        TreeModel::Children rows = track_display_model->children();
	TreeModel::Children::iterator i;

	no_track_list_redisplay = true;

	for (i = rows.begin(); i != rows.end(); ++i) {

		TreeModel::Row row = (*i);
		MixerStrip* strip = row[track_display_columns.strip];
		
		if (strip == 0) {
			continue;
		}
		
		if (strip->route().master() || strip->route().control()) {
			continue;
		}

		(*i)[track_display_columns.visible] = yn;
	}

	no_track_list_redisplay = false;
	redisplay_track_list ();
}


void
Mixer_UI::set_all_audio_visibility (int tracks, bool yn) 
{
        TreeModel::Children rows = track_display_model->children();
	TreeModel::Children::iterator i;

	no_track_list_redisplay = true;

	for (i = rows.begin(); i != rows.end(); ++i) {
		TreeModel::Row row = (*i);
		MixerStrip* strip = row[track_display_columns.strip];

		if (strip == 0) {
			continue;
		}

		if (strip->route().master() || strip->route().control()) {
			continue;
		}

		AudioTrack* at = dynamic_cast<AudioTrack*> (&strip->route());

		switch (tracks) {
		case 0:
			(*i)[track_display_columns.visible] = yn;
			break;
			
		case 1:
			if (at) { /* track */
				(*i)[track_display_columns.visible] = yn;
			}
			break;
			
		case 2:
			if (!at) { /* bus */
				(*i)[track_display_columns.visible] = yn;
			}
			break;
		}
	}

	no_track_list_redisplay = false;
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
Mixer_UI::track_list_change (const Gtk::TreeModel::Path& path,const Gtk::TreeModel::iterator& iter)
{
	redisplay_track_list ();
}

void
Mixer_UI::track_list_delete (const Gtk::TreeModel::Path& path)
{
	redisplay_track_list ();
}

void
Mixer_UI::redisplay_track_list ()
{
	TreeModel::Children rows = track_display_model->children();
	TreeModel::Children::iterator i;
	long order;

	if (no_track_list_redisplay) {
		return;
	}

	for (order = 0, i = rows.begin(); i != rows.end(); ++i, ++order) {
		MixerStrip* strip = (*i)[track_display_columns.strip];

		bool visible = (*i)[track_display_columns.visible];

		if (visible) {
			strip->set_marked_for_display (true);
			strip->route().set_order_key (N_("signal"), order);

			if (strip->packed()) {

				if (strip->route().master() || strip->route().control()) {
					out_packer.reorder_child (*strip, -1);
				} else {
					strip_packer.reorder_child (*strip, -1); /* put at end */
				}

			} else {

				if (strip->route().master() || strip->route().control()) {
					out_packer.pack_start (*strip, false, false);
				} else {
					strip_packer.pack_start (*strip, false, false);
				}
				strip->set_packed (true);
				strip->show ();
			}

		} else {

			if (strip->route().master() || strip->route().control()) {
				/* do nothing, these cannot be hidden */
			} else {
				strip_packer.remove (*strip);
				strip->set_packed (false);
			}
		}
	}
}

struct SignalOrderRouteSorter {
    bool operator() (Route* a, Route* b) {
	    /* use of ">" forces the correct sort order */
	    return a->order_key ("signal") > b->order_key ("signal");
    }
};

void
Mixer_UI::initial_track_display ()
{
	Session::RouteList routes = session->get_routes();
	SignalOrderRouteSorter sorter;

	routes.sort (sorter);
	
	no_track_list_redisplay = true;

	track_display_model->clear ();

	for (Session::RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {
		add_strip (*i);
	}

	no_track_list_redisplay = false;

	redisplay_track_list ();
}

void
Mixer_UI::show_track_list_menu ()
{
	if (track_menu == 0) {
		build_track_menu ();
	}

	track_menu->popup (1, 0);
}

bool
Mixer_UI::track_display_button_press (GdkEventButton* ev)
{
	if (Keyboard::is_context_menu_event (ev)) {
		show_track_list_menu ();
		return true;
	}

	TreeIter iter;
	TreeModel::Path path;
	TreeViewColumn* column;
	int cellx;
	int celly;
	
	if (!track_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		return false;
	}

	switch (GPOINTER_TO_UINT (column->get_data (X_("colnum")))) {
	case 0:
		/* allow normal processing to occur */
		return false;

	case 1: /* visibility */

		if ((iter = track_display_model->get_iter (path))) {
			MixerStrip* strip = (*iter)[track_display_columns.strip];
			if (strip) {

				if (!strip->route().master() && !strip->route().control()) {
					bool visible = (*iter)[track_display_columns.visible];
					(*iter)[track_display_columns.visible] = !visible;
				}
			}
		}
		return true;

	default:
		break;
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
	
	items.push_back (MenuElem (_("Show All"), mem_fun(*this, &Mixer_UI::show_all_routes)));
	items.push_back (MenuElem (_("Hide All"), mem_fun(*this, &Mixer_UI::hide_all_routes)));
	items.push_back (MenuElem (_("Show All Audio Tracks"), mem_fun(*this, &Mixer_UI::show_all_audiotracks)));
	items.push_back (MenuElem (_("Hide All Audio Tracks"), mem_fun(*this, &Mixer_UI::hide_all_audiotracks)));
	items.push_back (MenuElem (_("Show All Audio Busses"), mem_fun(*this, &Mixer_UI::show_all_audiobus)));
	items.push_back (MenuElem (_("Hide All Audio Busses"), mem_fun(*this, &Mixer_UI::hide_all_audiobus)));

}

void
Mixer_UI::strip_name_changed (void* src, MixerStrip* mx)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Mixer_UI::strip_name_changed), src, mx));
	
	TreeModel::Children rows = track_display_model->children();
	TreeModel::Children::iterator i;
	
	for (i = rows.begin(); i != rows.end(); ++i) {
		if ((*i)[track_display_columns.strip] == mx) {
			(*i)[track_display_columns.text] = mx->route().name();
			return;
		}
	} 

	error << _("track display list item for renamed strip not found!") << endmsg;
}

void
Mixer_UI::new_mix_group ()
{
	ArdourPrompter prompter;
	string result;

	prompter.set_prompt (_("Name for new mix group"));
	prompter.show_all ();
	
	switch (prompter.run ()) {
	case GTK_RESPONSE_ACCEPT:
		prompter.get_result (result);
		if (result.length()) {
			session->add_mix_group (result);
		}	
		break;
	}
}

bool
Mixer_UI::group_display_button_press (GdkEventButton* ev)
{
	RouteGroup* group;

	TreeIter iter;
	TreeModel::Path path;
	TreeViewColumn* column;
	int cellx;
	int celly;

	if (!group_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		return false;
	}
	
	switch (GPOINTER_TO_UINT (column->get_data (X_("colnum")))) {
	case 0:
		/* active column click */
		
		if ((iter = group_display_model->get_iter (path))) {
			/* path points to a valid node */
			if ((group = (*iter)[group_display_columns.group]) != 0) {
				group->set_active (!group->is_active (), this);
			}
		}
		break;

	case 1:
		if (Keyboard::is_edit_event (ev)) {
			// RouteGroup* group = (RouteGroup *) group_display.row(row).get_data ();
			// edit_mix_group (group);

		} else {
			/* allow regular select to occur */
			return false;
		}
		break;
	}
		
	return true;
}

void
Mixer_UI::group_display_selection_changed ()
{
	TreeModel::iterator i;
	TreeModel::Children rows = group_display_model->children();
	Glib::RefPtr<TreeSelection> selection = group_display.get_selection();

	for (i = rows.begin(); i != rows.end(); ++i) {
		RouteGroup* group;

		group = (*i)[group_display_columns.group];

		if (selection->is_selected (i)) {
			group->set_hidden (true, this);
		} else {
			group->set_hidden (true, this);
		}
	}

	redisplay_track_list ();
}

void
Mixer_UI::group_flags_changed (void* src, RouteGroup* group)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Mixer_UI::group_flags_changed), src, group));
	
	TreeModel::iterator i;
	TreeModel::Children rows = group_display_model->children();
	Glib::RefPtr<TreeSelection> selection = group_display.get_selection();
	
	for (i = rows.begin(); i != rows.end(); ++i) {
		if ((*i)[group_display_columns.group] == group) {
			(*i)[group_display_columns.visible] = group->is_hidden ();
			break;
		}
	}
}

void
Mixer_UI::add_mix_group (RouteGroup* group)

{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Mixer_UI::add_mix_group), group));

	TreeModel::Row row = *(group_display_model->append());
	row[group_display_columns.active] = group->is_active();
	row[group_display_columns.visible] = true;
	row[group_display_columns.text] = group->name();
	row[group_display_columns.group] = group;

	group->FlagsChanged.connect (bind (mem_fun(*this, &Mixer_UI::group_flags_changed), group));
}

bool
Mixer_UI::strip_scroller_button_release (GdkEventButton* ev)
{
	using namespace Menu_Helpers;

	if (Keyboard::is_context_menu_event (ev)) {
		ARDOUR_UI::instance()->add_route();
		return true;
	}

	return false;
}

void
Mixer_UI::set_strip_width (Width w)
{
	_strip_width = w;

	for (list<MixerStrip*>::iterator i = strips.begin(); i != strips.end(); ++i) {
		(*i)->set_width (w);
	}
}


int
Mixer_UI::set_state (const XMLNode& node)
{
	const XMLProperty* prop;
	XMLNode* geometry;
	int x, y, width, height, xoff, yoff;
	
	if ((geometry = find_named_node (node, "geometry")) == 0) {

		width = default_width;
		height = default_height;
		x = 1;
		y = 1;
		xoff = 0;
		yoff = 21;

	} else {

		width = atoi(geometry->property("x_size")->value().c_str());
		height = atoi(geometry->property("y_size")->value().c_str());
		x = atoi(geometry->property("x_pos")->value().c_str());
		y = atoi(geometry->property("y_pos")->value().c_str());
		xoff = atoi(geometry->property("x_off")->value().c_str());
		yoff = atoi(geometry->property("y_off")->value().c_str());
	}
		
	set_default_size(width, height);
	// GTK2FIX
	// set_uposition(x, y-yoff);

	if ((prop = node.property ("narrow-strips"))) {
		if (prop->value() == "yes") {
			set_strip_width (Narrow);
		} else {
			set_strip_width (Wide);
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
		
		int x, y, xoff, yoff, width, height;
		win->get_root_origin(x, y);
		win->get_position(xoff, yoff);
		win->get_size(width, height);

		XMLNode* geometry = new XMLNode ("geometry");
		char buf[32];
		snprintf(buf, sizeof(buf), "%d", width);
		geometry->add_property(X_("x_size"), string(buf));
		snprintf(buf, sizeof(buf), "%d", height);
		geometry->add_property(X_("y_size"), string(buf));
		snprintf(buf, sizeof(buf), "%d", x);
		geometry->add_property(X_("x_pos"), string(buf));
		snprintf(buf, sizeof(buf), "%d", y);
		geometry->add_property(X_("y_pos"), string(buf));
		snprintf(buf, sizeof(buf), "%d", xoff);
		geometry->add_property(X_("x_off"), string(buf));
		snprintf(buf, sizeof(buf), "%d", yoff);
		geometry->add_property(X_("y_off"), string(buf));

		snprintf(buf,sizeof(buf), "%d",gtk_paned_get_position (static_cast<Paned*>(&rhs_pane1)->gobj()));
		geometry->add_property(X_("mixer_rhs_pane1_pos"), string(buf));
		snprintf(buf,sizeof(buf), "%d",gtk_paned_get_position (static_cast<Paned*>(&list_hpane)->gobj()));
		geometry->add_property(X_("mixer_list_hpane_pos"), string(buf));

		node->add_child_nocopy (*geometry);
	}

	node->add_property ("narrow-strips", _strip_width == Narrow ? "yes" : "no");

	return *node;
}


void 
Mixer_UI::pane_allocation_handler (Allocation& alloc, Gtk::Paned* which)
{
	int pos;
	XMLProperty* prop = 0;
	char buf[32];
	XMLNode* node = ARDOUR_UI::instance()->mixer_settings();
	XMLNode* geometry;
	int width, height;
	static int32_t done[3] = { 0, 0, 0 };

	if ((geometry = find_named_node (*node, "geometry")) == 0) {
		width = default_width;
		height = default_height;
	} else {
		width = atoi(geometry->property("x_size")->value());
		height = atoi(geometry->property("y_size")->value());
	}

	if (which == static_cast<Gtk::Paned*> (&rhs_pane1)) {

		if (done[0]) {
			return;
		}

		if (!geometry || (prop = geometry->property("mixer_rhs_pane1_pos")) == 0) {
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

		if (!geometry || (prop = geometry->property("mixer_list_hpane_pos")) == 0) {
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

