/*
 * Copyright (C) 2013-2016 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2013-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __gtk2_ardour_window_manager_h__
#define __gtk2_ardour_window_manager_h__

#include <string>
#include <map>

#include <boost/function.hpp>
#include <glibmm/refptr.h>
#include <sigc++/trackable.h>

#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/window_proxy.h"

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

namespace WM {

class ProxyBase;

class Manager : public ARDOUR::SessionHandlePtr
{
public:
	static Manager& instance();

	void register_window (ProxyBase*);
	void remove (const ProxyBase*);
	void toggle_window (ProxyBase*);
	void show_visible () const;
	void set_session (ARDOUR::Session*);
	void add_state (XMLNode&) const;

	/* HACK HACK HACK */
	void set_transient_for (Gtk::Window*);
	Gtk::Window* transient_parent() const { return current_transient_parent; }

private:
	typedef std::list<ProxyBase*> Windows;
	Windows _windows;
	Glib::RefPtr<Gtk::ActionGroup> window_actions;
	Gtk::Window* current_transient_parent;

	Manager();
	~Manager();

	static Manager* _instance;
private:
	void window_proxy_was_mapped (ProxyBase*);
	void window_proxy_was_unmapped (ProxyBase*);
};

class ProxyBase : public ARDOUR::SessionHandlePtr, public Gtkmm2ext::WindowProxy
{
public:
	ProxyBase (const std::string& name, const std::string& menu_name);
	ProxyBase (const std::string& name, const std::string& menu_name, const XMLNode&);

	virtual ARDOUR::SessionHandlePtr* session_handle () = 0;

protected:
	void setup ();
};

class ProxyTemporary: public ProxyBase
{
public:
	ProxyTemporary (const std::string& name, Gtk::Window* win);

	Gtk::Window* get (bool create = false) {
		(void) create;
		return _window;
	}

	Gtk::Window* operator->() {
		return _window;
	}

	ARDOUR::SessionHandlePtr* session_handle ();

	void explicit_delete () { _window = 0 ; delete this; }
};

template<typename T>
class ProxyWithConstructor: public ProxyBase
{
public:
	ProxyWithConstructor (const std::string& name, const std::string& menu_name, const boost::function<T*()>& c)
		: ProxyBase (name, menu_name) , creator (c) {}

	ProxyWithConstructor (const std::string& name, const std::string& menu_name, const boost::function<T*()>& c, const XMLNode* node)
		: ProxyBase (name, menu_name, *node) , creator (c) {}

	Gtk::Window* get (bool create = false) {
		if (!_window) {
			if (!create) {
				return 0;
			}

			_window = dynamic_cast<Gtk::Window*> (creator ());

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

	void set_session(ARDOUR::Session *s) {
		SessionHandlePtr::set_session (s);
		ARDOUR::SessionHandlePtr* sp = session_handle ();
		if (sp) {
			sp->set_session (s);
		}
		ARDOUR::SessionHandlePtr* wsp = dynamic_cast<T*>(_window);
		if (wsp && wsp != sp) {
			/* can this happen ? */
			assert (0);
			wsp->set_session(s);
		}
	}

private:
	boost::function<T*()>	creator;
};

template<typename T>
class Proxy : public ProxyBase
{
public:
	Proxy (const std::string& name, const std::string& menu_name)
		: ProxyBase (name, menu_name) {}

	Proxy (const std::string& name, const std::string& menu_name, const XMLNode* node)
		: ProxyBase (name, menu_name, *node)  {}

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
		return dynamic_cast<T*> (get(true));
	}

	ARDOUR::SessionHandlePtr* session_handle () {
		/* may return null */
		return dynamic_cast<T*> (_window);
	}

	void set_session(ARDOUR::Session *s) {
		SessionHandlePtr::set_session (s);
		ARDOUR::SessionHandlePtr* sp = session_handle ();
		if (sp) {
			sp->set_session (s);
		}
		ARDOUR::SessionHandlePtr* wsp = dynamic_cast<T*>(_window);
		if (wsp && wsp != sp) {
			/* can this happen ? */
			assert (0);
			wsp->set_session(s);
		}
	}

private:
	boost::function<T*()>	creator;
};

} /* namespace */

#endif /* __gtk2_ardour_window_manager_h__ */
