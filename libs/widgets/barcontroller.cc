/*
 * Copyright (C) 2004 Paul Davis <paul@linuxaudiosystems.com>
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
#include <sstream>
#include <climits>
#include <cstdio>
#include <cmath>
#include <algorithm>

#include <pbd/controllable.h>

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/cairo_widget.h"

#include "widgets/barcontroller.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ArdourWidgets;

BarController::BarController (Gtk::Adjustment& adj,
		boost::shared_ptr<PBD::Controllable> mc)
	: _slider (&adj, mc, 60, 16)
	, _switching (false)
	, _switch_on_release (false)
{

	add_events (Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);
	set (.5, .5, 1.0, 1.0);
	set_border_width (0);
	_slider.set_tweaks (ArdourFader::NoShowUnityLine);

	_slider.StartGesture.connect (sigc::mem_fun(*this, &BarController::passtrhu_gesture_start));
	_slider.StopGesture.connect (sigc::mem_fun(*this, &BarController::passtrhu_gesture_stop));
	_slider.OnExpose.connect (sigc::mem_fun(*this, &BarController::before_expose));
	_slider.set_name (get_name());

	Gtk::SpinButton& spinner = _slider.get_spin_button();
	spinner.signal_activate().connect (mem_fun (*this, &BarController::entry_activated));
	spinner.signal_focus_out_event().connect (mem_fun (*this, &BarController::entry_focus_out));
	if (mc && mc->is_gain_like ()) {
		spinner.set_digits (2); // 0.01 dB
	} else {
		spinner.set_digits (4);
	}
	spinner.set_numeric (true);
	spinner.set_name ("BarControlSpinner");
	add (_slider);
	show_all ();
}

BarController::~BarController ()
{
}

bool
BarController::on_button_press_event (GdkEventButton* ev)
{
	if (get_child() != &_slider) {
		return false;
	}
	if (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS) {
		_switch_on_release = true;
		return true;
	} else {
		_switch_on_release = false;
	}
	return false;
}

bool
BarController::on_button_release_event (GdkEventButton* ev)
{
	if (get_child() != &_slider) {
		return false;
	}
	if (ev->button == 1 && _switch_on_release) {
		Glib::signal_idle().connect (mem_fun (*this, &BarController::switch_to_spinner));
		return true;
	}
	return false;
}

void
BarController::on_style_changed (const Glib::RefPtr<Gtk::Style>&)
{
	_slider.set_name (get_name());
}

gint
BarController::switch_to_bar ()
{
	if (_switching || get_child() == &_slider) {
		return FALSE;
	}
	_switching = true;
	remove ();
	add (_slider);
	_slider.show ();
	_slider.queue_draw ();
	_switching = false;
	SpinnerActive (false); /* EMIT SIGNAL */
	return FALSE;
}

gint
BarController::switch_to_spinner ()
{
	if (_switching || get_child() != &_slider) {
		return FALSE;
	}

	_switching = true;
	Gtk::SpinButton& spinner = _slider.get_spin_button();
	if (spinner.get_parent()) {
		spinner.get_parent()->remove(spinner);
	}
	remove ();
	add (spinner);
	spinner.show ();
	spinner.select_region (0, spinner.get_text_length());
	spinner.grab_focus ();
	_switching = false;
	SpinnerActive (true); /* EMIT SIGNAL */
	return FALSE;
}

void
BarController::entry_activated ()
{
	switch_to_bar ();
}

bool
BarController::entry_focus_out (GdkEventFocus* /*ev*/)
{
	entry_activated ();
	return true;
}

void
BarController::before_expose ()
{
	double xpos = -1;
	_slider.set_text (get_label (xpos), false, false);
}

void
BarController::set_sensitive (bool yn)
{
	Alignment::set_sensitive (yn);
	_slider.set_sensitive (yn);
}
