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
#include <string>

#include "pbd/file_utils.h"

#include "ardour/audiofilesource.h"
#include "ardour/audioregion.h"
#include "ardour/midi_source.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/session_directory.h"
#include "ardour/session_playlist.h"
#include "ardour/silentfilesource.h"
#include "ardour/smf_source.h"

#include "gtkmm2ext/treeutils.h"
#include "gtkmm2ext/utils.h"

#include "widgets/tooltips.h"

#include "actions.h"
#include "ardour_ui.h"
#include "audio_clock.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "main_clock.h"
#include "public_editor.h"
#include "region_list_base.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtk;
using namespace Temporal;

using Gtkmm2ext::Keyboard;

RegionListBase::RegionListBase ()
	: _name_editable (0)
	, _tags_editable (0)
	, _old_focus (0)
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

	_display.add_object_drag (-1, "x-ardour/region.pbdid", Gtk::TARGET_SAME_APP);
	_display.set_drag_column (_columns.name.index ());
	_display.signal_drag_begin ().connect (sigc::mem_fun (*this, &RegionListBase::drag_begin));
	_display.signal_drag_end ().connect (sigc::mem_fun (*this, &RegionListBase::drag_end));
	_display.signal_drag_data_get ().connect (sigc::mem_fun (*this, &RegionListBase::drag_data_get));

	_display.set_model (_model);

	_display.set_headers_visible (true);
	_display.set_rules_hint ();

	if (UIConfiguration::instance ().get_use_tooltips ()) {
		/* show path as the row tooltip */
		_display.set_tooltip_column (13); /* path */
	}

	_display.get_selection ()->set_mode (SELECTION_MULTIPLE);

	_scroller.add (_display);
	_scroller.set_policy (POLICY_AUTOMATIC, POLICY_AUTOMATIC);

	_display.signal_button_press_event ().connect (sigc::mem_fun (*this, &RegionListBase::button_press), false);
	_display.signal_enter_notify_event ().connect (sigc::mem_fun (*this, &RegionListBase::enter_notify), false);
	_display.signal_leave_notify_event ().connect (sigc::mem_fun (*this, &RegionListBase::leave_notify), false);
	_scroller.signal_focus_in_event ().connect (sigc::mem_fun (*this, &RegionListBase::focus_in), false);
	_scroller.signal_focus_out_event ().connect (sigc::mem_fun (*this, &RegionListBase::focus_out));
	_scroller.signal_key_press_event ().connect (sigc::mem_fun (*this, &RegionListBase::key_press), false);

	ARDOUR_UI::instance ()->primary_clock->mode_changed.connect (sigc::mem_fun (*this, &RegionListBase::clock_format_changed));
}

void
RegionListBase::setup_col (TreeViewColumn* col, int sort_idx, Gtk::AlignmentEnum al, const char* label, const char* tooltip)
{
	/* add the label */
	Gtk::Label* l = manage (new Label (label));
	l->set_alignment (al);
	ArdourWidgets::set_tooltip (*l, tooltip);
	col->set_widget (*l);
	l->show ();

	col->set_sort_column (sort_idx);
	col->set_expand (false);

	/* this sets the alignment of the column header... */
	col->set_alignment (al);

	/* ...and this sets the alignment for the data cells */
	CellRendererText* renderer = dynamic_cast<CellRendererText*> (col->get_first_cell_renderer ());
	if (renderer) {
		renderer->property_xalign () = (al == ALIGN_RIGHT ? 1.0 : (al == ALIGN_LEFT ? 0.0 : 0.5));
	}
}

void
RegionListBase::setup_toggle (Gtk::TreeViewColumn* tvc, sigc::slot<void, std::string> cb)
{
	CellRendererToggle* tc      = dynamic_cast<CellRendererToggle*> (tvc->get_first_cell_renderer ());
	tc->property_activatable () = true;
	tc->signal_toggled ().connect (cb);
}

void
RegionListBase::add_name_column ()
{
	TreeViewColumn* tvc = append_col (_columns.name, 120);
	setup_col (tvc, 0, ALIGN_LEFT, _("Name"), ("Region name"));

	/* Region Name: make editable */
	CellRendererText* region_name_cell     = dynamic_cast<CellRendererText*> (tvc->get_first_cell_renderer ());
	region_name_cell->property_editable () = true;
	region_name_cell->signal_edited ().connect (sigc::mem_fun (*this, &RegionListBase::name_edit));
	region_name_cell->signal_editing_started ().connect (sigc::mem_fun (*this, &RegionListBase::name_editing_started));
	/* Region Name: color turns red if source is missing. */
	tvc->add_attribute (region_name_cell->property_text (), _columns.name);
	tvc->add_attribute (region_name_cell->property_foreground_gdk (), _columns.color_);
	tvc->set_expand (true);
}

void
RegionListBase::add_tag_column ()
{
	TreeViewColumn* tvc = append_col (_columns.tags, "2099-10-10 10:10:30");
	setup_col (tvc, 2, ALIGN_LEFT, _("Tags"), _("Tags"));

	/* Tags cell: make editable */
	CellRendererText* region_tags_cell     = dynamic_cast<CellRendererText*> (tvc->get_first_cell_renderer ());
	region_tags_cell->property_editable () = true;
	region_tags_cell->signal_edited ().connect (sigc::mem_fun (*this, &RegionListBase::tag_edit));
	region_tags_cell->signal_editing_started ().connect (sigc::mem_fun (*this, &RegionListBase::tag_editing_started));
}

bool
RegionListBase::focus_in (GdkEventFocus*)
{
	Window* win = dynamic_cast<Window*> (_scroller.get_toplevel ());

	if (win) {
		_old_focus = win->get_focus ();
	} else {
		_old_focus = 0;
	}

	_tags_editable = 0;
	_name_editable = 0;

	/* try to do nothing on focus in (doesn't work, hence selection_count nonsense) */
	return true;
}

bool
RegionListBase::focus_out (GdkEventFocus*)
{
	if (_old_focus) {
		_old_focus->grab_focus ();
		_old_focus = 0;
	}

	_tags_editable = 0;
	_name_editable = 0;

	return false;
}

bool
RegionListBase::enter_notify (GdkEventCrossing*)
{
	if (_name_editable || _tags_editable) {
		return true;
	}

	Keyboard::magic_widget_grab_focus ();
	return false;
}

bool
RegionListBase::leave_notify (GdkEventCrossing*)
{
	if (_old_focus) {
		_old_focus->grab_focus ();
		_old_focus = 0;
	}
	Keyboard::magic_widget_drop_focus ();
	return false;
}

void
RegionListBase::drag_begin (Glib::RefPtr<Gdk::DragContext> const&)
{
	if (_display.get_selection ()->count_selected_rows () == 0) {
		PublicEditor::instance ().pbdid_dragged_dt = DataType::NIL;
	}
	TreeView::Selection::ListHandle_Path rows = _display.get_selection ()->get_selected_rows ();
	for (TreeView::Selection::ListHandle_Path::iterator i = rows.begin (); i != rows.end (); ++i) {
		boost::shared_ptr<Region> region           = (*_model->get_iter (*i))[_columns.region];
		PublicEditor::instance ().pbdid_dragged_dt = region->data_type ();
		break;
	}
}

void
RegionListBase::drag_end (Glib::RefPtr<Gdk::DragContext> const&)
{
	PublicEditor::instance ().pbdid_dragged_dt = DataType::NIL;
}

void
RegionListBase::drag_data_get (Glib::RefPtr<Gdk::DragContext> const&, Gtk::SelectionData& data, guint, guint)
{
	if (data.get_target () != "x-ardour/region.pbdid") {
		return;
	}
	TreeView::Selection::ListHandle_Path rows = _display.get_selection ()->get_selected_rows ();
	for (TreeView::Selection::ListHandle_Path::iterator i = rows.begin (); i != rows.end (); ++i) {
		boost::shared_ptr<Region> region = (*_model->get_iter (*i))[_columns.region];
		data.set (data.get_target (), region->id ().to_s ());
		break;
	}
}

void
RegionListBase::set_session (ARDOUR::Session* s)
{
	SessionHandlePtr::set_session (s);

	if (!s) {
		clear ();
		return;
	}

	ARDOUR::Region::RegionsPropertyChanged.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&RegionListBase::regions_changed, this, _1, _2), gui_context ());
	ARDOUR::RegionFactory::CheckNewRegion.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&RegionListBase::add_region, this, _1), gui_context ());

	redisplay ();
}

void
RegionListBase::remove_weak_region (boost::weak_ptr<ARDOUR::Region> r)
{
	boost::shared_ptr<ARDOUR::Region> region = r.lock ();
	if (!region) {
		return;
	}

	RegionRowMap::iterator map_it = region_row_map.find (region);
	if (map_it != region_row_map.end ()) {
		Gtk::TreeModel::iterator r_it = map_it->second;
		region_row_map.erase (map_it);
		_model->erase (r_it);
	}
}

bool
RegionListBase::list_region (boost::shared_ptr<ARDOUR::Region> region) const
{
	/* whole-file regions are shown in the Source List */
	return !region->whole_file ();
}

void
RegionListBase::add_region (boost::shared_ptr<Region> region)
{
	if (!region || !_session || !list_region (region)) {
		return;
	}

	/* we only show files-on-disk.
	 * if there's some other kind of region, we ignore it (for now)
	 */
	boost::shared_ptr<FileSource> fs = boost::dynamic_pointer_cast<FileSource> (region->source ());
	if (!fs) {
		return;
	}

	if (fs->empty ()) {
		/* MIDI sources are allowed to be empty */
		if (!boost::dynamic_pointer_cast<MidiSource> (region->source ())) {
			return;
		}
	}

	if (region->whole_file ()) {
		region->DropReferences.connect (_remove_region_connections, MISSING_INVALIDATOR, boost::bind (&RegionListBase::remove_weak_region, this, boost::weak_ptr<Region> (region)), gui_context ());
	}

	PropertyChange                pc;
	boost::shared_ptr<RegionList> rl (new RegionList);
	rl->push_back (region);
	regions_changed (rl, pc);
}

void
RegionListBase::regions_changed (boost::shared_ptr<RegionList> rl, const PropertyChange& what_changed)
{
	bool freeze = rl->size () > 2;
	if (freeze) {
		freeze_tree_model ();
	}
	for (RegionList::const_iterator i = rl->begin (); i != rl->end (); ++i) {
		boost::shared_ptr<Region> r = *i;

		RegionRowMap::iterator              map_it = region_row_map.find (r);
		boost::shared_ptr<ARDOUR::Playlist> pl     = r->playlist ();

		bool is_on_active_playlist = pl && _session && _session->playlist_is_active (pl);

		if (!((is_on_active_playlist || r->whole_file ()) && list_region (r))) {
			/* this region is not on an active playlist
			 * maybe it got deleted, or whatever */
			if (map_it != region_row_map.end ()) {
				Gtk::TreeModel::iterator r_it = map_it->second;
				region_row_map.erase (map_it);
				_model->erase (r_it);
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
RegionListBase::redisplay ()
{
	if (_no_redisplay || !_session) {
		return;
	}

	/* store sort column id and type for later */
	_model->get_sort_column_id (_sort_col_id, _sort_type);

	_remove_region_connections.drop_connections ();

	_display.set_model (Glib::RefPtr<Gtk::TreeStore> (0));
	_model->clear ();
	/* Disable sorting to gain performance */
	_model->set_sort_column (-2, SORT_ASCENDING);

	region_row_map.clear ();

	RegionFactory::foreach_region (sigc::mem_fun (*this, &RegionListBase::add_region));

	/* re-enabale sorting */
	_model->set_sort_column (_sort_col_id, _sort_type);
	_display.set_model (_model);
}

void
RegionListBase::clock_format_changed ()
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

	TreeModel::Children rows = _model->children ();
	for (TreeModel::iterator i = rows.begin (); i != rows.end (); ++i) {
		boost::shared_ptr<ARDOUR::Region> r = (*i)[_columns.region];
		populate_row (r, *i, change);
	}
}

void
RegionListBase::format_position (timepos_t const& p, char* buf, size_t bufsize, bool onoff)
{
	Temporal::BBT_Time bbt;
	Timecode::Time     timecode;
	samplepos_t        pos (p.samples ());

	if (pos < 0) {
		error << string_compose (_("RegionListBase::format_position: negative timecode position: %1"), pos) << endmsg;
		snprintf (buf, bufsize, "invalid");
		return;
	}

	switch (ARDOUR_UI::instance ()->primary_clock->mode ()) {
		case AudioClock::BBT:
			bbt = Temporal::TempoMap::use ()->bbt_at (p);
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
RegionListBase::populate_row (boost::shared_ptr<Region> region, TreeModel::Row const& row, PBD::PropertyChange const& what_changed)
{
	assert (region);

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
	/* CAPTURED DROPOUTS */
	row[_columns.captd_xruns] = region->source ()->n_captured_xruns ();
}

void
RegionListBase::populate_row_length (boost::shared_ptr<Region> region, TreeModel::Row const& row)
{
	char buf[16];

	if (ARDOUR_UI::instance ()->primary_clock->mode () == AudioClock::BBT) {
		TempoMap::SharedPtr map (TempoMap::use ());
		Temporal::BBT_Time  bbt; /* uninitialized until full duration works */
		// Temporal::BBT_Time bbt = map->bbt_duration_at (region->position(), region->length());
		snprintf (buf, sizeof (buf), "%03d|%02d|%04d", bbt.bars, bbt.beats, bbt.ticks);
	} else {
		format_position (timepos_t (region->length ()), buf, sizeof (buf));
	}

	row[_columns.length] = buf;
}

void
RegionListBase::populate_row_end (boost::shared_ptr<Region> region, TreeModel::Row const& row)
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
RegionListBase::populate_row_position (boost::shared_ptr<Region> region, TreeModel::Row const& row)
{
	row[_columns.position] = region->position ();

	char buf[16];
	format_position (region->position (), buf, sizeof (buf));
	row[_columns.start] = buf;
}

void
RegionListBase::populate_row_sync (boost::shared_ptr<Region> region, TreeModel::Row const& row)
{
#ifndef SHOW_REGION_EXTRAS
	return;
#endif
	if (region->sync_position () == region->position ()) {
		row[_columns.sync] = _("Start");
	} else if (region->sync_position () == (region->last_sample ())) {
		row[_columns.sync] = _("End");
	} else {
		char buf[16];
		format_position (region->sync_position (), buf, sizeof (buf));
		row[_columns.sync] = buf;
	}
}

void
RegionListBase::populate_row_fade_in (boost::shared_ptr<Region> region, TreeModel::Row const& row, boost::shared_ptr<AudioRegion> audioregion)
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
RegionListBase::populate_row_fade_out (boost::shared_ptr<Region> region, TreeModel::Row const& row, boost::shared_ptr<AudioRegion> audioregion)
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
RegionListBase::populate_row_locked (boost::shared_ptr<Region> region, TreeModel::Row const& row)
{
	row[_columns.locked] = region->locked ();
}

void
RegionListBase::populate_row_glued (boost::shared_ptr<Region> region, TreeModel::Row const& row)
{
	if (region->position_time_domain () == Temporal::BeatTime) {
		row[_columns.glued] = true;
	} else {
		row[_columns.glued] = false;
	}
}

void
RegionListBase::populate_row_muted (boost::shared_ptr<Region> region, TreeModel::Row const& row)
{
	row[_columns.muted] = region->muted ();
}

void
RegionListBase::populate_row_opaque (boost::shared_ptr<Region> region, TreeModel::Row const& row)
{
	row[_columns.opaque] = region->opaque ();
}

void
RegionListBase::populate_row_name (boost::shared_ptr<Region> region, TreeModel::Row const& row)
{
	row[_columns.name] = Gtkmm2ext::markup_escape_text (region->name ());

	if (region->data_type () == DataType::MIDI) {
		row[_columns.channels] = 0; /*TODO: some better recognition of midi regions*/
	} else {
		row[_columns.channels] = region->sources ().size ();
	}

	row[_columns.tags] = region->tags ();
}

void
RegionListBase::populate_row_source (boost::shared_ptr<Region> region, TreeModel::Row const& row)
{
	boost::shared_ptr<ARDOUR::Source> source = region->source ();
	if (boost::dynamic_pointer_cast<SilentFileSource> (source)) {
		row[_columns.path] = _("MISSING ") + Gtkmm2ext::markup_escape_text (source->name ());
	} else {
		row[_columns.path] = Gtkmm2ext::markup_escape_text (source->name ());

		boost::shared_ptr<FileSource> fs = boost::dynamic_pointer_cast<FileSource> (source);
		if (fs) {
			boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource> (source);
			if (afs) {
				const string audio_directory = _session->session_directory ().sound_path ();
				if (!PBD::path_is_within (audio_directory, fs->path ())) {
					row[_columns.path] = Gtkmm2ext::markup_escape_text (fs->path ());
				}
			}
			boost::shared_ptr<SMFSource> mfs = boost::dynamic_pointer_cast<SMFSource> (source);
			if (mfs) {
				const string midi_directory = _session->session_directory ().midi_path ();
				if (!PBD::path_is_within (midi_directory, fs->path ())) {
					row[_columns.path] = Gtkmm2ext::markup_escape_text (fs->path ());
				}
			}
		}
	}

	row[_columns.captd_for] = source->captured_for ();
	row[_columns.take_id]   = source->take_id ();

	/* Natural Position (samples, an invisible column for sorting) */
	row[_columns.natural_s] = source->natural_position ();

	/* Natural Position (text representation) */
	if (source->have_natural_position ()) {
		char buf[64];
		format_position (source->natural_position (), buf, sizeof (buf));
		row[_columns.natural_pos] = buf;
	} else {
		row[_columns.natural_pos] = X_("--");
	}
}

bool
RegionListBase::key_press (GdkEventKey* ev)
{
	TreeViewColumn* col;

	switch (ev->keyval) {
		case GDK_Tab:
		case GDK_ISO_Left_Tab:

			if (_name_editable) {
				_name_editable->editing_done ();
				_name_editable = 0;
			}

			if (_tags_editable) {
				_tags_editable->editing_done ();
				_tags_editable = 0;
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

void
RegionListBase::name_editing_started (CellEditable* ce, const Glib::ustring& path)
{
	_name_editable = ce;

	/* give it a special name */

	Gtk::Entry* e = dynamic_cast<Gtk::Entry*> (ce);

	if (e) {
		e->set_name (X_("RegionNameEditorEntry"));

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
RegionListBase::name_edit (const std::string& path, const std::string& new_text)
{
	_name_editable = 0;

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
RegionListBase::tag_editing_started (CellEditable* ce, const Glib::ustring& path)
{
	_tags_editable = ce;

	/* give it a special name */

	Gtk::Entry* e = dynamic_cast<Gtk::Entry*> (ce);

	if (e) {
		e->set_name (X_("RegionTagEditorEntry"));

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
RegionListBase::tag_edit (const std::string& path, const std::string& new_text)
{
	_tags_editable = 0;

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

void
RegionListBase::clear ()
{
	_remove_region_connections.drop_connections ();
	_display.set_model (Glib::RefPtr<Gtk::TreeStore> (0));
	_model->clear ();
	_display.set_model (_model);

	/* Clean up the maps */
	region_row_map.clear ();
}

void
RegionListBase::freeze_tree_model ()
{
	/* store sort column id and type for later */
	_model->get_sort_column_id (_sort_col_id, _sort_type);
	_change_connection.block (true);
	_display.set_model (Glib::RefPtr<Gtk::TreeStore> (0));
	_model->set_sort_column (-2, SORT_ASCENDING); //Disable sorting to gain performance
}

void
RegionListBase::thaw_tree_model ()
{
	_model->set_sort_column (_sort_col_id, _sort_type); // re-enabale sorting
	_display.set_model (_model);
	_change_connection.block (false);
}

void
RegionListBase::locked_changed (std::string const& path)
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
RegionListBase::glued_changed (std::string const& path)
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
RegionListBase::muted_changed (std::string const& path)
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
RegionListBase::opaque_changed (std::string const& path)
{
	TreeIter i = _model->get_iter (path);
	if (i) {
		boost::shared_ptr<ARDOUR::Region> region = (*i)[_columns.region];
		if (region) {
			region->set_opaque (!(*i)[_columns.opaque]);
		}
	}
}
