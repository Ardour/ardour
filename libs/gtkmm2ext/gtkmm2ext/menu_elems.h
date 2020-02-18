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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef _gtkmm2ext_menu_elems_h_
#define _gtkmm2ext_menu_elems_h_

#include <gtkmm/menu_elems.h>
#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API MenuElemNoMnemonic : public Gtk::Menu_Helpers::Element
{
public:
	MenuElemNoMnemonic (const Glib::ustring& label, const CallSlot& slot);
};

class LIBGTKMM2EXT_API CheckMenuElemNoMnemonic : public Gtk::Menu_Helpers::Element
{
public:
	CheckMenuElemNoMnemonic (const Glib::ustring& label, const CallSlot& slot = CallSlot());
};

}
#endif
