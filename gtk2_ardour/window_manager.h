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

#ifndef __gtk2_ardour_window_manager_h__
#define __gtk2_ardour_window_manager_h__

#include <string>
#include <map>

#include <boost/function.hpp>
#include <glibmm/refptr.h>
#include <sigc++/trackable.h>

class XMLNode;

namespace Gtk {
	class Window;
	class Action;
}

namespace Gtkmm2ext {
	class VisibilityTracker;
}

namespace ARDOUR {
	class Session;
	class SessionHandlePtr;
}

class WindowManager 
{
  public:
    static WindowManager& instance();

    class ProxyBase : public sigc::trackable {
      public:
	ProxyBase (const std::string& name, const std::string& menu_name);
	ProxyBase (const std::string& name, const std::string& menu_name, const XMLNode&);
	virtual ~ProxyBase();

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

        void set_state (const XMLNode&);
	XMLNode& get_state () const;

	virtual ARDOUR::SessionHandlePtr* session_handle () = 0;

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

	void setup ();
    };

    template<typename T>
    class ProxyWithConstructor: public ProxyBase {
      public:
	ProxyWithConstructor (const std::string& name, const std::string& menu_name, const boost::function<T*()>& c)
		: ProxyBase (name, menu_name) , creator (c) {}
	
	ProxyWithConstructor (const std::string& name, const std::string& menu_name, const boost::function<T*()>& c, const XMLNode* node)
		: ProxyBase (name, menu_name, node) , creator (c) {}
	
	Gtk::Window* get (bool create = false) { 
		if (!_window) {
			if (!create) {
				return 0;
			}

			_window = creator ();

			if (_window) {
				setup ();
			}	
		}

		return _window;
	}

	T* operator->() { 
		return dynamic_cast<T*> (get (true));
	}

	ARDOUR::SessionHandlePtr* session_handle () {
		/* may return null */
		return dynamic_cast<T*> (_window);
	}

      private:
	boost::function<T*()>	creator;
    };

    template<typename T>
    class Proxy : public ProxyBase {
      public:
	Proxy (const std::string& name, const std::string& menu_name)
		: ProxyBase (name, menu_name) {}

	Proxy (const std::string& name, const std::string& menu_name, const XMLNode* node)
		: ProxyBase (name, menu_name, node)  {}
	
	Gtk::Window* get (bool create = false) { 
		if (!_window) {
			if (!create) {
				return 0;
			}

			_window = new T ();

			if (_window) {
				setup ();
			}	
		}

		return _window;
	}

	T* operator->() { 
		/* make return null */
		return dynamic_cast<T*> (_window);
	}

	ARDOUR::SessionHandlePtr* session_handle () {
		return dynamic_cast<T*> (get());
	}

      private:
	boost::function<T*()>	creator;
    };
    
    void register_window (ProxyBase*);
    void remove (const ProxyBase*);
    void toggle_window (ProxyBase*);
    void show_visible () const;
    void set_session (ARDOUR::Session*);
    void add_state (XMLNode&) const;

  private:
    typedef std::list<ProxyBase*> Windows;
    Windows _windows;
    Glib::RefPtr<Gtk::ActionGroup> window_actions;

    WindowManager();
    ~WindowManager();

    static WindowManager* _instance;
};

#endif /* __gtk2_ardour_window_manager_h__ */
