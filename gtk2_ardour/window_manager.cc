/*
 * Copyright (C) 2013-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2018 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/window.h>

#include "pbd/xml++.h"

#include "ardour/session_handle.h"

#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/visibility_tracker.h"

#include "actions.h"
#include "ardour_dialog.h"
#include "ardour_ui.h"
#include "ardour_window.h"
#include "window_manager.h"
#include "processor_box.h"

#include "pbd/i18n.h"

using std::string;
using namespace WM;
using namespace PBD;

Manager* Manager::_instance = 0;

Manager&
Manager::instance ()
{
	if (!_instance) {
		_instance = new Manager;
	}
	return *_instance;
}

Manager::Manager ()
	: current_transient_parent (0)
{
}

Manager::~Manager ()
{
}

void
Manager::register_window (ProxyBase* info)
{
	_windows.push_back (info);

	if (!info->menu_name().empty()) {

		if (!window_actions) {
			window_actions = ActionManager::create_action_group (Gtkmm2ext::UI::instance()->global_bindings, X_("Window"));
		}

		ActionManager::register_toggle_action (window_actions,
		                                       info->action_name().c_str(), info->menu_name().c_str(),
		                                       sigc::bind (sigc::mem_fun (*this, &Manager::toggle_window), info));

		info->signal_map.connect (sigc::bind (sigc::mem_fun (*this, &Manager::window_proxy_was_mapped), info));
		info->signal_unmap.connect (sigc::bind (sigc::mem_fun (*this, &Manager::window_proxy_was_unmapped), info));

	}
}

void
Manager::window_proxy_was_mapped (ProxyBase* proxy)
{
	Glib::RefPtr<Gtk::Action> act = ActionManager::get_action (string_compose ("%1/%2", window_actions->get_name(), proxy->action_name()));
	if (!act) {
		return;
	}
	Glib::RefPtr<Gtk::ToggleAction> tact = Glib::RefPtr<Gtk::ToggleAction>::cast_dynamic (act);
	if (!tact) {
		return;
	}

	tact->set_active (true);
}

void
Manager::window_proxy_was_unmapped (ProxyBase* proxy)
{
	Glib::RefPtr<Gtk::Action> act = ActionManager::get_action (string_compose ("%1/%2", window_actions->get_name(), proxy->action_name()));
	if (!act) {
		return;
	}
	Glib::RefPtr<Gtk::ToggleAction> tact = Glib::RefPtr<Gtk::ToggleAction>::cast_dynamic (act);
	if (!tact) {
		return;
	}

	tact->set_active (false);
}

void
Manager::remove (const ProxyBase* info)
{
	for (Windows::iterator i = _windows.begin(); i != _windows.end(); ++i) {
		if ((*i) == info) {
			_windows.erase (i);
			return;
		}
	}
}

void
Manager::toggle_window (ProxyBase* proxy)
{
	Glib::RefPtr<Gtk::Action> act = ActionManager::get_action (string_compose ("%1/%2", window_actions->get_name(), proxy->action_name()));
	if (!act) {
		return;
	}
	Glib::RefPtr<Gtk::ToggleAction> tact = Glib::RefPtr<Gtk::ToggleAction>::cast_dynamic (act);
	if (!tact) {
		return;
	}

	if (tact->get_active()) {
		proxy->present ();
	} else {
		proxy->hide ();
	}
}

void
Manager::show_visible() const
{
	for (Windows::const_iterator i = _windows.begin(); i != _windows.end(); ++i) {
		if ((*i)->visible()) {
			Gtk::Window* win = (*i)->get (true);
			if (!win) {
				/* the window may be a plugin GUI for a plugin which
				 * is disabled or longer present.
				 */
				continue;
			}
			if (dynamic_cast<ArdourDialog*> (win)) {
				/* do not show dialogs at startup. Most
				 * dialogs require some signal connection work
				 * because we are trying to avoid recursive
				 * event loops (connecting instead to
				 * ::signal_response(). This means we need to
				 * destroy the window as well, so that the code
				 * which checks if it should be created will
				 * find that it is missing and will create it
				 * and connect to any necessary signals.
				 */
				(*i)->drop_window ();
				continue;
			}
			(*i)->show_all ();
			(*i)->present ();
		}
	}
}

void
Manager::add_state (XMLNode& root) const
{
	for (Windows::const_iterator i = _windows.begin(); i != _windows.end(); ++i) {
		/* don't save state for temporary proxy windows
		 */

		if (dynamic_cast<ProxyTemporary*> (*i)) {
			continue;
		}

		root.add_child_nocopy ((*i)->get_state());
	}
}

void
Manager::set_session (ARDOUR::Session* s)
{
	SessionHandlePtr::set_session (s);
	for (Windows::const_iterator i = _windows.begin(); i != _windows.end(); ++i) {
		(*i)->set_session(s);
	}
}

void
Manager::set_transient_for (Gtk::Window* parent)
{
	/* OS X has a richer concept of window layering than X does (or
	 * certainly, than any accepted conventions on X), and so the use of
	 * Manager::set_transient_for() is not necessary on that platform.
	 *
	 * On OS X this is mostly taken care of by using the window type rather
	 * than explicit 1:1 transient-for relationships.
	 */

#ifndef __APPLE__
	if (parent) {
		for (Windows::const_iterator i = _windows.begin(); i != _windows.end(); ++i) {
			Gtk::Window* win = (*i)->get();
			if (win) {
				win->set_transient_for (*parent);
			}
		}
	} else {
		for (Windows::const_iterator i = _windows.begin(); i != _windows.end(); ++i) {
			Gtk::Window* win = (*i)->get();
			if (win) {
				gtk_window_set_transient_for (win->gobj(), 0);
			}
		}
	}

	current_transient_parent = parent;
#endif
}

/*-------------------------*/

ProxyBase::ProxyBase (const std::string& name, const std::string& menu_name)
	: WindowProxy (name, menu_name)
{
}

ProxyBase::ProxyBase (const std::string& name, const std::string& menu_name, const XMLNode& node)
	: WindowProxy (name, menu_name, node)
{
}

void
ProxyBase::setup ()
{
	WindowProxy::setup ();
	set_session(_session);

}

/*-----------------------*/

ProxyTemporary::ProxyTemporary (const string& name, Gtk::Window* win)
	: ProxyBase (name, string())
{
	_window = win;
}

ARDOUR::SessionHandlePtr*
ProxyTemporary::session_handle()
{
	/* may return null */
	ArdourWindow* aw = dynamic_cast<ArdourWindow*> (_window);
	if (aw) { return aw; }
	ArdourDialog* ad = dynamic_cast<ArdourDialog*> (_window);
	if (ad) { return ad; }
	return 0;
}
