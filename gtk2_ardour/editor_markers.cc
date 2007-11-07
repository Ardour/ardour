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
#include <gtkmm2ext/window_title.h>

#include <ardour/location.h>
#include <pbd/memento_command.h>

#include "editor.h"
#include "marker.h"
#include "selection.h"
#include "editing.h"
#include "gui_thread.h"
#include "simplerect.h"
#include "actions.h"
#include "prompter.h"

#include "i18n.h"

using namespace std;
using namespace sigc;
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
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &Editor::add_new_location), location));

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
		lam->start = new Marker (*this, *marker_group, color, location->name(), Marker::Mark, location->start());
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
		lam->start = new Marker (*this, *range_marker_group, color, 
					 location->name(), Marker::Start, location->start());
		lam->end   = new Marker (*this, *range_marker_group, color, 
					 location->name(), Marker::End, location->end());
	}

	if (location->is_hidden ()) {
		lam->hide();
	} else {
		lam->show ();
	}

	location->start_changed.connect (mem_fun(*this, &Editor::location_changed));
	location->end_changed.connect (mem_fun(*this, &Editor::location_changed));
	location->changed.connect (mem_fun(*this, &Editor::location_changed));
	location->name_changed.connect (mem_fun(*this, &Editor::location_changed));
	location->FlagsChanged.connect (mem_fun(*this, &Editor::location_flags_changed));

	pair<Location*,LocationMarkers*> newpair;

	newpair.first = location;
	newpair.second = lam;

	location_markers.insert (newpair);
}

void
Editor::location_changed (Location *location)
{
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &Editor::location_changed), location));

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
Editor::location_flags_changed (Location *location, void *src)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::location_flags_changed), location, src));
	
	LocationMarkers *lam = find_location_markers (location);
	
	if (lam == 0) {
		/* a location that isn't "marked" with markers */
		return;
	}

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

Editor::LocationMarkers::~LocationMarkers ()
{
	if (start) {
		delete start;
	}

	if (end) {
		delete end;
	}
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
	clear_marker_display ();
	
	for (Locations::LocationList::iterator i = locations.begin(); i != locations.end(); ++i) {
		add_new_location (*i);
	}
}

void
Editor::refresh_location_display ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &Editor::refresh_location_display));
	
	if (session) {
		session->locations()->apply (*this, &Editor::refresh_location_display_internal);
	}
}

void
Editor::refresh_location_display_s (Change ignored)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::refresh_location_display_s), ignored));

	if (session) {
		session->locations()->apply (*this, &Editor::refresh_location_display_internal);
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
Editor::LocationMarkers::set_position (nframes_t startf, 
				       nframes_t endf) 
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
Editor::mouse_add_new_marker (nframes_t where)
{
	string markername;
	if (session) {
		session->locations()->next_available_name(markername,"mark");
		Location *location = new Location (where, where, markername, Location::IsMark);
		session->begin_reversible_command (_("add marker"));
                XMLNode &before = session->locations()->get_state();
		session->locations()->add (location, true);
                XMLNode &after = session->locations()->get_state();
		session->add_command (new MementoCommand<Locations>(*(session->locations()), &before, &after));
		session->commit_reversible_command ();
	}
}

void
Editor::remove_marker (ArdourCanvas::Item& item, GdkEvent* event)
{
	Marker* marker;
	bool is_start;

	if ((marker = static_cast<Marker*> (item.get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	Location* loc = find_location_from_marker (marker, is_start);

	if (session && loc) {
	  	Glib::signal_idle().connect (bind (mem_fun(*this, &Editor::really_remove_marker), loc));
	}
}

gint
Editor::really_remove_marker (Location* loc)
{
	session->begin_reversible_command (_("remove marker"));
	XMLNode &before = session->locations()->get_state();
	session->locations()->remove (loc);
	XMLNode &after = session->locations()->get_state();
	session->add_command (new MementoCommand<Locations>(*(session->locations()), &before, &after));
	session->commit_reversible_command ();
	return FALSE;
}

void
Editor::location_gone (Location *location)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::location_gone), location));
	
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
Editor::new_transport_marker_context_menu (GdkEventButton* ev, ArdourCanvas::Item* item)
{
	if (new_transport_marker_menu == 0) {
		build_new_transport_marker_menu ();
	}

	new_transport_marker_menu->popup (1, ev->time);

}

void
Editor::transport_marker_context_menu (GdkEventButton* ev, ArdourCanvas::Item* item)
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

	items.push_back (MenuElem (_("Locate to Mark"), mem_fun(*this, &Editor::marker_menu_set_playhead)));
	items.push_back (MenuElem (_("Play from Mark"), mem_fun(*this, &Editor::marker_menu_play_from)));
	items.push_back (MenuElem (_("Set Mark from Playhead"), mem_fun(*this, &Editor::marker_menu_set_from_playhead)));

	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Hide Mark"), mem_fun(*this, &Editor::marker_menu_hide)));
	if (start_or_end) return;
	items.push_back (MenuElem (_("Rename Mark"), mem_fun(*this, &Editor::marker_menu_rename)));
	items.push_back (MenuElem (_("Remove Mark"), mem_fun(*this, &Editor::marker_menu_remove)));

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

	items.push_back (MenuElem (_("Locate to Range Mark"), mem_fun(*this, &Editor::marker_menu_set_playhead)));
	items.push_back (MenuElem (_("Play from Range Mark"), mem_fun(*this, &Editor::marker_menu_play_from)));
	if (!loop_or_punch) {
		items.push_back (MenuElem (_("Play Range"), mem_fun(*this, &Editor::marker_menu_play_range)));
		items.push_back (MenuElem (_("Loop Range"), mem_fun(*this, &Editor::marker_menu_loop_range)));
	}
	items.push_back (MenuElem (_("Set Range Mark from Playhead"), mem_fun(*this, &Editor::marker_menu_set_from_playhead)));
	items.push_back (MenuElem (_("Set Range from Range Selection"), mem_fun(*this, &Editor::marker_menu_set_from_selection)));

	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Hide Range"), mem_fun(*this, &Editor::marker_menu_hide)));
	if (!loop_or_punch) {
		items.push_back (MenuElem (_("Rename Range"), mem_fun(*this, &Editor::marker_menu_rename)));
		items.push_back (MenuElem (_("Remove Range"), mem_fun(*this, &Editor::marker_menu_remove)));
	}

	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Separate Regions in Range"), mem_fun(*this, &Editor::marker_menu_separate_regions_using_location)));
	items.push_back (MenuElem (_("Select All in Range"), mem_fun(*this, &Editor::marker_menu_select_all_selectables_using_range)));
	items.push_back (MenuElem (_("Select Range"), mem_fun(*this, &Editor::marker_menu_select_using_range)));

}

void
Editor::build_tm_marker_menu ()
{
	using namespace Menu_Helpers;

	tm_marker_menu = new Menu;
	MenuList& items = tm_marker_menu->items();
	tm_marker_menu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Edit"), mem_fun(*this, &Editor::marker_menu_edit)));
	items.push_back (MenuElem (_("Remove"), mem_fun(*this, &Editor::marker_menu_remove)));
}

void
Editor::build_new_transport_marker_menu ()
{
	using namespace Menu_Helpers;

	new_transport_marker_menu = new Menu;
	MenuList& items = new_transport_marker_menu->items();
	new_transport_marker_menu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Set Loop Range"), mem_fun(*this, &Editor::new_transport_marker_menu_set_loop)));
	items.push_back (MenuElem (_("Set Punch Range"), mem_fun(*this, &Editor::new_transport_marker_menu_set_punch)));

	new_transport_marker_menu->signal_unmap_event().connect ( mem_fun(*this, &Editor::new_transport_marker_menu_popdown)); 
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
			session->request_locate (l->start(), true);
		}
		else {
			//session->request_bounded_roll (l->start(), l->end());
			
			if (is_start) {
				session->request_locate (l->start(), true);
			} else {
				session->request_locate (l->end(), true);
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
			session->request_locate (l->start(), false);
		}
		else {
			if (is_start) {
				session->request_locate (l->start(), false);
			} else {
				session->request_locate (l->end(), false);
			}
		}
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
			l->set_start (session->transport_frame ());
		}
		else {
			if (is_start) {
				l->set_start (session->transport_frame ());
			} else {
				l->set_end (session->transport_frame ());
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
		} else {

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
			session->request_locate (l->start(), true);
		}
		else {
			session->request_bounded_roll (l->start(), l->end());
			
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
			session->request_play_loop(true);
			session->request_locate (l2->start(), true);
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

	WindowTitle title(Glib::get_application_name());
	if (loc->is_mark()) {
		title += _("Rename Mark");
	} else {
		title += _("Rename Range");
	}

	dialog.set_title(title.get_string());

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
        XMLNode &before = session->locations()->get_state();

	dialog.get_result(txt);
	loc->set_name (txt);
	
        XMLNode &after = session->locations()->get_state();
	session->add_command (new MementoCommand<Locations>(*(session->locations()), &before, &after));
	commit_reversible_command ();
}

gint
Editor::new_transport_marker_menu_popdown (GdkEventAny *ev)
{
	// hide rects
	transport_bar_drag_rect->hide();
	range_marker_drag_rect->hide();

	return FALSE;
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
	if (session == 0) {
		return;
	}

	Location* tll;

	if (session->get_play_loop() && ((tll = transport_loop_location()) != 0)) {

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
	if (session == 0) {
		return;
	}

	Location* tpl;

	if ((Config->get_punch_in() || Config->get_punch_out()) && ((tpl = transport_punch_location()) != 0)) {

		double x1 = frame_to_pixel (tpl->start());
		double x2 = frame_to_pixel (tpl->end());
		
		transport_punch_range_rect->property_x1() = x1;
		transport_punch_range_rect->property_x2() = x2;
		
		if (visibility) {
		        transport_punch_range_rect->show();
		}
	}
	else if (visibility) {
	        transport_punch_range_rect->hide();
	}

// 	if (session->get_punch_in()) {
// 		double x = frame_to_pixel (transport_punch_location->start());
// 		gnome_canvas_item_set (transport_punchin_line, "x1", x, "x2", x, NULL);
		
// 		if (visibility) {
// 			gnome_canvas_item_show (transport_punchin_line);
// 		}
// 	}
// 	else if (visibility) {
// 		gnome_canvas_item_hide (transport_punchin_line);
// 	}
	
// 	if (session->get_punch_out()) {
// 		double x = frame_to_pixel (transport_punch_location->end());
		
// 		gnome_canvas_item_set (transport_punchout_line, "x1", x, "x2", x, NULL);
		
// 		if (visibility) {
// 			gnome_canvas_item_show (transport_punchout_line);
// 		}
// 	}
// 	else if (visibility) {
// 		gnome_canvas_item_hide (transport_punchout_line);
// 	}
}
