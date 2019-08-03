/*
 * Copyright (C) 2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef _WIDGETS_ARDOUR_DISPLAY_H_
#define _WIDGETS_ARDOUR_DISPLAY_H_

#include <list>
#include <stdint.h>

#include <gtkmm/action.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>

#include "widgets/ardour_dropdown.h"
#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API ArdourDisplay : public ArdourWidgets::ArdourDropdown
{
public:

	ArdourDisplay (Element e = default_elements);
	virtual ~ArdourDisplay ();

	boost::shared_ptr<PBD::Controllable> get_controllable() { return binding_proxy.get_controllable(); }
	void set_controllable (boost::shared_ptr<PBD::Controllable> c);

	bool on_scroll_event (GdkEventScroll* ev);

	void add_controllable_preset (const char*, float);
	void handle_controllable_preset (float p);

	void controllable_changed ();
	PBD::ScopedConnection watch_connection;
};

} /* end namespace */

#endif /* __gtk2_ardour_ardour_menu_h__ */
