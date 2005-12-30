/*
    Copyright (C) 2000-2005 Paul Davis 

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
#include "actions.h"

#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace Gtk;
using namespace Glib;
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
	string str;
	TreeModel::Row row;

	if (!show_automatic_regions_in_region_list && region->automatic()) {
		return;
	}

	if (region->hidden()) {

		TreeModel::iterator iter = region_list_model->get_iter (_("/Hidden"));
		TreeModel::Row parent;
		TreeModel::Row child;

		if (iter == region_list_model->children().end()) {
			
			parent = *(region_list_model->append());
			
			parent[region_list_columns.name] = _("Hidden");
			parent[region_list_columns.region] = 0;
		} else {
			parent = *iter;
		}

		row = *(region_list_model->append (parent.children()));

	} else if (region->whole_file()) {

		TreeModel::Row row = *(region_list_model->append());

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

		row[region_list_columns.name] = str;
		row[region_list_columns.region] = region;

		return;
		
	} else {

		/* find parent node, add as new child */
		
		TreeModel::iterator i;
		TreeModel::Children rows = region_list_model->children();
		bool found_parent = false;

		for (i = rows.begin(); i != rows.end(); ++i) {

			Region* rr = (*i)[region_list_columns.region];
			AudioRegion* r = dynamic_cast<AudioRegion*>(rr);

			if (r && r->whole_file()) {
				
				if (region->source_equivalent (*r)) {
					row = *(region_list_model->append ((*i).children()));
					found_parent = true;
					break;
				}
			}
		}

		if (!found_parent) {
			row = *(region_list_model->append());
		}

		
	}
	
	row[region_list_columns.region] = region;
	
	if (region->n_channels() > 1) {
		row[region_list_columns.name] = string_compose("%1  [%2]", region->name(), region->n_channels());
	} else {
		row[region_list_columns.name] = region->name();
	}
}

void
Editor::region_list_selection_changed() 
{
	bool sensitive;

	if (region_list_display.get_selection()->count_selected_rows() > 0) {
		sensitive = true;
	} else {
		sensitive = false;
	}
	
	for (vector<Glib::RefPtr<Gtk::Action> >::iterator i = ActionManager::region_list_selection_sensitive_actions.begin(); i != ActionManager::region_list_selection_sensitive_actions.end(); ++i) {
		(*i)->set_sensitive (sensitive);
	}

	// GTK2FIX
	// set_selected_regionview_from_region_list (*region, false);

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

		region_list_display.set_model (Glib::RefPtr<Gtk::TreeStore>(0));
		region_list_model->clear ();

		/* now add everything we have, via a temporary list used to help with
		   sorting.
		*/
		
		tmp_audio_region_list.clear();
		session->foreach_audio_region (this, &Editor::insert_into_tmp_audio_regionlist);

		for (list<AudioRegion*>::iterator r = tmp_audio_region_list.begin(); r != tmp_audio_region_list.end(); ++r) {
			add_audio_region_to_region_display (*r);
		}
		
		region_list_display.set_model (region_list_model);
	}
}

void
Editor::region_list_clear ()
{
	region_list_model->clear();
}

void
Editor::build_region_list_menu ()
{
	region_list_menu = dynamic_cast<Menu*>(ActionManager::get_widget ("/RegionListMenu"));
					       
	/* now grab specific menu items that we need */

	toggle_full_region_list_action = ActionManager::get_action (X_("RegionList"), X_("rlShowAll"));
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
	if (toggle_full_region_list_item->get_active()) {
		region_list_display.expand_all ();
	} else {
		region_list_display.collapse_all ();
	}
}

bool
Editor::region_list_display_key_press (GdkEventKey* ev)
{
	return false;
}

bool
Editor::region_list_display_key_release (GdkEventKey* ev)
{
	switch (ev->keyval) {
	case GDK_Delete:
		remove_region_from_region_list ();
		return true;
		break;
	default:
		break;
	}

	return false;
}

bool
Editor::region_list_display_button_press (GdkEventButton *ev)
{
	Region* region;
	TreeIter iter;
	TreeModel::Path path;
	TreeViewColumn* column;
	int cellx;
	int celly;

	if (region_list_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		if ((iter = region_list_model->get_iter (path))) {
			region = (*iter)[region_list_columns.region];
		}
	}

	if (region == 0) {
		return false;
	}

	if (Keyboard::is_delete_event (ev)) {
		session->remove_region_from_region_list (*region);
		return true;
	}

	if (Keyboard::is_context_menu_event (ev)) {
		if (region_list_menu == 0) {
			build_region_list_menu ();
		}
		region_list_menu->popup (ev->button, ev->time);
		return true;
	}

	switch (ev->button) {
	case 1:
		/* audition on double click */
		if (ev->type == GDK_2BUTTON_PRESS) {
			consider_auditioning (*region);
			return true;
		}
		return false;
		break;

	case 2:
		if (!Keyboard::modifier_state_equals (ev->state, Keyboard::Control)) {
			consider_auditioning (*region);
		}
		return true;
		break;

	default:
		break; 
	}

	return false;
}	

bool
Editor::region_list_display_button_release (GdkEventButton *ev)
{
	TreeIter iter;
	TreeModel::Path path;
	TreeViewColumn* column;
	int cellx;
	int celly;
	Region* region;

	if (region_list_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		if ((iter = region_list_model->get_iter (path))) {
			region = (*iter)[region_list_columns.region];
		}
	}

	if (Keyboard::is_delete_event (ev)) {
		session->remove_region_from_region_list (*region);
		return true;
	}

	switch (ev->button) {
	case 1:
		return false;
		break;

	case 3:
		return false;
		break;

	default:
		break;
	}

	return false;
}

void
Editor::consider_auditioning (Region& region)
{
	AudioRegion* r = dynamic_cast<AudioRegion*> (&region);

	if (r == 0) {
		session->cancel_audition ();
		return;
	}

	if (session->is_auditioning()) {
		session->cancel_audition ();
		if (r == last_audition_region) {
			return;
		}
	}

	session->audition_region (*r);
	last_audition_region = r;
}

int
Editor::region_list_sorter (TreeModel::iterator a, TreeModel::iterator b)
{
	int cmp;

	Region* r1 = (*a)[region_list_columns.region];
	Region* r2 = (*b)[region_list_columns.region];

	AudioRegion* region1 = dynamic_cast<AudioRegion*> (r1);
	AudioRegion* region2 = dynamic_cast<AudioRegion*> (r2);

	if (region1 == 0 || region2 == 0) {
		Glib::ustring s1;
		Glib::ustring s2;
		switch (region_list_sort_type) {
		case ByName:
			s1 = (*a)[region_list_columns.name];
			s2 = (*b)[region_list_columns.name];
			return (s1.compare (s2));
		default:
			return 0;
		}
	}

	switch (region_list_sort_type) {
	case ByName:
		cmp = strcasecmp (region1->name().c_str(), region2->name().c_str());
		break;

	case ByLength:
		cmp = region1->length() - region2->length();
		break;
		
	case ByPosition:
		cmp = region1->position() - region2->position();
		break;
		
	case ByTimestamp:
		cmp = region1->source().timestamp() - region2->source().timestamp();
		break;
	
	case ByStartInFile:
		cmp = region1->start() - region2->start();
		break;
		
	case ByEndInFile:
		cmp = (region1->start() + region1->length()) - (region2->start() + region2->length());
		break;
		
	case BySourceFileName:
		cmp = strcasecmp (region1->source().name().c_str(), region2->source().name().c_str());
		break;

	case BySourceFileLength:
		cmp = region1->source().length() - region2->source().length();
		break;
		
	case BySourceFileCreationDate:
		cmp = region1->source().timestamp() - region2->source().timestamp();
		break;

	case BySourceFileFS:
		if (region1->source().name() == region2->source().name()) {
			cmp = strcasecmp (region1->name().c_str(),  region2->name().c_str());
		} else {
			cmp = strcasecmp (region1->source().name().c_str(),  region2->source().name().c_str());
		}
		break;
	}

	if (cmp < 0) {
		return -1;
	} else if (cmp > 0) {
		return 1;
	} else {
		return 0;
	}
}

void
Editor::reset_region_list_sort_type (RegionListSortType type)
{
	if (type != region_list_sort_type) {
		region_list_sort_type = type;

		switch (type) {
		case ByName:
			region_list_display.get_column (0)->set_title (_("Regions/name"));
			break;
			
		case ByLength:
			region_list_display.get_column (0)->set_title (_("Regions/length"));
			break;
			
		case ByPosition:
			region_list_display.get_column (0)->set_title (_("Regions/position"));
			break;
			
		case ByTimestamp:
			region_list_display.get_column (0)->set_title (_("Regions/creation"));
			break;
			
		case ByStartInFile:
			region_list_display.get_column (0)->set_title (_("Regions/start"));
			break;
			
		case ByEndInFile:
			region_list_display.get_column (0)->set_title (_("Regions/end"));
			break;
			
		case BySourceFileName:
			region_list_display.get_column (0)->set_title (_("Regions/file name"));
			break;
			
		case BySourceFileLength:
			region_list_display.get_column (0)->set_title (_("Regions/file size"));
			break;
			
		case BySourceFileCreationDate:
			region_list_display.get_column (0)->set_title (_("Regions/file date"));
			break;
			
		case BySourceFileFS:
			region_list_display.get_column (0)->set_title (_("Regions/file system"));
			break;
		}
			
		// region_list_sort_model->set_sort_func (0, mem_fun (*this, &Editor::region_list_sorter));
	}
}

void
Editor::reset_region_list_sort_direction (bool up)
{
	// GTK2FIX
	//region_list_display.set_sort_type (up ? GTK_SORT_ASCENDING : GTK_SORT_DESCENDING);
	/* reset to force resort */
	// region_list_sort_model->set_sort_func (0, mem_fun (*this, &Editor::region_list_sorter));
}

void
Editor::region_list_selection_mapover (slot<void,Region&> sl)
{
	Glib::RefPtr<TreeSelection> selection = region_list_display.get_selection();
	TreeView::Selection::ListHandle_Path rows = selection->get_selected_rows ();
	TreeView::Selection::ListHandle_Path::iterator i = rows.begin();

	if (selection->count_selected_rows() == 0 || session == 0) {
		return;
	}

	for (; i != rows.end(); ++i) {
		TreeIter iter;

		if ((iter = region_list_model->get_iter (*i))) {
			sl (*((*iter)[region_list_columns.region]));
		}
	}
}

void
Editor::hide_a_region (Region& r)
{
	r.set_hidden (true);
}

void
Editor::remove_a_region (Region& r)
{
	session->remove_region_from_region_list (r);
}

void
Editor::audition_region_from_region_list ()
{
	region_list_selection_mapover (mem_fun (*this, &Editor::consider_auditioning));
}

void
Editor::hide_region_from_region_list ()
{
	region_list_selection_mapover (mem_fun (*this, &Editor::hide_a_region));
}

void
Editor::remove_region_from_region_list ()
{
	region_list_selection_mapover (mem_fun (*this, &Editor::remove_a_region));
}

void  
Editor::region_list_display_drag_data_received (const RefPtr<Gdk::DragContext>& context,
						int x, int y, 
						const SelectionData& data,
						guint info, guint time)
{
	vector<string> paths;

	if (convert_drop_to_paths (paths, context, x, y, data, info, time) == 0) {
		do_embed_sndfiles (paths, false);
		context->drag_finish (true, false, time);
	}
}
