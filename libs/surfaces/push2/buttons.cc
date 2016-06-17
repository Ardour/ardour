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
