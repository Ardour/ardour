/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2011 Paul Davis
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <boost/algorithm/string.hpp>
#ifndef __gtk2_ardour_ardour_spinner_h__
#define __gtk2_ardour_ardour_spinner_h__

#include <gtkmm.h>

#include "ardour/automatable.h"
#include "ardour/automation_control.h"
#include "ardour_button.h"

class ArdourSpinner : public Gtk::Alignment
{
	public:
		ArdourSpinner (
				boost::shared_ptr<ARDOUR::AutomationControl>,
				Gtk::Adjustment* adj,
				boost::shared_ptr<ARDOUR::Automatable>);

		virtual ~ArdourSpinner ();

	protected:
		bool on_button_press_event (GdkEventButton*);
		bool on_button_release_event (GdkEventButton*);
		bool on_scroll_event (GdkEventScroll* ev);

		void controllable_changed ();
		PBD::ScopedConnection watch_connection;

	private:

		bool entry_focus_out (GdkEventFocus*);
		void entry_activated ();
		gint switch_to_button ();
		gint switch_to_spinner ();

		void ctrl_adjusted();
		void spin_adjusted();

		ArdourButton     _btn;
		Gtk::Adjustment* _ctrl_adj;
		Gtk::Adjustment  _spin_adj;
		Gtk::SpinButton  _spinner;
		bool             _switching;
		bool             _switch_on_release;
		bool             _ctrl_ignore;
		bool             _spin_ignore;

		boost::shared_ptr<ARDOUR::AutomationControl> _controllable;
		boost::shared_ptr<ARDOUR::Automatable>       _printer;

};

#endif /* __gtk2_ardour_ardour_menu_h__ */
