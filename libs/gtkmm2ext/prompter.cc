/*
    Copyright (C) 1999 Paul Barton-Davis 

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
#include <gtkmm2ext/prompter.h>

#include "i18n.h"

using namespace std;
using namespace Gtkmm2ext;

Prompter::Prompter (Gtk::Window& parent, bool modal)
	: Gtk::Dialog ("", parent, modal)
	, first_show (true)
	, can_accept_from_entry (false)
{
	init ();
}

Prompter::Prompter (bool modal)
	: Gtk::Dialog ("", modal)
	, first_show (true)
	, can_accept_from_entry (false)
{
	init ();
}

void
Prompter::init ()
{
	set_type_hint (Gdk::WINDOW_TYPE_HINT_DIALOG);
	set_position (Gtk::WIN_POS_MOUSE);
	set_name ("Prompter");
	
	add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);

	/* 
	   Alas a generic 'affirmative' button seems a bit useless sometimes.
	   You will have to add your own.
	   After adding, use :
	   set_response_sensitive (Gtk::RESPONSE_ACCEPT, false)
	   to prevent the RESPONSE_ACCEPT button from permitting blank strings.
	*/
	
	entryLabel.set_line_wrap (true);
	entryLabel.set_name ("PrompterLabel");

	entryBox.set_homogeneous (false);
	entryBox.set_spacing (5);
	entryBox.set_border_width (10);
	entryBox.pack_start (entryLabel, false, false);
	entryBox.pack_start (entry, true, true);

	get_vbox()->pack_start (entryBox);
	show_all_children();
}	

void
Prompter::on_show ()
{
	/* don't connect to signals till shown, so that we don't change the
	   response sensitivity etc. when the setup of the dialog sets the text.
	*/

	if (first_show) {
		entry.signal_changed().connect (mem_fun (*this, &Prompter::on_entry_changed));
		entry.signal_activate().connect (mem_fun (*this, &Prompter::entry_activated));
		can_accept_from_entry = !entry.get_text().empty();
		first_show = false;
	}

	Dialog::on_show ();
}

void
Prompter::change_labels (string /*okstr*/, string /*cancelstr*/)
{
	// dynamic_cast<Gtk::Label*>(ok.get_child())->set_text (okstr);
	// dynamic_cast<Gtk::Label*>(cancel.get_child())->set_text (cancelstr);
}

void
Prompter::get_result (string &str, bool strip)
{
	str = entry.get_text ();
	if (strip) {
		PBD::strip_whitespace_edges (str);
	}
}

void
Prompter::entry_activated ()
{
	if (can_accept_from_entry) {
		response (Gtk::RESPONSE_ACCEPT);
	} else {
		response (Gtk::RESPONSE_CANCEL);
	}
}		

void
Prompter::on_entry_changed ()
{
	/* 
	   This is set up so that entering text in the entry 
	   field makes the RESPONSE_ACCEPT button active. 
	   Of course if you haven't added a RESPONSE_ACCEPT 
	   button, nothing will happen at all.
	*/

	if (!entry.get_text().empty()) {
		set_response_sensitive (Gtk::RESPONSE_ACCEPT, true);
		set_default_response (Gtk::RESPONSE_ACCEPT);
		can_accept_from_entry = true;
	} else {
		set_response_sensitive (Gtk::RESPONSE_ACCEPT, false);
	}
}
