/*
    Copyright (C) 2013 Paul Davis

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

#include <gtkmm/window.h>

#include "pbd/xml++.h"

#include "ardour/session_handle.h"

#include "gtkmm2ext/visibility_tracker.h"

#include "actions.h"
#include "ardour_dialog.h"
#include "ardour_window.h"
#include "window_manager.h"
#include "processor_box.h"

#include "i18n.h"

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
			window_actions = Gtk::ActionGroup::create (X_("Window"));
			ActionManager::add_action_group (window_actions);
		}

		info->set_action (ActionManager::register_action (window_actions, info->action_name().c_str(), info->menu_name().c_str(), 
								  sigc::bind (sigc::mem_fun (*this, &Manager::toggle_window), info)));
	}
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
	if (proxy) {
		proxy->toggle ();
	}
}

void
Manager::show_visible() const
{
	for (Windows::const_iterator i = _windows.begin(); i != _windows.end(); ++i) {
		if ((*i)->visible()) {
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
		if (dynamic_cast<ProcessorWindowProxy*> (*i)) {
			ProcessorWindowProxy *pi = dynamic_cast<ProcessorWindowProxy*> (*i);
			root.add_child_nocopy (pi->get_state());
		} else {
			root.add_child_nocopy ((*i)->get_state());
		}
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

ProxyBase::ProxyBase (const string& name, const std::string& menu_name)
	: _name (name)
	, _menu_name (menu_name)
	, _window (0)
	, _visible (false)
	, _x_off (-1)
	, _y_off (-1)
	, _width (-1)
	, _height (-1) 
	, vistracker (0)
{
}

ProxyBase::ProxyBase (const string& name, const std::string& menu_name, const XMLNode& node)
	: _name (name)
	, _menu_name (menu_name)
	, _window (0)
	, _visible (false)
	, _x_off (-1)
	, _y_off (-1)
	, _width (-1)
	, _height (-1) 
	, vistracker (0)
{
	set_state (node);
}

ProxyBase::~ProxyBase ()
{
	delete vistracker;
}

void
ProxyBase::set_state (const XMLNode& node)
{
	XMLNodeList children = node.children ();

	XMLNodeList::const_iterator i = children.begin ();

	while (i != children.end()) {
		XMLProperty* prop = (*i)->property (X_("name"));
		if ((*i)->name() == X_("Window") && prop && prop->value() == _name) {
			break;
		}

		++i;
	}

	if (i != children.end()) {

		XMLProperty* prop;

		if ((prop = (*i)->property (X_("visible"))) != 0) {
			_visible = PBD::string_is_affirmative (prop->value ());
		}

		if ((prop = (*i)->property (X_("x-off"))) != 0) {
			_x_off = atoi (prop->value());
		}
		if ((prop = (*i)->property (X_("y-off"))) != 0) {
			_y_off = atoi (prop->value());
		}
		if ((prop = (*i)->property (X_("x-size"))) != 0) {
			_width = atoi (prop->value());
		}
		if ((prop = (*i)->property (X_("y-size"))) != 0) {
			_height = atoi (prop->value());
		}
	}

	/* if we have a window already, reset its properties */

	if (_window) {
		setup ();
	}
}

void
ProxyBase::set_action (Glib::RefPtr<Gtk::Action> act)
{
	_action = act;
}

std::string
ProxyBase::action_name() const 
{
	return string_compose (X_("toggle-%1"), _name);
}

void
ProxyBase::toggle() 
{
	if (!_window) {
		(void) get (true);
		assert (_window);
		/* XXX this is a hack - the window object should really
		   ensure its components are all visible. sigh.
		*/
		_window->show_all();
		/* we'd like to just call this and nothing else */
		_window->present ();

		if (_width != -1 && _height != -1) {
			_window->set_default_size (_width, _height);
		}
		if (_x_off != -1 && _y_off != -1) {
			_window->move (_x_off, _y_off);
		}

	} else {
		if (_window->is_mapped()) {
			save_pos_and_size();
		}
		vistracker->cycle_visibility ();
		if (_window->is_mapped()) {
			if (_width != -1 && _height != -1) {
				_window->set_default_size (_width, _height);
			}
			if (_x_off != -1 && _y_off != -1) {
				_window->move (_x_off, _y_off);
			}
		}
	}
}

XMLNode&
ProxyBase::get_state () const
{
	XMLNode* node = new XMLNode (X_("Window"));
	char buf[32];	

	node->add_property (X_("name"), _name);

	if (_window && vistracker) {
		
		/* we have a window, so use current state */

		_visible = vistracker->partially_visible ();
		if (_visible) {
			_window->get_position (_x_off, _y_off);
			_window->get_size (_width, _height);
		}
	}

	node->add_property (X_("visible"), _visible? X_("yes") : X_("no"));
	
	snprintf (buf, sizeof (buf), "%d", _x_off);
	node->add_property (X_("x-off"), buf);
	snprintf (buf, sizeof (buf), "%d", _y_off);
	node->add_property (X_("y-off"), buf);
	snprintf (buf, sizeof (buf), "%d", _width);
	node->add_property (X_("x-size"), buf);
	snprintf (buf, sizeof (buf), "%d", _height);
	node->add_property (X_("y-size"), buf);

	return *node;
}

void
ProxyBase::drop_window ()
{
	if (_window) {
		_window->hide ();
		delete _window;
		_window = 0;
		delete vistracker;
		vistracker = 0;
	}
}

void
ProxyBase::use_window (Gtk::Window& win)
{
	drop_window ();
	_window = &win;
	setup ();
}

void
ProxyBase::setup ()
{
	assert (_window);

	vistracker = new Gtkmm2ext::VisibilityTracker (*_window);
	_window->signal_delete_event().connect (sigc::mem_fun (*this, &ProxyBase::handle_win_event));

	if (_width != -1 || _height != -1 || _x_off != -1 || _y_off != -1) {
		/* cancel any mouse-based positioning */
		_window->set_position (Gtk::WIN_POS_NONE);
	}

	if (_width != -1 && _height != -1) {
		_window->set_default_size (_width, _height);
	}

	if (_x_off != -1 && _y_off != -1) {
		_window->move (_x_off, _y_off);
	}
	set_session(_session);
}
	
void
ProxyBase::show ()
{
	Gtk::Window* win = get (true);
	win->show ();
}

void
ProxyBase::maybe_show ()
{
	if (_visible) {
		show ();
	}
}

void
ProxyBase::show_all ()
{
	Gtk::Window* win = get (true);
	win->show_all ();
}


void
ProxyBase::present ()
{
	Gtk::Window* win = get (true);
	win->show_all ();
	win->present ();

	/* turn off any mouse-based positioning */
	_window->set_position (Gtk::WIN_POS_NONE);
}

void
ProxyBase::hide ()
{
	Gtk::Window* win = get (false);
	if (win) {
		save_pos_and_size();
		win->hide ();
	}
}

bool
ProxyBase::handle_win_event (GdkEventAny *ev)
{
	hide();
	return true;
}

void
ProxyBase::save_pos_and_size ()
{
	Gtk::Window* win = get (false);
	if (win) {
		win->get_position (_x_off, _y_off);
		win->get_size (_width, _height);
	}
}
/*-----------------------*/

ProxyTemporary::ProxyTemporary (const string& name, Gtk::Window* win)
	: ProxyBase (name, string())
{
	_window = win;
}

ProxyTemporary::~ProxyTemporary ()
{
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
