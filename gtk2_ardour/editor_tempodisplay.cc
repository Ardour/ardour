/*
    Copyright (C) 2002 Paul Davis

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
#include "tempo_lines.h"
#include "ui_config.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Editing;

void
Editor::remove_metric_marks ()
{
	/* don't delete these while handling events, just punt till the GUI is idle */

	for (Marks::iterator x = metric_marks.begin(); x != metric_marks.end(); ++x) {
		delete_when_idle (*x);
	}
	metric_marks.clear ();
}

void
Editor::draw_metric_marks (const Metrics& metrics)
{

	const MeterSection *ms;
	const TempoSection *ts;
	char buf[64];

	remove_metric_marks ();

	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {

		if ((ms = dynamic_cast<const MeterSection*>(*i)) != 0) {
			snprintf (buf, sizeof(buf), "%g/%g", ms->divisions_per_bar(), ms->note_divisor ());
			metric_marks.push_back (new MeterMarker (*this, *meter_group, UIConfiguration::instance().color ("meter marker"), buf,
								 *(const_cast<MeterSection*>(ms))));
		} else if ((ts = dynamic_cast<const TempoSection*>(*i)) != 0) {
			if (UIConfiguration::instance().get_allow_non_quarter_pulse()) {
				snprintf (buf, sizeof (buf), "%.2f/%.0f", ts->beats_per_minute(), ts->note_type());
			} else {
				snprintf (buf, sizeof (buf), "%.2f", ts->beats_per_minute());
			}
			metric_marks.push_back (new TempoMarker (*this, *tempo_group, UIConfiguration::instance().color ("tempo marker"), buf,
								 *(const_cast<TempoSection*>(ts))));
		}

	}

}


void
Editor::tempo_map_changed (const PropertyChange& /*ignored*/)
{
	if (!_session) {
		return;
	}

	ENSURE_GUI_THREAD (*this, &Editor::tempo_map_changed, ignored);

	if (tempo_lines) {
		tempo_lines->tempo_map_changed();
	}

	std::vector<TempoMap::BBTPoint> grid;

	compute_current_bbt_points (grid, leftmost_frame, leftmost_frame + current_page_samples());
	_session->tempo_map().apply_with_metrics (*this, &Editor::draw_metric_marks); // redraw metric markers
	draw_measures (grid);
	update_tempo_based_rulers (grid);
}

void
Editor::marker_position_changed ()
{
	if (!_session) {
		return;
	}

	ENSURE_GUI_THREAD (*this, &Editor::tempo_map_changed);

	if (tempo_lines) {
		tempo_lines->tempo_map_changed();
	}
	TempoMarker* tempo_marker;
	MeterMarker* meter_marker;
	const TempoSection *ts;
	const MeterSection *ms;

	for (Marks::iterator x = metric_marks.begin(); x != metric_marks.end(); ++x) {
		if ((tempo_marker = dynamic_cast<TempoMarker*> (*x)) != 0) {
			if ((ts = &tempo_marker->tempo()) != 0) {
				cerr << "tempo section found for tempo marker " << endl;
				tempo_marker->set_position (ts->frame ());
			}
		}
		if ((meter_marker = dynamic_cast<MeterMarker*> (*x)) != 0) {
			if ((ms = &meter_marker->meter()) != 0) {
				cerr << "meter section found for meter marker " << endl;
				meter_marker->set_position (ms->frame ());
			}
		}
	}
	std::vector<TempoMap::BBTPoint> grid;
	compute_current_bbt_points (grid, leftmost_frame, leftmost_frame + current_page_samples());
	draw_measures (grid);
	update_tempo_based_rulers (grid);
}

void
Editor::redisplay_tempo (bool immediate_redraw)
{
	if (!_session) {
		return;
	}

	if (immediate_redraw) {
		std::vector<TempoMap::BBTPoint> grid;

		compute_current_bbt_points (grid, leftmost_frame, leftmost_frame + current_page_samples());
		draw_measures (grid);
		update_tempo_based_rulers (grid); // redraw rulers and measure lines

	} else {
		Glib::signal_idle().connect (sigc::bind_return (sigc::bind (sigc::mem_fun (*this, &Editor::redisplay_tempo), true), false));
	}
}

/* computes a grid starting a beat before and ending a beat after leftmost and rightmost respectively */
void
Editor::compute_current_bbt_points (std::vector<TempoMap::BBTPoint>& grid, framepos_t leftmost, framepos_t rightmost)
{
	if (!_session) {
		return;
	}

	framecnt_t beat_before_lower_pos = _session->tempo_map().frame_at_beat (floor(_session->tempo_map().beat_at_frame (leftmost)));
	framecnt_t beat_after_upper_pos = _session->tempo_map().frame_at_beat (floor (_session->tempo_map().beat_at_frame (rightmost)) + 1.0);

	/* prevent negative values of leftmost from creeping into tempomap
	 */
	_session->tempo_map().get_grid (grid, max (beat_before_lower_pos, (framepos_t) 0), beat_after_upper_pos);
}

void
Editor::hide_measures ()
{
	if (tempo_lines) {
		tempo_lines->hide();
	}
}

void
Editor::draw_measures (std::vector<ARDOUR::TempoMap::BBTPoint>& grid)
{
	if (_session == 0 || _show_measures == false || distance (grid.begin(), grid.end()) == 0) {
		return;
	}

	if (tempo_lines == 0) {
		tempo_lines = new TempoLines (time_line_group, ArdourCanvas::LineSet::Vertical);
	}

	const unsigned divisions = get_grid_beat_divisions(leftmost_frame);
	tempo_lines->draw (grid, divisions, leftmost_frame, _session->frame_rate());
}

void
Editor::mouse_add_new_tempo_event (framepos_t frame)
{
	if (_session == 0) {
		return;
	}

	TempoMap& map(_session->tempo_map());
	TempoDialog tempo_dialog (map, frame, _("add"));

	//this causes compiz to display no border.
	//tempo_dialog.signal_realize().connect (sigc::bind (sigc::ptr_fun (set_decoration), &tempo_dialog, Gdk::WMDecoration (Gdk::DECOR_BORDER|Gdk::DECOR_RESIZEH)));

	switch (tempo_dialog.run()) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
	}

	double bpm = 0;
	Timecode::BBT_Time requested;

	bpm = tempo_dialog.get_bpm ();
	double nt = tempo_dialog.get_note_type();
	bpm = max (0.01, bpm);

	tempo_dialog.get_bbt_time (requested);

	begin_reversible_command (_("add tempo mark"));
        XMLNode &before = map.get_state();
	map.add_tempo (Tempo (bpm,nt), requested, tempo_dialog.get_tempo_type());
        XMLNode &after = map.get_state();
	_session->add_command(new MementoCommand<TempoMap>(map, &before, &after));
	commit_reversible_command ();

	//map.dump (cerr);
}

void
Editor::mouse_add_new_meter_event (framepos_t frame)
{
	if (_session == 0) {
		return;
	}


	TempoMap& map(_session->tempo_map());
	MeterDialog meter_dialog (map, frame, _("add"));

	//this causes compiz to display no border..
	//meter_dialog.signal_realize().connect (sigc::bind (sigc::ptr_fun (set_decoration), &meter_dialog, Gdk::WMDecoration (Gdk::DECOR_BORDER|Gdk::DECOR_RESIZEH)));

	switch (meter_dialog.run ()) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
	}

	double bpb = meter_dialog.get_bpb ();
	bpb = max (1.0, bpb); // XXX is this a reasonable limit?

	double note_type = meter_dialog.get_note_type ();
	Timecode::BBT_Time requested;

	meter_dialog.get_bbt_time (requested);

	begin_reversible_command (_("add meter mark"));
        XMLNode &before = map.get_state();
	map.add_meter (Meter (bpb, note_type), requested);
	_session->add_command(new MementoCommand<TempoMap>(map, &before, &map.get_state()));
	commit_reversible_command ();

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

	if (tempo_marker->tempo().movable()) {
		Glib::signal_idle().connect (sigc::bind (sigc::mem_fun(*this, &Editor::real_remove_tempo_marker), &tempo_marker->tempo()));
	}
}

void
Editor::edit_meter_section (MeterSection* section)
{
	MeterDialog meter_dialog (*section, _("done"));

	switch (meter_dialog.run()) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
	}

	double bpb = meter_dialog.get_bpb ();
	bpb = max (1.0, bpb); // XXX is this a reasonable limit?

	double note_type = meter_dialog.get_note_type ();

	Timecode::BBT_Time when;
	meter_dialog.get_bbt_time(when);

	begin_reversible_command (_("replace tempo mark"));
        XMLNode &before = _session->tempo_map().get_state();
	_session->tempo_map().replace_meter (*section, Meter (bpb, note_type), when);
        XMLNode &after = _session->tempo_map().get_state();
	_session->add_command(new MementoCommand<TempoMap>(_session->tempo_map(), &before, &after));
	commit_reversible_command ();
}

void
Editor::edit_tempo_section (TempoSection* section)
{
	TempoDialog tempo_dialog (*section, _("done"));

	switch (tempo_dialog.run ()) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
	}

	double bpm = tempo_dialog.get_bpm ();
	double nt = tempo_dialog.get_note_type ();
	Timecode::BBT_Time when;
	tempo_dialog.get_bbt_time(when);
	bpm = max (0.01, bpm);

	begin_reversible_command (_("replace tempo mark"));
	XMLNode &before = _session->tempo_map().get_state();
	_session->tempo_map().replace_tempo (*section, Tempo (bpm, nt), when, tempo_dialog.get_tempo_type());
	XMLNode &after = _session->tempo_map().get_state();
	_session->add_command (new MementoCommand<TempoMap>(_session->tempo_map(), &before, &after));
	commit_reversible_command ();
}

void
Editor::edit_tempo_marker (TempoMarker& tm)
{
	edit_tempo_section (&tm.tempo());
}

void
Editor::edit_meter_marker (MeterMarker& mm)
{
	edit_meter_section (&mm.meter());
}

gint
Editor::real_remove_tempo_marker (TempoSection *section)
{
	begin_reversible_command (_("remove tempo mark"));
	XMLNode &before = _session->tempo_map().get_state();
	_session->tempo_map().remove_tempo (*section, true);
	XMLNode &after = _session->tempo_map().get_state();
	_session->add_command(new MementoCommand<TempoMap>(_session->tempo_map(), &before, &after));
	commit_reversible_command ();

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

	if (meter_marker->meter().movable()) {
	  Glib::signal_idle().connect (sigc::bind (sigc::mem_fun(*this, &Editor::real_remove_meter_marker), &meter_marker->meter()));
	}
}

gint
Editor::real_remove_meter_marker (MeterSection *section)
{
	begin_reversible_command (_("remove tempo mark"));
	XMLNode &before = _session->tempo_map().get_state();
	_session->tempo_map().remove_meter (*section, true);
	XMLNode &after = _session->tempo_map().get_state();
	_session->add_command(new MementoCommand<TempoMap>(_session->tempo_map(), &before, &after));
	commit_reversible_command ();

	return FALSE;
}
