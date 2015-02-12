/*
    Copyright (C) 2008 Paul Davis
	Copyright (C) 2015 Waves Audio Ltd.
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

#include "waves_export_timespan_selector.h"

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

WavesExportTimespanSelector::WavesExportTimespanSelector (ARDOUR::Session * session, ProfileManagerPtr manager)
  : Gtk::VBox ()
  , WavesUI ("waves_export_timespan_selector.xml", *this)
  , manager (manager)
  , _time_format_dropdown (get_waves_dropdown ("time_format_dropdown"))
  , _range_view (get_tree_view ("range_view"))
  , _range_scroller (get_scrolled_window ("range_scroller"))
  , _select_all_button (get_waves_button("select_all_button"))
  , _deselect_all_button (get_waves_button("deselect_all_button"))
{
	set_session (session);

	_select_all_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportTimespanSelector::on_selection_all_buttons));
	_deselect_all_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportTimespanSelector::on_selection_all_buttons));
	
	/*** Combo boxes ***/
	_time_format_dropdown.selected_item_changed.connect (sigc::mem_fun(*this, &WavesExportTimespanSelector::on_time_format_changed));

	/* Range view */
	range_list = Gtk::ListStore::create (range_cols);
	// order by location start times
	range_list->set_sort_column(range_cols.location, Gtk::SORT_ASCENDING);
	range_list->set_sort_func(range_cols.location, sigc::mem_fun(*this, &WavesExportTimespanSelector::location_sorter));
	_range_view.set_model (range_list);
	_range_view.set_headers_visible (true);
}

WavesExportTimespanSelector::~WavesExportTimespanSelector ()
{

}

int
WavesExportTimespanSelector::location_sorter(Gtk::TreeModel::iterator a, Gtk::TreeModel::iterator b)
{
	Location *l1 = (*a)[range_cols.location];
	Location *l2 = (*b)[range_cols.location];
	const Location *ls = _session->locations()->session_range_location();

	// always sort session range first
	if (l1 == ls)
		return -1;
	if (l2 == ls)
		return +1;

	return l1->start() - l2->start();
}

void
WavesExportTimespanSelector::add_range_to_selection (ARDOUR::Location const * loc)
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
WavesExportTimespanSelector::set_time_format_from_state ()
{
	int itemdata = -1;
	switch (state->time_format) {
	case ExportProfileManager::Timecode:
		itemdata = Timecode;
		break;
	case ExportProfileManager::MinSec:
		itemdata = MinSec;
		break;
	case ExportProfileManager::Frames:
		itemdata = Samples;
		break;
	}
	
	unsigned int size = _time_format_dropdown.get_menu ().items ().size();
	for (unsigned int i = 0; i < size; i++) {
		if (_time_format_dropdown.get_item_data_u (i) == itemdata) {
			_time_format_dropdown.set_current_item (i);
			break;
		}
	}
}

void
WavesExportTimespanSelector::sync_with_manager ()
{
	state = manager->get_timespans().front();
	fill_range_list ();
	CriticalSelectionChanged();
}

void
WavesExportTimespanSelector::on_time_format_changed (WavesDropdown*, int format_id)
{
	switch (format_id) {
	case Timecode:
		state->time_format = ExportProfileManager::Timecode;
		break;
	case MinSec:
		state->time_format = ExportProfileManager::MinSec;
		break;
    case Samples:
		state->time_format = ExportProfileManager::Frames;
		break;
	}

	for (Gtk::ListStore::Children::iterator it = range_list->children().begin(); it != range_list->children().end(); ++it) {
		Location * location = it->get_value (range_cols.location);
		it->set_value (range_cols.label, construct_label (location));
		it->set_value (range_cols.length, construct_length (location));
	}
}

std::string
WavesExportTimespanSelector::construct_label (ARDOUR::Location const * location) const
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
WavesExportTimespanSelector::construct_length (ARDOUR::Location const * location) const
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
WavesExportTimespanSelector::bbt_str (framepos_t frames) const
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
WavesExportTimespanSelector::timecode_str (framecnt_t frames) const
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
WavesExportTimespanSelector::ms_str (framecnt_t frames) const
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
WavesExportTimespanSelector::update_range_name (std::string const & path, std::string const & new_text)
{
	Gtk::TreeStore::iterator it = range_list->get_iter (path);
	it->get_value (range_cols.location)->set_name (new_text);

	CriticalSelectionChanged();
}

void
WavesExportTimespanSelector::on_selection_all_buttons (WavesButton* button)
{
	bool selected = (button == &_select_all_button);
    state->timespans->clear();
    
	for (Gtk::ListStore::Children::iterator it = range_list->children().begin(); it != range_list->children().end(); ++it) {
		it->set_value (range_cols.selected, selected);
        if (selected)
            add_range_to_selection (it->get_value (range_cols.location));
	}
    
    CriticalSelectionChanged();
}

/*** WavesExportTimespanSelectorSingle ***/

WavesExportTimespanSelectorSingle::WavesExportTimespanSelectorSingle (ARDOUR::Session * session, ProfileManagerPtr manager, std::string range_id) :
	WavesExportTimespanSelector (session, manager),
	range_id (range_id)
{
	_range_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_NEVER);
	_range_view.append_column_editable (_("Range"), range_cols.name);

	if (Gtk::CellRendererText * renderer = dynamic_cast<Gtk::CellRendererText *> (_range_view.get_column_cell_renderer (0))) {
		renderer->signal_edited().connect (sigc::mem_fun (*this, &WavesExportTimespanSelectorSingle::update_range_name));
	}

	Gtk::CellRendererText * label_render = Gtk::manage (new Gtk::CellRendererText());
	Gtk::TreeView::Column * label_col = Gtk::manage (new Gtk::TreeView::Column (_("Time Span"), *label_render));
	label_col->add_attribute (label_render->property_markup(), range_cols.label);
	_range_view.append_column (*label_col);

	_range_view.append_column (_("Length"), range_cols.length);
}

void
WavesExportTimespanSelectorSingle::fill_range_list ()
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

/*** WavesExportTimespanSelectorMultiple ***/

WavesExportTimespanSelectorMultiple::WavesExportTimespanSelectorMultiple (ARDOUR::Session * session, ProfileManagerPtr manager) :
  WavesExportTimespanSelector (session, manager)
{
	_range_scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	_range_view.append_column_editable ("", range_cols.selected);
	_range_view.append_column_editable (_("Range"), range_cols.name);

	if (Gtk::CellRendererToggle * renderer = dynamic_cast<Gtk::CellRendererToggle *> (_range_view.get_column_cell_renderer (0))) {
		renderer->signal_toggled().connect (sigc::hide (sigc::mem_fun (*this, &WavesExportTimespanSelectorMultiple::update_selection)));
	}

	if (Gtk::CellRendererText * renderer = dynamic_cast<Gtk::CellRendererText *> (_range_view.get_column_cell_renderer (1))) {
		renderer->signal_edited().connect (sigc::mem_fun (*this, &WavesExportTimespanSelectorMultiple::update_range_name));
	}

	Gtk::CellRendererText * label_render = Gtk::manage (new Gtk::CellRendererText());
	Gtk::TreeView::Column * label_col = Gtk::manage (new Gtk::TreeView::Column (_("Time Span"), *label_render));
	label_col->add_attribute (label_render->property_markup(), range_cols.label);
	_range_view.append_column (*label_col);

	_range_view.append_column (_("Length"), range_cols.length);
}

void
WavesExportTimespanSelectorMultiple::fill_range_list ()
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
WavesExportTimespanSelectorMultiple::set_selection_from_state ()
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
WavesExportTimespanSelectorMultiple::update_selection ()
{
	update_timespans ();
	CriticalSelectionChanged ();
}

void
WavesExportTimespanSelectorMultiple::update_timespans ()
{
	state->timespans->clear();

	for (Gtk::TreeStore::Children::iterator it = range_list->children().begin(); it != range_list->children().end(); ++it) {
		if (it->get_value (range_cols.selected)) {
			add_range_to_selection (it->get_value (range_cols.location));
		}
	}
}

