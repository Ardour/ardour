/*
    Copyright (C) 2000-2007 Paul Davis 

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

#ifndef __gtk_ardour_add_midi_cc_track_dialog_h__
#define __gtk_ardour_add_midi_cc_track_dialog_h__

#include <string>

#include <gtkmm/dialog.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/spinbutton.h>
#include <ardour/types.h>
#include <ardour/data_type.h>
#include <evoral/Parameter.hpp>

class AddMidiCCTrackDialog : public Gtk::Dialog
{
  public:
	AddMidiCCTrackDialog ();

	Evoral::Parameter parameter ();

  private:
	Gtk::Adjustment _chan_adjustment;
	Gtk::SpinButton _chan_spinner;
	Gtk::Adjustment _cc_num_adjustment;
	Gtk::SpinButton _cc_num_spinner;
};

#endif /* __gtk_ardour_add_midi_cc_track_dialog_h__ */
