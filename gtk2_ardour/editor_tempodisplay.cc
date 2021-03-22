/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2008-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2017-2018 Ben Loftis <ben@harrisonconsoles.com>
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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <cstdio> // for sprintf, grrr
#include <cstdlib>
#include <cmath>
#include <string>
#include <climits>

#include "pbd/error.h"
#include "pbd/memento_command.h"

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/gtk_ui.h>

#include "ardour/session.h"
#include "ardour/tempo.h"
#include <gtkmm2ext/doi.h>
#include <gtkmm2ext/utils.h>

#include "canvas/canvas.h"
#include "canvas/item.h"
#include "canvas/line_set.h"

#include "editor.h"
#include "marker.h"
#include "tempo_dialog.h"
#include "rgb_macros.h"
#include "gui_thread.h"
#include "time_axis_view.h"
#include "grid_lines.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Editing;
using namespace Temporal;

void
Editor::remove_metric_marks ()
{
	/* don't delete these while handling events, just punt till the GUI is idle */

	for (Marks::iterator x = metric_marks.begin(); x != metric_marks.end(); ++x) {
		delete_when_idle (*x);
	}
	metric_marks.clear ();

	for (Curves::iterator x = tempo_curves.begin(); x != tempo_curves.end(); ++x) {
		delete (*x);
	}
	tempo_curves.clear ();
}

struct CurveComparator {
	bool operator() (TempoCurve const * a, TempoCurve const * b) {
		return a->tempo().sclock() < b->tempo().sclock();
	}
};

void
Editor::draw_metric_marks (TempoMap::Metrics const & metrics)
{
	if (!_session) {
		return;
	}

	char buf[64];
	TempoPoint* prev_ts = 0;
	double max_tempo = 0.0;
	double min_tempo = DBL_MAX;
	const samplecnt_t sr (_session->sample_rate());

	remove_metric_marks (); // also clears tempo curves

	for (TempoMap::Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		Temporal::MeterPoint *ms;
		Temporal::TempoPoint *ts;
		Temporal::MusicTimePoint *mtp;

		/* must check MusicTimePoint first, since it IS-A TempoPoint
		 * and MeterPoint.
		 */

		if ((mtp = dynamic_cast<Temporal::MusicTimePoint*>(*i)) != 0) {

			if (ms->map().time_domain() == BeatTime) {
				metric_marks.push_back (new BBTMarker (*this, *bbt_ruler, UIConfiguration::instance().color ("meter marker music"), "bar!", *mtp));
			} else {
				metric_marks.push_back (new BBTMarker (*this, *bbt_ruler, UIConfiguration::instance().color ("meter marker"), "foo!", *mtp));
			}
		} else if ((ms = dynamic_cast<Temporal::MeterPoint*>(*i)) != 0) {
			snprintf (buf, sizeof(buf), "%d/%d", ms->divisions_per_bar(), ms->note_value ());
			if (ms->map().time_domain() == BeatTime) {
				metric_marks.push_back (new MeterMarker (*this, *meter_group, UIConfiguration::instance().color ("meter marker music"), buf, *ms));
			} else {
				metric_marks.push_back (new MeterMarker (*this, *meter_group, UIConfiguration::instance().color ("meter marker"), buf, *ms));
			}
		} else if ((ts = dynamic_cast<Temporal::TempoPoint*>(*i)) != 0) {
			max_tempo = max (max_tempo, ts->note_types_per_minute());
			max_tempo = max (max_tempo, ts->end_note_types_per_minute());
			min_tempo = min (min_tempo, ts->note_types_per_minute());
			min_tempo = min (min_tempo, ts->end_note_types_per_minute());
			uint32_t const tc_color = UIConfiguration::instance().color ("tempo curve");

			tempo_curves.push_back (new TempoCurve (*this, *tempo_group, tc_color, *ts, ts->sample (sr), false));

			const std::string tname (X_(""));
			if (ts->map().time_domain() == BeatTime) {
				metric_marks.push_back (new TempoMarker (*this, *tempo_group, UIConfiguration::instance().color ("tempo marker music"), tname, *ts));

			} else {
				metric_marks.push_back (new TempoMarker (*this, *tempo_group, UIConfiguration::instance().color ("tempo marker"), tname, *ts));
			}
			if (prev_ts && abs (prev_ts->end_note_types_per_minute() - ts->note_types_per_minute()) < 1.0) {
				metric_marks.back()->set_points_color (UIConfiguration::instance().color ("tempo marker music"));
			} else {
				metric_marks.back()->set_points_color (UIConfiguration::instance().color ("tempo marker"));
			}
			prev_ts = ts;
		}

	}
	tempo_curves.sort (CurveComparator());

	const double min_tempo_range = 5.0;
	const double tempo_delta = fabs (max_tempo - min_tempo);

	if (tempo_delta < min_tempo_range) {
		max_tempo += min_tempo_range - tempo_delta;
		min_tempo += tempo_delta - min_tempo_range;
	}

	for (Curves::iterator x = tempo_curves.begin(); x != tempo_curves.end(); ) {
		Curves::iterator tmp = x;
		(*x)->set_max_tempo (max_tempo);
		(*x)->set_min_tempo (min_tempo);
		++tmp;
		if (tmp != tempo_curves.end()) {
			(*x)->set_position ((*x)->tempo().sample(sr), (*tmp)->tempo().sample(sr));
		} else {
			(*x)->set_position ((*x)->tempo().sample(sr), UINT32_MAX);
		}

		if (!(*x)->tempo().active()) {
			(*x)->hide();
		} else {
			(*x)->show();
		}

		++x;
	}

	for (Marks::iterator x = metric_marks.begin(); x != metric_marks.end(); ++x) {
		TempoMarker* tempo_marker;

		if ((tempo_marker = dynamic_cast<TempoMarker*> (*x)) != 0) {
			tempo_marker->update_height_mark ((tempo_marker->tempo().note_types_per_minute() - min_tempo) / max (10.0, max_tempo - min_tempo));
		}
	}
}

void
Editor::tempo_map_changed ()
{
	PropertyChange pc;

	if (!_session) {
		return;
	}

	TempoMap::use()->apply_with_metrics (*this, &Editor::draw_metric_marks); // redraw metric markers

	compute_bbt_ruler_scale (_leftmost_sample, _leftmost_sample + current_page_samples());

	update_tempo_based_rulers ();

	maybe_draw_grid_lines ();
}

void
Editor::redisplay_grid (bool immediate_redraw)
{
	if (!_session) {
		return;
	}

	if (immediate_redraw) {

		update_tempo_based_rulers ();

		update_grid();

	} else {
		Glib::signal_idle().connect (sigc::bind_return (sigc::bind (sigc::mem_fun (*this, &Editor::redisplay_grid), true), false));
	}
}
void
Editor::tempo_curve_selected (Temporal::TempoPoint const * ts, bool yn)
{
	if (ts == 0) {
		return;
	}

	for (Curves::iterator x = tempo_curves.begin(); x != tempo_curves.end(); ++x) {
		if (&(*x)->tempo() == ts) {
			if (yn) {
				(*x)->set_color_rgba (UIConfiguration::instance().color ("location marker"));
			} else {
				(*x)->set_color_rgba (UIConfiguration::instance().color ("tempo curve"));
			}
			break;
		}
	}
}

/* computes a grid starting a beat before and ending a beat after leftmost and rightmost respectively */
void
Editor::compute_current_bbt_points (Temporal::TempoMapPoints& grid, samplepos_t leftmost, samplepos_t rightmost)
{
	if (!_session) {
		return;
	}

	TempoMap::SharedPtr tmap (TempoMap::use());

	/* prevent negative values of leftmost from creeping into tempomap
	 */

	const Beats left = tmap->quarters_at_sample (leftmost).round_down_to_beat();
	const Beats lower_beat = (left < Beats() ? Beats() : left);
	const samplecnt_t sr (_session->sample_rate());

	switch (bbt_ruler_scale) {

	case bbt_show_quarters:
	case bbt_show_eighths:
	case bbt_show_sixteenths:
	case bbt_show_thirtyseconds:
	case bbt_show_sixtyfourths:
	case bbt_show_onetwentyeighths:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 0);
		break;

	case bbt_show_1:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 1);
		break;

	case bbt_show_4:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 4);
		break;

	case bbt_show_16:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 16);
		break;

	case bbt_show_64:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 64);
		break;

	default:
		/* bbt_show_many */
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 128);
		break;
	}
}

void
Editor::hide_grid_lines ()
{
	if (grid_lines) {
		grid_lines->hide();
	}
}

void
Editor::maybe_draw_grid_lines ()
{
	if ( _session == 0 ) {
		return;
	}

	if (grid_lines == 0) {
		grid_lines = new GridLines (time_line_group, ArdourCanvas::LineSet::Vertical);
	}

	grid_marks.clear();
	samplepos_t rightmost_sample = _leftmost_sample + current_page_samples();

	if ( grid_musical() ) {
		 metric_get_bbt (grid_marks, _leftmost_sample, rightmost_sample, 12);
	} else if (_grid_type== GridTypeTimecode) {
		 metric_get_timecode (grid_marks, _leftmost_sample, rightmost_sample, 12);
	} else if (_grid_type == GridTypeCDFrame) {
		metric_get_minsec (grid_marks, _leftmost_sample, rightmost_sample, 12);
	} else if (_grid_type == GridTypeMinSec) {
		metric_get_minsec (grid_marks, _leftmost_sample, rightmost_sample, 12);
	}

	grid_lines->draw ( grid_marks );
	grid_lines->show();
}

void
Editor::mouse_add_new_tempo_event (timepos_t pos)
{
	if (_session == 0) {
		return;
	}

	if (pos.beats() > Beats()) {

		begin_reversible_command (_("add tempo mark"));

		TempoMap::SharedPtr map (TempoMap::write_copy());

		XMLNode &before = map->get_state();

		/* add music-locked ramped (?) tempo using the bpm/note type at sample*/

		map->set_tempo (map->tempo_at (pos), pos);
		XMLNode &after = map->get_state();
		_session->add_command (new MementoCommand<Temporal::TempoMap> (new Temporal::TempoMap::MementoBinder(), &before, &after));
		commit_reversible_command ();

		TempoMap::update (map);
	}

	//map.dump (cerr);
}

void
Editor::mouse_add_new_meter_event (timepos_t pos)
{
	if (_session == 0) {
		return;
	}

	MeterDialog meter_dialog (TempoMap::use(), pos, _("add"));

	switch (meter_dialog.run ()) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
	}

	TempoMap::SharedPtr map (TempoMap::write_copy());

	double bpb = meter_dialog.get_bpb ();
	bpb = max (1.0, bpb); // XXX is this a reasonable limit?

	double note_type = meter_dialog.get_note_type ();

	Temporal::BBT_Time requested;
	meter_dialog.get_bbt_time (requested);

	begin_reversible_command (_("add meter mark"));

	XMLNode &before = map->get_state();

	if (map->time_domain() == BeatTime) {
		pos = timepos_t (map->quarters_at (requested));
	} else {
		pos = timepos_t (map->sample_at (requested, _session->sample_rate()));
	}

	map->set_meter (Meter (bpb, note_type), pos);

	_session->add_command (new MementoCommand<Temporal::TempoMap> (new Temporal::TempoMap::MementoBinder(), &before, &map->get_state()));
	commit_reversible_command ();

	TempoMap::update (map);

	//map.dump (cerr);
}

void
Editor::remove_tempo_marker (ArdourCanvas::Item* item)
{
	ArdourMarker* marker;
	TempoMarker* tempo_marker;

	if ((marker = reinterpret_cast<ArdourMarker *> (item->get_data ("marker"))) == 0) {
		fatal << _("programming error: tempo marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	if ((tempo_marker = dynamic_cast<TempoMarker*> (marker)) == 0) {
		fatal << _("programming error: marker for tempo is not a tempo marker!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	if (!tempo_marker->tempo().locked_to_meter() && tempo_marker->tempo().active()) {
		Glib::signal_idle().connect (sigc::bind (sigc::mem_fun(*this, &Editor::real_remove_tempo_marker), &tempo_marker->tempo()));
	}
}

void
Editor::edit_meter_section (Temporal::MeterPoint& section)
{
	MeterDialog meter_dialog (section, _("done"));

	switch (meter_dialog.run()) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
	}

	double bpb = meter_dialog.get_bpb ();
	bpb = max (1.0, bpb); // XXX is this a reasonable limit?

	double const note_type = meter_dialog.get_note_type ();
	const Meter meter (bpb, note_type);

	Temporal::BBT_Time when;
	meter_dialog.get_bbt_time (when);

	TempoMap::SharedPtr tmap (TempoMap::write_copy());

	begin_reversible_command (_("replace meter mark"));
	XMLNode &before = tmap->get_state();

	tmap->set_meter (meter, when);

	XMLNode &after = tmap->get_state();
	_session->add_command (new MementoCommand<Temporal::TempoMap> (new Temporal::TempoMap::MementoBinder(), &before, &after));
	commit_reversible_command ();

	TempoMap::update (tmap);
}

void
Editor::edit_tempo_section (TempoPoint& section)
{
	TempoDialog tempo_dialog (TempoMap::use(), section, _("done"));

	switch (tempo_dialog.run ()) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
	}

	double bpm = tempo_dialog.get_bpm ();
	double end_bpm = tempo_dialog.get_end_bpm ();
	int nt = tempo_dialog.get_note_type ();
	bpm = max (0.01, bpm);

	const Tempo tempo (bpm, end_bpm, nt);

	TempoMap::SharedPtr tmap (TempoMap::write_copy());

	Temporal::BBT_Time when;
	tempo_dialog.get_bbt_time (when);

	begin_reversible_command (_("replace tempo mark"));
	XMLNode &before = tmap->get_state();


	tmap->set_tempo (tempo, when);

	XMLNode &after = tmap->get_state();
	_session->add_command (new MementoCommand<Temporal::TempoMap> (new Temporal::TempoMap::MementoBinder(), &before, &after));
	commit_reversible_command ();

	TempoMap::update (tmap);
}

void
Editor::edit_tempo_marker (TempoMarker& tm)
{
	edit_tempo_section (tm.tempo());
}

void
Editor::edit_meter_marker (MeterMarker& mm)
{
	edit_meter_section (mm.meter());
}

gint
Editor::real_remove_tempo_marker (TempoPoint *section)
{
	begin_reversible_command (_("remove tempo mark"));
	TempoMap::SharedPtr tmap (TempoMap::write_copy());
	XMLNode &before = tmap->get_state();
	tmap->remove_tempo (*section);
	XMLNode &after = tmap->get_state();
	_session->add_command (new MementoCommand<Temporal::TempoMap> (new Temporal::TempoMap::MementoBinder(), &before, &after));
	commit_reversible_command ();

	TempoMap::update (tmap);

	return FALSE;
}

void
Editor::remove_meter_marker (ArdourCanvas::Item* item)
{
	ArdourMarker* marker;
	MeterMarker* meter_marker;

	if ((marker = reinterpret_cast<ArdourMarker *> (item->get_data ("marker"))) == 0) {
		fatal << _("programming error: meter marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	if ((meter_marker = dynamic_cast<MeterMarker*> (marker)) == 0) {
		fatal << _("programming error: marker for meter is not a meter marker!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	if (!meter_marker->meter().map().is_initial(meter_marker->meter())) {
		Glib::signal_idle().connect (sigc::bind (sigc::mem_fun(*this, &Editor::real_remove_meter_marker), &meter_marker->meter()));
	}
}

gint
Editor::real_remove_meter_marker (Temporal::MeterPoint *section)
{
	begin_reversible_command (_("remove tempo mark"));
	TempoMap::SharedPtr tmap (TempoMap::write_copy());
	XMLNode &before = tmap->get_state();
	tmap->remove_meter (*section);
	XMLNode &after = tmap->get_state();
	_session->add_command (new MementoCommand<Temporal::TempoMap> (new Temporal::TempoMap::MementoBinder(), &before, &after));
	commit_reversible_command ();

	TempoMap::update (tmap);

	return FALSE;
}
