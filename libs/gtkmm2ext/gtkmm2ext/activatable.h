/*
    Copyright (C) 2011 Paul Davis
    Author: David Robillard

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

#ifndef __libgtkmm2ext_activatable_h__
#define __libgtkmm2ext_activatable_h__

#include <gtkmm/action.h>

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {

/**
   A Widget with an associated Action.

   Gtkmm itself has a class for this.  I don't know why we don't use it.
*/
class LIBGTKMM2EXT_API Activatable {
public:
	virtual ~Activatable() {}

	virtual void set_related_action(Glib::RefPtr<Gtk::Action> a) {
		_action = a;
	}

	Glib::RefPtr<Gtk::Action> get_related_action() {
		return _action;
	}

protected:
	Glib::RefPtr<Gtk::Action> _action;
};

} /* namespace */

#endif /* __libgtkmm2ext_actions_h__ */
