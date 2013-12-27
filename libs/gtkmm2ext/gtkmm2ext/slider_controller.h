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
        SliderController (Gtk::Adjustment* adj, int orientation, int, int);
	
        virtual ~SliderController () {}

	void set_value (float);

	Gtk::SpinButton& get_spin_button () { return spin; }
	
	bool on_button_press_event (GdkEventButton *ev);

	void set_controllable (boost::shared_ptr<PBD::Controllable> c) { binding_proxy.set_controllable (c); }

  protected:
	BindingProxy binding_proxy;
	Gtk::SpinButton     spin;
	Gtk::Frame          spin_frame;
	Gtk::HBox           spin_hbox;

	void init ();
};

class LIBGTKMM2EXT_API VSliderController : public SliderController
{
  public:
        VSliderController (Gtk::Adjustment *adj, int, int, bool with_numeric = true);
};

class LIBGTKMM2EXT_API HSliderController : public SliderController
{
  public:
	HSliderController (Gtk::Adjustment *adj, int, int, bool with_numeric = true);
};


}; /* namespace */

#endif // __gtkmm2ext_slider_controller_h__		
