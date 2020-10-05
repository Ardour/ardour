/*
 * Copyright (C) 2005-2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2005-2017 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2005-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2010 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2015 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2013-2016 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2013-2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015 André Nusser <andre.nusser@googlemail.com>
 * Copyright (C) 2016-2018 Len Ovens <len@ovenwerks.net>
 * Copyright (C) 2017 Johannes Mueller <github@johannes-mueller.org>
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
#include "gtk2ardour-version.h"
#endif

#include "ardour/monitor_processor.h"
#include "ardour/session.h"
#include "ardour/route.h"

#include "actions.h"
#include "ardour_ui.h"
#include "audio_clock.h"
#include "gui_thread.h"
#include "main_clock.h"
#include "public_editor.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace ArdourWidgets;
using namespace Gtk;
using namespace std;
using namespace Editing;

void
ARDOUR_UI::cancel_solo ()
{
	if (_session) {
		_session->cancel_all_solo ();
	}
}

void
ARDOUR_UI::reset_focus (Gtk::Widget* w)
{
	/* this resets focus to the first focusable parent of the given widget,
	 * or, if there is no focusable parent, cancels focus in the toplevel
	 * window that the given widget is packed into (if there is one).
	 */

	if (!w) {
		return;
	}

	Gtk::Widget* top = w->get_toplevel();

	if (!top || !top->is_toplevel()) {
		return;
	}

	w = w->get_parent ();

	while (w) {

		if (w->is_toplevel()) {
			/* Setting the focus widget to a Gtk::Window causes all
			 * subsequent calls to ::has_focus() on the nominal
			 * focus widget in that window to return
			 * false. Workaround: never set focus to the toplevel
			 * itself.
			 */
			break;
		}

		if (w->get_can_focus ()) {
			Gtk::Window* win = dynamic_cast<Gtk::Window*> (top);
			win->set_focus (*w);
			return;
		}
		w = w->get_parent ();
	}

	if (top == &_main_window) {

	}

	/* no focusable parent found, cancel focus in top level window.
	   C++ API cannot be used for this. Thanks, references.
	*/

	gtk_window_set_focus (GTK_WINDOW(top->gobj()), 0);

}

void
ARDOUR_UI::monitor_dim_all ()
{
	boost::shared_ptr<Route> mon = _session->monitor_out ();
	if (!mon) {
		return;
	}
	boost::shared_ptr<ARDOUR::MonitorProcessor> _monitor = mon->monitor_control ();

	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Monitor"), "monitor-dim-all");
	_monitor->set_dim_all (tact->get_active());
}

void
ARDOUR_UI::monitor_cut_all ()
{
	boost::shared_ptr<Route> mon = _session->monitor_out ();
	if (!mon) {
		return;
	}
	boost::shared_ptr<ARDOUR::MonitorProcessor> _monitor = mon->monitor_control ();

	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Monitor"), "monitor-cut-all");
	_monitor->set_cut_all (tact->get_active());
}

void
ARDOUR_UI::monitor_mono ()
{
	boost::shared_ptr<Route> mon = _session->monitor_out ();
	if (!mon) {
		return;
	}
	boost::shared_ptr<ARDOUR::MonitorProcessor> _monitor = mon->monitor_control ();

	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Monitor"), "monitor-mono");
	_monitor->set_mono (tact->get_active());
}

Gtk::Menu*
ARDOUR_UI::shared_popup_menu ()
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::shared_popup_menu, ignored);

	assert (!_shared_popup_menu || !_shared_popup_menu->is_visible());
	delete _shared_popup_menu;
	_shared_popup_menu = new Gtk::Menu;
	return _shared_popup_menu;
}

void
ARDOUR_UI::set_flat_buttons ()
{
	CairoWidget::set_flat_buttons( UIConfiguration::instance().get_flat_buttons() );
}


void
ARDOUR_UI::update_transport_clocks (samplepos_t p)
{
	timepos_t pos (p);

	switch (UIConfiguration::instance().get_primary_clock_delta_mode()) {
		case NoDelta:
			primary_clock->set (pos);
			break;
		case DeltaEditPoint:
			primary_clock->set (pos, false, timecnt_t (editor->get_preferred_edit_position (EDIT_IGNORE_PHEAD)));
			break;
		case DeltaOriginMarker:
			{
				Location* loc = _session->locations()->clock_origin_location ();
				primary_clock->set (pos, false, timecnt_t (loc ? loc->start_sample() : 0));
			}
			break;
	}

	switch (UIConfiguration::instance().get_secondary_clock_delta_mode()) {
		case NoDelta:
			secondary_clock->set (pos);
			break;
		case DeltaEditPoint:
			secondary_clock->set (pos, false, timecnt_t (editor->get_preferred_edit_position (EDIT_IGNORE_PHEAD)));
			break;
		case DeltaOriginMarker:
			{
				Location* loc = _session->locations()->clock_origin_location ();
				secondary_clock->set (pos, false, timecnt_t (loc ? loc->start_sample() : 0));
			}
			break;
	}

	if (big_clock_window) {
		big_clock->set (pos);
	}

	if (!editor->preview_video_drag_active ()) {
		ARDOUR_UI::instance()->video_timeline->manual_seek_video_monitor(p);
	}
}


void
ARDOUR_UI::record_state_changed ()
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::record_state_changed);

	if (!_session) {
		/* why bother - the clock isn't visible */
		return;
	}

	ActionManager::set_sensitive (ActionManager::rec_sensitive_actions, !_session->actively_recording());

	if (_session->record_status () == Session::Recording && _session->have_rec_enabled_track ()) {
		big_clock->set_active (true);
	} else {
		big_clock->set_active (false);
	}
}
