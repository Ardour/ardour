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

#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <string>

#include <pbd/basename.h>

#include <ardour/audioregion.h>
#include <ardour/session_region.h>

#include <gtkmm2ext/stop_signal.h>

#include "editor.h"
#include "editing.h"
#include "ardour_ui.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace Gtk;
using namespace Editing;

#define wave_cursor_width 43
#define wave_cursor_height 61
#define wave_cursor_x_hot 0
#define wave_cursor_y_hot 25
static const gchar wave_cursor_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xff, 0xff,
0x03,
   0x02, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00,
0x02,
   0x02, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00,
0x02,
   0x02, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00,
0x02,
   0x02, 0x04, 0x00, 0x00, 0x00, 0x02, 0x02, 0x04, 0x00, 0x04, 0x00,
0x02,
   0x02, 0x04, 0x00, 0x04, 0x00, 0x02, 0x02, 0x0c, 0x08, 0x0c, 0x00,
0x02,
   0x02, 0x1c, 0x08, 0x0c, 0x00, 0x02, 0x02, 0x1c, 0x08, 0x0c, 0x04,
0x02,
   0x02, 0x3c, 0x18, 0x0c, 0x04, 0x02, 0x02, 0x7c, 0x18, 0x1c, 0x0c,
0x02,
   0x82, 0xfc, 0x38, 0x1c, 0x0c, 0x02, 0xc2, 0xfc, 0x78, 0x3c, 0x1c,
0x02,
   0xe2, 0xfd, 0xf9, 0x7d, 0x1c, 0x02, 0xf2, 0xff, 0xfb, 0xff, 0x1c,
0x02,
   0xfa, 0xff, 0xfb, 0xff, 0x3f, 0x02, 0xfe, 0xff, 0xff, 0xff, 0xff,
0x03,
   0xfe, 0xff, 0xff, 0xff, 0xff, 0x03, 0xfa, 0xff, 0xff, 0xff, 0x3f,
0x02,
   0xf2, 0xff, 0xfb, 0xfd, 0x3c, 0x02, 0xe2, 0xfd, 0x7b, 0x7c, 0x1c,
0x02,
   0xc2, 0xfc, 0x39, 0x3c, 0x1c, 0x02, 0x82, 0xfc, 0x18, 0x1c, 0x1c,
0x02,
   0x02, 0xfc, 0x18, 0x1c, 0x0c, 0x02, 0x02, 0x7c, 0x18, 0x0c, 0x0c,
0x02,
   0x02, 0x3c, 0x08, 0x0c, 0x04, 0x02, 0x02, 0x1c, 0x08, 0x0c, 0x04,
0x02,
   0x02, 0x1c, 0x08, 0x0c, 0x00, 0x02, 0x02, 0x0c, 0x00, 0x04, 0x00,
0x02,
   0x02, 0x04, 0x00, 0x04, 0x00, 0x02, 0x02, 0x04, 0x00, 0x00, 0x00,
0x02,
   0x02, 0x04, 0x00, 0x00, 0x00, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00,
0x02,
   0x02, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00,
0x02,
   0x02, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00,
0x02,
   0x02, 0x00, 0x00, 0x00, 0x00, 0x02, 0xfe, 0xff, 0xff, 0xff, 0xff,
0x03,
   0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

#define wave_cursor_mask_width 43
#define wave_cursor_mask_height 61
#define wave_cursor_mask_x_hot 0
#define wave_cursor_mask_y_hot 25
static const gchar wave_cursor_mask_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x04, 0x00,
0x00,
   0x00, 0x04, 0x00, 0x04, 0x00, 0x00, 0x00, 0x0c, 0x08, 0x0c, 0x00,
0x00,
   0x00, 0x1c, 0x08, 0x0c, 0x00, 0x00, 0x00, 0x1c, 0x08, 0x0c, 0x04,
0x00,
   0x00, 0x3c, 0x18, 0x0c, 0x04, 0x00, 0x00, 0x7c, 0x18, 0x1c, 0x0c,
0x00,
   0x80, 0xfc, 0x38, 0x1c, 0x0c, 0x00, 0xc0, 0xfc, 0x78, 0x3c, 0x1c,
0x00,
   0xe0, 0xfd, 0xf9, 0x7d, 0x1c, 0x00, 0xf0, 0xff, 0xfb, 0xff, 0x1c,
0x00,
   0xf8, 0xff, 0xfb, 0xff, 0x3f, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
0x07,
   0xff, 0xff, 0xff, 0xff, 0xff, 0x07, 0xf8, 0xff, 0xff, 0xff, 0x3f,
0x00,
   0xf0, 0xff, 0xfb, 0xfd, 0x3c, 0x00, 0xe0, 0xfd, 0x7b, 0x7c, 0x1c,
0x00,
   0xc0, 0xfc, 0x39, 0x3c, 0x1c, 0x00, 0x80, 0xfc, 0x18, 0x1c, 0x1c,
0x00,
   0x00, 0xfc, 0x18, 0x1c, 0x0c, 0x00, 0x00, 0x7c, 0x18, 0x0c, 0x0c,
0x00,
   0x00, 0x3c, 0x08, 0x0c, 0x04, 0x00, 0x00, 0x1c, 0x08, 0x0c, 0x04,
0x00,
   0x00, 0x1c, 0x08, 0x0c, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x04, 0x00,
0x00,
   0x00, 0x04, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
0x00,
   0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

GdkCursor *wave_cursor = 0;

void
Editor::handle_audio_region_removed (AudioRegion* ignored)
{
	redisplay_regions ();
}

void
Editor::handle_new_audio_region (AudioRegion *region)
{
	/* don't copy region - the one we are being notified
	   about belongs to the session, and so it will
	   never be edited.
	*/
	add_audio_region_to_region_display (region);
}

void
Editor::region_hidden (Region* r)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::region_hidden), r));	

	redisplay_regions ();
}

void
Editor::add_audio_region_to_region_display (AudioRegion *region)
{
	using namespace Gtk::CTree_Helpers;

	vector<const char*> item;
	RowList::iterator i;
	RowList::iterator tmpi;
	string str;

	if (!show_automatic_regions_in_region_list && region->automatic()) {
		return;
	}

	if (region->hidden()) {

		if (region_list_hidden_node == region_list_display.rows().end()) {
			item.clear ();
			item.push_back (_("hidden"));
			region_list_hidden_node = region_list_display.rows().insert (region_list_display.rows().end(),
									 Element (item));
			(*region_list_hidden_node).set_data (0);
			(*region_list_hidden_node).set_leaf (false);
		}

		item.clear ();
		if (region->n_channels() > 1) {
			str = string_compose("%1  [%2]", region->name(), region->n_channels());
			item.push_back (str.c_str());
		} else {
			item.push_back (region->name().c_str());
		}

		tmpi = region_list_hidden_node->subtree().insert (region_list_hidden_node->subtree().end(), 
								  Element (item));
		(*tmpi).set_data (region);
		return;

	} else if (region->whole_file()) {

		item.clear ();

		if (region->source().name()[0] == '/') { // external file

			if (region->whole_file()) {
				str = ".../";
				str += PBD::basename_nosuffix (region->source().name());
			} else {
				str = region->name();
			}

		} else {

			str = region->name();

		}

		item.push_back (str.c_str());

		tmpi = region_list_display.rows().insert (region_list_display.rows().end(), 
							  Element (item));

		(*tmpi).set_data (region);
		(*tmpi).set_leaf (false);

		return;
		
	} else {

		/* find parent node, add as new child */
		
		for (i = region_list_display.rows().begin(); i != region_list_display.rows().end(); ++i) {

			AudioRegion* r = static_cast<AudioRegion*> ((*i).get_data());

			if (r && r->whole_file()) {
				
				if (region->source_equivalent (*r)) {

					item.clear ();
					
					if (region->n_channels() > 1) {
						str = string_compose("%1  [%2]", region->name(), region->n_channels());
						item.push_back (str.c_str());
					} else {
						item.push_back (region->name().c_str());
					}

					
					tmpi = i->subtree().insert (i->subtree().end(), Element (item));
					(*tmpi).set_data (region);
					
					return;
				}
			}
		}
	}
	
	item.clear ();
	
	if (region->n_channels() > 1) {
		str = string_compose("%1  [%2]", region->name(), region->n_channels());
		item.push_back (str.c_str());
	} else {
		item.push_back (region->name().c_str());
	}
	
	tmpi = region_list_display.rows().insert (region_list_display.rows().end(), Element (item));
	(*tmpi).set_data (region);
	(*tmpi).set_leaf (true);
}

void
Editor::insert_into_tmp_audio_regionlist(AudioRegion* region)
{
	/* keep all whole files at the beginning */
	
	if (region->whole_file()) {
		tmp_audio_region_list.push_front (region);
	} else {
		tmp_audio_region_list.push_back (region);
	}
}

void
Editor::redisplay_regions ()
{
	if (session) {
		region_list_display.freeze ();
		region_list_clear ();
		region_list_hidden_node = region_list_display.rows().end();

		/* now add everything we have, via a temporary list used to help with
		   sorting.
		*/
		
		tmp_audio_region_list.clear();
		session->foreach_audio_region (this, &Editor::insert_into_tmp_audio_regionlist);

		for (list<AudioRegion*>::iterator r = tmp_audio_region_list.begin(); r != tmp_audio_region_list.end(); ++r) {
			add_audio_region_to_region_display (*r);
		}
		
		region_list_display.sort ();
		region_list_display.thaw ();
	}
}

void
Editor::region_list_clear ()
{
	/* ---------------------------------------- */
	/* XXX MAKE ME A FUNCTION (no CTree::clear() in gtkmm 1.2) */

	gtk_ctree_remove_node (region_list_display.gobj(), NULL);

	/* ---------------------------------------- */
}

void
Editor::region_list_column_click (gint col)
{
	bool sensitive;

	if (region_list_menu == 0) {
		build_region_list_menu ();
	}

	if (region_list_display.selection().size() != 0) {
		sensitive = true;
	} else {
		sensitive = false;
	}

	for (vector<MenuItem*>::iterator i = rl_context_menu_region_items.begin(); i != rl_context_menu_region_items.end(); ++i) {
		(*i)->set_sensitive (sensitive);
	}

	region_list_menu->popup (0, 0);
}

void
Editor::build_region_list_menu ()
{
	using namespace Gtk::Menu_Helpers;

	region_list_menu = new Menu;
	
	MenuList& items = region_list_menu->items();
	region_list_menu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Audition"), mem_fun(*this, &Editor::audition_region_from_region_list)));
	rl_context_menu_region_items.push_back (items.back());
	items.push_back (MenuElem (_("Hide"), mem_fun(*this, &Editor::hide_region_from_region_list)));
	rl_context_menu_region_items.push_back (items.back());
	items.push_back (MenuElem (_("Remove"), mem_fun(*this, &Editor::remove_region_from_region_list)));
	rl_context_menu_region_items.push_back (items.back());


	items.push_back (SeparatorElem());
	

	// items.push_back (MenuElem (_("Find")));
	items.push_back (CheckMenuElem (_("Show all"), mem_fun(*this, &Editor::toggle_full_region_list)));
	toggle_full_region_list_item = static_cast<CheckMenuItem*> (items.back());
	
	Gtk::Menu *sort_menu = manage (new Menu);
	MenuList& sort_items = sort_menu->items();
	sort_menu->set_name ("ArdourContextMenu");
	RadioMenuItem::Group sort_order_group;
	RadioMenuItem::Group sort_type_group;

	sort_items.push_back (RadioMenuElem (sort_order_group, _("Ascending"),
					     bind (mem_fun(*this, &Editor::reset_region_list_sort_direction), true)));
	sort_items.push_back (RadioMenuElem (sort_order_group, _("Descending"), 
					     bind (mem_fun(*this, &Editor::reset_region_list_sort_direction), false)));
	sort_items.push_back (SeparatorElem());

	sort_items.push_back (RadioMenuElem (sort_type_group, _("By Region Name"),
					     bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByName)));
	sort_items.push_back (RadioMenuElem (sort_type_group, _("By Region Length"),
					     bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByLength)));
	sort_items.push_back (RadioMenuElem (sort_type_group, _("By Region Position"),
					     bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByPosition)));
	sort_items.push_back (RadioMenuElem (sort_type_group, _("By Region Timestamp"),
					     bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByTimestamp)));
	sort_items.push_back (RadioMenuElem (sort_type_group, _("By Region Start in File"),
					     bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByStartInFile)));
	sort_items.push_back (RadioMenuElem (sort_type_group, _("By Region End in File"),
					     bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByEndInFile)));
	sort_items.push_back (RadioMenuElem (sort_type_group, _("By Source File Name"),
					     bind (mem_fun(*this, &Editor::reset_region_list_sort_type), BySourceFileName)));
	sort_items.push_back (RadioMenuElem (sort_type_group, _("By Source File Length"),
					     bind (mem_fun(*this, &Editor::reset_region_list_sort_type), BySourceFileLength)));
	sort_items.push_back (RadioMenuElem (sort_type_group, _("By Source File Creation Date"),
					     bind (mem_fun(*this, &Editor::reset_region_list_sort_type), BySourceFileCreationDate)));
	sort_items.push_back (RadioMenuElem (sort_type_group, _("By Source Filesystem"),
					     bind (mem_fun(*this, &Editor::reset_region_list_sort_type), BySourceFileFS)));
	
	items.push_back (MenuElem (_("Sorting"), *sort_menu));
	items.push_back (SeparatorElem());

//	items.push_back (CheckMenuElem (_("Display Automatic Regions"), mem_fun(*this, &Editor::toggle_show_auto_regions)));
//	toggle_auto_regions_item = static_cast<CheckMenuItem*> (items.back());
//	toggle_auto_regions_item->set_active (show_automatic_regions_in_region_list);
//	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Import audio (copy)"), bind (mem_fun(*this, &Editor::import_audio), false)));
	import_audio_item = items.back();
	if (!session) {
		import_audio_item->set_sensitive (false);
	}
	items.push_back (MenuElem (_("Embed audio (link)"), mem_fun(*this, &Editor::embed_audio)));
	embed_audio_item = items.back();
	if (!session) {
		embed_audio_item->set_sensitive (false);
	}
}

void
Editor::toggle_show_auto_regions ()
{
	//show_automatic_regions_in_region_list = toggle_auto_regions_item->get_active();
	show_automatic_regions_in_region_list = true;
	redisplay_regions ();
}

void
Editor::toggle_full_region_list ()
{
	region_list_display.freeze ();
	if (toggle_full_region_list_item->get_active()) {
		for (CTree_Helpers::RowIterator r = region_list_display.rows().begin(); r != region_list_display.rows().end(); ++r) {
			r->expand_recursive ();
		}
	} else {
		for (CTree_Helpers::RowIterator r = region_list_display.rows().begin(); r != region_list_display.rows().end(); ++r) {
			r->collapse ();
		}
	}
	region_list_display.thaw ();
}

gint
Editor::region_list_display_key_press (GdkEventKey* ev)
{
	return FALSE;
}

gint
Editor::region_list_display_key_release (GdkEventKey* ev)
{
	switch (ev->keyval) {
	case GDK_Delete:
		remove_selected_regions_from_region_list ();
		return TRUE;
		break;
	default:
		break;
	}

	return FALSE;

}

gint
Editor::region_list_display_button_press (GdkEventButton *ev)
{
	int row, col;
	AudioRegion *region;

	if (Keyboard::is_delete_event (ev)) {
		if (region_list_display.get_selection_info ((int)ev->x, (int)ev->y, &row, &col) != 0) {
			if ((region = (AudioRegion *) region_list_display.row(row).get_data()) != 0) {
				delete region;
			}
		}
		return TRUE;
	}

	if (Keyboard::is_context_menu_event (ev)) {
		region_list_column_click (-1);
		return TRUE;
	}

	switch (ev->button) {
	case 1:
		if (region_list_display.get_selection_info ((int)ev->x, (int)ev->y, &row, &col) != 0) {
			if ((region = (AudioRegion *) region_list_display.row(row).get_data()) != 0) {

				if (wave_cursor == 0) {
					GdkPixmap *source, *mask;
					GdkColor fg = { 0, 65535, 0, 0 }; /* Red. */
					GdkColor bg = { 0, 0, 0, 65535 }; /* Blue. */
					
					source = gdk_bitmap_create_from_data (NULL, wave_cursor_bits,
									      wave_cursor_width, wave_cursor_height);
					mask = gdk_bitmap_create_from_data (NULL, wave_cursor_mask_bits,
									    wave_cursor_mask_width, wave_cursor_mask_height);

					wave_cursor = gdk_cursor_new_from_pixmap (source, 
										  mask,
										  &fg, 
										  &bg,
										  wave_cursor_x_hot,
										  wave_cursor_y_hot);
					gdk_pixmap_unref (source);
					gdk_pixmap_unref (mask);
				}
				region_list_display_drag_region = region;
				need_wave_cursor = 1;

				/* audition on double click */
				if (ev->type == GDK_2BUTTON_PRESS) {
					consider_auditioning (region);
				}

				return TRUE;
			}
			
		}
		break;

	case 2:
		if (!Keyboard::modifier_state_equals (ev->state, Keyboard::Control)) {
			if (region_list_display.get_selection_info ((int)ev->x, (int)ev->y, &row, &col) != 0) {
				if ((region = (AudioRegion *) region_list_display.get_row_data (row)) != 0) {
					if (consider_auditioning (region)) {
						region_list_display.row(row).select();
					}
					else {
						region_list_display.row(row).unselect();
					}
					return TRUE;
				}
			}
		} 

		/* to prevent regular selection -- i dont think this is needed JLC */
		return stop_signal (region_list_display, "button_press_event");
		break;

	case 3:
		break;
	default:
		break; 
	}

	return FALSE;
}

gint
Editor::region_list_display_button_release (GdkEventButton *ev)
{
	int row, col;

	if (region_list_display.get_selection_info ((int)ev->x, (int)ev->y, &row, &col) != 0) {
		region_list_button_region = (AudioRegion *) region_list_display.get_row_data (row);
	} else {
		region_list_button_region = 0;
	}

	if (Keyboard::is_delete_event (ev)) {
		remove_region_from_region_list ();
		return TRUE;
	}

	switch (ev->button) {
	case 1:
		if (region_list_display_drag_region) {
			insert_region_list_drag (*region_list_display_drag_region);
		}

		track_canvas_scroller.get_window().set_cursor (current_canvas_cursor);
		region_list_display.get_window().set_cursor (0);

		region_list_display_drag_region = 0;
		need_wave_cursor = 0;

		return TRUE;
		break;

	case 3:
		if (!Keyboard::modifier_state_equals (ev->state, Keyboard::Control)) {

			if (region_list_menu == 0) {
				build_region_list_menu ();
			}
			
			bool sensitive;
			
			if (region_list_display.selection().size() != 0) {
				sensitive = true;
			} else {
				sensitive = false;
			}

			for (vector<MenuItem*>::iterator i = rl_context_menu_region_items.begin(); i != rl_context_menu_region_items.end(); ++i) {
				(*i)->set_sensitive (sensitive);
			}

			region_list_menu->popup (0, 0);
		}

		return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

gint
Editor::region_list_display_motion (GdkEventMotion *ev)
{
	if (need_wave_cursor == 1) {
		track_canvas_scroller.get_window().set_cursor (wave_cursor);
		region_list_display.get_window().set_cursor (wave_cursor);
		gdk_flush ();
		need_wave_cursor = 2;
	}
	return FALSE;
}

void
Editor::region_list_display_selected (gint row, gint col, GdkEvent *ev)
{
	AudioRegion* region = static_cast<AudioRegion *>(region_list_display.get_row_data (row));

	if (session == 0 || region == 0) {
		return;
	}

	set_selected_regionview_from_region_list (*region, false);
}

void
Editor::region_list_display_unselected (gint row, gint col, GdkEvent *ev)
{
}

bool
Editor::consider_auditioning (AudioRegion *r)
{
	if (r == 0) {
		session->cancel_audition ();
		return false;
	}

	if (session->is_auditioning()) {
		session->cancel_audition ();
		if (r == last_audition_region) {
			return false;
		}
	}

	session->audition_region (*r);
	last_audition_region = r;

	return true;
}

gint
Editor::region_list_display_enter_notify (GdkEventCrossing *ev)
{
	ARDOUR_UI::instance()->allow_focus (true);
	region_list_display.grab_focus ();
	return FALSE;
}

gint
Editor::region_list_display_leave_notify (GdkEventCrossing *ev)
{
	ARDOUR_UI::instance()->allow_focus (false);
	return FALSE;
}

gint
Editor::_region_list_sorter (GtkCList* clist, gconstpointer a, gconstpointer b)
{
	Editor* editor = static_cast<Editor*> (gtk_object_get_data (GTK_OBJECT(clist), "editor"));
	return editor->region_list_sorter (a, b);
}

gint
Editor::region_list_sorter (gconstpointer a, gconstpointer b)
{
	GtkCListRow* row1 = (GtkCListRow *) a;
	GtkCListRow* row2 = (GtkCListRow *) b;

	AudioRegion* region1 = static_cast<AudioRegion*> (row1->data);
	AudioRegion* region2 = static_cast<AudioRegion*> (row2->data);

	if (region1 == 0 || region2 == 0) {
		switch (region_list_sort_type) {
		case ByName:
			return true; /* XXX compare text in rows */
		default:
			return true;
		}
	}

	switch (region_list_sort_type) {
	case ByName:
		return strcasecmp (region1->name().c_str(), region2->name().c_str());
		break;

	case ByLength:
		return region1->length() - region2->length();
		break;
		
	case ByPosition:
		return region1->position() - region2->position();
		break;
		
	case ByTimestamp:
		return region1->source().timestamp() - region2->source().timestamp();
		break;
	
	case ByStartInFile:
		return region1->start() - region2->start();
		break;
		
	case ByEndInFile:
		return (region1->start() + region1->length()) - (region2->start() + region2->length());
		break;
		
	case BySourceFileName:
		return strcasecmp (region1->source().name().c_str(), region2->source().name().c_str());
		break;

	case BySourceFileLength:
		return region1->source().length() - region2->source().length();
		break;
		
	case BySourceFileCreationDate:
		return region1->source().timestamp() - region2->source().timestamp();
		break;

	case BySourceFileFS:
		if (region1->source().name() == region2->source().name()) {
			return strcasecmp (region1->name().c_str(),  region2->name().c_str());
		} else {
			return strcasecmp (region1->source().name().c_str(),  region2->source().name().c_str());
		}
		break;
	}

	return FALSE;
}

void
Editor::reset_region_list_sort_type (RegionListSortType type)
{
	if (type != region_list_sort_type) {
		region_list_sort_type = type;

		switch (type) {
		case ByName:
			region_list_display.set_column_title(0, _("Regions/name"));
			break;
			
		case ByLength:
			region_list_display.set_column_title (0, _("Regions/length"));
			break;
			
		case ByPosition:
			region_list_display.set_column_title (0, _("Regions/position"));
			break;
			
		case ByTimestamp:
			region_list_display.set_column_title (0, _("Regions/creation"));
			break;
			
		case ByStartInFile:
			region_list_display.set_column_title (0, _("Regions/start"));
			break;
			
		case ByEndInFile:
			region_list_display.set_column_title (0, _("Regions/end"));
			break;
			
		case BySourceFileName:
			region_list_display.set_column_title (0, _("Regions/file name"));
			break;
			
		case BySourceFileLength:
			region_list_display.set_column_title (0, _("Regions/file size"));
			break;
			
		case BySourceFileCreationDate:
			region_list_display.set_column_title (0, _("Regions/file date"));
			break;
			
		case BySourceFileFS:
			region_list_display.set_column_title (0, _("Regions/file system"));
			break;
		}
			
		region_list_display.sort ();
	}
}

void
Editor::reset_region_list_sort_direction (bool up)
{
	region_list_display.set_sort_type (up ? GTK_SORT_ASCENDING : GTK_SORT_DESCENDING);
	region_list_display.sort ();
}

void
Editor::audition_region_from_region_list ()
{
	if (region_list_button_region) {
		consider_auditioning (dynamic_cast<AudioRegion*> (region_list_button_region));
	}
}

void
Editor::hide_region_from_region_list ()
{
	if (session == 0 || region_list_button_region == 0) {
		return;
	}

	region_list_button_region->set_hidden (true);
}

void
Editor::remove_region_from_region_list ()
{
	if (session == 0 || region_list_button_region == 0) {
		return;
	}

	session->remove_region_from_region_list (*region_list_button_region);
}

void
Editor::remove_selected_regions_from_region_list ()
{
	using namespace Gtk::CTree_Helpers;
	SelectionList& selected = region_list_display.selection();

	/* called from idle context to avoid snafus with the list
	   state.
	*/
	
	if (selected.empty() || session == 0) {
		return;
	}
	
	vector<Region*> to_be_deleted;

	for (SelectionList::iterator i = selected.begin(); i != selected.end(); ++i) {
		to_be_deleted.push_back (static_cast<Region*> ((*i).get_data()));
	}

	for (vector<Region*>::iterator i = to_be_deleted.begin(); i != to_be_deleted.end(); ++i) {
		session->remove_region_from_region_list (**i);
	}

	return;
}

void  
Editor::region_list_display_drag_data_received  (GdkDragContext     *context,
						 gint                x,
						 gint                y,
						 GtkSelectionData   *data,
						 guint               info,
						 guint               time)
{
	vector<string> paths;

	if (convert_drop_to_paths (paths, context, x, y, data, info, time) == 0) {
		do_embed_sndfiles (paths, false);
	}

	gtk_drag_finish (context, TRUE, FALSE, time);
}
