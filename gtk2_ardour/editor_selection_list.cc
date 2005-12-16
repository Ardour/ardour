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
#include <vector>

#include <gtkmm.h>

#include <ardour/named_selection.h>
#include <ardour/session_selection.h>
#include <ardour/playlist.h>

#include <gtkmm2ext/stop_signal.h>

#include "editor.h"
#include "selection.h"
#include "time_axis_view.h"
#include "ardour_ui.h"
#include "prompter.h"

#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace Gtk;
using namespace Gtkmm2ext;

void
Editor::handle_new_named_selection ()
{
	ARDOUR_UI::instance()->call_slot (mem_fun(*this, &Editor::redisplay_named_selections));
}

void
Editor::add_named_selection_to_named_selection_display (NamedSelection& selection)
{
        TreeModel::Row row = *(named_selection_model->append());
	row[named_selection_columns.text] = selection.name;
	row[named_selection_columns.selection] = &selection;
}

void
Editor::redisplay_named_selections ()
{
        //GTK2FIX
        //named_selection_display.freeze ();
	named_selection_model->clear ();
	session->foreach_named_selection (*this, &Editor::add_named_selection_to_named_selection_display);
	//named_selection_display.thaw ();
}

gint
Editor::named_selection_display_button_press (GdkEventButton *ev)
{

	TreeModel::Children rows = named_selection_model->children();
	TreeModel::Children::iterator i;
	Glib::RefPtr<TreeSelection> selection = named_selection_display.get_selection();

	for (i = rows.begin(); i != rows.end(); ++i) {
	        if (selection->is_selected (i)) {
		        switch (ev->button) {
			case 1:
			        if (Keyboard::is_delete_event (ev)) {
				        session->remove_named_selection ((*i)[named_selection_columns.selection]);
					return stop_signal (named_selection_display, "button_press_event");
				}
				break;
			case 2:
			        break;
			case 3:
			        break;
			default:
			        break;
			}
		}
	}
	return FALSE;
}


void
Editor::named_selection_display_selection_changed ()
{
}

void
Editor::name_selection ()
{
	ArdourPrompter p;

	p.set_prompt (_("name for chunk:"));
	p.change_labels (_("Create chunk"), _("Forget it"));
	p.show_all ();

	switch (p.run ()) {
	case GTK_RESPONSE_ACCEPT:
	  string name;
		p.get_result (name);
		if (name.length()) {
		  create_named_selection (name);
		}	
		break;
	}

}

void
Editor::named_selection_name_chosen ()
{
	Gtk::Main::quit ();
}

void
Editor::create_named_selection (stringcr_t name)
{
	if (session == 0) {
		return;
	}

	/* check for a range-based selection */

	if (selection->time.empty()) {
		return;
	}

	
	TrackViewList *views = get_valid_views (selection->time.track, selection->time.group);

	if (views->empty()) {
		delete views;
		return;
	}

	Playlist*       what_we_found;
	list<Playlist*> thelist;

	for (TrackViewList::iterator i = views->begin(); i != views->end(); ++i) {
		
		Playlist *pl = (*i)->playlist();
		
		if (pl) {
			
			if ((what_we_found = pl->copy (selection->time, false)) != 0) {

				thelist.push_back (what_we_found);
			}
		}
	}

	NamedSelection* ns;
	TreeModel::Row row = *(named_selection_model->append());

	ns = new NamedSelection (name, thelist);
	row[named_selection_columns.selection] = ns;
	row[named_selection_columns.text] = name;

	/* make the one we just added be selected */

	named_selection_display.get_selection()->select (row);

}

