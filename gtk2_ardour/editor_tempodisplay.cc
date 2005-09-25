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

#include <gtk-canvas.h>

#include <pbd/error.h>

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/gtk_ui.h>

#include <ardour/session.h>
#include <ardour/tempo.h>
#include <gtkmm2ext/doi.h>

#include "editor.h"
#include "marker.h"
#include "canvas-simpleline.h"
#include "tempo_dialog.h"
#include "rgb_macros.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace std;
using namespace sigc;
using namespace ARDOUR;
using namespace Gtk;
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
			metric_marks.push_back (new MeterMarker (*this, GTK_CANVAS_GROUP(meter_group), color_map[cMeterMarker], buf, 
								 *(const_cast<MeterSection*>(ms)), PublicEditor::canvas_meter_marker_event));
		} else if ((ts = dynamic_cast<const TempoSection*>(*i)) != 0) {
			snprintf (buf, sizeof (buf), "%.2f", ts->beats_per_minute());
			metric_marks.push_back (new TempoMarker (*this, GTK_CANVAS_GROUP(tempo_group), color_map[cTempoMarker], buf, 
								 *(const_cast<TempoSection*>(ts)), PublicEditor::canvas_tempo_marker_event));
		}
		
	}
}

void
Editor::tempo_map_changed (Change ignored)
{
	ENSURE_GUI_THREAD(bind (slot (*this, &Editor::tempo_map_changed), ignored));
	
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
		gtk_canvas_item_hide (*i);
		free_measure_lines.push_back (*i);
	}
	used_measure_lines.clear ();
}

GtkCanvasItem *
Editor::get_time_line ()
{
	GtkCanvasItem *line;

	if (free_measure_lines.empty()) {
		line = gtk_canvas_item_new (GTK_CANVAS_GROUP(time_line_group),
					    gtk_canvas_simpleline_get_type(),
					    NULL);
		// cerr << "measure line @ " << line << endl;
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
	GtkCanvasItem *line;
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
			xpos = p.frame / (gdouble) frames_per_unit;
			if (last_beat < xpos) {
				beat_spacing = xpos - last_beat;
			}
			last_beat = xpos;
		}
	}

	for (i = all_bbt_points->begin(); i != all_bbt_points->end(); ++i) {

		TempoMap::BBTPoint& p = (*i);

		switch (p.type) {
		case TempoMap::Bar:
			break;

		case TempoMap::Beat:
			xpos = p.frame / (gdouble) frames_per_unit;

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
				gtk_object_set (GTK_OBJECT(line),
						"x1", xpos,
						"x2", xpos,
						"y2", (gdouble) canvas_height,
						"color_rgba", color,
						NULL);
				gtk_canvas_item_raise_to_top (line);
				gtk_canvas_item_show (line);
				last_xpos = xpos;	
				++cnt;
			} 
			break;
		}
	}

	delete all_bbt_points;

	/* the cursors are always on top of everything */

	gtk_canvas_item_raise_to_top (cursor_group);
	gtk_canvas_item_lower_to_bottom (time_line_group);
}

void
Editor::mouse_add_new_tempo_event (jack_nframes_t frame)
{
	if (session == 0) {
		return;
	}


	TempoMap& map(session->tempo_map());
	TempoDialog tempo_dialog (map, frame, _("add"));
	
	tempo_dialog.bpm_entry.activate.connect (bind (slot (tempo_dialog, &ArdourDialog::stop), 0));
	tempo_dialog.ok_button.signal_clicked().connect (bind (slot (tempo_dialog, &ArdourDialog::stop), 0));
	tempo_dialog.cancel_button.signal_clicked().connect (bind (slot (tempo_dialog, &ArdourDialog::stop), -1));

	tempo_dialog.set_position (Gtk::WIN_POS_MOUSE);
	tempo_dialog.realize ();
	tempo_dialog.get_window().set_decorations (GdkWMDecoration (GDK_DECOR_BORDER|GDK_DECOR_RESIZEH));

	ensure_float (tempo_dialog);

	tempo_dialog.run();

	if (tempo_dialog.run_status() == 0) {
		
		double bpm = 0;
		BBT_Time requested;

		bpm = tempo_dialog.get_bpm ();
		bpm = max (0.01, bpm);

		tempo_dialog.get_bbt_time (requested);

		begin_reversible_command (_("add tempo mark"));
		session->add_undo (map.get_memento());
		map.add_tempo (Tempo (bpm), requested);
		session->add_redo_no_execute (map.get_memento());
		commit_reversible_command ();

		map.dump (cerr);
	}
}

void
Editor::mouse_add_new_meter_event (jack_nframes_t frame)
{
	if (session == 0) {
		return;
	}


	TempoMap& map(session->tempo_map());
	MeterDialog meter_dialog (map, frame, _("add"));

	meter_dialog.ok_button.signal_clicked().connect (bind (slot (meter_dialog, &ArdourDialog::stop), 0));
	meter_dialog.cancel_button.signal_clicked().connect (bind (slot (meter_dialog, &ArdourDialog::stop), -1));

	meter_dialog.set_position (Gtk::WIN_POS_MOUSE);
	meter_dialog.realize ();
	meter_dialog.get_window().set_decorations (GdkWMDecoration (GDK_DECOR_BORDER|GDK_DECOR_RESIZEH));

	ensure_float (meter_dialog);
	
	meter_dialog.run ();
	
	if (meter_dialog.run_status() == 0) {
		
		double bpb = meter_dialog.get_bpb ();
		bpb = max (1.0, bpb); // XXX is this a reasonable limit?

		double note_type = meter_dialog.get_note_type ();
		BBT_Time requested;

		meter_dialog.get_bbt_time (requested);

		begin_reversible_command (_("add meter mark"));
		session->add_undo (map.get_memento());
		map.add_meter (Meter (bpb, note_type), requested);
		session->add_redo_no_execute (map.get_memento());
		commit_reversible_command ();

		map.dump (cerr);
	}
}

void
Editor::remove_tempo_marker (GtkCanvasItem* item)
{
	Marker* marker;
	TempoMarker* tempo_marker;

	if ((marker = reinterpret_cast<Marker *> (gtk_object_get_data (GTK_OBJECT(item), "marker"))) == 0) {
		fatal << _("programming error: tempo marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	if ((tempo_marker = dynamic_cast<TempoMarker*> (marker)) == 0) {
		fatal << _("programming error: marker for tempo is not a tempo marker!") << endmsg;
		/*NOTREACHED*/
	}		

	if (tempo_marker->tempo().movable()) {
		Gtk::Main::idle.connect (bind (slot (*this, &Editor::real_remove_tempo_marker), &tempo_marker->tempo()));
	}
}

void
Editor::edit_meter_section (MeterSection* section)
{
	MeterDialog meter_dialog (*section, _("done"));

	meter_dialog.ok_button.signal_clicked().connect (bind (slot (meter_dialog, &ArdourDialog::stop), 0));
	meter_dialog.cancel_button.signal_clicked().connect (bind (slot (meter_dialog, &ArdourDialog::stop), -1));

	meter_dialog.set_position (Gtk::WIN_POS_MOUSE);
	meter_dialog.realize ();
	meter_dialog.get_window().set_decorations (GdkWMDecoration (GDK_DECOR_BORDER|GDK_DECOR_RESIZEH));

	ensure_float (meter_dialog);

	meter_dialog.run ();

	if (meter_dialog.run_status() == 0) {

		double bpb = meter_dialog.get_bpb ();
		bpb = max (1.0, bpb); // XXX is this a reasonable limit?

		double note_type = meter_dialog.get_note_type ();

		begin_reversible_command (_("replace tempo mark"));
		session->add_undo (session->tempo_map().get_memento());
		session->tempo_map().replace_meter (*section, Meter (bpb, note_type));
		session->add_redo_no_execute (session->tempo_map().get_memento());
		commit_reversible_command ();
	}
}

void
Editor::edit_tempo_section (TempoSection* section)
{
	TempoDialog tempo_dialog (*section, _("done"));

	tempo_dialog.bpm_entry.activate.connect (bind (slot (tempo_dialog, &ArdourDialog::stop), 0));
	tempo_dialog.ok_button.signal_clicked().connect (bind (slot (tempo_dialog, &ArdourDialog::stop), 0));
	tempo_dialog.cancel_button.signal_clicked().connect (bind (slot (tempo_dialog, &ArdourDialog::stop), -1));

	tempo_dialog.set_position (Gtk::WIN_POS_MOUSE);
	tempo_dialog.realize ();
	tempo_dialog.get_window().set_decorations (GdkWMDecoration (GDK_DECOR_BORDER|GDK_DECOR_RESIZEH));

	ensure_float (tempo_dialog);
	
	tempo_dialog.run ();

	if (tempo_dialog.run_status() == 0) {

		double bpm = tempo_dialog.get_bpm ();
	        BBT_Time when;
		tempo_dialog.get_bbt_time(when);
		bpm = max (0.01, bpm);

		begin_reversible_command (_("replace tempo mark"));
		session->add_undo (session->tempo_map().get_memento());
		session->tempo_map().replace_tempo (*section, Tempo (bpm));
		session->tempo_map().move_tempo (*section, when);
		session->add_redo_no_execute (session->tempo_map().get_memento());
		commit_reversible_command ();
	}
}

void
Editor::edit_tempo_marker (GtkCanvasItem *item)
{
	Marker* marker;
	TempoMarker* tempo_marker;

	if ((marker = reinterpret_cast<Marker *> (gtk_object_get_data (GTK_OBJECT(item), "marker"))) == 0) {
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
Editor::edit_meter_marker (GtkCanvasItem *item)
{
	Marker* marker;
	MeterMarker* meter_marker;

	if ((marker = reinterpret_cast<Marker *> (gtk_object_get_data (GTK_OBJECT(item), "marker"))) == 0) {
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
	session->add_undo (session->tempo_map().get_memento());
	session->tempo_map().remove_tempo (*section);
	session->add_redo_no_execute (session->tempo_map().get_memento());
	commit_reversible_command ();

	return FALSE;
}

void
Editor::remove_meter_marker (GtkCanvasItem* item)
{
	Marker* marker;
	MeterMarker* meter_marker;

	if ((marker = reinterpret_cast<Marker *> (gtk_object_get_data (GTK_OBJECT(item), "marker"))) == 0) {
		fatal << _("programming error: meter marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	if ((meter_marker = dynamic_cast<MeterMarker*> (marker)) == 0) {
		fatal << _("programming error: marker for meter is not a meter marker!") << endmsg;
		/*NOTREACHED*/
	}		

	if (meter_marker->meter().movable()) {
		Gtk::Main::idle.connect (bind (slot (*this, &Editor::real_remove_meter_marker), &meter_marker->meter()));
	}
}

gint
Editor::real_remove_meter_marker (MeterSection *section)
{
	begin_reversible_command (_("remove tempo mark"));
	session->add_undo (session->tempo_map().get_memento());
	session->tempo_map().remove_meter (*section);
	session->add_redo_no_execute (session->tempo_map().get_memento());
	commit_reversible_command ();
	return FALSE;
}
