#include <string>
#include <iostream>

#include <gtkmm/main.h>

#include <gtkmm2ext/stateful_button.h>

using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace std;

StateButton::StateButton ()
{
	visual_state = 0;
	have_saved_bg = false;
}

void
StateButton::set_colors (const vector<Gdk::Color>& c)
{
	colors = c;
	visual_state++; // to force transition
	set_visual_state (visual_state - 1);
}

void
StateButton::set_visual_state (int n)
{
	if (!have_saved_bg) {
		/* not yet realized */
		visual_state = n;
		return;
	}

	if (n == visual_state) {
		return;
	}
	
	if (n == 0) {
		
		/* back to the default color */
		
		if (have_saved_bg) {
			bg_modify (STATE_NORMAL, saved_bg);
			bg_modify (STATE_ACTIVE, saved_bg);
			bg_modify (STATE_SELECTED, saved_bg);
			bg_modify (STATE_PRELIGHT, saved_bg);
		}
		
		
	} else {
		
		int index = (n-1) % colors.size ();
		
		bg_modify (STATE_NORMAL, colors[index]);
		bg_modify (STATE_ACTIVE, colors[index]);
		bg_modify (STATE_SELECTED, colors[index]);
		bg_modify (STATE_PRELIGHT, colors[index]);
	}

	visual_state = n;
}

/* ----------------------------------------------------------------- */

void
StatefulToggleButton::on_realize ()
{
	ToggleButton::on_realize ();

	if (!have_saved_bg) {
		saved_bg = get_style()->get_bg (STATE_NORMAL);
		have_saved_bg = true;
	}

	visual_state++; // to force transition
	set_visual_state (visual_state - 1);
}

void
StatefulButton::on_realize ()
{
	Button::on_realize ();

	if (!have_saved_bg) {
		saved_bg = get_style()->get_bg (STATE_NORMAL);
		have_saved_bg = true;
	}

	visual_state++; // to force transition
	set_visual_state (visual_state - 1);
}

void
StatefulToggleButton::on_toggled ()
{
	if (!_self_managed) {
		if (get_active()) {
			set_visual_state (1);
		} else {
			set_visual_state (0);
		}
	}
}
