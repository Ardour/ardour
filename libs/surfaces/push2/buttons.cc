/*
  Copyright (C) 2016 Paul Davis

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "ardour/mute_control.h"
#include "ardour/session.h"
#include "ardour/solo_control.h"

#include "push2.h"

using namespace ArdourSurface;
using namespace ARDOUR;

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

void
Push2::button_browse ()
{
	switch_bank (max (0, bank_start - 8));
}

void
Push2::button_clip ()
{
	switch_bank (max (0, bank_start + 8));
}

void
Push2::button_upper (uint32_t n)
{
	if (!stripable[n]) {
		return;
	}

	boost::shared_ptr<SoloControl> sc = stripable[n]->solo_control ();
	if (sc) {
		sc->set_value (!sc->get_value(), PBD::Controllable::UseGroup);
	}
}

void
Push2::button_lower (uint32_t n)
{
	if (!stripable[n]) {
		return;
	}

	boost::shared_ptr<MuteControl> mc = stripable[n]->mute_control ();

	if (mc) {
		mc->set_value (!mc->get_value(), PBD::Controllable::UseGroup);
	}
}
