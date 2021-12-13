/*
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
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
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>

#include "pbd/basename.h"
#include "pbd/enumwriter.h"

#include "ardour/audiofilesource.h"
#include "ardour/audioregion.h"
#include "ardour/profile.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/session_playlist.h"
#include "ardour/silentfilesource.h"

#include "gtkmm2ext/treeutils.h"
#include "gtkmm2ext/utils.h"

#include "widgets/choice.h"
#include "widgets/tooltips.h"

#include "actions.h"
#include "ardour_ui.h"
#include "audio_clock.h"
#include "editing.h"
#include "editing_convert.h"
#include "editor.h"
#include "editor_drag.h"
#include "editor_regions.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "main_clock.h"
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
using namespace Editing;
using namespace Temporal;

using Gtkmm2ext::Keyboard;

//#define SHOW_REGION_EXTRAS

EditorRegions::EditorRegions (Editor* e)
        : EditorComponent (e)
        , old_focus (0)
        , name_editable (0)
        , tags_editable (0)
        , _menu (0)
        , _no_redisplay (false)
{
	_display.set_size_request (100, -1);
	_display.set_rules_hint (true);
	_display.set_name ("RegionList");
	_display.set_fixed_height_mode (true);
	_display.set_reorderable (false);

	/* Try to prevent single mouse presses from initiating edits.
	 * This relies on a hack in gtktreeview.c:gtk_treeview_button_press() */
	_display.set_data ("mouse-edits-require-mod1", (gpointer)0x1);

	_model = TreeStore::create (_columns);
	_model->set_sort_column (0, SORT_ASCENDING);

	/* column widths */
	int bbt_width, date_width, chan_width, check_width, height;

	Glib::RefPtr<Pango::Layout> layout = _display.create_pango_layout (X_ ("000|000|000"));
	Gtkmm2ext::get_pixel_size (layout, bbt_width, height);

	Glib::RefPtr<Pango::Layout> layout2 = _display.create_pango_layout (X_ ("2099-10-10 10:10:30"));
	Gtkmm2ext::get_pixel_size (layout2, date_width, height);

	Glib::RefPtr<Pango::Layout> layout3 = _display.create_pango_layout (X_ ("Chans    "));
	Gtkmm2ext::get_pixel_size (layout3, chan_width, height);

	check_width = 20;

	TreeViewColumn* col_name = manage (new TreeViewColumn ("", _columns.name));
	col_name->set_fixed_width (120);
	col_name->set_sizing (TREE_VIEW_COLUMN_FIXED);
	TreeViewColumn* col_chans = manage (new TreeViewColumn ("", _columns.channels));
	col_chans->set_fixed_width (chan_width);
	col_chans->set_sizing (TREE_VIEW_COLUMN_FIXED);
	TreeViewColumn* col_tags = manage (new TreeViewColumn ("", _columns.tags));
	col_tags->set_fixed_width (date_width);
	col_tags->set_sizing (TREE_VIEW_COLUMN_FIXED);
	TreeViewColumn* col_start = manage (new TreeViewColumn ("", _columns.start));
	col_start->set_fixed_width (bbt_width);
	col_start->set_sizing (TREE_VIEW_COLUMN_FIXED);
	TreeViewColumn* col_end = manage (new TreeViewColumn ("", _columns.end));
	col_end->set_fixed_width (bbt_width);
	col_end->set_sizing (TREE_VIEW_COLUMN_FIXED);
	TreeViewColumn* col_length = manage (new TreeViewColumn ("", _columns.length));
	col_length->set_fixed_width (bbt_width);
	col_length->set_sizing (TREE_VIEW_COLUMN_FIXED);
	TreeViewColumn* col_sync = manage (new TreeViewColumn ("", _columns.sync));
	col_sync->set_fixed_width (bbt_width);
	col_sync->set_sizing (TREE_VIEW_COLUMN_FIXED);
	TreeViewColumn* col_fadein = manage (new TreeViewColumn ("", _columns.fadein));
	col_fadein->set_fixed_width (bbt_width);
	col_fadein->set_sizing (TREE_VIEW_COLUMN_FIXED);
	TreeViewColumn* col_fadeout = manage (new TreeViewColumn ("", _columns.fadeout));
	col_fadeout->set_fixed_width (bbt_width);
	col_fadeout->set_sizing (TREE_VIEW_COLUMN_FIXED);
	TreeViewColumn* col_locked = manage (new TreeViewColumn ("", _columns.locked));
	col_locked->set_fixed_width (check_width);
	col_locked->set_sizing (TREE_VIEW_COLUMN_FIXED);
	TreeViewColumn* col_glued = manage (new TreeViewColumn ("", _columns.glued));
	col_glued->set_fixed_width (check_width);
	col_glued->set_sizing (TREE_VIEW_COLUMN_FIXED);
	TreeViewColumn* col_muted = manage (new TreeViewColumn ("", _columns.muted));
	col_muted->set_fixed_width (check_width);
	col_muted->set_sizing (TREE_VIEW_COLUMN_FIXED);
	TreeViewColumn* col_opaque = manage (new TreeViewColumn ("", _columns.opaque));
	col_opaque->set_fixed_width (check_width);
	col_opaque->set_sizing (TREE_VIEW_COLUMN_FIXED);

	_display.append_column (*col_name);
	_display.append_column (*col_chans);
	_display.append_column (*col_tags);
	_display.append_column (*col_start);
	_display.append_column (*col_length);
	_display.append_column (*col_locked);
	_display.append_column (*col_glued);
	_display.append_column (*col_muted);
	_display.append_column (*col_opaque);

#ifdef SHOW_REGION_EXTRAS
	_display.append_column (*col_end);
	_display.append_column (*col_sync);
	_display.append_column (*col_fadein);
	_display.append_column (*col_fadeout);
#endif

	TreeViewColumn* col;
	Gtk::Label*     l;

	struct ColumnInfo {
		int                index;
		int                sort_idx;
		Gtk::AlignmentEnum al;
		const char*        label;
		const char*        tooltip;
	} ci[] = {
	/* clang-format off */
		{ 0,  0,  ALIGN_LEFT,    _("Name"),      _("Region name") },
		{ 1,  1,  ALIGN_LEFT,    _("# Ch"),      _("# Channels in the region") },
		{ 2,  2,  ALIGN_LEFT,    _("Tags"),      _("Tags") },
		{ 3, 16,  ALIGN_RIGHT,   _("Start"),     _("Position of start of region") },
		{ 4,  4,  ALIGN_RIGHT,   _("Length"),    _("Length of the region") },
		{ 5, -1,  ALIGN_CENTER, S_("Lock|L"),    _("Region position locked?") },
		{ 6, -1,  ALIGN_CENTER, S_("Glued|G"),   _("Region position glued to Bars|Beats time?") },
		{ 7, -1,  ALIGN_CENTER, S_("Mute|M"),    _("Region muted?") },
		{ 8, -1,  ALIGN_CENTER, S_("Opaque|O"),  _("Region opaque (blocks regions below it from being heard)?") },
#ifdef SHOW_REGION_EXTRAS
		{ 9,  5,  ALIGN_RIGHT,  _("End"),       _("Position of end of region") },
		{ 10, -1,  ALIGN_RIGHT,  _("Sync"),      _("Position of region sync point, relative to start of the region") },
		{ 11,-1,  ALIGN_RIGHT,  _("Fade In"),   _("Length of region fade-in (units: secondary clock), () if disabled") },
		{ 12,-1,  ALIGN_RIGHT,  _("Fade Out"),  _("Length of region fade-out (units: secondary clock), () if disabled") },
#endif
		{ -1,-1,  ALIGN_CENTER, 0, 0 }
	};
	/* clang-format on */

	for (int i = 0; ci[i].index >= 0; ++i) {
		col = _display.get_column (ci[i].index);

		/* add the label */
		l = manage (new Label (ci[i].label));
		l->set_alignment (ci[i].al);
		set_tooltip (*l, ci[i].tooltip);
		col->set_widget (*l);
		l->show ();

		col->set_sort_column (ci[i].sort_idx);

		col->set_expand (false);

		/* this sets the alignment of the column header... */
		col->set_alignment (ci[i].al);

		/* ...and this sets the alignment for the data cells */
		CellRendererText* renderer = dynamic_cast<CellRendererText*> (_display.get_column_cell_renderer (i));
		if (renderer) {
			renderer->property_xalign () = (ci[i].al == ALIGN_RIGHT ? 1.0 : (ci[i].al == ALIGN_LEFT ? 0.0 : 0.5));
		}
	}

	_display.set_model (_model);

	_display.set_headers_visible (true);
	_display.set_rules_hint ();

	if (UIConfiguration::instance ().get_use_tooltips ()) {
		/* show path as the row tooltip */
		_display.set_tooltip_column (13); /* path */
	}
	_display.get_selection ()->set_select_function (sigc::mem_fun (*this, &EditorRegions::selection_filter));

	/* Name cell: make editable */
	CellRendererText* region_name_cell     = dynamic_cast<CellRendererText*> (_display.get_column_cell_renderer (0));
	region_name_cell->property_editable () = true;
	region_name_cell->signal_edited ().connect (sigc::mem_fun (*this, &EditorRegions::name_edit));
	region_name_cell->signal_editing_started ().connect (sigc::mem_fun (*this, &EditorRegions::name_editing_started));

	/* Region Name: color turns red if source is missing. */
	TreeViewColumn*   tv_col   = _display.get_column (0);
	CellRendererText* renderer = dynamic_cast<CellRendererText*> (_display.get_column_cell_renderer (0));
	tv_col->add_attribute (renderer->property_text (), _columns.name);
	tv_col->add_attribute (renderer->property_foreground_gdk (), _columns.color_);
	tv_col->set_expand (true);

	/* Tags cell: make editable */
	CellRendererText* region_tags_cell     = dynamic_cast<CellRendererText*> (_display.get_column_cell_renderer (2));
	region_tags_cell->property_editable () = true;
	region_tags_cell->signal_edited ().connect (sigc::mem_fun (*this, &EditorRegions::tag_edit));
	region_tags_cell->signal_editing_started ().connect (sigc::mem_fun (*this, &EditorRegions::tag_editing_started));

	/* checkbox cells */
	int check_start_col = 5;

	CellRendererToggle* locked_cell      = dynamic_cast<CellRendererToggle*> (_display.get_column_cell_renderer (check_start_col++));
	locked_cell->property_activatable () = true;
	locked_cell->signal_toggled ().connect (sigc::mem_fun (*this, &EditorRegions::locked_changed));

	CellRendererToggle* glued_cell      = dynamic_cast<CellRendererToggle*> (_display.get_column_cell_renderer (check_start_col++));
	glued_cell->property_activatable () = true;
	glued_cell->signal_toggled ().connect (sigc::mem_fun (*this, &EditorRegions::glued_changed));

	CellRendererToggle* muted_cell      = dynamic_cast<CellRendererToggle*> (_display.get_column_cell_renderer (check_start_col++));
	muted_cell->property_activatable () = true;
	muted_cell->signal_toggled ().connect (sigc::mem_fun (*this, &EditorRegions::muted_changed));

	CellRendererToggle* opaque_cell      = dynamic_cast<CellRendererToggle*> (_display.get_column_cell_renderer (check_start_col));
	opaque_cell->property_activatable () = true;
	opaque_cell->signal_toggled ().connect (sigc::mem_fun (*this, &EditorRegions::opaque_changed));

	_display.get_selection ()->set_mode (SELECTION_MULTIPLE);
	_display.add_object_drag (_columns.region.index (), "regions");
	_display.set_drag_column (_columns.name.index ());

	/* setup DnD handling */

	list<TargetEntry> region_list_target_table;

	region_list_target_table.push_back (TargetEntry ("text/uri-list"));
	region_list_target_table.push_back (TargetEntry ("text/plain"));
	region_list_target_table.push_back (TargetEntry ("application/x-rootwin-drop"));

	_display.add_drop_targets (region_list_target_table);
	_display.signal_drag_data_received ().connect (sigc::mem_fun (*this, &EditorRegions::drag_data_received));

	_scroller.add (_display);
	_scroller.set_policy (POLICY_AUTOMATIC, POLICY_AUTOMATIC);

	_display.signal_button_press_event ().connect (sigc::mem_fun (*this, &EditorRegions::button_press), false);
	_change_connection = _display.get_selection ()->signal_changed ().connect (sigc::mem_fun (*this, &EditorRegions::selection_changed));

	_scroller.signal_key_press_event ().connect (sigc::mem_fun (*this, &EditorRegions::key_press), false);
	_scroller.signal_focus_in_event ().connect (sigc::mem_fun (*this, &EditorRegions::focus_in), false);
	_scroller.signal_focus_out_event ().connect (sigc::mem_fun (*this, &EditorRegions::focus_out));

	_display.signal_enter_notify_event ().connect (sigc::mem_fun (*this, &EditorRegions::enter_notify), false);
	_display.signal_leave_notify_event ().connect (sigc::mem_fun (*this, &EditorRegions::leave_notify), false);

	ARDOUR_UI::instance ()->primary_clock->mode_changed.connect (sigc::mem_fun (*this, &EditorRegions::clock_format_changed));

	e->EditorFreeze.connect (editor_freeze_connection, MISSING_INVALIDATOR, boost::bind (&EditorRegions::freeze_tree_model, this), gui_context ());
	e->EditorThaw.connect (editor_thaw_connection, MISSING_INVALIDATOR, boost::bind (&EditorRegions::thaw_tree_model, this), gui_context ());
}

bool
EditorRegions::focus_in (GdkEventFocus*)
{
	Window* win = dynamic_cast<Window*> (_scroller.get_toplevel ());

	if (win) {
		old_focus = win->get_focus ();
	} else {
		old_focus = 0;
	}

	name_editable = 0;
	tags_editable = 0;

	/* try to do nothing on focus in (doesn't work, hence selection_count nonsense) */
	return true;
}

bool
EditorRegions::focus_out (GdkEventFocus*)
{
	if (old_focus) {
		old_focus->grab_focus ();
		old_focus = 0;
	}

	name_editable = 0;
	tags_editable = 0;

	return false;
}

bool
EditorRegions::enter_notify (GdkEventCrossing*)
{
	if (name_editable || tags_editable) {
		return true;
	}

	Keyboard::magic_widget_grab_focus ();
	return false;
}

bool
EditorRegions::leave_notify (GdkEventCrossing*)
{
	if (old_focus) {
		old_focus->grab_focus ();
		old_focus = 0;
	}

	Keyboard::magic_widget_drop_focus ();
	return false;
}

void
EditorRegions::set_session (ARDOUR::Session* s)
{
	SessionHandlePtr::set_session (s);

	ARDOUR::Region::RegionsPropertyChanged.connect (region_property_connection, MISSING_INVALIDATOR, boost::bind (&EditorRegions::regions_changed, this, _1, _2), gui_context ());
	ARDOUR::RegionFactory::CheckNewRegion.connect (check_new_region_connection, MISSING_INVALIDATOR, boost::bind (&EditorRegions::add_region, this, _1), gui_context ());

	redisplay ();
}

void
EditorRegions::add_region (boost::shared_ptr<Region> region)
{
	if (!region || !_session) {
		return;
	}

	/* whole-file regions are shown in the Source List */
	if (region->whole_file ()) {
		return;
	}

	/* we only show files-on-disk.
	 * if there's some other kind of region, we ignore it (for now)
	 */
	boost::shared_ptr<FileSource> fs = boost::dynamic_pointer_cast<FileSource> (region->source());
	if (!fs || fs->empty()) {
		return;
	}

	PropertyChange pc;
	boost::shared_ptr<RegionList> rl (new RegionList);
	rl->push_back (region);
	regions_changed (rl, pc);
}

void
EditorRegions::destroy_region (boost::shared_ptr<ARDOUR::Region> region)
{
	//UNTESTED
	//At the time of writing, the only way to remove regions is "cleanup"
	//by definition, "cleanup" only removes regions that aren't on the timeline
	//so this would be a no-op anyway
	//perhaps someday we will allow users to manually destroy regions.
	RegionRowMap::iterator map_it = region_row_map.find (region);
	if (map_it != region_row_map.end ()) {
		region_row_map.erase (map_it);
		_model->erase (map_it->second);
	}
}

void
EditorRegions::remove_unused_regions ()
{
	vector<string> choices;
	string         prompt;

	if (!_session) {
		return;
	}

	prompt = _ ("Do you really want to remove unused regions?"
	            "\n(This is destructive and cannot be undone)");

	choices.push_back (_ ("No, do nothing."));
	choices.push_back (_ ("Yes, remove."));

	ArdourWidgets::Choice prompter (_ ("Remove unused regions"), prompt, choices);

	if (prompter.run () == 1) {
		_no_redisplay = true;
		_session->cleanup_regions ();
		_no_redisplay = false;
		redisplay ();
	}
}

void
EditorRegions::regions_changed (boost::shared_ptr<RegionList> rl, const PropertyChange& what_changed)
{
	bool freeze = rl->size () > 2;
	if (freeze) {
		freeze_tree_model ();
	}
	for (RegionList::const_iterator i = rl->begin (); i != rl->end(); ++i) {
		boost::shared_ptr<Region> r = *i;

		RegionRowMap::iterator map_it = region_row_map.find (r);

		boost::shared_ptr<ARDOUR::Playlist> pl = r->playlist ();
		if (!(pl && _session && _session->playlist_is_active (pl))) {
			/* this region is not on an active playlist
			 * maybe it got deleted, or whatever */
			if (map_it != region_row_map.end ()) {
				Gtk::TreeModel::iterator r = map_it->second;
				region_row_map.erase (map_it);
				_model->erase (r);
			}
			break;
		}

		if (map_it != region_row_map.end ()) {
			/* found the region, update its row properties */
			TreeModel::Row row = *(map_it->second);
			populate_row (r, row, what_changed);

		} else {
			/* new region, add it to the list */
			TreeModel::iterator iter = _model->append ();
			TreeModel::Row      row  = *iter;
			region_row_map.insert (pair<boost::shared_ptr<ARDOUR::Region>, Gtk::TreeModel::iterator> (r, iter));

			/* set the properties that don't change */
			row[_columns.region] = r;

			/* now populate the properties that might change... */
			populate_row (r, row, PropertyChange ());
		}
	}
	if (freeze) {
		thaw_tree_model ();
	}
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
EditorRegions::redisplay ()
{
	if (_no_redisplay || !_session) {
		return;
	}

	/* store sort column id and type for later */
	_model->get_sort_column_id (_sort_col_id, _sort_type);

	_display.set_model (Glib::RefPtr<Gtk::TreeStore> (0));
	_model->clear ();
	/* Disable sorting to gain performance */
	_model->set_sort_column (-2, SORT_ASCENDING);

	region_row_map.clear ();

	RegionFactory::foreach_region (sigc::mem_fun (*this, &EditorRegions::add_region));

	_model->set_sort_column (_sort_col_id, _sort_type); // re-enabale sorting
	_display.set_model (_model);
}

void
EditorRegions::update_row (boost::shared_ptr<Region> region)
{
	if (!region || !_session) {
		return;
	}

	RegionRowMap::iterator it;

	it = region_row_map.find (region);

	if (it != region_row_map.end ()) {
		PropertyChange      c;
		TreeModel::iterator j = it->second;
		populate_row (region, (*j), c);
	}
}

void
EditorRegions::clock_format_changed ()
{
	if (!_session) {
		return;
	}

	PropertyChange change;
	change.add (ARDOUR::Properties::start);
	change.add (ARDOUR::Properties::length);
	change.add (ARDOUR::Properties::sync_position);
	change.add (ARDOUR::Properties::fade_in);
	change.add (ARDOUR::Properties::fade_out);

	RegionRowMap::iterator i;

	for (i = region_row_map.begin (); i != region_row_map.end (); ++i) {
		TreeModel::iterator j = i->second;

		boost::shared_ptr<Region> region = (*j)[_columns.region];

		populate_row (region, (*j), change);
	}
}

void
EditorRegions::format_position (timepos_t const & p, char* buf, size_t bufsize, bool onoff)
{
	Temporal::BBT_Time bbt;
	Timecode::Time     timecode;
	samplepos_t pos (p.samples());

	if (pos < 0) {
		error << string_compose (_ ("EditorRegions::format_position: negative timecode position: %1"), pos) << endmsg;
		snprintf (buf, bufsize, "invalid");
		return;
	}

	switch (ARDOUR_UI::instance ()->primary_clock->mode ()) {
		case AudioClock::BBT:
			bbt = Temporal::TempoMap::use()->bbt_at (p);
			if (onoff) {
				snprintf (buf, bufsize, "%03d|%02d|%04d", bbt.bars, bbt.beats, bbt.ticks);
			} else {
				snprintf (buf, bufsize, "(%03d|%02d|%04d)", bbt.bars, bbt.beats, bbt.ticks);
			}
			break;

		case AudioClock::MinSec:
			samplepos_t left;
			int         hrs;
			int         mins;
			float       secs;

			left = pos;
			hrs  = (int)floor (left / (_session->sample_rate () * 60.0f * 60.0f));
			left -= (samplecnt_t)floor (hrs * _session->sample_rate () * 60.0f * 60.0f);
			mins = (int)floor (left / (_session->sample_rate () * 60.0f));
			left -= (samplecnt_t)floor (mins * _session->sample_rate () * 60.0f);
			secs = left / (float)_session->sample_rate ();
			if (onoff) {
				snprintf (buf, bufsize, "%02d:%02d:%06.3f", hrs, mins, secs);
			} else {
				snprintf (buf, bufsize, "(%02d:%02d:%06.3f)", hrs, mins, secs);
			}
			break;

		case AudioClock::Seconds:
			if (onoff) {
				snprintf (buf, bufsize, "%.1f", pos / (float)_session->sample_rate ());
			} else {
				snprintf (buf, bufsize, "(%.1f)", pos / (float)_session->sample_rate ());
			}
			break;

		case AudioClock::Samples:
			if (onoff) {
				snprintf (buf, bufsize, "%" PRId64, pos);
			} else {
				snprintf (buf, bufsize, "(%" PRId64 ")", pos);
			}
			break;

		case AudioClock::Timecode:
		default:
			_session->timecode_time (pos, timecode);
			if (onoff) {
				snprintf (buf, bufsize, "%02d:%02d:%02d:%02d", timecode.hours, timecode.minutes, timecode.seconds, timecode.frames);
			} else {
				snprintf (buf, bufsize, "(%02d:%02d:%02d:%02d)", timecode.hours, timecode.minutes, timecode.seconds, timecode.frames);
			}
			break;
	}
}

void
EditorRegions::populate_row (boost::shared_ptr<Region> region, TreeModel::Row const& row, PBD::PropertyChange const& what_changed)
{
	/* the grid is most interested in the regions that are *visible* in the editor.
	 * this is a convenient place to flag changes to the grid cache, on a visible region */
	PropertyChange grid_interests;
	grid_interests.add (ARDOUR::Properties::length);
	grid_interests.add (ARDOUR::Properties::sync_position);

	if (what_changed.contains (grid_interests)) {
		_editor->mark_region_boundary_cache_dirty ();
	}

	{
		Gdk::Color c;
		bool       missing_source = boost::dynamic_pointer_cast<SilentFileSource> (region->source ()) != NULL;
		if (missing_source) {
			set_color_from_rgba (c, UIConfiguration::instance ().color ("region list missing source"));
		} else {
			set_color_from_rgba (c, UIConfiguration::instance ().color ("region list whole file"));
		}
		row[_columns.color_] = c;
	}

	boost::shared_ptr<AudioRegion> audioregion = boost::dynamic_pointer_cast<AudioRegion> (region);

	PropertyChange c;
	const bool     all = what_changed == c;

	if (all || what_changed.contains (Properties::length)) {
		populate_row_position (region, row);
	}
	if (all || what_changed.contains (Properties::start) || what_changed.contains (Properties::sync_position)) {
		populate_row_sync (region, row);
	}
	if (all || what_changed.contains (Properties::fade_in)) {
		populate_row_fade_in (region, row, audioregion);
	}
	if (all || what_changed.contains (Properties::fade_out)) {
		populate_row_fade_out (region, row, audioregion);
	}
	if (all || what_changed.contains (Properties::locked)) {
		populate_row_locked (region, row);
	}
	if (all || what_changed.contains (Properties::time_domain)) {
		populate_row_glued (region, row);
	}
	if (all || what_changed.contains (Properties::muted)) {
		populate_row_muted (region, row);
	}
	if (all || what_changed.contains (Properties::opaque)) {
		populate_row_opaque (region, row);
	}
	if (all || what_changed.contains (Properties::length)) {
		populate_row_end (region, row);
		populate_row_length (region, row);
	}
	if (all) {
		populate_row_source (region, row);
	}
	if (all || what_changed.contains (Properties::name) || what_changed.contains (Properties::tags)) {
		populate_row_name (region, row);
	}
}

#if 0
	if (audioRegion && fades_in_seconds) {

		samplepos_t left;
		int mins;
		int millisecs;

		left = audioRegion->fade_in()->back()->when;
		mins = (int) floor (left / (_session->sample_rate() * 60.0f));
		left -= (samplepos_t) floor (mins * _session->sample_rate() * 60.0f);
		millisecs = (int) floor ((left * 1000.0f) / _session->sample_rate());

		if (audioRegion->fade_in()->back()->when >= _session->sample_rate()) {
			sprintf (fadein_str, "%01dM %01dmS", mins, millisecs);
		} else {
			sprintf (fadein_str, "%01dmS", millisecs);
		}

		left = audioRegion->fade_out()->back()->when;
		mins = (int) floor (left / (_session->sample_rate() * 60.0f));
		left -= (samplepos_t) floor (mins * _session->sample_rate() * 60.0f);
		millisecs = (int) floor ((left * 1000.0f) / _session->sample_rate());

		if (audioRegion->fade_out()->back()->when >= _session->sample_rate()) {
			sprintf (fadeout_str, "%01dM %01dmS", mins, millisecs);
		} else {
			sprintf (fadeout_str, "%01dmS", millisecs);
		}
	}
#endif

void
EditorRegions::populate_row_length (boost::shared_ptr<Region> region, TreeModel::Row const& row)
{
	char buf[16];

	if (ARDOUR_UI::instance ()->primary_clock->mode () == AudioClock::BBT) {
		TempoMap::SharedPtr map (TempoMap::use());
		Temporal::BBT_Time bbt; /* uninitialized until full duration works */
		// Temporal::BBT_Time bbt = map->bbt_duration_at (region->position(), region->length());
		snprintf (buf, sizeof (buf), "%03d|%02d|%04d", bbt.bars, bbt.beats, bbt.ticks);
	} else {
		format_position (timepos_t (region->length ()), buf, sizeof (buf));
	}

	row[_columns.length] = buf;
}

void
EditorRegions::populate_row_end (boost::shared_ptr<Region> region, TreeModel::Row const& row)
{
#ifndef SHOW_REGION_EXTRAS
	return;
#endif

	if (region->last_sample () >= region->first_sample ()) {
		char buf[16];
		format_position (region->nt_last (), buf, sizeof (buf));
		row[_columns.end] = buf;
	} else {
		row[_columns.end] = "empty";
	}
}

void
EditorRegions::populate_row_position (boost::shared_ptr<Region> region, TreeModel::Row const& row)
{
	row[_columns.position] = region->position ();

	char buf[16];
	format_position (region->position (), buf, sizeof (buf));
	row[_columns.start] = buf;
}

void
EditorRegions::populate_row_sync (boost::shared_ptr<Region> region, TreeModel::Row const& row)
{
#ifndef SHOW_REGION_EXTRAS
	return;
#endif
	if (region->sync_position () == region->position ()) {
		row[_columns.sync] = _ ("Start");
	} else if (region->sync_position () == (region->last_sample ())) {
		row[_columns.sync] = _ ("End");
	} else {
		char buf[16];
		format_position (region->sync_position (), buf, sizeof (buf));
		row[_columns.sync] = buf;
	}
}

void
EditorRegions::populate_row_fade_in (boost::shared_ptr<Region> region, TreeModel::Row const& row, boost::shared_ptr<AudioRegion> audioregion)
{
#ifndef SHOW_REGION_EXTRAS
	return;
#endif
	if (!audioregion) {
		row[_columns.fadein] = "";
	} else {
		char buf[32];
		format_position (audioregion->fade_in ()->back ()->when, buf, sizeof (buf), audioregion->fade_in_active ());
		row[_columns.fadein] = buf;
	}
}

void
EditorRegions::populate_row_fade_out (boost::shared_ptr<Region> region, TreeModel::Row const& row, boost::shared_ptr<AudioRegion> audioregion)
{
#ifndef SHOW_REGION_EXTRAS
	return;
#endif
	if (!audioregion) {
		row[_columns.fadeout] = "";
	} else {
		char buf[32];
		format_position (audioregion->fade_out ()->back ()->when, buf, sizeof (buf), audioregion->fade_out_active ());
		row[_columns.fadeout] = buf;
	}
}

void
EditorRegions::populate_row_locked (boost::shared_ptr<Region> region, TreeModel::Row const& row)
{
	row[_columns.locked] = region->locked ();
}

void
EditorRegions::populate_row_glued (boost::shared_ptr<Region> region, TreeModel::Row const& row)
{
	if (region->position_time_domain () == Temporal::BeatTime) {
		row[_columns.glued] = true;
	} else {
		row[_columns.glued] = false;
	}
}

void
EditorRegions::populate_row_muted (boost::shared_ptr<Region> region, TreeModel::Row const& row)
{
	row[_columns.muted] = region->muted ();
}

void
EditorRegions::populate_row_opaque (boost::shared_ptr<Region> region, TreeModel::Row const& row)
{
	row[_columns.opaque] = region->opaque ();
}

void
EditorRegions::populate_row_name (boost::shared_ptr<Region> region, TreeModel::Row const& row)
{
	row[_columns.name] = Gtkmm2ext::markup_escape_text (region->name ());

	if (region->data_type() == DataType::MIDI) {
		row[_columns.channels] = 0;  /*TODO: some better recognition of midi regions*/
	} else {
		row[_columns.channels] = region->sources().size();
	}

	row[_columns.tags] = region->tags ();
}

void
EditorRegions::populate_row_source (boost::shared_ptr<Region> region, TreeModel::Row const& row)
{
	if (boost::dynamic_pointer_cast<SilentFileSource> (region->source ())) {
		row[_columns.path] = _ ("MISSING ") + Gtkmm2ext::markup_escape_text (region->source ()->name ());
	} else {
		row[_columns.path] = Gtkmm2ext::markup_escape_text (region->source ()->name ());
	}
}

void
EditorRegions::show_context_menu (int button, int time)
{
	using namespace Gtk::Menu_Helpers;
	Gtk::Menu* menu = dynamic_cast<Menu*> (ActionManager::get_widget (X_ ("/PopupRegionMenu")));
	menu->popup (button, time);
}

bool
EditorRegions::key_press (GdkEventKey* ev)
{
	TreeViewColumn* col;

	switch (ev->keyval) {
		case GDK_Tab:
		case GDK_ISO_Left_Tab:

			if (name_editable) {
				name_editable->editing_done ();
				name_editable = 0;
			}

			if (tags_editable) {
				tags_editable->editing_done ();
				tags_editable = 0;
			}

			col = _display.get_column (0); // select&focus on name column

			if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
				treeview_select_previous (_display, _model, col);
			} else {
				treeview_select_next (_display, _model, col);
			}

			return true;
			break;

		default:
			break;
	}

	return false;
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
EditorRegions::selection_mapover (sigc::slot<void, boost::shared_ptr<Region> > sl)
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
EditorRegions::drag_data_received (const RefPtr<Gdk::DragContext>& context,
                                   int x, int y,
                                   const SelectionData& data,
                                   guint info, guint dtime)
{
	vector<string> paths;

	if (data.get_target () == "GTK_TREE_MODEL_ROW") {
		/* something is being dragged over the region list */
		_editor->_drags->abort ();
		_display.on_drag_data_received (context, x, y, data, info, dtime);
		return;
	}

	if (_session && convert_drop_to_paths (paths, data)) {
		timepos_t pos;
		bool      copy = ((context->get_actions () & (Gdk::ACTION_COPY | Gdk::ACTION_LINK | Gdk::ACTION_MOVE)) == Gdk::ACTION_COPY);

		if (UIConfiguration::instance ().get_only_copy_imported_files () || copy) {
			_editor->do_import (paths, Editing::ImportDistinctFiles, Editing::ImportAsRegion,
			                    SrcBest, SMFTrackName, SMFTempoIgnore, pos);
		} else {
			_editor->do_embed (paths, Editing::ImportDistinctFiles, ImportAsRegion, pos);
		}
		context->drag_finish (true, false, dtime);
	}
}

bool
EditorRegions::selection_filter (const RefPtr<TreeModel>& model, const TreeModel::Path& path, bool already_selected)
{
	if (already_selected) {
		/* deselecting path, if it is selected, is OK */
		return true;
	}

	/* not possible to select rows that do not represent regions, like "Hidden" */
	TreeModel::iterator iter = model->get_iter (path);
	if (iter) {
		boost::shared_ptr<Region> r = (*iter)[_columns.region];
		if (!r) {
			return false;
		}
	}

	return true;
}

void
EditorRegions::name_editing_started (CellEditable* ce, const Glib::ustring& path)
{
	name_editable = ce;

	/* give it a special name */

	Gtk::Entry* e = dynamic_cast<Gtk::Entry*> (ce);

	if (e) {
		e->set_name (X_ ("RegionNameEditorEntry"));

		TreeIter iter;
		if ((iter = _model->get_iter (path))) {
			boost::shared_ptr<Region> region = (*iter)[_columns.region];

			if (region) {
				e->set_text (region->name ());
			}
		}
	}
}

void
EditorRegions::name_edit (const std::string& path, const std::string& new_text)
{
	name_editable = 0;

	boost::shared_ptr<Region> region;
	TreeIter                  row_iter;

	if ((row_iter = _model->get_iter (path))) {
		region                     = (*row_iter)[_columns.region];
		(*row_iter)[_columns.name] = new_text;
	}

	if (region) {
		region->set_name (new_text);

		populate_row_name (region, (*row_iter));
	}
}

void
EditorRegions::tag_editing_started (CellEditable* ce, const Glib::ustring& path)
{
	tags_editable = ce;

	/* give it a special name */

	Gtk::Entry* e = dynamic_cast<Gtk::Entry*> (ce);

	if (e) {
		e->set_name (X_ ("RegionTagEditorEntry"));

		TreeIter iter;
		if ((iter = _model->get_iter (path))) {
			boost::shared_ptr<Region> region = (*iter)[_columns.region];

			if (region) {
				e->set_text (region->tags ());
			}
		}
	}
}

void
EditorRegions::tag_edit (const std::string& path, const std::string& new_text)
{
	tags_editable = 0;

	boost::shared_ptr<Region> region;
	TreeIter                  row_iter;

	if ((row_iter = _model->get_iter (path))) {
		region                     = (*row_iter)[_columns.region];
		(*row_iter)[_columns.tags] = new_text;
	}

	if (region) {
		region->set_tags (new_text);

		populate_row_name (region, (*row_iter));
	}
}

/** @return Region that has been dragged out of the list, or 0 */
boost::shared_ptr<Region>
EditorRegions::get_dragged_region ()
{
	list<boost::shared_ptr<Region> > regions;
	TreeView*                        source;
	_display.get_object_drag_data (regions, &source);

	if (regions.empty ()) {
		return boost::shared_ptr<Region> ();
	}

	return regions.front ();
}

void
EditorRegions::clear ()
{
	_display.set_model (Glib::RefPtr<Gtk::TreeStore> (0));
	_model->clear ();
	_display.set_model (_model);

	/* Clean up the maps */
	region_row_map.clear ();
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
EditorRegions::freeze_tree_model ()
{
	/* store sort column id and type for later */
	_model->get_sort_column_id (_sort_col_id, _sort_type);
	_change_connection.block (true);
	_display.set_model (Glib::RefPtr<Gtk::TreeStore> (0));
	_model->set_sort_column (-2, SORT_ASCENDING); //Disable sorting to gain performance
}

void
EditorRegions::thaw_tree_model ()
{
	_model->set_sort_column (_sort_col_id, _sort_type); // re-enabale sorting
	_display.set_model (_model);
	_change_connection.block (false);
}

void
EditorRegions::locked_changed (std::string const& path)
{
	TreeIter i = _model->get_iter (path);
	if (i) {
		boost::shared_ptr<ARDOUR::Region> region = (*i)[_columns.region];
		if (region) {
			region->set_locked (!(*i)[_columns.locked]);
		}
	}
}

void
EditorRegions::glued_changed (std::string const& path)
{
	TreeIter i = _model->get_iter (path);
	if (i) {
		boost::shared_ptr<ARDOUR::Region> region = (*i)[_columns.region];
		if (region) {
			/* `glued' means MusicTime, and we're toggling here */
			region->set_position_time_domain ((*i)[_columns.glued] ? Temporal::AudioTime : Temporal::BeatTime);
		}
	}
}

void
EditorRegions::muted_changed (std::string const& path)
{
	TreeIter i = _model->get_iter (path);
	if (i) {
		boost::shared_ptr<ARDOUR::Region> region = (*i)[_columns.region];
		if (region) {
			region->set_muted (!(*i)[_columns.muted]);
		}
	}
}

void
EditorRegions::opaque_changed (std::string const& path)
{
	TreeIter i = _model->get_iter (path);
	if (i) {
		boost::shared_ptr<ARDOUR::Region> region = (*i)[_columns.region];
		if (region) {
			region->set_opaque (!(*i)[_columns.opaque]);
		}
	}
}

XMLNode&
EditorRegions::get_state () const
{
	XMLNode* node = new XMLNode (X_ ("RegionList"));

	//TODO:  save sort state?
	//	node->set_property (X_("sort-col"), _sort_type);
	//	node->set_property (X_("sort-asc"), _sort_type);

	return *node;
}

void
EditorRegions::set_state (const XMLNode& node)
{
	bool changed = false;

	if (node.name () != X_ ("RegionList")) {
		return;
	}

	if (changed) {
		redisplay ();
	}
}

RefPtr<Action>
EditorRegions::remove_unused_regions_action () const
{
	return ActionManager::get_action (X_ ("RegionList"), X_ ("removeUnusedRegions"));
}
