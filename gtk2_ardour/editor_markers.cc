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

#include "pbd/memento_command.h"

#include "ardour/session.h"
#include "ardour/location.h"
#include "ardour/midi_port.h"
#include "ardour/profile.h"
#include "ardour/engine_state_controller.h"
#include "ardour/audioengine.h"

#include "canvas/canvas.h"
#include "canvas/item.h"
#include "canvas/rectangle.h"
#include "canvas/utils.h"

#include "ardour_ui.h"
#include "editor.h"
#include "marker.h"
#include "selection.h"
#include "editing.h"
#include "gui_thread.h"
#include "actions.h"
#include "prompter.h"
#include "editor_drag.h"
#include "floating_text_entry.h"

#include "i18n.h"

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

	(void) add_new_location_internal (location);

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

	/* make a note here of which group this marker ends up in */
	ArdourCanvas::Container* group = 0;

	if (location->is_mark()) {

		if (location->is_cd_marker() && ruler_cd_marker_action->get_active()) {
			lam->start = new Marker (location, *this, *cd_marker_group, marker_height, 0, location->name(), Marker::Mark, location->start());
			group = cd_marker_group;
		} else {
			lam->start = new Marker (location, *this, *marker_group, marker_height, 0, location->name(), Marker::Mark, location->start());
			group = marker_group;
		}

		lam->end = 0;

	} else if (location->is_auto_loop()) {

		// no name shown ; actual marker is some portion of the height of the bar on which we drag,
                // so that it only appears in the upper part of the bar (ruler).
		lam->start = new RulerMarker (location, *this, *ruler_group, ruler_height, 
                                              ARDOUR_UI::instance()->config()->get_canvasvar_LoopRangeMarkerInactive(),
                                              "", location->start(), location->end());
                lam->end = 0;
		group = ruler_group;

	} else if (location->is_auto_punch()) {

		// transport marker
		lam->start = new Marker (location, *this, *transport_marker_group, marker_height, 0,
					 location->name(), Marker::PunchIn, location->start());
		lam->end   = new Marker (location, *this, *transport_marker_group, marker_height, 0,
					 location->name(), Marker::PunchOut, location->end());
		group = transport_marker_group;

	} else if (location->is_session_range()) {

                /* Tracks does not display this range */
                lam->start = 0;
                lam->end = 0;
                group = 0;

        } else if (location->is_skip ()) {
                /* skip: single marker that spans entire skip area */
                lam->start = new RangeMarker (location, *this, *skip_group, skipbar_height, 0, location->name(), location->start(), location->end());
                lam->end = 0;
                group = skip_group;

	} else {
		// range marker
		if (location->is_cd_marker() && ruler_cd_marker_action->get_active()) {
			lam->start = new Marker (location, *this, *cd_marker_group, marker_height, 0,
						 location->name(), Marker::RangeStart, location->start());
			lam->end   = new Marker (location, *this, *cd_marker_group, marker_height, 0,
						 location->name(), Marker::RangeEnd, location->end());
			group = cd_marker_group;
		} else {
			lam->start = new Marker (location, *this, *range_marker_group, marker_height, 0,
						 location->name(), Marker::RangeStart, location->start());
			lam->end   = new Marker (location, *this, *range_marker_group, marker_height, 0,
						 location->name(), Marker::RangeEnd, location->end());
			group = range_marker_group;
		}
	}

        if (lam->start || lam->end) {

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
                
                /* Add these markers to the appropriate sorted marker lists, which will render
                   them unsorted until a call to update_marker_labels() sorts them out.
                */
                _sorted_marker_lists[group].push_back (lam->start);

                if (lam->end) {
                        _sorted_marker_lists[group].push_back (lam->end);
                }
        } else {
                delete lam;
        }

	return group;
}

void
Editor::location_changed (Location *location)
{
	if (location->is_auto_loop()) {
		update_loop_range_view ();
	} else if (location->is_auto_punch()) {
		update_punch_range_view ();
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
Editor::find_location_from_marker (Marker *marker, bool& is_start) const
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
Editor::LocationMarkers::set_selected (bool s)
{
	start->set_selected (s);
	if (end) {
		end->set_selected (s);
	}
}

void
Editor::mouse_add_new_marker (framepos_t where, bool is_cd, bool is_xrun)
{
	string markername, markerprefix;
	int flags = (is_cd ? Location::IsCDMarker|Location::IsMark : Location::IsMark);

	if (is_xrun) {
		markerprefix = "xrun";
		flags = Location::IsMark;
	} else {
		markerprefix = _(Marker::default_new_marker_prefix);
	}

	if (_session) {
		_session->locations()->next_available_name(markername, markerprefix);
		if (!is_xrun && !choose_new_marker_name(markername)) {
			return;
		}
		Location *location = new Location (*_session, where, where, markername, (Location::Flags) flags);
		_session->begin_reversible_command (_("add marker"));
		XMLNode &before = _session->locations()->get_state();
		_session->locations()->add (location, true);
		XMLNode &after = _session->locations()->get_state();
		_session->add_command (new MementoCommand<Locations>(*(_session->locations()), &before, &after));
		_session->commit_reversible_command ();

		/* find the marker we just added */

		LocationMarkers *lam = find_location_markers (location);
		if (lam) {
			/* make it the selected marker */
			selection->set (lam->start);
		}
	}
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
	Marker* marker;
	bool is_start;

	if ((marker = static_cast<Marker*> (item.get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
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
	_session->begin_reversible_command (_("remove marker"));
	XMLNode &before = _session->locations()->get_state();
	_session->locations()->remove (loc);
	XMLNode &after = _session->locations()->get_state();
	_session->add_command (new MementoCommand<Locations>(*(_session->locations()), &before, &after));
	_session->commit_reversible_command ();
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
		can_remove = mm->meter().movable ();
	} else if (tm) {
		can_remove = tm->tempo().movable ();
	} else {
		return;
	}

	delete tempo_or_meter_marker_menu;
	build_tempo_or_meter_marker_menu (can_remove);
	tempo_or_meter_marker_menu->popup (1, ev->time);
}

void
Editor::marker_context_menu (GdkEventButton* ev, ArdourCanvas::Item* item)
{
	Marker * marker;
	if ((marker = reinterpret_cast<Marker *> (item->get_data("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	bool is_start;
	Location * loc = find_location_from_marker (marker, is_start);

	if (loc == transport_loop_location() || loc == transport_punch_location() || loc->is_session_range ()) {

		if (transport_marker_menu == 0) {
			build_range_marker_menu (loc == transport_loop_location() || loc == transport_punch_location(), loc->is_session_range());
		}

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
		if (range_marker_menu == 0) {
			build_range_marker_menu (false, false);
		}
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

	items.push_back (MenuElem (_("Move Playhead to Marker"), sigc::mem_fun(*this, &Editor::marker_menu_set_playhead)));
	items.push_back (MenuElem (_("Move Marker to Playhead"), sigc::mem_fun(*this, &Editor::marker_menu_set_from_playhead)));
    items.push_back (MenuElem (_("Create Range to Next Marker"), sigc::mem_fun(*this, &Editor::marker_menu_range_to_next)));
	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Rename"), sigc::mem_fun(*this, &Editor::marker_menu_rename)));
	items.push_back (CheckMenuElem (_("Lock")));
	Gtk::CheckMenuItem* lock_item = static_cast<Gtk::CheckMenuItem*> (&items.back());
	if (loc->locked ()) {
		lock_item->set_active ();
	}
	lock_item->signal_activate().connect (sigc::mem_fun (*this, &Editor::toggle_marker_menu_lock));
    items.push_back (MenuElem (_("Delete"), sigc::mem_fun(*this, &Editor::marker_menu_remove)));
	items.push_back (SeparatorElem());
    
    items.push_back (MenuElem (_("Show Marker Inspector"), sigc::mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::show_marker_inspector)));
}

void
Editor::build_range_marker_menu (bool loop_or_punch, bool session)
{
	using namespace Menu_Helpers;

	bool const loop_or_punch_or_session = loop_or_punch | session;

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
	if (!Profile->get_sae()) {
		items.push_back (MenuElem (_("Set Range from Selection"), sigc::bind (sigc::mem_fun(*this, &Editor::marker_menu_set_from_selection), false)));
	}

	items.push_back (MenuElem (_("Zoom to Range"), sigc::mem_fun (*this, &Editor::marker_menu_zoom_to_range)));

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
	if (!Profile->get_sae()) {
		items.push_back (MenuElem (_("Select Range"), sigc::mem_fun(*this, &Editor::marker_menu_select_using_range)));
	}
}

void
Editor::build_tempo_or_meter_marker_menu (bool can_remove)
{
	using namespace Menu_Helpers;

	tempo_or_meter_marker_menu = new Menu;
	MenuList& items = tempo_or_meter_marker_menu->items();
	tempo_or_meter_marker_menu->set_name ("ArdourContextMenu");

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
	Marker* marker;

	if ((marker = reinterpret_cast<Marker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
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
	Marker* marker;

	if ((marker = reinterpret_cast<Marker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
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
	Marker* marker;

	if ((marker = reinterpret_cast<Marker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
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
	Marker* marker;

	if ((marker = reinterpret_cast<Marker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
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
	Marker* marker;

	if ((marker = reinterpret_cast<Marker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
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
	Marker* marker;

	if ((marker = reinterpret_cast<Marker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
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
	Marker* marker;
	if (!_session) {
		return;
	}

	if ((marker = reinterpret_cast<Marker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
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
	Marker* marker;

	if ((marker = reinterpret_cast<Marker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	Location* l;
	bool is_start;

	if ((l = find_location_from_marker (marker, is_start)) != 0) {

		if (l->is_mark()) {
			l->set_start (_session->audible_frame ());
		}
		else {
			if (is_start) {
				l->set_start (_session->audible_frame ());
			} else {
				l->set_end (_session->audible_frame ());
			}
		}
	}
}

void
Editor::marker_menu_set_from_selection (bool /*force_regions*/)
{
	Marker* marker;

	if ((marker = reinterpret_cast<Marker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
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
	Marker* marker;

	if ((marker = reinterpret_cast<Marker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
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
	Marker* marker;

	if ((marker = reinterpret_cast<Marker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
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
	Marker* marker = reinterpret_cast<Marker *> (marker_menu_item->get_data ("marker"));
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
	Marker* marker = reinterpret_cast<Marker*> (p);
	if (!marker) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
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
Editor::toggle_marker_menu_lock ()
{
	Marker* marker;

	if ((marker = reinterpret_cast<Marker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
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
	Marker* marker;

	if ((marker = reinterpret_cast<Marker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}


	rename_marker (marker);
}

void
Editor::rename_marker(Marker *marker)
{
	Location* loc;
	bool is_start;

	loc = find_location_from_marker (marker, is_start);

	if (!loc)
	       return;

	if (loc == transport_loop_location() || loc == transport_punch_location() || loc->is_session_range()) {
		return;
        }

        FloatingTextEntry* flt = new FloatingTextEntry (loc->name());

        flt->use_text.connect (sigc::bind (sigc::mem_fun (*this, &Editor::finish_rename_marker), marker));
        flt->present ();
}

void
Editor::finish_rename_marker (std::string txt, Marker* marker)
{
	Location* loc;
	bool ignored;

	loc = find_location_from_marker (marker, ignored);

	if (!loc || txt.empty()) {
		return;
	}

	begin_reversible_command ( _("rename marker") );
	XMLNode &before = _session->locations()->get_state();


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

	Location* tll = transport_loop_location();
        LocationMarkers* lam = find_location_markers (tll);

	if (_session->get_play_loop() && tll) {

		double x1 = sample_to_pixel (tll->start());
		double x2 = sample_to_pixel (tll->end());

		transport_loop_range_rect->set_x0 (x1);
		transport_loop_range_rect->set_x1 (x2);
                
		transport_loop_range_rect->show();

                if (lam) {
                        lam->start->set_color (ARDOUR_UI::instance()->config()->get_canvasvar_LoopRangeMarkerActive());
                }

	} else {
		transport_loop_range_rect->hide();

                if (lam) {
                        lam->start->set_color (ARDOUR_UI::instance()->config()->get_canvasvar_LoopRangeMarkerInactive());
                }
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
	Marker* marker;

	if ((marker = reinterpret_cast<Marker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	Location* loc;
	bool ignored;

	loc = find_location_from_marker (marker, ignored);

	if (!loc) {
		return;
	}

	if (loc->position_lock_style() == MusicTime) {
		loc->set_position_lock_style (AudioTime);
	} else {
		loc->set_position_lock_style (MusicTime);
	}

}

void
Editor::toggle_marker_lines ()
{
	_show_marker_lines = !_show_marker_lines;

#if 0
	for (LocationMarkerMap::iterator i = location_markers.begin(); i != location_markers.end(); ++i) {
		i->second->set_show_lines (_show_marker_lines);
	}
#endif

}

void
Editor::remove_sorted_marker (Marker* m)
{
	for (std::map<ArdourCanvas::Container *, std::list<Marker *> >::iterator i = _sorted_marker_lists.begin(); i != _sorted_marker_lists.end(); ++i) {
		i->second.remove (m);
	}
}

Marker *
Editor::find_marker_from_location_id (PBD::ID const & id, bool is_start) const
{
	for (LocationMarkerMap::const_iterator i = location_markers.begin(); i != location_markers.end(); ++i) {
		if (i->first->id() == id) {
			return is_start ? i->second->start : i->second->end;
		}
	}

	return 0;
}

void
Editor::marker_midi_input_activity ()
{
        if (!midi_marker_input_activity_image.is_visible () &&
             midi_marker_input_enabled_image.is_visible () ) {
                midi_marker_input_disabled_image.hide ();
                midi_marker_input_enabled_image.hide ();
                midi_marker_input_activity_image.show ();
                /* hide the image again in 1/2 second */
                Glib::signal_timeout().connect (sigc::bind (sigc::mem_fun (*this, &Editor::reset_marker_midi_images), true), 500);
        }
}

void
Editor::marker_midi_output_activity ()
{
        if (!midi_marker_output_activity_image.is_visible () &&
             midi_marker_output_enabled_image.is_visible () ) {
                midi_marker_output_disabled_image.hide ();
                midi_marker_output_enabled_image.hide ();
                midi_marker_output_activity_image.show ();
                /* hide the image again in 1/2 second */
                Glib::signal_timeout().connect (sigc::bind (sigc::mem_fun (*this, &Editor::reset_marker_midi_images), false), 500);
        }
}

bool
Editor::reset_marker_midi_images (bool input)
{
        if (!_session) {
                return false;
        }

        if (input) {
                if (_session->scene_in()->connected()) {
                        midi_marker_input_enabled_image.show ();
                        midi_marker_input_disabled_image.hide ();
                } else {
                        midi_marker_input_enabled_image.hide ();
                        midi_marker_input_disabled_image.show ();
                }
                midi_marker_input_activity_image.hide ();
        } else {
                if (_session->scene_out()->connected()) {
                        midi_marker_output_enabled_image.show ();
                        midi_marker_output_disabled_image.hide ();
                } else {
                        midi_marker_output_enabled_image.hide ();
                        midi_marker_output_disabled_image.show ();
                }
                midi_marker_output_activity_image.hide ();
        }

        return false; /* do not call again */
}

/* MIDI/scene change Markers */

void
Editor::midi_input_chosen (WavesDropdown* dropdown, int el_number)
{
    char* full_name_of_chosen_port = (char*)dropdown->get_item_associated_data(el_number);
    
    if (full_name_of_chosen_port) {
        Gtk::MenuItem* item = dropdown->get_item(el_number);
        Gtk::CheckMenuItem* check_item  = dynamic_cast<Gtk::CheckMenuItem*>(item);
        
        if (check_item) {
            bool active = check_item->get_active();
            EngineStateController::instance()->set_physical_midi_scene_in_connection_state((char*) full_name_of_chosen_port, active);
        }
    }
}

void
Editor::midi_output_chosen (WavesDropdown* dropdown, int el_number)
{
    char* full_name_of_chosen_port = (char*)dropdown->get_item_associated_data(el_number);
    
    if (full_name_of_chosen_port) {
        Gtk::MenuItem* item = dropdown->get_item(el_number);
        Gtk::CheckMenuItem* check_item  = dynamic_cast<Gtk::CheckMenuItem*>(item);
        
        if (check_item) {
            bool active = check_item->get_active();
            EngineStateController::instance()->set_physical_midi_scenen_out_connection_state((char*) full_name_of_chosen_port, active);
        }
    }
}

void
Editor::populate_midi_inout_dropdowns  ()
{
	populate_midi_inout_dropdown (false);
	populate_midi_inout_dropdown (true);
}

void
Editor::populate_midi_inout_dropdown  (bool playback)
{
    using namespace ARDOUR;
        
	WavesDropdown* dropdown = playback ? &_midi_output_dropdown : &_midi_input_dropdown;
        
	std::vector<EngineStateController::MidiPortState> midi_states;
	static const char* midi_port_name_prefix = "system_midi:";
	const char* midi_type_suffix;
	bool have_first = false;

	if (playback) {
		EngineStateController::instance()->get_physical_midi_output_states(midi_states);
		midi_type_suffix = X_(" playback");
	} else {
		EngineStateController::instance()->get_physical_midi_input_states(midi_states);
		midi_type_suffix = X_(" capture");
	}

	dropdown->clear_items ();

	std::vector<EngineStateController::MidiPortState>::const_iterator state_iter;

	for (state_iter = midi_states.begin(); state_iter != midi_states.end(); ++state_iter) {

		// strip the device name from input port name

		std::string device_name;

		ARDOUR::remove_pattern_from_string(state_iter->name, midi_port_name_prefix, device_name);
		ARDOUR::remove_pattern_from_string(device_name, midi_type_suffix, device_name);

		if (state_iter->active) {
            
            Gtk::CheckMenuItem& new_item = dropdown->add_check_menu_item (device_name, strdup (state_iter->name.c_str()));
            new_item.set_active(state_iter->scene_connected);
            
            
            if (!have_first) {
				dropdown->set_text (device_name);
				have_first = true;
			}
		}
	}
}        
