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

#ifndef __waves_prompter_h__
#define __waves_prompter_h__

#include <string>
#include <sigc++/sigc++.h>

#include "waves_dialog.h"

class WavesPrompter : public WavesDialog

{
  public:
	WavesPrompter (const std::string &layout_script_file);
	~WavesPrompter () {};

	void set_prompt (std::string prompt) {
		_entry_label.set_label (prompt);
	}

	void set_initial_text (std::string txt) {
		_entry.set_text (txt);
		_entry.select_region (0, _entry.get_text_length());
	}

	void get_result (std::string &str, bool strip=true);

  protected:
	Gtk::Entry& the_entry() { return _entry; }

	void on_entry_changed ();
	void on_show ();

  private:
	Gtk::Entry& _entry;
	Gtk::Label& _entry_label;
	WavesButton& _accept_button;
	WavesButton& _cancel_button;
	bool _first_show;
	bool _can_accept_from_entry;

	void _init ();
	void _entry_activated ();
	void _on_accept_button_clicked (WavesButton*);
	void _on_cancel_button_clicked (WavesButton*);
};

#endif  /* __waves_prompter_h__ */
