/*
    Copyright (C) 2003-2004 Paul Davis 

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

#include <gtkmm2ext/utils.h>
#include <ardour/audioengine.h>

#include "editor.h"
#include "mixer_strip.h"
#include "ardour_ui.h"
#include "selection.h"
#include "audio_time_axis.h"
#include "actions.h"

#include "i18n.h"

void
Editor::editor_mixer_button_toggled ()
{
	Glib::RefPtr<Gtk::Action> act = ActionManager::get_action (X_("Editor"), X_("show-editor-mixer"));
	if (act) {
		Glib::RefPtr<Gtk::ToggleAction> tact = Glib::RefPtr<Gtk::ToggleAction>::cast_dynamic(act);
		show_editor_mixer (tact->get_active());
	}
}

void
Editor::cms_deleted ()
{
	current_mixer_strip = 0;
}

void
Editor::show_editor_mixer (bool yn)
{
	show_editor_mixer_when_tracks_arrive = false;

	if (yn) {

		if (current_mixer_strip == 0) {

			if (selection->tracks.empty()) {
				
				if (track_views.empty()) {	
					show_editor_mixer_when_tracks_arrive = true;
					return;
				} 
				
				for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
					AudioTimeAxisView* atv;

					if ((atv = dynamic_cast<AudioTimeAxisView*> (*i)) != 0) {
						
						current_mixer_strip = new MixerStrip (*ARDOUR_UI::instance()->the_mixer(),
										      *session,
										      atv->route(), false);

						current_mixer_strip->GoingAway.connect (mem_fun(*this, &Editor::cms_deleted));						
						break;
					}
				}

			} else {
				for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
					AudioTimeAxisView* atv;

					if ((atv = dynamic_cast<AudioTimeAxisView*> (*i)) != 0) {

						current_mixer_strip = new MixerStrip (*ARDOUR_UI::instance()->the_mixer(),
										      *session,
										      atv->route(), false);
						current_mixer_strip->GoingAway.connect (mem_fun(*this, &Editor::cms_deleted));						
						break;
					}
				}

			}

			if (current_mixer_strip == 0) {
				return;
			}		
		}
		
		if (current_mixer_strip->get_parent() == 0) {
			current_mixer_strip->set_embedded (true);
			current_mixer_strip->Hiding.connect (mem_fun(*this, &Editor::current_mixer_strip_hidden));
			current_mixer_strip->GoingAway.connect (mem_fun(*this, &Editor::current_mixer_strip_removed));
			current_mixer_strip->set_width (editor_mixer_strip_width);
			
			global_hpacker.pack_start (*current_mixer_strip, Gtk::PACK_SHRINK );
 			global_hpacker.reorder_child (*current_mixer_strip, 0);

			current_mixer_strip->show_all ();
		}

	} else {

		if (current_mixer_strip) {
		        editor_mixer_strip_width = current_mixer_strip->get_width ();
			if (current_mixer_strip->get_parent() != 0) {
				global_hpacker.remove (*current_mixer_strip);
			}
		}
	}
}

void
Editor::set_selected_mixer_strip (TimeAxisView& view)
{
	AudioTimeAxisView* at;
	bool show = false;

	if (!session || (at = dynamic_cast<AudioTimeAxisView*>(&view)) == 0) {
		return;
	}
	
	if (current_mixer_strip) {

		/* might be nothing to do */

		if (current_mixer_strip->route() == at->route()) {
			return;
		}

		if (current_mixer_strip->get_parent()) {
			show = true;
		}
		delete current_mixer_strip;
		current_mixer_strip = 0;
	}

	current_mixer_strip = new MixerStrip (*ARDOUR_UI::instance()->the_mixer(),
					      *session,
					      at->route());
	current_mixer_strip->GoingAway.connect (mem_fun(*this, &Editor::cms_deleted));
	
	if (show) {
		show_editor_mixer (true);
	}
}

void
Editor::update_current_screen ()
{
	if (session && engine.running()) {

		nframes_t frame;

		frame = session->audible_frame();

		/* only update if the playhead is on screen or we are following it */

		if (_follow_playhead) {


			playhead_cursor->canvas_item.show();
			if (frame != last_update_frame) {
				const jack_nframes_t page_width = current_page_frames();

				// Percentage width of the visible range to use as a scroll interval
				// Idea: snap this to the nearest bar/beat/tick/etc, would make scrolling much
				// less jarring when zoomed in.. and it would be fun to watch :)
				static const double scroll_pct = 3.0/4.0;

				const jack_nframes_t rightmost_frame = leftmost_frame + page_width;
				const jack_nframes_t scroll_interval = (jack_nframes_t)(page_width * scroll_pct);
				const jack_nframes_t padding = (jack_nframes_t)floor((page_width-scroll_interval) / 2.0);

				if (frame < leftmost_frame + padding || frame > rightmost_frame - padding) {

					if (session->transport_speed() < 0) {
						if (frame > scroll_interval) {
							center_screen (frame - scroll_interval/2);
						} else {
							center_screen (scroll_interval);
						}
					} else {
						center_screen(frame + scroll_interval/2);
					}
				}

				playhead_cursor->set_position (frame);
			}

		} else {

			if (frame != last_update_frame) {
				if (frame < leftmost_frame || frame > leftmost_frame + current_page_frames()) {
					playhead_cursor->canvas_item.hide();
				} else {
					playhead_cursor->set_position (frame);
				}
			}
		}

		last_update_frame = frame;

		if (current_mixer_strip) {
			current_mixer_strip->fast_update ();
		}

	}
}

void
Editor::current_mixer_strip_removed ()
{
	if (current_mixer_strip) {
		/* it is being deleted */
		current_mixer_strip = 0;
	}
}

void
Editor::current_mixer_strip_hidden ()
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		
		AudioTimeAxisView* tmp;
		
		if ((tmp = dynamic_cast<AudioTimeAxisView*>(*i)) != 0) {
			if (tmp->route() == current_mixer_strip->route()) {
				(*i)->set_selected (false);
				break;
			}
		}
	}

	Glib::RefPtr<Gtk::Action> act = ActionManager::get_action (X_("Editor"), X_("show-editor-mixer"));
	if (act) {
		Glib::RefPtr<Gtk::ToggleAction> tact = Glib::RefPtr<Gtk::ToggleAction>::cast_dynamic(act);
		tact->set_active (false);
	}
}

void
Editor::session_going_away ()
{
	for (vector<sigc::connection>::iterator i = session_connections.begin(); i != session_connections.end(); ++i) {
		(*i).disconnect ();
	}

	stop_scrolling ();
	selection->clear ();
	cut_buffer->clear ();

	clicked_regionview = 0;
	clicked_axisview = 0;
	clicked_routeview = 0;
	clicked_crossfadeview = 0;
	entered_regionview = 0;
	entered_track = 0;
	latest_regionview = 0;
	last_update_frame = 0;
	drag_info.item = 0;
	last_canvas_frame = 0;

	/* hide all tracks */

	hide_all_tracks (false);

	/* rip everything out of the list displays */

	region_list_clear (); // no clear() method in gtkmm 1.2 
	route_display_model->clear ();
	named_selection_model->clear ();
	group_model->clear ();

	edit_cursor_clock.set_session (0);
	zoom_range_clock.set_session (0);
	nudge_clock.set_session (0);

	/* put editor/mixer toggle button in off position and disable until a new session is loaded */

	editor_mixer_button.set_active(false);
	editor_mixer_button.set_sensitive(false);
	/* clear tempo/meter rulers */

	remove_metric_marks ();
	hide_measures ();
	clear_marker_display ();

	if (current_bbt_points) {
		delete current_bbt_points;
		current_bbt_points = 0;
	}

	/* mixer strip will be deleted all by itself 
	   when its route is deleted.
	*/

	current_mixer_strip = 0;

	set_title (_("ardour: editor"));

	session = 0;
}
