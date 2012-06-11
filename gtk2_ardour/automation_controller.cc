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

#include <iomanip>
#include <cmath>

#include "pbd/error.h"

#include "ardour/automatable.h"
#include "ardour/automation_control.h"
#include "ardour/session.h"

#include "ardour_ui.h"
#include "utils.h"
#include "automation_controller.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace Gtk;

AutomationController::AutomationController(boost::shared_ptr<Automatable> printer, boost::shared_ptr<AutomationControl> ac, Adjustment* adj)
	: BarController (*adj, ac)
	, _ignore_change(false)
        , _printer (printer)
	, _controllable(ac)
	, _adjustment(adj)
{
        assert (_printer);

	set_name (X_("PluginSlider")); // FIXME: get yer own name!
	set_style (BarController::LeftToRight);
	set_use_parent (true);

	StartGesture.connect (sigc::mem_fun(*this, &AutomationController::start_touch));
	StopGesture.connect (sigc::mem_fun(*this, &AutomationController::end_touch));

	_adjustment->signal_value_changed().connect (
			sigc::mem_fun(*this, &AutomationController::value_adjusted));

	_screen_update_connection = ARDOUR_UI::RapidScreenUpdate.connect (
			sigc::mem_fun (*this, &AutomationController::display_effective_value));

	ac->Changed.connect (_changed_connection, invalidator (*this), boost::bind (&AutomationController::value_changed, this), gui_context());
}

AutomationController::~AutomationController()
{
}

boost::shared_ptr<AutomationController>
AutomationController::create(
		boost::shared_ptr<Automatable> printer,
		const Evoral::Parameter& param,
		boost::shared_ptr<AutomationControl> ac)
{
	Gtk::Adjustment* adjustment = manage (
		new Gtk::Adjustment (
			ac->internal_to_interface (param.normal()),
			ac->internal_to_interface (param.min()),
			ac->internal_to_interface (param.max()),
			(param.max() - param.min()) / 100.0,
			(param.max() - param.min()) / 10.0
			)
		);

        assert (ac);
        assert(ac->parameter() == param);
	return boost::shared_ptr<AutomationController>(new AutomationController(printer, ac, adjustment));
}

std::string
AutomationController::get_label (double& xpos)
{
        xpos = 0.5;
        return _printer->value_as_string (_controllable);
}

void
AutomationController::display_effective_value()
{
	double const interface_value = _controllable->internal_to_interface (_controllable->get_value());

	if (_adjustment->get_value () != interface_value) {
		_ignore_change = true;
		_adjustment->set_value (interface_value);
		_ignore_change = false;
	}
}

void
AutomationController::value_adjusted ()
{
	if (!_ignore_change) {
		_controllable->set_value (_controllable->interface_to_internal (_adjustment->get_value()));
	}
}

void
AutomationController::start_touch()
{
	_controllable->start_touch (_controllable->session().transport_frame());
}

void
AutomationController::end_touch ()
{
	if (_controllable->automation_state() == Touch) {

		bool mark = false;
		double when = 0;

		if (_controllable->session().transport_rolling()) {
			mark = true;
			when = _controllable->session().transport_frame();
		}

		_controllable->stop_touch (mark, when);
	}
}

void
AutomationController::value_changed ()
{
	Gtkmm2ext::UI::instance()->call_slot (invalidator (*this), boost::bind (&AutomationController::display_effective_value, this));
}

/** Stop updating our value from our controllable */
void
AutomationController::stop_updating ()
{
	_screen_update_connection.disconnect ();
}
