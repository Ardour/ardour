/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __gtk2_ardour_control_slave_ui_h__
#define __gtk2_ardour_control_slave_ui_h__

#include <stdint.h>

#include <boost/shared_ptr.hpp>

#include <gtkmm/box.h>
#include <gtkmm/checkmenuitem.h>

#include "pbd/signals.h"
#include "pbd/properties.h"

#include "ardour/session_handle.h"

#include "widgets/ardour_button.h"

namespace ARDOUR {
	class VCA;
	class Stripable;
	class Session;
}

class ControlSlaveUI : public Gtk::HBox, public ARDOUR::SessionHandlePtr
{
public:
	ControlSlaveUI (ARDOUR::Session*);
	~ControlSlaveUI ();
	void set_stripable (boost::shared_ptr<ARDOUR::Stripable>);

private:
	boost::shared_ptr<ARDOUR::Stripable> stripable;
	PBD::ScopedConnectionList   connections;
	PBD::ScopedConnectionList   master_connections;
	ArdourWidgets::ArdourButton initial_button;

	void master_property_changed (PBD::PropertyChange const &);
	void update_vca_display ();
	void vca_menu_toggle (Gtk::CheckMenuItem*, uint32_t n);
	bool specific_vca_button_release (GdkEventButton* ev, uint32_t n);
	bool vca_event_box_release (GdkEventButton* ev);
	bool vca_button_release (GdkEventButton* ev, uint32_t n);
	void add_vca_button (boost::shared_ptr<ARDOUR::VCA>);
	void unassign_all ();

	Gtk::Menu* context_menu;
};

#endif /* __gtk2_ardour_control_slave_ui_h__ */
