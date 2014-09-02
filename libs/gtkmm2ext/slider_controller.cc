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
#include <gtkmm2ext/pixfader.h>
#include <gtkmm2ext/slider_controller.h>

#include "i18n.h"

using namespace Gtkmm2ext;
using namespace PBD;

SliderController::SliderController (Gtk::Adjustment *adj, int orientation, int fader_length, int fader_girth)
	: PixFader (*adj, orientation, fader_length, fader_girth)
	, spin (*adj, 0, 2)
{			  
	spin.set_name ("SliderControllerValue");
	spin.set_numeric (true);
	spin.set_snap_to_ticks (false);
}

bool 
SliderController::on_button_press_event (GdkEventButton *ev) 
{
	if (binding_proxy.button_press_handler (ev)) {
		return true;
	}

	return PixFader::on_button_press_event (ev);
}

VSliderController::VSliderController (Gtk::Adjustment *adj, int fader_length, int fader_girth)
	: SliderController (adj, VERT, fader_length, fader_girth)
{
}

HSliderController::HSliderController (Gtk::Adjustment *adj, int fader_length, int fader_girth)
	: SliderController (adj, HORIZ, fader_length, fader_girth)
{
}
