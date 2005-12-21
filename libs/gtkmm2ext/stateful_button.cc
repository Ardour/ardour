#include <string>
#include <iostream>
#include "gtkmm2ext/stateful_button.h"

using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace std;

StatefulButton::StatefulButton ()
{
	current_state = 0;
	have_saved_bg = false;
}

StatefulButton::StatefulButton (const string& label)
	: Button (label)
{
	current_state = 0;
	have_saved_bg = false;
}

void
StatefulButton::set_colors (const vector<Gdk::Color>& c)
{
	colors = c;
	current_state++; // to force transition
	set_state (current_state - 1);
}

void
StatefulButton::on_realize ()
{
	Button::on_realize ();

	if (!have_saved_bg) {
		saved_bg = get_style()->get_bg (STATE_NORMAL);
		have_saved_bg = true;
	}

	current_state++; // to force transition
	set_state (current_state - 1);
}

void
StatefulButton::set_state (int n)
{
	if (is_realized()) {

		if (n == current_state) {
			return;
		}
		
		if (n == 0) {
			
			/* back to the default color */
			
			if (have_saved_bg) {
				modify_bg (STATE_NORMAL, saved_bg);
				modify_bg (STATE_ACTIVE, saved_bg);
				modify_bg (STATE_SELECTED, saved_bg);
				modify_bg (STATE_PRELIGHT, saved_bg);
			}
			

		} else {
			
			int index = (n-1) % colors.size ();
			
			modify_bg (STATE_NORMAL, colors[index]);
			modify_bg (STATE_ACTIVE, colors[index]);
			modify_bg (STATE_SELECTED, colors[index]);
			modify_bg (STATE_PRELIGHT, colors[index]);
		}
		
		/* leave insensitive alone */
	}

	current_state = n;
}
