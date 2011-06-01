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

#ifndef __ardour_window_proxy_h__
#define __ardour_window_proxy_h__

#include <gtkmm/action.h>
#include <gtkmm/toggleaction.h>
#include "actions.h"

class XMLNode;

/** A class to proxy for a window that may not have been created yet.
 *  It allows the management of visibility, position and size state
 *  so that it can be saved and restored across session loads.
 *
 *  Subclasses of WindowProxy handle windows that are created in different
 *  ways.
 */

class WindowProxyBase
{
public:
	WindowProxyBase (std::string const &, XMLNode const *);
	virtual ~WindowProxyBase () {}

	std::string name () const {
		return _name;
	}

	void maybe_show ();
	XMLNode* get_state () const;
	void setup ();

	/** Show this window */
	virtual void show () = 0;

	/** @return true if the configuration for this window should be
	 *  global (ie across all sessions), otherwise false if it should
	 *  be session-specific.
	 */
	virtual bool rc_configured () const = 0;

	virtual Gtk::Window* get_gtk_window () const = 0;

private:
	XMLNode* state_node (bool, int, int, int, int) const;

	std::string _name; ///< internal unique name for this window
	bool _visible; ///< true if the window should be visible on startup
	int _x_off; ///< x position
	int _y_off; ///< y position
	int _width; ///< width
	int _height; ///< height
};

/** Templated WindowProxy which contains a pointer to the window that is proxying for */
template <class T>
class WindowProxy : public WindowProxyBase
{
public:
	WindowProxy (std::string const & name, XMLNode const * node)
		: WindowProxyBase (name, node)
		, _window (0)
	{

	}

	Gtk::Window* get_gtk_window () const {
		return _window;
	}

	T* get () const {
		return _window;
	}

	/** Set the window and maybe set it up.  To be used after initial window creation */
	void set (T* w, bool s = true) {
		_window = w;
		if (s) {
			setup ();
		}
	}

private:
	T* _window;
};

/** WindowProxy for windows that are created in response to a GTK Action being set active.
 *  Templated on the type of the window.
 */
template <class T>
class ActionWindowProxy : public WindowProxy<T>
{
public:
	/** ActionWindowProxy constructor.
	 *  @param name Unique internal name for this window.
	 *  @param node <UI> node containing <Window> children, the appropriate one of which is used
	 *  to set up this object.
	 *  @param action Name of the ToggleAction that controls this window's visibility.
	 */
	ActionWindowProxy (std::string const & name, XMLNode const * node, std::string const & action)
		: WindowProxy<T> (name, node)
		, _action (action)
	{

	}

	void show () {
		/* Set the appropriate action active so that the window gets shown */
		Glib::RefPtr<Gtk::Action> act = ActionManager::get_action ("Common", _action.c_str());
		if (act) {
			Glib::RefPtr<Gtk::ToggleAction> tact = Glib::RefPtr<Gtk::ToggleAction>::cast_dynamic (act);
			assert (tact);
			tact->set_active (true);
		}
	}

	bool rc_configured () const {
		return true;
	}

private:
	std::string _action;
};

#endif
