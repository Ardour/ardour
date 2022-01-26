/*
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
#include <sstream>
#include <string>

#include "ardour/audiofilesource.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/session_directory.h"
#include "ardour/smf_source.h"
#include "ardour/source.h"

#include "widgets/choice.h"

#include "context_menu_helper.h"
#include "editing.h"
#include "editor.h"
#include "editor_drag.h"
#include "editor_sources.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "region_view.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using Gtkmm2ext::Keyboard;

EditorSources::EditorSources (Editor* e)
	: EditorComponent (e)
{
	init ();

	/* setup DnD Receive */
	list<TargetEntry> source_list_target_table;

	source_list_target_table.push_back (TargetEntry ("text/plain"));
	source_list_target_table.push_back (TargetEntry ("text/uri-list"));
	source_list_target_table.push_back (TargetEntry ("application/x-rootwin-drop"));

	_display.add_drop_targets (source_list_target_table);
	_display.signal_drag_data_received ().connect (sigc::mem_fun (*this, &EditorSources::drag_data_received));

	_change_connection = _display.get_selection ()->signal_changed ().connect (sigc::mem_fun (*this, &EditorSources::selection_changed));

	e->EditorFreeze.connect (_editor_freeze_connection, MISSING_INVALIDATOR, boost::bind (&EditorSources::freeze_tree_model, this), gui_context ());
	e->EditorThaw.connect (_editor_thaw_connection, MISSING_INVALIDATOR, boost::bind (&EditorSources::thaw_tree_model, this), gui_context ());
}

void
EditorSources::init ()
{
	int bbt_width, date_width, height;

	Glib::RefPtr<Pango::Layout> layout = _display.create_pango_layout (X_("000|000|000"));
	Gtkmm2ext::get_pixel_size (layout, bbt_width, height);
	Glib::RefPtr<Pango::Layout> layout2 = _display.create_pango_layout (X_("2018-10-14 12:12:30"));
	Gtkmm2ext::get_pixel_size (layout2, date_width, height);

	add_name_column ();

	setup_col (append_col (_columns.channels, "Chans    "), 1, ALIGN_LEFT, _("# Ch"), _("# Channels in the region"));
	setup_col (append_col (_columns.captd_for, date_width), 17, ALIGN_LEFT, _("Captured For"), _("Original Track this was recorded on"));
	setup_col (append_col (_columns.captd_xruns, "1234567890"), 21, ALIGN_RIGHT, _("# Xruns"), _("Number of dropouts that occurred during recording"));

	add_tag_column ();

	setup_col (append_col (_columns.take_id, date_width), 18, ALIGN_LEFT, _("Take ID"), _("Take ID"));
	setup_col (append_col (_columns.natural_pos, bbt_width), 20, ALIGN_RIGHT, _("Orig Pos"), _("Original Position of the file on timeline, when it was recorded"));

	TreeViewColumn* tvc = append_col (_columns.path, bbt_width);
	setup_col (tvc, 13, ALIGN_LEFT, _("Path"), _("Path (folder) of the file location"));
	tvc->set_expand (true);

	/* make Name and Path columns manually resizable */
	_display.get_column (0)->set_resizable (true);
	_display.get_column (5)->set_resizable (true);
}

void
EditorSources::selection_changed ()
{
	if (_display.get_selection ()->count_selected_rows () > 0) {
		TreeIter                             iter;
		TreeView::Selection::ListHandle_Path rows = _display.get_selection ()->get_selected_rows ();

		_editor->get_selection ().clear_regions ();

		for (TreeView::Selection::ListHandle_Path::iterator i = rows.begin (); i != rows.end (); ++i) {
			if ((iter = _model->get_iter (*i))) {
				/* highlight any regions in the editor that use this region's source */
				boost::shared_ptr<ARDOUR::Region> region = (*iter)[_columns.region];
				if (!region)
					continue;

				boost::shared_ptr<ARDOUR::Source> source = region->source ();
				if (source) {
					set<boost::shared_ptr<Region>> regions;
					RegionFactory::get_regions_using_source (source, regions);

					for (set<boost::shared_ptr<Region>>::iterator region = regions.begin (); region != regions.end (); region++) {
						_change_connection.block (true);
						_editor->set_selected_regionview_from_region_list (*region, Selection::Add);
						_change_connection.block (false);
					}
				}
			}
		}
	} else {
		_editor->get_selection ().clear_regions ();
	}
}

void
EditorSources::show_context_menu (int button, int time)
{
	using namespace Gtk::Menu_Helpers;
	Gtk::Menu* menu  = ARDOUR_UI_UTILS::shared_popup_menu ();
	MenuList&  items = menu->items ();
#ifdef RECOVER_REGIONS_IS_WORKING
	items.push_back (MenuElem (_("Recover the selected Sources to their original Track & Position"),
	                           sigc::mem_fun (*this, &EditorSources::recover_selected_sources)));
#endif
	items.push_back (MenuElem (_("Remove the selected Sources"),
	                           sigc::mem_fun (*this, &EditorSources::remove_selected_sources)));
	menu->popup (1, time);
}

void
EditorSources::recover_selected_sources ()
{
	ARDOUR::RegionList to_be_recovered;

	if (_display.get_selection ()->count_selected_rows () > 0) {
		TreeIter                             iter;
		TreeView::Selection::ListHandle_Path rows = _display.get_selection ()->get_selected_rows ();
		for (TreeView::Selection::ListHandle_Path::iterator i = rows.begin (); i != rows.end (); ++i) {
			if ((iter = _model->get_iter (*i))) {
				boost::shared_ptr<ARDOUR::Region> region = (*iter)[_columns.region];
				if (region) {
					to_be_recovered.push_back (region);
				}
			}
		}
	}

	/* ToDo */
	_editor->recover_regions (to_be_recovered); // this operation should be undo-able
}

void
EditorSources::remove_selected_sources ()
{
	vector<string> choices;
	string         prompt;

	prompt = _("Do you want to remove the selected Sources?"
	           "\nThis operation cannot be undone."
	           "\nThe source files will not actually be deleted until you execute Session->Cleanup.");

	choices.push_back (_("No, do nothing."));
	choices.push_back (_("Only remove the Regions that use these Sources."));
	choices.push_back (_("Yes, remove the Regions and Sources (cannot be undone!)"));

	Choice prompter (_("Remove selected Sources"), prompt, choices);

	int opt = prompter.run ();

	if (opt >= 1) {
		std::list<boost::weak_ptr<ARDOUR::Source>> to_be_removed;

		if (_display.get_selection ()->count_selected_rows () > 0) {
			TreeIter                             iter;
			TreeView::Selection::ListHandle_Path rows = _display.get_selection ()->get_selected_rows ();

			_editor->get_selection ().clear_regions ();

			for (TreeView::Selection::ListHandle_Path::iterator i = rows.begin (); i != rows.end (); ++i) {
				if ((iter = _model->get_iter (*i))) {
					boost::shared_ptr<ARDOUR::Region> region = (*iter)[_columns.region];

					if (!region)
						continue;

					boost::shared_ptr<ARDOUR::Source> source = region->source ();
					if (source) {
						set<boost::shared_ptr<Region>> regions;
						RegionFactory::get_regions_using_source (source, regions);

						for (set<boost::shared_ptr<Region>>::iterator region = regions.begin (); region != regions.end (); region++) {
							_change_connection.block (true);
							_editor->set_selected_regionview_from_region_list (*region, Selection::Add);
							_change_connection.block (false);
						}

						to_be_removed.push_back (source);
					}
				}
			}

			_editor->remove_regions (_editor->get_regions_from_selection_and_entered (), false /*can_ripple*/, false /*as_part_of_other_command*/); // this operation is undo-able

			if (opt == 2) {
				for (std::list<boost::weak_ptr<ARDOUR::Source>>::iterator i = to_be_removed.begin (); i != to_be_removed.end (); ++i) {
					_session->remove_source (*i); // this operation is (currently) not undo-able
				}
			}
		}
	}
}

bool
EditorSources::key_press (GdkEventKey* ev)
{
	switch (ev->keyval) {
		case GDK_BackSpace:
			remove_selected_sources ();
			return true;

		default:
			break;
	}

	return RegionListBase::key_press (ev);
}

bool
EditorSources::button_press (GdkEventButton* ev)
{
	if (Keyboard::is_context_menu_event (ev)) {
		show_context_menu (ev->button, ev->time);
		return true;
	}
	return false;
}

void
EditorSources::drag_data_received (const RefPtr<Gdk::DragContext>& context,
                                   int x, int y,
                                   const SelectionData& data,
                                   guint info, guint dtime)
{
	vector<string> paths;

	if (data.get_target () == "GTK_TREE_MODEL_ROW") {
		/* something is being dragged over the source list */
		_editor->_drags->abort ();
		_display.on_drag_data_received (context, x, y, data, info, dtime);
		return;
	}

	if (_session && convert_drop_to_paths (paths, data)) {
		timepos_t pos;
		bool      copy = ((context->get_actions () & (Gdk::ACTION_COPY | Gdk::ACTION_LINK | Gdk::ACTION_MOVE)) == Gdk::ACTION_COPY);

		if (UIConfiguration::instance ().get_only_copy_imported_files () || copy) {
			_editor->do_import (paths, Editing::ImportDistinctFiles, Editing::ImportAsRegion,
			                    SrcBest, SMFTrackNumber, SMFTempoIgnore, pos);
		} else {
			_editor->do_embed (paths, Editing::ImportDistinctFiles, Editing::ImportAsRegion, pos);
		}
		context->drag_finish (true, false, dtime);
	}
}

boost::shared_ptr<ARDOUR::Region>
EditorSources::get_single_selection ()
{
	Glib::RefPtr<TreeSelection> selected = _display.get_selection ();

	if (selected->count_selected_rows () != 1) {
		return boost::shared_ptr<ARDOUR::Region> ();
	}

	TreeView::Selection::ListHandle_Path rows = selected->get_selected_rows ();

	/* only one row selected, so rows.begin() is it */

	TreeIter iter = _model->get_iter (*rows.begin ());

	if (!iter) {
		return boost::shared_ptr<ARDOUR::Region> ();
	}

	return (*iter)[_columns.region];
}
