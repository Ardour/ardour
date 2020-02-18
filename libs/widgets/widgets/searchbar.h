/*
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

#pragma once

#include <gtkmm/entry.h>
#include <string>

#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API SearchBar : public Gtk::Entry
{
public:
	SearchBar (
		const std::string& placeholder_text = "Search...",
		bool icon_click_resets = true);

	/** resets the searchbar to the initial state */
	void reset ();

	/* emitted when the filter has been updated */
	sigc::signal<void, const std::string&> signal_search_string_updated () { return sig_search_string_updated; }

protected:
	bool focus_in_event (GdkEventFocus*);
	bool focus_out_event (GdkEventFocus*);

	bool key_press_event (GdkEventKey*);
	void icon_clicked_event (Gtk::EntryIconPosition, const GdkEventButton*);

	const std::string placeholder_text;
	sigc::signal<void, const std::string&> sig_search_string_updated;

private:
	void search_string_changed () const;

	Glib::RefPtr<Gdk::Pixbuf> icon;
	bool icon_click_resets;
};

} /* end namespace */
