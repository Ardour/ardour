/*
 * Copyright (C) 2021 Ben Loftis <ben@harrisonconsoles.com>
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

#include <ardour/session.h>

#include "pbd/unwind.h"

#include "varispeed_dialog.h"

#include "ardour_ui.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace Gtk;

VarispeedDialog::VarispeedDialog ()
	: ArdourDialog (_("Varispeed"), false)
	, _semitones_adjustment (0.0, -12.0, 12.0, 1.0, 4.0)
	, _cents_adjustment (0.0, -100.0, 100.0, 1.0, 10.0)
	, _percent_adjustment (100.0, 48.0, 200.0, 1.0, 10.0)
	, _semitones_spinner (_semitones_adjustment)
	, _cents_spinner (_cents_adjustment)
	, _percent_spinner (_percent_adjustment)
	, ignore_changes (false)
{
	Table* t = manage (new Table (5, 2));
	t->set_row_spacings (6);
	t->set_col_spacings (6);

	int    r = 0;
	Label* l = manage (new Label (_("Semitones:"), ALIGN_START, ALIGN_CENTER, false));
	t->attach (*l, 0, 1, r, r + 1, FILL, EXPAND, 0, 0);
	t->attach (_semitones_spinner, 1, 2, r, r + 1, FILL, EXPAND & FILL, 0, 0);
	++r;

	l = manage (new Label (_("Cents:"), ALIGN_START, ALIGN_CENTER, false));
	t->attach (*l, 0, 1, r, r + 1, FILL, EXPAND, 0, 0);
	t->attach (_cents_spinner, 1, 2, r, r + 1, FILL, EXPAND & FILL, 0, 0);
	++r;

	l = manage (new Label (_("Percentage:"), ALIGN_START, ALIGN_CENTER, false));
	t->attach (*l, 0, 1, r, r + 1, FILL, EXPAND, 0, 0);
	t->attach (_percent_spinner, 1, 2, r, r + 1, FILL, EXPAND & FILL, 0, 0);
	++r;

	get_vbox ()->set_spacing (6);
	get_vbox ()->pack_start (*t, false, false);

	_semitones_spinner.set_can_focus (false);
	_cents_spinner.set_can_focus (false);
	_percent_spinner.set_can_focus (false);

	_semitones_spinner.signal_changed ().connect (sigc::mem_fun (*this, &VarispeedDialog::apply_semitones));
	_cents_spinner.signal_changed ().connect (sigc::mem_fun (*this, &VarispeedDialog::apply_semitones));
	_percent_spinner.signal_changed ().connect (sigc::mem_fun (*this, &VarispeedDialog::apply_percentage));

	show_all_children ();
}

bool
VarispeedDialog::on_key_press_event (GdkEventKey* ev)
{
	Gtk::Window& main_window (ARDOUR_UI::instance ()->main_window ());
	return ARDOUR_UI_UTILS::relay_key_press (ev, &main_window);
}

void
VarispeedDialog::adj_semi (double delta)
{
	int cents = _semitones_spinner.get_value () * 100 + _cents_spinner.get_value ();
	cents += 100.0 * delta;

	_semitones_spinner.set_value (cents / 100);
	_cents_spinner.set_value (cents % 100);
}

void
VarispeedDialog::apply_semitones ()
{
	if (ignore_changes) {
		return;
	}

	int cents = _semitones_spinner.get_value () * 100 + _cents_spinner.get_value ();
	double speed = pow (2.0, ((double)cents / 1200.0));
	double const max_speed = ARDOUR::Config->get_max_transport_speed ();

	{
		PBD::Unwinder<bool> uw (ignore_changes, true);

		if (speed >= max_speed) {
			speed = max_speed;
			double semitones = 12. * (log (speed) / log(2.));
			int cents = 100 * fmod (semitones, 1.);
			if (cents > 50) {
				cents -= 100;
				semitones += 1;
			}
			_semitones_adjustment.set_value (floor (semitones));
			_cents_adjustment.set_value (cents);
		}

		_percent_adjustment.set_value (100. * speed);
	}

	if (_session && _session->default_play_speed () != speed) {
		_session->request_default_play_speed (speed);
	}
}

void
VarispeedDialog::apply_percentage ()
{
	if (ignore_changes) {
		return;
	}

	double speed = _percent_spinner.get_value() / 100.0;
	double max_speed = ARDOUR::Config->get_max_transport_speed ();

	{
		PBD::Unwinder<bool> uw (ignore_changes, true);

		if (speed >= max_speed) {
			speed = max_speed;
			_percent_adjustment.set_value (100. * speed);
		}

		double absspeed = ::abs (speed);
		double semitones = 12. * (log (absspeed) / log(2.));
		int cents = 100 * fmod (semitones, 1.);
		if (cents > 50) {
			cents -= 100;
			semitones += 1;
		}
		_semitones_adjustment.set_value (floor (semitones));
		_cents_adjustment.set_value (cents);
	}

	if (_session && _session->default_play_speed () != speed) {
		_session->request_default_play_speed (speed);
	}
}

void
VarispeedDialog::on_show ()
{
	apply_semitones ();
	ArdourDialog::on_show ();
	set_position (Gtk::WIN_POS_NONE); // remember position from now on
}

void
VarispeedDialog::on_hide ()
{
	if (_session && _session->default_play_speed () != 1.0) {
		_session->request_default_play_speed (1.0);
	}
	ArdourDialog::on_hide ();
}
