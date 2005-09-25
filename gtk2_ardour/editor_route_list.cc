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
	const gchar *rowdata[1];

	if (route.hidden()) {
		return;
	}
		
	tv = new AudioTimeAxisView (*this, *session, route, track_canvas);

	track_views.push_back (tv);

	rowdata[0] = route.name ().c_str();

	ignore_route_list_reorder = true;
	route_list.rows().push_back (rowdata);
	route_list.rows().back().set_data (tv);
	if (tv->marked_for_display()) {
		route_list.rows().back().select();
	}

	if ((atv = dynamic_cast<AudioTimeAxisView*> (tv)) != 0) {
		/* added a new fresh one at the end */
		if (atv->route().order_key(N_("editor")) == -1) {
			atv->route().set_order_key (N_("editor"), route_list.rows().size()-1);
		}
	}

	ignore_route_list_reorder = false;
	
	route.gui_changed.connect (mem_fun(*this, &Editor::handle_gui_changes));

	tv->GoingAway.connect (bind (mem_fun(*this, &Editor::remove_route), tv));
	
	editor_mixer_button.set_sensitive(true);
	
}

void
Editor::handle_gui_changes (string what, void *src)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::handle_gui_changes), what, src));
	
	if (what == "track_height") {
		route_list_reordered ();
	}
}

void
Editor::remove_route (TimeAxisView *tv)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::remove_route), tv));
	
	TrackViewList::iterator i;
	CList_Helpers::RowList::iterator ri;

	if ((i = find (track_views.begin(), track_views.end(), tv)) != track_views.end()) {
		track_views.erase (i);
	}

	for (ri  = route_list.rows().begin(); ri != route_list.rows().end(); ++ri) {
		if (tv == ri->get_data()) {
			route_list.rows().erase (ri);
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
	CList_Helpers::RowList::iterator i;
	gint row;

	for (row = 0, i  = route_list.rows().begin(); i != route_list.rows().end(); ++i, ++row) {
		if (tv == i->get_data()) {
			route_list.cell (row, 0).set_text (tv->name());
			break;
		}
	}
}

void
Editor::route_list_selected (gint row, gint col, GdkEvent *ev)
{
	TimeAxisView *tv;
	if ((tv = (TimeAxisView *) route_list.get_row_data (row)) != 0) {
		tv->set_marked_for_display (true);
		route_list_reordered ();
	}
}

void
Editor::route_list_unselected (gint row, gint col, GdkEvent *ev)
{
	TimeAxisView *tv;
	AudioTimeAxisView *atv;

	if ((tv = (TimeAxisView *) route_list.get_row_data (row)) != 0) {

		tv->set_marked_for_display (false);

		if ((atv = dynamic_cast<AudioTimeAxisView*>(tv)) != 0) {
			if (current_mixer_strip && &(atv->route()) == &(current_mixer_strip->route())) {
				/* this will hide the mixer strip */
				set_selected_mixer_strip(*atv);
			}
		}

		route_list_reordered ();
	}
}

void
Editor::unselect_strip_in_display (TimeAxisView& tv)
{
	CList_Helpers::RowIterator i;

	if ((i = route_list.rows().find_data (&tv)) != route_list.rows().end()) {
		(*i).unselect ();
	}
}

void
Editor::select_strip_in_display (TimeAxisView& tv)
{
	CList_Helpers::RowIterator i;

	if ((i = route_list.rows().find_data (&tv)) != route_list.rows().end()) {
		(*i).select ();
	}
}

void
Editor::queue_route_list_reordered (gint arg1, gint arg2)

{
	/* the problem here is that we are called *before* the
	   list has been reordered. so just queue up
	   the actual re-drawer to happen once the re-ordering
	   is complete.
	*/

	Main::idle.connect (mem_fun(*this, &Editor::route_list_reordered));
}

void
Editor::redisplay_route_list ()
{
	route_list_reordered ();
}

gint
Editor::route_list_reordered ()
{
	CList_Helpers::RowList::iterator i;
	gdouble y;
	int n;

	for (n = 0, y = 0, i  = route_list.rows().begin(); i != route_list.rows().end(); ++i) {

		TimeAxisView *tv = (TimeAxisView *) (*i)->get_data ();
		
		AudioTimeAxisView* at;
		
		if (!ignore_route_list_reorder) {
			
				/* this reorder is caused by user action, so reassign sort order keys
				   to tracks.
				*/
			
			if ((at = dynamic_cast<AudioTimeAxisView*> (tv)) != 0) {
				at->route().set_order_key (N_("editor"), n);
			}
		}
		
		if (tv->marked_for_display()) {
			y += tv->show_at (y, n, &edit_controls_vbox);
			y += track_spacing;
		} else {
			tv->hide ();
		}
		
		n++;
	}

	edit_controls_scroller.queue_resize ();
	reset_scrolling_region ();

	//gnome_canvas_item_raise_to_top (time_line_group);
	gnome_canvas_item_raise_to_top (cursor_group);
	
	return FALSE;
}

void
Editor::hide_all_tracks (bool with_select)
{
	Gtk::CList_Helpers::RowList::iterator i;
	Gtk::CList_Helpers::RowList& rowlist = route_list.rows();
	
	route_list.freeze ();

	for (i = rowlist.begin(); i != rowlist.end(); ++i) {
		TimeAxisView *tv = (TimeAxisView *) i->get_data ();

		if (with_select) {
			i->unselect ();
		} else {
			tv->set_marked_for_display (false);
			tv->hide();
		}
	}

	route_list.thaw ();

	reset_scrolling_region ();
}

void
Editor::route_list_column_click (gint col)
{
	if (route_list_menu == 0) {
		build_route_list_menu ();
	}

	route_list_menu->popup (0, 0);
}

void
Editor::build_route_list_menu ()
{
	using namespace Gtk::Menu_Helpers;

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
	CList_Helpers::RowList::iterator i;

	for (i = route_list.rows().begin(); i != route_list.rows().end(); ++i) {
		i->select ();
	}
}

void
Editor::select_all_audiotracks () 
{
	Gtk::CList_Helpers::RowList::iterator i;
	Gtk::CList_Helpers::RowList& rowlist = route_list.rows();
	
	route_list.freeze ();
	
	for (i = rowlist.begin(); i != rowlist.end(); ++i) {
		TimeAxisView *tv = (TimeAxisView *) i->get_data ();
		AudioTimeAxisView* atv;

		if ((atv = dynamic_cast<AudioTimeAxisView*>(tv)) != 0) {
			if (atv->is_audio_track()) {
				i->select ();
			}
		}
	}
		
	route_list.thaw ();

}

void 
Editor::unselect_all_audiotracks () 
{
	Gtk::CList_Helpers::RowList::iterator i;
	Gtk::CList_Helpers::RowList& rowlist = route_list.rows();
	
	route_list.freeze ();

	for (i = rowlist.begin(); i != rowlist.end(); ++i) {
		TimeAxisView *tv = (TimeAxisView *) i->get_data ();
		AudioTimeAxisView* atv;

		if ((atv = dynamic_cast<AudioTimeAxisView*>(tv)) != 0) {
			if (atv->is_audio_track()) {
				i->unselect ();
			}
		}
	}
	
	route_list.thaw ();

}

void
Editor::select_all_audiobus () 
{
	Gtk::CList_Helpers::RowList::iterator i;
	Gtk::CList_Helpers::RowList& rowlist = route_list.rows();
	
	route_list.freeze ();

	for (i = rowlist.begin(); i != rowlist.end(); ++i) {
		TimeAxisView *tv = (TimeAxisView *) i->get_data ();
		AudioTimeAxisView* atv;

		if ((atv = dynamic_cast<AudioTimeAxisView*>(tv)) != 0) {
			if (!atv->is_audio_track()) {
				i->select ();
			}
		}
	}

	route_list.thaw ();

}

void
Editor::unselect_all_audiobus () 
{
	Gtk::CList_Helpers::RowList::iterator i;
	Gtk::CList_Helpers::RowList& rowlist = route_list.rows();
	
	route_list.freeze ();

	for (i = rowlist.begin(); i != rowlist.end(); ++i) {
		TimeAxisView *tv = (TimeAxisView *) i->get_data ();
		AudioTimeAxisView* atv;

		if ((atv = dynamic_cast<AudioTimeAxisView*>(tv)) != 0) {
			if (!atv->is_audio_track()) {
				i->unselect ();
			}
		}
	}

	route_list.thaw ();

}

gint
route_list_compare_func (GtkCList* clist, gconstpointer a, gconstpointer b)
{
	TimeAxisView *tv1;
	TimeAxisView *tv2;
	AudioTimeAxisView *atv1;
	AudioTimeAxisView *atv2;
	Route* ra;
	Route* rb;

	GtkCListRow *row1 = (GtkCListRow *) a;
	GtkCListRow *row2 = (GtkCListRow *) b;

	tv1 = static_cast<TimeAxisView*> (row1->data);
	tv2 = static_cast<TimeAxisView*> (row2->data);

	if ((atv1 = dynamic_cast<AudioTimeAxisView*>(tv1)) == 0 ||
	    (atv2 = dynamic_cast<AudioTimeAxisView*>(tv2)) == 0) {
		return FALSE;
	}

	ra = &atv1->route();
	rb = &atv2->route();

	/* use of ">" forces the correct sort order */

	return ra->order_key ("editor") > rb->order_key ("editor");
}

