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

#include <pbd/lockmonitor.h>
#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/stop_signal.h>

#include <ardour/audioengine.h>
#include <ardour/session.h>
#include <ardour/session_route.h>
#include <ardour/diskstream.h>
#include <ardour/plugin_manager.h>

#include "mixer_ui.h"
#include "mixer_strip.h"
#include "plugin_selector.h"
#include "ardour_ui.h"
#include "check_mark.h"
#include "prompter.h"
#include "utils.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace sigc;

static const gchar* group_list_titles[] = { 
	N_("***"), 
	N_("Bar"),
	0
};

GdkPixmap* Mixer_UI::check_pixmap = 0;
GdkBitmap* Mixer_UI::check_mask = 0;
GdkPixmap* Mixer_UI::empty_pixmap = 0;
GdkBitmap* Mixer_UI::empty_mask = 0;


Mixer_UI::Mixer_UI (AudioEngine& eng)
	: Gtk::Window (GTK_WINDOW_TOPLEVEL),
	  KeyboardTarget (*this, "mixer"),
	  engine (eng),
{
	_strip_width = Wide;
	track_menu = 0;

	check_pixmap = gdk_pixmap_colormap_create_from_xpm_d (NULL, 
		      gtk_widget_get_colormap (GTK_WIDGET(group_list.gobj())),
		      &check_mask, NULL, (gchar **) check_xpm);
	empty_pixmap = gdk_pixmap_colormap_create_from_xpm_d (NULL, 
		      gtk_widget_get_colormap (GTK_WIDGET(group_list.gobj())),
		      &empty_mask, NULL, (gchar **) empty_xpm);

	XMLNode* node = ARDOUR_UI::instance()->mixer_settings();
	set_state (*node);


 	scroller_base.signal_add_event()s (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
 	scroller_base.set_name ("MixerWindow");
 	scroller_base.signal_button_release_event().connect (mem_fun(*this, &Mixer_UI::strip_scroller_button_release));
	// add as last item of strip packer
	strip_packer.pack_end (scroller_base, true, true);

	scroller.add_with_viewport (strip_packer);
	scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

	track_display_model = ListStore::create (track_display_columns);
	track_display.set_model (track_display_model);
	track_display.append_column (_("Strips"));
	track_display.set_name (X_("MixerTrackDisplayList"));
	track_display.set_selection_mode (GTK_SELECTION_MULTIPLE);
	track_display.set_reorderable (true);
	track_display.set_size_request (75, -1);
	track_display.set_headers_visible (true);
	track_display.set_headers_clickable (true);
	track_display_scroller.add (track_display_list);
	track_display_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	group_display_model = ListStore::create (group_display_columns);
	group_display.set_model (group_display_model);
	group_display.append_column (_("Mix Groups"));
	group_display.set_name ("MixerGroupList");
	group_display.set_reorderable (true);
	group_display.set_size_request (true);
	group_display.set_headers_visible (true);
	group_display.set_headers_clickable (true);
	group_display_scroller.add (group_list);
	group_display_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	snapshot_display_model = ListStore::create (group_display_columns);
	snapshot_display.set_model (snapshot_display_model);
	snapshot_display.append_column (_("Mix Groups"));
	snapshot_display.set_name ("MixerSnapshotDisplayList");
	snapshot_display.set_size_request (75, -1);
	snapshot_display.set_reorderable (true);
	snapshot_display_scroller.add (snapshot_display);
	snapshot_display_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	group_list_vbox.pack_start (group_list_button, false, false);
	group_list_vbox.pack_start (group_list_scroller, true, true);

	list<string> stupid_list;

	stupid_list.push_back ("");
	stupid_list.push_back (_("-all-"));

	group_list.rows().push_back (stupid_list);
	group_list.rows().back().set_data (0);
	group_list.rows().back().select();
	group_list.cell(0,0).set_pixmap (check_pixmap);

	track_display_frame.set_name("BaseFrame");
	track_display_frame.set_shadow_type (Gtk::SHADOW_IN);
	track_display_frame.add(track_display_scroller);

	group_list_frame.set_name ("BaseFrame");
	group_list_frame.set_shadow_type (Gtk::SHADOW_IN);
	group_list_frame.add (group_list_vbox);

	rhs_pane1.add1 (track_display_frame);
	rhs_pane1.add2 (rhs_pane2);

	rhs_pane2.add1 (group_list_frame);
	rhs_pane2.add2 (snapshot_display_scroller);

	list_vpacker.pack_start (rhs_pane1, true, true);

	global_hpacker.pack_start (scroller, true, true);
	global_hpacker.pack_start (out_packer, false, false);

	list_hpane.add1(list_vpacker);
	list_hpane.add2(global_hpacker);

	rhs_pane1.size_allocate.connect_after (bind (mem_fun(*this, &Mixer_UI::pane_allocation_handler), 
						      static_cast<Gtk::Paned*> (&rhs_pane1)));
	rhs_pane2.size_allocate.connect_after (bind (mem_fun(*this, &Mixer_UI::pane_allocation_handler), 
						      static_cast<Gtk::Paned*> (&rhs_pane2)));
	list_hpane.size_allocate.connect_after (bind (mem_fun(*this, &Mixer_UI::pane_allocation_handler), 
						      static_cast<Gtk::Paned*> (&list_hpane)));


	rhs_pane1.set_data ("collapse-direction", (gpointer) 0);
	rhs_pane2.set_data ("collapse-direction", (gpointer) 0);
	list_hpane.set_data ("collapse-direction", (gpointer) 1);

	rhs_pane1.signal_button_release_event().connect (bind (ptr_fun (pane_handler), static_cast<Paned*>(&rhs_pane1)));
	rhs_pane2.signal_button_release_event().connect (bind (ptr_fun (pane_handler), static_cast<Paned*>(&rhs_pane2)));
	list_hpane.signal_button_release_event().connect (bind (ptr_fun (pane_handler), static_cast<Paned*>(&list_hpane)));
	
	global_vpacker.pack_start (list_hpane, true, true);

	add (global_vpacker);
	set_name ("MixerWindow");
	set_title (_("ardour: mixer"));
	set_wmclass (_("ardour_mixer"), "Ardour");

	delete_event.connect (bind (ptr_fun (just_hide_it), static_cast<Gtk::Window *>(this)));
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);

	track_display.get_selection()->signal_changed().connect (mem_fun(*this, &Mixer_UI::track_display_selection_changed));
	track_display_model.signal_rows_reordered().connect (mem_fun (*this, &Mixer_UI::track_display_reordered));
	track_display.signal_button_press_event().connect_notify (mem_fun (*this, &Mixer_UI::track_display_button_press));

	group_display.signal_button_press_event().connect_notify (mem_fun (*this, &Mixer_UI::group_display_button_press));
	group_display.get_selection()->signal_changed().connect (mem_fun (*this, &Mixer_UI::group_display_selection_changed));

	snapshot_display.get_selection()->signal_changed().connect (mem_fun(*this, &Mixer_UI::snapshot_display_selection_changed));
	snapshot_display.signal_button_press_event().connect_notify (mem_fun (*this, &Mixer_UI::snapshot_display_button_press));

	_plugin_selector = new PluginSelector (PluginManager::the_manager());
	_plugin_selector->signal_delete_event().connect (bind (ptr_fun (just_hide_it), 
						     static_cast<Window *> (_plugin_selector)));

	configure_event.connect (mem_fun (*ARDOUR_UI::instance(), &ARDOUR_UI::configure_handler));

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
	CList_Helpers::RowList::iterator i;

	for (i = track_display_list.rows().begin(); i != track_display_list.rows().end(); ++i) {
		ms = (MixerStrip *) i->get_data ();
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

	TreeModel::
	TreeModel::Row row = *(track_display_model->append());
	row[columns.text] = route->name();
	row[columns.data] = strip;

	if (strip->marked_for_display() || strip->packed()) {
		track_display.get_selection()->select (row);
	}
	
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
	
	TreeModel::Children::iterator rows = track_display_model.children();
	TreeModel::Children::iterator ri;
	list<MixerStrip *>::iterator i;

	if ((i = find (strips.begin(), strips.end(), strip)) != strips.end()) {
		strips.erase (i);
	}

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		if ((MixerStrip *) ri->get_data () == strip) {
			track_display_model.erase (ri);
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

gint
Mixer_UI::strip_button_release_event (GdkEventButton *ev, MixerStrip *strip)
{
	if (ev->button == 1) {

		/* this allows the user to click on the strip to terminate comment
		   editing. XXX it needs improving so that we don't select the strip
		   at the same time.
		*/
		
		ARDOUR_UI::instance()->allow_focus (false);
		
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

	return TRUE;
}

void
Mixer_UI::connect_to_session (Session* sess)
{
	session = sess;

	string wintitle = _("ardour: mixer: ");
	wintitle += session->name();
	set_title (wintitle);

	// GTK2FIX
	// track_display_list.freeze ();

	track_display_model.clear ();

	session->foreach_route (this, &Mixer_UI::add_strip);
	
	// track_display_list.thaw ();

	session->going_away.connect (mem_fun(*this, &Mixer_UI::disconnect_from_session));
	session->RouteAdded.connect (mem_fun(*this, &Mixer_UI::add_strip));
	session->mix_group_added.connect (mem_fun(*this, &Mixer_UI::add_mix_group));

	session->foreach_mix_group(this, &Mixer_UI::add_mix_group);
	
	session->StateSaved.connect (mem_fun(*this, &Mixer_UI::session_state_saved));
	redisplay_snapshots ();
	
	_plugin_selector->set_session (session);

	start_updating ();
}

void
Mixer_UI::disconnect_from_session ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &Mixer_UI::disconnect_from_session));
	
	group_list.clear ();
	set_title (_("ardour: mixer"));
	stop_updating ();
	hide_all_strips (false);
}

void
Mixer_UI::hide_all_strips (bool with_select)
{
	MixerStrip* ms;
	TreeModel::Children rows = track_display_model->children();
	TreeModel::Children::iterator i;

	// GTK2FIX
	// track_display_list.freeze ();
	
	for (i = rows.begin(); i != rows.end(); ++i) {
		
		TreeModel::Row row = (*i);
		MixerStrip* ms = row[columns.data];
		
		if (with_select) {
			track_display.get_selection()->unselect (i);
		} else {
			hide_strip (ms);
		}
	}

	// track_display_list.thaw ();
}

void
Mixer_UI::unselect_all_strips ()
{
	hide_all_strips (false);
}

void
Mixer_UI::select_all_strips ()
{
	TreeModel::Children rows = track_display_model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		track_display.get_selection()->select (i);
	}
}

void
Mixer_UI::strip_select_op (bool audiotrack, bool select)
{
	MixerStrip* ms;
	TreeModel::Children rows = track_display_model->children();
	TreeModel::Children::iterator i;

	// GTK2FIX
	// track_display_list.freeze ();
	
	for (i = rows.begin(); i != rows.end(); ++i) {
		ms = (MixerStrip *) (*i)[columns.data];

		if (ms->is_audio_track() == audiotrack) {
			if (select) {
				track_display.get_selection()->select (i);
			} else {
				track_display.get_selection()->unselect (i);
			}
		}
	}
	
	// track_display_list.thaw ();	
}

void
Mixer_UI::select_all_audiotrack_strips ()
{
	strip_select_op (true, true);
}
void
Mixer_UI::unselect_all_audiotrack_strips ()
{
	strip_select_op (true, false);
}

void
Mixer_UI::select_all_audiobus_strips ()
{
	strip_select_op (false, true);
}

void
Mixer_UI::select_all_audiobus_strips ()
{
	strip_select_op (false, false);
}

void
Mixer_UI::show_strip (MixerStrip* ms)
{
	if (!ms->packed()) {
		
		if (ms->route().master() || ms->route().control()) {
			out_packer.pack_start (*ms, false, false);
		} else {
			strip_packer.pack_start (*ms, false, false);
		}
		ms->set_packed (true);
		ms->show ();

	}
}

void
Mixer_UI::hide_strip (MixerStrip* ms)
{
	if (ms->packed()) {
		if (ms->route().master() || ms->route().control()) {
			out_packer.remove (*ms);
		} else {
			strip_packer.remove (*ms);
		}
		ms->set_packed (false);
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
Mixer_UI::snapshot_display_selected (gint row, gint col, GdkEvent* ev)
{
	string* snap_name;
	
	if ((snap_name = (string*) snapshot_display.get_row_data(row)) != 0){
		if (session->snap_name() == *snap_name){
			return;
		}
		string path = session->path();
		ARDOUR_UI::instance()->load_session(path, *snap_name);
	}
}

void
Mixer_UI::track_display_selection_changed ()
{
	MixerStrip* strip;

	TreeModel::Children rows = track_display_model->children();
	TreeModel::Children::iterator i;
	Glib::RefPtr<TreeViewSelection> selection = track_display.get_selection();

	for (i = rows.begin(); i != rows.end(); ++i) {
		if (selection->is_selected (i)) {
			strip->set_marked_for_display  (true);
			show_strip (strip);
		} else {
			strip->set_marked_for_display (false);
			hide_strip (strip);
		}
	}
	
	track_display_reordered ();
}

void
Mixer_UI::unselect_strip_in_display (MixerStrip *strip)
{
	CList_Helpers::RowIterator i;

	if ((i = track_display_list.rows().find_data (strip)) != track_display_list.rows().end()) {
		(*i).unselect ();
	}
}

void
Mixer_UI::select_strip_in_display (MixerStrip *strip)
{
	CList_Helpers::RowIterator i;

	if ((i = track_display_list.rows().find_data (strip)) != track_display_list.rows().end()) {
		(*i).select ();
	}
}

void
Mixer_UI::queue_track_display_reordered (gint arg1, gint arg2)
{
	/* the problem here is that we are called *before* the
	   list has been reordered. so just queue up
	   the actual re-drawer to happen once the re-ordering
	   is complete.
	*/

	Main::idle.connect (mem_fun(*this, &Mixer_UI::track_display_reordered));
}

int
Mixer_UI::track_display_reordered ()
{
	CList_Helpers::RowList::iterator i;
	long order;

	// hide_all_strips (false);

	for (order = 0, i  = track_display_list.rows().begin(); i != track_display_list.rows().end(); ++i, ++order) {
		MixerStrip* strip = (MixerStrip *) (*i)->get_data ();
		if (strip->marked_for_display()) {
			strip->route().set_order_key (N_("signal"), order);
			strip_packer.reorder_child (*strip, -1); /* put at end */
		}
	}

	return FALSE;
}

void
Mixer_UI::track_column_click (gint col)
{
	if (track_menu == 0) {
		build_track_menu ();
	}

	track_menu->popup (0, 0);
}

void
Mixer_UI::build_track_menu ()
{
	using namespace Menu_Helpers;
	using namespace Gtk;

	track_menu = new Menu;
	track_menu->set_name ("ArdourContextMenu");
	MenuList& items = track_menu->items();
	track_menu->set_name ("ArdourContextMenu");
	
	items.push_back (MenuElem (_("Show All"), mem_fun(*this, &Mixer_UI::select_all_strips)));
	items.push_back (MenuElem (_("Hide All"), mem_fun(*this, &Mixer_UI::unselect_all_strips)));
	items.push_back (MenuElem (_("Show All AudioTrack MixerStrips"), mem_fun(*this, &Mixer_UI::select_all_audiotrack_strips)));
	items.push_back (MenuElem (_("Hide All AudioTrack MixerStrips"), mem_fun(*this, &Mixer_UI::unselect_all_audiotrack_strips)));
	items.push_back (MenuElem (_("Show All AudioBus MixerStrips"), mem_fun(*this, &Mixer_UI::select_all_audiobus_strips)));
	items.push_back (MenuElem (_("Hide All AudioBus MixerStrips"), mem_fun(*this, &Mixer_UI::unselect_all_audiobus_strips)));

}

void
Mixer_UI::strip_name_changed (void* src, MixerStrip* mx)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Mixer_UI::strip_name_changed), src, mx));
	
	CList_Helpers::RowList::iterator i;

	if ((i = track_display_list.rows().find_data (mx)) == track_display_list.rows().end()) {
		error << _("track display list item for renamed strip not found!") << endmsg;
		return;
	}

	track_display_list.cell ((*i)->get_row_num(), 0).set_text (mx->route().name());
}

void
Mixer_UI::new_mix_group ()
{
	ArdourPrompter prompter;
	string result;

	prompter.set_prompt (_("Name for new mix group"));
	prompter.done.connect (Main::quit.slot());

	prompter.show_all ();
	
	Main::run ();
	
	if (prompter.status != Gtkmm2ext::Prompter::entered) {
		return;
	}
	
	prompter.get_result (result);

	if (result.length()) {
		session->add_mix_group (result);
	}
}

void
Mixer_UI::group_list_button_clicked ()
{
	if (session) {
		new_mix_group ();
	}
}

gint
Mixer_UI::group_list_button_press_event (GdkEventButton* ev)
{
	gint row, col;
	RouteGroup* group;

	if (group_list.get_selection_info ((int)ev->x, (int)ev->y, &row, &col) == 0) {
		return FALSE;
	}

	switch (col) {
	case 1:
		if (Keyboard::is_edit_event (ev)) {
			// RouteGroup* group = (RouteGroup *) group_list.row(row).get_data ();

			// edit_mix_group (group);

			return stop_signal (group_list, "button_press_event");

		} else {
			/* allow regular select to occur */
			return FALSE;
		}

	case 0:
		stop_signal (group_list, "button_press_event");
		if ((group = (RouteGroup *) group_list.row(row).get_data ()) != 0) {
			group->set_active (!group->is_active (), this);
		}
		break;
	}
		
	return TRUE;
}

void
Mixer_UI::group_selected (gint row, gint col, GdkEvent* ev)
{
	RouteGroup* group = (RouteGroup *) group_list.row(row).get_data ();

	if (group) {
		group->set_hidden (false, this);
	} else {
		/* the master group */

		for (list<MixerStrip *>::iterator i = strips.begin(); i != strips.end(); ++i) {
			show_strip (*i);
		}

		track_display_reordered ();
	}
}

void
Mixer_UI::group_unselected (gint row, gint col, GdkEvent* ev)

{
	RouteGroup* group = (RouteGroup *) group_list.row(row).get_data ();

	if (group) {
		group->set_hidden (true, this);
	} else {
		/* the master group */

		for (list<MixerStrip *>::iterator i = strips.begin(); i != strips.end(); ++i) {
			hide_strip (*i);
		}
	}
}

void
Mixer_UI::group_flags_changed (void* src, RouteGroup* group)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Mixer_UI::group_flags_changed), src, group));
	
	if (src != this) {
		// select row
	}

	CList_Helpers::RowIterator ri = group_list.rows().find_data (group);

	for (list<MixerStrip *>::iterator i = strips.begin(); i != strips.end(); ++i) {
		if ((*i)->route().mix_group() == group) {
			if (group->is_hidden ()) {
				unselect_strip_in_display(*i);
			} else {
				select_strip_in_display(*i);
				(*ri)->select();
			}
		}
	}

	if (group->is_active()) {
		group_list.cell (ri->get_row_num(),0).set_pixmap (check_pixmap, check_mask);
	} else {
		group_list.cell (ri->get_row_num(),0).set_pixmap (empty_pixmap, empty_mask);
	}
}

void
Mixer_UI::add_mix_group (RouteGroup* group)

{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Mixer_UI::add_mix_group), group));
	
	list<string> names;

	names.push_back ("");
	names.push_back (group->name());

	group_list.rows().push_back (names);
	group_list.rows().back().set_data (group);
	
	/* update display to reflect group flags */

	group_flags_changed (0, group);

	group->FlagsChanged.connect (bind (mem_fun(*this, &Mixer_UI::group_flags_changed), group));
}

void
Mixer_UI::redisplay_snapshots ()
{
	if (session == 0) {
		return;
	}

	vector<string*>* states;
	if ((states = session->possible_states()) == 0) {
		return;
	}

	snapshot_display_model.clear ();

	for (vector<string*>::iterator i = states->begin(); i != states->end(); ++i) {
		string statename = *(*i);
		row = *(snapshot_display_model->append());
		row[visible] = statename;
		row[hidden] = statename;
	}

	delete states;
}

void
Mixer_UI::session_state_saved (string snap_name)
{
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &Mixer_UI::session_state_saved), snap_name));
	redisplay_snapshots ();
}

gint
Mixer_UI::strip_scroller_button_release (GdkEventButton* ev)
{
	using namespace Menu_Helpers;

	if (Keyboard::is_context_menu_event (ev)) {
		ARDOUR_UI::instance()->add_route();
		return TRUE;
	}

	return FALSE;
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
	set_uposition(x, y-yoff);

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
		Gdk_Window win = get_window();
		
		int x, y, xoff, yoff, width, height;
		win.get_root_origin(x, y);
		win.get_position(xoff, yoff);
		win.get_size(width, height);

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
		snprintf(buf,sizeof(buf), "%d",gtk_paned_get_position (static_cast<Paned*>(&rhs_pane2)->gobj()));
		geometry->add_property(X_("mixer_rhs_pane2_pos"), string(buf));
		snprintf(buf,sizeof(buf), "%d",gtk_paned_get_position (static_cast<Paned*>(&list_hpane)->gobj()));
		geometry->add_property(X_("mixer_list_hpane_pos"), string(buf));

		node->add_child_nocopy (*geometry);
	}

	node->add_property ("narrow-strips", _strip_width == Narrow ? "yes" : "no");

	return *node;
}


void 
Mixer_UI::pane_allocation_handler (GtkAllocation *alloc, Gtk::Paned* which)
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

	} else if (which == static_cast<Gtk::Paned*> (&rhs_pane2)) {

		if (done[1]) {
			return;
		}

		if (!geometry || (prop = geometry->property("mixer_rhs_pane2_pos")) == 0) {
			pos = height / 3;
			snprintf (buf, sizeof(buf), "%d", pos);
		} else {
			pos = atoi (prop->value());
		}

		if ((done[1] = GTK_WIDGET(rhs_pane2.gobj())->allocation.height > pos)) {
			rhs_pane2.set_position (pos);
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

