/*
    Copyright (C) 2007 Paul Davis 
    Author: Dave Robillard

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

#include <boost/shared_ptr.hpp>
#include <gtkmm.h>
#include <gtkmm2ext/barcontroller.h>

namespace ARDOUR {
	class Session;
	class AutomationList;
	class AutomationControl;
	class Automatable;
}


class AutomationController : public Gtkmm2ext::BarController {
public:
	static boost::shared_ptr<AutomationController> create(
			boost::shared_ptr<ARDOUR::Automatable> parent,
			const Evoral::Parameter& param,
			boost::shared_ptr<ARDOUR::AutomationControl> ac);

	~AutomationController();
	
	boost::shared_ptr<ARDOUR::AutomationControl> controllable() { return _controllable; }

	Gtk::Adjustment* adjustment() { return _adjustment; }
	
	void display_effective_value();
	void value_adjusted();

private:
	AutomationController (boost::shared_ptr<ARDOUR::AutomationControl> ac, Gtk::Adjustment* adj);
	std::string get_label (int&);
	
	void start_touch();
	void end_touch();

	void value_changed();
	void automation_state_changed();

	bool                                         _ignore_change;
	boost::shared_ptr<ARDOUR::AutomationControl> _controllable;
	Gtk::Adjustment*                             _adjustment;
	sigc::connection                             _screen_update_connection;
};


#endif /* __ardour_gtk_automation_controller_h__ */
