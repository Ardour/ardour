/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#include "export_timespan_selector.h"

#include "ardour_ui.h"

#include "ardour/location.h"
#include "ardour/types.h"
#include "ardour/session.h"
#include "ardour/export_handler.h"
#include "ardour/export_timespan.h"

#include "pbd/enumwriter.h"
#include "pbd/convert.h"

#include <sstream>
#include <iomanip>

#include "i18n.h"

using namespace Glib;
using namespace ARDOUR;
using namespace PBD;
using std::string;

ExportTimespanSelector::ExportTimespanSelector (ARDOUR::Session * session, ProfileManagerPtr manager) :
	manager (manager),
	time_format_label (_("Show Times as:"), Gtk::ALIGN_LEFT)
{
	set_session (session);

	option_hbox.pack_start (time_format_label, false, false, 0);
	option_hbox.pack_start (time_format_combo, false, false, 6);

	Gtk::Button* b = Gtk::manage (new Gtk::Button (_("Select All")));
	b->signal_clicked().connect (
		sigc::bind (
			sigc::mem_fun (*this, &ExportTimespanSelector::set_selection_state_of_all_timespans), true
			)
		);
	option_hbox.pack_start (*b, false, false, 6);
	
	b = Gtk::manage (new Gtk::Button (_("Deselect All")));
	b->signal_clicked().connect (
		sigc::bind (
			sigc::mem_fun (*this, &ExportTimespanSelector::set_selection_state_of_all_timespans), false
			)
		);
	option_hbox.pack_start (*b, false, false, 6);

	range_scroller.add (range_view);

	pack_start (option_hbox, false, false, 0);
	pack_start (range_scroller, true, true, 6);

	/*** Combo boxes ***/

	Gtk::TreeModel::iterator iter;
	Gtk::TreeModel::Row row;

	/* Time format combo */

	time_format_list = Gtk::ListStore::create (time_format_cols);
	time_format_combo.set_model (time_format_list);
	time_format_combo.set_name ("PaddedButton");

	iter = time_format_list->append();
	row = *iter;
	row[time_format_cols.format] = ExportProfileManager::Timecode;
	row[time_format_cols.label] = _("Timecode");

	iter = time_format_list->append();
	row = *iter;
	row[time_format_cols.format] = ExportProfileManager::MinSec;
	row[time_format_cols.label] = _("Minutes:Seconds");

	iter = time_format_list->append();
	row = *iter;
	row[time_format_cols.format] = ExportProfileManager::BBT;
	row[time_format_cols.label] = _("Bars:Beats");

	time_format_combo.pack_start (time_format_cols.label);
	time_format_combo.set_active (0);

	time_format_combo.signal_changed().connect (sigc::mem_fun (*this, &ExportTimespanSelector::change_time_format));

	/* Range view */

	range_list = Gtk::ListStore::create (range_cols);
	range_view.set_model (range_list);
	range_view.set_headers_visible (true);
}

ExportTimespanSelector::~ExportTimespanSelector ()
{

}

void
ExportTimespanSelector::add_range_to_selection (ARDOUR::Location const * loc)
{
	ExportTimespanPtr span = _session->get_export_handler()->add_timespan();

	std::string id;
	if (loc == state->selection_range.get()) {
		id = "selection";
	} else {
		id = loc->id().to_s();
	}

	span->set_range (loc->start(), loc->end());
	span->set_name (loc->name());
	span->set_range_id (id);
	state->timespans->push_back (span);
}

void
ExportTimespanSelector::set_time_format_from_state ()
{
	Gtk::TreeModel::Children::iterator tree_it;
	for (tree_it = time_format_list->children().begin(); tree_it != time_format_list->children().end(); ++tree_it) {
		if (tree_it->get_value (time_format_cols.format) == state->time_format) {
			time_format_combo.set_active (tree_it);
		}
	}
}

void
ExportTimespanSelector::sync_with_manager ()
{
	state = manager->get_timespans().front();
	fill_range_list ();
	CriticalSelectionChanged();
}

void
ExportTimespanSelector::change_time_format ()
{
	state->time_format = time_format_combo.get_active()->get_value (time_format_cols.format);

	for (Gtk::ListStore::Children::iterator it = range_list->children().begin(); it != range_list->children().end(); ++it) {
		Location * location = it->get_value (range_cols.location);
		it->set_value (range_cols.label, construct_label (location));
		it->set_value (range_cols.length, construct_length (location));
	}
}

std::string
ExportTimespanSelector::construct_label (ARDOUR::Location const * location) const
{
	std::string label;
	std::string start;
	std::string end;

	framepos_t start_frame = location->start();
	framepos_t end_frame = location->end();

	switch (state->time_format) {
	  case AudioClock::BBT:
		start = bbt_str (start_frame);
		end = bbt_str (end_frame);
		break;

	  case AudioClock::Timecode:
		start = timecode_str (start_frame);
		end = timecode_str (end_frame);
		break;

	  case AudioClock::MinSec:
		start = ms_str (start_frame);
		end = ms_str (end_frame);
		break;

	  case AudioClock::Frames:
		start = to_string (start_frame, std::dec);
		end = to_string (end_frame, std::dec);
		break;
	}

	// label += _("from ");

	// label += "<span color=\"#7fff7f\">";
	label += start;
// 	label += "</span>";

	label += _(" to ");

// 	label += "<span color=\"#7fff7f\">";
	label += end;
// 	label += "</span>";

	return label;
}

std::string
ExportTimespanSelector::construct_length (ARDOUR::Location const * location) const
{
	if (location->length() == 0) {
		return "";
	}

	std::stringstream s;

	switch (state->time_format) {
	case AudioClock::BBT:
		s << bbt_str (location->length ());
		break;

	case AudioClock::Timecode:
	{
		Timecode::Time tc;
		_session->timecode_duration (location->length(), tc);
		tc.print (s);
		break;
	}

	case AudioClock::MinSec:
		s << ms_str (location->length ());
		break;

	case AudioClock::Frames:
		s << location->length ();
		break;
	}

	return s.str ();
}


std::string
ExportTimespanSelector::bbt_str (framepos_t frames) const
{
	if (!_session) {
		return "Error!";
	}

	std::ostringstream oss;
	Timecode::BBT_Time time;
	_session->bbt_time (frames, time);

	print_padded (oss, time);
	return oss.str ();
}

std::string
ExportTimespanSelector::timecode_str (framecnt_t frames) const
{
	if (!_session) {
		return "Error!";
	}

	std::ostringstream oss;
	Timecode::Time time;

	_session->timecode_time (frames, time);

	oss << std::setfill('0') << std::right <<
	  std::setw(2) <<
	  time.hours << ":" <<
	  std::setw(2) <<
	  time.minutes << ":" <<
	  std::setw(2) <<
	  time.seconds << ":" <<
	  std::setw(2) <<
	  time.frames;

	return oss.str();
}

std::string
ExportTimespanSelector::ms_str (framecnt_t frames) const
{
	if (!_session) {
		return "Error!";
	}

	std::ostringstream oss;
	framecnt_t left;
	int hrs;
	int mins;
	int secs;
	int sec_promilles;

	left = frames;
	hrs = (int) floor (left / (_session->frame_rate() * 60.0f * 60.0f));
	left -= (framecnt_t) floor (hrs * _session->frame_rate() * 60.0f * 60.0f);
	mins = (int) floor (left / (_session->frame_rate() * 60.0f));
	left -= (framecnt_t) floor (mins * _session->frame_rate() * 60.0f);
	secs = (int) floor (left / (float) _session->frame_rate());
	left -= (framecnt_t) floor ((double)(secs * _session->frame_rate()));
	sec_promilles = (int) (left * 1000 / (float) _session->frame_rate() + 0.5);

	oss << std::setfill('0') << std::right <<
	  std::setw(2) <<
	  hrs << ":" <<
	  std::setw(2) <<
	  mins << ":" <<
	  std::setw(2) <<
	  secs << "." <<
	  std::setw(3) <<
	  sec_promilles;

	return oss.str();
}

void
ExportTimespanSelector::update_range_name (std::string const & path, std::string const & new_text)
{
	Gtk::TreeStore::iterator it = range_list->get_iter (path);
	it->get_value (range_cols.location)->set_name (new_text);

	CriticalSelectionChanged();
}

void
ExportTimespanSelector::set_selection_state_of_all_timespans (bool s)
{
	for (Gtk::ListStore::Children::iterator it = range_list->children().begin(); it != range_list->children().end(); ++it) {
		it->set_value (range_cols.selected, s);
	}
}

/*** ExportTimespanSelectorSingle ***/

ExportTimespanSelectorSingle::ExportTimespanSelectorSingle (ARDOUR::Session * session, ProfileManagerPtr manager, std::string range_id) :
	ExportTimespanSelector (session, manager),
	range_id (range_id)
{
	range_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_NEVER);
	range_view.append_column_editable (_("Range"), range_cols.name);

	if (Gtk::CellRendererText * renderer = dynamic_cast<Gtk::CellRendererText *> (range_view.get_column_cell_renderer (0))) {
		renderer->signal_edited().connect (sigc::mem_fun (*this, &ExportTimespanSelectorSingle::update_range_name));
	}

	Gtk::CellRendererText * label_render = Gtk::manage (new Gtk::CellRendererText());
	Gtk::TreeView::Column * label_col = Gtk::manage (new Gtk::TreeView::Column (_("Time Span"), *label_render));
	label_col->add_attribute (label_render->property_markup(), range_cols.label);
	range_view.append_column (*label_col);

	range_view.append_column (_("Length"), range_cols.length);
}

void
ExportTimespanSelectorSingle::fill_range_list ()
{
	if (!state) { return; }

	std::string id;
	if (!range_id.compare (X_("selection"))) {
		id = state->selection_range->id().to_s();
	} else {
		id = range_id;
	}

	range_list->clear();
	state->timespans->clear();

	Gtk::TreeModel::iterator iter;
	Gtk::TreeModel::Row row;
	for (LocationList::const_iterator it = state->ranges->begin(); it != state->ranges->end(); ++it) {

		if (!(*it)->id().to_s().compare (id)) {
			iter = range_list->append();
			row = *iter;

			row[range_cols.location] = *it;
			row[range_cols.selected] = true;
			row[range_cols.name] = (*it)->name();
			row[range_cols.label] = construct_label (*it);
			row[range_cols.length] = construct_length (*it);

			add_range_to_selection (*it);

			break;
		}
	}

	set_time_format_from_state();
}

/*** ExportTimespanSelectorMultiple ***/

ExportTimespanSelectorMultiple::ExportTimespanSelectorMultiple (ARDOUR::Session * session, ProfileManagerPtr manager) :
  ExportTimespanSelector (session, manager)
{
	range_scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	range_view.append_column_editable ("", range_cols.selected);
	range_view.append_column_editable (_("Range"), range_cols.name);

	if (Gtk::CellRendererToggle * renderer = dynamic_cast<Gtk::CellRendererToggle *> (range_view.get_column_cell_renderer (0))) {
		renderer->signal_toggled().connect (sigc::hide (sigc::mem_fun (*this, &ExportTimespanSelectorMultiple::update_selection)));
	}
	if (Gtk::CellRendererText * renderer = dynamic_cast<Gtk::CellRendererText *> (range_view.get_column_cell_renderer (1))) {
		renderer->signal_edited().connect (sigc::mem_fun (*this, &ExportTimespanSelectorMultiple::update_range_name));
	}

	Gtk::CellRendererText * label_render = Gtk::manage (new Gtk::CellRendererText());
	Gtk::TreeView::Column * label_col = Gtk::manage (new Gtk::TreeView::Column (_("Time Span"), *label_render));
	label_col->add_attribute (label_render->property_markup(), range_cols.label);
	range_view.append_column (*label_col);

	range_view.append_column (_("Length"), range_cols.length);
}

void
ExportTimespanSelectorMultiple::fill_range_list ()
{
	if (!state) { return; }

	range_list->clear();

	Gtk::TreeModel::iterator iter;
	Gtk::TreeModel::Row row;
	for (LocationList::const_iterator it = state->ranges->begin(); it != state->ranges->end(); ++it) {

		iter = range_list->append();
		row = *iter;

		row[range_cols.location] = *it;
		row[range_cols.selected] = false;
		row[range_cols.name] = (*it)->name();
		row[range_cols.label] = construct_label (*it);
		row[range_cols.length] = construct_length (*it);
	}

	set_selection_from_state ();
}

void
ExportTimespanSelectorMultiple::set_selection_from_state ()
{
	Gtk::TreeModel::Children::iterator tree_it;

	for (TimespanList::iterator it = state->timespans->begin(); it != state->timespans->end(); ++it) {
		string id = (*it)->range_id();
		for (tree_it = range_list->children().begin(); tree_it != range_list->children().end(); ++tree_it) {
			Location * loc = tree_it->get_value (range_cols.location);

			if ((id == "selection" && loc == state->selection_range.get()) ||
			    (id == loc->id().to_s())) {
				tree_it->set_value (range_cols.selected, true);
			}
		}
	}

	set_time_format_from_state();
}

void
ExportTimespanSelectorMultiple::update_selection ()
{
	update_timespans ();
	CriticalSelectionChanged ();
}

void
ExportTimespanSelectorMultiple::update_timespans ()
{
	state->timespans->clear();

	for (Gtk::TreeStore::Children::iterator it = range_list->children().begin(); it != range_list->children().end(); ++it) {
		if (it->get_value (range_cols.selected)) {
			add_range_to_selection (it->get_value (range_cols.location));
		}
	}
}

