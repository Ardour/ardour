/*
 * Copyright (C) 2008-2013 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2013-2014 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#include <sstream>
#include <iomanip>

#include "pbd/enumwriter.h"
#include "pbd/string_convert.h"

#include "ardour/location.h"
#include "ardour/types.h"
#include "ardour/session.h"
#include "ardour/export_handler.h"
#include "ardour/export_timespan.h"

#include "export_timespan_selector.h"

#include "pbd/i18n.h"

using namespace Glib;
using namespace ARDOUR;
using namespace PBD;
using std::string;

ExportTimespanSelector::ExportTimespanSelector (ARDOUR::Session * session, ProfileManagerPtr manager, bool multi)
	: manager (manager)
	, _realtime_available (false)
	, time_format_label (_("Show Times as:"), Gtk::ALIGN_LEFT)
	, realtime_checkbutton (_("Realtime Export"))
{
	set_session (session);

	option_hbox.pack_start (time_format_label, false, false, 0);
	option_hbox.pack_start (time_format_combo, false, false, 6);

	if (multi) {
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
	}
	option_hbox.pack_start (realtime_checkbutton, false, false, 6);
	realtime_checkbutton.set_active (session->config.get_realtime_export ());
	realtime_checkbutton.set_sensitive (_realtime_available);

	realtime_checkbutton.signal_toggled ().connect (
			sigc::mem_fun (*this, &ExportTimespanSelector::toggle_realtime)
			);

	range_scroller.add (range_view);

	pack_start (option_hbox, false, false, 0);
	pack_start (range_scroller, true, true, 6);

	/*** Combo boxes ***/

	Gtk::TreeModel::iterator iter;
	Gtk::TreeModel::Row row;

	/* Time format combo */

	time_format_list = Gtk::ListStore::create (time_format_cols);
	time_format_combo.set_model (time_format_list);

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
	// order by location start times
	range_list->set_sort_column(range_cols.location, Gtk::SORT_ASCENDING);
	range_list->set_sort_func(range_cols.location, sigc::mem_fun(*this, &ExportTimespanSelector::location_sorter));
	range_view.set_model (range_list);
	range_view.set_headers_visible (true);
}

ExportTimespanSelector::~ExportTimespanSelector ()
{

}

int
ExportTimespanSelector::location_sorter(Gtk::TreeModel::iterator a, Gtk::TreeModel::iterator b)
{
	Location *l1 = (*a)[range_cols.location];
	Location *l2 = (*b)[range_cols.location];
	const Location *ls = _session->locations()->session_range_location();

	// always sort session range first
	if (l1 == ls)
		return -1;
	if (l2 == ls)
		return +1;

	return l2->start().distance (l1->start()).samples();
}

void
ExportTimespanSelector::add_range_to_selection (ARDOUR::Location const * loc, bool rt)
{
	ExportTimespanPtr span = _session->get_export_handler()->add_timespan();

	std::string id;
	if (loc == state->selection_range.get()) {
		id = "selection";
	} else {
		id = loc->id().to_s();
	}

	span->set_range (loc->start().samples(), loc->end().samples());
	span->set_name (loc->name());
	span->set_range_id (id);
	span->set_realtime (rt);
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
ExportTimespanSelector::allow_realtime_export (bool yn)
{
	if (_realtime_available == yn) {
		return;
	}
	_realtime_available = yn;
	realtime_checkbutton.set_sensitive (_realtime_available);
	update_timespans ();
}

void
ExportTimespanSelector::toggle_realtime ()
{
	const bool realtime = !_session->config.get_realtime_export ();
	_session->config.set_realtime_export (realtime);
	realtime_checkbutton.set_inconsistent (false);
	realtime_checkbutton.set_active (realtime);

	for (Gtk::TreeStore::Children::iterator it = range_list->children().begin(); it != range_list->children().end(); ++it) {
		it->set_value (range_cols.realtime, realtime);
	}
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

	samplepos_t start_sample = location->start().samples();
	samplepos_t end_sample = location->end().samples();

	switch (state->time_format) {
	  case ExportProfileManager::BBT:
		start = bbt_str (start_sample);
		end = bbt_str (end_sample);
		break;

	  case ExportProfileManager::Timecode:
		start = timecode_str (start_sample);
		end = timecode_str (end_sample);
		break;

	  case ExportProfileManager::MinSec:
		start = ms_str (start_sample);
		end = ms_str (end_sample);
		break;

	  case ExportProfileManager::Samples:
		start = to_string (start_sample);
		end = to_string (end_sample);
		break;
	}

	label += start;
	label += _(" to ");
	label += end;

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
	case ExportProfileManager::BBT:
		s << bbt_str (location->length ().samples());
		break;

	case ExportProfileManager::Timecode:
	{
		Timecode::Time tc;
		_session->timecode_duration (location->length().samples(), tc);
		tc.print (s);
		break;
	}

	case ExportProfileManager::MinSec:
		s << ms_str (location->length ().samples());
		break;

	case ExportProfileManager::Samples:
		s << location->length ().samples();
		break;
	}

	return s.str ();
}


std::string
ExportTimespanSelector::bbt_str (samplepos_t samples) const
{
	if (!_session) {
		return "Error!";
	}

	std::ostringstream oss;
	Temporal::BBT_Time time;
	_session->bbt_time (timepos_t (samples), time);

	time.print_padded (oss);

	return oss.str ();
}

std::string
ExportTimespanSelector::timecode_str (samplecnt_t samples) const
{
	if (!_session) {
		return "Error!";
	}

	std::ostringstream oss;
	Timecode::Time time;

	_session->timecode_time (samples, time);

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
ExportTimespanSelector::ms_str (samplecnt_t samples) const
{
	if (!_session) {
		return "Error!";
	}

	std::ostringstream oss;
	samplecnt_t left;
	int hrs;
	int mins;
	int secs;
	int sec_promilles;

	left = samples;
	hrs = (int) floor (left / (_session->sample_rate() * 60.0f * 60.0f));
	left -= (samplecnt_t) floor (hrs * _session->sample_rate() * 60.0f * 60.0f);
	mins = (int) floor (left / (_session->sample_rate() * 60.0f));
	left -= (samplecnt_t) floor (mins * _session->sample_rate() * 60.0f);
	secs = (int) floor (left / (float) _session->sample_rate());
	left -= (samplecnt_t) floor ((double)(secs * _session->sample_rate()));
	sec_promilles = (int) (left * 1000 / (float) _session->sample_rate() + 0.5);

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

	update_timespans ();
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
	ExportTimespanSelector (session, manager, false),
	range_id (range_id)
{
	range_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_NEVER);
	range_view.append_column_editable (_("RT"), range_cols.realtime); // 0
	range_view.append_column_editable (_("Range"), range_cols.name); // 1

	if (Gtk::CellRendererToggle * renderer = dynamic_cast<Gtk::CellRendererToggle *> (range_view.get_column_cell_renderer (0))) {
		renderer->signal_toggled().connect (sigc::hide (sigc::mem_fun (*this, &ExportTimespanSelectorSingle::update_timespans)));
	}

	if (Gtk::CellRendererText * renderer = dynamic_cast<Gtk::CellRendererText *> (range_view.get_column_cell_renderer (1))) {
		renderer->signal_edited().connect (sigc::mem_fun (*this, &ExportTimespanSelectorSingle::update_range_name));
	}

	Gtk::CellRendererText * label_render = Gtk::manage (new Gtk::CellRendererText());
	Gtk::TreeView::Column * label_col = Gtk::manage (new Gtk::TreeView::Column (_("Time Span"), *label_render));
	label_col->add_attribute (label_render->property_markup(), range_cols.label);
	range_view.append_column (*label_col); // 2

	range_view.append_column (_("Length"), range_cols.length); // 3
	range_view.append_column (_("Creation Date"), range_cols.date); // 4

	Gtk::TreeViewColumn* range_col = range_view.get_column (1); // "Range"
	range_col->set_sort_column (range_cols.name);

	Gtk::TreeViewColumn* time_span_col = range_view.get_column (2); // "Time Span"
	time_span_col->set_sort_column (range_cols.start);

	Gtk::TreeViewColumn* length_col = range_view.get_column (3); // "Length"
	length_col->set_sort_column (range_cols.length_actual);

	Gtk::TreeViewColumn* date_col = range_view.get_column (4); // "Creation Date"
	date_col->set_sort_column (range_cols.timestamp);
}

void
ExportTimespanSelectorSingle::allow_realtime_export (bool yn)
{
	ExportTimespanSelector::allow_realtime_export (yn);
	range_view.get_column (0)->set_visible (_realtime_available);
}

void
ExportTimespanSelectorSingle::fill_range_list ()
{
	if (!state) { return; }
	const bool realtime = _session->config.get_realtime_export ();

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
			row[range_cols.realtime] = realtime;
			row[range_cols.name] = (*it)->name();
			row[range_cols.label] = construct_label (*it);
			row[range_cols.length] = construct_length (*it);
			//the actual samplecnt_t for sorting
			row[range_cols.length_actual] = (*it)->length().samples();

			//start samplecnt_t for sorting
			row[range_cols.start] = (*it)->start().samples();

			Glib::DateTime gdt(Glib::DateTime::create_now_local ((*it)->timestamp()));
			row[range_cols.timestamp] = (*it)->timestamp();
			row[range_cols.date] = gdt.format ("%F %H:%M");;


			add_range_to_selection (*it, false);

			break;
		}
	}

	set_time_format_from_state();
}

void
ExportTimespanSelectorSingle::update_timespans ()
{
	state->timespans->clear();
	const bool realtime = _session->config.get_realtime_export ();
	bool inconsistent = false;
	bool rt_match = false;

	for (Gtk::TreeStore::Children::iterator it = range_list->children().begin(); it != range_list->children().end(); ++it) {
		add_range_to_selection (it->get_value (range_cols.location), it->get_value (range_cols.realtime) && _realtime_available);
		if (it->get_value (range_cols.realtime) != realtime) {
			inconsistent = true;
		} else {
			rt_match = true;
		}
	}
	if (!rt_match) {
		realtime_checkbutton.set_inconsistent (false);
		realtime_checkbutton.set_active (!realtime);
	} else {
		realtime_checkbutton.set_inconsistent (inconsistent);
	}
}

/*** ExportTimespanSelectorMultiple ***/

ExportTimespanSelectorMultiple::ExportTimespanSelectorMultiple (ARDOUR::Session * session, ProfileManagerPtr manager) :
  ExportTimespanSelector (session, manager, true)
{
	range_scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	range_view.append_column_editable ("", range_cols.selected);
	range_view.append_column_editable (_("RT"), range_cols.realtime);
	range_view.append_column_editable (_("Range"), range_cols.name);

	if (Gtk::CellRendererToggle * renderer = dynamic_cast<Gtk::CellRendererToggle *> (range_view.get_column_cell_renderer (0))) {
		renderer->signal_toggled().connect (sigc::hide (sigc::mem_fun (*this, &ExportTimespanSelectorMultiple::update_selection)));
	}
	if (Gtk::CellRendererToggle * renderer = dynamic_cast<Gtk::CellRendererToggle *> (range_view.get_column_cell_renderer (1))) {
		renderer->signal_toggled().connect (sigc::hide (sigc::mem_fun (*this, &ExportTimespanSelectorMultiple::update_selection)));
	}
	if (Gtk::CellRendererText * renderer = dynamic_cast<Gtk::CellRendererText *> (range_view.get_column_cell_renderer (2))) {
		renderer->signal_edited().connect (sigc::mem_fun (*this, &ExportTimespanSelectorMultiple::update_range_name));
	}

	Gtk::CellRendererText * label_render = Gtk::manage (new Gtk::CellRendererText());
	Gtk::TreeView::Column * label_col = Gtk::manage (new Gtk::TreeView::Column (_("Time Span"), *label_render));
	label_col->add_attribute (label_render->property_markup(), range_cols.label);
	range_view.append_column (*label_col); // 3

	range_view.append_column (_("Length"), range_cols.length); // 4
	range_view.append_column (_("Creation Date"), range_cols.date); // 5

	Gtk::TreeViewColumn* range_col = range_view.get_column (2); // "Range"
	range_col->set_sort_column (range_cols.name);

	Gtk::TreeViewColumn* time_span_col = range_view.get_column (3); // "Time Span"
	time_span_col->set_sort_column (range_cols.start);

	Gtk::TreeViewColumn* length_col = range_view.get_column (4); // "Length"
	length_col->set_sort_column (range_cols.length_actual);

	Gtk::TreeViewColumn* date_col = range_view.get_column (5); // "Creation Date"
	date_col->set_sort_column (range_cols.timestamp);
}

void
ExportTimespanSelectorMultiple::allow_realtime_export (bool yn)
{
	ExportTimespanSelector::allow_realtime_export (yn);
	range_view.get_column (1)->set_visible (_realtime_available);
}

void
ExportTimespanSelectorMultiple::fill_range_list ()
{
	if (!state) { return; }
	const bool realtime = _session->config.get_realtime_export ();

	range_list->clear();

	Gtk::TreeModel::iterator iter;
	Gtk::TreeModel::Row row;
	for (LocationList::const_iterator it = state->ranges->begin(); it != state->ranges->end(); ++it) {

		iter = range_list->append();
		row = *iter;

		row[range_cols.location] = *it;
		row[range_cols.selected] = false;
		row[range_cols.realtime] = realtime;
		row[range_cols.name] = (*it)->name();
		row[range_cols.label] = construct_label (*it);
		row[range_cols.length] = construct_length (*it);
		//the actual samplecnt_t for sorting
		row[range_cols.length_actual] = (*it)->length().samples();

		//start samplecnt_t for sorting
		row[range_cols.start] = (*it)->start().samples();

		Glib::DateTime gdt(Glib::DateTime::create_now_local ((*it)->timestamp()));
		row[range_cols.timestamp] = (*it)->timestamp();
		row[range_cols.date] = gdt.format ("%F %H:%M");;

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
				tree_it->set_value (range_cols.realtime, (*it)->realtime ());
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
	const bool realtime = _session->config.get_realtime_export ();
	bool inconsistent = false;
	bool rt_match = false;

	for (Gtk::TreeStore::Children::iterator it = range_list->children().begin(); it != range_list->children().end(); ++it) {
		if (it->get_value (range_cols.selected)) {
			add_range_to_selection (it->get_value (range_cols.location), it->get_value (range_cols.realtime) && _realtime_available);
		}
		if (it->get_value (range_cols.realtime) != realtime) {
			inconsistent = true;
		} else {
			rt_match = true;
		}
	}
	if (!rt_match) {
		realtime_checkbutton.set_inconsistent (false);
		realtime_checkbutton.set_active (!realtime);
	} else {
		realtime_checkbutton.set_inconsistent (inconsistent);
	}
}

