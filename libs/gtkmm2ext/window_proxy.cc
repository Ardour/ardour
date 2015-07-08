/*
    Copyright (C) 2015 Paul Davis

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

#include <gtkmm/action.h>
#include <gtkmm/window.h>

#include "pbd/convert.h"
#include "pbd/xml++.h"

#include "gtkmm2ext/window_proxy.h"
#include "gtkmm2ext/visibility_tracker.h"

#include "i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;

WindowProxy::WindowProxy (const std::string& name)
	: _name (name)
	, _window (0)
	, _visible (false)
	, _x_off (-1)
	, _y_off (-1)
	, _width (-1)
	, _height (-1) 
	, vistracker (0)
{
}

WindowProxy::WindowProxy (const std::string& name, const std::string& menu_name)
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

WindowProxy::WindowProxy (const std::string& name, const std::string& menu_name, const XMLNode& node)
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
	set_state (node, 0);
}

WindowProxy::~WindowProxy ()
{
	delete vistracker;
	delete _window;
}

int
WindowProxy::set_state (const XMLNode& node, int /* version */)
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

	/* if the window is marked visible but doesn't yet exist, create it */
	
	if (_visible) {
		if (!_window) {
			_window = get (true);
		}
	}
	
	if (_window) {
		setup ();
	}

	return 0;
}

void
WindowProxy::set_action (Glib::RefPtr<Gtk::Action> act)
{
	_action = act;
}

std::string
WindowProxy::action_name() const 
{
	return string_compose (X_("toggle-%1"), _name);
}

void
WindowProxy::toggle() 
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
WindowProxy::get_state ()
{
	XMLNode* node = new XMLNode (X_("Window"));
	char buf[32];	

	node->add_property (X_("name"), _name);

	if (_window && vistracker) {
		
		/* we have a window, so use current state */

		_visible = vistracker->partially_visible ();
		_window->get_position (_x_off, _y_off);
		_window->get_size (_width, _height);
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
WindowProxy::drop_window ()
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
WindowProxy::use_window (Gtk::Window& win)
{
	drop_window ();
	_window = &win;
	setup ();
}

void
WindowProxy::setup ()
{
	assert (_window);

	vistracker = new Gtkmm2ext::VisibilityTracker (*_window);
	_window->signal_delete_event().connect (sigc::mem_fun (*this, &WindowProxy::delete_event_handler));

	set_pos_and_size ();
}
	
void
WindowProxy::show ()
{
	get (true);
	assert (_window);
	_window->show ();
}

void
WindowProxy::maybe_show ()
{
	if (_visible) {
		show ();
	}
}

void
WindowProxy::show_all ()
{
	get (true);
	assert (_window);
	_window->show_all ();
}

void
WindowProxy::present ()
{
	get (true);
	assert (_window);

	_window->show_all ();
	_window->present ();

	/* turn off any mouse-based positioning */
	_window->set_position (Gtk::WIN_POS_NONE);
}

void
WindowProxy::hide ()
{
	if (_window) {
		save_pos_and_size();
		_window->hide ();
	}
}

bool
WindowProxy::delete_event_handler (GdkEventAny* /*ev*/)
{
	hide();
	return true;
}

void
WindowProxy::save_pos_and_size ()
{
	if (_window) {
		_window->get_position (_x_off, _y_off);
		_window->get_size (_width, _height);
	}
}

void
WindowProxy::set_pos_and_size ()
{
	if (!_window) {
		return;
	}

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
}

