/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/label.h>

#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/window_title.h"

#include "ardour/session.h"

#include "actions.h"
#include "ardour_ui.h"
#include "gui_thread.h"
#include "recorder_ui.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace std;

RecorderUI::RecorderUI ()
	: Tabbable (_content, _("Recoder"), X_("recorder"))
{
	load_bindings ();
	register_actions ();

	Label* l = manage (new Label ("Hello World!"));
	_content.pack_start (*l, true, true);
	_content.set_data ("ardour-bindings", bindings);

	update_title ();
	_content.show ();

	//ARDOUR_UI::instance()->Escape.connect (*this, invalidator (*this), boost::bind (&RecorderUI::escape, this), gui_context());
}

RecorderUI::~RecorderUI ()
{
}

void
RecorderUI::cleanup ()
{
}

Gtk::Window*
RecorderUI::use_own_window (bool and_fill_it)
{
	bool new_window = !own_window ();

	Gtk::Window* win = Tabbable::use_own_window (and_fill_it);

	if (win && new_window) {
		win->set_name ("RecorderWindow");
		ARDOUR_UI::instance ()->setup_toplevel_window (*win, _("Recorder"), this);
		win->signal_event ().connect (sigc::bind (sigc::ptr_fun (&Keyboard::catch_user_event_for_pre_dialog_focus), win));
		win->set_data ("ardour-bindings", bindings);
		update_title ();
#if 0 // TODO
		if (!win->get_focus()) {
			win->set_focus (scroller);
		}
#endif
	}

	contents ().show_all ();

	return win;
}

XMLNode&
RecorderUI::get_state ()
{
	XMLNode* node = new XMLNode (X_("Recorder"));
	node->add_child_nocopy (Tabbable::get_state());
	return *node;
}

int
RecorderUI::set_state (const XMLNode& node, int version)
{
	return Tabbable::set_state (node, version);
}

void
RecorderUI::load_bindings ()
{
	bindings = Bindings::get_bindings (X_("Recorder"));
}

void
RecorderUI::register_actions ()
{
	Glib::RefPtr<ActionGroup> group = ActionManager::create_action_group (bindings, X_("Recorder"));
}

void
RecorderUI::set_session (ARDOUR::Session* s)
{
	SessionHandlePtr::set_session (s);

	if (!_session) {
		return;
	}

	XMLNode* node = ARDOUR_UI::instance()->recorder_settings();
	set_state (*node, Stateful::loading_state_version);

	_session->DirtyChanged.connect (_session_connections, invalidator (*this), boost::bind (&RecorderUI::update_title, this), gui_context ());
	_session->StateSaved.connect (_session_connections, invalidator (*this), boost::bind (&RecorderUI::update_title, this), gui_context ());

	_session->config.ParameterChanged.connect (_session_connections, invalidator (*this), boost::bind (&RecorderUI::parameter_changed, this, _1), gui_context ());
	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&RecorderUI::parameter_changed, this, _1), gui_context ());

	update_title ();
}

void
RecorderUI::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &RecorderUI::session_going_away);
	SessionHandlePtr::session_going_away ();
	update_title ();
}

void
RecorderUI::update_title ()
{
	if (!own_window ()) {
		return;
	}

	if (_session) {
		string n;

		if (_session->snap_name () != _session->name ()) {
			n = _session->snap_name ();
		} else {
			n = _session->name ();
		}

		if (_session->dirty ()) {
			n = "*" + n;
		}

		WindowTitle title (n);
		title += S_("Window|Recorder");
		title += Glib::get_application_name ();
		own_window ()->set_title (title.get_string ());

	} else {
		WindowTitle title (S_("Window|Recorder"));
		title += Glib::get_application_name ();
		own_window ()->set_title (title.get_string ());
	}
}

void
RecorderUI::parameter_changed (string const& p)
{
}
