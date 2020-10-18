/*
 * Copyright (C) 2018-2019 Jan Lentfer <jan.lentfer@web.de>
 * Copyright (C) 2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Térence Clastres <t.clastres@gmail.com>
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
#include "ardour/solo_isolate_control.h"

#include "launch_control_xl.h"

using namespace ArdourSurface;
using namespace ARDOUR;
using namespace PBD;
using std::cerr;

void
LaunchControlXL::build_maps ()
{
	cc_knob_map.clear();
	id_knob_map.clear();
	cc_fader_map.clear();
	id_fader_map.clear();
	nn_note_button_map.clear();
	id_note_button_map.clear();
	cc_controller_button_map.clear();
	id_controller_button_map.clear();

	/* Knobs */
	boost::shared_ptr<Knob> knob;

	#define MAKE_KNOB(i,cc,index,c_on,c_off,a) \
		knob.reset (new Knob ((i), (cc), (index), (c_on), (c_off), (a), (*this))); \
		cc_knob_map.insert (std::make_pair (knob->controller_number(), knob)); \
		id_knob_map.insert (std::make_pair (knob->id(), knob));
	#define MAKE_DM_KNOB(i,cc,index,c_on,c_off,action,check) \
		knob.reset (new Knob ((i), (cc), (index), (c_on), (c_off), (action), (check), (*this))); \
		cc_knob_map.insert (std::make_pair (knob->controller_number(), knob)); \
		id_knob_map.insert (std::make_pair (knob->id(), knob));

	if (!device_mode()) {	/* mixer mode */
		MAKE_KNOB (SendA1, 13, 0, RedFull, RedLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_sendA, this, 0)));
		MAKE_KNOB (SendA2, 14, 1, YellowFull, YellowLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_sendA, this, 1)));
		MAKE_KNOB (SendA3, 15, 2, GreenFull, GreenLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_sendA, this, 2)));
		MAKE_KNOB (SendA4, 16, 3, AmberFull, AmberLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_sendA, this, 3)));
		MAKE_KNOB (SendA5, 17, 4, RedFull, RedLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_sendA, this, 4)));
		MAKE_KNOB (SendA6, 18, 5, YellowFull, YellowLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_sendA, this, 5)));
		MAKE_KNOB (SendA7, 19, 6, GreenFull, GreenLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_sendA, this, 6)));
		MAKE_KNOB (SendA8, 20, 7, AmberFull, AmberLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_sendA, this, 7)));

		MAKE_KNOB (SendB1, 29, 8, RedFull, RedLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_sendB, this, 0)));
		MAKE_KNOB (SendB2, 30, 9, YellowFull, YellowLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_sendB, this, 1)));
		MAKE_KNOB (SendB3, 31, 10, GreenFull, GreenLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_sendB, this, 2)));
		MAKE_KNOB (SendB4, 32, 11, AmberFull, AmberLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_sendB, this, 3)));
		MAKE_KNOB (SendB5, 33, 12, RedFull, RedLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_sendB, this, 4)));
		MAKE_KNOB (SendB6, 34, 13, YellowFull, YellowLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_sendB, this, 5)));
		MAKE_KNOB (SendB7, 35, 14, GreenFull, GreenLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_sendB, this, 6)));
		MAKE_KNOB (SendB8, 36, 15, AmberFull, AmberLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_sendB, this, 7)));

		MAKE_KNOB (Pan1, 49, 16, RedFull, RedLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_pan, this, 0)));
		MAKE_KNOB (Pan2, 50, 17, YellowFull, YellowLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_pan, this, 1)));
		MAKE_KNOB (Pan3, 51, 18, GreenFull, GreenLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_pan, this, 2)));
		MAKE_KNOB (Pan4, 52, 19, AmberFull, AmberLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_pan, this, 3)));
		MAKE_KNOB (Pan5, 53, 20, RedFull, RedLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_pan, this, 4)));
		MAKE_KNOB (Pan6, 54, 21, YellowFull, YellowLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_pan, this, 5)));
		MAKE_KNOB (Pan7, 55, 22, GreenFull, GreenLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_pan, this, 6)));
		MAKE_KNOB (Pan8, 56, 23, AmberFull, AmberLow, boost::function<void ()> (boost::bind (&LaunchControlXL::knob_pan, this, 7)));

	} else {	/* device mode */

#ifdef MIXBUS // from here Mixbus Standard + 32C
		MAKE_DM_KNOB (SendB5, 33, 12, RedFull, RedLow, boost::function<void ()>
				(boost::bind (&LaunchControlXL::dm_trim, this, SendB5)),
				boost::bind(&LaunchControlXL::dm_check_trim, this));
		MAKE_DM_KNOB (SendB6, 34, 13, GreenFull, GreenLow, boost::function<void ()>
				(boost::bind (&LaunchControlXL::dm_mb_comp, this, SendB6, CompMakeup)),
				boost::bind(&LaunchControlXL::dm_mb_comp_enabled,this));
		MAKE_DM_KNOB (SendB7, 35, 14, GreenFull, GreenLow, boost::function<void ()>
				(boost::bind (&LaunchControlXL::dm_mb_comp, this, SendB7, CompSpeed)),
				boost::bind(&LaunchControlXL::dm_mb_comp_enabled, this));
		MAKE_DM_KNOB (SendB8, 36, 15, GreenFull, GreenLow, boost::function<void ()>
				(boost::bind (&LaunchControlXL::dm_mb_comp, this, SendB8, CompMode)),
				boost::bind(&LaunchControlXL::dm_mb_comp_enabled, this));

		/* Pan Knobs -> Sends */
		for (uint8_t i = 0; i < 8; ++i) {
			MAKE_DM_KNOB (static_cast<KnobID>(i + 16), (i + 49), (i + 16), GreenLow, YellowLow, boost::function<void()>
				(boost::bind (&LaunchControlXL::dm_mb_sends, this, static_cast<KnobID>(i + 16))),
				boost::function<uint8_t ()> (boost::bind(&LaunchControlXL::dm_mb_check_send_knob, this, static_cast<KnobID>(i + 16))));
		}

#ifdef MIXBUS32C // from here 32C only

		if (first_selected_stripable() && (first_selected_stripable()->is_master() || first_selected_stripable()->mixbus())) {
			MAKE_DM_KNOB (SendA1, 13, 0, AmberFull, AmberLow, boost::function<void ()>
					(boost::bind (&LaunchControlXL::dm_mb_eq, this, SendA1, true, 0)),
					boost::function<uint8_t ()> (boost::bind(&LaunchControlXL::dm_mb_eq_gain_enabled, this, 0)));
			MAKE_DM_KNOB (SendA2, 14, 1, AmberFull, AmberLow, boost::function<void ()>
					(boost::bind (&LaunchControlXL::dm_mb_eq, this, SendA2, true, 1)),
					boost::function<uint8_t ()> (boost::bind(&LaunchControlXL::dm_mb_eq_gain_enabled, this, 1)));
			MAKE_DM_KNOB (SendA3, 15, 2, AmberFull, AmberLow, boost::function<void ()>
					(boost::bind (&LaunchControlXL::dm_mb_eq, this, SendA3, true, 2)),
					boost::function<uint8_t ()> (boost::bind(&LaunchControlXL::dm_mb_eq_gain_enabled, this, 2)));
			MAKE_DM_KNOB (SendA6, 18, 5, RedLow, RedLow, boost::function<void ()>
					(boost::bind (&LaunchControlXL::dm_pan_width, this, SendA6)),
					boost::bind(&LaunchControlXL::dm_check_pan_width, this));
			MAKE_DM_KNOB (SendA7, 19, 6, AmberLow, AmberLow, boost::function<void ()>
					(boost::bind (&LaunchControlXL::dm_mb_tapedrive, this, SendA7)),
					boost::bind(&LaunchControlXL::dm_mb_has_tapedrive, this));
		} else {
			MAKE_DM_KNOB (SendA1, 13, 0, AmberFull, AmberLow, boost::function<void ()>
			              (boost::bind (&LaunchControlXL::dm_mb_eq, this, SendA1, false, 0)),
			              boost::bind(&LaunchControlXL::dm_mb_eq_freq_enabled, this));
			MAKE_DM_KNOB (SendA2, 14, 1, AmberFull, AmberLow, boost::function<void ()>
			              (boost::bind (&LaunchControlXL::dm_mb_eq, this, SendA2, true, 0)),
			              boost::function<uint8_t ()> (boost::bind(&LaunchControlXL::dm_mb_eq_gain_enabled, this, 0)));
			MAKE_DM_KNOB (SendA3, 15, 2, YellowLow, AmberLow, boost::function<void ()>
			              (boost::bind (&LaunchControlXL::dm_mb_eq, this, SendA3, false, 1)),
			              boost::bind(&LaunchControlXL::dm_mb_eq_freq_enabled, this));
			MAKE_DM_KNOB (SendA4, 16, 3, YellowLow, AmberLow, boost::function<void ()>
			              (boost::bind (&LaunchControlXL::dm_mb_eq, this, SendA4, true, 1)),
			              boost::function<uint8_t ()> (boost::bind(&LaunchControlXL::dm_mb_eq_gain_enabled, this, 1)));
			MAKE_DM_KNOB (SendA5, 17, 4, AmberFull, AmberLow, boost::function<void ()>
			              (boost::bind (&LaunchControlXL::dm_mb_eq, this, SendA5, false, 2)),
			              boost::bind(&LaunchControlXL::dm_mb_eq_freq_enabled, this));
			MAKE_DM_KNOB (SendA6, 18, 5, AmberFull, AmberLow, boost::function<void ()>
			              (boost::bind (&LaunchControlXL::dm_mb_eq, this, SendA6, true, 2)),
			              boost::function<uint8_t ()> (boost::bind(&LaunchControlXL::dm_mb_eq_gain_enabled, this, 2)));
			MAKE_DM_KNOB (SendA7, 19, 6, YellowLow, AmberLow, boost::function<void ()>
			              (boost::bind (&LaunchControlXL::dm_mb_eq, this, SendA7, false, 3)),
			              boost::bind(&LaunchControlXL::dm_mb_eq_freq_enabled, this));
			MAKE_DM_KNOB (SendA8, 20, 7, YellowLow, AmberLow, boost::function<void ()>
			              (boost::bind (&LaunchControlXL::dm_mb_eq, this, SendA8, true, 3)),
			              boost::function<uint8_t ()> (boost::bind(&LaunchControlXL::dm_mb_eq_gain_enabled, this, 3)));
		}
			MAKE_DM_KNOB (SendB1, 29, 8, YellowFull, AmberLow, boost::function<void ()>
			              (boost::bind (&LaunchControlXL::dm_mb_flt_frq, this, SendB1, true)),
			              boost::bind(&LaunchControlXL::dm_mb_flt_enabled, this));
			MAKE_DM_KNOB (SendB2, 30, 9, YellowFull, AmberLow, boost::function<void ()>
			              (boost::bind (&LaunchControlXL::dm_mb_flt_frq, this, SendB2, false)),
			              boost::bind(&LaunchControlXL::dm_mb_flt_enabled, this));
			MAKE_DM_KNOB (SendB4, 32, 11, RedLow, AmberLow, boost::function<void ()>
			              (boost::bind (&LaunchControlXL::dm_pan_azi, this, SendB4)),
			              boost::bind(&LaunchControlXL::dm_check_pan_azi, this));

#else	// from here Mixbus Standard only
		MAKE_DM_KNOB (SendA1, 13, 0, AmberFull, AmberLow, boost::function<void ()>
				(boost::bind (&LaunchControlXL::dm_mb_eq, this, SendA1, true, 0)),
				boost::function<uint8_t ()> (boost::bind(&LaunchControlXL::dm_mb_eq_gain_enabled, this, 0)));
		MAKE_DM_KNOB (SendA2, 14, 1, AmberFull, AmberLow, boost::function<void ()>
				(boost::bind (&LaunchControlXL::dm_mb_eq, this, SendA2, true, 1)),
				boost::function<uint8_t ()> (boost::bind(&LaunchControlXL::dm_mb_eq_gain_enabled, this, 1)));
		MAKE_DM_KNOB (SendA3, 15, 2, AmberFull, AmberLow, boost::function<void ()>
				(boost::bind (&LaunchControlXL::dm_mb_eq, this, SendA3, true, 2)),
				boost::function<uint8_t ()> (boost::bind(&LaunchControlXL::dm_mb_eq_gain_enabled, this, 2)));
		MAKE_DM_KNOB (SendA5, 17, 4, RedLow, AmberLow, boost::function<void ()>
				(boost::bind (&LaunchControlXL::dm_pan_azi, this, SendA5)),
				boost::bind(&LaunchControlXL::dm_check_pan_azi, this));
		MAKE_DM_KNOB (SendA6, 18, 5, RedLow, RedLow, boost::function<void ()>
				(boost::bind (&LaunchControlXL::dm_pan_width, this, SendA6)),
				boost::bind(&LaunchControlXL::dm_check_pan_width, this));
		MAKE_DM_KNOB (SendA7, 19, 6, AmberLow, AmberLow, boost::function<void ()>
				(boost::bind (&LaunchControlXL::dm_mb_tapedrive, this, SendA7)),
				boost::bind(&LaunchControlXL::dm_mb_has_tapedrive, this));
		MAKE_DM_KNOB (SendB1, 29, 8, YellowFull, AmberLow, boost::function<void ()>
				(boost::bind (&LaunchControlXL::dm_mb_eq, this, SendB1, false, 0)),
				boost::bind(&LaunchControlXL::dm_mb_eq_freq_enabled, this));
		MAKE_DM_KNOB (SendB2, 30, 9, YellowFull, AmberLow, boost::function<void ()>
				(boost::bind (&LaunchControlXL::dm_mb_eq, this, SendB2, false, 1)),
				boost::bind(&LaunchControlXL::dm_mb_eq_freq_enabled, this));
		MAKE_DM_KNOB (SendB3, 31, 10, YellowFull, AmberLow, boost::function<void ()>
				(boost::bind (&LaunchControlXL::dm_mb_eq, this, SendB3, false, 2)),
				boost::bind(&LaunchControlXL::dm_mb_eq_freq_enabled, this));
		MAKE_DM_KNOB (SendB4, 32, 11, YellowFull, AmberLow, boost::function<void ()>
				(boost::bind (&LaunchControlXL::dm_mb_flt_frq, this, SendB4, true)),
				boost::bind(&LaunchControlXL::dm_mb_eq_freq_enabled, this));
#endif
#else // from here Ardour
#endif
	}


	/* Faders */
	boost::shared_ptr<Fader> fader;

	#define MAKE_FADER(i,cc,a) \
		fader.reset (new Fader ((i), (cc), (a))); \
		cc_fader_map.insert (std::make_pair (fader->controller_number(), fader)); \
		id_fader_map.insert (std::make_pair (fader->id(), fader))

	if (!device_mode()) {	/* mix mode */
		for (uint8_t i = 0; i < 8; ++i) {
			MAKE_FADER(static_cast<FaderID>(i), i+77, boost::function<void()>
				(boost::bind (&LaunchControlXL::fader, this, i)));
		}

	} else {	/* device mode */
		MAKE_FADER(Fader1, 77,  boost::function<void()>
			(boost::bind (&LaunchControlXL::dm_fader, this, Fader1)));
#ifdef MIXBUS
		MAKE_FADER(Fader2, 78,  boost::function<void()>
			(boost::bind (&LaunchControlXL::dm_mb_comp_thresh, this, Fader2)));
#endif
	}

	/* Buttons */
	boost::shared_ptr<ControllerButton> controller_button;
	boost::shared_ptr<NoteButton> note_button;


	#define MAKE_TRACK_BUTTON_PRESS(i,nn,index,c_on,c_off,p,check) \
		note_button.reset (new TrackButton ((i), (nn), (index), (c_on), (c_off), (p), \
		boost::bind(&LaunchControlXL::relax, this), boost::bind(&LaunchControlXL::relax, this), (check), (*this))); \
		nn_note_button_map.insert (std::make_pair (note_button->note_number(), note_button)); \
		id_note_button_map.insert (std::make_pair (note_button->id(), note_button))
	#define MAKE_CTRL_BUTTON_PRESS(i,nn,index,c_on,c_off,p,check) \
		note_button.reset (new TrackButton ((i), (nn), (index), (c_on), (c_off),  (p), \
		boost::bind(&LaunchControlXL::relax, this), boost::bind(&LaunchControlXL::relax, this), (check), (*this))); \
		nn_note_button_map.insert (std::make_pair (note_button->note_number(), note_button)); \
		id_note_button_map.insert (std::make_pair (note_button->id(), note_button))
	#define MAKE_SELECT_BUTTON_PRESS(i,cc,index,p) \
		controller_button.reset (new SelectButton ((i), (cc), (index), (p), \
		boost::bind(&LaunchControlXL::relax, this), boost::bind(&LaunchControlXL::relax, this), (*this))); \
		cc_controller_button_map.insert (std::make_pair (controller_button->controller_number(), controller_button)); \
		id_controller_button_map.insert (std::make_pair (controller_button->id(), controller_button))
	#define MAKE_TRACK_STATE_BUTTON_PRESS(i,nn,index,p) \
		note_button.reset (new TrackStateButton ((i), (nn), (index), (p), \
		boost::bind(&LaunchControlXL::relax, this), boost::bind(&LaunchControlXL::relax, this), (*this))); \
		nn_note_button_map.insert (std::make_pair (note_button->note_number(), note_button)); \
		id_note_button_map.insert (std::make_pair (note_button->id(), note_button))
		#define MAKE_TRACK_STATE_BUTTON_PRESS_RELEASE_LONG(i,nn,index, p,r,l) \
			note_button.reset (new TrackStateButton ((i), (nn), (index), (p), (r), (l), (*this))); \
			nn_note_button_map.insert (std::make_pair (note_button->note_number(), note_button)); \
			id_note_button_map.insert (std::make_pair (note_button->id(), note_button))

	/* Track Focus Buttons */
	if (!device_mode()) {	/* mixer mode */
		for (uint8_t i = 0; i < 4; ++i) {
			MAKE_TRACK_BUTTON_PRESS(static_cast<ButtonID>(i), i+41, i+24, YellowFull, YellowLow,
				boost::function<void()>	(boost::bind (&LaunchControlXL::button_track_focus, this, i)),
				boost::function<uint8_t()> (boost::bind (&LaunchControlXL::dm_check_dummy, this, dev_nonexistant)));
		}

		for (uint8_t i = 0; i < 4; ++i) {
			MAKE_TRACK_BUTTON_PRESS(static_cast<ButtonID>(i+4), i+57, i+28, YellowFull, YellowLow,
				boost::function<void()>	(boost::bind (&LaunchControlXL::button_track_focus, this, i+4)),
				boost::function<uint8_t()> (boost::bind (&LaunchControlXL::dm_check_dummy, this, dev_nonexistant)));
		}

	} else {		/* device mode */
#ifdef MIXBUS
		for (uint8_t i = 0; i < 4; ++i) {
			MAKE_TRACK_BUTTON_PRESS(static_cast<ButtonID>(i), i+41, i+24, GreenFull, YellowLow,
				boost::function<void()>	(boost::bind (&LaunchControlXL::dm_mb_send_switch, this, static_cast<ButtonID>(i))),
				boost::function<uint8_t()> (boost::bind (&LaunchControlXL::dm_mb_check_send_button, this, i)));

		}

		for (uint8_t i = 0; i < 4; ++i) {
			MAKE_TRACK_BUTTON_PRESS(static_cast<ButtonID>(i+4), i+57, i+28, GreenFull, YellowLow,
				boost::function<void()>	(boost::bind (&LaunchControlXL::dm_mb_send_switch, this, static_cast<ButtonID>(i+4))),
				boost::function<uint8_t()> (boost::bind (&LaunchControlXL::dm_mb_check_send_button, this, i+4)));
		}
#endif
	}
	/* Track Control Buttons */
	if (!device_mode()) {	/* mixer mode */
		/* Control Buttons in mix mode change their color dynamically so we won't set them here */
		for (uint8_t i = 0; i < 4; ++i) {
			MAKE_CTRL_BUTTON_PRESS(static_cast<ButtonID>(i+8), i+73, i+32, Off, Off,
				boost::function<void()>	(boost::bind (&LaunchControlXL::button_press_track_control, this, i)),
				boost::function<uint8_t()> (boost::bind (&LaunchControlXL::dm_check_dummy, this, dev_nonexistant)));
		}

		for (uint8_t i = 0; i < 4; ++i) {
			MAKE_CTRL_BUTTON_PRESS(static_cast<ButtonID>(i+12), i+89, i+36, Off, Off,
				boost::function<void()>	(boost::bind (&LaunchControlXL::button_press_track_control, this, i+4)),
				boost::function<uint8_t()> (boost::bind (&LaunchControlXL::dm_check_dummy, this, dev_nonexistant)));
		}

	} else { /*device mode */
#ifdef MIXBUS // from here Mixbus Standard + 32C
		MAKE_CTRL_BUTTON_PRESS(Control1, 73, 32, YellowFull, YellowLow, (boost::bind (&LaunchControlXL::dm_mute_switch, this)),
			boost::bind (&LaunchControlXL::dm_mute_enabled, this));
		MAKE_CTRL_BUTTON_PRESS(Control2, 74, 33, GreenFull, GreenLow, (boost::bind (&LaunchControlXL::dm_solo_switch, this)),
			boost::bind (&LaunchControlXL::dm_solo_enabled, this));
		MAKE_CTRL_BUTTON_PRESS(Control3, 75, 34, AmberFull, AmberLow, (boost::bind (&LaunchControlXL::dm_mb_eq_switch, this)),
			boost::function<uint8_t()> (boost::bind(&LaunchControlXL::dm_mb_eq_gain_enabled, this, 0)));


#ifdef MIXBUS32C // from here 32C only
		MAKE_CTRL_BUTTON_PRESS(Control4, 76, 35, AmberFull, AmberLow,
			boost::function<void()> (boost::bind (&LaunchControlXL::dm_mb_eq_shape_switch, this, 0)),
			boost::function<uint8_t()> (boost::bind(&LaunchControlXL::dm_mb_eq_shape_enabled, this, 0 )));
		MAKE_CTRL_BUTTON_PRESS(Control5, 89, 36, AmberFull, AmberLow,
			boost::function<void()> (boost::bind (&LaunchControlXL::dm_mb_eq_shape_switch, this, 3)),
			boost::function<uint8_t()> (boost::bind(&LaunchControlXL::dm_mb_eq_shape_enabled, this, 3 )));
		MAKE_CTRL_BUTTON_PRESS(Control6, 90, 37, YellowFull, YellowLow, (boost::bind (&LaunchControlXL::dm_mb_flt_switch, this)),
			boost::bind(&LaunchControlXL::dm_mb_flt_enabled, this));
		MAKE_CTRL_BUTTON_PRESS(Control7, 91, 38, GreenFull, GreenLow, (boost::bind (&LaunchControlXL::dm_mb_master_assign_switch, this)),
			boost::bind(&LaunchControlXL::dm_mb_master_assign_enabled, this));
		MAKE_CTRL_BUTTON_PRESS(Control8, 92, 39, GreenFull, GreenLow, (boost::bind (&LaunchControlXL::dm_mb_comp_switch, this)),
			boost::bind(&LaunchControlXL::dm_mb_comp_enabled, this));

#else // from here Mixbus Standard only
		MAKE_CTRL_BUTTON_PRESS(Control5, 89, 36, GreenFull, GreenLow, (boost::bind (&LaunchControlXL::dm_mb_master_assign_switch, this)),
			boost::bind(&LaunchControlXL::dm_mb_master_assign_enabled, this));
		MAKE_CTRL_BUTTON_PRESS(Control7, 91, 38, GreenFull, GreenLow, (boost::bind (&LaunchControlXL::dm_mb_comp_switch, this)),
			boost::bind(&LaunchControlXL::dm_mb_comp_enabled, this));
		MAKE_CTRL_BUTTON_PRESS(Control8, 92, 39, RedFull, RedLow, (boost::bind (&LaunchControlXL::dm_recenable_switch, this)),
			boost::bind(&LaunchControlXL::dm_recenable_enabled, this));

#endif
#else // Ardour
#endif
	}

	/* Select and Mode Buttons on the right side */

	/* Sends Select buttons are independent of mode */
	MAKE_SELECT_BUTTON_PRESS(SelectUp, 104, 44, boost::function<void()> (boost::bind (&LaunchControlXL::send_bank_switch, this, false)));
	MAKE_SELECT_BUTTON_PRESS(SelectDown, 105, 45,  boost::function<void()> (boost::bind (&LaunchControlXL::send_bank_switch, this, true)));

	/* Device Button needs to be always there */
	MAKE_TRACK_STATE_BUTTON_PRESS_RELEASE_LONG(Device, 105, 40,
		boost::bind (&LaunchControlXL::relax, this) ,
		boost::bind (&LaunchControlXL::button_device, this),
	        boost::bind (&LaunchControlXL::button_device_long_press, this));


	/* Cancel all mute / solo is available in both modes */

	MAKE_TRACK_STATE_BUTTON_PRESS_RELEASE_LONG(Mute, 106, 41,
			boost::bind (&LaunchControlXL::relax, this) ,
			boost::bind (&LaunchControlXL::button_mute, this),
	        boost::bind (&LaunchControlXL::button_mute_long_press, this));

	MAKE_TRACK_STATE_BUTTON_PRESS_RELEASE_LONG(Solo, 107, 42,
			boost::bind (&LaunchControlXL::relax, this) ,
			boost::bind (&LaunchControlXL::button_solo, this),
	        boost::bind (&LaunchControlXL::button_solo_long_press, this));


	if (!device_mode()) {	/* mixer mode */
		MAKE_SELECT_BUTTON_PRESS(SelectLeft, 106, 46, boost::bind (&LaunchControlXL::button_select_left, this));
		MAKE_SELECT_BUTTON_PRESS(SelectRight, 107, 47, boost::bind (&LaunchControlXL::button_select_right, this));

		MAKE_TRACK_STATE_BUTTON_PRESS(Record, 108, 43, boost::bind (&LaunchControlXL::button_record, this));

	} else {	/* device mode */
		MAKE_SELECT_BUTTON_PRESS(SelectLeft, 106, 46,  boost::bind (&LaunchControlXL::dm_select_prev_strip, this));
		MAKE_SELECT_BUTTON_PRESS(SelectRight, 107, 47,  boost::bind (&LaunchControlXL::dm_select_next_strip, this));
	}
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

boost::shared_ptr<LaunchControlXL::Knob>
LaunchControlXL::knob_by_id(KnobID id)
{
	IDKnobMap::iterator k = id_knob_map.find(id);
	return boost::dynamic_pointer_cast<Knob> (k->second);

}

boost::shared_ptr<LaunchControlXL::Knob>*
LaunchControlXL::knobs_by_column(uint8_t col, boost::shared_ptr<Knob>* knob_col)
{
	for (uint8_t n = 0; n < 3; ++n) {
		if (id_knob_map.find(static_cast<KnobID>(col+n*8)) != id_knob_map.end()) {
			knob_col[n] = id_knob_map.find(static_cast<KnobID>(col+n*8))->second;
		}
	}

	return knob_col;
}

void
LaunchControlXL::update_knob_led_by_id (uint8_t id, LEDColor color)
{

	boost::shared_ptr<Knob> knob;
	IDKnobMap::iterator k = id_knob_map.find(static_cast<KnobID>(id));

	if (k != id_knob_map.end()) {
		knob = k->second;
	}

	knob->set_color(color);
	write (knob->state_msg());
}

void
LaunchControlXL::update_knob_led_by_strip(uint8_t n)
{
	LEDColor color;

	boost::shared_ptr<Knob> knobs_col[3];
	knobs_by_column(n, knobs_col);

	for  (uint8_t s = 0; s < 3; ++s) {
		if (knobs_col[s]) {
			if (stripable[n]) {
				if (stripable[n]->is_selected()) {
					color = knobs_col[s]->color_enabled();
				} else {
					color = knobs_col[s]->color_disabled();
				}
				knobs_col[s]->set_color(color);
			} else {
				knobs_col[s]->set_color(Off);
			}
			write (knobs_col[s]->state_msg());
		}
	}
}

void
LaunchControlXL::update_track_control_led(uint8_t n)
{
	boost::shared_ptr<TrackButton> b = control_button_by_column(n);

	if (!b) {
		return;
	}

	if ((buttons_down.find(Device) != buttons_down.end())) {
		/* Don't update LEDs if Device button is hold - we are working on selected strips */
		return;
	}

	if (stripable[n]) {
			boost::shared_ptr<AutomationControl> ac = get_ac_by_state(n);
			if (ac) {
				if (ac->get_value()) {
					b->set_color(b->color_enabled());
				} else {
				b->set_color(b->color_disabled());
				}
			} else {
				b->set_color(Off);
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
	DEBUG_TRACE (DEBUG::LaunchControlXL, "solo_mute_rec_changed - CALLING switch_bank(bank_start)\n");
	switch_bank(bank_start);
	//update_track_control_led(n);
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
	if (device_mode()) { return; }

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
				if (b) {
					write (b->state_msg());
				}
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
	if (device_mode()) { return; }

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
			if (b) {
				write (b->state_msg());
			}
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

	boost::shared_ptr<Fader> fader;
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

	boost::shared_ptr<Knob> knob;
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
		ac = stripable[n]->send_level_controllable (send_bank_base());
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

	boost::shared_ptr<Knob> knob;
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
		ac = stripable[n]->send_level_controllable (send_bank_base() + 1);
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

	boost::shared_ptr<Knob> knob;
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


	if (ac && check_pick_up(knob, ac, true)) {
		ac->set_value (ac->interface_to_internal((knob->value() / 127.0), true), PBD::Controllable::UseGroup);
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
			 ControlProtocol::remove_stripable_from_selection (stripable[n]);
		} else {
			ControlProtocol::add_stripable_to_selection (stripable[n]);
		}
	} else {
		return;
	}
}

void
LaunchControlXL::button_press_track_control(uint8_t n) {
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

		if (mute && solo && record) {
			write(mute->state_msg((state == TrackMute)));
			write(solo->state_msg((state == TrackSolo)));
			write(record->state_msg((state == TrackRecord)));
		}
}

void
LaunchControlXL::button_select_left()
{
	switch_bank (std::max (0, bank_start - (7 + (fader8master() ? 0 : 1))));
}

void
LaunchControlXL::button_select_right()
{
	switch_bank (std::max (0, bank_start + 7 + (fader8master() ? 0 : 1)));
}

void
LaunchControlXL::send_bank_switch(bool down) {
	DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("send_bank_switch - down: %1\n", down));
	if (down) {
		set_send_bank(+2);
	} else {
		set_send_bank(-2);
	}
}

void
LaunchControlXL::button_device()
{
#ifndef MIXBUS
	return; // currently device mode only on Mixbus
#endif
	LaunchControlXL::set_device_mode(!device_mode());
}

void
LaunchControlXL::button_device_long_press()
{
	if (device_mode()) { return ; }

	solo_iso_led_bank();
#ifdef MIXBUS
	master_send_led_bank();
#endif
}

void
LaunchControlXL::button_mute()
{
	if (device_mode()) { return ; }

	if (buttons_down.find(Device) != buttons_down.end()) {
		access_action ("Editor/track-mute-toggle");
	} else {
		button_track_mode(TrackMute);
	}
}

void
LaunchControlXL::button_mute_long_press()
{
	session->cancel_all_mute();
}

void
LaunchControlXL::button_solo()
{
	if (device_mode()) { return ; }

	if (buttons_down.find(Device) != buttons_down.end()) {
		access_action ("Editor/track-solo-toggle");
	} else {
		button_track_mode(TrackSolo);
	}
}

void
LaunchControlXL::button_solo_long_press()
{
	cancel_all_solo();
}

void
LaunchControlXL::button_record()
{
	if (device_mode()) { return ; }

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
		(button->long_press_method) ();
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
	ButtonID no_timeout_buttons[] = { SelectUp, SelectDown, SelectLeft, SelectRight };

	for (size_t n = 0; n < sizeof (no_timeout_buttons) / sizeof (no_timeout_buttons[0]); ++n) {
		if (id == no_timeout_buttons[n]) {
			DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose ("Not using timeout for button id %1\n", id));
			return;
		}
	}

	Glib::RefPtr<Glib::TimeoutSource> timeout = Glib::TimeoutSource::create (500); // milliseconds
	button->timeout_connection = timeout->connect (sigc::bind (sigc::mem_fun (*this, &LaunchControlXL::button_long_press_timeout), id, button));
	timeout->attach (main_loop()->get_context());
}


/* Device Mode functions */

void
LaunchControlXL::dm_select_prev_strip()
{
	access_action ("Editor/select-prev-stripable");
}

void
LaunchControlXL::dm_select_next_strip()
{
	access_action ("Editor/select-next-stripable");
}

uint8_t
LaunchControlXL::dm_check_dummy (DeviceStatus ds)
{
	return ds;
}

void
LaunchControlXL::dm_fader (FaderID id) {

	if (!first_selected_stripable()) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac;
	boost::shared_ptr<Fader> fader;

	IDFaderMap::iterator f = id_fader_map.find(id);

	if (f != id_fader_map.end()) {
		fader = f->second;
	}

	ac = first_selected_stripable()->gain_control();
	if (ac && check_pick_up(fader, ac)) {
		ac->set_value ( ac->interface_to_internal( fader->value() / 127.0), PBD::Controllable::UseGroup );
	}
}

uint8_t
LaunchControlXL::dm_check_pan_azi()
{
	if (!first_selected_stripable()) {
		return dev_nonexistant;
	}

	uint8_t dev_status = dev_nonexistant;
	if (first_selected_stripable()->pan_azimuth_control()) {
			dev_status = dev_active;
	}

	return dev_status;
}

void
LaunchControlXL::dm_pan_azi (KnobID k)
{
	if (!first_selected_stripable()) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac;
	boost::shared_ptr<Knob> knob = knob_by_id (k);

	ac = first_selected_stripable()->pan_azimuth_control();

	if (ac && check_pick_up(knob, ac, true)) {
		ac->set_value (ac->interface_to_internal((knob->value() / 127.0), true), PBD::Controllable::UseGroup);
	}
}


uint8_t
LaunchControlXL::dm_check_pan_width()
{
	if (!first_selected_stripable()) {
		return dev_nonexistant;
	}

	uint8_t dev_status = dev_nonexistant;
	if (first_selected_stripable()->pan_width_control()) {
			dev_status = dev_active;
	}

	return dev_status;
}

void
LaunchControlXL::dm_pan_width (KnobID k)
{
	if (!first_selected_stripable()) {
		return;
	}

	DEBUG_TRACE (DEBUG::LaunchControlXL, "dm_pan_width()\n");
	boost::shared_ptr<AutomationControl> ac;
	boost::shared_ptr<Knob> knob = knob_by_id (k);

	ac = first_selected_stripable()->pan_width_control();

	if (ac && check_pick_up(knob, ac)) {
		ac->set_value ( ac->interface_to_internal( knob->value() / 127.0), PBD::Controllable::UseGroup );
	}
}

uint8_t
LaunchControlXL::dm_check_trim()
{
	if (!first_selected_stripable()) {
		return dev_nonexistant;
	}

	uint8_t dev_status = dev_nonexistant;
	if (first_selected_stripable()->trim_control()) {
			dev_status = dev_active;
	}

	return dev_status;
}

void
LaunchControlXL::dm_trim (KnobID k)
{
	if (!first_selected_stripable()) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac;
	boost::shared_ptr<Knob> knob = knob_by_id (k);

	ac = first_selected_stripable()->trim_control();

	if (ac && check_pick_up(knob, ac)) {
		ac->set_value ( ac->interface_to_internal( knob->value() / 127.0), PBD::Controllable::UseGroup );
	}
}

uint8_t
LaunchControlXL::dm_mute_enabled()
{
	if (!first_selected_stripable()) {
		return dev_nonexistant;
	}

	uint8_t dev_status = dev_nonexistant;
	if (first_selected_stripable()->mute_control()->get_value()) {
		dev_status = dev_active;
	} else {
		dev_status = dev_inactive;
	}

	return dev_status;
}

void
LaunchControlXL::dm_mute_switch()
{
	if (!first_selected_stripable()) {
		return;
	}

	if (first_selected_stripable()->mute_control()) {
		first_selected_stripable()->mute_control()->set_value(!first_selected_stripable()->mute_control()->get_value(), PBD::Controllable::NoGroup);
	}
}

uint8_t
LaunchControlXL::dm_solo_enabled()
{
	if (!first_selected_stripable() || first_selected_stripable()->is_master()) {
		return dev_nonexistant;
	}

	uint8_t dev_status = dev_nonexistant;
	if (first_selected_stripable()->solo_control()) {
		if (first_selected_stripable()->solo_control()->get_value()) {
			dev_status = dev_active;
		} else {
			dev_status = dev_inactive;
		}
	}

	return dev_status;
}

void
LaunchControlXL::dm_solo_switch()
{
	if (!first_selected_stripable()) {
		return;
	}

	if (first_selected_stripable()->solo_control()) {
		first_selected_stripable()->solo_control()->set_value(!first_selected_stripable()->solo_control()->get_value(), PBD::Controllable::NoGroup);
	}
}

uint8_t
LaunchControlXL::dm_recenable_enabled()
{
	if (!first_selected_stripable()) {
		return dev_nonexistant;
	}

	uint8_t dev_status = dev_nonexistant;
	if (first_selected_stripable()->rec_enable_control()) {
		if (first_selected_stripable()->rec_enable_control()->get_value()) {
			dev_status = dev_active;
		}  else {
			dev_status = dev_inactive;
		}
	}

	return dev_status;
}

void
LaunchControlXL::dm_recenable_switch()
{
	if (!first_selected_stripable()) {
		return;
	}

	if (first_selected_stripable()->rec_enable_control()) {
		first_selected_stripable()->rec_enable_control()->set_value(!first_selected_stripable()->rec_enable_control()->get_value(), PBD::Controllable::NoGroup);
	}
}


#ifdef MIXBUS
uint8_t
LaunchControlXL::dm_mb_eq_freq_enabled()
{
	if (!first_selected_stripable()) {
		return dev_nonexistant;
	}

	uint8_t dev_status = dev_nonexistant;

	if (first_selected_stripable()->eq_enable_controllable()) {
		if (first_selected_stripable()->eq_enable_controllable()->get_value()) {
			dev_status = dev_active;
		} else {
			dev_status = dev_inactive;
		}
	}

	if (first_selected_stripable()->mixbus() || first_selected_stripable()->is_master()) {
		dev_status = dev_nonexistant;
	}

	return dev_status;
}


uint8_t
LaunchControlXL::dm_mb_eq_gain_enabled(uint8_t band)
{
	if (!first_selected_stripable()) {
		return dev_nonexistant;
	}

	uint8_t dev_status = dev_nonexistant;

	if (first_selected_stripable()->eq_enable_controllable()) {
		if (first_selected_stripable()->eq_enable_controllable()->get_value()) {
			dev_status = dev_active;
		} else {
			dev_status = dev_inactive;
		}
	}

	if (first_selected_stripable()->mixbus() && band > 3) {
		dev_status = dev_nonexistant;
	}

	return dev_status;
}

void
LaunchControlXL::dm_mb_eq (KnobID k, bool gain, uint8_t band)
{
	if (!first_selected_stripable()) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac;
	boost::shared_ptr<Knob> knob = knob_by_id (k);
	if (gain) {
		ac = first_selected_stripable()->eq_gain_controllable(band);
	} else {
		ac = first_selected_stripable()->eq_freq_controllable(band);
	}

	if (ac && check_pick_up(knob, ac)) {
		ac->set_value ( ac->interface_to_internal( knob->value() / 127.0), PBD::Controllable::UseGroup );
	}
}

void
LaunchControlXL::dm_mb_eq_shape_switch (uint8_t band)
{
	if (!first_selected_stripable()) {
		return;
	}

	if (first_selected_stripable()->eq_shape_controllable(band)) {
	first_selected_stripable()->eq_shape_controllable(band)->set_value
			(!first_selected_stripable()->eq_shape_controllable(band)->get_value(), PBD::Controllable::NoGroup );
	}
}


uint8_t
LaunchControlXL::dm_mb_eq_shape_enabled(uint8_t band)
{
	if (!first_selected_stripable()) {
		return dev_nonexistant;
	}

	uint8_t dev_status = dev_nonexistant;

	if (first_selected_stripable()->eq_shape_controllable(band)) {
		if (first_selected_stripable()->eq_shape_controllable(band)->get_value()) {
			dev_status = dev_active;
		} else {
			dev_status = dev_inactive;
		}
	}

	return dev_status;
}


void
LaunchControlXL::dm_mb_eq_switch()
{
	if (!first_selected_stripable()) {
		return;
	}

	if (first_selected_stripable()->eq_enable_controllable()) {
		first_selected_stripable()->eq_enable_controllable()->set_value
			(!first_selected_stripable()->eq_enable_controllable()->get_value(), PBD::Controllable::NoGroup );
	}
}

uint8_t
LaunchControlXL::dm_mb_flt_enabled()
{
	if (!first_selected_stripable()) {
		return dev_nonexistant;
	}

	DEBUG_TRACE (DEBUG::LaunchControlXL, "dm_mb_flt_enabled()\n");
	uint8_t dev_status = dev_nonexistant;

	if (first_selected_stripable()->filter_enable_controllable(true)) {
		DEBUG_TRACE (DEBUG::LaunchControlXL, "dm_mb_flt_enabled() - filter exists\n");
		if (first_selected_stripable()->filter_enable_controllable(true)->get_value()) {
			DEBUG_TRACE (DEBUG::LaunchControlXL, "dm_mb_flt_enabled: get_value true\n");
			dev_status = dev_active;
		} else {
			dev_status = dev_inactive;
		}
	}
	DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose("dm_mb_flt_enabled: dev_status: %1\n", (int)dev_status));
	return dev_status;
}


void
LaunchControlXL::dm_mb_flt_switch()
{
	if (!first_selected_stripable()) {
		return;
	}

	if (first_selected_stripable()->filter_enable_controllable(true)) {
		first_selected_stripable()->filter_enable_controllable(true)->set_value
			(!first_selected_stripable()->filter_enable_controllable(true)->get_value(), PBD::Controllable::NoGroup );
	}
}



void
LaunchControlXL::dm_mb_flt_frq (KnobID k, bool hpf)
{
	if (!first_selected_stripable()) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac;
	boost::shared_ptr<Knob> knob = knob_by_id (k);
	if (hpf) {
		ac = first_selected_stripable()->filter_freq_controllable(true);
	} else {
		ac = first_selected_stripable()->filter_freq_controllable(false);
	}

	if (ac && check_pick_up(knob, ac)) {
		ac->set_value ( ac->interface_to_internal( knob->value() / 127.0), PBD::Controllable::UseGroup );
	}
}

uint8_t
LaunchControlXL::dm_mb_check_send_knob (KnobID k)
{
	if (!first_selected_stripable()) {
		return dev_nonexistant;
	}

	uint8_t send = static_cast<uint8_t> (k) - 16 + 4 * send_bank_base();

	uint8_t dev_status = dev_nonexistant;

	if (first_selected_stripable()->send_enable_controllable(send)) {
		dev_status = dev_inactive;
		if (first_selected_stripable()->send_enable_controllable(send)->get_value()) {
			dev_status = dev_active;
		}
	}

	return dev_status;
}

uint8_t
LaunchControlXL::dm_mb_check_send_button (uint8_t s)
{
	if (!first_selected_stripable()) {
		return dev_nonexistant;
	}


	uint8_t send = s + 4 * send_bank_base();

	uint8_t dev_status = dev_nonexistant;

	if (first_selected_stripable()->send_enable_controllable(send)) {
		dev_status = dev_inactive;
		if (first_selected_stripable()->send_enable_controllable(send)->get_value()) {
			dev_status = dev_active;
		}
	}

	return dev_status;
}



void
LaunchControlXL::dm_mb_sends (KnobID k)
{
	if (!first_selected_stripable()) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac;
	boost::shared_ptr<Knob> knob = knob_by_id (k);

	uint8_t send = static_cast<uint8_t> (k) - 16 + 4 * send_bank_base();
	DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose("dm_mb_send: knobid '%1'\n", k));
	DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose("dm_mb_send: send '%1'\n", (int)send));


	if (buttons_down.find(Device) != buttons_down.end()) { // Device button hold
		ac = first_selected_stripable()->send_pan_azimuth_controllable(send);
	} else {
		ac = first_selected_stripable()->send_level_controllable(send);
	}

	if (ac && check_pick_up(knob, ac)) {
		ac->set_value ( ac->interface_to_internal( knob->value() / 127.0), PBD::Controllable::UseGroup );
	}
}

uint8_t
LaunchControlXL::dm_mb_comp_enabled()
{
	if (!first_selected_stripable()) {
		return dev_nonexistant;
	}

	uint8_t dev_status = dev_nonexistant;
	if (first_selected_stripable()->comp_enable_controllable()) {
		if (first_selected_stripable()->comp_enable_controllable()->get_value()) {
			dev_status = dev_active;
		} else {
			dev_status = dev_inactive;
		}
	}

	return dev_status;
}

void
LaunchControlXL::dm_mb_comp_switch()
{
	DEBUG_TRACE (DEBUG::LaunchControlXL, "dm_mb_comp_siwtch() \n");
	if (!first_selected_stripable()) {
		return;
	}

	if (first_selected_stripable()->comp_enable_controllable()) {
		DEBUG_TRACE (DEBUG::LaunchControlXL, "comp_enable_controllable exists\n");
		DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose("comp_enable_controllable(): '%1'\n", first_selected_stripable()->comp_enable_controllable()->get_value() ));
		first_selected_stripable()->comp_enable_controllable()->set_value
			(!first_selected_stripable()->comp_enable_controllable()->get_value(), PBD::Controllable::NoGroup);
	}

}

void
LaunchControlXL::dm_mb_comp (KnobID k, CompParam c)
{
	if (!first_selected_stripable()) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac;
	boost::shared_ptr<Knob> knob = knob_by_id (k);

	switch (c) {
		case (CompMakeup):
			ac = first_selected_stripable()->comp_makeup_controllable();
			break;
		case (CompMode):
			ac = first_selected_stripable()->comp_mode_controllable();
			break;
		case (CompSpeed):
			ac = first_selected_stripable()->comp_speed_controllable();
			break;
	}

		if (ac && check_pick_up(knob, ac)) {
		ac->set_value ( ac->interface_to_internal( knob->value() / 127.0), PBD::Controllable::UseGroup );
	}
}

void
LaunchControlXL::dm_mb_comp_thresh (FaderID id) {

	if (!first_selected_stripable()) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac;
	boost::shared_ptr<Fader> fader;

	IDFaderMap::iterator f = id_fader_map.find(id);

	if (f != id_fader_map.end()) {
		fader = f->second;
	}

	ac = first_selected_stripable()->comp_threshold_controllable();
	if (ac && check_pick_up(fader, ac)) {
		ac->set_value ( ac->interface_to_internal( fader->value() / 127.0), PBD::Controllable::UseGroup );
	}
}

uint8_t
LaunchControlXL::dm_mb_has_tapedrive()
{
	if (!first_selected_stripable()) {
		return dev_nonexistant;
	}

	uint8_t dev_status = dev_nonexistant;

	if (first_selected_stripable()->mixbus() || first_selected_stripable()->is_master()) {
		dev_status = dev_active;
	}

	return dev_status;
}

void
LaunchControlXL::dm_mb_tapedrive (KnobID k)
{
	if (!first_selected_stripable()) {
		return;
	}

	boost::shared_ptr<AutomationControl> ac;
	boost::shared_ptr<Knob> knob = knob_by_id (k);

	ac = first_selected_stripable()->tape_drive_controllable();

	if (ac && check_pick_up(knob, ac)) {
		ac->set_value ( ac->interface_to_internal( knob->value() / 127.0), PBD::Controllable::UseGroup );
	}
}

uint8_t
LaunchControlXL::dm_mb_master_assign_enabled()
{
	if (!first_selected_stripable()) {
		return dev_nonexistant;
	}

	uint8_t dev_status = dev_nonexistant;

	if (first_selected_stripable()->master_send_enable_controllable()) {
		if (first_selected_stripable()->master_send_enable_controllable()->get_value()) {
			dev_status = dev_active;
		} else {
			dev_status = dev_inactive;
		}
	}

	return dev_status;
}

void
LaunchControlXL::dm_mb_master_assign_switch()
{
	if (!first_selected_stripable()) {
		return;
	}

	if (first_selected_stripable()->master_send_enable_controllable()) {
		first_selected_stripable()->master_send_enable_controllable()->set_value
			(!first_selected_stripable()->master_send_enable_controllable()->get_value(), PBD::Controllable::NoGroup );
	}
}

void
LaunchControlXL::dm_mb_send_switch(ButtonID id)
{
	if (!first_selected_stripable()) {
		return;
	}

	boost::shared_ptr<Button> button = id_note_button_map[id];;

	uint8_t send = static_cast<uint8_t> (id) + 4 * send_bank_base();
	DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose("dm_mb_send: buttonid '%1'\n", (int)id));
	DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose("dm_mb_send: send '%1'\n", (int)send));

	if (first_selected_stripable()->send_enable_controllable(send)) {
		DEBUG_TRACE (DEBUG::LaunchControlXL, string_compose("dm_mb_send: send '%1' exists\n", (int)send));
		first_selected_stripable()->send_enable_controllable(send)->set_value
			(!first_selected_stripable()->send_enable_controllable(send)->get_value(), PBD::Controllable::UseGroup);
	}
}

#endif
