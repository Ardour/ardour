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

#ifndef _WIDGETS_PROMPTER_H_
#define _WIDGETS_PROMPTER_H_

#include <string>
#include <gtkmm/box.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/dialog.h>
#include <sigc++/sigc++.h>

#include "widgets/visibility.h"

namespace Gtk {
	class Window;
}

namespace ArdourWidgets {

class LIBWIDGETS_API Prompter : public Gtk::Dialog
{
public:
	Prompter (bool modal = false, bool with_cancel_button = true);
	Prompter (Gtk::Window& parent, bool modal = false, bool with_cancel_button = true);
	~Prompter () {};

	void set_prompt (std::string prompt) {
		entryLabel.set_label (prompt);
	}

	void set_initial_text (std::string txt, bool allow_replace = false) {
		entry.set_text (txt);
		entry.select_region (0, entry.get_text_length());
		if (allow_replace) {
			on_entry_changed ();
		}
	}

	void change_labels (std::string ok, std::string cancel);

	void get_result (std::string &str, bool strip=true);

protected:
	Gtk::Entry& the_entry() { return entry; }

	void on_entry_changed ();
	void on_show ();

private:
	Gtk::Entry entry;
	Gtk::HBox entryBox;
	Gtk::Label entryLabel;
	bool first_show;
	bool can_accept_from_entry;

	void init (bool with_cancel);
	void entry_activated ();
};

} /* namespace */

#endif
