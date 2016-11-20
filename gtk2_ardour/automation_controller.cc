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

#include "pbd/compose.h"
#include "pbd/error.h"

#include "ardour/automatable.h"
#include "ardour/automation_control.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "ardour_button.h"
#include "ardour_knob.h"
#include "automation_controller.h"
#include "gui_thread.h"
#include "note_select_dialog.h"
#include "timers.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Gtk;

using PBD::Controllable;

AutomationBarController::AutomationBarController (
		boost::shared_ptr<Automatable>       printer,
		boost::shared_ptr<AutomationControl> ac,
		Adjustment*                          adj)
	: Gtkmm2ext::BarController(*adj, ac)
	, _printer(printer)
	, _controllable(ac)
{
}

std::string
AutomationBarController::get_label (double& xpos)
{
        xpos = 0.5;
        return _printer->value_as_string (_controllable);
}

AutomationBarController::~AutomationBarController()
{
}

AutomationController::AutomationController(boost::shared_ptr<Automatable>       printer,
                                           boost::shared_ptr<AutomationControl> ac,
                                           Adjustment*                          adj,
                                           bool                                 use_knob)
	: _widget(NULL)
	, _printer (printer)
	, _controllable(ac)
	, _adjustment(adj)
	, _ignore_change(false)
{
	assert (_printer);

	if (ac->toggled()) {
		ArdourButton* but = manage(new ArdourButton());

		// Apply styles for special types
		if (ac->parameter().type() == MuteAutomation) {
			but->set_name("mute button");
		} else if (ac->parameter().type() == SoloAutomation) {
			but->set_name("solo button");
		} else {
			but->set_name("generic button");
		}
		but->set_controllable(ac);
		but->signal_clicked.connect(
			sigc::mem_fun(*this, &AutomationController::toggled));
		const bool active = _adjustment->get_value() >= 0.5;
		if (but->get_active() != active) {
			but->set_active(active);
		}
		_widget = but;
	} else if (use_knob) {
		ArdourKnob* knob = manage (new ArdourKnob (ArdourKnob::default_elements, ArdourKnob::Detent));
		knob->set_controllable (ac);
		knob->set_name("processor control knob");
		_widget = knob;
	} else {
		AutomationBarController* bar = manage(new AutomationBarController(_printer, ac, adj));

		bar->set_name(X_("ProcessorControlSlider"));
		bar->StartGesture.connect(
			sigc::mem_fun(*this, &AutomationController::start_touch));
		bar->StopGesture.connect(
			sigc::mem_fun(*this, &AutomationController::end_touch));
		bar->signal_button_release_event().connect(
			sigc::mem_fun(*this, &AutomationController::on_button_release));

		_widget = bar;
	}

	_adjustment->signal_value_changed().connect(
		sigc::mem_fun(*this, &AutomationController::value_adjusted));

	_screen_update_connection = Timers::rapid_connect (
			sigc::mem_fun (*this, &AutomationController::display_effective_value));

	ac->Changed.connect (_changed_connection, invalidator (*this), boost::bind (&AutomationController::value_changed, this), gui_context());

	add(*_widget);
	show_all();
}

AutomationController::~AutomationController()
{
}

boost::shared_ptr<AutomationController>
AutomationController::create(boost::shared_ptr<Automatable>       printer,
                             const Evoral::Parameter&             param,
                             const ParameterDescriptor&           desc,
                             boost::shared_ptr<AutomationControl> ac,
                             bool use_knob)
{
	const double lo        = ac->internal_to_interface(desc.lower);
	const double up        = ac->internal_to_interface(desc.upper);
	const double normal    = ac->internal_to_interface(desc.normal);
	const double smallstep = ac->internal_to_interface(desc.lower + desc.smallstep);
	const double largestep = ac->internal_to_interface(desc.lower + desc.largestep);

	Gtk::Adjustment* adjustment = manage (
		new Gtk::Adjustment (normal, lo, up, smallstep, largestep));

	assert (ac);
	assert(ac->parameter() == param);
	return boost::shared_ptr<AutomationController>(new AutomationController(printer, ac, adjustment, use_knob));
}

void
AutomationController::display_effective_value()
{
	double const interface_value = _controllable->internal_to_interface(_controllable->get_value());

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
		_controllable->set_value (_controllable->interface_to_internal(_adjustment->get_value()), Controllable::NoGroup);
	}

	/* A bar controller will automatically follow the adjustment, but for a
	   button we have to do it manually. */
	ArdourButton* but = dynamic_cast<ArdourButton*>(_widget);
	if (but) {
		const bool active = _adjustment->get_value() >= 0.5;
		if (but->get_active() != active) {
			but->set_active(active);
		}
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
	} else {
		_controllable->stop_touch (false, _controllable->session().transport_frame());
	}
}

void
AutomationController::toggled ()
{
	ArdourButton* but = dynamic_cast<ArdourButton*>(_widget);
	const AutoState as = _controllable->automation_state ();
	const double where = _controllable->session ().audible_frame ();
	const bool to_list = _controllable->list () && _controllable->session().transport_rolling () && (as == Touch || as == Write);

	if (but) {
		if (to_list) {
			if (as == Touch && _controllable->list ()->in_new_write_pass ()) {
				_controllable->alist ()->start_write_pass (where);
			}
			_controllable->list ()->set_in_write_pass (true, false, where);
		}
		/* set to opposite value.*/
		_controllable->set_double (but->get_active () ? 0.0 : 1.0, where, to_list);

		const bool active = _controllable->get_double (to_list, where) >= 0.5;
		if (active && !but->get_active ()) {
			_adjustment->set_value (1.0);
			but->set_active (true);
		} else if (!active && but->get_active ()) {
			_adjustment->set_value (0.0);
			but->set_active (false);
		}
	}
}

static double
midi_note_to_hz(int note)
{
	const double tuning = 440.0;
	return tuning * pow(2, (note - 69.0) / 12.0);
}

static double
clamp(double val, double min, double max)
{
	if (val < min) {
		return min;
	} else if (val > max) {
		return max;
	}
	return val;
}

void
AutomationController::run_note_select_dialog()
{
	const ARDOUR::ParameterDescriptor& desc   = _controllable->desc();
	NoteSelectDialog*                  dialog = new NoteSelectDialog();
	if (dialog->run() == Gtk::RESPONSE_ACCEPT) {
		const double value = ((_controllable->desc().unit == ARDOUR::ParameterDescriptor::HZ)
		                      ? midi_note_to_hz(dialog->note_number())
		                      : dialog->note_number());
		_controllable->set_value(clamp(value, desc.lower, desc.upper), Controllable::NoGroup);
	}
	delete dialog;
}

void
AutomationController::set_freq_beats(double beats)
{
	const ARDOUR::ParameterDescriptor& desc    = _controllable->desc();
	const ARDOUR::Session&             session = _controllable->session();
	const framepos_t                   pos     = session.transport_frame();
	const ARDOUR::Tempo&               tempo   = session.tempo_map().tempo_at_frame (pos);
	const double                       bpm     = tempo.note_types_per_minute();
	const double                       bps     = bpm / 60.0;
	const double                       freq    = bps / beats;
	_controllable->set_value(clamp(freq, desc.lower, desc.upper), Controllable::NoGroup);
}

void
AutomationController::set_ratio(double ratio)
{
	const ARDOUR::ParameterDescriptor& desc  = _controllable->desc();
	const double                       value = _controllable->get_value() * ratio;
	_controllable->set_value(clamp(value, desc.lower, desc.upper), Controllable::NoGroup);
}

bool
AutomationController::on_button_release(GdkEventButton* ev)
{
	using namespace Gtk::Menu_Helpers;

	if (ev->button != 3) {
		return false;
	}

	const ARDOUR::ParameterDescriptor& desc = _controllable->desc();
	if (desc.unit == ARDOUR::ParameterDescriptor::MIDI_NOTE) {
		Gtk::Menu* menu  = manage(new Menu());
		MenuList&  items = menu->items();
		items.push_back(MenuElem(_("Select Note..."),
		                         sigc::mem_fun(*this, &AutomationController::run_note_select_dialog)));
		menu->popup(1, ev->time);
		return true;
	} else if (desc.unit == ARDOUR::ParameterDescriptor::HZ) {
		Gtk::Menu* menu  = manage(new Menu());
		MenuList&  items = menu->items();
		items.push_back(MenuElem(_("Halve"),
		                         sigc::bind(sigc::mem_fun(*this, &AutomationController::set_ratio),
		                                    0.5)));
		items.push_back(MenuElem(_("Double"),
		                         sigc::bind(sigc::mem_fun(*this, &AutomationController::set_ratio),
		                                    2.0)));
		const bool is_audible = desc.upper > 40.0;
		const bool is_low     = desc.lower < 1.0;
		if (is_audible) {
			items.push_back(MenuElem(_("Select Note..."),
			                         sigc::mem_fun(*this, &AutomationController::run_note_select_dialog)));
		}
		if (is_low) {
			for (int beats = 1; beats <= 16; ++beats) {
				items.push_back(MenuElem (string_compose(P_("Set to %1 beat", "Set to %1 beats", beats), beats),
				                         sigc::bind(sigc::mem_fun(*this, &AutomationController::set_freq_beats),
				                                    (double)beats)));
			}
		}
		menu->popup(1, ev->time);
		return true;
	}

	return false;
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

void
AutomationController::disable_vertical_scroll ()
{
	AutomationBarController* bar = dynamic_cast<AutomationBarController*>(_widget);
	if (bar) {
		bar->set_tweaks (
			Gtkmm2ext::PixFader::Tweaks(bar->tweaks() |
			                            Gtkmm2ext::PixFader::NoVerticalScroll));
	}
}
