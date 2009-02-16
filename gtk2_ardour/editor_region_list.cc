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

*/

#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <string>
#include <sstream>

#include <pbd/basename.h>

#include <ardour/audioregion.h>
#include <ardour/audiofilesource.h>
#include <ardour/silentfilesource.h>
#include <ardour/session_region.h>
#include <ardour/profile.h>

#include <gtkmm2ext/stop_signal.h>

#include "editor.h"
#include "editing.h"
#include "keyboard.h"
#include "ardour_ui.h"
#include "gui_thread.h"
#include "actions.h"
#include "region_view.h"
#include "utils.h"


#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Editing;

void
Editor::handle_region_removed (boost::weak_ptr<Region> wregion)
{
	ENSURE_GUI_THREAD (mem_fun (*this, &Editor::redisplay_regions));
	redisplay_regions ();
}

void
Editor::handle_new_regions (vector<boost::weak_ptr<Region> >& v)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &Editor::handle_new_regions), v));
	add_regions_to_region_display (v);
}

void
Editor::region_hidden (boost::shared_ptr<Region> r)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::region_hidden), r));	
	redisplay_regions ();
}

void
Editor::add_regions_to_region_display (vector<boost::weak_ptr<Region> >& regions)
{
	for (vector<boost::weak_ptr<Region> >::iterator x = regions.begin(); x != regions.end(); ++x) {
		boost::shared_ptr<Region> region ((*x).lock());
		if (region) {
			add_region_to_region_display (region);
		}
	}
}

void
Editor::add_region_to_region_display (boost::shared_ptr<Region> region)
{
	if (!region || !session) {
		return;
	}
	
	string str;
	TreeModel::Row row;
	Gdk::Color c;
	bool missing_source = boost::dynamic_pointer_cast<SilentFileSource>(region->source());

	if (!show_automatic_regions_in_region_list && region->automatic()) {
		return;
	}

	if (region->hidden()) {
		TreeModel::iterator iter = region_list_model->get_iter ("0");
		TreeModel::Row parent;
		TreeModel::Row child;

		if (!iter) {
			parent = *(region_list_model->append());
			parent[region_list_columns.name] = _("Hidden");
			boost::shared_ptr<Region> proxy = parent[region_list_columns.region];
			proxy.reset ();
		} else {
			if ((*iter)[region_list_columns.name] != _("Hidden")) {
				parent = *(region_list_model->insert(iter));
				parent[region_list_columns.name] = _("Hidden");
				boost::shared_ptr<Region> proxy = parent[region_list_columns.region];
				proxy.reset ();
			} else {
				parent = *iter;
			}
		}

		row = *(region_list_model->append (parent.children()));

	} else if (region->whole_file()) {

		TreeModel::iterator i;
		TreeModel::Children rows = region_list_model->children();

		for (i = rows.begin(); i != rows.end(); ++i) {
			boost::shared_ptr<Region> rr = (*i)[region_list_columns.region];
			
			if (rr && region->region_list_equivalent (rr)) {
				return;
			}
		}

		row = *(region_list_model->append());
		
		if (missing_source) {
			c.set_rgb(65535,0,0);     // FIXME: error color from style
		
		} else if (region->automatic()){
			c.set_rgb(0,65535,0);     // FIXME: error color from style
		
		} else {
			set_color(c, rgba_from_style ("RegionListWholeFile", 0xff, 0, 0, 0, "fg", Gtk::STATE_NORMAL, false ));
		
		}
		
		row[region_list_columns.color_] = c;

		if (region->source()->name()[0] == '/') { // external file
			
			if (region->whole_file()) {
				
				boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(region->source());
				str = ".../";

				if (afs) {
					str = region_name_from_path (afs->path(), region->n_channels() > 1);
				} else {
					str += region->source()->name();
				}

			} else {
				str = region->name();
			}

		} else {
			str = region->name();
		}

		if (region->n_channels() > 1) {
			std::stringstream foo;
			foo << region->n_channels ();
			str += " [";
			str += foo.str();
			str += ']';
		}

		row[region_list_columns.name] = str;
		row[region_list_columns.region] = region;
		
		if (missing_source) {
			row[region_list_columns.path] = _("(MISSING) ") + region->source()->name();
		
		} else {
			row[region_list_columns.path] = region->source()->name();
		
		} 
		
		if (region->automatic()) {
			return;
		}
			
	} else {

		/* find parent node, add as new child */
		
		TreeModel::iterator i;
		TreeModel::Children rows = region_list_model->children();
		bool found_parent = false;

		for (i = rows.begin(); i != rows.end(); ++i) {
			boost::shared_ptr<Region> rr = (*i)[region_list_columns.region];
			boost::shared_ptr<AudioRegion> r = boost::dynamic_pointer_cast<AudioRegion>(rr);
			
			if (r && r->whole_file()) {
				
				if (region->source_equivalent (r)) {
					row = *(region_list_model->append ((*i).children()));
					found_parent = true;
					break;
				}
			}
			
			TreeModel::iterator ii;
			TreeModel::Children subrows = (*i).children();

			for (ii = subrows.begin(); ii != subrows.end(); ++ii) {
				boost::shared_ptr<Region> rrr = (*ii)[region_list_columns.region];

				if (region->region_list_equivalent (rrr)) {
					return;
				
				}
			}
		}
		
		if (!found_parent) {
			row = *(region_list_model->append());
		}	
	}
	
	row[region_list_columns.region] = region;
	
	populate_row(region, row);
}


void
Editor::region_list_region_changed (Change what_changed, boost::weak_ptr<Region> region)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &Editor::region_list_region_changed), what_changed, region));
	
	boost::shared_ptr<Region> r = region.lock ();
	
	if (!r) {
		return;
	}
	
	if (what_changed & ARDOUR::NameChanged) {
		/* find the region in our model and change its name */
		TreeModel::Children rows = region_list_model->children ();
		TreeModel::iterator i = rows.begin ();
		while (i != rows.end ()) {
			TreeModel::Children children = (*i)->children ();
			TreeModel::iterator j = children.begin ();
			while (j != children.end()) {
				boost::shared_ptr<Region> c = (*j)[region_list_columns.region];
				if (c == r) {
					break;
				}
				++j;
			}

			if (j != children.end()) {
				(*j)[region_list_columns.name] = r->name ();
				break;
			}
			
			++i;
		}

	}
}

void
Editor::region_list_selection_changed() 
{
	if (region_list_display.get_selection()->count_selected_rows() > 0) {
		
		TreeIter iter;
		TreeView::Selection::ListHandle_Path rows = region_list_display.get_selection()->get_selected_rows ();
		
		deselect_all();
		
		for (TreeView::Selection::ListHandle_Path::iterator i = rows.begin(); i != rows.end(); ++i) {
			
			if (iter = region_list_model->get_iter (*i)) {									// they could have clicked on a row that is just a placeholder, like "Hidden"
				boost::shared_ptr<Region> region = (*iter)[region_list_columns.region];
				
				if (region) {
					
					if (region->automatic()) {
						region_list_display.get_selection()->unselect(*i);
						
					} else {
						region_list_change_connection.block(true);
						//editor_regions_selection_changed_connection.block(true);

						set_selected_regionview_from_region_list (region, Selection::Add);

						region_list_change_connection.block(false);
						//editor_regions_selection_changed_connection.block(false);
					}
				}
			}
		}
	} else {
		deselect_all();
	}
}

void
Editor::set_selected_in_region_list(RegionSelection& regions)
{
	for (RegionSelection::iterator iter = regions.begin(); iter != regions.end(); ++iter) {
	
		TreeModel::iterator i;
		TreeModel::Children rows = region_list_model->children();
		boost::shared_ptr<Region> r ((*iter)->region());
		
		for (i = rows.begin(); i != rows.end(); ++i) {
			
			boost::shared_ptr<Region> compared_region = (*i)[region_list_columns.region];

			if (r == compared_region) {
				region_list_display.get_selection()->select(*i);;
				break;
			}
			
			if (!(*i).children().empty()) {
				if (set_selected_in_region_list_subrow(r, (*i), 2)) {
					break;
				}
			}
		}
	}
}

bool
Editor::set_selected_in_region_list_subrow (boost::shared_ptr<Region> region, TreeModel::Row const &parent_row, int level)
{
	TreeModel::iterator i;
	TreeModel::Children subrows = (*parent_row).children();
	
	for (i = subrows.begin(); i != subrows.end(); ++i) {
		
		boost::shared_ptr<Region> compared_region = (*i)[region_list_columns.region];
		
		if (region == compared_region) {
			region_list_display.get_selection()->select(*i);;
			return true;
		}
		
		if (!(*i).children().empty()) {
			if (update_region_subrows(region, (*i), level + 1)) {
				return true;
			}
		}
	}
	return false;
}

void
Editor::insert_into_tmp_regionlist(boost::shared_ptr<Region> region)
{
	/* keep all whole files at the beginning */
	
	if (region->whole_file()) {
		tmp_region_list.push_front (region);
	} else {
		tmp_region_list.push_back (region);
	}
}

void
Editor::redisplay_regions ()
{	
	if (no_region_list_redisplay || !session) {
		return;
	}
		
	bool tree_expanded = false;
	
	if (toggle_full_region_list_action && toggle_full_region_list_action->get_active()) {   //If the list was expanded prior to rebuilding, 
		tree_expanded = true;																//expand it again afterwards
	}
	
	region_list_display.set_model (Glib::RefPtr<Gtk::TreeStore>(0));
	region_list_model->clear ();

	/* now add everything we have, via a temporary list used to help with
		sorting.
	*/
	
	tmp_region_list.clear();
	session->foreach_region (this, &Editor::insert_into_tmp_regionlist);

	for (list<boost::shared_ptr<Region> >::iterator r = tmp_region_list.begin(); r != tmp_region_list.end(); ++r) {
		add_region_to_region_display (*r);
	}
	tmp_region_list.clear();
	
	region_list_display.set_model (region_list_model);
	
	if (tree_expanded) {
		region_list_display.expand_all();
	}
}

void
Editor::update_region_row (boost::shared_ptr<Region> region)
{	
	if (!region || !session) {
		return;
	}
	
	TreeModel::iterator i;
	TreeModel::Children rows = region_list_model->children();
	
	for (i = rows.begin(); i != rows.end(); ++i) {
		
//		cerr << "Level 1: Compare " << region->name() << " with parent " << (*i)[region_list_columns.name] << "\n";
		
		boost::shared_ptr<Region> compared_region = (*i)[region_list_columns.region];
		
		if (region == compared_region) {
//			cerr << "Matched\n";
			populate_row(region, (*i));
			return;
		}
		
		if (!(*i).children().empty()) {
			if (update_region_subrows(region, (*i), 2)) {
				return;
			}
		}
	}
//	cerr << "Returning - No match\n";
}

bool
Editor::update_region_subrows (boost::shared_ptr<Region> region, TreeModel::Row const &parent_row, int level)
{
	TreeModel::iterator i;
	TreeModel::Children subrows = (*parent_row).children();
	
	for (i = subrows.begin(); i != subrows.end(); ++i) {
		
//		cerr << "Level " << level << ": Compare " << region->name() << " with child " << (*i)[region_list_columns.name] << "\n";
		
		boost::shared_ptr<Region> compared_region = (*i)[region_list_columns.region];
		
		if (region == compared_region) {
			populate_row(region, (*i));
//			cerr << "Matched\n";
			return true;
		}
		
		if (!(*i).children().empty()) {
			if (update_region_subrows(region, (*i), level + 1)) {
				return true;
			}
		}
	}
	return false;
}

void
Editor::update_all_region_rows ()
{
	if (!session) {
		return;
	}

	TreeModel::iterator i;
	TreeModel::Children rows = region_list_model->children();
	
	for (i = rows.begin(); i != rows.end(); ++i) {
		
		boost::shared_ptr<Region> region = (*i)[region_list_columns.region];
	
		if (!region->automatic()) {
			cerr << "level 1 : Updating " << region->name() << "\n";
			populate_row(region, (*i));
		}
		
		if (!(*i).children().empty()) {
			update_all_region_subrows((*i), 2);
		}
	}
}

void
Editor::update_all_region_subrows (TreeModel::Row const &parent_row, int level)
{
	TreeModel::iterator i;
	TreeModel::Children subrows = (*parent_row).children();
	
	for (i = subrows.begin(); i != subrows.end(); ++i) {
		
		boost::shared_ptr<Region> region = (*i)[region_list_columns.region];
		
		if (!region->automatic()) {
			cerr << "level " << level << " : Updating " << region->name() << "\n";
			populate_row(region, (*i));
		}
			
		if (!(*i).children().empty()) {
			update_all_region_subrows((*i), level + 1);
		}
	}
}

void
Editor::populate_row (boost::shared_ptr<Region> region, TreeModel::Row const &row)
{
	char start_str[16];
	char end_str[16];
	char length_str[16];
	char sync_str[16];
	char fadein_str[16];
	char fadeout_str[16];
	char used_str[8];
	int used;
	BBT_Time bbt;				// FIXME Why do these have to be declared here ?
	SMPTE::Time smpte;			// FIXME I would like them declared in the case statment where they are used.
	
	bool missing_source = boost::dynamic_pointer_cast<SilentFileSource>(region->source());
	
	boost::shared_ptr<AudioRegion> audioRegion = boost::dynamic_pointer_cast<AudioRegion>(region);
	
	bool fades_in_seconds = false;

	start_str[0] = '\0';
	end_str[0] = '\0';
	length_str[0] = '\0';
	sync_str[0] = '\0';
	fadein_str[0] = '\0';
	fadeout_str[0] = '\0';
	used_str[0] = '\0';

	used = get_regionview_count_from_region_list(region);
	sprintf (used_str, "%4d" , used);
	
	switch (ARDOUR_UI::instance()->secondary_clock.mode ()) {
	case AudioClock::SMPTE:
	case AudioClock::Off:												/* If the secondary clock is off, default to SMPTE */
		session->smpte_time (region->position(), smpte);
		sprintf (start_str, "%02d:%02d:%02d:%02d", smpte.hours, smpte.minutes, smpte.seconds, smpte.frames);
		session->smpte_time (region->position() + region->length() - 1, smpte);
		sprintf (end_str, "%02d:%02d:%02d:%02d", smpte.hours, smpte.minutes, smpte.seconds, smpte.frames);
		session->smpte_time (region->length(), smpte);
		sprintf (length_str, "%02d:%02d:%02d:%02d", smpte.hours, smpte.minutes, smpte.seconds, smpte.frames);
		session->smpte_time (region->sync_position() + region->position(), smpte);
		sprintf (sync_str, "%02d:%02d:%02d:%02d", smpte.hours, smpte.minutes, smpte.seconds, smpte.frames);
		
		if (audioRegion && !fades_in_seconds) {	
			session->smpte_time (audioRegion->fade_in()->back()->when, smpte);
			sprintf (fadein_str, "%02d:%02d:%02d:%02d", smpte.hours, smpte.minutes, smpte.seconds, smpte.frames);
			session->smpte_time (audioRegion->fade_out()->back()->when, smpte);
			sprintf (fadeout_str, "%02d:%02d:%02d:%02d", smpte.hours, smpte.minutes, smpte.seconds, smpte.frames);
		}
		
		break;
		
	case AudioClock::BBT:
		session->tempo_map().bbt_time (region->position(), bbt);
		sprintf (start_str, "%03d|%02d|%04d" , bbt.bars, bbt.beats, bbt.ticks);
		session->tempo_map().bbt_time (region->position() + region->length() - 1, bbt);
		sprintf (end_str, "%03d|%02d|%04d" , bbt.bars, bbt.beats, bbt.ticks);
		session->tempo_map().bbt_time (region->length(), bbt);
		sprintf (length_str, "%03d|%02d|%04d" , bbt.bars, bbt.beats, bbt.ticks);
		session->tempo_map().bbt_time (region->sync_position() + region->position(), bbt);
		sprintf (sync_str, "%03d|%02d|%04d" , bbt.bars, bbt.beats, bbt.ticks);
		
		if (audioRegion && !fades_in_seconds) {
			session->tempo_map().bbt_time (audioRegion->fade_in()->back()->when, bbt);
			sprintf (fadein_str, "%03d|%02d|%04d" , bbt.bars, bbt.beats, bbt.ticks);
			session->tempo_map().bbt_time (audioRegion->fade_out()->back()->when, bbt);
			sprintf (fadeout_str, "%03d|%02d|%04d" , bbt.bars, bbt.beats, bbt.ticks);
		} 
		break;
		
	case AudioClock::MinSec:
		nframes_t left;
		int hrs;
		int mins;
		float secs;
	
		left = region->position();
		hrs = (int) floor (left / (session->frame_rate() * 60.0f * 60.0f));
		left -= (nframes_t) floor (hrs * session->frame_rate() * 60.0f * 60.0f);
		mins = (int) floor (left / (session->frame_rate() * 60.0f));
		left -= (nframes_t) floor (mins * session->frame_rate() * 60.0f);
		secs = left / (float) session->frame_rate();
		sprintf (start_str, "%02d:%02d:%06.3f", hrs, mins, secs);
		
		left = region->position() + region->length() - 1;
		hrs = (int) floor (left / (session->frame_rate() * 60.0f * 60.0f));
		left -= (nframes_t) floor (hrs * session->frame_rate() * 60.0f * 60.0f);
		mins = (int) floor (left / (session->frame_rate() * 60.0f));
		left -= (nframes_t) floor (mins * session->frame_rate() * 60.0f);
		secs = left / (float) session->frame_rate();
		sprintf (end_str, "%02d:%02d:%06.3f", hrs, mins, secs);
		
		left = region->length();
		hrs = (int) floor (left / (session->frame_rate() * 60.0f * 60.0f));
		left -= (nframes_t) floor (hrs * session->frame_rate() * 60.0f * 60.0f);
		mins = (int) floor (left / (session->frame_rate() * 60.0f));
		left -= (nframes_t) floor (mins * session->frame_rate() * 60.0f);
		secs = left / (float) session->frame_rate();
		sprintf (length_str, "%02d:%02d:%06.3f", hrs, mins, secs);
		
		left = region->sync_position() + region->position();
		hrs = (int) floor (left / (session->frame_rate() * 60.0f * 60.0f));
		left -= (nframes_t) floor (hrs * session->frame_rate() * 60.0f * 60.0f);
		mins = (int) floor (left / (session->frame_rate() * 60.0f));
		left -= (nframes_t) floor (mins * session->frame_rate() * 60.0f);
		secs = left / (float) session->frame_rate();
		sprintf (sync_str, "%02d:%02d:%06.3f", hrs, mins, secs);
		
		if (audioRegion && !fades_in_seconds) {
			left = audioRegion->fade_in()->back()->when;
			hrs = (int) floor (left / (session->frame_rate() * 60.0f * 60.0f));
			left -= (nframes_t) floor (hrs * session->frame_rate() * 60.0f * 60.0f);
			mins = (int) floor (left / (session->frame_rate() * 60.0f));
			left -= (nframes_t) floor (mins * session->frame_rate() * 60.0f);
			secs = left / (float) session->frame_rate();
			sprintf (fadein_str, "%02d:%02d:%06.3f", hrs, mins, secs);
			
			left = audioRegion->fade_out()->back()->when;
			hrs = (int) floor (left / (session->frame_rate() * 60.0f * 60.0f));
			left -= (nframes_t) floor (hrs * session->frame_rate() * 60.0f * 60.0f);
			mins = (int) floor (left / (session->frame_rate() * 60.0f));
			left -= (nframes_t) floor (mins * session->frame_rate() * 60.0f);
			secs = left / (float) session->frame_rate();
			sprintf (fadeout_str, "%02d:%02d:%06.3f", hrs, mins, secs);
		}
		
		break;
		
	case AudioClock::Frames:
		snprintf (start_str, sizeof (start_str), "%u", region->position());
		snprintf (end_str, sizeof (end_str), "%u", (region->position() + region->length() - 1));
		snprintf (length_str, sizeof (length_str), "%u", region->length());
		snprintf (sync_str, sizeof (sync_str), "%u", region->sync_position() + region->position());
		
		if (audioRegion && !fades_in_seconds) {
			snprintf (fadein_str, sizeof (fadein_str), "%u", uint (audioRegion->fade_in()->back()->when));
			snprintf (fadeout_str, sizeof (fadeout_str), "%u", uint (audioRegion->fade_out()->back()->when));
		}
		
		break;
	
	default:
		break;
	}
	
	if (audioRegion && fades_in_seconds) {
			
		nframes_t left;
		int mins;
		int millisecs;
		
		left = audioRegion->fade_in()->back()->when;
		mins = (int) floor (left / (session->frame_rate() * 60.0f));
		left -= (nframes_t) floor (mins * session->frame_rate() * 60.0f);
		millisecs = (int) floor ((left * 1000.0f) / session->frame_rate());
		
		if (audioRegion->fade_in()->back()->when >= session->frame_rate()) {
			sprintf (fadein_str, "%01dM %01dmS", mins, millisecs);
		} else {
			sprintf (fadein_str, "%01dmS", millisecs);
		}
		
		left = audioRegion->fade_out()->back()->when;
		mins = (int) floor (left / (session->frame_rate() * 60.0f));
		left -= (nframes_t) floor (mins * session->frame_rate() * 60.0f);
		millisecs = (int) floor ((left * 1000.0f) / session->frame_rate());
		
		if (audioRegion->fade_out()->back()->when >= session->frame_rate()) {
			sprintf (fadeout_str, "%01dM %01dmS", mins, millisecs);
		} else {
			sprintf (fadeout_str, "%01dmS", millisecs);
		}
	}
	
	if (used > 1) {
		row[region_list_columns.start] = _("Multiple");
		row[region_list_columns.end] = _("Multiple");
		row[region_list_columns.sync] = _("Multiple");
		row[region_list_columns.fadein] = _("Multiple");
		row[region_list_columns.fadeout] = _("Multiple");
		row[region_list_columns.locked] = false;
		row[region_list_columns.glued] = false;
		row[region_list_columns.muted] = false;
		row[region_list_columns.opaque] = false;
	} else {
		row[region_list_columns.start] = start_str;
		row[region_list_columns.end] = end_str;
		
		if (region->sync_position() == region->position()) {
			row[region_list_columns.sync] = _("Start");
		} else if (region->sync_position() == (region->position() + region->length() - 1)) {
			row[region_list_columns.sync] = _("End");
		} else {
			row[region_list_columns.sync] = sync_str;
		}
	
		if (audioRegion) {
			if (audioRegion->fade_in_active()) {
				row[region_list_columns.fadein] = string_compose("%1%2%3", " ", fadein_str, " ");
			} else {
				row[region_list_columns.fadein] = string_compose("%1%2%3", "(", fadein_str, ")");
			}
		} else {
			row[region_list_columns.fadein] = "";
		}

		if (audioRegion) {
			if (audioRegion->fade_out_active()) {
				row[region_list_columns.fadeout] = string_compose("%1%2%3", " ", fadeout_str, " ");
			} else {
				row[region_list_columns.fadeout] = string_compose("%1%2%3", "(", fadeout_str, ")");
			}
		} else {
			row[region_list_columns.fadeout] = "";
		}
		
		row[region_list_columns.locked] = region->locked();
	
		if (region->positional_lock_style() == Region::MusicTime) {
			row[region_list_columns.glued] = true;
		} else {
			row[region_list_columns.glued] = false;
		}
		
		row[region_list_columns.muted] = region->muted();
		row[region_list_columns.opaque] = region->opaque();
	}
	
	row[region_list_columns.length] = length_str;
	row[region_list_columns.used] = used_str;
	
	if (missing_source) {
		row[region_list_columns.path] = _("MISSING ") + region->source()->name();
	} else {
		row[region_list_columns.path] = region->source()->name();
	}
	
	if (region->n_channels() > 1) {
		row[region_list_columns.name] = string_compose("%1  [%2]", region->name(), region->n_channels());
	} else {
		row[region_list_columns.name] = region->name();
	}
}

void
Editor::build_region_list_menu ()
{
	region_list_menu = dynamic_cast<Menu*>(ActionManager::get_widget ("/RegionListMenu"));
					       
	/* now grab specific menu items that we need */

	Glib::RefPtr<Action> act;

	act = ActionManager::get_action (X_("RegionList"), X_("rlShowAll"));
	if (act) {
		toggle_full_region_list_action = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
	}

	act = ActionManager::get_action (X_("RegionList"), X_("rlShowAuto"));
	if (act) {
		toggle_show_auto_regions_action = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
	}
}

void
Editor::toggle_show_auto_regions ()
{
	show_automatic_regions_in_region_list = toggle_show_auto_regions_action->get_active();
	redisplay_regions ();
}

void
Editor::toggle_full_region_list ()
{
	if (toggle_full_region_list_action->get_active()) {
		region_list_display.expand_all ();
	} else {
		region_list_display.collapse_all ();
	}
}

void
Editor::show_region_list_display_context_menu (int button, int time)
{
	if (region_list_menu == 0) {
		build_region_list_menu ();
	}

	if (region_list_display.get_selection()->count_selected_rows() > 0) {
		ActionManager::set_sensitive (ActionManager::region_list_selection_sensitive_actions, true);
	} else {
		ActionManager::set_sensitive (ActionManager::region_list_selection_sensitive_actions, false);
	}

	region_list_menu->popup (button, time);
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
	boost::shared_ptr<Region> region;
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

	if (Keyboard::is_context_menu_event (ev)) {
		show_region_list_display_context_menu (ev->button, ev->time);
		return true;
	}

	if (region == 0) {
		region_list_display.get_selection()->unselect_all();
		deselect_all();
		return false;
	}

	switch (ev->button) {
	case 1:
		break;

	case 2:
		// audition on middle click (stop audition too)
		if (!Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
			consider_auditioning (region);
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
	boost::shared_ptr<Region> region;

	if (region_list_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		if ((iter = region_list_model->get_iter (path))) {
			region = (*iter)[region_list_columns.region];
		}
	}

	if (region && Keyboard::is_delete_event (ev)) {
		session->remove_region_from_region_list (region);
		return true;
	}

	return false;
}

void
Editor::consider_auditioning (boost::shared_ptr<Region> region)
{
	boost::shared_ptr<AudioRegion> r = boost::dynamic_pointer_cast<AudioRegion> (region);

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

	session->audition_region (r);
	last_audition_region = r;
}

int
Editor::region_list_sorter (TreeModel::iterator a, TreeModel::iterator b)
{
	int cmp = 0;

	boost::shared_ptr<Region> r1 = (*a)[region_list_columns.region];
	boost::shared_ptr<Region> r2 = (*b)[region_list_columns.region];

	/* handle rows without regions, like "Hidden" */

	if (r1 == 0) {
		return -1;
	}

	if (r2 == 0) {
		return 1;
	}

	boost::shared_ptr<AudioRegion> region1 = boost::dynamic_pointer_cast<AudioRegion> (r1);
	boost::shared_ptr<AudioRegion> region2 = boost::dynamic_pointer_cast<AudioRegion> (r2);

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
		cmp = region1->source()->timestamp() - region2->source()->timestamp();
		break;
	
	case ByStartInFile:
		cmp = region1->start() - region2->start();
		break;
		
	case ByEndInFile:
		cmp = (region1->start() + region1->length()) - (region2->start() + region2->length());
		break;
		
	case BySourceFileName:
		cmp = strcasecmp (region1->source()->name().c_str(), region2->source()->name().c_str());
		break;

	case BySourceFileLength:
		cmp = region1->source()->length() - region2->source()->length();
		break;
		
	case BySourceFileCreationDate:
		cmp = region1->source()->timestamp() - region2->source()->timestamp();
		break;

	case BySourceFileFS:
		if (region1->source()->name() == region2->source()->name()) {
			cmp = strcasecmp (region1->name().c_str(),  region2->name().c_str());
		} else {
			cmp = strcasecmp (region1->source()->name().c_str(),  region2->source()->name().c_str());
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
		region_list_model->set_sort_func (0, (mem_fun (*this, &Editor::region_list_sorter)));
	}
}

void
Editor::reset_region_list_sort_direction (bool up)
{
	region_list_model->set_sort_column (0, up ? SORT_ASCENDING : SORT_DESCENDING);
}

void
Editor::region_list_selection_mapover (slot<void,boost::shared_ptr<Region> > sl)
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

			/* some rows don't have a region associated with them, but can still be
			   selected (XXX maybe prevent them from being selected)
			*/

			boost::shared_ptr<Region> r = (*iter)[region_list_columns.region];

			if (r) {
				sl (r);
			}
		}
	}
}

void
Editor::hide_a_region (boost::shared_ptr<Region> r)
{
	r->set_hidden (true);
}

void
Editor::remove_a_region (boost::shared_ptr<Region> r)
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
	vector<ustring> paths;

	if (data.get_target() == "GTK_TREE_MODEL_ROW") {
		region_list_display.on_drag_data_received (context, x, y, data, info, time);
		return;
	}

	if (convert_drop_to_paths (paths, context, x, y, data, info, time) == 0) {
		nframes64_t pos = 0;
		if (Profile->get_sae() || Config->get_only_copy_imported_files()) {
			do_import (paths, Editing::ImportDistinctFiles, Editing::ImportAsRegion, SrcBest, pos); 
		} else {
			do_embed (paths, Editing::ImportDistinctFiles, ImportAsRegion, pos);
		}
		context->drag_finish (true, false, time);
	}
}

bool
Editor::region_list_selection_filter (const RefPtr<TreeModel>& model, const TreeModel::Path& path, bool yn)
{
	/* not possible to select rows that do not represent regions, like "Hidden" */
	
	TreeModel::iterator iter = model->get_iter (path);

	if (iter) {
		boost::shared_ptr<Region> r =(*iter)[region_list_columns.region];
		if (!r) {
			return false;
		}
	} 

	return true;
}

void
Editor::region_name_edit (const Glib::ustring& path, const Glib::ustring& new_text)
{
	boost::shared_ptr<Region> region;
	TreeIter iter;
	
	if ((iter = region_list_model->get_iter (path))) {
		region = (*iter)[region_list_columns.region];
		(*iter)[region_list_columns.name] = new_text;
	}
	
	/* now mapover everything */

	if (region) {
		vector<RegionView*> equivalents;
		get_regions_corresponding_to (region, equivalents);

		for (vector<RegionView*>::iterator i = equivalents.begin(); i != equivalents.end(); ++i) {
			if (new_text != (*i)->region()->name()) {
				(*i)->region()->set_name (new_text);
			}
		}
	}

}

