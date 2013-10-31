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

*/

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <glibmm/miscutils.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/window_title.h>

#include "pbd/enumwriter.h"

#include "ardour/rc_configuration.h"

#include "actions.h"
#include "ardour_ui.h"
#include "audio_time_axis.h"
#include "automation_time_axis.h"
#include "editor.h"
#include "editor_route_groups.h"
#include "editor_regions.h"
#include "gui_thread.h"
#include "midi_time_axis.h"
#include "mixer_strip.h"
#include "mixer_ui.h"
#include "selection.h"

#include "i18n.h"

using namespace std;
using namespace Gtkmm2ext;
using namespace PBD;

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
Editor::editor_list_button_toggled ()
{
	Glib::RefPtr<Gtk::Action> act = ActionManager::get_action (X_("Editor"), X_("show-editor-list"));
	if (act) {
		Glib::RefPtr<Gtk::ToggleAction> tact = Glib::RefPtr<Gtk::ToggleAction>::cast_dynamic(act);
		show_editor_list (tact->get_active());
	}
}

void
Editor::show_editor_mixer (bool yn)
{
	boost::shared_ptr<ARDOUR::Route> r;

	show_editor_mixer_when_tracks_arrive = false;

	if (yn) {
		Glib::RefPtr<Gdk::Window> win = get_window ();
		Glib::RefPtr<Gdk::Screen> screen;
		
		if (win) {
			 screen = win->get_screen();
		} else {
			screen = Gdk::Screen::get_default();
		}

		if (g_getenv ("ARDOUR_LOVES_STUPID_TINY_SCREENS") == 0 && screen && screen->get_height() < 700) {
			Gtk::MessageDialog msg (_("This screen is not tall enough to display the editor mixer"));
			msg.run ();
			return;
		}
	}

	if (!_session) {
		show_editor_mixer_when_tracks_arrive = yn;
		return;
	}

	if (yn) {

		if (selection->tracks.empty()) {

			if (track_views.empty()) {
				show_editor_mixer_when_tracks_arrive = true;
				return;
			}

			for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
				RouteTimeAxisView* atv;

				if ((atv = dynamic_cast<RouteTimeAxisView*> (*i)) != 0) {
					r = atv->route();
					break;
				}
			}

		} else {
			sort_track_selection (selection->tracks);

			for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
				RouteTimeAxisView* atv;

				if ((atv = dynamic_cast<RouteTimeAxisView*> (*i)) != 0) {
					r = atv->route();
					break;
				}
			}
		}

		if (r) {
			if (current_mixer_strip == 0) {
				create_editor_mixer ();
			}

			current_mixer_strip->set_route (r);
			current_mixer_strip->set_width_enum (editor_mixer_strip_width, (void*) this);
		}

		if (current_mixer_strip->get_parent() == 0) {
			global_hpacker.pack_start (*current_mixer_strip, Gtk::PACK_SHRINK );
 			global_hpacker.reorder_child (*current_mixer_strip, 0);
			current_mixer_strip->show ();
		}

	} else {

		if (current_mixer_strip) {
			if (current_mixer_strip->get_parent() != 0) {
				global_hpacker.remove (*current_mixer_strip);
			}
		}
	}

#ifdef GTKOSX
	/* XXX gtk problem here */
	ensure_all_elements_drawn();
#endif
}

#ifdef GTKOSX
void
Editor::ensure_all_elements_drawn ()
{
	controls_layout.queue_draw ();
	ruler_label_event_box.queue_draw ();
	time_bars_event_box.queue_draw ();
}
#endif

void
Editor::create_editor_mixer ()
{
	current_mixer_strip = new MixerStrip (*ARDOUR_UI::instance()->the_mixer(),
					      _session,
					      false);
	current_mixer_strip->Hiding.connect (sigc::mem_fun(*this, &Editor::current_mixer_strip_hidden));
	current_mixer_strip->WidthChanged.connect (sigc::mem_fun (*this, &Editor::mixer_strip_width_changed));

#ifdef GTKOSX
	current_mixer_strip->WidthChanged.connect (sigc::mem_fun(*this, &Editor::ensure_all_elements_drawn));
#endif
	current_mixer_strip->set_embedded (true);
}

void
Editor::set_selected_mixer_strip (TimeAxisView& view)
{
	if (!_session) {
		return;
	}

	Glib::RefPtr<Gtk::Action> act = ActionManager::get_action (X_("Editor"), X_("show-editor-mixer"));

	if (act) {
		Glib::RefPtr<Gtk::ToggleAction> tact = Glib::RefPtr<Gtk::ToggleAction>::cast_dynamic(act);
		if (!tact || !tact->get_active()) {
			/* not showing mixer strip presently */
			return;
		}
	}

	if (current_mixer_strip == 0) {
		create_editor_mixer ();
	}


	// if this is an automation track, then we shold the mixer strip should
	// show the parent

	boost::shared_ptr<ARDOUR::Route> route;
	AutomationTimeAxisView* atv;

	if ((atv = dynamic_cast<AutomationTimeAxisView*>(&view)) != 0) {

		AudioTimeAxisView *parent = dynamic_cast<AudioTimeAxisView*>(view.get_parent());

		if (parent) {
			route = parent->route ();
		}

	} else {

		AudioTimeAxisView* at = dynamic_cast<AudioTimeAxisView*> (&view);

		if (at) {
			route = at->route();
		} else {
			MidiTimeAxisView* mt = dynamic_cast<MidiTimeAxisView*> (&view);
			if (mt) {
				route = mt->route();
			}
		}
	}

	if (current_mixer_strip->route() == route) {
		return;
	}

	if (route) {
		current_mixer_strip->set_route (route);
		current_mixer_strip->set_width_enum (editor_mixer_strip_width, (void*) this);
	}
}

void
Editor::current_mixer_strip_hidden ()
{
	Glib::RefPtr<Gtk::Action> act = ActionManager::get_action (X_("Editor"), X_("show-editor-mixer"));
	if (act) {
		Glib::RefPtr<Gtk::ToggleAction> tact = Glib::RefPtr<Gtk::ToggleAction>::cast_dynamic(act);
		tact->set_active (false);
	}
}

void
Editor::maybe_add_mixer_strip_width (XMLNode& node)
{
	if (current_mixer_strip) {
		node.add_property ("mixer-width", enum_2_string (editor_mixer_strip_width));
	}
}

void
Editor::mixer_strip_width_changed ()
{
#ifdef GTKOSX
	ensure_all_elements_drawn ();
#endif

	editor_mixer_strip_width = current_mixer_strip->get_width_enum ();
}

void
Editor::track_mixer_selection ()
{
	Mixer_UI::instance()->selection().RoutesChanged.connect (sigc::mem_fun (*this, &Editor::follow_mixer_selection));
}

void
Editor::follow_mixer_selection ()
{
	if (!ARDOUR::Config->get_link_editor_and_mixer_selection() || _following_mixer_selection) {
		return;
	}

	_following_mixer_selection = true;
	selection->block_tracks_changed (true);

	RouteUISelection& s (Mixer_UI::instance()->selection().routes);

	selection->clear_tracks ();

	for (RouteUISelection::iterator i = s.begin(); i != s.end(); ++i) {
		TimeAxisView* tav = get_route_view_by_route_id ((*i)->route()->id());
		if (tav) {
			selection->add (tav);
		}
	}

	_following_mixer_selection = false;
	selection->block_tracks_changed (false);
	selection->TracksChanged (); /* EMIT SIGNAL */
}
