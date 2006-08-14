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

    $Id$
*/

#include <cstdio> // for sprintf, grrr 
#include <cstdlib>
#include <cmath>
#include <string>
#include <climits>

#include <libgnomecanvasmm.h>

#include <pbd/error.h>
#include <pbd/memento_command.h>

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/gtk_ui.h>

#include <ardour/session.h>
#include <ardour/tempo.h>
#include <gtkmm2ext/doi.h>
#include <gtkmm2ext/utils.h>

#include "editor.h"
#include "marker.h"
#include "simpleline.h"
#include "tempo_dialog.h"
#include "rgb_macros.h"
#include "gui_thread.h"
#include "color.h"

#include "i18n.h"

using namespace std;
using namespace sigc;
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
	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		const MeterSection *ms;
		const TempoSection *ts;
		char buf[64];
		
		if ((ms = dynamic_cast<const MeterSection*>(*i)) != 0) {
			snprintf (buf, sizeof(buf), "%g/%g", ms->beats_per_bar(), ms->note_divisor ());
			metric_marks.push_back (new MeterMarker (*this, *meter_group, color_map[cMeterMarker], buf, 
								 *(const_cast<MeterSection*>(ms))));
		} else if ((ts = dynamic_cast<const TempoSection*>(*i)) != 0) {
			snprintf (buf, sizeof (buf), "%.2f", ts->beats_per_minute());
			metric_marks.push_back (new TempoMarker (*this, *tempo_group, color_map[cTempoMarker], buf, 
								 *(const_cast<TempoSection*>(ts))));
		}
		
	}
}

void
Editor::tempo_map_changed (Change ignored)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::tempo_map_changed), ignored));
	
	if (current_bbt_points) {
		delete current_bbt_points;
		current_bbt_points = 0;
	}

	if (session) {
		current_bbt_points = session->tempo_map().get_points (leftmost_frame, leftmost_frame + current_page_frames());
	} else {
		current_bbt_points = 0;
	}

	redisplay_tempo ();
}

void
Editor::redisplay_tempo ()
{
	update_tempo_based_rulers ();

	remove_metric_marks ();	
	hide_measures ();

	if (session && current_bbt_points) {
		session->tempo_map().apply_with_metrics (*this, &Editor::draw_metric_marks);
		draw_measures ();
	}
	
}

void
Editor::hide_measures ()
{
	for (TimeLineList::iterator i = used_measure_lines.begin(); i != used_measure_lines.end(); ++i) {
		(*i)->hide();
		free_measure_lines.push_back (*i);
	}
	used_measure_lines.clear ();
}

ArdourCanvas::SimpleLine *
Editor::get_time_line ()
{
	ArdourCanvas::SimpleLine *line;

	if (free_measure_lines.empty()) {
		line = new ArdourCanvas::SimpleLine (*time_line_group);
		used_measure_lines.push_back (line);
	} else {
		line = free_measure_lines.front();
		free_measure_lines.erase (free_measure_lines.begin());
		used_measure_lines.push_back (line);
	}

	return line;
}

void
Editor::draw_measures ()
{
	if (session == 0 || _show_measures == false) {
		return;
	}

	TempoMap::BBTPointList::iterator i;
	TempoMap::BBTPointList *all_bbt_points;
	ArdourCanvas::SimpleLine *line;
	gdouble xpos, last_xpos;
	uint32_t cnt;
	uint32_t color;

	if (current_bbt_points == 0 || current_bbt_points->empty()) {
		return;
	}

	all_bbt_points = session->tempo_map().get_points (leftmost_frame, leftmost_frame + current_page_frames());

	cnt = 0;
	last_xpos = 0;

	/* get the first bar spacing */

	gdouble last_beat = DBL_MAX;
	gdouble beat_spacing = 0;

	for (i = all_bbt_points->begin(); i != all_bbt_points->end() && beat_spacing == 0; ++i) {
		TempoMap::BBTPoint& p = (*i);

		switch (p.type) {
		case TempoMap::Bar:
			break;

		case TempoMap::Beat:
			xpos = frame_to_unit (p.frame);
			if (last_beat < xpos) {
				beat_spacing = xpos - last_beat;
			}
			last_beat = xpos;
		}
	}

	double x1, x2, y1, y2;
	track_canvas.get_scroll_region (x1, y1, x2, y2);
	y2 = 1000000000.0f;

	for (i = all_bbt_points->begin(); i != all_bbt_points->end(); ++i) {

		TempoMap::BBTPoint& p = (*i);

		switch (p.type) {
		case TempoMap::Bar:
			break;

		case TempoMap::Beat:
			xpos = frame_to_unit (p.frame);
			
			if (p.beat == 1) {
				color = color_map[cMeasureLineBeat];
			} else {
				color = color_map[cMeasureLineBar];

				/* only draw beat lines if the gaps between beats
				   are large.
				*/

				if (beat_spacing < 25.0) {
					break;
				}
			}
			
			if (cnt == 0 || xpos - last_xpos > 4.0) {
				line = get_time_line ();
				line->property_x1() = xpos;
				line->property_x2() = xpos;
				line->property_y2() = y2;
				line->property_color_rgba() = color;
				line->raise_to_top();
				line->show();
				last_xpos = xpos;	
				++cnt;
			} 
			break;
		}
	}

	delete all_bbt_points;

	/* the cursors are always on top of everything */

	cursor_group->raise_to_top();
	time_line_group->lower_to_bottom();
}

void
Editor::mouse_add_new_tempo_event (jack_nframes_t frame)
{
	if (session == 0) {
		return;
	}

	TempoMap& map(session->tempo_map());
	TempoDialog tempo_dialog (map, frame, _("add"));
	
	tempo_dialog.set_position (Gtk::WIN_POS_MOUSE);
	tempo_dialog.signal_realize().connect (bind (sigc::ptr_fun (set_decoration), &tempo_dialog, Gdk::WMDecoration (Gdk::DECOR_BORDER|Gdk::DECOR_RESIZEH)));

	ensure_float (tempo_dialog);

	switch (tempo_dialog.run()) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
	}

	double bpm = 0;
	BBT_Time requested;
	
	bpm = tempo_dialog.get_bpm ();
	bpm = max (0.01, bpm);
	
	tempo_dialog.get_bbt_time (requested);
	
	begin_reversible_command (_("add tempo mark"));
        XMLNode &before = map.get_state();
	map.add_tempo (Tempo (bpm), requested);
        XMLNode &after = map.get_state();
	session->add_command(new MementoCommand<TempoMap>(map, before, after));
	commit_reversible_command ();
	
	map.dump (cerr);
}

void
Editor::mouse_add_new_meter_event (jack_nframes_t frame)
{
	if (session == 0) {
		return;
	}


	TempoMap& map(session->tempo_map());
	MeterDialog meter_dialog (map, frame, _("add"));

	meter_dialog.set_position (Gtk::WIN_POS_MOUSE);
	meter_dialog.signal_realize().connect (bind (sigc::ptr_fun (set_decoration), &meter_dialog, Gdk::WMDecoration (Gdk::DECOR_BORDER|Gdk::DECOR_RESIZEH)));

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
	BBT_Time requested;
	
	meter_dialog.get_bbt_time (requested);
	
	begin_reversible_command (_("add meter mark"));
        XMLNode &before = map.get_state();
	map.add_meter (Meter (bpb, note_type), requested);
	session->add_command(new MementoCommand<TempoMap>(map, before, map.get_state()));
	commit_reversible_command ();
	
	map.dump (cerr);
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
	  Glib::signal_idle().connect (bind (mem_fun(*this, &Editor::real_remove_tempo_marker), &tempo_marker->tempo()));
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

	begin_reversible_command (_("replace tempo mark"));
        XMLNode &before = session->tempo_map().get_state();
	session->tempo_map().replace_meter (*section, Meter (bpb, note_type));
        XMLNode &after = session->tempo_map().get_state();
	session->add_command(new MementoCommand<TempoMap>(session->tempo_map(), before, after));
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
	BBT_Time when;
	tempo_dialog.get_bbt_time(when);
	bpm = max (0.01, bpm);
	
	begin_reversible_command (_("replace tempo mark"));
        XMLNode &before = session->tempo_map().get_state();
	session->tempo_map().replace_tempo (*section, Tempo (bpm));
	session->tempo_map().move_tempo (*section, when);
        XMLNode &after = session->tempo_map().get_state();
	session->add_command (new MementoCommand<TempoMap>(session->tempo_map(), before, after));
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
	XMLNode &before = session->tempo_map().get_state();
	session->tempo_map().remove_tempo (*section);
	XMLNode &after = session->tempo_map().get_state();
	session->add_command(new MementoCommand<TempoMap>(session->tempo_map(), before, after));
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
	  Glib::signal_idle().connect (bind (mem_fun(*this, &Editor::real_remove_meter_marker), &meter_marker->meter()));
	}
}

gint
Editor::real_remove_meter_marker (MeterSection *section)
{
	begin_reversible_command (_("remove tempo mark"));
	XMLNode &before = session->tempo_map().get_state();
	session->tempo_map().remove_meter (*section);
	XMLNode &after = session->tempo_map().get_state();
	session->add_command(new MementoCommand<TempoMap>(session->tempo_map(), before, after));
	commit_reversible_command ();
	return FALSE;
}
