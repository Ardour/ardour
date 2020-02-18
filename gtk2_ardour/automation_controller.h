/*
 * Copyright (C) 2007-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_gtk_automation_controller_h__
#define __ardour_gtk_automation_controller_h__

#ifdef YES
#undef YES
#endif
#ifdef NO
#undef NO
#endif

#include <boost/shared_ptr.hpp>

#include <gtkmm/alignment.h>

#include "pbd/signals.h"
#include "ardour/parameter_descriptor.h"

#include "widgets/barcontroller.h"

namespace ARDOUR {
	class Session;
	class AutomationList;
	class AutomationControl;
}

namespace Gtk {
	class Adjustment;
	class Widget;
}

class AutomationBarController : public ArdourWidgets::BarController {
public:
	AutomationBarController(boost::shared_ptr<ARDOUR::AutomationControl> ac,
	                        Gtk::Adjustment*                             adj);
	~AutomationBarController();
private:
	std::string get_label (double&);
	boost::shared_ptr<ARDOUR::AutomationControl> _controllable;
};

/** A BarController which displays the value and allows control of an AutomationControl */
class AutomationController : public Gtk::Alignment {
public:
	static boost::shared_ptr<AutomationController> create (
		const Evoral::Parameter&                     param,
		const ARDOUR::ParameterDescriptor&           desc,
		boost::shared_ptr<ARDOUR::AutomationControl> ac,
		bool                                         use_knob = false);

	~AutomationController();

	boost::shared_ptr<ARDOUR::AutomationControl> controllable() { return _controllable; }

	void disable_vertical_scroll();

	Gtk::Adjustment* adjustment() { return _adjustment; }
	Gtk::Widget*     widget()     { return _widget; }

	void display_effective_value ();
	void automation_state_changed ();
	void value_adjusted();

	void stop_updating ();

private:
	AutomationController (boost::shared_ptr<ARDOUR::AutomationControl> ac,
	                      Gtk::Adjustment*                             adj,
	                      bool                                         use_knob);

	void start_touch();
	void end_touch();
	bool button_press(GdkEventButton*);
	bool button_release(GdkEventButton*);

	void run_note_select_dialog();
	void set_ratio(double ratio);
	void set_freq_beats(double beats);
	bool on_button_release(GdkEventButton* ev);

	Gtk::Widget*                                 _widget;
	boost::shared_ptr<ARDOUR::AutomationControl> _controllable;
	Gtk::Adjustment*                             _adjustment;
	sigc::connection                             _screen_update_connection;
	PBD::ScopedConnectionList                    _changed_connections;
	bool                                         _ignore_change;
	bool                                         _grabbed;
};


#endif /* __ardour_gtk_automation_controller_h__ */
