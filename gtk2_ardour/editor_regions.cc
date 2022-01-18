/*
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2021 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2018-2019 Ben Loftis <ben@harrisonconsoles.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <algorithm>
#include <list>
#include <string>

#include "ardour/region.h"
#include "ardour/session.h"

#include "widgets/choice.h"

#include "ardour_ui.h"
#include "editor.h"
#include "editor_regions.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "region_view.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;

using Gtkmm2ext::Keyboard;

EditorRegions::EditorRegions (Editor* e)
	: EditorComponent (e)
{
	init ();

	_change_connection = _display.get_selection ()->signal_changed ().connect (sigc::mem_fun (*this, &EditorRegions::selection_changed));

	e->EditorFreeze.connect (_editor_freeze_connection, MISSING_INVALIDATOR, boost::bind (&EditorRegions::freeze_tree_model, this), gui_context ());
	e->EditorThaw.connect (_editor_thaw_connection, MISSING_INVALIDATOR, boost::bind (&EditorRegions::thaw_tree_model, this), gui_context ());
}

void
EditorRegions::init ()
{
	add_name_column ();
	setup_col (append_col (_columns.channels, "Chans    "), 1, ALIGN_LEFT, _("# Ch"), _("# Channels in the region"));
	add_tag_column ();

	int cb_width = 24;
	int bbt_width, height;

	Glib::RefPtr<Pango::Layout> layout = _display.create_pango_layout (X_("000|000|000"));
	Gtkmm2ext::get_pixel_size (layout, bbt_width, height);

	TreeViewColumn* tvc;

	tvc = append_col (_columns.start, bbt_width);
	setup_col (tvc, 16, ALIGN_RIGHT, _("Start"), _("Position of start of region"));
	tvc = append_col (_columns.length, bbt_width);
	setup_col (tvc, 4, ALIGN_RIGHT, _("Length"), _("Length of the region"));

	tvc = append_col (_columns.locked, cb_width);
	setup_col (tvc, -1, ALIGN_CENTER, S_("Lock|L"), _("Region position locked?"));
	setup_toggle (tvc, sigc::mem_fun (*this, &EditorRegions::locked_changed));

	tvc = append_col (_columns.glued, cb_width);
	setup_col (tvc, -1, ALIGN_CENTER, S_("Glued|G"), _("Region position glued to Bars|Beats time?"));
	setup_toggle (tvc, sigc::mem_fun (*this, &EditorRegions::glued_changed));

	tvc = append_col (_columns.muted, cb_width);
	setup_col (tvc, -1, ALIGN_CENTER, S_("Mute|M"), _("Region muted?"));
	setup_toggle (tvc, sigc::mem_fun (*this, &EditorRegions::muted_changed));

	tvc = append_col (_columns.opaque, cb_width);
	setup_col (tvc, -1, ALIGN_CENTER, S_("Opaque|O"), _("Region opaque (blocks regions below it from being heard)?"));
	setup_toggle (tvc, sigc::mem_fun (*this, &EditorRegions::opaque_changed));

#ifdef SHOW_REGION_EXTRAS
	tvc = append_col (_columns.end, bbt_width);
	setup_col (tvc, 5, ALIGN_RIGHT, _("End"), _("Position of end of region"));
	tvc = append_col (_columns.sync, bbt_width);
	setup_col (tvc, -1, ALIGN_RIGHT, _("Sync"), _("Position of region sync point, relative to start of the region"));
	tvc = append_col (_columns.fadein, bbt_width);
	setup_col (tvc, -1, ALIGN_RIGHT, _("Fade In"), _("Length of region fade-in (units: secondary clock, () if disabled"));
	tvc = append_col (_columns.fadeout, bbt_width);
	setup_col (tvc, -1, ALIGN_RIGHT, _("Fade out"), _("Length of region fade-out (units: secondary clock, () if disabled"));
#endif
}

void
EditorRegions::selection_changed ()
{
	_editor->_region_selection_change_updates_region_list = false;

	if (_display.get_selection ()->count_selected_rows () > 0) {
		TreeIter                             iter;
		TreeView::Selection::ListHandle_Path rows = _display.get_selection ()->get_selected_rows ();

		_editor->get_selection ().clear_regions ();

		for (TreeView::Selection::ListHandle_Path::iterator i = rows.begin (); i != rows.end (); ++i) {
			if ((iter = _model->get_iter (*i))) {
				boost::shared_ptr<Region> region = (*iter)[_columns.region];

				// they could have clicked on a row that is just a placeholder, like "Hidden"
				// although that is not allowed by our selection filter. check it anyway
				// since we need a region ptr.

				if (region) {
					_change_connection.block (true);
					_editor->set_selected_regionview_from_region_list (region, Selection::Add);
					_change_connection.block (false);
				}
			}
		}
	} else {
		_editor->get_selection ().clear_regions ();
	}

	_editor->_region_selection_change_updates_region_list = true;
}

void
EditorRegions::set_selected (RegionSelection& regions)
{
	for (RegionSelection::iterator i = regions.begin (); i != regions.end (); ++i) {
		boost::shared_ptr<Region> r ((*i)->region ());

		RegionRowMap::iterator it;

		it = region_row_map.find (r);

		if (it != region_row_map.end ()) {
			TreeModel::iterator j = it->second;
			_display.get_selection ()->select (*j);
		}
	}
}

void
EditorRegions::show_context_menu (int button, int time)
{
	using namespace Gtk::Menu_Helpers;
	Gtk::Menu* menu = dynamic_cast<Menu*> (ActionManager::get_widget (X_("/PopupRegionMenu")));
	menu->popup (button, time);
}

bool
EditorRegions::button_press (GdkEventButton* ev)
{
	boost::shared_ptr<Region> region;
	TreeIter                  iter;
	TreeModel::Path           path;
	TreeViewColumn*           column;
	int                       cellx;
	int                       celly;

	if (_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		if ((iter = _model->get_iter (path))) {
			region = (*iter)[_columns.region];
		}
	}

	if (Keyboard::is_context_menu_event (ev)) {
		show_context_menu (ev->button, ev->time);
		return true;
	}

	if (region != 0 && Keyboard::is_button2_event (ev)) {
		/* start/stop audition */
		if (!Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
			_editor->consider_auditioning (region);
		}
		return true;
	}

	return false;
}

void
EditorRegions::selection_mapover (sigc::slot<void, boost::shared_ptr<Region>> sl)
{
	Glib::RefPtr<TreeSelection>                    selection = _display.get_selection ();
	TreeView::Selection::ListHandle_Path           rows      = selection->get_selected_rows ();
	TreeView::Selection::ListHandle_Path::iterator i         = rows.begin ();

	if (selection->count_selected_rows () == 0 || _session == 0) {
		return;
	}

	for (; i != rows.end (); ++i) {
		TreeIter iter;

		if ((iter = _model->get_iter (*i))) {
			/* some rows don't have a region associated with them, but can still be
			   selected (XXX maybe prevent them from being selected)
			*/

			boost::shared_ptr<Region> r = (*iter)[_columns.region];

			if (r) {
				sl (r);
			}
		}
	}
}

void
EditorRegions::regions_changed (boost::shared_ptr<RegionList> rl, const PropertyChange& what_changed)
{
	/* the grid is most interested in the regions that are *visible* in the editor.
	 * this is a convenient place to flag changes to the grid cache, on a visible region */
	PropertyChange grid_interests;
	grid_interests.add (ARDOUR::Properties::length);
	grid_interests.add (ARDOUR::Properties::sync_position);

	if (what_changed.contains (grid_interests)) {
		_editor->mark_region_boundary_cache_dirty ();
	}

	RegionListBase::regions_changed (rl, what_changed);
}

boost::shared_ptr<Region>
EditorRegions::get_single_selection ()
{
	Glib::RefPtr<TreeSelection> selected = _display.get_selection ();

	if (selected->count_selected_rows () != 1) {
		return boost::shared_ptr<Region> ();
	}

	TreeView::Selection::ListHandle_Path rows = selected->get_selected_rows ();

	/* only one row selected, so rows.begin() is it */

	TreeIter iter = _model->get_iter (*rows.begin ());

	if (!iter) {
		return boost::shared_ptr<Region> ();
	}

	return (*iter)[_columns.region];
}

void
EditorRegions::remove_unused_regions ()
{
	vector<string> choices;
	string         prompt;

	if (!_session) {
		return;
	}

	prompt = _("Do you really want to remove unused regions?"
	           "\n(This is destructive and cannot be undone)");

	choices.push_back (_("No, do nothing."));
	choices.push_back (_("Yes, remove."));

	ArdourWidgets::Choice prompter (_("Remove unused regions"), prompt, choices);

	if (prompter.run () == 1) {
		_no_redisplay = true;
		_session->cleanup_regions ();
		_no_redisplay = false;
		redisplay ();
	}
}
