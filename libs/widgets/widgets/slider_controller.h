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

#ifndef _WIDGETS_SLIDER_CONTROLLER_H_
#define _WIDGETS_SLIDER_CONTROLLER_H_

#ifdef interface
#undef interface
#endif

#include <boost/shared_ptr.hpp>

#include <gtkmm/adjustment.h>
#include <gtkmm/spinbutton.h>

#include "widgets/ardour_fader.h"
#include "widgets/binding_proxy.h"
#include "widgets/visibility.h"

namespace PBD {
	class Controllable;
}

namespace ArdourWidgets {

class LIBWIDGETS_API SliderController : public ArdourWidgets::ArdourFader
{
public:
	SliderController (Gtk::Adjustment* adj, boost::shared_ptr<PBD::Controllable> mc, int orientation, int, int);

	virtual ~SliderController () {}

	Gtk::SpinButton& get_spin_button () { assert(_ctrl); return _spin; }
	void set_controllable (boost::shared_ptr<PBD::Controllable> c) { _binding_proxy.set_controllable (c); }

protected:
	bool on_button_press_event (GdkEventButton *ev);
	bool on_enter_notify_event (GdkEventCrossing* ev);
	bool on_leave_notify_event (GdkEventCrossing* ev);
	void ctrl_adjusted();
	void spin_adjusted();

	BindingProxy _binding_proxy;
	boost::shared_ptr<PBD::Controllable> _ctrl;
	Gtk::Adjustment *_ctrl_adj;
	Gtk::Adjustment _spin_adj;
	Gtk::SpinButton _spin;
	bool _ctrl_ignore;
	bool _spin_ignore;
};

class LIBWIDGETS_API VSliderController : public SliderController
{
public:
	VSliderController (Gtk::Adjustment *adj, boost::shared_ptr<PBD::Controllable> mc, int, int);
};

class LIBWIDGETS_API HSliderController : public SliderController
{
public:
	HSliderController (Gtk::Adjustment *adj, boost::shared_ptr<PBD::Controllable> mc, int, int);
};

}; /* namespace */

#endif
