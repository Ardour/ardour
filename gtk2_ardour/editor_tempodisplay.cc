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
Editor::reassociate_metric_markers (TempoMap::SharedPtr const & tmap)
{
	TempoMap::Metrics metrics;
	tmap->get_metrics (metrics);

	for (auto & m : tempo_marks) {
		reassociate_metric_marker (tmap, metrics, *m);
	}
	for (auto & m : meter_marks) {
		reassociate_metric_marker (tmap, metrics, *m);
	}
	for (auto & m : bbt_marks) {
		reassociate_metric_marker (tmap, metrics, *m);
	}
}

void
Editor::reassociate_metric_marker (TempoMap::SharedPtr const & tmap, TempoMap::Metrics & metrics, MetricMarker& marker)
{
	TempoMarker* tm;
	MeterMarker* mm;
	BBTMarker* bm;

	Temporal::TempoPoint* tp;
	Temporal::MeterPoint* mp;
	Temporal::MusicTimePoint* mtp;

	if ((tm = dynamic_cast<TempoMarker*> (&marker)) != 0) {

		for (TempoMap::Metrics::iterator m = metrics.begin(); m != metrics.end(); ++m) {
			if ((mtp = dynamic_cast<Temporal::MusicTimePoint*>(*m)) != 0) {
				/* do nothing .. but we had to catch
				   this first because MusicTimePoint
				   IS-A TempoPoint
				*/
			} else if ((tp = dynamic_cast<Temporal::TempoPoint*>(*m)) != 0) {
				if (tm->tempo() == *tp) {
					tm->reset_tempo (*tp);
					break;
				}
			}
		}
	} else if ((mm = dynamic_cast<MeterMarker*> (&marker)) != 0) {
		for (TempoMap::Metrics::iterator m = metrics.begin(); m != metrics.end(); ++m) {
			if ((mtp = dynamic_cast<Temporal::MusicTimePoint*>(*m)) != 0) {
				/* do nothing .. but we had to catch
				   this first because MusicTimePoint
				   IS-A TempoPoint
				*/

			} else if ((mp = dynamic_cast<Temporal::MeterPoint*>(*m)) != 0) {
				if (mm->meter() == *mp) {
					mm->reset_meter (*mp);
					break;
				}
			}
		}

	} else if ((bm = dynamic_cast<BBTMarker*> (&marker)) != 0) {

		for (TempoMap::Metrics::iterator m = metrics.begin(); m != metrics.end(); ++m) {
			if ((mtp = dynamic_cast<Temporal::MusicTimePoint*>(*m)) != 0) {
				if (bm->point() == *mtp) {
					bm->reset_point (*mtp);
					break;
				}
			}
		}
	}
}

void
Editor::make_bbt_marker (MusicTimePoint const  * mtp)
{
	if (mtp->map().time_domain() == BeatTime) {
		bbt_marks.push_back (new BBTMarker (*this, *bbt_ruler, UIConfiguration::instance().color ("meter marker music"), "bar!", *mtp));
	} else {
		bbt_marks.push_back (new BBTMarker (*this, *bbt_ruler, UIConfiguration::instance().color ("meter marker"), "foo!", *mtp));
	}
}

void
Editor::make_meter_marker (Temporal::MeterPoint const * ms)
{
	char buf[64];

	snprintf (buf, sizeof(buf), "%d/%d", ms->divisions_per_bar(), ms->note_value ());
	if (ms->map().time_domain() == BeatTime) {
		meter_marks.push_back (new MeterMarker (*this, *meter_group, UIConfiguration::instance().color ("meter marker music"), buf, *ms));
	} else {
		meter_marks.push_back (new MeterMarker (*this, *meter_group, UIConfiguration::instance().color ("meter marker"), buf, *ms));
	}
}

void
Editor::make_tempo_marker (Temporal::TempoPoint const * ts, double& min_tempo, double& max_tempo, TempoPoint const *& prev_ts, uint32_t tc_color, samplecnt_t sr)
{
	max_tempo = max (max_tempo, ts->note_types_per_minute());
	max_tempo = max (max_tempo, ts->end_note_types_per_minute());
	min_tempo = min (min_tempo, ts->note_types_per_minute());
	min_tempo = min (min_tempo, ts->end_note_types_per_minute());

	const std::string tname (X_(""));
	char const * color_name;

	/* XXX not sure this is the right thing to do here (differentiate time
	 * domains with color).
	 */

	if (ts->map().time_domain() == BeatTime) {
		color_name = X_("tempo marker music");
	} else {
		color_name = X_("tempo marker music");
	}

	tempo_marks.push_back (new TempoMarker (*this, *tempo_group, UIConfiguration::instance().color (color_name), tname, *ts, ts->sample (sr), tc_color));

	/* XXX the point of this code was "a jump in tempo by more than 1 ntpm results in a red
	   tempo mark pointer."  (3a7bc1fd3f32f0)
	*/

	if (prev_ts && abs (prev_ts->end_note_types_per_minute() - ts->note_types_per_minute()) < 1.0) {
		tempo_marks.back()->set_points_color (UIConfiguration::instance().color ("tempo marker music"));
	} else {
		tempo_marks.back()->set_points_color (UIConfiguration::instance().color ("tempo marker"));
	}

	prev_ts = ts;
}

void
Editor::draw_metric_marks (Temporal::TempoMap::Metrics const &)
{
	draw_tempo_marks ();
	draw_meter_marks ();
	draw_bbt_marks ();
}

void
Editor::draw_tempo_marks ()
{
	if (!_session) {
		return;
	}

	const uint32_t tc_color = UIConfiguration::instance().color ("tempo curve");
	const samplecnt_t sr (_session->sample_rate());
	TempoPoint const * prev_ts = 0;
	Temporal::TempoMap::SharedPtr tmap (TempoMap::use());
	TempoMap::Tempos const & tempi (tmap->tempos());
	TempoMap::Tempos::const_iterator t = tempi.begin();
	Marks::iterator mm = tempo_marks.begin();
	double max_tempo = 0.0;
	double min_tempo = DBL_MAX;

	std::cerr << "**** BEGIN DRAW TEMPO\n";

	while (t != tempi.end() && mm != tempo_marks.end()) {

		Temporal::Point const & mark_point ((*mm)->point());
		Temporal::TempoPoint const & metric_point (*t);

		std::cerr << "\tmark @ " << mark_point.sclock() << " tempo @ " << metric_point.sclock() << std::endl;

		if (mark_point.sclock() < metric_point.sclock()) {

			/* advance through markers, deleting the unused ones */

			std::cerr << "\tDeleting marker that doesn't match a tempo point\n";
			delete *mm;
			mm = tempo_marks.erase (mm);


		} else if (metric_point.sclock() < mark_point.sclock()) {

			std::cerr << "\tCreating a marker for " << metric_point << " @ " << metric_point.sample (sr) << " next marker @ " << mark_point.sample (sr) << std::endl;
			make_tempo_marker (&metric_point, min_tempo, max_tempo, prev_ts, tc_color, sr);
			++t;

		} else {
			/* marker represents an existing point, update text, properties etc */
			/* XXX left/right text stuff */
			// (*mm)->set_name ((*m)->name());
			std::cerr << "\tMoving marker to " << t->time() << std::endl;
			(*mm)->set_position (t->time());

			max_tempo = max (max_tempo, t->note_types_per_minute());
			max_tempo = max (max_tempo, t->end_note_types_per_minute());
			min_tempo = min (min_tempo, t->note_types_per_minute());
			min_tempo = min (min_tempo, t->end_note_types_per_minute());

			++t;
			++mm;
		}
	}

	if ((t == tempi.end()) && (mm != tempo_marks.end())) {
		while (mm != tempo_marks.end()) {
			std::cerr << "\tdrop excess tempo marker @ " << (*mm)->point().time() << std::endl;
			delete *mm;
			mm = tempo_marks.erase (mm);
		}
	}

	if ((mm == tempo_marks.end()) && (t != tempi.end())) {
		while (t != tempi.end()) {
			std::cerr << "\tmake new tempo marker @ " << t->time() << std::endl;
			make_tempo_marker (&*t, min_tempo, max_tempo, prev_ts, tc_color, sr);
			++t;
		}
	}

	update_tempo_curves (min_tempo, max_tempo, sr);
}

void
Editor::draw_meter_marks ()
{
	if (!_session) {
		return;
	}

	Temporal::TempoMap::SharedPtr tmap (TempoMap::use());
	TempoMap::Meters const & meters (tmap->meters());
	TempoMap::Meters::const_iterator m = meters.begin();
	Marks::iterator mm = meter_marks.begin();

	while (m != meters.end() && mm != meter_marks.end()) {

		Temporal::Point const & mark_point ((*mm)->point());
		Temporal::MeterPoint const & metric_point (*m);

		if (mark_point.sclock() < metric_point.sclock()) {

			/* advance through markers, deleting the unused ones */

			delete *mm;
			mm = meter_marks.erase (mm);

		} else if (metric_point.sclock() < mark_point.sclock()) {

			make_meter_marker (&metric_point);
			++m;

		} else {
			/* marker represents an existing point, update text, properties etc */
			/* XXX left/right text stuff */
			// (*mm)->set_name ((*m)->name());
			(*mm)->set_position (m->time());
			++m;
			++mm;
		}
	}

	if ((m == meters.end()) && (mm != meter_marks.end())) {
		while (mm != meter_marks.end()) {
			delete *mm;
			mm = meter_marks.erase (mm);
		}
	}

	if ((mm == meter_marks.end()) && (m != meters.end())) {
		while (m != meters.end()) {
			make_meter_marker (&*m);
			++m;
		}
	}
}

void
Editor::draw_bbt_marks ()
{
	if (!_session) {
		return;
	}

	Temporal::TempoMap::SharedPtr tmap (TempoMap::use());
	TempoMap::MusicTimes const & bartimes (tmap->bartimes());
	TempoMap::MusicTimes::const_iterator m = bartimes.begin();
	Marks::iterator mm = bbt_marks.begin();

	while (m != bartimes.end() && mm != bbt_marks.end()) {

		Temporal::Point const & mark_point ((*mm)->point());
		Temporal::MeterPoint const & metric_point (*m);

		if (mark_point.sclock() < metric_point.sclock()) {

			/* advance through markers, deleting the unused ones */

			delete *mm;
			mm = bbt_marks.erase (mm);

		} else if (metric_point.sclock() < mark_point.sclock()) {

			make_meter_marker (&metric_point);
			++m;

		} else {
			/* marker represents an existing point, update text, properties etc */
			/* XXX left/right text stuff */
			// (*mm)->set_name ((*m)->name());
			(*mm)->set_position (m->time());
			++m;
			++mm;
		}
	}

	if ((m == bartimes.end()) && (mm != bbt_marks.end())) {
		while (mm != bbt_marks.end()) {
			delete *mm;
			mm = bbt_marks.erase (mm);
		}
	}

	if ((mm == bbt_marks.end()) && (m != bartimes.end())) {
		while (m != bartimes.end()) {
			make_meter_marker (&*m);
			++m;
		}
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
	TempoMap::Metrics metrics;
	TempoMap::fetch()->get_metrics (metrics);

	draw_metric_marks (metrics);
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
		pos = timepos_t (map->sample_at (requested));
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

	reassociate_metric_markers (tmap);

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
	reassociate_metric_markers (tmap);

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
	edit_tempo_section (const_cast<Temporal::TempoPoint&>(tm.tempo()));
}

void
Editor::edit_meter_marker (MeterMarker& mm)
{
	edit_meter_section (const_cast<Temporal::MeterPoint&>(mm.meter()));
}

gint
Editor::real_remove_tempo_marker (TempoPoint const * section)
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
Editor::real_remove_meter_marker (Temporal::MeterPoint const * section)
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

void
Editor::begin_tempo_map_edit ()
{
	TempoMap::fetch_writable ();
	TempoMap::SharedPtr tmap (TempoMap::use());
	reassociate_metric_markers (tmap);
}

void
Editor::abort_tempo_map_edit ()
{
	/* this drops the lock held while we have a writable copy in our per-thread pointer */
	TempoMap::abort_update ();

	TempoMap::SharedPtr tmap (TempoMap::fetch());
	reassociate_metric_markers (tmap);
}

void
Editor::commit_tempo_map_edit ()
{
	TempoMap::SharedPtr tmap (TempoMap::use());
	TempoMap::update (tmap);
}

void
Editor::mid_tempo_change ()
{
	std::cerr << "============== MID TEMPO\n";
	draw_tempo_marks ();
}
