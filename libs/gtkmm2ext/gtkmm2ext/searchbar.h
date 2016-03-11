#pragma once

#include <gtkmm/entry.h>
#include <string>

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API SearchBar : public Gtk::Entry
{
public:
	SearchBar(
		const std::string& placeholder_text = "Search...",
		bool icon_click_resets = true);

	// resets the searchbar to the initial state
	void reset ();
	// emitted when the filter has been updated
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

}
