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
#include "pbd/stacktrace.h"

#include "gtkmm2ext/window_proxy.h"
#include "gtkmm2ext/visibility_tracker.h"

#include "pbd/i18n.h"

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
	, _state_mask (StateMask (Position|Size))
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
	, _state_mask (StateMask (Position|Size))
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
	XMLNode const * child;
	XMLNodeList::const_iterator i = children.begin ();

	while (i != children.end()) {
		child = *i;
		XMLProperty const * prop = child->property (X_("name"));
		if (child->name() == X_("Window") && prop && prop->value() == _name) {
			break;
		}

		++i;
	}

	if (i != children.end()) {

		XMLProperty const * prop;
		child = *i;

		if ((prop = child->property (X_("visible"))) != 0) {
			_visible = PBD::string_is_affirmative (prop->value ());
		}

		if ((prop = child->property (X_("x-off"))) != 0) {
			_x_off = atoi (prop->value());
		}
		if ((prop = child->property (X_("y-off"))) != 0) {
			_y_off = atoi (prop->value());
		}
		if ((prop = child->property (X_("x-size"))) != 0) {
			_width = atoi (prop->value());
		}
		if ((prop = child->property (X_("y-size"))) != 0) {
			_height = atoi (prop->value());
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
		setup ();
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

std::string
WindowProxy::xml_node_name()
{
	return X_("Window");
}

XMLNode&
WindowProxy::get_state ()
{
	XMLNode* node = new XMLNode (xml_node_name());
	char buf[32];

	node->add_property (X_("name"), _name);

	if (_window && vistracker) {

		/* we have a window, so use current state */

		_visible = vistracker->partially_visible ();
		_window->get_position (_x_off, _y_off);
		_window->get_size (_width, _height);
	}

	int x, y, w, h;

	if (_state_mask & Position) {
		x = _x_off;
		y = _y_off;
	} else {
		x = -1;
		y = -1;
	}

	if (_state_mask & Size) {
		w = _width;
		h = _height;
	} else {
		w = -1;
		h = -1;
	}

	node->add_property (X_("visible"), _visible? X_("yes") : X_("no"));
	snprintf (buf, sizeof (buf), "%d", x);
	node->add_property (X_("x-off"), buf);
	snprintf (buf, sizeof (buf), "%d", y);
	node->add_property (X_("y-off"), buf);
	snprintf (buf, sizeof (buf), "%d", w);
	node->add_property (X_("x-size"), buf);
	snprintf (buf, sizeof (buf), "%d", h);
	node->add_property (X_("y-size"), buf);

	return *node;
}

void
WindowProxy::drop_window ()
{
	if (_window) {
		delete_connection.disconnect ();
		configure_connection.disconnect ();
		map_connection.disconnect ();
		unmap_connection.disconnect ();
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

	delete_connection = _window->signal_delete_event().connect (sigc::mem_fun (*this, &WindowProxy::delete_event_handler));
	configure_connection = _window->signal_configure_event().connect (sigc::mem_fun (*this, &WindowProxy::configure_handler), false);
	map_connection = _window->signal_map().connect (sigc::mem_fun (*this, &WindowProxy::map_handler), false);
	unmap_connection = _window->signal_unmap().connect (sigc::mem_fun (*this, &WindowProxy::unmap_handler), false);

	set_pos_and_size ();
}

void
WindowProxy::map_handler ()
{
	/* emit our own signal */
	signal_map ();
}

void
WindowProxy::unmap_handler ()
{
	/* emit out own signal */
	signal_unmap ();
}

bool
WindowProxy::configure_handler (GdkEventConfigure* ev)
{
	/* stupidly, the geometry data in the event isn't the same as we get
	   from the window geometry APIs.so we have to actively interrogate
	   them to get the new information.

	   the difference is generally down to window manager framing.
	*/
	if (!visible() || !_window->is_mapped()) {
		return false;
	}
	save_pos_and_size ();
	return false;
}


bool
WindowProxy::visible() const
{
	if (vistracker) {
		/* update with current state */
		_visible = vistracker->partially_visible();
	}
	return _visible;
}

bool
WindowProxy::fully_visible () const
{
	if (!vistracker) {
		/* no vistracker .. no window .. cannot be fully visible */
		return false;
	}
	return vistracker->fully_visible();
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
	if (_action) {
		_action->activate ();
	} else {
		hide();
	}

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

	if ((_state_mask & Position) && (_width != -1 || _height != -1 || _x_off != -1 || _y_off != -1)) {
		/* cancel any mouse-based positioning */
		_window->set_position (Gtk::WIN_POS_NONE);
	}

	if ((_state_mask & Size) && _width != -1 && _height != -1) {
		_window->resize (_width, _height);
	}

	if ((_state_mask & Position) && _x_off != -1 && _y_off != -1) {
		_window->move (_x_off, _y_off);
	}
}

void
WindowProxy::set_pos ()
{
	if (!_window) {
		return;
	}

	if (!(_state_mask & Position)) {
		return;
	}

	if (_width != -1 || _height != -1 || _x_off != -1 || _y_off != -1) {
		/* cancel any mouse-based positioning */
		_window->set_position (Gtk::WIN_POS_NONE);
	}

	if (_x_off != -1 && _y_off != -1) {
		_window->move (_x_off, _y_off);
	}
}

void
WindowProxy::set_state_mask (StateMask sm)
{
	_state_mask = sm;
}
