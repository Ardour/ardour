/*
 * Copyright (C) 2015-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __gtk_ardour_duplicate_route_dialog_h__
#define __gtk_ardour_duplicate_route_dialog_h__

#include <gtkmm/entry.h>
#include <gtkmm/box.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/spinbutton.h>

#include "ardour/types.h"

#include "ardour_dialog.h"
#include "route_dialogs.h"

namespace ARDOUR {
class Session;
}

class Editor;

class DuplicateRouteDialog : public ArdourDialog
{
public:
	DuplicateRouteDialog ();

	int restart (ARDOUR::Session*);

private:
	Gtk::Entry name_template_entry;
	Gtk::VBox playlist_button_box;
	Gtk::Label playlist_option_label;
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
	RouteDialogs::InsertAt insert_at();
	ARDOUR::PlaylistDisposition playlist_disposition() const;

private:
	Gtk::ComboBoxText insert_at_combo;
};

#endif /* __gtk_ardour_duplicate_route_dialog_h__ */
