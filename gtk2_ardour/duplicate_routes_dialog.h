/*
    Copyright (C) 2015 Paul Davis

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

#ifndef __gtk_ardour_duplicate_route_dialog_h__
#define __gtk_ardour_duplicate_route_dialog_h__

#include <gtkmm/entry.h>
#include <gtkmm/box.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/spinbutton.h>

#include "ardour/types.h"

#include "ardour_dialog.h"

class Editor;

class DuplicateRouteDialog : public ArdourDialog
{
  public:
	DuplicateRouteDialog ();
	~DuplicateRouteDialog ();

	int restart ();

  private:
	Gtk::Entry name_template_entry;
	Gtk::VBox playlist_button_box;
	Gtk::RadioButtonGroup playlist_button_group;
	Gtk::RadioButton copy_playlists_button;
	Gtk::RadioButton new_playlists_button;
	Gtk::RadioButton share_playlists_button;
	Gtk::Adjustment count_adjustment;
	Gtk::SpinButton count_spinner;
	Gtk::HBox count_box;
	Gtk::Label count_label;

	void on_response (int);

	uint32_t count() const;
	ARDOUR::PlaylistDisposition playlist_disposition() const;
};

#endif /* __gtk_ardour_duplicate_route_dialog_h__ */
