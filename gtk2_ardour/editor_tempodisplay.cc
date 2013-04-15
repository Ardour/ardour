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

#include "editor.h"
#include "marker.h"
#include "tempo_dialog.h"
#include "rgb_macros.h"
#include "gui_thread.h"
#include "time_axis_view.h"
#include "ardour_ui.h"
#include "tempo_lines.h"
#include "utils.h"

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
			metric_marks.push_back (new MeterMarker (*this, *meter_group, ARDOUR_UI::config()->canvasvar_MeterMarker.get(), buf,
								 *(const_cast<MeterSection*>(ms))));
		} else if ((ts = dynamic_cast<const TempoSection*>(*i)) != 0) {
			if (Config->get_allow_non_quarter_pulse()) {
				snprintf (buf, sizeof (buf), "%.2f/%.0f", ts->beats_per_minute(), ts->note_type());
			} else {
				snprintf (buf, sizeof (buf), "%.2f", ts->beats_per_minute());
			}
			metric_marks.push_back (new TempoMarker (*this, *tempo_group, ARDOUR_UI::config()->canvasvar_TempoMarker.get(), buf,
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

	ARDOUR::TempoMap::BBTPointList::const_iterator begin;
	ARDOUR::TempoMap::BBTPointList::const_iterator end;

	compute_current_bbt_points (leftmost_frame, leftmost_frame + current_page_samples(), begin, end);
	_session->tempo_map().apply_with_metrics (*this, &Editor::draw_metric_marks); // redraw metric markers
	redraw_measures ();
	update_tempo_based_rulers (begin, end);
}

void
Editor::redisplay_tempo (bool immediate_redraw)
{
	if (!_session) {
		return;
	}

	ARDOUR::TempoMap::BBTPointList::const_iterator current_bbt_points_begin;
	ARDOUR::TempoMap::BBTPointList::const_iterator current_bbt_points_end;

	compute_current_bbt_points (leftmost_frame, leftmost_frame + current_page_samples(),
				    current_bbt_points_begin, current_bbt_points_end);

	if (immediate_redraw) {
		redraw_measures ();
	} else {
#ifdef GTKOSX
		redraw_measures ();
#else
		Glib::signal_idle().connect (sigc::mem_fun (*this, &Editor::redraw_measures));
#endif
	}
	update_tempo_based_rulers (current_bbt_points_begin, current_bbt_points_end); // redraw rulers and measures
}

void
Editor::compute_current_bbt_points (framepos_t leftmost, framepos_t rightmost,
				    ARDOUR::TempoMap::BBTPointList::const_iterator& begin,
				    ARDOUR::TempoMap::BBTPointList::const_iterator& end)
{
	if (!_session) {
		return;
	}

	/* prevent negative values of leftmost from creeping into tempomap
	 */

	_session->tempo_map().get_grid (begin, end, max (leftmost, (framepos_t) 0), rightmost);
}

void
Editor::hide_measures ()
{
	if (tempo_lines)
		tempo_lines->hide();
}

bool
Editor::redraw_measures ()
{
	ARDOUR::TempoMap::BBTPointList::const_iterator begin;
	ARDOUR::TempoMap::BBTPointList::const_iterator end;

	compute_current_bbt_points (leftmost_frame, leftmost_frame + current_page_samples(), begin, end);
        draw_measures (begin, end);

	return false;
}

void
Editor::draw_measures (ARDOUR::TempoMap::BBTPointList::const_iterator& begin,
		       ARDOUR::TempoMap::BBTPointList::const_iterator& end)
{
	if (_session == 0 || _show_measures == false || distance (begin, end) == 0) {
		return;
	}

	if (tempo_lines == 0) {
		tempo_lines = new TempoLines (*_track_canvas_viewport, time_line_group, physical_screen_height(get_window()));
	}
	
	tempo_lines->draw (begin, end, samples_per_pixel);
}

void
Editor::mouse_add_new_tempo_event (framepos_t frame)
{
	if (_session == 0) {
		return;
	}

	TempoMap& map(_session->tempo_map());
	TempoDialog tempo_dialog (map, frame, _("add"));

	tempo_dialog.set_position (Gtk::WIN_POS_MOUSE);
	//this causes compiz to display no border.
	//tempo_dialog.signal_realize().connect (sigc::bind (sigc::ptr_fun (set_decoration), &tempo_dialog, Gdk::WMDecoration (Gdk::DECOR_BORDER|Gdk::DECOR_RESIZEH)));

	ensure_float (tempo_dialog);

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
	map.add_tempo (Tempo (bpm,nt), requested);
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

	meter_dialog.set_position (Gtk::WIN_POS_MOUSE);

	//this causes compiz to display no border..
	//meter_dialog.signal_realize().connect (sigc::bind (sigc::ptr_fun (set_decoration), &meter_dialog, Gdk::WMDecoration (Gdk::DECOR_BORDER|Gdk::DECOR_RESIZEH)));

	ensure_float (meter_dialog);

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
	Marker* marker;
	TempoMarker* tempo_marker;

	if ((marker = reinterpret_cast<Marker *> (item->get_data ("marker"))) == 0) {
		fatal << _("programming error: tempo marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	if ((tempo_marker = dynamic_cast<TempoMarker*> (marker)) == 0) {
		fatal << _("programming error: marker for tempo is not a tempo marker!") << endmsg;
		/*NOTREACHED*/
	}

	if (tempo_marker->tempo().movable()) {
		Glib::signal_idle().connect (sigc::bind (sigc::mem_fun(*this, &Editor::real_remove_tempo_marker), &tempo_marker->tempo()));
	}
}

void
Editor::edit_meter_section (MeterSection* section)
{
	MeterDialog meter_dialog (*section, _("done"));

	meter_dialog.set_position (Gtk::WIN_POS_MOUSE);

	ensure_float (meter_dialog);

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

	tempo_dialog.set_position (Gtk::WIN_POS_MOUSE);

	ensure_float (tempo_dialog);

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
	_session->tempo_map().replace_tempo (*section, Tempo (bpm, nt), when);
	XMLNode &after = _session->tempo_map().get_state();
	_session->add_command (new MementoCommand<TempoMap>(_session->tempo_map(), &before, &after));
	commit_reversible_command ();
}

void
Editor::edit_tempo_marker (ArdourCanvas::Item *item)
{
	Marker* marker;
	TempoMarker* tempo_marker;

	if ((marker = reinterpret_cast<Marker *> (item->get_data ("marker"))) == 0) {
		fatal << _("programming error: tempo marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	if ((tempo_marker = dynamic_cast<TempoMarker*> (marker)) == 0) {
		fatal << _("programming error: marker for tempo is not a tempo marker!") << endmsg;
		/*NOTREACHED*/
	}

	edit_tempo_section (&tempo_marker->tempo());
}

void
Editor::edit_meter_marker (ArdourCanvas::Item *item)
{
	Marker* marker;
	MeterMarker* meter_marker;

	if ((marker = reinterpret_cast<Marker *> (item->get_data ("marker"))) == 0) {
		fatal << _("programming error: tempo marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	if ((meter_marker = dynamic_cast<MeterMarker*> (marker)) == 0) {
		fatal << _("programming error: marker for meter is not a meter marker!") << endmsg;
		/*NOTREACHED*/
	}

	edit_meter_section (&meter_marker->meter());
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
	Marker* marker;
	MeterMarker* meter_marker;

	if ((marker = reinterpret_cast<Marker *> (item->get_data ("marker"))) == 0) {
		fatal << _("programming error: meter marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	if ((meter_marker = dynamic_cast<MeterMarker*> (marker)) == 0) {
		fatal << _("programming error: marker for meter is not a meter marker!") << endmsg;
		/*NOTREACHED*/
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
