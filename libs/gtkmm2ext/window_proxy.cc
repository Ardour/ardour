/*
 * Copyright (C) 2015-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/action.h>
#include <gtkmm/window.h>

#include "pbd/xml++.h"

#include "gtkmm2ext/window_proxy.h"
#include "gtkmm2ext/visibility_tracker.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;

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
	, _state_mask (StateMask (Position|Size))
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
		std::string name;
		if (child->name () == X_("Window") && child->get_property (X_("name"), name) &&
		    name == _name) {
			break;
		}

		++i;
	}

	if (i != children.end()) {

		child = *i;

		child->get_property (X_("visible"), _visible);
		child->get_property (X_("x-off"), _x_off);
		child->get_property (X_("y-off"), _y_off);
		child->get_property (X_("x-size"), _width);
		child->get_property (X_("y-size"), _height);
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

		if (vistracker) {
			vistracker->cycle_visibility ();
		} else {
			_window->present ();
		}

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

	node->set_property (X_("name"), _name);

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

	node->set_property (X_("visible"), _visible);
	node->set_property (X_("x-off"), x);
	node->set_property (X_("y-off"), y);
	node->set_property (X_("x-size"), w);
	node->set_property (X_("y-size"), h);

	return *node;
}

void
WindowProxy::drop_window ()
{
	if (_window) {
		_window->hide ();
		delete_connection.disconnect ();
		configure_connection.disconnect ();
		map_connection.disconnect ();
		unmap_connection.disconnect ();
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

	assert (_window);

	delete_connection = _window->signal_delete_event().connect (sigc::mem_fun (*this, &WindowProxy::delete_event_handler));
	configure_connection = _window->signal_configure_event().connect (sigc::mem_fun (*this, &WindowProxy::configure_handler), false);
	map_connection = _window->signal_map().connect (sigc::mem_fun (*this, &WindowProxy::map_handler), false);
	unmap_connection = _window->signal_unmap().connect (sigc::mem_fun (*this, &WindowProxy::unmap_handler), false);

	set_pos_and_size ();
}

void
WindowProxy::map_handler ()
{
	vistracker = new Gtkmm2ext::VisibilityTracker (*_window);
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
