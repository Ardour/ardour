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

#include <algorithm>

#include "ardour/debug.h"
#include "ardour/mute_control.h"
#include "ardour/session.h"
#include "ardour/solo_control.h"

#include "launch_control_xl.h"

using namespace ArdourSurface;
using namespace ARDOUR;
using namespace PBD;
using std::cerr;

void
LaunchControlXL::build_maps ()
{
	/* Knobs */

	Knob* knob;

	#define MAKE_KNOB(i,cc, index, color) \
		knob = new Knob ((i), (cc), (index), (color), (*this)); \
		cc_knob_map.insert (std::make_pair (knob->controller_number(), knob)); \
		id_knob_map.insert (std::make_pair (knob->id(), knob))

		for (uint8_t n = 0; n < 8; ++n) {
			MAKE_KNOB (static_cast<KnobID>(n), (n + 13), n, RedFull);
			MAKE_KNOB (static_cast<KnobID>(n + 8), (n + 29), (n + 8), GreenFull);
			MAKE_KNOB (static_cast<KnobID>(n + 16), (n + 49), (n + 16), Yellow);
		}

	/* Faders */

	Fader* fader;

	#define MAKE_FADER(i,cc) \
		fader = new Fader ((i), (cc)); \
		cc_fader_map.insert (std::make_pair (fader->controller_number(), fader)); \
		id_fader_map.insert (std::make_pair (fader->id(), fader))

		for (uint8_t n = 0; n < 8; ++n) {
			MAKE_FADER (static_cast<FaderID>(n), (n + 77) );
		}

	/* Buttons */

	ControllerButton *controller_button;
	NoteButton *note_button;


	#define MAKE_TRACK_BUTTON_PRESS(i,nn,index,color,p) \
		note_button = new TrackButton ((i), (nn), (index), (color), (p), (*this)); \
		nn_note_button_map.insert (std::make_pair (note_button->note_number(), note_button)); \
		id_note_button_map.insert (std::make_pair (note_button->id(), note_button))
	#define MAKE_SELECT_BUTTON_PRESS(i,cc,index,p) \
		controller_button = new SelectButton ((i), (cc), (index), (p), (*this)); \
		cc_controller_button_map.insert (std::make_pair (controller_button->controller_number(), controller_button)); \
		id_controller_button_map.insert (std::make_pair (controller_button->id(), controller_button))
	#define MAKE_TRACK_STATE_BUTTON_PRESS(i,nn,index,p) \
		note_button = new TrackStateButton ((i), (nn), (index), (p), (*this)); \
		nn_note_button_map.insert (std::make_pair (note_button->note_number(), note_button)); \
		id_note_button_map.insert (std::make_pair (note_button->id(), note_button))
		#define MAKE_TRACK_STATE_BUTTON_PRESS_RELEASE_LONG(i,nn,index, p,r,l) \
			note_button = new TrackStateButton ((i), (nn), (index), (p), (r), (l), (*this)); \
			nn_note_button_map.insert (std::make_pair (note_button->note_number(), note_button)); \
			id_note_button_map.insert (std::make_pair (note_button->id(), note_button))


	MAKE_TRACK_BUTTON_PRESS(Focus1, 41, 24, GreenFull, &LaunchControlXL::button_track_focus_1);
	MAKE_TRACK_BUTTON_PRESS(Focus2, 42, 25, GreenFull, &LaunchControlXL::button_track_focus_2);
	MAKE_TRACK_BUTTON_PRESS(Focus3, 43, 26, GreenFull, &LaunchControlXL::button_track_focus_3);
	MAKE_TRACK_BUTTON_PRESS(Focus4, 44, 27, GreenFull, &LaunchControlXL::button_track_focus_4);
	MAKE_TRACK_BUTTON_PRESS(Focus5, 57, 28, GreenFull, &LaunchControlXL::button_track_focus_5);
	MAKE_TRACK_BUTTON_PRESS(Focus6, 58, 29, GreenFull, &LaunchControlXL::button_track_focus_6);
	MAKE_TRACK_BUTTON_PRESS(Focus7, 59, 30, GreenFull, &LaunchControlXL::button_track_focus_7);
	MAKE_TRACK_BUTTON_PRESS(Focus8, 60, 31, GreenFull, &LaunchControlXL::button_track_focus_8);
	MAKE_TRACK_BUTTON_PRESS(Control1, 73, 32, Yellow, &LaunchControlXL::button_track_control_1);
	MAKE_TRACK_BUTTON_PRESS(Control2, 74, 33, Yellow, &LaunchControlXL::button_track_control_2);
	MAKE_TRACK_BUTTON_PRESS(Control3, 75, 34, Yellow, &LaunchControlXL::button_track_control_3);
	MAKE_TRACK_BUTTON_PRESS(Control4, 76, 35, Yellow, &LaunchControlXL::button_track_control_4);
	MAKE_TRACK_BUTTON_PRESS(Control5, 89, 36, Yellow, &LaunchControlXL::button_track_control_5);
	MAKE_TRACK_BUTTON_PRESS(Control6, 90, 37, Yellow, &LaunchControlXL::button_track_control_6);
	MAKE_TRACK_BUTTON_PRESS(Control7, 91, 38, Yellow, &LaunchControlXL::button_track_control_7);
	MAKE_TRACK_BUTTON_PRESS(Control8, 92, 39, Yellow, &LaunchControlXL::button_track_control_8);

	MAKE_SELECT_BUTTON_PRESS(SelectUp, 104, 44, &LaunchControlXL::button_select_up);
	MAKE_SELECT_BUTTON_PRESS(SelectDown, 105, 45, &LaunchControlXL::button_select_down);
	MAKE_SELECT_BUTTON_PRESS(SelectLeft, 106, 46, &LaunchControlXL::button_select_left);
	MAKE_SELECT_BUTTON_PRESS(SelectRight, 107, 47, &LaunchControlXL::button_select_right);

	MAKE_TRACK_STATE_BUTTON_PRESS_RELEASE_LONG(Device, 105, 40, &LaunchControlXL::relax, &LaunchControlXL::button_device, &LaunchControlXL::button_device_long_press);;
	MAKE_TRACK_STATE_BUTTON_PRESS(Mute, 106, 41, &LaunchControlXL::button_mute);
	MAKE_TRACK_STATE_BUTTON_PRESS(Solo, 107, 42, &LaunchControlXL::button_solo);
	MAKE_TRACK_STATE_BUTTON_PRESS(Record, 108, 43, &LaunchControlXL::button_record);

}

std::string
LaunchControlXL::button_name_by_id (ButtonID id)
{
	switch (id) {
		case Device:
			return "Device";
		case Mute:
			return "Mute";
		case Solo:
			return "Solo";
		case Record:
			return "Record";
		case SelectUp:
			return "Select Up";
		case SelectDown:
			return "Select Down";
		case SelectRight:
			return "Select Right";
		case SelectLeft:
			return "Select Left";
		case Focus1:
			return "Focus 1";
		case Focus2:
			return "Focus 2";
		case Focus3:
			return "Focus 3";
		case Focus4:
			return "Focus 4";
		case Focus5:
			return "Focus 5";
		case Focus6:
			return "Focus 6";
		case Focus7:
			return "Focus 7";
		case Focus8:
			return "Focus 8";
		case Control1:
			return "Control 1";
		case Control2:
			return "Control 2";
		case Control3:
			return "Control 3";
		case Control4:
			return "Control 4";
		case Control5:
			return "Control 5";
		case Control6:
			return "Control 6";
		case Control7:
			return "Control 7";
		case Control8:
			return "Control 8";
	default:
		break;
	}

	return "???";
}

std::string
LaunchControlXL::knob_name_by_id (KnobID id)
{
	switch (id) {
		case SendA1:
			return "SendA 1";
		case SendA2:
			return "SendA 2";
		case SendA3:
			return "SendA 3";
		case SendA4:
			return "SendA 4";
		case SendA5:
			return "SendA 5";
		case SendA6:
			return "SendA 6";
		case SendA7:
			return "SendA 7";
		case SendA8:
			return "SendA 8";
		case SendB1:
			return "SendB 1";
		case SendB2:
			return "SendB 2";
		case SendB3:
			return "SendB 3";
		case SendB4:
			return "SendB 4";
		case SendB5:
			return "SendB 5";
		case SendB6:
			return "SendB 6";
		case SendB7:
			return "SendB 7";
		case SendB8:
			return "SendB 8";
		case Pan1:
			return "Pan 1";
		case Pan2:
			return "Pan 2";
		case Pan3:
			return "Pan 3";
		case Pan4:
			return "Pan 4";
		case Pan5:
			return "Pan 5";
		case Pan6:
			return "Pan 6";
		case Pan7:
			return "Pan 7";
		case Pan8:
			return "Pan 8";
	default:
		break;
	}

	return "???";
}

std::string
LaunchControlXL::fader_name_by_id (FaderID id)
{
	switch (id) {
		case Fader1:
			return "Fader 1";
		case Fader2:
			return "Fader 2";
		case Fader3:
			return "Fader 3";
		case Fader4:
			return "Fader 4";
		case Fader5:
			return "Fader 5";
		case Fader6:
			return "Fader 6";
		case Fader7:
			return "Fader 7";
		case Fader8:
			return "Fader 8";
	default:
		break;
	}

	return "???";
}

LaunchControlXL::TrackButton*
LaunchControlXL::track_button_by_number(uint8_t n, uint8_t first, uint8_t middle)
{
	NNNoteButtonMap::iterator b;
	if ( n < 4)	{
	 	b = nn_note_button_map.find (first + n);
	} else {
		b = nn_note_button_map.find (middle + n - 4);
	}

	TrackButton* button = 0;

	if (b != nn_note_button_map.end()) {
		button = static_cast<TrackButton*>(b->second);
	}

	return button;

}

void
LaunchControlXL::button_track_focus(uint8_t n)
{
	TrackButton* b = focus_button_by_number(n);

	if (!b) {
		return;
	}

	if (stripable[n]) {
		if ( stripable[n]->is_selected() ) {
			b->set_color(AmberFull);
		} else {
			b->set_color(AmberLow);
		}
	} else {
		b->set_color(Off);
	}

	write (b->state_msg());
}

boost::shared_ptr<AutomationControl>
LaunchControlXL::get_ac_by_state(uint8_t n) {
		boost::shared_ptr<AutomationControl> ac;

		switch(track_mode()) {
			case TrackMute:
				ac = stripable[n]->mute_control();
				break;

			case TrackSolo:
				ac = stripable[n]->solo_control();
				break;

			case TrackRecord:
				ac = stripable[n]->rec_enable_control();
				break;

			default:
			break;
		}
		return ac;
}


void
LaunchControlXL::update_track_control_led(uint8_t n)
{
	TrackButton* b = control_button_by_number(n);

	if (!b) {
		return;
	}

	if (stripable[n]) {
		boost::shared_ptr<AutomationControl> ac = get_ac_by_state(n);

		switch(track_mode()) {
			case TrackMute:
				if (ac->get_value()) {
					b->set_color(AmberFull);
				} else {
					b->set_color(AmberLow);
				}
				break;
			case TrackSolo:
				if (ac && stripable[n] != master ) {
					if (ac->get_value()) {
						b->set_color(GreenFull);
					} else {
						b->set_color(GreenLow);
					}
				} else {
					b->set_color(Off);
				}
				break;
			case TrackRecord:
				if (ac) {
					if (ac->get_value()) {
						b->set_color(RedFull);
					} else {
						b->set_color(RedLow);
					}
				} else {
					b->set_color(Off);
				}
				break;

			default:
			break;
		}
	} else {
		b->set_color(Off);
	}

	write (b->state_msg());
}

void
LaunchControlXL::solo_mute_rec_changed(uint32_t n) {
	if (!stripable[n]) {
		return;
	}
	update_track_control_led(n);
}

void
LaunchControlXL::button_track_control(uint8_t n) {
	if (!stripable[n]) {
		return;
	}
	boost::shared_ptr<AutomationControl> ac = get_ac_by_state(n);

	if (ac) {
		session->set_control (ac, !ac->get_value(), PBD::Controllable::UseGroup);
	}
}

void
LaunchControlXL::button_track_mode(TrackMode state)
{
		set_track_mode(state);
		for (uint8_t n = 0; n < 8; ++n) {
			update_track_control_led(n);
		}

		TrackStateButton* mute = static_cast<TrackStateButton*>(id_note_button_map[Mute]);
		TrackStateButton* solo = static_cast<TrackStateButton*>(id_note_button_map[Solo]);
		TrackStateButton* record = static_cast<TrackStateButton*>(id_note_button_map[Record]);

		write(mute->state_msg( (state == TrackMute) ));
		write(solo->state_msg( (state == TrackSolo) ));
		write(record->state_msg( (state == TrackRecord) ));
}

void
LaunchControlXL::button_select_left()
{
	switch_bank (max (0, bank_start - 1));
}

void
LaunchControlXL::button_select_right()
{
	switch_bank (max (0, bank_start + 1));
}

void
LaunchControlXL::button_select_up()
{

}

void
LaunchControlXL::button_select_down()
{

}

void
LaunchControlXL::button_device()
{

}

void
LaunchControlXL::button_device_long_press()
{

}

bool
LaunchControlXL::button_long_press_timeout (ButtonID id, Button* button)
{
	if (buttons_down.find (id) != buttons_down.end()) {
		DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("long press timeout for %1, invoking method\n", id));
		(this->*button->long_press_method) ();
	} else {
		DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("long press timeout for %1, expired/cancelled\n", id));
		/* release happened and somehow we were not cancelled */
	}

	/* whichever button this was, we've used it ... don't invoke the
	   release action.
	*/
	consumed.insert (id);

	return false; /* don't get called again */
}


void
LaunchControlXL::start_press_timeout (Button* button, ButtonID id)
{
	Glib::RefPtr<Glib::TimeoutSource> timeout = Glib::TimeoutSource::create (500); // milliseconds
	button->timeout_connection = timeout->connect (sigc::bind (sigc::mem_fun (*this, &LaunchControlXL::button_long_press_timeout), id, button));
	timeout->attach (main_loop()->get_context());
}
