/*
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2017 Johannes Mueller <github@johannes-mueller.org>
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

#include <gtkmm/box.h>
#include <gtkmm/frame.h>
#include <gtkmm/label.h>
#include <gtkmm/stock.h>

#include "ardour/session.h"

#include "save_template_dialog.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;

SaveTemplateDialog::SaveTemplateDialog (const std::string& name, const std::string& desc)
	: ArdourDialog (_("Save as template"))
{
	_name_entry.get_buffer()->set_text (name);
	_description_editor.get_buffer()->set_text (desc);
	_description_editor.set_wrap_mode (Gtk::WRAP_WORD);
	_description_editor.set_size_request(400, 300);

	HBox* hb = manage (new HBox);
	hb->set_spacing (8);
	Label* lb = manage (new Label(_("Template name:")));
	hb->pack_start (*lb, false, true);
	hb->pack_start (_name_entry, true, true);

	Frame* fd = manage (new Frame(_("Description:")));
	fd->add (_description_editor);

	get_vbox()->set_spacing (8);
	get_vbox()->pack_start (*fd);
	get_vbox()->pack_start (*hb);

	add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_ACCEPT);

	show_all_children ();
}


std::string
SaveTemplateDialog::get_template_name () const
{
	return _name_entry.get_buffer()->get_text();
}

std::string
SaveTemplateDialog::get_description () const
{
	std::string desc_txt = _description_editor.get_buffer()->get_text ();
	std::string::reverse_iterator wss = desc_txt.rbegin();
	while (wss != desc_txt.rend() && isspace (*wss)) {
		desc_txt.erase (--(wss++).base());
	}

	return desc_txt;
}
