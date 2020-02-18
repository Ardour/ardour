/*
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

#ifndef _WIDGETS_ARDOUR_SPINNER_H_
#define _WIDGETS_ARDOUR_SPINNER_H_

#include <boost/algorithm/string.hpp>

#include <gtkmm/adjustment.h>
#include <gtkmm/alignment.h>
#include <gtkmm/spinbutton.h>

#include "pbd/controllable.h"

#include "widgets/ardour_button.h"
#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API ArdourSpinner : public Gtk::Alignment
{
	public:
		ArdourSpinner (boost::shared_ptr<PBD::Controllable>, Gtk::Adjustment* adj);

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

		ArdourWidgets::ArdourButton _btn;
		Gtk::Adjustment* _ctrl_adj;
		Gtk::Adjustment  _spin_adj;
		Gtk::SpinButton  _spinner;
		bool             _switching;
		bool             _switch_on_release;
		bool             _ctrl_ignore;
		bool             _spin_ignore;

		boost::shared_ptr<PBD::Controllable> _controllable;

};

} /* end namespace */

#endif
