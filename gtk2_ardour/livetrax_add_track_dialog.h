/*
 * Copyright (C) 2024 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __gtk_ardour_livetrax_add_track_dialog_h__
#define __gtk_ardour_livetrax_add_track_dialog_h__

#include <gtkmm/dialog.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/button.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/adjustment.h>

#include "ardour_dialog.h"

class LiveTraxAddTrackDialog : public ArdourDialog
{
public:
	LiveTraxAddTrackDialog ();
	~LiveTraxAddTrackDialog ();

	int num_tracks() const;
	bool stereo () const;

private:
	Gtk::Adjustment track_count;
	Gtk::SpinButton track_count_spinner;
	Gtk::RadioButtonGroup channel_button_group;
	Gtk::RadioButton mono_button;
	Gtk::RadioButton stereo_button;
};

#endif // __gtk_ardour_livetrax_add_track_dialog_h__ */
