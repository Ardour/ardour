#include "ardour/session.h"

#include "push2.h"

using namespace ArdourSurface;

void
Push2::button_play ()
{
	if (session->transport_rolling ()) {
		transport_stop ();
	} else {
		transport_play ();
	}
}

void
Push2::button_recenable ()
{
	rec_enable_toggle ();
}

void
Push2::button_up ()
{
	scroll_up_1_track ();
}

void
Push2::button_down ()
{
	scroll_dn_1_track ();
}

void
Push2::button_right ()
{
	ScrollTimeline (0.75);
}

void
Push2::button_left ()
{
	ScrollTimeline (-0.75);
}

void
Push2::button_repeat ()
{
	loop_toggle ();
}

void
Push2::button_metronome ()
{
	toggle_click ();
}

void
Push2::button_solo ()
{
	cancel_all_solo ();
}

void
Push2::button_new ()
{
	access_action ("Editor/start-range-from-playhead");

	id_button_map[New]->set_color (LED::White);
	id_button_map[New]->set_state (LED::NoTransition);
	write (id_button_map[New]->state_msg());

	/* blink the button for the other half of this operation */

	id_button_map[FixedLength]->set_color (LED::White);
	id_button_map[FixedLength]->set_state (LED::Blinking4th);
	write (id_button_map[FixedLength]->state_msg());
}


void
Push2::button_fixed_length ()
{
	access_action ("Editor/finish-range-from-playhead");

	/* turn off both buttons for this operation */

	id_button_map[New]->set_color (LED::Black);
	id_button_map[New]->set_state (LED::NoTransition);
	write (id_button_map[New]->state_msg());
	id_button_map[FixedLength]->set_color (LED::Black);
	id_button_map[FixedLength]->set_state (LED::NoTransition);
	write (id_button_map[FixedLength]->state_msg());
}

void
Push2::button_shift_press ()
{
	modifier_state = ModifierState (modifier_state | ModShift);
}

void
Push2::button_shift_release ()
{
	modifier_state = ModifierState (modifier_state & ~ModShift);
}

