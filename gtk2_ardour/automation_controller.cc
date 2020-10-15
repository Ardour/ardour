/*
 * Copyright (C) 2007-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2016 Nick Mainsbridge <mainsbridge@gmail.com>
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

#include <iomanip>
#include <cmath>

#include "pbd/compose.h"
#include "pbd/error.h"

#include "ardour/automatable.h"
#include "ardour/automation_control.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_knob.h"

#include "automation_controller.h"
#include "context_menu_helper.h"
#include "gui_thread.h"
#include "note_select_dialog.h"
#include "timers.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace ArdourWidgets;

using PBD::Controllable;

AutomationBarController::AutomationBarController (
		boost::shared_ptr<AutomationControl> ac,
		Adjustment*                          adj)
	: ArdourWidgets::BarController (*adj, ac)
	, _controllable (ac)
{
}

std::string
AutomationBarController::get_label (double& xpos)
{
	xpos = 0.5;
	return _controllable->get_user_string();
}

AutomationBarController::~AutomationBarController()
{
}

AutomationController::AutomationController(boost::shared_ptr<AutomationControl> ac,
                                           Adjustment*                          adj,
                                           bool                                 use_knob)
	: _widget(NULL)
	, _controllable(ac)
	, _adjustment(adj)
	, _ignore_change(false)
	, _grabbed(false)
{
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
		but->set_fallthrough_to_parent(true);
		but->set_controllable(ac);
		but->signal_button_press_event().connect(
			sigc::mem_fun(*this, &AutomationController::button_press));
		but->signal_button_release_event().connect(
			sigc::mem_fun(*this, &AutomationController::button_release));
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
		knob->StartGesture.connect(sigc::mem_fun(*this, &AutomationController::start_touch));
		knob->StopGesture.connect(sigc::mem_fun(*this, &AutomationController::end_touch));
	} else {
		AutomationBarController* bar = manage(new AutomationBarController(ac, adj));

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

	ac->Changed.connect (_changed_connections, invalidator (*this), boost::bind (&AutomationController::display_effective_value, this), gui_context());
	display_effective_value ();

	if (ac->alist ()) {
		ac->alist()->automation_state_changed.connect (_changed_connections, invalidator (*this), boost::bind (&AutomationController::automation_state_changed, this), gui_context());
		automation_state_changed ();
	}

	add(*_widget);
	show_all();
}

AutomationController::~AutomationController()
{
}

boost::shared_ptr<AutomationController>
AutomationController::create(const Evoral::Parameter&             param,
                             const ParameterDescriptor&           desc,
                             boost::shared_ptr<AutomationControl> ac,
                             bool use_knob)
{
	const double lo        = ac->internal_to_interface(desc.lower, true);
	const double normal    = ac->internal_to_interface(desc.normal, true);
	const double smallstep = fabs (ac->internal_to_interface(desc.lower + desc.smallstep, true) - lo);
	const double largestep = fabs (ac->internal_to_interface(desc.lower + desc.largestep, true) - lo);

	/* even though internal_to_interface() may not generate the full range
	 * 0..1, the interface range is 0..1 by definition,  so just hard code
	 * that.
	 */

	Gtk::Adjustment* adjustment = manage (new Gtk::Adjustment (normal, 0.0, 1.0, smallstep, largestep));

	assert (ac);
	assert(ac->parameter() == param);
	return boost::shared_ptr<AutomationController>(new AutomationController(ac, adjustment, use_knob));
}

void
AutomationController::automation_state_changed ()
{
	bool x = _controllable->alist()->automation_state() & Play;
	_widget->set_sensitive (!x);
}

void
AutomationController::display_effective_value ()
{
	double const interface_value = _controllable->internal_to_interface(_controllable->get_value(), true);

	if (_grabbed) {
		/* we cannot use _controllable->touching() here
		 * because that's only set in Write or Touch mode.
		 * Besides ctrl-surfaces may also set touching()
		 */
		return;
	}
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
		const double new_val = _controllable->interface_to_internal(_adjustment->get_value(), true);
		if (_controllable->user_double() != new_val) {
			_controllable->set_value (new_val, Controllable::NoGroup);
		}
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
	_grabbed = true;
	_controllable->start_touch (timepos_t (_controllable->session().transport_sample()));
}

void
AutomationController::end_touch ()
{
	_controllable->stop_touch (timepos_t (_controllable->session().transport_sample()));
	if (_grabbed) {
		_grabbed = false;
		display_effective_value ();
	}
}

bool
AutomationController::button_press (GdkEventButton*)
{
	ArdourButton* but = dynamic_cast<ArdourButton*>(_widget);
	if (but) {
		start_touch ();
		_controllable->set_value (but->get_active () ? 0.0 : 1.0, Controllable::UseGroup);
	}
	return false;
}

bool
AutomationController::button_release (GdkEventButton*)
{
	end_touch ();
	return true;
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
	const samplepos_t                  pos     = session.transport_sample();
	const ARDOUR::Tempo&               tempo   = session.tempo_map().tempo_at_sample (pos);
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
		Gtk::Menu* menu  = ARDOUR_UI_UTILS::shared_popup_menu ();
		MenuList&  items = menu->items();
		items.push_back(MenuElem(_("Select Note..."),
		                         sigc::mem_fun(*this, &AutomationController::run_note_select_dialog)));
		menu->popup(1, ev->time);
		return true;
	} else if (desc.unit == ARDOUR::ParameterDescriptor::HZ) {
		Gtk::Menu* menu  = ARDOUR_UI_UTILS::shared_popup_menu ();
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
			ArdourWidgets::ArdourFader::Tweaks(bar->tweaks() | ArdourWidgets::ArdourFader::NoVerticalScroll));
	}
}
