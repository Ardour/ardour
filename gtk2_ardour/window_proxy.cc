/*
    Copyright (C) 2010 Paul Davis

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
#include "window_proxy.h"

#include "pbd/convert.h"

#include "i18n.h"

using namespace std;

/** WindowProxyBase constructor.
 *  @param name Unique internal name for this window.
 *  @param node <UI> node containing <Window> children, the appropriate one of which is used
 *  to set up this object.
 */
WindowProxyBase::WindowProxyBase (string const & name, XMLNode const * node)
	: _name (name)
	, _visible (false)
	, _x_off (-1)
	, _y_off (-1)
	, _width (-1)
	, _height (-1)
{
	if (!node) {
		return;
	}

	XMLNodeList children = node->children ();

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
			_x_off = atoi (prop->value().c_str());
		}
		if ((prop = (*i)->property (X_("y-off"))) != 0) {
			_y_off = atoi (prop->value().c_str());
		}
		if ((prop = (*i)->property (X_("x-size"))) != 0) {
			_width = atoi (prop->value().c_str());
		}
		if ((prop = (*i)->property (X_("y-size"))) != 0) {
			_height = atoi (prop->value().c_str());
		}
	}
}

/** Show this window if it was configured as visible.  This should
 *  be called at session startup only.
 */
void
WindowProxyBase::maybe_show ()
{
	if (_visible) {
		show ();
	}
}

/** Set up our window's position and size */
void
WindowProxyBase::setup ()
{
	Gtk::Window* window = get_gtk_window ();
	if (!window) {
		return;
	}

	if (_width != -1 && _height != -1) {
		window->set_default_size (_width, _height);
	}

	if (_x_off != -1 && _y_off != -1) {
		window->move (_x_off, _y_off);
	}
}

XMLNode *
WindowProxyBase::get_state () const
{
	bool v = _visible;
	int x = _x_off;
	int y = _y_off;
	int w = _width;
	int h = _height;

	/* If the window has been created, get its current state; otherwise use
	   the state that we started off with.
	*/

	Gtk::Window* gtk_window = get_gtk_window ();
	if (gtk_window) {
		v = gtk_window->is_visible ();

		Glib::RefPtr<Gdk::Window> gdk_window = gtk_window->get_window ();
		if (gdk_window) {
			gdk_window->get_position (x, y);
			gdk_window->get_size (w, h);
		}

	}

	return state_node (v, x, y, w, h);
}


XMLNode *
WindowProxyBase::state_node (bool v, int x, int y, int w, int h) const
{
	XMLNode* node = new XMLNode (X_("Window"));
	node->add_property (X_("name"), _name);
	node->add_property (X_("visible"), v ? X_("yes") : X_("no"));

	char buf[32];
	snprintf (buf, sizeof (buf), "%d", x);
	node->add_property (X_("x-off"), buf);
	snprintf (buf, sizeof (buf), "%d", y);
	node->add_property (X_("y-off"), buf);
	snprintf (buf, sizeof (buf), "%d", w);
	node->add_property (X_("x-size"), buf);
	snprintf (buf, sizeof (buf), "%d", h);
	node->add_property (X_("y-size"), buf);

	return node;
}
