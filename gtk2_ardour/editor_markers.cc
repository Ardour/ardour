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

#include <sigc++/retype.h>
#include <cstdlib>
#include <cmath>

#include <libgnomecanvas/libgnomecanvas.h>
#include <gtkmm2ext/gtk_ui.h>

#include "ardour/session.h"
#include "ardour/location.h"
#include "ardour/profile.h"
#include "pbd/memento_command.h"

#include "editor.h"
#include "marker.h"
#include "selection.h"
#include "editing.h"
#include "gui_thread.h"
#include "simplerect.h"
#include "actions.h"
#include "prompter.h"
#include "editor_drag.h"

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
}

void
Editor::add_new_location (Location *location)
{
	ENSURE_GUI_THREAD (*this, &Editor::add_new_location, location)

	LocationMarkers *lam = new LocationMarkers;
	uint32_t color;

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
			lam->start = new Marker (*this, *cd_marker_group, color, location->name(), Marker::Mark, location->start());
		}
		else {
			lam->start = new Marker (*this, *marker_group, color, location->name(), Marker::Mark, location->start());
		}
		lam->end   = 0;

	} else if (location->is_auto_loop()) {
		// transport marker
		lam->start = new Marker (*this, *transport_marker_group, color,
					 location->name(), Marker::LoopStart, location->start());
		lam->end   = new Marker (*this, *transport_marker_group, color,
					 location->name(), Marker::LoopEnd, location->end());

	} else if (location->is_auto_punch()) {
		// transport marker
		lam->start = new Marker (*this, *transport_marker_group, color,
					 location->name(), Marker::PunchIn, location->start());
		lam->end   = new Marker (*this, *transport_marker_group, color,
					 location->name(), Marker::PunchOut, location->end());

	} else {
		// range marker
		if (location->is_cd_marker() && ruler_cd_marker_action->get_active()) {
			lam->start = new Marker (*this, *cd_marker_group, color,
						 location->name(), Marker::Start, location->start());
			lam->end   = new Marker (*this, *cd_marker_group, color,
						 location->name(), Marker::End, location->end());
		}
		else {

			lam->start = new Marker (*this, *range_marker_group, color,
						 location->name(), Marker::Start, location->start());
			lam->end   = new Marker (*this, *range_marker_group, color,
						 location->name(), Marker::End, location->end());
		}
	}

	if (location->is_hidden ()) {
		lam->hide();
	} else {
		lam->show ();
	}

	location->start_changed.connect (*this, ui_bind (&Editor::location_changed, this, _1), gui_context());
	location->end_changed.connect (*this, ui_bind (&Editor::location_changed, this, _1), gui_context());
	location->changed.connect (*this, ui_bind (&Editor::location_changed, this, _1), gui_context());
	location->name_changed.connect (*this, ui_bind (&Editor::location_changed, this, _1), gui_context());
	location->FlagsChanged.connect (*this, ui_bind (&Editor::location_flags_changed, this, _1, _2), gui_context());

	pair<Location*,LocationMarkers*> newpair;

	newpair.first = location;
	newpair.second = lam;

	location_markers.insert (newpair);

	if (select_new_marker && location->is_mark()) {
		selection->set (lam->start);
		select_new_marker = false;
	}
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

	lam->set_name (location->name());
	lam->set_position (location->start(), location->end());

	if (location->is_auto_loop()) {
		update_loop_range_view ();
	} else if (location->is_auto_punch()) {
		update_punch_range_view ();
	}
}

void
Editor::location_flags_changed (Location *location, void*)
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
Editor::refresh_location_display_internal (Locations::LocationList& locations)
{
	/* invalidate all */

	for (LocationMarkerMap::iterator i = location_markers.begin(); i != location_markers.end(); ++i) {
		i->second->valid = false;
	}

	/* add new ones */

	for (Locations::LocationList::iterator i = locations.begin(); i != locations.end(); ++i) {

		LocationMarkerMap::iterator x;

		if ((x = location_markers.find (*i)) != location_markers.end()) {
			x->second->valid = true;
			continue;
		}

		add_new_location (*i);
	}

	/* remove dead ones */

	for (LocationMarkerMap::iterator i = location_markers.begin(); i != location_markers.end(); ) {

		LocationMarkerMap::iterator tmp;

		tmp = i;
		++tmp;

		if (!i->second->valid) {
			delete i->second;
			location_markers.erase (i);
		}

		i = tmp;
	}

	update_punch_range_view (false);
	update_loop_range_view (false);
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
Editor::refresh_location_display_s (const PropertyChange&)
{
	ENSURE_GUI_THREAD (*this, &Editor::refresh_location_display_s, ignored)

	if (_session) {
		_session->locations()->apply (*this, &Editor::refresh_location_display_internal);
	}
}

void
Editor::LocationMarkers::hide()
{
	start->hide ();
	if (end) { end->hide(); }
}

void
Editor::LocationMarkers::show()
{
	start->show ();
	if (end) { end->show(); }
}

void
Editor::LocationMarkers::set_name (const string& str)
{
	start->set_name (str);
	if (end) { end->set_name (str); }
}

void
Editor::LocationMarkers::set_position (nframes64_t startf,
				       nframes64_t endf)
{
	start->set_position (startf);
	if (end) { end->set_position (endf); }
}

void
Editor::LocationMarkers::set_color_rgba (uint32_t rgba)
{
	start->set_color_rgba (rgba);
	if (end) { end->set_color_rgba (rgba); }
}

void
Editor::mouse_add_new_marker (nframes64_t where, bool is_cd, bool is_xrun)
{
	string markername, markerprefix;
	int flags = (is_cd ? Location::IsCDMarker|Location::IsMark : Location::IsMark);

	if (is_xrun) {
		markerprefix = "xrun";
		flags = Location::IsMark;
	} else {
		markerprefix = "mark";
	}

	if (_session) {
		_session->locations()->next_available_name(markername, markerprefix);
		if (!is_xrun && !choose_new_marker_name(markername)) {
			return;
		}
		Location *location = new Location (where, where, markername, (Location::Flags) flags);
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
		update_loop_range_view (true);
	}

	if (location == transport_punch_location()) {
		update_punch_range_view (true);
	}

	for (i = location_markers.begin(); i != location_markers.end(); ++i) {
		if ((*i).first == location) {
			delete (*i).second;
			location_markers.erase (i);
			break;
		}
	}
}

void
Editor::tm_marker_context_menu (GdkEventButton* ev, ArdourCanvas::Item* item)
{
	if (tm_marker_menu == 0) {
		build_tm_marker_menu ();
	}

	marker_menu_item = item;
	tm_marker_menu->popup (1, ev->time);

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
	if (loc == transport_loop_location() || loc == transport_punch_location()) {
		if (transport_marker_menu == 0) {
			build_range_marker_menu (true);
		}
		marker_menu_item = item;
		transport_marker_menu->popup (1, ev->time);
	} else {

		if (loc->is_mark()) {
			bool start_or_end = loc->is_start() || loc->is_end();
			Menu *markerMenu;
			if (start_or_end) {
				if (start_end_marker_menu == 0)
			   		build_marker_menu (true);
				markerMenu = start_end_marker_menu;
			} else {
				if (marker_menu == 0)
			   		build_marker_menu (false);
				markerMenu = marker_menu;
			}


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
		markerMenu->popup (1, ev->time);
		}

	        if (loc->is_range_marker()) {
		       if (range_marker_menu == 0){
			      build_range_marker_menu (false);
		       }
		       marker_menu_item = item;
		       range_marker_menu->popup (1, ev->time);
	        }
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
Editor::transport_marker_context_menu (GdkEventButton* ev, ArdourCanvas::Item*)
{
	if (transport_marker_menu == 0) {
		build_range_marker_menu (true);
	}

	transport_marker_menu->popup (1, ev->time);
}

void
Editor::build_marker_menu (bool start_or_end)
{
	using namespace Menu_Helpers;

	Menu *markerMenu = new Menu;
	if (start_or_end) {
		start_end_marker_menu = markerMenu;
	} else {
		marker_menu = markerMenu;
	}
	MenuList& items = markerMenu->items();
	markerMenu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Locate to here"), sigc::mem_fun(*this, &Editor::marker_menu_set_playhead)));
	items.push_back (MenuElem (_("Play from here"), sigc::mem_fun(*this, &Editor::marker_menu_play_from)));
	items.push_back (MenuElem (_("Move Mark to Playhead"), sigc::mem_fun(*this, &Editor::marker_menu_set_from_playhead)));

	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Create range to next marker"), sigc::mem_fun(*this, &Editor::marker_menu_range_to_next)));

	items.push_back (MenuElem (_("Hide"), sigc::mem_fun(*this, &Editor::marker_menu_hide)));
	if (start_or_end) return;
	items.push_back (MenuElem (_("Rename"), sigc::mem_fun(*this, &Editor::marker_menu_rename)));
	items.push_back (MenuElem (_("Lock"), sigc::bind (sigc::mem_fun(*this, &Editor::marker_menu_lock), true)));
	items.push_back (MenuElem (_("Unlock"), sigc::bind (sigc::mem_fun(*this, &Editor::marker_menu_lock), false)));

	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Remove"), sigc::mem_fun(*this, &Editor::marker_menu_remove)));
}

void
Editor::build_range_marker_menu (bool loop_or_punch)
{
	using namespace Menu_Helpers;

	Menu *markerMenu = new Menu;
	if (loop_or_punch) {
		transport_marker_menu = markerMenu;
	} else {
		range_marker_menu = markerMenu;
	}
	MenuList& items = markerMenu->items();
	markerMenu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Play Range"), sigc::mem_fun(*this, &Editor::marker_menu_play_range)));
	items.push_back (MenuElem (_("Locate to Range Mark"), sigc::mem_fun(*this, &Editor::marker_menu_set_playhead)));
	items.push_back (MenuElem (_("Play from Range Mark"), sigc::mem_fun(*this, &Editor::marker_menu_play_from)));
	if (! loop_or_punch) {
		items.push_back (MenuElem (_("Loop Range"), sigc::mem_fun(*this, &Editor::marker_menu_loop_range)));
	}
	items.push_back (MenuElem (_("Set Range Mark from Playhead"), sigc::mem_fun(*this, &Editor::marker_menu_set_from_playhead)));
	if (!Profile->get_sae()) {
		items.push_back (MenuElem (_("Set Range from Range Selection"), sigc::mem_fun(*this, &Editor::marker_menu_set_from_selection)));
	}

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Export Range"), sigc::mem_fun(*this, &Editor::export_range)));
	items.push_back (SeparatorElem());

	if (!loop_or_punch) {
		items.push_back (MenuElem (_("Hide Range"), sigc::mem_fun(*this, &Editor::marker_menu_hide)));
		items.push_back (MenuElem (_("Rename Range"), sigc::mem_fun(*this, &Editor::marker_menu_rename)));
		items.push_back (MenuElem (_("Remove Range"), sigc::mem_fun(*this, &Editor::marker_menu_remove)));
		items.push_back (SeparatorElem());
	}

	items.push_back (MenuElem (_("Separate Regions in Range"), sigc::mem_fun(*this, &Editor::marker_menu_separate_regions_using_location)));
	items.push_back (MenuElem (_("Select All in Range"), sigc::mem_fun(*this, &Editor::marker_menu_select_all_selectables_using_range)));
	if (!Profile->get_sae()) {
		items.push_back (MenuElem (_("Select Range"), sigc::mem_fun(*this, &Editor::marker_menu_select_using_range)));
	}
}

void
Editor::build_tm_marker_menu ()
{
	using namespace Menu_Helpers;

	tm_marker_menu = new Menu;
	MenuList& items = tm_marker_menu->items();
	tm_marker_menu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Edit"), sigc::mem_fun(*this, &Editor::marker_menu_edit)));
	items.push_back (MenuElem (_("Remove"), sigc::mem_fun(*this, &Editor::marker_menu_remove)));
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
	        select_all_within (l->start(), l->end() - 1, 0,  DBL_MAX, track_views, Selection::Set);
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

	nframes64_t start;
	nframes64_t end;
	_session->locations()->marks_either_side (marker->position(), start, end);

	if (end != max_frames) {
		string range_name = l->name();
		range_name += "-range";

		Location* newrange = new Location (marker->position(), end, range_name, Location::IsRangeMarker);
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
Editor::marker_menu_set_from_selection ()
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
		}
		else {

			/* if range selection use first to last */

			if (mouse_mode == Editing::MouseRange) {
				if (!selection->time.empty()) {
					l->set_start (selection->time.start());
					l->set_end (selection->time.end_frame());
				}
			}
			else {
				if (!selection->regions.empty()) {
					l->set_start (selection->regions.start());
					l->set_end (selection->regions.end_frame());
				}
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
			_session->request_play_loop(true);
			_session->request_locate (l2->start(), true);
		}
	}
}

void
Editor::marker_menu_edit ()
{
	MeterMarker* mm;
	TempoMarker* tm;
	Marker* marker;

	if ((marker = reinterpret_cast<Marker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	if ((mm = dynamic_cast<MeterMarker*> (marker)) != 0) {
		edit_meter_section (&mm->meter());
	} else if ((tm = dynamic_cast<TempoMarker*> (marker)) != 0) {
		edit_tempo_section (&tm->tempo());
	} else {
		fatal << X_("programming erorr: unhandled marker type in Editor::marker_menu_edit")
		      << endmsg;
		/*NOTREACHED*/
	}
}

void
Editor::marker_menu_remove ()
{
	MeterMarker* mm;
	TempoMarker* tm;
	Marker* marker;

	if ((marker = reinterpret_cast<Marker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	if ((mm = dynamic_cast<MeterMarker*> (marker)) != 0) {
		remove_meter_marker (marker_menu_item);
	} else if ((tm = dynamic_cast<TempoMarker*> (marker)) != 0) {
		remove_tempo_marker (marker_menu_item);
	} else {
		remove_marker (*marker_menu_item, (GdkEvent*) 0);
	}
}

void
Editor::marker_menu_lock (bool yn)
{

	Marker* marker;

	if ((marker = reinterpret_cast<Marker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	Location* loc;
	bool ignored;

	loc = find_location_from_marker (marker, ignored);

	if (!loc) return;

	if (yn) {
		loc->lock();
	} else {
		loc->unlock ();
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

	Location* loc;
	bool is_start;

	loc = find_location_from_marker (marker, is_start);

	if (!loc) return;

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

	XMLNode &after = _session->locations()->get_state();
	_session->add_command (new MementoCommand<Locations>(*(_session->locations()), &before, &after));
	commit_reversible_command ();
}

void
Editor::new_transport_marker_menu_popdown ()
{
	// hide rects
	transport_bar_drag_rect->hide();

	_drags->break_drag ();
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
Editor::update_loop_range_view (bool visibility)
{
	if (_session == 0) {
		return;
	}

	Location* tll;

	if (_session->get_play_loop() && ((tll = transport_loop_location()) != 0)) {

		double x1 = frame_to_pixel (tll->start());
		double x2 = frame_to_pixel (tll->end());

		transport_loop_range_rect->property_x1() = x1;
		transport_loop_range_rect->property_x2() = x2;

		if (visibility) {
			transport_loop_range_rect->show();
		}

	} else if (visibility) {
		transport_loop_range_rect->hide();
	}
}

void
Editor::update_punch_range_view (bool visibility)
{
	if (_session == 0) {
		return;
	}

	Location* tpl;

	if ((_session->config.get_punch_in() || _session->config.get_punch_out()) && ((tpl = transport_punch_location()) != 0)) {
		guint track_canvas_width,track_canvas_height;
		track_canvas->get_size(track_canvas_width,track_canvas_height);
		if (_session->config.get_punch_in()) {
			transport_punch_range_rect->property_x1() = frame_to_pixel (tpl->start());
			transport_punch_range_rect->property_x2() = (_session->config.get_punch_out() ? frame_to_pixel (tpl->end()) : frame_to_pixel (JACK_MAX_FRAMES));
		} else {
			transport_punch_range_rect->property_x1() = 0;
			transport_punch_range_rect->property_x2() = (_session->config.get_punch_out() ? frame_to_pixel (tpl->end()) : track_canvas_width);
		}

		if (visibility) {
		        transport_punch_range_rect->show();
		}
	} else if (visibility) {
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
		LocationMarkers* lam = i->second;

		if (lam->start) {
			lam->start->hide_line();
		}

		if (lam->end) {
			lam->end->hide_line();
		}
	}

	for (MarkerSelection::iterator x = selection->markers.begin(); x != selection->markers.end(); ++x) {
		(*x)->add_line (cursor_group, 0, _canvas_height);
		(*x)->show_line ();
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
		if ((*i)->is_mark() && !(*i)->is_hidden() && !(*i)->is_start()) {
			if (n == 0) {
				_session->request_locate ((*i)->start(), _session->transport_rolling());
				break;
			}
			--n;
		}
	}
}
