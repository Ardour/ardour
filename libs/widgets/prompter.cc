/*
 * Copyright (C) 1999 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#include <string>
#include <gtkmm/stock.h>

#include "pbd/whitespace.h"
#include "widgets/prompter.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ArdourWidgets;

Prompter::Prompter (Gtk::Window& parent, bool modal, bool with_cancel)
	: Gtk::Dialog ("", parent, modal)
	, first_show (true)
	, can_accept_from_entry (false)
{
	init (with_cancel);
}

Prompter::Prompter (bool modal, bool with_cancel)
	: Gtk::Dialog ("", modal)
	, first_show (true)
	, can_accept_from_entry (false)
{
	init (with_cancel);
}

void
Prompter::init (bool with_cancel)
{
	set_type_hint (Gdk::WINDOW_TYPE_HINT_DIALOG);
	set_position (Gtk::WIN_POS_MOUSE);
	set_name ("Prompter");

	if (with_cancel) {
		/* some callers need to name this button more sensibly */
		add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	}

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
