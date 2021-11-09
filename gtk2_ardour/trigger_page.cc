/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/window_title.h"


#include "actions.h"
#include "ardour_ui.h"
#include "gui_thread.h"
#include "public_editor.h"
#include "trigger_page.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace std;

TriggerPage::TriggerPage ()
	: Tabbable (_content, _("Trigger Drom"), X_("trigger"))
{
	load_bindings ();
	register_actions ();

	/* Top-level VBox */
	Label* l = manage (new Label ("Hello World!"));
	_content.pack_start (*l, true, true);
	_content.show ();

	/* setup keybidings */
	_content.set_data ("ardour-bindings", bindings);

	/* subscribe to signals */
	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&TriggerPage::parameter_changed, this, _1), gui_context ());

	/* init */
	update_title ();
}

TriggerPage::~TriggerPage ()
{
}

Gtk::Window*
TriggerPage::use_own_window (bool and_fill_it)
{
	bool new_window = !own_window ();

	Gtk::Window* win = Tabbable::use_own_window (and_fill_it);

	if (win && new_window) {
		win->set_name ("TriggerWindow");
		ARDOUR_UI::instance ()->setup_toplevel_window (*win, _("Trigger Drom"), this);
		win->signal_event ().connect (sigc::bind (sigc::ptr_fun (&Keyboard::catch_user_event_for_pre_dialog_focus), win));
		win->set_data ("ardour-bindings", bindings);
		update_title ();
#if 0 // TODO
		if (!win->get_focus()) {
			win->set_focus (scroller);
		}
#endif
	}

	contents ().show ();
	return win;
}

XMLNode&
TriggerPage::get_state ()
{
	XMLNode* node = new XMLNode (X_("TriggerPage"));
	node->add_child_nocopy (Tabbable::get_state ());
	return *node;
}

int
TriggerPage::set_state (const XMLNode& node, int version)
{
	return Tabbable::set_state (node, version);
}

void
TriggerPage::load_bindings ()
{
	bindings = Bindings::get_bindings (X_("TriggerPage"));
}

void
TriggerPage::register_actions ()
{
	Glib::RefPtr<ActionGroup> group = ActionManager::create_action_group (bindings, X_("TriggerPage"));
}

void
TriggerPage::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	if (!_session) {
		return;
	}

	XMLNode* node = ARDOUR_UI::instance()->trigger_page_settings ();
	set_state (*node, Stateful::loading_state_version);

	_session->DirtyChanged.connect (_session_connections, invalidator (*this), boost::bind (&TriggerPage::update_title, this), gui_context ());
	_session->StateSaved.connect (_session_connections, invalidator (*this), boost::bind (&TriggerPage::update_title, this), gui_context ());

	_session->config.ParameterChanged.connect (_session_connections, invalidator (*this), boost::bind (&TriggerPage::parameter_changed, this, _1), gui_context ());

	update_title ();
}

void
TriggerPage::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &TriggerPage::session_going_away);
	SessionHandlePtr::session_going_away ();
	update_title ();
}

void
TriggerPage::update_title ()
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
		title += S_("Window|Trigger");
		title += Glib::get_application_name ();
		own_window ()->set_title (title.get_string ());

	} else {
		WindowTitle title (S_("Window|Trigger"));
		title += Glib::get_application_name ();
		own_window ()->set_title (title.get_string ());
	}
}

void
TriggerPage::parameter_changed (string const& p)
{
}
