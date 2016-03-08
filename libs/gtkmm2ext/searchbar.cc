#include "gtkmm2ext/searchbar.h"
#include "gtkmm2ext/keyboard.h"
#include <iostream>

namespace Gtkmm2ext {

SearchBar::SearchBar (const std::string& label, bool icon_resets)
	: placeholder_text (label)
	, icon_click_resets (icon_resets)
{
	set_text (placeholder_text);
	set_alignment (Gtk::ALIGN_CENTER);
	signal_key_press_event().connect (sigc::mem_fun (*this, &SearchBar::key_press_event));
	signal_focus_in_event().connect (sigc::mem_fun (*this, &SearchBar::focus_in_event));
	signal_focus_out_event().connect (sigc::mem_fun (*this, &SearchBar::focus_out_event));
	signal_changed().connect (sigc::mem_fun (*this, &SearchBar::search_string_changed));
	signal_icon_release().connect (sigc::mem_fun (*this, &SearchBar::icon_clicked_event));
}

bool
SearchBar::focus_in_event (GdkEventFocus*)
{
	if (get_text ().compare (placeholder_text) == 0) {
		set_text ("");
	}

	icon = get_icon_pixbuf ();
	if (icon) {
		set_icon_from_pixbuf (Glib::RefPtr<Gdk::Pixbuf> ());
	}
	return true;
}

bool
SearchBar::focus_out_event (GdkEventFocus*)
{
	if (get_text ().empty ()) {
		set_text (placeholder_text);
	}

	if (icon) {
		set_icon_from_pixbuf (icon);
		icon.reset ();
	}

	search_string_changed ();
	return false;
}

bool
SearchBar::key_press_event (GdkEventKey* ev)
{
	switch (ev->keyval) {
	case GDK_Escape: 
		set_text (placeholder_text);
		return true;
	default:
		break;
	}

	return false;
}

void
SearchBar::icon_clicked_event (Gtk::EntryIconPosition, const GdkEventButton*)
{
	if (icon_click_resets) {
		reset ();
	}
	else {
		search_string_changed ();
	}
}

void
SearchBar::search_string_changed () const
{
	const std::string& text = get_text ();
	if (text.empty() || text.compare (placeholder_text) == 0) {
		sig_search_string_updated ("");
		return;
	}
	sig_search_string_updated (text);
}

void
SearchBar::reset ()
{
	set_text (placeholder_text);
	search_string_changed ();
}

}