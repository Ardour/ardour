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

*/

#include <cstdlib>
#include <cmath>

#include <gtkmm2ext/gtk_ui.h>

#include "ardour/session.h"
#include "ardour/location.h"
#include "ardour/profile.h"
#include "pbd/memento_command.h"

#include "canvas/canvas.h"
#include "canvas/item.h"
#include "canvas/rectangle.h"

#include "editor.h"
#include "marker.h"
#include "selection.h"
#include "editing.h"
#include "gui_thread.h"
#include "actions.h"
#include "prompter.h"
#include "editor_drag.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;

void
Editor::clear_marker_display ()
{
	for (LocationMarkerMap::iterator i = location_markers.begin(); i != location_markers.end(); ++i) {
		delete i->second;
	}

	location_markers.clear ();
	_sorted_marker_lists.clear ();
}

void
Editor::add_new_location (Location *location)
{
	ENSURE_GUI_THREAD (*this, &Editor::add_new_location, location);

	ArdourCanvas::Container* group = add_new_location_internal (location);

	/* Do a full update of the markers in this group */
	update_marker_labels (group);

	if (location->is_auto_punch()) {
		update_punch_range_view ();
	}

	if (location->is_auto_loop()) {
		update_loop_range_view ();
	}
}

/** Add a new location, without a time-consuming update of all marker labels;
 *  the caller must call update_marker_labels () after calling this.
 *  @return canvas group that the location's marker was added to.
 */
ArdourCanvas::Container*
Editor::add_new_location_internal (Location* location)
{
	LocationMarkers *lam = new LocationMarkers;
	uint32_t color;

	/* make a note here of which group this marker ends up in */
	ArdourCanvas::Container* group = 0;

	if (location->is_cd_marker()) {
		color = location_cd_marker_color;
	} else if (location->is_mark()) {
		color = location_marker_color;
	} else if (location->is_auto_loop()) {
		color = location_loop_color;
	} else if (location->is_auto_punch()) {
		color = location_punch_color;
	} else {
		color = location_range_color;
	}

	if (location->is_mark()) {

		if (location->is_cd_marker() && ruler_cd_marker_action->get_active()) {
			lam->start = new ArdourMarker (*this, *cd_marker_group, color, location->name(), ArdourMarker::Mark, location->start());
			group = cd_marker_group;
		} else {
			lam->start = new ArdourMarker (*this, *marker_group, color, location->name(), ArdourMarker::Mark, location->start());
			group = marker_group;
		}

		lam->end = 0;

	} else if (location->is_auto_loop()) {

		// transport marker
		lam->start = new ArdourMarker (*this, *transport_marker_group, color,
					 location->name(), ArdourMarker::LoopStart, location->start());
		lam->end   = new ArdourMarker (*this, *transport_marker_group, color,
					 location->name(), ArdourMarker::LoopEnd, location->end());
		group = transport_marker_group;

	} else if (location->is_auto_punch()) {

		// transport marker
		lam->start = new ArdourMarker (*this, *transport_marker_group, color,
					 location->name(), ArdourMarker::PunchIn, location->start());
		lam->end   = new ArdourMarker (*this, *transport_marker_group, color,
					 location->name(), ArdourMarker::PunchOut, location->end());
		group = transport_marker_group;

	} else if (location->is_session_range()) {

		// session range
		lam->start = new ArdourMarker (*this, *marker_group, color, _("start"), ArdourMarker::SessionStart, location->start());
		lam->end = new ArdourMarker (*this, *marker_group, color, _("end"), ArdourMarker::SessionEnd, location->end());
		group = marker_group;

	} else {
		// range marker
		if (location->is_cd_marker() && ruler_cd_marker_action->get_active()) {
			lam->start = new ArdourMarker (*this, *cd_marker_group, color,
						 location->name(), ArdourMarker::RangeStart, location->start());
			lam->end   = new ArdourMarker (*this, *cd_marker_group, color,
						 location->name(), ArdourMarker::RangeEnd, location->end());
			group = cd_marker_group;
		} else {
			lam->start = new ArdourMarker (*this, *range_marker_group, color,
						 location->name(), ArdourMarker::RangeStart, location->start());
			lam->end   = new ArdourMarker (*this, *range_marker_group, color,
						 location->name(), ArdourMarker::RangeEnd, location->end());
			group = range_marker_group;
		}
	}

	if (location->is_hidden ()) {
		lam->hide();
	} else {
		lam->show ();
	}

	location->name_changed.connect (*this, invalidator (*this), boost::bind (&Editor::location_changed, this, _1), gui_context());
	location->FlagsChanged.connect (*this, invalidator (*this), boost::bind (&Editor::location_flags_changed, this, location), gui_context());

	pair<Location*,LocationMarkers*> newpair;

	newpair.first = location;
	newpair.second = lam;

	location_markers.insert (newpair);

	if (select_new_marker && location->is_mark()) {
		selection->set (lam->start);
		select_new_marker = false;
	}

	lam->canvas_height_set (_visible_canvas_height);
	lam->set_show_lines (_show_marker_lines);

	/* Add these markers to the appropriate sorted marker lists, which will render
	   them unsorted until a call to update_marker_labels() sorts them out.
	*/
	_sorted_marker_lists[group].push_back (lam->start);
	if (lam->end) {
		_sorted_marker_lists[group].push_back (lam->end);
	}

	return group;
}

void
Editor::location_changed (Location *location)
{
	ENSURE_GUI_THREAD (*this, &Editor::location_changed, location)

	LocationMarkers *lam = find_location_markers (location);

	if (lam == 0) {
		/* a location that isn't "marked" with markers */
		return;
	}

	lam->set_name (location->name ());
	lam->set_position (location->start(), location->end());

	if (location->is_auto_loop()) {
		update_loop_range_view ();
	} else if (location->is_auto_punch()) {
		update_punch_range_view ();
	}

	check_marker_label (lam->start);
	if (lam->end) {
		check_marker_label (lam->end);
	}
}

/** Look at a marker and check whether its label, and those of the previous and next markers,
 *  need to have their labels updated (in case those labels need to be shortened or can be
 *  lengthened)
 */
void
Editor::check_marker_label (ArdourMarker* m)
{
	/* Get a time-ordered list of markers from the last time anything changed */
	std::list<ArdourMarker*>& sorted = _sorted_marker_lists[m->get_parent()];

	list<ArdourMarker*>::iterator i = find (sorted.begin(), sorted.end(), m);

	list<ArdourMarker*>::iterator prev = sorted.end ();
	list<ArdourMarker*>::iterator next = i;
	++next;

	/* Look to see if the previous marker is still behind `m' in time */
	if (i != sorted.begin()) {

		prev = i;
		--prev;

		if ((*prev)->position() > m->position()) {
			/* This marker is no longer in the correct order with the previous one, so
			 * update all the markers in this group.
			 */
			update_marker_labels (m->get_parent ());
			return;
		}
	}

	/* Look to see if the next marker is still ahead of `m' in time */
	if (next != sorted.end() && (*next)->position() < m->position()) {
		/* This marker is no longer in the correct order with the next one, so
		 * update all the markers in this group.
		 */
		update_marker_labels (m->get_parent ());
		return;
	}

	if (prev != sorted.end()) {

		/* Update just the available space between the previous marker and this one */

		double const p = sample_to_pixel (m->position() - (*prev)->position());

		if (m->label_on_left()) {
			(*prev)->set_right_label_limit (p / 2);
		} else {
			(*prev)->set_right_label_limit (p);
		}

		if ((*prev)->label_on_left ()) {
			m->set_left_label_limit (p);
		} else {
			m->set_left_label_limit (p / 2);
		}
	}

	if (next != sorted.end()) {

		/* Update just the available space between this marker and the next */

		double const p = sample_to_pixel ((*next)->position() - m->position());

		if ((*next)->label_on_left()) {
			m->set_right_label_limit (p / 2);
		} else {
			m->set_right_label_limit (p);
		}

		if (m->label_on_left()) {
			(*next)->set_left_label_limit (p);
		} else {
			(*next)->set_left_label_limit (p / 2);
		}
	}
}

struct MarkerComparator {
	bool operator() (ArdourMarker const * a, ArdourMarker const * b) {
		return a->position() < b->position();
	}
};

/** Update all marker labels in all groups */
void
Editor::update_marker_labels ()
{
	for (std::map<ArdourCanvas::Container *, std::list<ArdourMarker *> >::iterator i = _sorted_marker_lists.begin(); i != _sorted_marker_lists.end(); ++i) {
		update_marker_labels (i->first);
	}
}

/** Look at all markers in a group and update label widths */
void
Editor::update_marker_labels (ArdourCanvas::Container* group)
{
	list<ArdourMarker*>& sorted = _sorted_marker_lists[group];

	if (sorted.empty()) {
		return;
	}

	/* We sort the list of markers and then set up the space available between each one */

	sorted.sort (MarkerComparator ());

	list<ArdourMarker*>::iterator i = sorted.begin ();

	list<ArdourMarker*>::iterator prev = sorted.end ();
	list<ArdourMarker*>::iterator next = i;

	if (next != sorted.end()) {
		++next;
	}

	while (i != sorted.end()) {

		if (prev != sorted.end()) {
			double const p = sample_to_pixel ((*i)->position() - (*prev)->position());

			if ((*prev)->label_on_left()) {
				(*i)->set_left_label_limit (p);
			} else {
				(*i)->set_left_label_limit (p / 2);
			}

		}

		if (next != sorted.end()) {
			double const p = sample_to_pixel ((*next)->position() - (*i)->position());

			if ((*next)->label_on_left()) {
				(*i)->set_right_label_limit (p / 2);
			} else {
				(*i)->set_right_label_limit (p);
			}

			++next;
		}

		prev = i;
		++i;
	}
}

void
Editor::location_flags_changed (Location *location)
{
	ENSURE_GUI_THREAD (*this, &Editor::location_flags_changed, location, src)

	LocationMarkers *lam = find_location_markers (location);

	if (lam == 0) {
		/* a location that isn't "marked" with markers */
		return;
	}

	// move cd markers to/from cd marker bar as appropriate
	ensure_cd_marker_updated (lam, location);

	if (location->is_cd_marker()) {
		lam->set_color_rgba (location_cd_marker_color);
	} else if (location->is_mark()) {
		lam->set_color_rgba (location_marker_color);
	} else if (location->is_auto_punch()) {
		lam->set_color_rgba (location_punch_color);
	} else if (location->is_auto_loop()) {
		lam->set_color_rgba (location_loop_color);
	} else {
		lam->set_color_rgba (location_range_color);
	}

	if (location->is_hidden()) {
		lam->hide();
	} else {
		lam->show ();
	}
}

void Editor::update_cd_marker_display ()
{
	for (LocationMarkerMap::iterator i = location_markers.begin(); i != location_markers.end(); ++i) {
		LocationMarkers * lam = i->second;
		Location * location = i->first;

		ensure_cd_marker_updated (lam, location);
	}
}

void Editor::ensure_cd_marker_updated (LocationMarkers * lam, Location * location)
{
	if (location->is_cd_marker()
	    && (ruler_cd_marker_action->get_active() &&  lam->start->get_parent() != cd_marker_group))
	{
		//cerr << "reparenting non-cd marker so it can be relocated: " << location->name() << endl;
		if (lam->start) {
			lam->start->reparent (*cd_marker_group);
		}
		if (lam->end) {
			lam->end->reparent (*cd_marker_group);
		}
	}
	else if ( (!location->is_cd_marker() || !ruler_cd_marker_action->get_active())
		  && (lam->start->get_parent() == cd_marker_group))
	{
		//cerr << "reparenting non-cd marker so it can be relocated: " << location->name() << endl;
		if (location->is_mark()) {
			if (lam->start) {
				lam->start->reparent (*marker_group);
			}
			if (lam->end) {
				lam->end->reparent (*marker_group);
			}
		}
		else {
			if (lam->start) {
				lam->start->reparent (*range_marker_group);
			}
			if (lam->end) {
				lam->end->reparent (*range_marker_group);
			}
		}
	}
}

Editor::LocationMarkers::~LocationMarkers ()
{
	delete start;
	delete end;
}

Editor::LocationMarkers *
Editor::find_location_markers (Location *location) const
{
	LocationMarkerMap::const_iterator i;

	for (i = location_markers.begin(); i != location_markers.end(); ++i) {
		if ((*i).first == location) {
			return (*i).second;
		}
	}

	return 0;
}

Location *
Editor::find_location_from_marker (ArdourMarker *marker, bool& is_start) const
{
	LocationMarkerMap::const_iterator i;

	for (i = location_markers.begin(); i != location_markers.end(); ++i) {
		LocationMarkers *lm = (*i).second;
		if (lm->start == marker) {
			is_start = true;
			return (*i).first;
		} else if (lm->end == marker) {
			is_start = false;
			return (*i).first;
		}
	}

	return 0;
}

void
Editor::refresh_location_display_internal (const Locations::LocationList& locations)
{
	/* invalidate all */

	for (LocationMarkerMap::iterator i = location_markers.begin(); i != location_markers.end(); ++i) {
		i->second->valid = false;
	}

	/* add new ones */

	for (Locations::LocationList::const_iterator i = locations.begin(); i != locations.end(); ++i) {

		LocationMarkerMap::iterator x;

		if ((x = location_markers.find (*i)) != location_markers.end()) {
			x->second->valid = true;
			continue;
		}

		add_new_location_internal (*i);
	}

	/* remove dead ones */

	for (LocationMarkerMap::iterator i = location_markers.begin(); i != location_markers.end(); ) {

		LocationMarkerMap::iterator tmp;

		tmp = i;
		++tmp;

		if (!i->second->valid) {

			remove_sorted_marker (i->second->start);
			if (i->second->end) {
				remove_sorted_marker (i->second->end);
			}

			LocationMarkers* m = i->second;
			location_markers.erase (i);
			delete m;
		}

		i = tmp;
	}

	update_punch_range_view ();
	update_loop_range_view ();
}

void
Editor::refresh_location_display ()
{
	ENSURE_GUI_THREAD (*this, &Editor::refresh_location_display)

	if (_session) {
		_session->locations()->apply (*this, &Editor::refresh_location_display_internal);
	}

	update_marker_labels ();
}

void
Editor::LocationMarkers::hide()
{
	start->hide ();
	if (end) {
		end->hide ();
	}
}

void
Editor::LocationMarkers::show()
{
	start->show ();
	if (end) {
		end->show ();
	}
}

void
Editor::LocationMarkers::canvas_height_set (double h)
{
	start->canvas_height_set (h);
	if (end) {
		end->canvas_height_set (h);
	}
}

void
Editor::LocationMarkers::set_name (const string& str)
{
	/* XXX: hack: don't change names of session start/end markers */

	if (start->type() != ArdourMarker::SessionStart) {
		start->set_name (str);
	}

	if (end && end->type() != ArdourMarker::SessionEnd) {
		end->set_name (str);
	}
}

void
Editor::LocationMarkers::set_position (framepos_t startf,
				       framepos_t endf)
{
	start->set_position (startf);
	if (end) {
		end->set_position (endf);
	}
}

void
Editor::LocationMarkers::set_color_rgba (uint32_t rgba)
{
	start->set_color_rgba (rgba);
	if (end) {
		end->set_color_rgba (rgba);
	}
}

void
Editor::LocationMarkers::set_show_lines (bool s)
{
	start->set_show_line (s);
	if (end) {
		end->set_show_line (s);
	}
}

void
Editor::LocationMarkers::set_selected (bool s)
{
	start->set_selected (s);
	if (end) {
		end->set_selected (s);
	}
}

void
Editor::LocationMarkers::setup_lines ()
{
	start->setup_line ();
	if (end) {
		end->setup_line ();
	}
}

void
Editor::mouse_add_new_marker (framepos_t where, bool is_cd)
{
	string markername;
	int flags = (is_cd ? Location::IsCDMarker|Location::IsMark : Location::IsMark);

	if (_session) {
		_session->locations()->next_available_name(markername, _("mark"));
		if (!choose_new_marker_name(markername)) {
			return;
		}
		Location *location = new Location (*_session, where, where, markername, (Location::Flags) flags, get_grid_music_divisions (0));
		begin_reversible_command (_("add marker"));

		XMLNode &before = _session->locations()->get_state();
		_session->locations()->add (location, true);
		XMLNode &after = _session->locations()->get_state();
		_session->add_command (new MementoCommand<Locations>(*(_session->locations()), &before, &after));

		/* find the marker we just added */

		LocationMarkers *lam = find_location_markers (location);
		if (lam) {
			/* make it the selected marker */
			selection->set (lam->start);
		}

		commit_reversible_command ();
	}
}

void
Editor::mouse_add_new_loop (framepos_t where)
{
	if (!_session) {
		return;
	}

	/* Make this marker 1/8th of the visible area of the session so that
	   it's reasonably easy to manipulate after creation.
	*/

	framepos_t const end = where + current_page_samples() / 8;

	set_loop_range (where, end,  _("set loop range"));
}

void
Editor::mouse_add_new_punch (framepos_t where)
{
	if (!_session) {
		return;
	}

	/* Make this marker 1/8th of the visible area of the session so that
	   it's reasonably easy to manipulate after creation.
	*/

	framepos_t const end = where + current_page_samples() / 8;

	set_punch_range (where, end,  _("set punch range"));
}

void
Editor::mouse_add_new_range (framepos_t where)
{
	if (!_session) {
		return;
	}

	/* Make this marker 1/8th of the visible area of the session so that
	   it's reasonably easy to manipulate after creation.
	*/

	framepos_t const end = where + current_page_samples() / 8;

	string name;
	_session->locations()->next_available_name (name, _("range"));
	Location* loc = new Location (*_session, where, end, name, Location::IsRangeMarker);

	begin_reversible_command (_("new range marker"));
	XMLNode& before = _session->locations()->get_state ();
	_session->locations()->add (loc, true);
	XMLNode& after = _session->locations()->get_state ();
	_session->add_command (new MementoCommand<Locations> (*_session->locations(), &before, &after));
	commit_reversible_command ();
}

void
Editor::remove_marker (ArdourCanvas::Item& item, GdkEvent*)
{
	ArdourMarker* marker;
	bool is_start;

	if ((marker = static_cast<ArdourMarker*> (item.get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	if (entered_marker == marker) {
		entered_marker = NULL;
	}

	Location* loc = find_location_from_marker (marker, is_start);

	if (_session && loc) {
	  	Glib::signal_idle().connect (sigc::bind (sigc::mem_fun(*this, &Editor::really_remove_marker), loc));
	}
}

gint
Editor::really_remove_marker (Location* loc)
{
	begin_reversible_command (_("remove marker"));
	XMLNode &before = _session->locations()->get_state();
	_session->locations()->remove (loc);
	XMLNode &after = _session->locations()->get_state();
	_session->add_command (new MementoCommand<Locations>(*(_session->locations()), &before, &after));
	commit_reversible_command ();
	return FALSE;
}

void
Editor::location_gone (Location *location)
{
	ENSURE_GUI_THREAD (*this, &Editor::location_gone, location)

	LocationMarkerMap::iterator i;

	if (location == transport_loop_location()) {
		update_loop_range_view ();
	}

	if (location == transport_punch_location()) {
		update_punch_range_view ();
	}

	for (i = location_markers.begin(); i != location_markers.end(); ++i) {
		if (i->first == location) {

			remove_sorted_marker (i->second->start);
			if (i->second->end) {
				remove_sorted_marker (i->second->end);
			}

			LocationMarkers* m = i->second;
			location_markers.erase (i);
			delete m;

			/* Markers that visually overlap with this (removed) marker
			 * need to be re-displayed.
			 * But finding such cases is similarly expensive as simply
			 * re-displaying all..  so:
			 */
			refresh_location_display ();
			break;
		}
	}
}

void
Editor::tempo_or_meter_marker_context_menu (GdkEventButton* ev, ArdourCanvas::Item* item)
{
	marker_menu_item = item;

	MeterMarker* mm;
	TempoMarker* tm;
	dynamic_cast_marker_object (marker_menu_item->get_data ("marker"), &mm, &tm);

	bool can_remove = false;

	if (mm) {
		can_remove = !mm->meter().initial ();
		delete meter_marker_menu;
		build_meter_marker_menu (mm, can_remove);
		meter_marker_menu->popup (1, ev->time);
	} else if (tm) {
		if (!tm->tempo().active()) {
			return;
		}
		can_remove = !tm->tempo().initial() && !tm->tempo().locked_to_meter();
		delete tempo_marker_menu;
		build_tempo_marker_menu (tm, can_remove);
		tempo_marker_menu->popup (1, ev->time);
	} else {
		return;
	}
}

void
Editor::marker_context_menu (GdkEventButton* ev, ArdourCanvas::Item* item)
{
	ArdourMarker * marker;
	if ((marker = reinterpret_cast<ArdourMarker *> (item->get_data("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	bool is_start;
	Location * loc = find_location_from_marker (marker, is_start);

	if (loc == transport_loop_location() || loc == transport_punch_location() || loc->is_session_range ()) {

		delete transport_marker_menu;
		build_range_marker_menu (loc, loc == transport_loop_location() || loc == transport_punch_location(), loc->is_session_range());

		marker_menu_item = item;
		transport_marker_menu->popup (1, ev->time);

	} else if (loc->is_mark()) {

			delete marker_menu;
			build_marker_menu (loc);

		// GTK2FIX use action group sensitivity
#ifdef GTK2FIX
			if (children.size() >= 3) {
				MenuItem * loopitem = &children[2];
				if (loopitem) {
					if (loc->is_mark()) {
						loopitem->set_sensitive(false);
					}
					else {
						loopitem->set_sensitive(true);
					}
				}
			}
#endif
			marker_menu_item = item;
			marker_menu->popup (1, ev->time);

	} else if (loc->is_range_marker()) {
		delete range_marker_menu;
		build_range_marker_menu (loc, false, false);

		marker_menu_item = item;
		range_marker_menu->popup (1, ev->time);
	}
}

void
Editor::new_transport_marker_context_menu (GdkEventButton* ev, ArdourCanvas::Item*)
{
	if (new_transport_marker_menu == 0) {
		build_new_transport_marker_menu ();
	}

	new_transport_marker_menu->popup (1, ev->time);

}

void
Editor::build_marker_menu (Location* loc)
{
	using namespace Menu_Helpers;

	marker_menu = new Menu;
	MenuList& items = marker_menu->items();
	marker_menu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Locate to Here"), sigc::mem_fun(*this, &Editor::marker_menu_set_playhead)));
	items.push_back (MenuElem (_("Play from Here"), sigc::mem_fun(*this, &Editor::marker_menu_play_from)));
	items.push_back (MenuElem (_("Move Mark to Playhead"), sigc::mem_fun(*this, &Editor::marker_menu_set_from_playhead)));

	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Create Range to Next Marker"), sigc::mem_fun(*this, &Editor::marker_menu_range_to_next)));

	items.push_back (MenuElem (_("Hide"), sigc::mem_fun(*this, &Editor::marker_menu_hide)));
	items.push_back (MenuElem (_("Rename..."), sigc::mem_fun(*this, &Editor::marker_menu_rename)));

	items.push_back (CheckMenuElem (_("Lock")));
	Gtk::CheckMenuItem* lock_item = static_cast<Gtk::CheckMenuItem*> (&items.back());
	if (loc->locked ()) {
		lock_item->set_active ();
	}
	lock_item->signal_activate().connect (sigc::mem_fun (*this, &Editor::toggle_marker_menu_lock));

	items.push_back (CheckMenuElem (_("Glue to Bars and Beats")));
	Gtk::CheckMenuItem* glue_item = static_cast<Gtk::CheckMenuItem*> (&items.back());
	glue_item->set_active (loc->position_lock_style() == MusicTime);

	glue_item->signal_activate().connect (sigc::mem_fun (*this, &Editor::toggle_marker_menu_glue));

	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Remove"), sigc::mem_fun(*this, &Editor::marker_menu_remove)));
}

void
Editor::build_range_marker_menu (Location* loc, bool loop_or_punch, bool session)
{
	using namespace Menu_Helpers;

	bool const loop_or_punch_or_session = loop_or_punch || session;

	Menu *markerMenu = new Menu;
	if (loop_or_punch_or_session) {
		transport_marker_menu = markerMenu;
	} else {
		range_marker_menu = markerMenu;
	}
	MenuList& items = markerMenu->items();
	markerMenu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Play Range"), sigc::mem_fun(*this, &Editor::marker_menu_play_range)));
	items.push_back (MenuElem (_("Locate to Marker"), sigc::mem_fun(*this, &Editor::marker_menu_set_playhead)));
	items.push_back (MenuElem (_("Play from Marker"), sigc::mem_fun(*this, &Editor::marker_menu_play_from)));
	items.push_back (MenuElem (_("Loop Range"), sigc::mem_fun(*this, &Editor::marker_menu_loop_range)));

	items.push_back (MenuElem (_("Set Marker from Playhead"), sigc::mem_fun(*this, &Editor::marker_menu_set_from_playhead)));
	items.push_back (MenuElem (_("Set Range from Selection"), sigc::bind (sigc::mem_fun(*this, &Editor::marker_menu_set_from_selection), false)));

	items.push_back (MenuElem (_("Zoom to Range"), sigc::mem_fun (*this, &Editor::marker_menu_zoom_to_range)));

	items.push_back (SeparatorElem());
	items.push_back (CheckMenuElem (_("Glue to Bars and Beats")));

	Gtk::CheckMenuItem* glue_item = static_cast<Gtk::CheckMenuItem*> (&items.back());
	glue_item->set_active (loc->position_lock_style() == MusicTime);
	glue_item->signal_activate().connect (sigc::mem_fun (*this, &Editor::toggle_marker_menu_glue));

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Export Range..."), sigc::mem_fun(*this, &Editor::export_range)));
	items.push_back (SeparatorElem());

	if (!loop_or_punch_or_session) {
		items.push_back (MenuElem (_("Hide Range"), sigc::mem_fun(*this, &Editor::marker_menu_hide)));
		items.push_back (MenuElem (_("Rename Range..."), sigc::mem_fun(*this, &Editor::marker_menu_rename)));
	}

	if (!session) {
		items.push_back (MenuElem (_("Remove Range"), sigc::mem_fun(*this, &Editor::marker_menu_remove)));
	}

	if (!loop_or_punch_or_session || !session) {
		items.push_back (SeparatorElem());
	}

	items.push_back (MenuElem (_("Separate Regions in Range"), sigc::mem_fun(*this, &Editor::marker_menu_separate_regions_using_location)));
	items.push_back (MenuElem (_("Select All in Range"), sigc::mem_fun(*this, &Editor::marker_menu_select_all_selectables_using_range)));
	items.push_back (MenuElem (_("Select Range"), sigc::mem_fun(*this, &Editor::marker_menu_select_using_range)));
}

void
Editor::build_tempo_marker_menu (TempoMarker* loc, bool can_remove)
{
	using namespace Menu_Helpers;

	tempo_marker_menu = new Menu;
	MenuList& items = tempo_marker_menu->items();
	tempo_marker_menu->set_name ("ArdourContextMenu");

	if (loc->tempo().type() == TempoSection::Constant) {
		items.push_back (MenuElem (_("Make Ramped"), sigc::mem_fun(*this, &Editor::toggle_tempo_type)));
	} else {
		items.push_back (MenuElem (_("Make Constant"), sigc::mem_fun(*this, &Editor::toggle_tempo_type)));
	}

	if (loc->tempo().position_lock_style() == AudioTime && can_remove) {
		items.push_back (MenuElem (_("Lock to Music"), sigc::mem_fun(*this, &Editor::toggle_marker_lock_style)));
	} else if (can_remove) {
		items.push_back (MenuElem (_("Lock to Audio"), sigc::mem_fun(*this, &Editor::toggle_marker_lock_style)));
	}

	items.push_back (MenuElem (_("Edit..."), sigc::mem_fun(*this, &Editor::marker_menu_edit)));
	items.push_back (MenuElem (_("Remove"), sigc::mem_fun(*this, &Editor::marker_menu_remove)));
	items.back().set_sensitive (can_remove);
}

void
Editor::build_meter_marker_menu (MeterMarker* loc, bool can_remove)
{
	using namespace Menu_Helpers;

	meter_marker_menu = new Menu;
	MenuList& items = meter_marker_menu->items();
	meter_marker_menu->set_name ("ArdourContextMenu");

	if (loc->meter().position_lock_style() == AudioTime && can_remove) {
		items.push_back (MenuElem (_("Lock to Music"), sigc::mem_fun(*this, &Editor::toggle_marker_lock_style)));
	} else if (can_remove) {
		items.push_back (MenuElem (_("Lock to Audio"), sigc::mem_fun(*this, &Editor::toggle_marker_lock_style)));
	}

	items.push_back (MenuElem (_("Edit..."), sigc::mem_fun(*this, &Editor::marker_menu_edit)));
	items.push_back (MenuElem (_("Remove"), sigc::mem_fun(*this, &Editor::marker_menu_remove)));

	items.back().set_sensitive (can_remove);
}

void
Editor::build_new_transport_marker_menu ()
{
	using namespace Menu_Helpers;

	new_transport_marker_menu = new Menu;
	MenuList& items = new_transport_marker_menu->items();
	new_transport_marker_menu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Set Loop Range"), sigc::mem_fun(*this, &Editor::new_transport_marker_menu_set_loop)));
	items.push_back (MenuElem (_("Set Punch Range"), sigc::mem_fun(*this, &Editor::new_transport_marker_menu_set_punch)));

	new_transport_marker_menu->signal_unmap().connect ( sigc::mem_fun(*this, &Editor::new_transport_marker_menu_popdown));
}

void
Editor::marker_menu_hide ()
{
	ArdourMarker* marker;

	if ((marker = reinterpret_cast<ArdourMarker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	Location* l;
	bool is_start;

	if ((l = find_location_from_marker (marker, is_start)) != 0) {
		l->set_hidden (true, this);
	}
}

void
Editor::marker_menu_select_using_range ()
{
	ArdourMarker* marker;

	if ((marker = reinterpret_cast<ArdourMarker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	Location* l;
	bool is_start;

	if (((l = find_location_from_marker (marker, is_start)) != 0) && (l->end() > l->start())) {
	        set_selection_from_range (*l);
	}
}

void
Editor::marker_menu_select_all_selectables_using_range ()
{
	ArdourMarker* marker;

	if ((marker = reinterpret_cast<ArdourMarker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	Location* l;
	bool is_start;

	if (((l = find_location_from_marker (marker, is_start)) != 0) && (l->end() > l->start())) {
	        select_all_within (l->start(), l->end() - 1, 0,  DBL_MAX, track_views, Selection::Set, false);
	}

}

void
Editor::marker_menu_separate_regions_using_location ()
{
	ArdourMarker* marker;

	if ((marker = reinterpret_cast<ArdourMarker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	Location* l;
	bool is_start;

	if (((l = find_location_from_marker (marker, is_start)) != 0) && (l->end() > l->start())) {
	        separate_regions_using_location (*l);
	}

}

void
Editor::marker_menu_play_from ()
{
	ArdourMarker* marker;

	if ((marker = reinterpret_cast<ArdourMarker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	Location* l;
	bool is_start;

	if ((l = find_location_from_marker (marker, is_start)) != 0) {

		if (l->is_mark()) {
			_session->request_locate (l->start(), true);
		}
		else {
			//_session->request_bounded_roll (l->start(), l->end());

			if (is_start) {
				_session->request_locate (l->start(), true);
			} else {
				_session->request_locate (l->end(), true);
			}
		}
	}
}

void
Editor::marker_menu_set_playhead ()
{
	ArdourMarker* marker;

	if ((marker = reinterpret_cast<ArdourMarker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	Location* l;
	bool is_start;

	if ((l = find_location_from_marker (marker, is_start)) != 0) {

		if (l->is_mark()) {
			_session->request_locate (l->start(), false);
		}
		else {
			if (is_start) {
				_session->request_locate (l->start(), false);
			} else {
				_session->request_locate (l->end(), false);
			}
		}
	}
}

void
Editor::marker_menu_range_to_next ()
{
	ArdourMarker* marker;
	if (!_session) {
		return;
	}

	if ((marker = reinterpret_cast<ArdourMarker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	Location* l;
	bool is_start;

	if ((l = find_location_from_marker (marker, is_start)) == 0) {
		return;
	}

	framepos_t start;
	framepos_t end;
	_session->locations()->marks_either_side (marker->position(), start, end);

	if (end != max_framepos) {
		string range_name = l->name();
		range_name += "-range";

		Location* newrange = new Location (*_session, marker->position(), end, range_name, Location::IsRangeMarker);
		_session->locations()->add (newrange);
	}
}

void
Editor::marker_menu_set_from_playhead ()
{
	ArdourMarker* marker;

	if ((marker = reinterpret_cast<ArdourMarker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	Location* l;
	bool is_start;
	const int32_t divisions = get_grid_music_divisions (0);

	if ((l = find_location_from_marker (marker, is_start)) != 0) {

		if (l->is_mark()) {
			l->set_start (_session->audible_frame (), false, true, divisions);
		}
		else {
			if (is_start) {
				l->set_start (_session->audible_frame (), false, true, divisions);
			} else {
				l->set_end (_session->audible_frame (), false, true, divisions);
			}
		}
	}
}

void
Editor::marker_menu_set_from_selection (bool /*force_regions*/)
{
	ArdourMarker* marker;

	if ((marker = reinterpret_cast<ArdourMarker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	Location* l;
	bool is_start;

	if ((l = find_location_from_marker (marker, is_start)) != 0) {

		if (l->is_mark()) {

			// nothing for now

		} else {

			if (!selection->time.empty()) {
				l->set (selection->time.start(), selection->time.end_frame());
			} else if (!selection->regions.empty()) {
				l->set (selection->regions.start(), selection->regions.end_frame());
			}
		}
	}
}


void
Editor::marker_menu_play_range ()
{
	ArdourMarker* marker;

	if ((marker = reinterpret_cast<ArdourMarker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	Location* l;
	bool is_start;

	if ((l = find_location_from_marker (marker, is_start)) != 0) {

		if (l->is_mark()) {
			_session->request_locate (l->start(), true);
		}
		else {
			_session->request_bounded_roll (l->start(), l->end());

		}
	}
}

void
Editor::marker_menu_loop_range ()
{
	ArdourMarker* marker;

	if ((marker = reinterpret_cast<ArdourMarker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	Location* l;
	bool is_start;

	if ((l = find_location_from_marker (marker, is_start)) != 0) {
		Location* l2;
		if ((l2 = transport_loop_location()) != 0) {
			l2->set (l->start(), l->end());

			// enable looping, reposition and start rolling
			_session->request_locate (l2->start(), true);
			_session->request_play_loop(true);
		}
	}
}

/** Temporal zoom to the range of the marker_menu_item (plus 5% either side) */
void
Editor::marker_menu_zoom_to_range ()
{
	ArdourMarker* marker = reinterpret_cast<ArdourMarker *> (marker_menu_item->get_data ("marker"));
	assert (marker);

	bool is_start;
	Location* l = find_location_from_marker (marker, is_start);
	if (l == 0) {
		return;
	}

	framecnt_t const extra = l->length() * 0.05;
	framepos_t a = l->start ();
	if (a >= extra) {
		a -= extra;
	}

	framepos_t b = l->end ();
	if (b < (max_framepos - extra)) {
		b += extra;
	}

	temporal_zoom_by_frame (a, b);
}

void
Editor::dynamic_cast_marker_object (void* p, MeterMarker** m, TempoMarker** t) const
{
	ArdourMarker* marker = reinterpret_cast<ArdourMarker*> (p);
	if (!marker) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	*m = dynamic_cast<MeterMarker*> (marker);
	*t = dynamic_cast<TempoMarker*> (marker);
}

void
Editor::marker_menu_edit ()
{
	MeterMarker* mm;
	TempoMarker* tm;
	dynamic_cast_marker_object (marker_menu_item->get_data ("marker"), &mm, &tm);

	if (mm) {
		edit_meter_section (&mm->meter());
	} else if (tm) {
		edit_tempo_section (&tm->tempo());
	}
}

void
Editor::marker_menu_remove ()
{
	MeterMarker* mm;
	TempoMarker* tm;
	dynamic_cast_marker_object (marker_menu_item->get_data ("marker"), &mm, &tm);

	if (mm) {
		remove_meter_marker (marker_menu_item);
	} else if (tm) {
		remove_tempo_marker (marker_menu_item);
	} else {
		remove_marker (*marker_menu_item, (GdkEvent*) 0);
	}
}

void
Editor::toggle_marker_lock_style ()
{
	MeterMarker* mm;
	TempoMarker* tm;
	dynamic_cast_marker_object (marker_menu_item->get_data ("marker"), &mm, &tm);

	if (mm) {
		begin_reversible_command (_("change meter lock style"));
		XMLNode &before = _session->tempo_map().get_state();
		MeterSection* msp = &mm->meter();

		const Meter meter (msp->divisions_per_bar(), msp->note_divisor());
		const Timecode::BBT_Time bbt (msp->bbt());
		const PositionLockStyle pls = (msp->position_lock_style() == AudioTime) ? MusicTime : AudioTime;

		_session->tempo_map().replace_meter (*msp, meter, bbt, msp->frame(), pls);

		XMLNode &after = _session->tempo_map().get_state();
		_session->add_command(new MementoCommand<TempoMap>(_session->tempo_map(), &before, &after));
		commit_reversible_command ();
	} else if (tm) {
		TempoSection* tsp = &tm->tempo();

		const Tempo tempo (tsp->note_types_per_minute(), tsp->note_type());
		const double pulse = tsp->pulse();
		const framepos_t frame = tsp->frame();
		const TempoSection::Type type = tsp->type();
		const PositionLockStyle pls = (tsp->position_lock_style() == AudioTime) ? MusicTime : AudioTime;

		begin_reversible_command (_("change tempo lock style"));
		XMLNode &before = _session->tempo_map().get_state();

		_session->tempo_map().replace_tempo (*tsp, tempo, pulse, frame, type, pls);

		XMLNode &after = _session->tempo_map().get_state();
		_session->add_command(new MementoCommand<TempoMap>(_session->tempo_map(), &before, &after));
		commit_reversible_command ();
	}
}

void
Editor::toggle_tempo_type ()
{
	TempoMarker* tm;
	MeterMarker* mm;
	dynamic_cast_marker_object (marker_menu_item->get_data ("marker"), &mm, &tm);

	if (tm) {
		TempoSection* tsp = &tm->tempo();

		const Tempo tempo (tsp->note_types_per_minute(), tsp->note_type());
		const double pulse = tsp->pulse();
		const framepos_t frame = tsp->frame();
		const TempoSection::Type type = (tsp->type() == TempoSection::Ramp) ? TempoSection::Constant : TempoSection::Ramp;
		const PositionLockStyle pls = tsp->position_lock_style();

		begin_reversible_command (_("change tempo type"));
		XMLNode &before = _session->tempo_map().get_state();

		_session->tempo_map().replace_tempo (*tsp, tempo, pulse, frame, type, pls);

		XMLNode &after = _session->tempo_map().get_state();
		_session->add_command(new MementoCommand<TempoMap>(_session->tempo_map(), &before, &after));
		commit_reversible_command ();
	}
}

void
Editor::toggle_marker_menu_lock ()
{
	ArdourMarker* marker;

	if ((marker = reinterpret_cast<ArdourMarker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	Location* loc;
	bool ignored;

	loc = find_location_from_marker (marker, ignored);

	if (!loc) {
		return;
	}

	if (loc->locked()) {
		loc->unlock ();
	} else {
		loc->lock ();
	}
}

void
Editor::marker_menu_rename ()
{
	ArdourMarker* marker;

	if ((marker = reinterpret_cast<ArdourMarker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}


	rename_marker (marker);
}

void
Editor::rename_marker(ArdourMarker *marker)
{
	Location* loc;
	bool is_start;

	loc = find_location_from_marker (marker, is_start);

	if (!loc)
	       return;

	if (loc == transport_loop_location() || loc == transport_punch_location() || loc->is_session_range())
		return;

	ArdourPrompter dialog (true);
	string txt;

	dialog.set_prompt (_("New Name:"));

	if (loc->is_mark()) {
		dialog.set_title (_("Rename Mark"));
	} else {
		dialog.set_title (_("Rename Range"));
	}

	dialog.set_name ("MarkRenameWindow");
	dialog.set_size_request (250, -1);
	dialog.set_position (Gtk::WIN_POS_MOUSE);

	dialog.add_button (_("Rename"), RESPONSE_ACCEPT);
	dialog.set_response_sensitive (Gtk::RESPONSE_ACCEPT, false);
	dialog.set_initial_text (loc->name());

	dialog.show ();

	switch (dialog.run ()) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
	}

	begin_reversible_command ( _("rename marker") );
	XMLNode &before = _session->locations()->get_state();

	dialog.get_result(txt);
	loc->set_name (txt);
	_session->set_dirty ();

	XMLNode &after = _session->locations()->get_state();
	_session->add_command (new MementoCommand<Locations>(*(_session->locations()), &before, &after));
	commit_reversible_command ();
}

void
Editor::new_transport_marker_menu_popdown ()
{
	// hide rects
	transport_bar_drag_rect->hide();

	_drags->abort ();
}

void
Editor::new_transport_marker_menu_set_loop ()
{
	set_loop_range (temp_location->start(), temp_location->end(), _("set loop range"));
}

void
Editor::new_transport_marker_menu_set_punch ()
{
	set_punch_range (temp_location->start(), temp_location->end(), _("set punch range"));
}

void
Editor::update_loop_range_view ()
{
	if (_session == 0) {
		return;
	}

	Location* tll;

	if (_session->get_play_loop() && ((tll = transport_loop_location()) != 0)) {

		double x1 = sample_to_pixel (tll->start());
		double x2 = sample_to_pixel (tll->end());

		transport_loop_range_rect->set_x0 (x1);
		transport_loop_range_rect->set_x1 (x2);

		transport_loop_range_rect->show();

	} else {
		transport_loop_range_rect->hide();
	}
}

void
Editor::update_punch_range_view ()
{
	if (_session == 0) {
		return;
	}

	Location* tpl;

	if ((_session->config.get_punch_in() || _session->config.get_punch_out()) && ((tpl = transport_punch_location()) != 0)) {

		double pixel_start;
		double pixel_end;

		if (_session->config.get_punch_in()) {
			pixel_start = sample_to_pixel (tpl->start());
		} else {
			pixel_start = 0;
		}
		if (_session->config.get_punch_out()) {
			pixel_end = sample_to_pixel (tpl->end());
		} else {
			pixel_end = sample_to_pixel (max_framepos);
		}

		transport_punch_range_rect->set_x0 (pixel_start);
		transport_punch_range_rect->set_x1 (pixel_end);
		transport_punch_range_rect->show();

	} else {

	        transport_punch_range_rect->hide();
	}
}

void
Editor::marker_selection_changed ()
{
	if (_session && _session->deletion_in_progress()) {
		return;
	}

	for (LocationMarkerMap::iterator i = location_markers.begin(); i != location_markers.end(); ++i) {
		i->second->set_selected (false);
	}

	for (MarkerSelection::iterator x = selection->markers.begin(); x != selection->markers.end(); ++x) {
		(*x)->set_selected (true);
	}
}

struct SortLocationsByPosition {
    bool operator() (Location* a, Location* b) {
	    return a->start() < b->start();
    }
};

void
Editor::goto_nth_marker (int n)
{
	if (!_session) {
		return;
	}
	const Locations::LocationList& l (_session->locations()->list());
	Locations::LocationList ordered;
	ordered = l;

	SortLocationsByPosition cmp;
	ordered.sort (cmp);

	for (Locations::LocationList::iterator i = ordered.begin(); n >= 0 && i != ordered.end(); ++i) {
		if ((*i)->is_mark() && !(*i)->is_hidden() && !(*i)->is_session_range()) {
			if (n == 0) {
				_session->request_locate ((*i)->start(), _session->transport_rolling());
				break;
			}
			--n;
		}
	}
}

void
Editor::toggle_marker_menu_glue ()
{
	ArdourMarker* marker;

	if ((marker = reinterpret_cast<ArdourMarker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	Location* loc;
	bool ignored;

	loc = find_location_from_marker (marker, ignored);

	if (!loc) {
		return;
	}

	begin_reversible_command (_("change marker lock style"));
	XMLNode &before = _session->locations()->get_state();

	if (loc->position_lock_style() == MusicTime) {
		loc->set_position_lock_style (AudioTime);
	} else {
		loc->set_position_lock_style (MusicTime);
	}

	XMLNode &after = _session->locations()->get_state();
	_session->add_command(new MementoCommand<Locations>(*(_session->locations()), &before, &after));
	commit_reversible_command ();
}

void
Editor::toggle_marker_lines ()
{
	_show_marker_lines = !_show_marker_lines;

	for (LocationMarkerMap::iterator i = location_markers.begin(); i != location_markers.end(); ++i) {
		i->second->set_show_lines (_show_marker_lines);
	}
}

void
Editor::remove_sorted_marker (ArdourMarker* m)
{
	for (std::map<ArdourCanvas::Container *, std::list<ArdourMarker *> >::iterator i = _sorted_marker_lists.begin(); i != _sorted_marker_lists.end(); ++i) {
		i->second.remove (m);
	}
}

ArdourMarker *
Editor::find_marker_from_location_id (PBD::ID const & id, bool is_start) const
{
	for (LocationMarkerMap::const_iterator i = location_markers.begin(); i != location_markers.end(); ++i) {
		if (i->first->id() == id) {
			return is_start ? i->second->start : i->second->end;
		}
	}

	return 0;
}
