#include <gtkmm2ext/focus_entry.h>

using namespace Gtkmm2ext;

FocusEntry::FocusEntry ()
{
	next_release_selects = false;
}

bool 
FocusEntry::on_button_press_event (GdkEventButton* ev)
{
	if (!has_focus()) {
		next_release_selects = true;
	}
	return Entry::on_button_press_event (ev);
}

bool 
FocusEntry::on_button_release_event (GdkEventButton* ev)
{
	if (next_release_selects) {
		bool ret = Entry::on_button_release_event (ev);
		select_region (0, -1);
		next_release_selects = false;
		return ret;
	} 

	return Entry::on_button_release_event (ev);
}

