/*
    Copyright (C) 1998-99 Paul Davis
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

    $Id$
*/

#include <string>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/pixscroller.h>
#include <gtkmm2ext/slider_controller.h>

#include "i18n.h"

using namespace Gtkmm2ext;
using namespace PBD;

SliderController::SliderController (Glib::RefPtr<Gdk::Pixbuf> slide,
				    Glib::RefPtr<Gdk::Pixbuf> rail,
				    Gtk::Adjustment *adj,
				    Controllable& c,
				    bool with_numeric)

	: PixScroller (*adj, slide, rail),
	  binding_proxy (c),
	  spin (*adj, 0, 2)
{			  
	spin.set_name ("SliderControllerValue");
	spin.set_size_request (70,-1); // should be based on font size somehow
	spin.set_numeric (true);
	spin.set_snap_to_ticks (false);
}

void
SliderController::set_value (float v)
{
	adj.set_value (v);
}

VSliderController::VSliderController (Glib::RefPtr<Gdk::Pixbuf> slide,
				      Glib::RefPtr<Gdk::Pixbuf> rail,
				      Gtk::Adjustment *adj,
				      Controllable& control,
				      bool with_numeric)

	: SliderController (slide, rail, adj, control, with_numeric)
{
	if (with_numeric) {
		spin_frame.add (spin);
		spin_frame.set_shadow_type (Gtk::SHADOW_IN);
		spin_frame.set_name ("BaseFrame");
		spin_hbox.pack_start (spin_frame, false, true);
		// pack_start (spin_hbox, false, false);
	}
}

HSliderController::HSliderController (Glib::RefPtr<Gdk::Pixbuf> slide,
				      Glib::RefPtr<Gdk::Pixbuf> rail,
				      Gtk::Adjustment *adj,
				      Controllable& control,
				      bool with_numeric)
	
	: SliderController (slide, rail, adj, control, with_numeric)
{
	if (with_numeric) {
		spin_frame.add (spin);
		//spin_frame.set_shadow_type (Gtk::SHADOW_IN);
		spin_frame.set_name ("BaseFrame");
		spin_hbox.pack_start (spin_frame, false, true);
		// pack_start (spin_hbox, false, false);
	}
}
