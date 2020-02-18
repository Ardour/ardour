/*
 * Copyright (C) 1998 Paul Davis <paul@linuxaudiosystems.com>
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

#include <string>

#include "gtkmm2ext/gtk_ui.h"
#include "pbd/controllable.h"

#include "widgets/ardour_fader.h"
#include "widgets/slider_controller.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace ArdourWidgets;

SliderController::SliderController (Gtk::Adjustment *adj, boost::shared_ptr<PBD::Controllable> mc, int orientation, int fader_length, int fader_girth)
	: ArdourFader (*adj, orientation, fader_length, fader_girth)
	, _ctrl (mc)
	, _ctrl_adj (adj)
	, _spin_adj (0, 0, 1.0, .1, .01)
	, _spin (_spin_adj, 0, 2)
	, _ctrl_ignore (false)
	, _spin_ignore (false)
{
	if (mc) {
		_spin_adj.set_lower (mc->lower ());
		_spin_adj.set_upper (mc->upper ());
		_spin_adj.set_step_increment(_ctrl->interface_to_internal(adj->get_step_increment()) - mc->lower ());
		_spin_adj.set_page_increment(_ctrl->interface_to_internal(adj->get_page_increment()) - mc->lower ());

		adj->signal_value_changed().connect (sigc::mem_fun(*this, &SliderController::ctrl_adjusted));
		_spin_adj.signal_value_changed().connect (sigc::mem_fun(*this, &SliderController::spin_adjusted));

		_binding_proxy.set_controllable (mc);
	}

	_spin.set_name ("SliderControllerValue");
	_spin.set_numeric (true);
	_spin.set_snap_to_ticks (false);
}

bool
SliderController::on_button_press_event (GdkEventButton *ev)
{
	if (_binding_proxy.button_press_handler (ev)) {
		return true;
	}

	return ArdourFader::on_button_press_event (ev);
}

bool
SliderController::on_enter_notify_event (GdkEventCrossing* ev)
{
	boost::shared_ptr<PBD::Controllable> c (_binding_proxy.get_controllable ());
	if (c) {
		PBD::Controllable::GUIFocusChanged (boost::weak_ptr<PBD::Controllable> (c));
	}
	return ArdourFader::on_enter_notify_event (ev);
}

bool
SliderController::on_leave_notify_event (GdkEventCrossing* ev)
{
	if (_binding_proxy.get_controllable()) {
		PBD::Controllable::GUIFocusChanged (boost::weak_ptr<PBD::Controllable> ());
	}
	return ArdourFader::on_leave_notify_event (ev);
}

void
SliderController::ctrl_adjusted ()
{
	assert (_ctrl); // only used w/BarControlle
	if (_spin_ignore) return;
	_ctrl_ignore = true;
	// TODO consider using internal_to_user, too (amp, dB)
	// (also needs _spin_adj min/max range changed accordingly
	//  and dedicated support for log-scale, revert parts of ceff2e3a62f839)
	_spin_adj.set_value (_ctrl->interface_to_internal (_ctrl_adj->get_value()));
	_ctrl_ignore = false;
}

void
SliderController::spin_adjusted ()
{
	assert (_ctrl); // only used w/BarController
	if (_ctrl_ignore) return;
	_spin_ignore = true;
	// TODO consider using user_to_internal, as well
	_ctrl_adj->set_value(_ctrl->internal_to_interface (_spin_adj.get_value()));
	_spin_ignore = false;
}



VSliderController::VSliderController (Gtk::Adjustment *adj, boost::shared_ptr<PBD::Controllable> mc, int fader_length, int fader_girth)
	: SliderController (adj, mc, VERT, fader_length, fader_girth)
{
}

HSliderController::HSliderController (Gtk::Adjustment *adj, boost::shared_ptr<PBD::Controllable> mc, int fader_length, int fader_girth)
	: SliderController (adj, mc, HORIZ, fader_length, fader_girth)
{
}
