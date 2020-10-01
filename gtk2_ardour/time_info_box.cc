/*
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2017 Nick Mainsbridge <mainsbridge@gmail.com>
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
#include "pbd/compose.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/actions.h"

#include "ardour/location.h"
#include "ardour/profile.h"
#include "ardour/session.h"

#include "audio_clock.h"
#include "automation_line.h"
#include "control_point.h"
#include "editor.h"
#include "region_view.h"
#include "time_info_box.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using std::min;
using std::max;

TimeInfoBox::TimeInfoBox (std::string state_node_name, bool with_punch)
	: table (3, 3)
	, punch_start (0)
	, punch_end (0)
	, syncing_selection (false)
	, syncing_punch (false)
	, with_punch_clock (with_punch)
{
	set_name (X_("TimeInfoBox"));

	selection_start = new AudioClock (
			string_compose ("%1-selection-start", state_node_name),
			false, "selection", false, false, false, false);
	selection_end = new AudioClock (
			string_compose ("%1-selection-end", state_node_name),
			false, "selection", false, false, false, false);
	selection_length = new AudioClock (
			string_compose ("%1-selection-length", state_node_name),
			false, "selection", false, false, true, false);

	selection_title.set_text (_("Selection"));

	set_homogeneous (false);
	set_spacing (0);
	set_border_width (2);

	pack_start (table, false, false);

	table.set_homogeneous (false);
	table.set_spacings (0);
	table.set_border_width (2);
	table.set_col_spacings (2);

	Gtk::Label* l;

	selection_title.set_name ("TimeInfoSelectionTitle");
	if (with_punch_clock) {
		table.attach (selection_title, 1, 2, 0, 1);
	}
	l = manage (new Label);
	l->set_text (_("Start"));
	l->set_alignment (1.0, 0.5);
	l->set_name (X_("TimeInfoSelectionLabel"));
	table.attach (*l, 0, 1, 1, 2, FILL);
	table.attach (*selection_start, 1, 2, 1, 2);

	l = manage (new Label);
	l->set_text (_("End"));
	l->set_alignment (1.0, 0.5);
	l->set_name (X_("TimeInfoSelectionLabel"));
	table.attach (*l, 0, 1, 2, 3, FILL);
	table.attach (*selection_end, 1, 2, 2, 3);

	l = manage (new Label);
	l->set_text (_("Length"));
	l->set_alignment (1.0, 0.5);
	l->set_name (X_("TimeInfoSelectionLabel"));
	table.attach (*l, 0, 1, 3, 4, FILL);
	table.attach (*selection_length, 1, 2, 3, 4);

	if (with_punch_clock) {
		punch_start = new AudioClock (
				string_compose ("%1-punch-start", state_node_name),
				false, "punch", false, false, false, false);
		punch_end = new AudioClock (
				string_compose ("%1-punch-end", state_node_name),
				false, "punch", false, false, false, false);
		punch_title.set_text (_("Punch"));

		punch_title.set_name ("TimeInfoSelectionTitle");
		table.attach (punch_title, 2, 3, 0, 1);
		table.attach (*punch_start, 2, 3, 1, 2);
		table.attach (*punch_end, 2, 3, 2, 3);
	}

	show_all ();

	selection_start->mode_changed.connect (sigc::bind (sigc::mem_fun (*this, &TimeInfoBox::sync_selection_mode), selection_start));
	selection_end->mode_changed.connect (sigc::bind (sigc::mem_fun (*this, &TimeInfoBox::sync_selection_mode), selection_end));
	selection_length->mode_changed.connect (sigc::bind (sigc::mem_fun (*this, &TimeInfoBox::sync_selection_mode), selection_length));

	selection_start->signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &TimeInfoBox::clock_button_release_event), selection_start), true);
	selection_end->signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &TimeInfoBox::clock_button_release_event), selection_end), true);

	if (with_punch_clock) {
		punch_start->mode_changed.connect (sigc::bind (sigc::mem_fun (*this, &TimeInfoBox::sync_punch_mode), punch_start));
		punch_end->mode_changed.connect (sigc::bind (sigc::mem_fun (*this, &TimeInfoBox::sync_punch_mode), punch_end));

		punch_start->signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &TimeInfoBox::clock_button_release_event), punch_start), true);
		punch_end->signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &TimeInfoBox::clock_button_release_event), punch_end), true);
	}

	Editor::instance().get_selection().TimeChanged.connect (sigc::mem_fun (*this, &TimeInfoBox::selection_changed));
	Editor::instance().get_selection().RegionsChanged.connect (sigc::mem_fun (*this, &TimeInfoBox::selection_changed));

	Editor::instance().MouseModeChanged.connect (editor_connections, invalidator(*this), boost::bind (&TimeInfoBox::track_mouse_mode, this), gui_context());
}

TimeInfoBox::~TimeInfoBox ()
{
	delete selection_length;
	delete selection_end;
	delete selection_start;

	delete punch_start;
	delete punch_end;
}

void
TimeInfoBox::track_mouse_mode ()
{
	selection_changed ();
}

bool
TimeInfoBox::clock_button_release_event (GdkEventButton* ev, AudioClock* src)
{
	if (!_session) {
		return false;
	}

	if (ev->button == 1) {
		if (!src->off()) {
			_session->request_locate (src->current_time ().samples());
		}
		return true;
	}

	return false;
}

void
TimeInfoBox::sync_selection_mode (AudioClock* src)
{
	if (!syncing_selection) {
		syncing_selection = true;
		selection_start->set_mode (src->mode());
		selection_end->set_mode (src->mode());
		selection_length->set_mode (src->mode());
		syncing_selection = false;
	}
}

void
TimeInfoBox::sync_punch_mode (AudioClock* src)
{
	if (!with_punch_clock) {
		return;
	}
	if (!syncing_punch) {
		syncing_punch = true;
		punch_start->set_mode (src->mode());
		punch_end->set_mode (src->mode());
		syncing_punch = false;
	}
}


void
TimeInfoBox::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	selection_start->set_session (s);
	selection_end->set_session (s);
	selection_length->set_session (s);

	if (!with_punch_clock) {
		return;
	}

	punch_start->set_session (s);
	punch_end->set_session (s);

	if (s) {
		Location* punch = s->locations()->auto_punch_location ();

		if (punch) {
			watch_punch (punch);
		}

		punch_changed (punch);

		_session->auto_punch_location_changed.connect (_session_connections, MISSING_INVALIDATOR,
				boost::bind (&TimeInfoBox::punch_location_changed, this, _1), gui_context());
	}
}

void
TimeInfoBox::region_selection_changed ()
{
	timepos_t s, e;
	Selection& selection (Editor::instance().get_selection());
	s = selection.regions.start_time();
	e = selection.regions.end_time();
	selection_start->set_off (false);
	selection_end->set_off (false);
	selection_length->set_off (false);
	selection_start->set (s);
	selection_end->set (e);
	selection_length->set_duration (timecnt_t (e), false, timecnt_t (s));
}

void
TimeInfoBox::selection_changed ()
{
	samplepos_t s, e;
	Selection& selection (Editor::instance().get_selection());

	region_property_connections.drop_connections();

	switch (Editor::instance().current_mouse_mode()) {

	case Editing::MouseContent:
		/* displaying MIDI note selection is tricky */
		selection_start->set_off (true);
		selection_end->set_off (true);
		selection_length->set_off (true);
		break;

	case Editing::MouseObject:
		if (selection.regions.empty()) {
			if (selection.points.empty()) {
				Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action ("MouseMode", "set-mouse-mode-object-range");
				if (tact->get_active() && !selection.time.empty()) {
					/* show selected range */
					selection_start->set_off (false);
					selection_end->set_off (false);
					selection_length->set_off (false);
					selection_start->set (selection.time.start_time());
					selection_end->set (selection.time.end_time());
					selection_length->set_is_duration (true, selection.time.start_time());
					selection_length->set_duration (selection.time.start_time().distance (selection.time.end_time()));
				} else {
					selection_start->set_off (true);
					selection_end->set_off (true);
					selection_length->set_off (true);
				}
			} else {
				s = max_samplepos;
				e = 0;
				for (PointSelection::iterator i = selection.points.begin(); i != selection.points.end(); ++i) {
					timepos_t const p = (*i)->line().session_position ((*i)->model ());
					s = min (s, p);
					e = max (e, p);
				}
				selection_start->set_off (false);
				selection_end->set_off (false);
				selection_length->set_off (false);
				selection_start->set (s);
				selection_end->set (e);
				selection_length->set (e, false, s);
			}
		} else {
			/* this is more efficient than tracking changes per region in large selections */
			std::set<boost::shared_ptr<ARDOUR::Playlist> > playlists;
			for (RegionSelection::iterator s = selection.regions.begin(); s != selection.regions.end(); ++s) {
				boost::shared_ptr<Playlist> pl = (*s)->region()->playlist();
				if (pl) {
					playlists.insert (pl);
				}
			}
			for (std::set<boost::shared_ptr<ARDOUR::Playlist> >::iterator ps = playlists.begin(); ps != playlists.end(); ++ps) {
				(*ps)->ContentsChanged.connect (region_property_connections, invalidator (*this),
								boost::bind (&TimeInfoBox::region_selection_changed, this), gui_context());
			}
			region_selection_changed ();
		}
		break;

	case Editing::MouseRange:
		if (selection.time.empty()) {
			Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action ("MouseMode", "set-mouse-mode-object-range");

			if (tact->get_active() &&  !selection.regions.empty()) {
				/* show selected regions */
				s = selection.regions.start();
				e = selection.regions.end_sample();
				selection_start->set_off (false);
				selection_end->set_off (false);
				selection_length->set_off (false);
				selection_start->set (s);
				selection_end->set (e);
				selection_length->set (e, false, s);
			} else {
				selection_start->set_off (true);
				selection_end->set_off (true);
				selection_length->set_off (true);
			}
		} else {
			selection_start->set_off (false);
			selection_end->set_off (false);
			selection_length->set_off (false);
			selection_start->set (selection.time.start_time());
			selection_end->set (selection.time.end_time());
			selection_length->set_is_duration (true, selection.time.start_time());
			selection_length->set_duration (selection.time.start_time().distance (selection.time.end_time()));
		}
		break;

	default:
		selection_start->set_off (true);
		selection_end->set_off (true);
		selection_length->set_off (true);
		break;
	}
}

void
TimeInfoBox::punch_location_changed (Location* loc)
{
	if (loc && with_punch_clock) {
		watch_punch (loc);
	}
}

void
TimeInfoBox::watch_punch (Location* punch)
{
	assert (with_punch_clock);
	punch_connections.drop_connections ();

	punch->start_changed.connect (punch_connections, MISSING_INVALIDATOR, boost::bind (&TimeInfoBox::punch_changed, this, _1), gui_context());
	punch->end_changed.connect (punch_connections, MISSING_INVALIDATOR, boost::bind (&TimeInfoBox::punch_changed, this, _1), gui_context());

	punch_changed (punch);
}

void
TimeInfoBox::punch_changed (Location* loc)
{
	assert (with_punch_clock);
	if (!loc) {
		punch_start->set_off (true);
		punch_end->set_off (true);
		return;
	}

	punch_start->set_off (false);
	punch_end->set_off (false);

	punch_start->set (loc->start());
	punch_end->set (loc->end());
}
