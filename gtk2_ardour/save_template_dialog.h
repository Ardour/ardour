/*
    Copyright (C) 2017 Paul Davis
    Author: Johannes Mueller

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

#ifndef __ardour_gtk_save_template_dialog_h__
#define __ardour_gtk_save_template_dialog_h__

#include <string>

#include <gtkmm/entry.h>
#include <gtkmm/textview.h>

#include "ardour_dialog.h"


class SaveTemplateDialog : public ArdourDialog
{
public:
	SaveTemplateDialog (const std::string& name, const std::string& description = "");

	std::string get_template_name () const;
	std::string get_description () const;

private:
	Gtk::Entry _name_entry;
	Gtk::TextView _description_editor;
};

#endif /* __ardour_gtk_save_template_dialog_h__ */
