/*
    Copyright (C) 2004 Paul Davis

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

#ifndef __bindable_button_h__
#define __bindable_button_h__

#include <string>

#include "pbd/signals.h"

#include "gtkmm2ext/visibility.h"
#include "gtkmm2ext/stateful_button.h"
#include "gtkmm2ext/binding_proxy.h"

namespace PBD {
	class Controllable;
}

class LIBGTKMM2EXT_API BindableToggleButton : public Gtkmm2ext::StatefulToggleButton
{
   public:
	BindableToggleButton (const std::string &label)
		: Gtkmm2ext::StatefulToggleButton (label) {}
	BindableToggleButton () {}

	virtual ~BindableToggleButton() {}
	
	bool on_button_press_event (GdkEventButton *ev) {
		if (!binding_proxy.button_press_handler (ev)) {
			StatefulToggleButton::on_button_press_event (ev);
			return false;
		} else {
			return true;
		}
	}
	
	boost::shared_ptr<PBD::Controllable> get_controllable() { return binding_proxy.get_controllable(); }
 	void set_controllable (boost::shared_ptr<PBD::Controllable> c);
        void watch ();

  protected:
        void controllable_changed ();
        PBD::ScopedConnection watch_connection;

  private:
	BindingProxy binding_proxy;
};

class LIBGTKMM2EXT_API BindableButton : public Gtkmm2ext::StatefulButton
{
   public:
	BindableButton (boost::shared_ptr<PBD::Controllable> c) : binding_proxy (c) {}
	~BindableButton() {}
	
	bool on_button_press_event (GdkEventButton *ev) {
		if (!binding_proxy.button_press_handler (ev)) {
			StatefulButton::on_button_press_event (ev);
			return false;
		} else {
			return true;
		}
	}

	boost::shared_ptr<PBD::Controllable> get_controllable() { return binding_proxy.get_controllable(); }
 	void set_controllable (boost::shared_ptr<PBD::Controllable> c) { binding_proxy.set_controllable (c); }

  private:
	BindingProxy binding_proxy;
};

#endif
