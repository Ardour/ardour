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
	_is_realized = false;
	visual_state = 0;
}

void
StateButton::set_visual_state (int n)
{
	if (!_is_realized) {
		/* not yet realized */
		visual_state = n;
		return;
	}

	if (n == visual_state) {
		return;
	}

	string name = get_widget_name ();
	name = name.substr (0, name.find_last_of ('-'));

	switch (n) {
	case 0:
		/* relax */
		break;
	case 1:
		name += "-active";
		break;
	case 2:
		name += "-alternate";
		break;
	}

	set_widget_name (name);
	visual_state = n;
}

/* ----------------------------------------------------------------- */

void
StatefulToggleButton::on_realize ()
{
	ToggleButton::on_realize ();

	_is_realized = true;
	visual_state++; // to force transition
	set_visual_state (visual_state - 1);
}

void
StatefulButton::on_realize ()
{
	Button::on_realize ();

	_is_realized = true;
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
