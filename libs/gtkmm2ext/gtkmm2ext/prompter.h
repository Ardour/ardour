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

#ifndef __gtkmm2ext_prompter_h__
#define __gtkmm2ext_prompter_h__

#include <string>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

namespace Gtkmm2ext {

class Prompter : public Gtk::Window

{
  public:
	Prompter (bool modal = false);
	~Prompter () {};

	void set_prompt (std::string prompt) {
		entryLabel.set_label (prompt);
	}

	void set_initial_text (std::string txt) {
		entry.set_text (txt);
		entry.select_region (0, entry.get_text_length());
	}

	void change_labels (std::string ok, std::string cancel);

	enum PrompterStatus {
		entered,
		cancelled
	};

	PrompterStatus status;
	void get_result (std::string &str);

	/* the prompter will send a `done' signal when it is finished.
	   the "caller" can then check `status' and if it wants to
	   can then call `get_result()'.
	*/

	sigc::signal<void> done;

  protected:
	Gtk::Entry& the_entry() { return entry; }

  private:
	Gtk::VBox packer;
	Gtk::HBox buttonBox;
	Gtk::Entry entry;
	Gtk::VBox entryBox;
	Gtk::Label entryLabel;
	Gtk::Button ok;
	Gtk::Button cancel;

	void activated ();
	void cancel_click ();

	bool deleted (GdkEventAny *);

	void on_realize ();
	void on_map ();
};

} /* namespace */

#endif  /* __gtkmm2ext_prompter_h__ */
