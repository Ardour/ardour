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

#ifndef __gtkmm2ext_window_proxy_h__

#include <string>
#include <gdkmm/event.h>
#include <glibmm/refptr.h>
#include <sigc++/trackable.h>

class XMLNode;

#include "gtkmm2ext/visibility.h"

namespace Gtk {
class Window;
class Action;
}

namespace Gtkmm2ext {

class VisibilityTracker;

class LIBGTKMM2EXT_API WindowProxy : public virtual sigc::trackable
{
  public:
	WindowProxy (const std::string& name, const std::string& menu_name);
	WindowProxy (const std::string& name, const std::string& menu_name, const XMLNode&);
	virtual ~WindowProxy();
    
	void show ();
	void show_all ();
	void hide ();
	void present ();
	void maybe_show ();
    
	bool visible() const { return _visible; }
	const std::string& name() const { return _name; }
	const std::string& menu_name() const { return _menu_name; }
    
	std::string action_name() const;
	void set_action (Glib::RefPtr<Gtk::Action>);
	Glib::RefPtr<Gtk::Action> action() const { return _action; };
    
	void drop_window ();
	void use_window (Gtk::Window&);
    
	virtual Gtk::Window* get (bool create = false) = 0;
    
	virtual void toggle ();
    
	virtual void set_state (const XMLNode&);
	virtual XMLNode& get_state () const;
    
	operator bool() const { return _window != 0; }
    
  protected:
	std::string  _name;
	std::string  _menu_name;
	Glib::RefPtr<Gtk::Action> _action;
	Gtk::Window* _window;
	mutable bool _visible; ///< true if the window should be visible on startup
	mutable int  _x_off; ///< x position
	mutable int  _y_off; ///< y position
	mutable int  _width; ///< width
	mutable int  _height; ///< height
	Gtkmm2ext::VisibilityTracker* vistracker;

	void save_pos_and_size ();
	void set_pos_and_size ();
	
	virtual bool delete_event_handler (GdkEventAny *ev);
    
	virtual void setup ();
};

}

#endif /* __gtkmm2ext_window_proxy_h__ */
