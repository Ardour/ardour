/*
 * Copyright (C) 2016-2018 Paul Davis <paul@linuxaudiosystems.com>
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

#include <algorithm>

#include "ardour/debug.h"
#include "ardour/mute_control.h"
#include "ardour/session.h"
#include "ardour/solo_control.h"

#include "layout.h"
#include "push2.h"
#include "track_mix.h"

using namespace ArdourSurface;
using namespace ARDOUR;
using namespace PBD;
using std::cerr;

void
Push2::build_maps ()
{
	/* Pads */

	boost::shared_ptr<Pad> pad;

#define MAKE_PAD(x,y,nn) \
	pad.reset (new Pad ((x), (y), (nn))); \
	nn_pad_map.insert (std::make_pair (pad->extra(), pad));

	MAKE_PAD (0, 0, 92);
	MAKE_PAD (0, 1, 93);
	MAKE_PAD (0, 2, 94);
	MAKE_PAD (0, 3, 95);
	MAKE_PAD (0, 4, 96);
	MAKE_PAD (0, 5, 97);
	MAKE_PAD (0, 6, 98);
	MAKE_PAD (0, 7, 99);
	MAKE_PAD (1, 0, 84);
	MAKE_PAD (1, 1, 85);
	MAKE_PAD (1, 2, 86);
	MAKE_PAD (1, 3, 87);
	MAKE_PAD (1, 4, 88);
	MAKE_PAD (1, 5, 89);
	MAKE_PAD (1, 6, 90);
	MAKE_PAD (1, 7, 91);
	MAKE_PAD (2, 0, 76);
	MAKE_PAD (2, 1, 77);
	MAKE_PAD (2, 2, 78);
	MAKE_PAD (2, 3, 79);
	MAKE_PAD (2, 4, 80);
	MAKE_PAD (2, 5, 81);
	MAKE_PAD (2, 6, 82);
	MAKE_PAD (2, 7, 83);
	MAKE_PAD (3, 0, 68);
	MAKE_PAD (3, 1, 69);
	MAKE_PAD (3, 2, 70);
	MAKE_PAD (3, 3, 71);
	MAKE_PAD (3, 4, 72);
	MAKE_PAD (3, 5, 73);
	MAKE_PAD (3, 6, 74);
	MAKE_PAD (3, 7, 75);
	MAKE_PAD (4, 0, 60);
	MAKE_PAD (4, 1, 61);
	MAKE_PAD (4, 2, 62);
	MAKE_PAD (4, 3, 63);
	MAKE_PAD (4, 4, 64);
	MAKE_PAD (4, 5, 65);
	MAKE_PAD (4, 6, 66);
	MAKE_PAD (4, 7, 67);
	MAKE_PAD (5, 0, 52);
	MAKE_PAD (5, 1, 53);
	MAKE_PAD (5, 2, 54);
	MAKE_PAD (5, 3, 55);
	MAKE_PAD (5, 4, 56);
	MAKE_PAD (5, 5, 57);
	MAKE_PAD (5, 6, 58);
	MAKE_PAD (5, 7, 59);
	MAKE_PAD (6, 0, 44);
	MAKE_PAD (6, 1, 45);
	MAKE_PAD (6, 2, 46);
	MAKE_PAD (6, 3, 47);
	MAKE_PAD (6, 4, 48);
	MAKE_PAD (6, 5, 49);
	MAKE_PAD (6, 6, 50);
	MAKE_PAD (6, 7, 51);
	MAKE_PAD (7, 0, 36);
	MAKE_PAD (7, 1, 37);
	MAKE_PAD (7, 2, 38);
	MAKE_PAD (7, 3, 39);
	MAKE_PAD (7, 4, 40);
	MAKE_PAD (7, 5, 41);
	MAKE_PAD (7, 6, 42);
	MAKE_PAD (7, 7, 43);

	/* Now color buttons */

	boost::shared_ptr<Button> button;

#define MAKE_COLOR_BUTTON(i,cc) \
	button.reset (new ColorButton ((i), (cc))); \
	cc_button_map.insert (std::make_pair (button->controller_number(), button)); \
	id_button_map.insert (std::make_pair (button->id, button));
#define MAKE_COLOR_BUTTON_PRESS(i,cc,p)\
	button.reset (new ColorButton ((i), (cc), (p))); \
	cc_button_map.insert (std::make_pair (button->controller_number(), button)); \
	id_button_map.insert (std::make_pair (button->id, button))
#define MAKE_COLOR_BUTTON_PRESS_RELEASE_LONG(i,cc,p,r,l)                      \
	button.reset (new ColorButton ((i), (cc), (p), (r), (l))); \
	cc_button_map.insert (std::make_pair (button->controller_number(), button)); \
	id_button_map.insert (std::make_pair (button->id, button))

	MAKE_COLOR_BUTTON_PRESS (Upper1, 102, &Push2::button_upper_1);
	MAKE_COLOR_BUTTON_PRESS (Upper2, 103, &Push2::button_upper_2);
	MAKE_COLOR_BUTTON_PRESS (Upper3, 104, &Push2::button_upper_3);
	MAKE_COLOR_BUTTON_PRESS (Upper4, 105, &Push2::button_upper_4);
	MAKE_COLOR_BUTTON_PRESS (Upper5, 106, &Push2::button_upper_5);
	MAKE_COLOR_BUTTON_PRESS (Upper6, 107, &Push2::button_upper_6);
	MAKE_COLOR_BUTTON_PRESS (Upper7, 108, &Push2::button_upper_7);
	MAKE_COLOR_BUTTON_PRESS (Upper8, 109, &Push2::button_upper_8);
	MAKE_COLOR_BUTTON_PRESS (Lower1, 20, &Push2::button_lower_1);
	MAKE_COLOR_BUTTON_PRESS (Lower2, 21, &Push2::button_lower_2);
	MAKE_COLOR_BUTTON_PRESS (Lower3, 22, &Push2::button_lower_3);
	MAKE_COLOR_BUTTON_PRESS (Lower4, 23, &Push2::button_lower_4);
	MAKE_COLOR_BUTTON_PRESS (Lower5, 24, &Push2::button_lower_5);
	MAKE_COLOR_BUTTON_PRESS (Lower6, 25, &Push2::button_lower_6);
	MAKE_COLOR_BUTTON_PRESS (Lower7, 26, &Push2::button_lower_7);
	MAKE_COLOR_BUTTON_PRESS (Lower8, 27, &Push2::button_lower_8);
	MAKE_COLOR_BUTTON_PRESS (Master, 28, &Push2::button_master);
	MAKE_COLOR_BUTTON_PRESS (Mute, 60, &Push2::button_mute);
	MAKE_COLOR_BUTTON_PRESS_RELEASE_LONG (Solo, 61, &Push2::relax, &Push2::button_solo, &Push2::button_solo_long_press);
	MAKE_COLOR_BUTTON_PRESS (Stop, 29, &Push2::button_stop);
	MAKE_COLOR_BUTTON_PRESS (Fwd32ndT, 43, &Push2::button_fwd32t);
	MAKE_COLOR_BUTTON_PRESS (Fwd32nd,42 , &Push2::button_fwd32);
	MAKE_COLOR_BUTTON_PRESS (Fwd16thT, 41, &Push2::button_fwd16t);
	MAKE_COLOR_BUTTON_PRESS (Fwd16th, 40, &Push2::button_fwd16);
	MAKE_COLOR_BUTTON_PRESS (Fwd8thT, 39 , &Push2::button_fwd8t);
	MAKE_COLOR_BUTTON_PRESS (Fwd8th, 38, &Push2::button_fwd8);
	MAKE_COLOR_BUTTON_PRESS (Fwd4trT, 37, &Push2::button_fwd4t);
	MAKE_COLOR_BUTTON_PRESS (Fwd4tr, 36, &Push2::button_fwd4);
	MAKE_COLOR_BUTTON (Automate, 89);
	MAKE_COLOR_BUTTON_PRESS (RecordEnable, 86, &Push2::button_recenable);
	MAKE_COLOR_BUTTON_PRESS (Play, 85, &Push2::button_play);

#define MAKE_WHITE_BUTTON(i,cc)\
	button.reset (new WhiteButton ((i), (cc))); \
	cc_button_map.insert (std::make_pair (button->controller_number(), button)); \
	id_button_map.insert (std::make_pair (button->id, button))
#define MAKE_WHITE_BUTTON_PRESS(i,cc,p)\
	button.reset (new WhiteButton ((i), (cc), (p))); \
	cc_button_map.insert (std::make_pair (button->controller_number(), button)); \
	id_button_map.insert (std::make_pair (button->id, button))
#define MAKE_WHITE_BUTTON_PRESS_RELEASE(i,cc,p,r)                                \
	button.reset (new WhiteButton ((i), (cc), (p), (r))); \
	cc_button_map.insert (std::make_pair (button->controller_number(), button)); \
	id_button_map.insert (std::make_pair (button->id, button))
#define MAKE_WHITE_BUTTON_PRESS_RELEASE_LONG(i,cc,p,r,l)                      \
	button.reset (new WhiteButton ((i), (cc), (p), (r), (l))); \
	cc_button_map.insert (std::make_pair (button->controller_number(), button)); \
	id_button_map.insert (std::make_pair (button->id, button))

	MAKE_WHITE_BUTTON (TapTempo, 3);
	MAKE_WHITE_BUTTON_PRESS (Metronome, 9, &Push2::button_metronome);
	MAKE_WHITE_BUTTON (Setup, 30);
	MAKE_WHITE_BUTTON (User, 59);
	MAKE_WHITE_BUTTON (Delete, 118);
	MAKE_WHITE_BUTTON (AddDevice, 52);
	MAKE_WHITE_BUTTON (Device, 110);
	MAKE_WHITE_BUTTON_PRESS (Mix, 112, &Push2::button_mix_press);
	MAKE_WHITE_BUTTON_PRESS (Undo, 119, &Push2::button_undo);
	MAKE_WHITE_BUTTON_PRESS (AddTrack, 53, &Push2::button_add_track);
	MAKE_WHITE_BUTTON_PRESS (Browse, 111, &Push2::button_browse);
	MAKE_WHITE_BUTTON_PRESS (Clip, 113, &Push2::button_clip);
	MAKE_WHITE_BUTTON (Convert, 35);
	MAKE_WHITE_BUTTON (DoubleLoop, 117);
	MAKE_WHITE_BUTTON_PRESS (Quantize, 116, &Push2::button_quantize);
	MAKE_WHITE_BUTTON_PRESS (Duplicate, 88, &Push2::button_duplicate);
	MAKE_WHITE_BUTTON_PRESS (New, 87, &Push2::button_new);
	MAKE_WHITE_BUTTON_PRESS (FixedLength, 90, &Push2::button_fixed_length);
	MAKE_WHITE_BUTTON_PRESS (Up, 46, &Push2::button_up);
	MAKE_WHITE_BUTTON_PRESS (Right, 45, &Push2::button_right);
	MAKE_WHITE_BUTTON_PRESS (Down, 47, &Push2::button_down);
	MAKE_WHITE_BUTTON_PRESS (Left, 44, &Push2::button_left);
	MAKE_WHITE_BUTTON_PRESS (Repeat, 56, &Push2::button_repeat);
	MAKE_WHITE_BUTTON (Accent, 57);
	MAKE_WHITE_BUTTON_PRESS (Scale, 58, &Push2::button_scale_press);
	MAKE_WHITE_BUTTON_PRESS (Layout, 31, &Push2::button_layout_press);
	MAKE_WHITE_BUTTON (Note, 50);
	MAKE_WHITE_BUTTON (Session, 51);
	MAKE_WHITE_BUTTON (Layout, 31);
	MAKE_WHITE_BUTTON_PRESS (OctaveUp, 55, &Push2::button_octave_up);
	MAKE_WHITE_BUTTON_PRESS (PageRight, 63, &Push2::button_page_right);
	MAKE_WHITE_BUTTON_PRESS (OctaveDown, 54, &Push2::button_octave_down);
	MAKE_WHITE_BUTTON_PRESS (PageLeft, 62, &Push2::button_page_left);
	MAKE_WHITE_BUTTON_PRESS_RELEASE_LONG (Shift, 49, &Push2::button_shift_press, &Push2::button_shift_release, &Push2::button_shift_long_press);
	MAKE_WHITE_BUTTON_PRESS_RELEASE_LONG (Select, 48, &Push2::button_select_press, &Push2::button_select_release, &Push2::button_select_long_press);
}

std::string
Push2::button_name_by_id (ButtonID id)
{
	switch (id) {
	case TapTempo:
		return "TapTempo";
	case Metronome:
		return "Metronome";
	case Upper1:
		return "Upper1";
	case Upper2:
		return "Upper2";
	case Upper3:
		return "Upper3";
	case Upper4:
		return "Upper4";
	case Upper5:
		return "Upper5";
	case Upper6:
		return "Upper6";
	case Upper7:
		return "Upper7";
	case Upper8:
		return "Upper8";
	case Setup:
		return "Setup";
	case User:
		return "User";
	case Delete:
		return "Delete";
	case AddDevice:
		return "AddDevice";
	case Device:
		return "Device";
	case Mix:
		return "Mix";
	case Undo:
		return "Undo";
	case AddTrack:
		return "AddTrack";
	case Browse:
		return "Browse";
	case Clip:
		return "Clip";
	case Mute:
		return "Mute";
	case Solo:
		return "Solo";
	case Stop:
		return "Stop";
	case Lower1:
		return "Lower1";
	case Lower2:
		return "Lower2";
	case Lower3:
		return "Lower3";
	case Lower4:
		return "Lower4";
	case Lower5:
		return "Lower5";
	case Lower6:
		return "Lower6";
	case Lower7:
		return "Lower7";
	case Lower8:
		return "Lower8";
	case Master:
		return "Master";
	case Convert:
		return "Convert";
	case DoubleLoop:
		return "DoubleLoop";
	case Quantize:
		return "Quantize";
	case Duplicate:
		return "Duplicate";
	case New:
		return "New";
	case FixedLength:
		return "FixedLength";
	case Automate:
		return "Automate";
	case RecordEnable:
		return "RecordEnable";
	case Play:
		return "Play";
	case Fwd32ndT:
		return "Fwd32ndT";
	case Fwd32nd:
		return "Fwd32nd";
	case Fwd16thT:
		return "Fwd16thT";
	case Fwd16th:
		return "Fwd16th";
	case Fwd8thT:
		return "Fwd8thT";
	case Fwd8th:
		return "Fwd8th";
	case Fwd4trT:
		return "Fwd4trT";
	case Fwd4tr:
		return "Fwd4tr";
	case Up:
		return "Up";
	case Right:
		return "Right";
	case Down:
		return "Down";
	case Left:
		return "Left";
	case Repeat:
		return "Repeat";
	case Accent:
		return "Accent";
	case Scale:
		return "Scale";
	case Layout:
		return "Layout";
	case Note:
		return "Note";
	case Session:
		return "Session";
	case OctaveUp:
		return "OctaveUp";
	case PageRight:
		return "PageRight";
	case OctaveDown:
		return "OctaveDown";
	case PageLeft:
		return "PageLeft";
	case Shift:
		return "Shift";
	case Select:
		return "Select";
	default:
		break;
	}

	return "???";
}

void
Push2::button_play ()
{
	if (!session) {
		return;
	}

	if (_modifier_state & ModShift) {
		goto_start (session->transport_rolling());
		return;
	}

	if (_modifier_state & ModSelect) {
		if (in_range_select) {
			in_range_select = true;
			access_action ("Common/start-range-from-playhead");
		} else {
			access_action ("Common/finish-range-from-playhead");
			in_range_select = false;
		}
		return;
	}

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
	_current_layout->button_up ();
}

void
Push2::button_down ()
{
	_current_layout->button_down ();
}

void
Push2::button_page_right ()
{
	ScrollTimeline (0.75);
}

void
Push2::button_page_left ()
{
	ScrollTimeline (-0.75);
}

void
Push2::button_right ()
{
	_current_layout->button_right ();
}

void
Push2::button_left ()
{
	_current_layout->button_left ();
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
Push2::button_solo_long_press ()
{
	cancel_all_solo ();
}

void
Push2::button_mute ()
{
	if (_current_layout) {
		_current_layout->button_mute ();
	}
}

void
Push2::button_solo ()
{
	if (_current_layout) {
		_current_layout->button_solo ();
	}
}

void
Push2::button_new ()
{
	access_action ("Common/start-range-from-playhead");

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
	access_action ("Common/finish-range-from-playhead");

	/* turn off both buttons for this operation */

	id_button_map[New]->set_color (LED::Black);
	id_button_map[New]->set_state (LED::NoTransition);
	write (id_button_map[New]->state_msg());
	id_button_map[FixedLength]->set_color (LED::Black);
	id_button_map[FixedLength]->set_state (LED::NoTransition);
	write (id_button_map[FixedLength]->state_msg());
}

void
Push2::button_browse ()
{
	access_action ("Common/addExistingAudioFiles");
}

void
Push2::button_clip ()
{
}

void
Push2::button_upper (uint32_t n)
{
	_current_layout->button_upper (n);
}

void
Push2::button_lower (uint32_t n)
{
	_current_layout->button_lower (n);
}

void
Push2::button_undo ()
{
	if (_modifier_state & ModShift) {
		ControlProtocol::Redo ();
	} else {
		ControlProtocol::Undo ();
	}
}

void
Push2::button_fwd32t ()
{
	const int n = (_modifier_state & ModShift) ? 8 : 0;
	goto_nth_marker (0+n);
}

void
Push2::button_fwd32 ()
{
	const int n = (_modifier_state & ModShift) ? 8 : 0;
	goto_nth_marker (1+n);
}

void
Push2::button_fwd16t ()
{
	const int n = (_modifier_state & ModShift) ? 8 : 0;
	goto_nth_marker (2+n);
}

void
Push2::button_fwd16 ()
{
	const int n = (_modifier_state & ModShift) ? 8 : 0;
	goto_nth_marker (3+n);
}

void
Push2::button_fwd8t ()
{
	const int n = (_modifier_state & ModShift) ? 8 : 0;
	goto_nth_marker (4+n);
}

void
Push2::button_fwd8 ()
{
	const int n = (_modifier_state & ModShift) ? 8 : 0;
	goto_nth_marker (5+n);
}

void
Push2::button_fwd4t ()
{
	const int n = (_modifier_state & ModShift) ? 8 : 0;
	goto_nth_marker (6+n);
}

void
Push2::button_fwd4 ()
{
	const int n = (_modifier_state & ModShift) ? 8 : 0;
	goto_nth_marker (7+n);
}

void
Push2::button_add_track ()
{
	access_action ("Main/AddTrackBus");
}

void
Push2::button_stop ()
{
	/* close current window */
	access_action ("Main/close-current-dialog");
}

void
Push2::button_shift_press ()
{
	start_shift ();
}

void
Push2::button_shift_release ()
{
	end_shift ();
}

void
Push2::button_shift_long_press ()
{
	access_action ("Main/close-current-dialog");
}

void
Push2::button_select_press ()
{
	cerr << "start select\n";
	_modifier_state = ModifierState (_modifier_state | ModSelect);
	boost::shared_ptr<Button> b = id_button_map[Select];
	b->set_color (Push2::LED::White);
	b->set_state (Push2::LED::Blinking16th);
	write (b->state_msg());

	_current_layout->button_select_press ();
}

void
Push2::button_select_release ()
{
	if (_modifier_state & ModSelect) {
		cerr << "end select\n";
		_modifier_state = ModifierState (_modifier_state & ~(ModSelect));
		boost::shared_ptr<Button> b = id_button_map[Select];
		b->timeout_connection.disconnect ();
		b->set_color (Push2::LED::White);
		b->set_state (Push2::LED::OneShot24th);
		write (b->state_msg());
	}

	_current_layout->button_select_release ();
}

void
Push2::button_select_long_press ()
{
	access_action ("Main/Escape");
}

bool
Push2::button_long_press_timeout (ButtonID id)
{
	if (buttons_down.find (id) != buttons_down.end()) {
		DEBUG_TRACE (DEBUG::Push2, string_compose ("long press timeout for %1, invoking method\n", id));
		boost::shared_ptr<Button> button = id_button_map[id];
		(this->*button->long_press_method) ();
	} else {
		DEBUG_TRACE (DEBUG::Push2, string_compose ("long press timeout for %1, expired/cancelled\n", id));
		/* release happened and somehow we were not cancelled */
	}

	/* whichever button this was, we've used it ... don't invoke the
	   release action.
	*/
	consumed.insert (id);

	return false; /* don't get called again */
}

void
Push2::start_press_timeout (boost::shared_ptr<Button> button, ButtonID id)
{
	assert (button);
	Glib::RefPtr<Glib::TimeoutSource> timeout = Glib::TimeoutSource::create (500); // milliseconds
	button->timeout_connection = timeout->connect (sigc::bind (sigc::mem_fun (*this, &Push2::button_long_press_timeout), id));
	timeout->attach (main_loop()->get_context());
}

void
Push2::button_octave_down ()
{
	if (_modifier_state & ModShift) {
		octave_shift = 0;
		return;
	}

	int os = (std::max (-4, octave_shift - 1));
	if (os != octave_shift) {
		octave_shift = os;
	}
}

void
Push2::button_octave_up ()
{
	if (_modifier_state & ModShift) {
		octave_shift = 0;
		return;
	}

	int os = (std::min (4, octave_shift + 1));
	if (os != octave_shift) {
		octave_shift = os;
	}
}

void
Push2::button_layout_press ()
{
	if (percussion) {
		set_percussive_mode (false);
	} else {
		set_percussive_mode (true);
	}
}

void
Push2::button_scale_press ()
{
	if (_current_layout != scale_layout) {
		set_current_layout (scale_layout);
	} else {
		if (ControlProtocol::first_selected_stripable()) {
			set_current_layout (mix_layout);
		}
	}
}

void
Push2::button_mix_press ()
{
	if (_current_layout == track_mix_layout) {
		set_current_layout (mix_layout);
	} else {
		if (ControlProtocol::first_selected_stripable()) {
			set_current_layout (track_mix_layout);
		}
	}
}

void
Push2::button_master ()
{
	boost::shared_ptr<Stripable> main_out = session->master_out ();

	if (!main_out) {
		return;
	}

	if (_current_layout != track_mix_layout) {
		ControlProtocol::set_stripable_selection (main_out);
		set_current_layout (track_mix_layout);
	} else {
		TrackMixLayout* tml = dynamic_cast<TrackMixLayout*> (_current_layout);
		if (tml->current_stripable() == main_out) {
			/* back to previous layout */
			set_current_layout (_previous_layout);
		} else {
			ControlProtocol::set_stripable_selection (main_out);
		}
	}
}

void
Push2::button_quantize ()
{
	access_action ("Editor/quantize");
}

void
Push2::button_duplicate ()
{
	access_action ("Editor/duplicate-range");
}
