/*
 * Copyright (C) 2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_gtk_save_as_dialog_h__
#define __ardour_gtk_save_as_dialog_h__

#include <gtkmm/entry.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/filechooserbutton.h>

#include "ardour_dialog.h"

class SaveAsDialog : public ArdourDialog
{
public:
	SaveAsDialog ();

	std::string new_parent_folder () const;
	std::string new_name () const;

	bool switch_to () const;
	bool include_media () const;
	bool copy_media () const;
	bool copy_external () const;

	void clear_name ();
	void set_name (std::string);

private:
	Gtk::CheckButton switch_to_button;
	Gtk::CheckButton copy_media_button;
	Gtk::CheckButton copy_external_button;
	Gtk::CheckButton no_include_media_button;
	Gtk::FileChooserButton new_parent_folder_selector;
	Gtk::Entry new_name_entry;

	void name_entry_changed ();
	void no_include_toggled ();
};

#endif /* __ardour_gtk_tempo_dialog_h__ */
