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
#include "pbd/unwind.h"

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/gtk_ui.h>

#include "ardour/session.h"
#include "ardour/tempo.h"
#include <gtkmm2ext/doi.h>
#include <gtkmm2ext/utils.h>

#include "canvas/canvas.h"
#include "canvas/item.h"
#include "canvas/line_set.h"

#include "automation_streamview.h"
#include "bbt_marker_dialog.h"
#include "editor.h"
#include "marker.h"
#include "tempo_dialog.h"
#include "rgb_macros.h"
#include "gui_thread.h"
#include "tempo_map_change.h"
#include "time_axis_view.h"
#include "grid_lines.h"
#include "region_view.h"
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

	for (auto & m : tempo_marks) {
		delete_when_idle (m);
	}
	for (auto & m : meter_marks) {
		delete_when_idle (m);
	}
	for (auto & m : bbt_marks) {
		delete_when_idle (m);
	}

	tempo_marks.clear ();
	meter_marks.clear ();
	bbt_marks.clear ();
}

void
Editor::reassociate_metric_markers (TempoMap::SharedPtr const& tmap)
{
	TempoMap::Metrics metrics;

	for (auto & t : tempo_marks) {
		reassociate_tempo_marker (tmap, tmap->tempos(), *dynamic_cast<TempoMarker*> (t));
	}
	for (auto & m : meter_marks) {
		reassociate_meter_marker (tmap, tmap->meters(), *dynamic_cast<MeterMarker*> (m));
	}
	for (auto & b : bbt_marks) {
		reassociate_bartime_marker (tmap, tmap->bartimes(), *dynamic_cast<BBTMarker*> (b));
	}
}

void
Editor::reassociate_tempo_marker (TempoMap::SharedPtr const & tmap, TempoMap::Tempos const & tempos, TempoMarker& marker)
{
	Temporal::MusicTimePoint const * mtp;

	for (auto const & tempo : tempos) {
		if ((mtp = dynamic_cast<Temporal::MusicTimePoint const *>(&tempo)) != 0) {
			/* do nothing .. but we had to catch
			   this first because MusicTimePoint
			   IS-A TempoPoint
			*/
			continue;
		}
		if (marker.point().sclock() == tempo.sclock()) {
			marker.reset_tempo (tempo);
			marker.curve().reset_point  (tempo);
			break;
		}
	}
}

void
Editor::reassociate_meter_marker (TempoMap::SharedPtr const & tmap, TempoMap::Meters const & meters, MeterMarker& marker)
{
	Temporal::MusicTimePoint const * mtp;

	for (auto const & meter : meters) {
		if ((mtp = dynamic_cast<Temporal::MusicTimePoint const *>(&meter)) != 0) {
			/* do nothing .. but we had to catch
			   this first because MusicTimePoint
			   IS-A MeterPoint
			*/
			continue;
		}
		if (marker.point().sclock() == meter.sclock()) {
			marker.reset_meter (meter);
			break;
		}
	}
}

void
Editor::reassociate_bartime_marker (TempoMap::SharedPtr const & tmap, TempoMap::MusicTimes const & bartimes, BBTMarker& marker)
{
	for (auto const & bartime : bartimes) {
		if (marker.point().sclock() == bartime.sclock()) {
			marker.reset_point (bartime);
			break;
		}
	}
}

void
Editor::make_bbt_marker (MusicTimePoint const  * mtp, Marks::iterator before)
{
	bbt_marks.insert (before, new BBTMarker (*this, *bbt_ruler, "meter marker", *mtp, *tempo_group, *mapping_group, *meter_group));
}

void
Editor::make_meter_marker (Temporal::MeterPoint const * ms, Marks::iterator before)
{
	char buf[64];

	snprintf (buf, sizeof(buf), "%d/%d", ms->divisions_per_bar(), ms->note_value ());
	meter_marks.insert (before, new MeterMarker (*this, *meter_group, "meter marker", buf, *ms));
}

void
Editor::make_tempo_marker (Temporal::TempoPoint const * ts, double& min_tempo, double& max_tempo, TempoPoint const *& prev_ts, uint32_t tc_color, samplecnt_t sr, Marks::iterator before)
{
	max_tempo = max (max_tempo, ts->note_types_per_minute());
	max_tempo = max (max_tempo, ts->end_note_types_per_minute());
	min_tempo = min (min_tempo, ts->note_types_per_minute());
	min_tempo = min (min_tempo, ts->end_note_types_per_minute());

	const std::string tname (X_(""));
	char const * color_name = X_("tempo marker");

	tempo_marks.insert (before, new TempoMarker (*this, *tempo_group, *mapping_group, color_name, tname, *ts, ts->sample (sr), tc_color));

	/* XXX the point of this code was "a jump in tempo by more than 1 ntpm results in a red
	   tempo mark pointer."  (3a7bc1fd3f32f0)
	*/

	if (prev_ts && abs (prev_ts->end_note_types_per_minute() - ts->note_types_per_minute()) < 1.0) {
		tempo_marks.back()->set_points_color ("tempo marker music");
	} else {
		tempo_marks.back()->set_points_color ("tempo marker");
	}

	prev_ts = ts;
}

void
Editor::reset_metric_marks ()
{
	reset_tempo_marks ();
	reset_meter_marks ();
	reset_bbt_marks ();
}

void
Editor::reset_tempo_marks ()
{
	if (!_session) {
		return;
	}

	const uint32_t tc_color = UIConfiguration::instance().color ("tempo curve");
	const samplecnt_t sr (_session->sample_rate());

	TempoMap::Tempos const & tempi (TempoMap::use()->tempos());
	TempoPoint const * prev_ts = 0;
	double max_tempo = 0.0;
	double min_tempo = DBL_MAX;

	for (auto & t : tempo_marks) {
		delete t;
	}

	tempo_marks.clear ();

	for (auto const & t : tempi) {

		/* do not draw BBT position elements that are both tempo & meter points */

		if (!dynamic_cast<Temporal::MusicTimePoint const *> (&t)) {
			make_tempo_marker (&t, min_tempo, max_tempo, prev_ts, tc_color, sr, tempo_marks.end());
			prev_ts = &t;
		}
	}

	update_tempo_curves (min_tempo, max_tempo, sr);
}

void
Editor::reset_meter_marks ()
{
	if (!_session) {
		return;
	}

	TempoMap::Meters const & meters (TempoMap::use()->meters());

	for (auto & m : meter_marks) {
		delete m;
	}

	meter_marks.clear ();

	for (auto const & m : meters) {

		/* do not draw BBT position elements that are both tempo & meter points */

		if (!dynamic_cast<Temporal::MusicTimePoint const *> (&m)) {
			make_meter_marker (&m, meter_marks.end());
		}
	}
}

void
Editor::reset_bbt_marks ()
{
	if (!_session) {
		return;
	}

	Temporal::TempoMap::SharedPtr tmap (TempoMap::use());
	TempoMap::MusicTimes const & bartimes (tmap->bartimes());

	for (auto & b : bbt_marks) {
		delete b;
	}

	bbt_marks.clear ();

	for (auto const & b : bartimes) {
		make_bbt_marker (&b, bbt_marks.end());
	}
}

void
Editor::update_tempo_curves (double min_tempo, double max_tempo, samplecnt_t sr)
{
	const double min_tempo_range = 5.0;
	const double tempo_delta = fabs (max_tempo - min_tempo);

	if (tempo_delta < min_tempo_range) {
		max_tempo += min_tempo_range - tempo_delta;
		min_tempo += tempo_delta - min_tempo_range;
	}

	for (Marks::iterator m = tempo_marks.begin(); m != tempo_marks.end(); ++m) {

		TempoMarker* tm = static_cast<TempoMarker*>(*m);
		Marks::iterator tmp = m;
		++tmp;

		TempoCurve& curve (tm->curve());

		curve.set_max_tempo (max_tempo);
		curve.set_min_tempo (min_tempo);

		if (tmp != tempo_marks.end()) {
			TempoMarker* nxt = static_cast<TempoMarker*>(*tmp);
			curve.set_duration (nxt->tempo().sample(sr) - tm->tempo().sample(sr));
		} else {
			curve.set_duration (samplecnt_t (UINT32_MAX));
		}

		if (!tm->tempo().active()) {
			curve.hide();
		} else {
			curve.show();
		}
	}
}

void
Editor::tempo_map_changed ()
{
	if (ignore_map_change) {
		return;
	}

	TempoMap::SharedPtr current_map = TempoMap::fetch ();

	/* If the tempo map was changed by something other than the Editor, we
	 * will need to reassociate all visual elements used for tempo display
	 * with the new map.
	 */

	 reset_metric_marks ();
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

	for (Marks::iterator x = tempo_marks.begin(); x != tempo_marks.end(); ++x) {
		TempoMarker* tm = static_cast<TempoMarker*> (*x);
		if (&tm->tempo() == ts) {
			if (yn) {
				tm->curve().set_color_rgba (UIConfiguration::instance().color ("location marker"));
			} else {
				tm->curve().set_color_rgba (UIConfiguration::instance().color ("tempo curve"));
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
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 0, 1);
		break;
	case bbt_show_eighths:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 0, 2);
		break;
	case bbt_show_sixteenths:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 0, 4);
		break;
	case bbt_show_thirtyseconds:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 0, 8);
		break;
	case bbt_show_sixtyfourths:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 0, 16);
		break;
	case bbt_show_onetwentyeighths:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 0, 32);
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

	grid_lines->draw (grid_marks);
	grid_lines->show();
}

void
Editor::mouse_add_new_tempo_event (timepos_t pos)
{
	if (_session == 0) {
		return;
	}

	if (pos.beats() > Beats()) {
		TempoMapChange tmc (*this, _("add tempo mark"));
		tmc.map().set_tempo (tmc.map().tempo_at (pos), pos);
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


	double bpb = meter_dialog.get_bpb ();
	bpb = max (1.0, bpb); // XXX is this a reasonable limit?

	double note_type = meter_dialog.get_note_type ();

	Temporal::BBT_Time r;
	meter_dialog.get_bbt_time (r);
	Temporal::BBT_Argument requested (timepos_t::zero (Temporal::BeatTime), r);

	TempoMapChange tmc (*this, _("add time signature"));
	pos = timepos_t (tmc.map().quarters_at (requested));
	tmc.map().set_meter (Meter (bpb, note_type), pos);
}

void
Editor::add_bbt_marker_at_playhead_cursor ()
{
	if (_session == 0) {
		return;
	}

	mouse_add_bbt_marker_event (timepos_t (_session->transport_sample ()));
}

void
Editor::mouse_add_bbt_marker_event (timepos_t pos)
{
	if (_session == 0) {
		return;
	}

	/* position markers must always be positioned using audio time */

	BBTMarkerDialog marker_dialog  (timepos_t (pos.samples()), BBT_Time ());

	/* run this modally since we are finishing a drag and the drag object
	 * will be destroyed when we return from here
	 */

	int result = marker_dialog.run ();

	switch (result) {
	case RESPONSE_ACCEPT:
	case RESPONSE_OK:
		break;
	default:
		return;
	}

	BBT_Time bbt;
	std::string name;

	bbt = marker_dialog.bbt_value ();
	name = marker_dialog.name();

	TempoMapChange tmc (*this, _("add BBT marker"));
	tmc.map().set_bartime (bbt, marker_dialog.position(), name);
}

void
Editor::remove_bbt_marker (ArdourCanvas::Item* item)
{
	ArdourMarker* marker;
	BBTMarker* bbt_marker;

	if ((marker = reinterpret_cast<ArdourMarker *> (item->get_data ("marker"))) == 0) {
		fatal << _("programming error: bbt marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	if ((bbt_marker = dynamic_cast<BBTMarker*> (marker)) == 0) {
		fatal << _("programming error: marker for bbt is not a bbt marker!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	Glib::signal_idle().connect (sigc::bind (sigc::mem_fun(*this, &Editor::real_remove_bbt_marker), &bbt_marker->mt_point()));
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

	Temporal::TempoMetric tm (TempoMap::use()->metric_at (timepos_t (section.sample(TEMPORAL_SAMPLE_RATE))));
	Temporal::MeterPoint const * mpp (TempoMap::use()->previous_meter (tm.meter()));

	double bpb = meter_dialog.get_bpb ();
	bpb = max (1.0, bpb); // XXX is this a reasonable limit?

	double const note_type = meter_dialog.get_note_type ();
	const Meter meter (bpb, note_type);

	Temporal::Beats new_pos;

	if (!mpp) {
		/* first meter, cannot move */
		new_pos = section.beats ();
	} else {
		/* Compute the given BBT time using a tempo metric composed
		   from the tempo in effect at the current position, and the
		   previous meter.

		   Step 1: get BBT time from dialog
		*/

		Temporal::BBT_Time w;
		meter_dialog.get_bbt_time (w);

		/* Step 2: construct the relevant tempo metric */

		TempoMetric prev_tm (tm.tempo(), *mpp);

		/* Step 3: construct new BBT_Argument */

		Temporal::BBT_Argument when (tm.reftime(), w);

		/* Step 4: convert to quarters */

		new_pos = prev_tm.quarters_at (when);
	}

	TempoMapChange tmc (*this, _("edit time signature"));
	tmc.map().set_meter (meter, timepos_t (new_pos));
}

void
Editor::edit_bbt (MusicTimePoint& point)
{
	BBTMarkerDialog dialog (point);

	switch (dialog.run ()) {
	case RESPONSE_OK:
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
	}

	if (dialog.bbt_value() == point.bbt()) {
		/* just a name change, no need to modify the map */
		point.set_name (dialog.name());
		/* XXX need to update marker label */
		return;
	}

	TempoMapChange tmc (*this, _("edit tempo"));
	tmc.map().remove_bartime (point);
	tmc.map().set_bartime (dialog.bbt_value(), dialog.position(), dialog.name());
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

	Temporal::TempoMetric tm (TempoMap::use()->metric_at (timepos_t (section.sample (TEMPORAL_SAMPLE_RATE))));
	Temporal::TempoPoint const * tpp (TempoMap::use()->previous_tempo (tm.tempo()));

	double bpm = tempo_dialog.get_bpm ();
	double end_bpm = tempo_dialog.get_end_bpm ();
	int nt = tempo_dialog.get_note_type ();
	bpm = max (0.01, bpm);

	const Tempo tempo (bpm, end_bpm, nt);

	Temporal::Beats new_pos;

	if (!tpp) {
		/* first tempo, cannot move */
		new_pos = section.beats ();
	} else {
		/* Compute the given BBT time using a tempo metric composed
		   from the meter in effect at the current position, and the
		   previous tempo.

		   Step 1: get BBT time from dialog
		*/

		Temporal::BBT_Time w;
		tempo_dialog.get_bbt_time (w);

		/* Step 2: construct the relevant tempo metric */

		TempoMetric prev_tm (*tpp, tm.meter());

		/* Step 3: construct new BBT_Argument */

		Temporal::BBT_Argument when (tm.reftime(), w);

		/* Step 4: convert to quarters */

		new_pos = prev_tm.quarters_at (when).round_to_beat ();
	}

	TempoMapChange tmc (*this, _("edit tempo"));

	// std::cerr << "using tempometric " << tm << std::endl;
	// std::cerr << "edit tempo at " << when << " via quarters at = " << tmc.map().quarters_at (when) << std::endl;

	tmc.map().replace_tempo (section, tempo, timepos_t (new_pos));
}

void
Editor::edit_tempo_marker (TempoMarker& tm)
{
	edit_tempo_section (const_cast<Temporal::TempoPoint&>(tm.tempo()));
}

void
Editor::edit_meter_marker (MeterMarker& mm)
{
	edit_meter_section (const_cast<Temporal::MeterPoint&>(mm.meter()));
}

void
Editor::edit_bbt_marker (BBTMarker& bm)
{
	edit_bbt (const_cast<Temporal::MusicTimePoint&>(bm.mt_point()));
}

gint
Editor::real_remove_bbt_marker (MusicTimePoint const * point)
{
	TempoMapChange tmc (*this, _("remove BBT marker"));
	tmc.map().remove_bartime (*point);
	return FALSE;
}

gint
Editor::real_remove_tempo_marker (TempoPoint const * section)
{
	TempoMapChange tmc (*this, _("remove tempo change"));
	tmc.map().remove_tempo (*section);
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
Editor::real_remove_meter_marker (Temporal::MeterPoint const * section)
{
	TempoMapChange tmc (*this, _("remove tempo mark"));
	tmc.map().remove_meter (*section);
	return FALSE;
}


Temporal::TempoMap::WritableSharedPtr
Editor::begin_tempo_mapping ()
{
	TempoMap::WritableSharedPtr wmap = TempoMap::write_copy ();
	TempoMap::set (wmap);
	reassociate_metric_markers (wmap);
	(void) Temporal::DomainSwapInformation::start (Temporal::BeatTime);
	_session->globally_change_time_domain (Temporal::BeatTime, Temporal::AudioTime);
	return wmap;
}

void
Editor::abort_tempo_mapping ()
{
	delete domain_swap; /* undo the domain swap */
	domain_swap = 0;

	TempoMap::abort_update ();
	TempoMap::SharedPtr tmap (TempoMap::fetch());
	reassociate_metric_markers (tmap);
}

void
Editor::commit_tempo_mapping (TempoMap::WritableSharedPtr& new_map)
{
	TempoMap::update (new_map);
	delete domain_swap; /* undo the domain swap */
	domain_swap = 0;
	TempoMap::SharedPtr tmap (TempoMap::fetch());
	reassociate_metric_markers (tmap);
}

Temporal::TempoMap::WritableSharedPtr
Editor::begin_tempo_map_edit ()
{
	TempoMap::WritableSharedPtr wmap = TempoMap::write_copy ();
	TempoMap::set (wmap);
	reassociate_metric_markers (wmap);
	return wmap;
}

void
Editor::abort_tempo_map_edit ()
{
	/* this drops the lock held while we have a writable copy in our per-thread pointer */
	TempoMap::abort_update ();

	/* Now update our own per-thread copy of the tempo map pointer to be
	   the canonical one, and reconnect markers with elements of that map
	*/
	TempoMap::SharedPtr tmap (TempoMap::fetch());
	reassociate_metric_markers (tmap);
}

void
Editor::_commit_tempo_map_edit (TempoMap::WritableSharedPtr& new_map, bool with_update)
{
	if (!with_update) {
		PBD::Unwinder<bool> uw (ignore_map_change, true);
		TempoMap::update (new_map);
	} else {
		TempoMap::update (new_map);
	}
}

void
Editor::mid_tempo_change (MidTempoChanges what_changed)
{
	// std::cerr << "============== MID TEMPO\n";
	// TempoMap::SharedPtr map (TempoMap::use());
	// map->dump (std::cerr);

	if ((what_changed & MidTempoChanges(BBTChanged|TempoChanged|MappingChanged))) {
		double min_tempo = DBL_MAX;
		double max_tempo = 0.0;

		for (auto & t : tempo_marks) {
			t->update ();

			TempoMarker* tm (dynamic_cast<TempoMarker*> (t));

			max_tempo = max (max_tempo, tm->tempo().note_types_per_minute());
			max_tempo = max (max_tempo, tm->tempo().end_note_types_per_minute());
			min_tempo = min (min_tempo, tm->tempo().note_types_per_minute());
			min_tempo = min (min_tempo, tm->tempo().end_note_types_per_minute());

		}
		update_tempo_curves (min_tempo, max_tempo, _session->sample_rate());
	}

	for (auto & m : meter_marks) {
		m->update ();
	}

	for (auto & b : bbt_marks) {
		b->update ();
	}

	update_tempo_based_rulers ();
	maybe_draw_grid_lines ();

	if (!(what_changed & (MappingChanged|BBTChanged))) {
		/* Nothing changes in tracks when it is a tempo mapping
		 * operation or a BBT change
		 */
		foreach_time_axis_view (sigc::mem_fun (*this, &Editor::mid_tempo_per_track_update));
	}
}

void
Editor::mid_tempo_per_track_update (TimeAxisView& tav)
{
	MidiTimeAxisView* mtav = dynamic_cast<MidiTimeAxisView*> (&tav);

	if (mtav) {
		MidiStreamView* msv = mtav->midi_view();

		if (msv) {
			msv->foreach_regionview (sigc::mem_fun (*this, &Editor::mid_tempo_per_region_update));
		}

		TimeAxisView::Children kids (tav.get_child_list());

		for (TimeAxisView::Children::iterator ct = kids.begin(); ct != kids.end(); ++ct) {

			std::shared_ptr<AutomationTimeAxisView> atav = std::dynamic_pointer_cast<AutomationTimeAxisView> (*ct);

			if (atav) {
				AutomationStreamView* asv = atav->automation_view ();

				if (asv) {
					asv->foreach_regionview (sigc::mem_fun (*this, &Editor::mid_tempo_per_region_update));
				}
			}
		}
	}
}

void
Editor::mid_tempo_per_region_update (RegionView* rv)
{
	rv->tempo_map_changed ();
}

void
Editor::set_tempo_edit_behavior (TempoEditBehavior teb)
{
	/* As with all things radio-action related, we carry out the change by
	   toggling the action, and then actually do the model-view changes in
	   the actions' toggled handler.
	*/

	Glib::RefPtr<Action> act;

	switch (teb) {
	case TempoMapping:
		act = ActionManager::get_action (X_("Editor"), X_("tempo-edit-is-mapping"));
		break;
	case TempoChanging:
		act = ActionManager::get_action (X_("Editor"), X_("tempo-edit-is-changing"));
	}

	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

	/* go there and back to ensure that the toggled handler is called to set up mouse_mode */
	tact->set_active (false);
	tact->set_active (true);
}

void
Editor::tempo_edit_behavior_toggled (TempoEditBehavior teb)
{
	Glib::RefPtr<Action> act;

	switch (teb) {
	case TempoMapping:
		act = ActionManager::get_action (X_("Editor"), X_("tempo-edit-is-mapping"));
		break;
	case TempoChanging:
		act = ActionManager::get_action (X_("Editor"), X_("tempo-edit-is-changing"));
	}

	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

	if (!tact->get_active()) {
		/* this was just the notification that the old mode has been
		 * left. we'll get called again with the new mode active in a
		 * jiffy.
		 */
		return;
	}

	/* change the ruler shown in the tempo position */
	_tempo_edit_behavior = teb;

	switch (teb) {
	case TempoMapping:
		tempo_group->hide ();
		mapping_group->show ();
		break;
	case TempoChanging:
		tempo_group->show ();
		mapping_group->hide ();
		break;
	}
}
