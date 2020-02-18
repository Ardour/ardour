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

#ifndef _WIDGETS_BAR_CONTROLLER_H_
#define _WIDGETS_BAR_CONTROLLER_H_

#include <gtkmm/alignment.h>
#include <cairo.h>

#include "widgets/binding_proxy.h"
#include "widgets/slider_controller.h"
#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API BarController : public Gtk::Alignment
{
public:
	BarController (Gtk::Adjustment& adj, boost::shared_ptr<PBD::Controllable>);

	virtual ~BarController ();

	void set_sensitive (bool yn);
	void set_digits (guint d) { _slider.get_spin_button().set_digits (d); }

	ArdourFader::Tweaks tweaks() const { return _slider.tweaks (); }
	void set_tweaks (ArdourFader::Tweaks t) { _slider.set_tweaks (t);}

	sigc::signal<void> StartGesture;
	sigc::signal<void> StopGesture;

	/* export this to allow direct connection to button events */
	Gtk::Widget& event_widget() { return _slider; }

	/** Emitted when the adjustment spinner is activated or deactivated;
	 *  the parameter is true on activation, false on deactivation.
	 */
	sigc::signal<void, bool> SpinnerActive;

protected:
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	void on_style_changed (const Glib::RefPtr<Gtk::Style>&);

	virtual std::string get_label (double& /*x*/) {
		return "";
	}

private:
	HSliderController _slider;
	bool entry_focus_out (GdkEventFocus*);
	void entry_activated ();
	void before_expose ();

	gint switch_to_bar ();
	gint switch_to_spinner ();

	bool _switching;
	bool _switch_on_release;


	void passtrhu_gesture_start() { StartGesture (); }
	void passtrhu_gesture_stop()  { StopGesture (); }
};


}; /* namespace */

#endif // __gtkmm2ext_bar_controller_h__
