/* menu_elems.h
 *
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 1998-2002 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <gtk/gtk.h>
#include "gtkmm2ext/menu_elems.h"

using namespace Gtkmm2ext;
using namespace Gtk;

MenuElemNoMnemonic::MenuElemNoMnemonic (const Glib::ustring& label, const CallSlot& slot)
{
	set_child (manage (new MenuItem (label, false)));
	if(slot) {
		child_->signal_activate().connect(slot);
	}
	child_->show();
}

CheckMenuElemNoMnemonic::CheckMenuElemNoMnemonic (const Glib::ustring& label, const CallSlot& slot)
{
	Gtk::CheckMenuItem* item = manage (new Gtk::CheckMenuItem (label, false));
	set_child (item);
	if(slot) {
		item->signal_toggled().connect(slot);
	}
	child_->show();
}
