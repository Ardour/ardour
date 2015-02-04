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

#include "waves_message_dialog.h"
WavesMessageDialog::WavesMessageDialog(const std::string& layout_script_file,
									   const std::string& title, 
									   const std::string& message,
									   unsigned buttons /* = WavesMessageDialog::BUTTONS_OK */)
	: WavesDialog (layout_script_file, true, false )
	, _ok_button (get_waves_button ("ok_button"))
	, _close_button (get_waves_button ("close_button"))
	, _accept_button (get_waves_button ("accept_button"))
	, _cancel_button (get_waves_button ("cancel_button"))
	, _yes_button (get_waves_button ("yes_button"))
	, _no_button (get_waves_button ("no_button"))
    , _message_label (get_label("message_label"))
{
	init (title, message, buttons);
}

WavesMessageDialog::WavesMessageDialog (const std::string& title,
										const std::string& message,
										unsigned buttons /* = WavesMessageDialog::BUTTONS_OK */)
	: WavesDialog ("waves_message_dialog.xml", true, false )
	, _ok_button (get_waves_button ("ok_button"))
	, _close_button (get_waves_button ("close_button"))
	, _accept_button (get_waves_button ("accept_button"))
	, _cancel_button (get_waves_button ("cancel_button"))
	, _yes_button (get_waves_button ("yes_button"))
	, _no_button (get_waves_button ("no_button"))
    , _message_label (get_label("message_label"))
{
	init (title, message, buttons);
}

void WavesMessageDialog::init(const std::string& title,
							  const std::string& message,
							  unsigned buttons)
{
	set_modal (true);
	set_resizable (false);
    set_keep_above (true);

	_ok_button.set_visible (buttons&BUTTON_OK);
    _ok_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesMessageDialog::_on_button_clicked));

	_close_button.set_visible  (buttons&BUTTON_CLOSE);
    _close_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesMessageDialog::_on_button_clicked));

	_accept_button.set_visible  (buttons&BUTTON_ACCEPT);
    _accept_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesMessageDialog::_on_button_clicked));

	_cancel_button.set_visible  (buttons&BUTTON_CANCEL);
    _cancel_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesMessageDialog::_on_button_clicked));

	_yes_button.set_visible  (buttons&BUTTON_YES);
    _yes_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesMessageDialog::_on_button_clicked));

	_no_button.set_visible  (buttons&BUTTON_NO);
    _no_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesMessageDialog::_on_button_clicked));
    
    _message_label.set_text (message);

	set_title (title);
	show_all ();
}

void
WavesMessageDialog::_on_button_clicked (WavesButton* clicked_button)
{
    hide ();
	if (clicked_button == &_ok_button) {
	    response (Gtk::RESPONSE_OK);
	} else if (clicked_button == &_close_button) {
	    response (Gtk::RESPONSE_CLOSE);
	} else if (clicked_button == &_accept_button) {
	    response (Gtk::RESPONSE_ACCEPT);
	} else if (clicked_button == &_cancel_button) {
	    response (Gtk::RESPONSE_CANCEL);
	} else if (clicked_button == &_yes_button) {
	    response (Gtk::RESPONSE_YES);
	} else if (clicked_button == &_no_button) {
	    response (Gtk::RESPONSE_NO);
	}
}

void
WavesMessageDialog::set_markup (std::string markup)
{
    _message_label.set_use_markup (true);
    _message_label.set_markup (markup);
}
