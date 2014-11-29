/*
    Copyright (C) 2007 Paul Davis
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

#ifndef __ardour_gtk_automation_controller_h__
#define __ardour_gtk_automation_controller_h__

#ifdef YES
#undef YES
#endif
#ifdef NO
#undef NO
#endif

#include <boost/shared_ptr.hpp>
#include <gtkmm.h>

#include "gtkmm2ext/barcontroller.h"
#include "pbd/signals.h"
#include "evoral/Parameter.hpp"

namespace ARDOUR {
	class Session;
	class AutomationList;
	class AutomationControl;
	class Automatable;
}

class AutomationBarController : public Gtkmm2ext::BarController {
public:
	AutomationBarController(boost::shared_ptr<ARDOUR::Automatable>       printer,
	                        boost::shared_ptr<ARDOUR::AutomationControl> ac,
	                        Gtk::Adjustment*                             adj);
	~AutomationBarController();
private:
	std::string get_label (double&);
	boost::shared_ptr<ARDOUR::Automatable>       _printer;
	boost::shared_ptr<ARDOUR::AutomationControl> _controllable;
};

/** A BarController which displays the value and allows control of an AutomationControl */
class AutomationController : public Gtk::Alignment {
public:
	static boost::shared_ptr<AutomationController> create(
		boost::shared_ptr<ARDOUR::Automatable>       parent,
		const Evoral::Parameter&                     param,
		const ARDOUR::ParameterDescriptor&           desc,
		boost::shared_ptr<ARDOUR::AutomationControl> ac);

	~AutomationController();

	boost::shared_ptr<ARDOUR::AutomationControl> controllable() { return _controllable; }

	void disable_vertical_scroll();

	Gtk::Adjustment* adjustment() { return _adjustment; }
	Gtk::Widget*     widget()     { return _widget; }

	void display_effective_value();
	void value_adjusted();

	void stop_updating ();

	sigc::signal<void> StartGesture;
	sigc::signal<void> StopGesture;

private:
	AutomationController (boost::shared_ptr<ARDOUR::Automatable>       printer,
	                      boost::shared_ptr<ARDOUR::AutomationControl> ac,
	                      Gtk::Adjustment*                             adj);

	void start_touch();
	void end_touch();
	void toggled();

	void run_note_select_dialog();
	void set_ratio(double ratio);
	void set_freq_beats(double beats);
	bool on_button_release(GdkEventButton* ev);

	void value_changed();

	Gtk::Widget*                                 _widget;
	boost::shared_ptr<ARDOUR::Automatable>       _printer;
	boost::shared_ptr<ARDOUR::AutomationControl> _controllable;
	Gtk::Adjustment*                             _adjustment;
	sigc::connection                             _screen_update_connection;
	PBD::ScopedConnection                        _changed_connection;
	bool                                         _ignore_change;
};


#endif /* __ardour_gtk_automation_controller_h__ */
