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

*/

#ifndef __gtkmm2ext_prompter_h__
#define __gtkmm2ext_prompter_h__

#include <string>
#include <gtkmm/box.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/dialog.h>
#include <sigc++/sigc++.h>

#include "gtkmm2ext/visibility.h"

namespace Gtk {
	class Window;
}

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API Prompter : public Gtk::Dialog

{
  public:
	Prompter (bool modal = false);
	Prompter (Gtk::Window& parent, bool modal = false);
	~Prompter () {};

	void set_prompt (std::string prompt) {
		entryLabel.set_label (prompt);
	}

	void set_initial_text (std::string txt) {
		entry.set_text (txt);
		entry.select_region (0, entry.get_text_length());
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

	void init ();
	void entry_activated ();
};

} /* namespace */

#endif  /* __gtkmm2ext_prompter_h__ */
