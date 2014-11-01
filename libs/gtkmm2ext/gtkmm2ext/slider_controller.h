/*
    Copyright (C) 1998-2006 Paul Davis 
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

#ifndef __gtkmm2ext_slider_controller_h__
#define __gtkmm2ext_slider_controller_h__

#ifdef interface
#undef interface
#endif

#include <gtkmm.h>
#include <gtkmm2ext/popup.h>
#include <gtkmm2ext/pixfader.h>
#include <gtkmm2ext/binding_proxy.h>

#include <boost/shared_ptr.hpp>

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {
	class Pix;
}

namespace PBD {
	class Controllable;
}

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API SliderController : public Gtkmm2ext::PixFader
{
	public:
	SliderController (Gtk::Adjustment* adj, boost::shared_ptr<PBD::Controllable> mc, int orientation, int, int);

	virtual ~SliderController () {}

	Gtk::SpinButton& get_spin_button () { assert(_ctrl); return _spin; }
	void set_controllable (boost::shared_ptr<PBD::Controllable> c) { _binding_proxy.set_controllable (c); }

	protected:
	bool on_button_press_event (GdkEventButton *ev);
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

class LIBGTKMM2EXT_API VSliderController : public SliderController
{
	public:
	VSliderController (Gtk::Adjustment *adj, boost::shared_ptr<PBD::Controllable> mc, int, int);
};

class LIBGTKMM2EXT_API HSliderController : public SliderController
{
	public:
	HSliderController (Gtk::Adjustment *adj, boost::shared_ptr<PBD::Controllable> mc, int, int);
};


}; /* namespace */

#endif // __gtkmm2ext_slider_controller_h__		
