/*
    Copyright (C) 2009 Paul Davis 

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

#include <gtkmm/spinbutton.h>
#include "ardour_dialog.h"

/// Dialog box to set options for the `strip silence' filter
class StripSilenceDialog : public ArdourDialog
{
public:
	StripSilenceDialog ();

	double threshold () const {
		return _threshold.get_value ();
	}

	nframes_t minimum_length () const {
		return _minimum_length.get_value_as_int ();
	}

	nframes_t fade_length () const {
		return _fade_length.get_value_as_int ();
	}
	
private:
	Gtk::SpinButton _threshold;
	Gtk::SpinButton _minimum_length;
	Gtk::SpinButton _fade_length;
};
