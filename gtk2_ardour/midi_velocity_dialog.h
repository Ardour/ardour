/*
    Copyright (C) 2012 Paul Davis

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

#ifndef __gtk2_ardour_midi_velocity_dialog_h__
#define __gtk2_ardour_midi_velocity_dialog_h__

#include <stdint.h>

#include <gtkmm/adjustment.h>
#include <gtkmm/spinbutton.h>

#include "ardour_dialog.h"


class MidiVelocityDialog : public ArdourDialog
{
  public:
	MidiVelocityDialog (uint8_t current_velocity = 0);
	uint8_t velocity() const;

  private:
    Gtk::Adjustment adjustment;
    Gtk::SpinButton spinner;
    Gtk::Label      label;
    Gtk::HBox       packer;
};

#endif /* __gtk2_ardour_midi_velocity_dialog_h__ */
