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

#include <algorithm>
#include <cstdlib>
#include <cmath>

#include "editor.h"
#include "ardour_ui.h"
#include "audio_time_axis.h"
#include "mixer_strip.h"
#include "gui_thread.h"

#include <ardour/route.h>

#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace Gtk;

void
Editor::handle_new_route_p (Route* route)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::handle_new_route_p), route));
	handle_new_route (*route);
}

void
Editor::handle_new_route (Route& route)
{
	TimeAxisView *tv;
	AudioTimeAxisView *atv;
	TreeModel::Row row = *(route_display_model->append());

	if (route.hidden()) {
		return;
	}
		
	tv = new AudioTimeAxisView (*this, *session, route, track_canvas);

	track_views.push_back (tv);

	row[route_display_columns.text] = route.name();
	row[route_display_columns.tv] = tv;

	ignore_route_list_reorder = true;
	
	if (!no_route_list_redisplay && tv->marked_for_display()) {
	        route_list_display.get_selection()->select (row);
	}

	if ((atv = dynamic_cast<AudioTimeAxisView*> (tv)) != 0) {
		/* added a new fresh one at the end */
		if (atv->route().order_key(N_("editor")) == -1) {
		        atv->route().set_order_key (N_("editor"), route_display_model->children().size()-1);
		}
	}

	ignore_route_list_reorder = false;
	
	route.gui_changed.connect (mem_fun(*this, &Editor::handle_gui_changes));

	tv->GoingAway.connect (bind (mem_fun(*this, &Editor::remove_route), tv));
	
	editor_mixer_button.set_sensitive(true);
}

void
Editor::handle_gui_changes (const string & what, void *src)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::handle_gui_changes), what, src));
	
	if (what == "track_height") {
		redisplay_route_list ();
	}
}


void
Editor::remove_route (TimeAxisView *tv)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::remove_route), tv));

	
	TrackViewList::iterator i;
	TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator ri;

	if ((i = find (track_views.begin(), track_views.end(), tv)) != track_views.end()) {
		track_views.erase (i);
	}

	for (ri = rows.begin(); ri != rows.end(); ++ri) {
		if ((*ri)[route_display_columns.tv] == tv) {
			route_display_model->erase (ri);
			break;
		}
	}
	/* since the editor mixer goes away when you remove a route, set the
	 * button to inacttive 
	 */
	editor_mixer_button.set_active(false);

	/* and disable if all tracks and/or routes are gone */

	if (track_views.size() == 0) {
		editor_mixer_button.set_sensitive(false);
	}
}

void
Editor::route_name_changed (TimeAxisView *tv)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::route_name_changed), tv));
	
	TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator i;
	
	for (i = rows.begin(); i != rows.end(); ++i) {
		if ((*i)[route_display_columns.tv] == tv) {
			(*i)[route_display_columns.text] = tv->name();
			break;
		}
	} 

}

void
Editor::route_display_selection_changed ()
{
	TimeAxisView *tv;
	TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator i;
	Glib::RefPtr<TreeSelection> selection = route_list_display.get_selection();

	for (i = rows.begin(); i != rows.end(); ++i) {
	        tv = (*i)[route_display_columns.tv];

		if (!selection->is_selected (i)) {
			tv->set_marked_for_display  (false);
		} else {
		        AudioTimeAxisView *atv;
			tv->set_marked_for_display (true);
			if ((atv = dynamic_cast<AudioTimeAxisView*>(tv)) != 0) {
			        if (current_mixer_strip && &(atv->route()) == &(current_mixer_strip->route())) {
				        // this will hide the mixer strip
				        set_selected_mixer_strip(*atv);
				}
			}
		}
	}
	
	redisplay_route_list ();
}

void
Editor::unselect_strip_in_display (TimeAxisView& tv)
{
	TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator i;
	Glib::RefPtr<TreeSelection> selection = route_list_display.get_selection();
	
	for (i = rows.begin(); i != rows.end(); ++i) {
		if ((*i)[route_display_columns.tv] == &tv) { 
		       selection->unselect (*i);
		}
	}
}

void
Editor::select_strip_in_display (TimeAxisView* tv)
{
	TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator i;
	Glib::RefPtr<TreeSelection> selection = route_list_display.get_selection();
	
	for (i = rows.begin(); i != rows.end(); ++i) {
		if ((*i)[route_display_columns.tv] == tv) { 
		       selection->select (*i);
		}
	}
}

void
Editor::route_list_reordered (const TreeModel::Path& path,const TreeModel::iterator& iter,int* what)
{
	redisplay_route_list ();
}

void
Editor::redisplay_route_list ()
{
	TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator i;
	uint32_t position;
	uint32_t order;
	int n;

	if (no_route_list_redisplay) {
		return;
	}

	for (n = 0, order = 0, position = 0, i = rows.begin(); i != rows.end(); ++i, ++order) {
		TimeAxisView *tv = (*i)[route_display_columns.tv];
		AudioTimeAxisView* at; 

		if (!ignore_route_list_reorder) {
			
			/* this reorder is caused by user action, so reassign sort order keys
			   to tracks.
			*/
			
			if ((at = dynamic_cast<AudioTimeAxisView*> (tv)) != 0) {
				at->route().set_order_key (N_("editor"), order);
			}
		}
		if (tv->marked_for_display()) {
			position += tv->show_at (position, n, &edit_controls_vbox);
			position += track_spacing;
		} else {
			tv->hide ();
		}
		
		n++;
		
	}

	controls_layout.queue_resize ();
	reset_scrolling_region ();
}

void
Editor::hide_all_tracks (bool with_select)
{
	TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator i;

	// GTK2FIX
	// track_display_list.freeze ();
	
	for (i = rows.begin(); i != rows.end(); ++i) {
		
		TreeModel::Row row = (*i);
		TimeAxisView *tv = row[route_display_columns.tv];
		
		if (with_select) {
			route_list_display.get_selection()->unselect (i);
		} else {
		        tv->set_marked_for_display (false);
			tv->hide();
		
		}
	}
	//route_list_display.thaw ();
	reset_scrolling_region ();
}

void
Editor::build_route_list_menu ()
{
        using namespace Menu_Helpers;
	using namespace Gtk;


	route_list_menu = new Menu;
	
	MenuList& items = route_list_menu->items();
	route_list_menu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Show All"), mem_fun(*this, &Editor::select_all_routes)));
	items.push_back (MenuElem (_("Hide All"), mem_fun(*this, &Editor::unselect_all_routes)));
	items.push_back (MenuElem (_("Show All AbstractTracks"), mem_fun(*this, &Editor::select_all_audiotracks)));
	items.push_back (MenuElem (_("Hide All AbstractTracks"), mem_fun(*this, &Editor::unselect_all_audiotracks)));
	items.push_back (MenuElem (_("Show All AudioBus"), mem_fun(*this, &Editor::select_all_audiobus)));
	items.push_back (MenuElem (_("Hide All AudioBus"), mem_fun(*this, &Editor::unselect_all_audiobus)));

}

void
Editor::unselect_all_routes ()
{
	hide_all_tracks (true);
}

void
Editor::select_all_routes ()

{
        TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
		route_list_display.get_selection()->select (i);
	}
}

void
Editor::select_all_audiotracks () 
{
        TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
	TreeModel::Row row = (*i);
	        TimeAxisView* tv = row[route_display_columns.tv];
		AudioTimeAxisView* atv;

		if ((atv = dynamic_cast<AudioTimeAxisView*>(tv)) != 0) {
			if (atv->is_audio_track()) {
			        route_list_display.get_selection()->select (i);

			}
		}
	}

}

void
Editor::unselect_all_audiotracks () 
{
        TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
	        TreeModel::Row row = (*i);
	        TimeAxisView *tv = row[route_display_columns.tv];
		AudioTimeAxisView* atv;

		if ((atv = dynamic_cast<AudioTimeAxisView*>(tv)) != 0) {
			if (atv->is_audio_track()) {
			        route_list_display.get_selection()->unselect (i);

			}
		}
	}

}

void
Editor::select_all_audiobus () 
{
        TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
	        TreeModel::Row row = (*i);
	        TimeAxisView* tv = row[route_display_columns.tv];
		AudioTimeAxisView* atv;

		if ((atv = dynamic_cast<AudioTimeAxisView*>(tv)) != 0) {
			if (!atv->is_audio_track()) {
			        route_list_display.get_selection()->select (i);

			}
		}
	}

}

void
Editor::unselect_all_audiobus () 
{
        TreeModel::Children rows = route_display_model->children();
	TreeModel::Children::iterator i;

	for (i = rows.begin(); i != rows.end(); ++i) {
	        TreeModel::Row row = (*i);
	        TimeAxisView* tv = row[route_display_columns.tv];
		AudioTimeAxisView* atv;

		if ((atv = dynamic_cast<AudioTimeAxisView*>(tv)) != 0) {
			if (!atv->is_audio_track()) {
			        route_list_display.get_selection()->unselect (i);

			}
		}
	}

}

gint
Editor::route_list_compare_func (TreeModel::iterator a, TreeModel::iterator b)
{
	TimeAxisView *tv1;
	TimeAxisView *tv2;
	AudioTimeAxisView *atv1;
	AudioTimeAxisView *atv2;
	Route* ra;
	Route* rb;

	tv1 = (*a)[route_display_columns.tv];
	tv2 = (*b)[route_display_columns.tv];

	if ((atv1 = dynamic_cast<AudioTimeAxisView*>(tv1)) == 0 ||
	    (atv2 = dynamic_cast<AudioTimeAxisView*>(tv2)) == 0) {
		return FALSE;
	}

	ra = &atv1->route();
	rb = &atv2->route();

	/* use of ">" forces the correct sort order */

	return ra->order_key ("editor") > rb->order_key ("editor");
}

