/*
    Copyright (C) 2004 Paul Davis
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

#include <string>
#include <sstream>
#include <climits>
#include <cstdio>
#include <cmath>
#include <algorithm>

#include <pbd/controllable.h>
#include <pbd/locale_guard.h>

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/barcontroller.h"
#include "gtkmm2ext/cairo_widget.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

BarController::BarController (Gtk::Adjustment& adj,
		boost::shared_ptr<PBD::Controllable> mc)
	: _slider (&adj, 60, 16)
	, _logarithmic (false)
	, _switching (false)
	, _switch_on_release (false)
{

	add_events (Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);
	set (.5, .5, 1.0, 1.0);
	set_border_width (0);
	_slider.set_controllable (mc);

	_slider.StartGesture.connect (sigc::mem_fun(*this, &BarController::passtrhu_gesture_start));
	_slider.StopGesture.connect (sigc::mem_fun(*this, &BarController::passtrhu_gesture_stop));
	_slider.OnExpose.connect (sigc::mem_fun(*this, &BarController::before_expose));

	Gtk::SpinButton& spinner = _slider.get_spin_button();
	spinner.signal_activate().connect (mem_fun (*this, &BarController::entry_activated));
	spinner.signal_focus_out_event().connect (mem_fun (*this, &BarController::entry_focus_out));
	spinner.signal_input().connect (mem_fun (*this, &BarController::entry_input));
	spinner.signal_output().connect (mem_fun (*this, &BarController::entry_output));
	spinner.set_digits (9);
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
	_slider.set_text (get_label (xpos), false);
}

void
BarController::set_sensitive (bool yn)
{
	Alignment::set_sensitive (yn);
	_slider.set_sensitive (yn);
}

/* 
    This is called when we need to update the adjustment with the value
    from the spinner's text entry.
    
    We need to use Gtk::Entry::get_text to avoid recursive nastiness :)
    
    If we're not in logarithmic mode we can return false to use the 
    default conversion.
    
    In theory we should check for conversion errors but set numeric
    mode to true on the spinner prevents invalid input.
*/
int
BarController::entry_input (double* new_value)
{
	if (!_logarithmic) {
		return false;
	}

	// extract a double from the string and take its log
	Gtk::SpinButton& spinner = _slider.get_spin_button();
	Entry *entry = dynamic_cast<Entry *>(&spinner);
	double value;

	{
		// Switch to user's preferred locale so that
		// if they use different LC_NUMERIC conventions,
		// we will honor them.

		PBD::LocaleGuard lg ("");
		sscanf (entry->get_text().c_str(), "%lf", &value);
	}

	*new_value = log(value);

	return true;
}

/* 
    This is called when we need to update the spinner's text entry 
    with the value of the adjustment.
    
    We need to use Gtk::Entry::set_text to avoid recursive nastiness :)
    
    If we're not in logarithmic mode we can return false to use the 
    default conversion.
*/
bool
BarController::entry_output ()
{
	if (!_logarithmic) {
		return false;
	}

	char buf[128];
	Gtk::SpinButton& spinner = _slider.get_spin_button();

	// generate the exponential and turn it into a string
	// convert to correct locale. 
	
	stringstream stream;
	string str;

	{
		// Switch to user's preferred locale so that
		// if they use different LC_NUMERIC conventions,
		// we will honor them.
		
		PBD::LocaleGuard lg ("");
		snprintf (buf, sizeof (buf), "%g", exp (spinner.get_adjustment()->get_value()));
	}

	Entry *entry = dynamic_cast<Entry *>(&spinner);
	entry->set_text(buf);
	
	return true;
}
