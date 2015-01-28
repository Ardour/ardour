/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#include "waves_edit_dialog.h"
WavesEditDialog::WavesEditDialog(const std::string& layout_script_file,
                                    const std::string& title,
                                    const std::string& message)
	: WavesDialog (layout_script_file, true, false )
	, _ok_button (get_waves_button ("ok_button"))
	, _cancel_button (get_waves_button ("cancel_button"))
    , _message_label (get_label("message_label"))
    , _name_entry (get_entry("name_entry"))
{
	init (title, message);
}

WavesEditDialog::WavesEditDialog (const std::string& title,
                                  const std::string& message)
	: WavesDialog ("waves_edit_dialog.xml", true, false )
	, _ok_button (get_waves_button ("ok_button"))
	, _cancel_button (get_waves_button ("cancel_button"))
    , _message_label (get_label("message_label"))
    , _name_entry (get_entry("name_entry"))
{
	init (title, message);
}

void WavesEditDialog::init(const std::string& title,
							  const std::string& message)
{
	set_modal (true);
	set_resizable (false);
    set_keep_above (true);

    _ok_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesEditDialog::_on_button_clicked));
    _cancel_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesEditDialog::_on_button_clicked));
    _message_label.set_text (message);

	set_title (title);
	show_all ();
}

void
WavesEditDialog::_on_button_clicked (WavesButton* clicked_button)
{
    hide ();
	if (clicked_button == &_ok_button) {
	    response (RESPONSE_DEFAULT);
	} else if (clicked_button == &_cancel_button) {
	    response (Gtk::RESPONSE_CANCEL);
	}
}

void
WavesEditDialog::set_entry_text (const std::string& message)
{
    _name_entry.set_text (message);
    _name_entry.select_region (0, -1);
    _name_entry.grab_focus();
}

std::string
WavesEditDialog::get_entry_text ()
{
    return _name_entry.get_text ();
}


