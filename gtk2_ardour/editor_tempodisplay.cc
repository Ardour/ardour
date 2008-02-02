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
#include "time_axis_view.h"
#include "ardour_ui.h"

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

	const MeterSection *ms;
	const TempoSection *ts;
	char buf[64];
	
	remove_metric_marks ();
	
	for (Metrics::const_iterator i = metrics.begin(); i != metrics.end(); ++i) {
		
		if ((ms = dynamic_cast<const MeterSection*>(*i)) != 0) {
			snprintf (buf, sizeof(buf), "%g/%g", ms->beats_per_bar(), ms->note_divisor ());
			metric_marks.push_back (new MeterMarker (*this, *meter_group, ARDOUR_UI::config()->canvasvar_MeterMarker.get(), buf, 
								 *(const_cast<MeterSection*>(ms))));
		} else if ((ts = dynamic_cast<const TempoSection*>(*i)) != 0) {
			snprintf (buf, sizeof (buf), "%.2f", ts->beats_per_minute());
			metric_marks.push_back (new TempoMarker (*this, *tempo_group, ARDOUR_UI::config()->canvasvar_TempoMarker.get(), buf, 
								 *(const_cast<TempoSection*>(ts))));
		}
		
	}

}

void
Editor::tempo_map_changed (Change ignored)
{
	if (!session) {
		return;
	}

        ENSURE_GUI_THREAD(bind (mem_fun (*this, &Editor::tempo_map_changed), ignored));

	compute_current_bbt_points(leftmost_frame, leftmost_frame + (nframes_t)(canvas_width * frames_per_unit));
	session->tempo_map().apply_with_metrics (*this, &Editor::draw_metric_marks); // redraw metric markers
	update_tempo_based_rulers ();
	if (tempo_map_change_idle_handler_id  < 0) {
			tempo_map_change_idle_handler_id = Glib::signal_idle().connect (mem_fun (*this, &Editor::redraw_measures));
	}
}

void
Editor::redisplay_tempo (bool immediate_redraw)
{
	if (!session) {
		return;
	}
	
	compute_current_bbt_points (leftmost_frame, leftmost_frame + (nframes_t)(canvas_width * frames_per_unit)); // redraw rulers and measures

	if (immediate_redraw) {

		hide_measures ();

		if (current_bbt_points) {
			draw_measures ();
		}

	} else if (tempo_map_change_idle_handler_id  < 0) {

		tempo_map_change_idle_handler_id = Glib::signal_idle().connect (mem_fun (*this, &Editor::redraw_measures));

	}

	update_tempo_based_rulers ();
}

void
Editor::compute_current_bbt_points (nframes_t leftmost, nframes_t rightmost)
{
	if (!session) {
		return;
	}

	BBT_Time previous_beat, next_beat; // the beats previous to the leftmost frame and after the rightmost frame

	session->bbt_time(leftmost, previous_beat);
	session->bbt_time(rightmost, next_beat);

	if (previous_beat.beats > 1) {
	        previous_beat.beats -= 1;
	} else if (previous_beat.bars > 1) {
 	        previous_beat.bars--;
		previous_beat.beats += 1;
	}
	previous_beat.ticks = 0;

	if (session->tempo_map().meter_at(rightmost).beats_per_bar () > next_beat.beats + 1) {
		next_beat.beats += 1;
	} else {
		next_beat.bars += 1;
		next_beat.beats = 1;
	}
	next_beat.ticks = 0;
	
	if (current_bbt_points) {
		delete current_bbt_points;
		current_bbt_points = 0;
	}

	current_bbt_points = session->tempo_map().get_points (session->tempo_map().frame_time (previous_beat), session->tempo_map().frame_time (next_beat) + 1);
}

void
Editor::hide_measures ()
{
	tempo_lines->hide();
	marker_tempo_lines->hide();
}

bool
Editor::redraw_measures ()
{
	hide_measures ();
	draw_measures ();
	tempo_map_change_idle_handler_id = -1;
	return false;
}

void
Editor::draw_measures ()
{
	if (session == 0 || _show_measures == false
			|| !current_bbt_points || current_bbt_points->empty()) {
		return;
	}

	tempo_lines->draw(*current_bbt_points, frames_per_unit);
	marker_tempo_lines->draw(*current_bbt_points, frames_per_unit);

	/*time_line_group->raise_to_top();
	time_line_group->lower(1);*/
	marker_time_line_group->raise_to_top();
	//marker_time_line_group->lower(1);

	/* the cursors are always on top of everything */
	cursor_group->raise_to_top();

	return;
}

void
Editor::mouse_add_new_tempo_event (nframes_t frame)
{
	if (session == 0) {
		return;
	}

	TempoMap& map(session->tempo_map());
	TempoDialog tempo_dialog (map, frame, _("add"));
	
	tempo_dialog.set_position (Gtk::WIN_POS_MOUSE);
	//this causes compiz to display no border.
	//tempo_dialog.signal_realize().connect (bind (sigc::ptr_fun (set_decoration), &tempo_dialog, Gdk::WMDecoration (Gdk::DECOR_BORDER|Gdk::DECOR_RESIZEH)));

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
	double nt = tempo_dialog.get_note_type();
	bpm = max (0.01, bpm);
	
	tempo_dialog.get_bbt_time (requested);
	
	begin_reversible_command (_("add tempo mark"));
        XMLNode &before = map.get_state();
	map.add_tempo (Tempo (bpm,nt), requested);
        XMLNode &after = map.get_state();
	session->add_command(new MementoCommand<TempoMap>(map, &before, &after));
	commit_reversible_command ();
	
	//map.dump (cerr);
}

void
Editor::mouse_add_new_meter_event (nframes_t frame)
{
	if (session == 0) {
		return;
	}


	TempoMap& map(session->tempo_map());
	MeterDialog meter_dialog (map, frame, _("add"));

	meter_dialog.set_position (Gtk::WIN_POS_MOUSE);

	//this causes compiz to display no border.. 
	//meter_dialog.signal_realize().connect (bind (sigc::ptr_fun (set_decoration), &meter_dialog, Gdk::WMDecoration (Gdk::DECOR_BORDER|Gdk::DECOR_RESIZEH)));

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
	session->add_command(new MementoCommand<TempoMap>(map, &before, &map.get_state()));
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
	session->add_command(new MementoCommand<TempoMap>(session->tempo_map(), &before, &after));
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
	BBT_Time when;
	tempo_dialog.get_bbt_time(when);
	bpm = max (0.01, bpm);
	
	begin_reversible_command (_("replace tempo mark"));
        XMLNode &before = session->tempo_map().get_state();
	session->tempo_map().replace_tempo (*section, Tempo (bpm,nt));
	session->tempo_map().move_tempo (*section, when);
        XMLNode &after = session->tempo_map().get_state();
	session->add_command (new MementoCommand<TempoMap>(session->tempo_map(), &before, &after));
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
	session->add_command(new MementoCommand<TempoMap>(session->tempo_map(), &before, &after));
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
	session->add_command(new MementoCommand<TempoMap>(session->tempo_map(), &before, &after));
	commit_reversible_command ();

	return FALSE;
}
