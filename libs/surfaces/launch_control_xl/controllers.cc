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
#include "ardour/solo_isolate_control.h"

#include "launch_control_xl.h"

using namespace ArdourSurface;
using namespace ARDOUR;
using namespace PBD;
using std::cerr;

void
LaunchControlXL::build_maps ()
{
	/* Knobs */

	boost::shared_ptr<Knob> knob;

	#define MAKE_KNOB(i,cc,index,a) \
		knob.reset (new Knob ((i), (cc), (index), (a), (*this))); \
		cc_knob_map.insert (std::make_pair (knob->controller_number(), knob)); \
		id_knob_map.insert (std::make_pair (knob->id(), knob))

	MAKE_KNOB (SendA1, 13, 0, &LaunchControlXL::knob_sendA1);
	MAKE_KNOB (SendA2, 14, 1, &LaunchControlXL::knob_sendA2);
	MAKE_KNOB (SendA3, 15, 2, &LaunchControlXL::knob_sendA3);
	MAKE_KNOB (SendA4, 16, 3, &LaunchControlXL::knob_sendA4);
	MAKE_KNOB (SendA5, 17, 4, &LaunchControlXL::knob_sendA5);
	MAKE_KNOB (SendA6, 18, 5, &LaunchControlXL::knob_sendA6);
	MAKE_KNOB (SendA7, 19, 6, &LaunchControlXL::knob_sendA7);
	MAKE_KNOB (SendA8, 20, 7, &LaunchControlXL::knob_sendA8);

	MAKE_KNOB (SendB1, 29, 8, &LaunchControlXL::knob_sendB1);
	MAKE_KNOB (SendB2, 30, 9, &LaunchControlXL::knob_sendB2);
	MAKE_KNOB (SendB3, 31, 10, &LaunchControlXL::knob_sendB3);
	MAKE_KNOB (SendB4, 32, 11, &LaunchControlXL::knob_sendB4);
	MAKE_KNOB (SendB5, 33, 12, &LaunchControlXL::knob_sendB5);
	MAKE_KNOB (SendB6, 34, 13, &LaunchControlXL::knob_sendB6);
	MAKE_KNOB (SendB7, 35, 14, &LaunchControlXL::knob_sendB7);
	MAKE_KNOB (SendB8, 36, 15, &LaunchControlXL::knob_sendB8);

	MAKE_KNOB (Pan1, 49, 16, &LaunchControlXL::knob_pan1);
	MAKE_KNOB (Pan2, 50, 17, &LaunchControlXL::knob_pan2);
	MAKE_KNOB (Pan3, 51, 18, &LaunchControlXL::knob_pan3);
	MAKE_KNOB (Pan4, 52, 19, &LaunchControlXL::knob_pan4);
	MAKE_KNOB (Pan5, 53, 20, &LaunchControlXL::knob_pan5);
	MAKE_KNOB (Pan6, 54, 21, &LaunchControlXL::knob_pan6);
	MAKE_KNOB (Pan7, 55, 22, &LaunchControlXL::knob_pan7);
	MAKE_KNOB (Pan8, 56, 23, &LaunchControlXL::knob_pan8);

	/* Faders */

	boost::shared_ptr<Fader> fader;

	#define MAKE_FADER(i,cc,a) \
		fader.reset (new Fader ((i), (cc), (a))); \
		cc_fader_map.insert (std::make_pair (fader->controller_number(), fader)); \
		id_fader_map.insert (std::make_pair (fader->id(), fader))

	MAKE_FADER (Fader1, 77, &LaunchControlXL::fader_1);
	MAKE_FADER (Fader2, 78, &LaunchControlXL::fader_2);
	MAKE_FADER (Fader3, 79, &LaunchControlXL::fader_3);
	MAKE_FADER (Fader4, 80, &LaunchControlXL::fader_4);
	MAKE_FADER (Fader5, 81, &LaunchControlXL::fader_5);
	MAKE_FADER (Fader6, 82, &LaunchControlXL::fader_6);
	MAKE_FADER (Fader7, 83, &LaunchControlXL::fader_7);
	MAKE_FADER (Fader8, 84, &LaunchControlXL::fader_8);


	/* Buttons */

	boost::shared_ptr<ControllerButton> controller_button;
	boost::shared_ptr<NoteButton> note_button;


	#define MAKE_TRACK_BUTTON_PRESS(i,nn,index,color,p) \
		note_button.reset (new TrackButton ((i), (nn), (index), (color), (p), (*this))); \
		nn_note_button_map.insert (std::make_pair (note_button->note_number(), note_button)); \
		id_note_button_map.insert (std::make_pair (note_button->id(), note_button))
	#define MAKE_SELECT_BUTTON_PRESS(i,cc,index,p) \
		controller_button.reset (new SelectButton ((i), (cc), (index), (p), (*this))); \
		cc_controller_button_map.insert (std::make_pair (controller_button->controller_number(), controller_button)); \
		id_controller_button_map.insert (std::make_pair (controller_button->id(), controller_button))
	#define MAKE_TRACK_STATE_BUTTON_PRESS(i,nn,index,p) \
		note_button.reset (new TrackStateButton ((i), (nn), (index), (p), (*this))); \
		nn_note_button_map.insert (std::make_pair (note_button->note_number(), note_button)); \
		id_note_button_map.insert (std::make_pair (note_button->id(), note_button))
		#define MAKE_TRACK_STATE_BUTTON_PRESS_RELEASE_LONG(i,nn,index, p,r,l) \
			note_button.reset (new TrackStateButton ((i), (nn), (index), (p), (r), (l), (*this))); \
			nn_note_button_map.insert (std::make_pair (note_button->note_number(), note_button)); \
			id_note_button_map.insert (std::make_pair (note_button->id(), note_button))


	MAKE_TRACK_BUTTON_PRESS(Focus1, 41, 24, YellowLow, &LaunchControlXL::button_track_focus_1);
	MAKE_TRACK_BUTTON_PRESS(Focus2, 42, 25, YellowLow, &LaunchControlXL::button_track_focus_2);
	MAKE_TRACK_BUTTON_PRESS(Focus3, 43, 26, YellowLow, &LaunchControlXL::button_track_focus_3);
	MAKE_TRACK_BUTTON_PRESS(Focus4, 44, 27, YellowLow, &LaunchControlXL::button_track_focus_4);
	MAKE_TRACK_BUTTON_PRESS(Focus5, 57, 28, YellowLow, &LaunchControlXL::button_track_focus_5);
	MAKE_TRACK_BUTTON_PRESS(Focus6, 58, 29, YellowLow, &LaunchControlXL::button_track_focus_6);
	MAKE_TRACK_BUTTON_PRESS(Focus7, 59, 30, YellowLow, &LaunchControlXL::button_track_focus_7);
	MAKE_TRACK_BUTTON_PRESS(Focus8, 60, 31, YellowLow, &LaunchControlXL::button_track_focus_8);
	MAKE_TRACK_BUTTON_PRESS(Control1, 73, 32, AmberLow, &LaunchControlXL::button_track_control_1);
	MAKE_TRACK_BUTTON_PRESS(Control2, 74, 33, AmberLow, &LaunchControlXL::button_track_control_2);
	MAKE_TRACK_BUTTON_PRESS(Control3, 75, 34, AmberLow, &LaunchControlXL::button_track_control_3);
	MAKE_TRACK_BUTTON_PRESS(Control4, 76, 35, AmberLow, &LaunchControlXL::button_track_control_4);
	MAKE_TRACK_BUTTON_PRESS(Control5, 89, 36, AmberLow, &LaunchControlXL::button_track_control_5);
	MAKE_TRACK_BUTTON_PRESS(Control6, 90, 37, AmberLow, &LaunchControlXL::button_track_control_6);
	MAKE_TRACK_BUTTON_PRESS(Control7, 91, 38, AmberLow, &LaunchControlXL::button_track_control_7);
	MAKE_TRACK_BUTTON_PRESS(Control8, 92, 39, AmberLow, &LaunchControlXL::button_track_control_8);

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

boost::shared_ptr<LaunchControlXL::TrackButton>
LaunchControlXL::track_button_by_range(uint8_t n, uint8_t first, uint8_t middle)
{
	NNNoteButtonMap::iterator b;
	if ( n < 4)	{
	 	b = nn_note_button_map.find (first + n);
	} else {
		b = nn_note_button_map.find (middle + n - 4);
	}

	if (b != nn_note_button_map.end()) {
		return boost::dynamic_pointer_cast<TrackButton> (b->second);
	}

	return boost::shared_ptr<TrackButton>();
}

void
LaunchControlXL::update_track_focus_led(uint8_t n)
{
	boost::shared_ptr<TrackButton> b = focus_button_by_column(n);

	if (!b) {
		return;
	}

	if (stripable[n]) {
		if ( stripable[n]->is_selected() ) {
			b->set_color(YellowFull);
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

boost::shared_ptr<LaunchControlXL::Knob>*
LaunchControlXL::knobs_by_column(uint8_t col, boost::shared_ptr<Knob>* knob_col)
{
	for (uint8_t n = 0; n < 3; ++n) {
		knob_col[n] = id_knob_map.find(static_cast<KnobID>(col+n*8))->second;
	}

	return knob_col;
}

void
LaunchControlXL::update_knob_led(uint8_t n)
{
	LEDColor color;

	uint32_t absolute_strip_num = (n + bank_start) % 8;

	if (stripable[n]) {
		switch (absolute_strip_num) {
			case 0:
			case 4:
				if (stripable[n]->is_selected()) {
					color = RedFull;
				} else {
					color = RedLow;
				}
				break;

			case 1:
			case 5:
				if (stripable[n]->is_selected()) {
					color = YellowFull;
				} else {
					color = YellowLow;
				}
				break;

			case 2:
			case 6:
				if (stripable[n]->is_selected()) {
					color = GreenFull;
				} else {
					color = GreenLow;
				}
				break;

			case 3:
			case 7:
				if (stripable[n]->is_master()) {
					color = RedFull;
				} else {
					if (stripable[n]->is_selected()) {
						color = AmberFull;
					} else {
						color = AmberLow;
					}
				}
		}
	}

	boost::shared_ptr<Knob> knobs_col[3];
	knobs_by_column(n, knobs_col);

	for  (uint8_t s = 0; s < 3; ++s)
	{
		if (stripable[n]) {
			knobs_col[s]->set_color(color);
		} else {
			knobs_col[s]->set_color(Off);
		}
		write (knobs_col[s]->state_msg());
	}
}

void
LaunchControlXL::update_track_control_led(uint8_t n)
{
	boost::shared_ptr<TrackButton> b = control_button_by_column(n);

	if (!b) {
		return;
	}

	if (stripable[n]) {
		boost::shared_ptr<AutomationControl> ac = get_ac_by_state(n);

		switch(track_mode()) {
			case TrackMute:
				if (ac->get_value()) {
					b->set_color(YellowFull);
				} else {
					b->set_color(AmberLow);
				}
				break;
			case TrackSolo:
				if (ac && !(stripable[n]->is_master())) {
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
LaunchControlXL::solo_iso_changed(uint32_t n)
{
	if (!stripable[n]) {
		return;
	} else {
		solo_iso_led_bank();
	}
}

void
LaunchControlXL::solo_iso_led_bank ()
{
	int stripable_counter = get_amount_of_tracks();

	if (!(buttons_down.find(Device) != buttons_down.end())) {
		return;
	} else {
		for (int n = 0; n < stripable_counter; ++n) {
			boost::shared_ptr<TrackButton> b = focus_button_by_column(n);
			if (stripable[n] && stripable[n]->solo_isolate_control()) {
				if (stripable[n]->solo_isolate_control()->get_value()) {
					b->set_color(RedFull);
				} else {
					b->set_color(Off);
				}
				write (b->state_msg());
			}
		}
		LaunchControlXL::set_refresh_leds_flag(true);
	}
}

#ifdef MIXBUS
void
LaunchControlXL::master_send_changed(uint32_t n)
{
	if (!stripable[n]) {
		return;
	} else {
		master_send_led_bank();
	}

}

void
LaunchControlXL::master_send_led_bank ()
{
	if (!(buttons_down.find(Device) != buttons_down.end())) {
		return;
	} else {
		int stripable_counter = LaunchControlXL::get_amount_of_tracks();

		for (int n = 0; n < stripable_counter; ++n) {
			boost::shared_ptr<TrackButton> b = control_button_by_column(n);
			if (stripable[n] && stripable[n]->master_send_enable_controllable()) {
				if (stripable[n]->master_send_enable_controllable()->get_value()) {
					b->set_color(GreenFull);
				} else {
					b->set_color(Off);
				}
			}
			write (b->state_msg());
		}
		LaunchControlXL::set_refresh_leds_flag(true);
	}
}
# endif

void
LaunchControlXL::fader(uint8_t n)
{
	if (!stripable[n]) {
		return;
	}

	boost::shared_ptr<Fader> fader = 0;
	IDFaderMap::iterator f = id_fader_map.find(static_cast<FaderID>(n));

	if (f != id_fader_map.end()) {
		fader = f->second;
	}

	if (!fader) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac = stripable[fader->id()]->gain_control();
	if (ac && check_pick_up(fader, ac)) {
		ac->set_value ( ac->interface_to_internal( fader->value() / 127.0), PBD::Controllable::UseGroup );
	}
}

void
LaunchControlXL::knob_sendA(uint8_t n)
{
	if (!stripable[n]) {
		return;
	}

	boost::shared_ptr<Knob> knob = 0;
	IDKnobMap::iterator k = id_knob_map.find(static_cast<KnobID>(n));

	if (k != id_knob_map.end()) {
		knob = k->second;
	}

	if (!knob) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac;

	if (buttons_down.find(Device) != buttons_down.end()) { // Device button hold
		ac = stripable[n]->trim_control();
	} else {
		ac = stripable[n]->send_level_controllable (0);
	}

	if (ac && check_pick_up(knob, ac)) {
		ac->set_value ( ac->interface_to_internal( knob->value() / 127.0), PBD::Controllable::UseGroup );
	}
}

void
LaunchControlXL::knob_sendB(uint8_t n)
{
	if (!stripable[n]) {
		return;
	}

	boost::shared_ptr<Knob> knob = 0;
	IDKnobMap::iterator k = id_knob_map.find(static_cast<KnobID>(n + 8));

	if (k != id_knob_map.end()) {
		knob = k->second;
	}

	if (!knob) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac;

	if (buttons_down.find(Device) != buttons_down.end()) { // Device button hold
	#ifdef MIXBUS
		ac = stripable[n]->filter_freq_controllable (true);
	#else
		/* something */
	#endif
	} else {
		ac = stripable[n]->send_level_controllable (1);
	}

	if (ac && check_pick_up(knob, ac)) {
		ac->set_value ( ac->interface_to_internal( knob->value() / 127.0), PBD::Controllable::UseGroup );
	}
}

void
LaunchControlXL::knob_pan(uint8_t n)
{
	if (!stripable[n]) {
		return;
	}

	boost::shared_ptr<Knob> knob = 0;
	IDKnobMap::iterator k = id_knob_map.find(static_cast<KnobID>(n + 16));

	if (k != id_knob_map.end()) {
		knob = k->second;
	}

	if (!knob) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac;

	if (buttons_down.find(Device) != buttons_down.end()) { // Device button hold
	#ifdef MIXBUS
		ac = stripable[n]->comp_threshold_controllable();
	#else
		ac = stripable[n]->pan_width_control();
	#endif
	} else {
		ac = stripable[n]->pan_azimuth_control();
	}

	if (ac && check_pick_up(knob, ac)) {
		ac->set_value ( ac->interface_to_internal( knob->value() / 127.0), PBD::Controllable::UseGroup );
	}
}

void
LaunchControlXL::button_track_focus(uint8_t n)
{
	if (buttons_down.find(Device) != buttons_down.end()) {
		DEBUG_TRACE (DEBUG::LaunchControlXL, "DEVICE BUTTON HOLD\n");
		if (stripable[n]->solo_isolate_control()) {
			bool solo_isolate_active = stripable[n]->solo_isolate_control()->get_value();
			stripable[n]->solo_isolate_control()->set_value (!solo_isolate_active, PBD::Controllable::UseGroup);
		}
		return;
	}

	if (stripable[n]) {
		if ( stripable[n]->is_selected() ) {
			 ControlProtocol::RemoveStripableFromSelection (stripable[n]);
		} else {
			ControlProtocol::AddStripableToSelection (stripable[n]);
		}
	} else {
		return;
	}
}

void
LaunchControlXL::button_track_control(uint8_t n) {
	if (!stripable[n]) {
		return;
	}

	if (buttons_down.find(Device) != buttons_down.end()) {
		DEBUG_TRACE (DEBUG::LaunchControlXL, "DEVICE BUTTON HOLD\n");
#ifdef MIXBUS
		if (stripable[n]->master_send_enable_controllable()) {
			bool master_send_active = stripable[n]->master_send_enable_controllable()->get_value();

			DEBUG_TRACE (DEBUG::LaunchControlXL, "MIXBUS Master Assign\n");
			stripable[n]->master_send_enable_controllable()->set_value (!master_send_active, PBD::Controllable::UseGroup);
		}

#else
		/* something useful for Ardour */
#endif
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

		boost::shared_ptr<TrackStateButton> mute = boost::dynamic_pointer_cast<TrackStateButton> (id_note_button_map[Mute]);
		boost::shared_ptr<TrackStateButton> solo = boost::dynamic_pointer_cast<TrackStateButton> (id_note_button_map[Solo]);
		boost::shared_ptr<TrackStateButton> record = boost::dynamic_pointer_cast<TrackStateButton> (id_note_button_map[Record]);

		write(mute->state_msg((state == TrackMute)));
		write(solo->state_msg((state == TrackSolo)));
		write(record->state_msg((state == TrackRecord)));
}

void
LaunchControlXL::button_select_left()
{
	switch_bank (max (0, bank_start - (7 + (fader8master() ? 0 : 1))));
}

void
LaunchControlXL::button_select_right()
{
	switch_bank (max (0, bank_start + 7 + (fader8master() ? 0 : 1)));
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
	solo_iso_led_bank();
#ifdef MIXBUS
	master_send_led_bank();
#endif
}

void
LaunchControlXL::button_mute()
{
	if (buttons_down.find(Device) != buttons_down.end()) {
		access_action ("Editor/track-mute-toggle");
	} else {
		button_track_mode(TrackMute);
	}
}

void
LaunchControlXL::button_solo()
{
	if (buttons_down.find(Device) != buttons_down.end()) {
		access_action ("Editor/track-solo-toggle");
	} else {
		button_track_mode(TrackSolo);
	}
}

void
LaunchControlXL::button_record()
{
	if (buttons_down.find(Device) != buttons_down.end()) {
		access_action ("Editor/track-record-enable-toggle");
	} else {
		button_track_mode(TrackRecord);
	}
}

bool
LaunchControlXL::button_long_press_timeout (ButtonID id, boost::shared_ptr<Button> button)
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
LaunchControlXL::start_press_timeout (boost::shared_ptr<Button> button, ButtonID id)
{
	Glib::RefPtr<Glib::TimeoutSource> timeout = Glib::TimeoutSource::create (500); // milliseconds
	button->timeout_connection = timeout->connect (sigc::bind (sigc::mem_fun (*this, &LaunchControlXL::button_long_press_timeout), id, button));
	timeout->attach (main_loop()->get_context());
}
