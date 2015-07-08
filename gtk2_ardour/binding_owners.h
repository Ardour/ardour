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

#ifndef __gtk_ardour_binding_owners_h__
#define __gtk_ardour_binding_owners_h__

#include <gtkmm/box.h>

#include "gtkmm2ext/bindings.h"

class HasBindings {
  public:
	HasBindings (Gtkmm2ext::Bindings& b) : _bindings (b) {}

	Gtkmm2ext::Bindings& bindings() const { return _bindings; }

  protected:
	Gtkmm2ext::Bindings& _bindings;
};

class VBoxWithBindings : public Gtk::VBox, public HasBindings
{
  public:
	VBoxWithBindings (Gtkmm2ext::Bindings& b) : HasBindings (b) {}
};

#endif /* __gtk_ardour_binding_owners_h__ */
