/*
    Copyright (C) 2011-2012 Paul Davis

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

#include <gtkmm/table.h>
#include <gtkmm/stock.h>
#include <gtkmm/alignment.h>

#include "pbd/stateful_diff_command.h"

#include "ardour/region.h"

#include "gui_thread.h"
#include "keyboard.h"
#include "public_editor.h"
#include "region_layering_order_editor.h"
#include "region_view.h"
#include "utils.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;

RegionLayeringOrderEditor::RegionLayeringOrderEditor (PublicEditor& pe)
	: ArdourWindow (_("RegionLayeringOrderEditor"))
	, position (0)
	, in_row_change (false)
        , regions_at_position (0)
	, layering_order_model (Gtk::ListStore::create (layering_order_columns))
        , clock ("layer dialog", true, "", false, false, false)
	, editor (pe)
	, _time_axis_view (0)
{
	set_name ("RegionLayeringOrderEditorWindow");

	layering_order_display.set_model (layering_order_model);

	layering_order_display.append_column (_("Region Name"), layering_order_columns.name);
	layering_order_display.set_headers_visible (true);
	layering_order_display.set_reorderable (false);
	layering_order_display.set_rules_hint (true);

	scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	scroller.add (layering_order_display);

	clock.set_mode (AudioClock::BBT);

        Gtk::Table* scroller_table = manage (new Gtk::Table);
        scroller_table->set_size_request (300, 250);
        scroller_table->attach (scroller, 0, 1, 0, 1);
        scroller_table->set_col_spacings (5);
        scroller_table->set_row_spacings (5);

        track_label.set_name ("RegionLayeringOrderEditorLabel");
        track_label.set_text (_("Track:"));
	track_label.set_alignment (0, 0.5);
        clock_label.set_name ("RegionLayeringOrderEditorLabel");
        clock_label.set_text (_("Position:"));
	clock_label.set_alignment (0, 0.5);
        track_name_label.set_name ("RegionLayeringOrderEditorNameLabel");
	track_name_label.set_alignment (0, 0.5);
        clock.set_mode (AudioClock::BBT);

        Gtk::Table* info_table = manage (new Gtk::Table (2, 2));
        info_table->set_col_spacings (5);
        info_table->set_row_spacings (5);
        info_table->attach (track_label, 0, 1, 0, 1, FILL, FILL);
        info_table->attach (track_name_label, 1, 2, 0, 1, FILL, FILL);
        info_table->attach (clock_label, 0, 1, 1, 2, FILL, FILL);
        info_table->attach (clock, 1, 2, 1, 2, Gtk::AttachOptions(0), FILL);

	Gtk::VBox* vbox = Gtk::manage (new Gtk::VBox ());
	vbox->set_spacing (12);
	vbox->pack_start (*info_table, false, false);
	vbox->pack_start (*scroller_table, true, true);
	add (*vbox);

        info_table->set_name ("RegionLayeringOrderTable");
        scroller_table->set_name ("RegionLayeringOrderTable");

	layering_order_display.set_name ("RegionLayeringOrderDisplay");
	layering_order_display.get_selection()->set_mode (SELECTION_SINGLE);
	layering_order_display.get_selection()->signal_changed ().connect (mem_fun (*this, &RegionLayeringOrderEditor::row_selected));

	layering_order_display.grab_focus ();

	set_title (_("Choose Top Region"));
	show_all();
}

RegionLayeringOrderEditor::~RegionLayeringOrderEditor ()
{
	
}

void
RegionLayeringOrderEditor::row_selected ()
{
	if (in_row_change) {
		return;
	}

	Glib::RefPtr<TreeSelection> selection = layering_order_display.get_selection();
	TreeModel::iterator iter = selection->get_selected(); // only used with Gtk::SELECTION_SINGLE

	if (!iter) {
		return;
	}
	
	TreeModel::Row row = *iter;
	RegionView* rv = row[layering_order_columns.region_view];
	
	vector<RegionView*> eq;
	editor.get_equivalent_regions (rv, eq, Properties::edit.property_id);

	/* XXX this should be reversible, really */
	
	for (vector<RegionView*>::iterator i = eq.begin(); i != eq.end(); ++i) {
		boost::shared_ptr<Playlist> pl = (*i)->region()->playlist();
		if (pl) {
			pl->raise_region_to_top ((*i)->region());
		}
	}
}

struct RegionViewCompareByLayer {
	bool operator() (RegionView* a, RegionView* b) const {
		return a->region()->layer() > b->region()->layer();
	}
};

void
RegionLayeringOrderEditor::refill ()
{
	assert (_time_axis_view);
	
	regions_at_position = 0;
	in_row_change = true;
	layering_order_model->clear ();

	RegionSelection region_list;
	TrackViewList ts;
	ts.push_back (_time_axis_view);
	editor.get_regions_at (region_list, position, ts);

	regions_at_position = region_list.size ();

	if (regions_at_position < 2) {
		playlist_modified_connection.disconnect ();
		hide ();
		in_row_change = false;
		return;
	}

	RegionViewCompareByLayer cmp;
	region_list.sort (cmp);

	for (RegionSelection::const_iterator i = region_list.begin(); i != region_list.end(); ++i) {
		TreeModel::Row newrow = *(layering_order_model->append());
		newrow[layering_order_columns.name] = (*i)->region()->name();
		newrow[layering_order_columns.region_view] = *i;

               if (i == region_list.begin()) {
                       layering_order_display.get_selection()->select(newrow);
               }
	}

	in_row_change = false;
}

void
RegionLayeringOrderEditor::set_context (const string& a_name, Session* s, TimeAxisView* tav, boost::shared_ptr<Playlist> pl, framepos_t pos)
{
        track_name_label.set_text (a_name);

	clock.set_session (s);
	clock.set (pos, true);

	playlist_modified_connection.disconnect ();
	pl->ContentsChanged.connect (playlist_modified_connection, invalidator (*this), boost::bind
				     (&RegionLayeringOrderEditor::playlist_modified, this), gui_context());

	_time_axis_view = tav;

	position = pos;
	refill ();
}

bool
RegionLayeringOrderEditor::on_key_press_event (GdkEventKey* ev)
{
	bool handled = false;

	/* in general, we want shortcuts working while in this
	   dialog. However, we'd like to treat "return" specially
	   since it is used for row activation. So ..

	   for return: try normal handling first
	   then try the editor (to get accelerators/shortcuts)
	   then try normal handling (for keys other than return)
	*/

	if (ev->keyval == GDK_Return) {
		handled = ArdourWindow::on_key_press_event (ev);
	}

	if (!handled) {
		handled = key_press_focus_accelerator_handler (editor, ev);
	}

	if (!handled) {
		handled = ArdourWindow::on_key_press_event (ev);
	}

	return handled;
}

void
RegionLayeringOrderEditor::maybe_present ()
{
	if (regions_at_position < 2) {
		hide ();
		return;
	}

	present ();
}

void
RegionLayeringOrderEditor::playlist_modified ()
{
	refill ();
}
