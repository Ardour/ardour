/*
    Copyright (C) 1999 Paul Barton-Davis 
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

    $Id$
*/

#include <string>

#include <pbd/whitespace.h>

#include <gtkmm/stock.h>
#include "waves_prompter.h"

#include "i18n.h"

WavesPrompter::WavesPrompter (const std::string &layout_script_file)
	: WavesDialog (layout_script_file)
	, _entry (get_entry ("entry"))
	, _entry_label (get_label ("entry_label"))
	, _accept_button (get_waves_button ("accept_button"))
	, _cancel_button (get_waves_button ("cancel_button"))
	, _first_show (true)
	, _can_accept_from_entry (false)
{
    _accept_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesPrompter::_on_accept_button_clicked));
    _cancel_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesPrompter::_on_cancel_button_clicked));
	set_type_hint (Gdk::WINDOW_TYPE_HINT_DIALOG);
	set_position (Gtk::WIN_POS_MOUSE);
}

void
WavesPrompter::on_show ()
{
	/* don't connect to signals till shown, so that we don't change the
	   response sensitivity etc. when the setup of the dialog sets the text.
	*/

	if (_first_show) {
		_entry.signal_changed().connect (mem_fun (*this, &WavesPrompter::on_entry_changed));
		_entry.signal_activate().connect (mem_fun (*this, &WavesPrompter::_entry_activated));
		_can_accept_from_entry = !_entry.get_text().empty();
		_first_show = false;
	}

	Dialog::on_show ();
}

void
WavesPrompter::get_result (std::string &str, bool strip)
{
	str = _entry.get_text ();
	if (strip) {
		PBD::strip_whitespace_edges (str);
	}
}

void
WavesPrompter::_entry_activated ()
{
	if (_can_accept_from_entry) {
		response (Gtk::RESPONSE_ACCEPT);
	} else {
		response (Gtk::RESPONSE_CANCEL);
	}
}		

void
WavesPrompter::on_entry_changed ()
{
	/* 
	   This is set up so that entering text in the _entry 
	   field makes the RESPONSE_ACCEPT button active. 
	   Of course if you haven't added a RESPONSE_ACCEPT 
	   button, nothing will happen at all.
	*/

	if (!_entry.get_text().empty()) {
		set_response_sensitive (Gtk::RESPONSE_ACCEPT, true);
		set_default_response (Gtk::RESPONSE_ACCEPT);
		_can_accept_from_entry = true;
	} else {
		set_response_sensitive (Gtk::RESPONSE_ACCEPT, false);
	}
}

void
WavesPrompter::_on_accept_button_clicked (WavesButton*)
{
	response (Gtk::RESPONSE_ACCEPT);
}

void
WavesPrompter::_on_cancel_button_clicked (WavesButton*)
{
	response (Gtk::RESPONSE_CANCEL);
}
