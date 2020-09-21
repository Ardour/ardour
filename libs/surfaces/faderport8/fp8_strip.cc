/*
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
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

#include "ardour/automation_control.h"
#include "ardour/gain_control.h"
#include "ardour/meter.h"
#include "ardour/plugin_insert.h"
#include "ardour/session.h"
#include "ardour/stripable.h"
#include "ardour/track.h"
#include "ardour/value_as_string.h"

#include "control_protocol/control_protocol.h"

#include "fp8_strip.h"

using namespace ARDOUR;
using namespace ArdourSurface::FP_NAMESPACE;
using namespace ArdourSurface::FP_NAMESPACE::FP8Types;

uint8_t /* static */
FP8Strip::midi_ctrl_id (CtrlElement type, uint8_t id)
{
	assert (id < N_STRIPS);
	if (id < 8) {
		switch (type) {
			case BtnSolo:
				return 0x08 + id;
			case BtnMute:
				return 0x10 + id;
			case BtnSelect:
				return 0x18 + id;
			case Fader:
				return 0xe0 + id;
			case Meter:
				return 0xd0 + id;
			case Redux:
				return 0xd8 + id;
			case BarVal:
				return 0x30 + id;
			case BarMode:
				return 0x38 + id;
		}
	} else {
		id -= 8;
		switch (type) {
			case BtnSolo:
				switch (id) {
					case 3:
						return 0x58;
					case 6:
						return 0x59;
					default:
						return 0x50 + id;
				}
			case BtnMute:
				return 0x78 + id;
			case BtnSelect:
				if (id == 0) { // strip 8
					return 0x07;
				} else {
					return 0x20 + id;
				}
			case Fader:
				return 0xe8 + id;
			case Meter:
				return 0xc0 + id;
			case Redux:
				return 0xc8 + id;
			case BarVal:
				return 0x40 + id;
			case BarMode:
				return 0x48 + id;
		}
	}
	assert (0);
	return 0;
}

FP8Strip::FP8Strip (FP8Base& b, uint8_t id)
	: _base (b)
	, _id (id)
	, _solo   (b, midi_ctrl_id (BtnSolo, id))
	, _mute   (b, midi_ctrl_id (BtnMute, id))
	, _selrec (b, midi_ctrl_id (BtnSelect, id), true)
	, _touching (false)
	, _strip_mode (0)
	, _bar_mode (0)
	, _displaymode (Stripables)
{
	assert (id < N_STRIPS);

	_last_fader = 65535;
	_last_meter = _last_redux = _last_barpos = 0xff;

	_mute.StateChange.connect_same_thread (_button_connections, boost::bind (&FP8Strip::set_mute, this, _1));
	_solo.StateChange.connect_same_thread (_button_connections, boost::bind (&FP8Strip::set_solo, this, _1));
	select_button ().released.connect_same_thread (_button_connections, boost::bind (&FP8Strip::set_select, this));
	recarm_button ().released.connect_same_thread (_button_connections, boost::bind (&FP8Strip::set_recarm, this));
	b.Periodic.connect_same_thread (_base_connection, boost::bind (&FP8Strip::periodic, this));
}

FP8Strip::~FP8Strip ()
{
	drop_automation_controls ();
	_base_connection.disconnect ();
	_button_connections.drop_connections ();
}

void
FP8Strip::drop_automation_controls ()
{
	_fader_connection.disconnect ();
	_mute_connection.disconnect ();
	_solo_connection.disconnect ();
	_rec_connection.disconnect ();
	_pan_connection.disconnect ();
	_x_select_connection.disconnect ();

	_fader_ctrl.reset ();
	_mute_ctrl.reset ();
	_solo_ctrl.reset ();
	_rec_ctrl.reset ();
	_pan_ctrl.reset ();
	_x_select_ctrl.reset ();
	_peak_meter.reset ();
	_redux_ctrl.reset ();
	_select_plugin_functor.clear ();
}

void
FP8Strip::initialize ()
{
	/* this is called once midi transmission is possible,
	 * ie from FaderPort8::connected()
	 */
	_solo.set_active (false);
	_solo.set_blinking (false);
	_mute.set_active (false);

	/* reset momentary button state */
	_mute.reset ();
	_solo.reset ();

	drop_automation_controls ();

	select_button ().set_color (0xffffffff);
	select_button ().set_active (false);
	select_button ().set_blinking (false);

	recarm_button ().set_active (false);
	recarm_button ().set_color (0xffffffff);

	set_strip_mode (0, true);

	// force unset txt
	_last_line[0].clear ();
	_last_line[1].clear ();
	_last_line[2].clear ();
	_last_line[3].clear ();
	_base.tx_sysex (4, 0x12, _id, 0x00, 0x00);
	_base.tx_sysex (4, 0x12, _id, 0x01, 0x00);
	_base.tx_sysex (4, 0x12, _id, 0x02, 0x00);
	_base.tx_sysex (4, 0x12, _id, 0x03, 0x00);

	set_bar_mode (4); // off

	_base.tx_midi2 (midi_ctrl_id (Meter, _id), 0); // reset meter
	_base.tx_midi2 (midi_ctrl_id (Redux, _id), 0); // reset redux

	_base.tx_midi3 (midi_ctrl_id (Fader, _id), 0, 0); // fader

	/* clear cached values */
	_last_fader = 65535;
	_last_meter = _last_redux = _last_barpos = 0xff;
}


#define GENERATE_SET_CTRL_FUNCTION(NAME)                                            \
void                                                                                \
FP8Strip::set_ ##NAME##_controllable (boost::shared_ptr<AutomationControl> ac)      \
{                                                                                   \
  if (_##NAME##_ctrl == ac) {                                                       \
    return;                                                                         \
  }                                                                                 \
  _##NAME##_connection.disconnect();                                                \
  _##NAME##_ctrl = ac;                                                              \
                                                                                    \
  if (ac) {                                                                         \
    ac->Changed.connect (_##NAME##_connection, MISSING_INVALIDATOR,                 \
      boost::bind (&FP8Strip::notify_##NAME##_changed, this), fp8_context());       \
  }                                                                                 \
  notify_##NAME##_changed ();                                                       \
}


GENERATE_SET_CTRL_FUNCTION (fader)
GENERATE_SET_CTRL_FUNCTION (mute)
GENERATE_SET_CTRL_FUNCTION (solo)
GENERATE_SET_CTRL_FUNCTION (rec)
GENERATE_SET_CTRL_FUNCTION (pan)
GENERATE_SET_CTRL_FUNCTION (x_select)

#undef GENERATE_SET_CTRL_FUNCTION

// special case -- w/_select_plugin_functor
void
FP8Strip::set_select_controllable (boost::shared_ptr<AutomationControl> ac)
{
	_select_plugin_functor.clear ();
	set_x_select_controllable (ac);
}

void
FP8Strip::set_select_cb (boost::function<void ()>& functor)
{
	set_select_controllable (boost::shared_ptr<AutomationControl>());
	_select_plugin_functor = functor;
}

void
FP8Strip::unset_controllables (int which)
{
	_peak_meter = boost::shared_ptr<ARDOUR::PeakMeter>();
	_redux_ctrl = boost::shared_ptr<ARDOUR::ReadOnlyControl>();
	_stripable_name.clear ();

	if (which & CTRL_FADER) {
		set_fader_controllable (boost::shared_ptr<AutomationControl>());
	}
	if (which & CTRL_MUTE) {
		set_mute_controllable (boost::shared_ptr<AutomationControl>());
	}
	if (which & CTRL_SOLO) {
		set_solo_controllable (boost::shared_ptr<AutomationControl>());
	}
	if (which & CTRL_REC) {
		set_rec_controllable (boost::shared_ptr<AutomationControl>());
	}
	if (which & CTRL_PAN) {
		set_pan_controllable (boost::shared_ptr<AutomationControl>());
	}
	if (which & CTRL_SELECT) {
		set_select_controllable (boost::shared_ptr<AutomationControl>());
		select_button ().set_color (0xffffffff);
		select_button ().set_active (false);
		select_button ().set_blinking (false);
	}
	if (which & CTRL_TEXT0) {
		set_text_line (0, "");
	}
	if (which & CTRL_TEXT1) {
		set_text_line (1, "");
	}
	if (which & CTRL_TEXT2) {
		set_text_line (2, "");
	}
	if (which & CTRL_TEXT3) {
		set_text_line (3, "");
	}
	set_bar_mode (4); // Off
}

void
FP8Strip::set_strip_name ()
{
	size_t lb = _base.show_meters () ? 6 : 9;
	set_text_line (0, _stripable_name.substr (0, lb));
	set_text_line (1, _stripable_name.length() > lb ? _stripable_name.substr (lb) : "");
}

void
FP8Strip::set_stripable (boost::shared_ptr<Stripable> s, bool panmode)
{
	assert (s);

	if (_base.show_meters () && _base.show_panner ()) {
		set_strip_mode (5, true);
	} else if (_base.show_meters ()) {
		set_strip_mode (4, true);
	} else {
		set_strip_mode (0, true);
	}
	if (!_base.show_panner ()) {
		set_bar_mode (4, true); // Off
	}

	if (panmode) {
		set_fader_controllable (s->pan_azimuth_control ());
	} else {
		set_fader_controllable (s->gain_control ());
	}
	set_pan_controllable (s->pan_azimuth_control ());

	if (s->is_monitor ()) {
		set_mute_controllable (boost::shared_ptr<AutomationControl>());
	} else {
		set_mute_controllable (s->mute_control ());
	}
	set_solo_controllable (s->solo_control ());

	if (boost::dynamic_pointer_cast<Track> (s)) {
		boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track>(s);
		set_rec_controllable (t->rec_enable_control ());
		recarm_button ().set_color (0xff0000ff);
	} else {
		set_rec_controllable (boost::shared_ptr<AutomationControl>());
		recarm_button ().set_color (0xffffffff);
		recarm_button ().set_active (false);
	}
	_peak_meter = s->peak_meter ();
	_redux_ctrl = s->comp_redux_controllable ();

	set_select_controllable (boost::shared_ptr<AutomationControl>());
	select_button ().set_active (s->is_selected ());

	set_select_button_color (s->presentation_info ().color());
	//select_button ().set_blinking (false);

	_stripable_name = s->name ();

	if (_base.twolinetext ()) {
		set_strip_name ();
	} else {
		set_text_line (0, s->name ());
		set_text_line (1, _pan_ctrl ? _pan_ctrl->get_user_string () : "");
	}
	set_text_line (2, "");
	set_text_line (3, "");
}

/* *****************************************************************************
 * Parse Strip Specifig MIDI Events
 */

bool
FP8Strip::midi_touch (bool t)
{
	_touching = t;
	boost::shared_ptr<AutomationControl> ac = _fader_ctrl;
	if (!ac) {
		return false;
	}
	timepos_t now (ac->session().transport_sample());
	if (t) {
		ac->start_touch (now);
	} else {
		ac->stop_touch (now);
	}
	return true;
}

bool
FP8Strip::midi_fader (float val)
{
	assert (val >= 0.f && val <= 1.f);
	if (!_touching) {
		return false;
	}
	boost::shared_ptr<AutomationControl> ac = _fader_ctrl;
	if (!ac) {
		return false;
	}
	ac->start_touch (timepos_t (ac->session().transport_sample()));
	ac->set_value (ac->interface_to_internal (val), group_mode ());
	return true;
}

/* *****************************************************************************
 * Actions from Controller, Update Model
 */

PBD::Controllable::GroupControlDisposition
FP8Strip::group_mode () const
{
	if (_base.shift_mod ()) {
		return PBD::Controllable::InverseGroup;
	} else {
		return PBD::Controllable::UseGroup;
	}
}

void
FP8Strip::set_mute (bool on)
{
	if (!_mute_ctrl) {
		return;
	}
	_mute_ctrl->start_touch (timepos_t (_mute_ctrl->session().transport_sample()));
	_mute_ctrl->set_value (on ? 1.0 : 0.0, group_mode ());
}

void
FP8Strip::set_solo (bool on)
{
	if (!_solo_ctrl) {
		return;
	}
	_solo_ctrl->start_touch (timepos_t (_solo_ctrl->session().transport_sample()));
	PBD::Controllable::GroupControlDisposition gcd = group_mode ();
	Session& s = const_cast<Session&> (_solo_ctrl->session());
	s.set_control (_solo_ctrl, on ? 1.0 : 0.0, gcd);
}

void
FP8Strip::set_recarm ()
{
	if (!_rec_ctrl) {
		return;
	}
	const bool on = !recarm_button ().is_active();
	_rec_ctrl->set_value (on ? 1.0 : 0.0, group_mode ());
}

void
FP8Strip::set_select ()
{
	if (!_select_plugin_functor.empty ()) {
		assert (!_x_select_ctrl);
		_select_plugin_functor ();
	} else if (_x_select_ctrl) {
		_x_select_ctrl->start_touch (timepos_t (_x_select_ctrl->session().transport_sample()));
		const bool on = !select_button ().is_active();
		_x_select_ctrl->set_value (on ? 1.0 : 0.0, group_mode ());
	}
}

/* *****************************************************************************
 * Callbacks from Stripable, Update View
 */

void
FP8Strip::notify_fader_changed ()
{
	boost::shared_ptr<AutomationControl> ac = _fader_ctrl;
	if (_touching) {
		return;
	}
	float val = 0;
	if (ac) {
		val = ac->internal_to_interface (ac->get_value());
		val = std::max (0.f, std::min (1.f, val)) * 16368.f; /* 16 * 1023 */
	}
	unsigned short mv = lrintf (val);
	if (mv == _last_fader) {
		return;
	}
	_last_fader = mv;
	_base.tx_midi3 (midi_ctrl_id (Fader, _id), (mv & 0x7f), (mv >> 7) & 0x7f);
}

void
FP8Strip::notify_solo_changed ()
{
	if (_solo_ctrl) {
		boost::shared_ptr<SoloControl> sc = boost::dynamic_pointer_cast<SoloControl> (_solo_ctrl);
		if (sc) {
			_solo.set_blinking (sc->soloed_by_others () && !sc->self_soloed ());
			_solo.set_active (sc->self_soloed ());
		} else {
			_solo.set_blinking (false);
			_solo.set_active (_solo_ctrl->get_value () > 0);
		}
	} else {
		_solo.set_blinking (false);
		_solo.set_active (false);
	}
}

void
FP8Strip::notify_mute_changed ()
{
	if (_mute_ctrl) {
		_mute.set_active (_mute_ctrl->get_value () > 0);
	} else {
		_mute.set_active (false);
	}
}

void
FP8Strip::notify_rec_changed ()
{
	if (_rec_ctrl) {
		recarm_button ().set_active (_rec_ctrl->get_value() > 0.);
	} else {
		recarm_button ().set_active (false);
	}
}

void
FP8Strip::notify_pan_changed ()
{
	// display only
}

void
FP8Strip::notify_x_select_changed ()
{
	if (!_select_plugin_functor.empty ()) {
		assert (!_x_select_ctrl);
		return;
	}

	if (_x_select_ctrl) {
		assert (_select_plugin_functor.empty ());
		select_button ().set_active (_x_select_ctrl->get_value() > 0.);
		select_button ().set_color (0xffff00ff);
		select_button ().set_blinking (false);
	}
}

/* *****************************************************************************
 * Periodic View Updates 
 */

void
FP8Strip::periodic_update_fader ()
{
	boost::shared_ptr<AutomationControl> ac = _fader_ctrl;
	if (!ac || _touching) {
		return;
	}

	if (!ac->automation_playback ()) {
		return;
	}
	notify_fader_changed ();
}

void
FP8Strip::set_periodic_display_mode (DisplayMode m) {
	_displaymode = m;
	if (_displaymode == SendDisplay || _displaymode == PluginParam) {
		// need to change to 4 lines before calling set_text()
		set_strip_mode (2); // 4 lines of small text
	}
}

void
FP8Strip::periodic_update_meter ()
{
	bool show_meters = _base.show_meters ();
	bool have_meter = false;
	bool have_panner = false;

	if (_peak_meter && show_meters) {
		have_meter = true;
		float dB = _peak_meter->meter_level (0, MeterMCP);
		// TODO: deflect meter
		int val = std::min (127.f, std::max (0.f, 2.f * dB + 127.f));
		if (val != _last_meter || val > 0) {
			_base.tx_midi2 (midi_ctrl_id (Meter, _id), val & 0x7f); // falls off automatically
			_last_meter = val;
		}

	} else if (show_meters) {
		if (0 != _last_meter) {
			_base.tx_midi2 (midi_ctrl_id (Meter, _id), 0);
			_last_meter = 0;
		}
	}

	// show redux only if there's a meter, too  (strip display mode 5)
	if (_peak_meter && _redux_ctrl && show_meters) {
		float rx = (1.f - _redux_ctrl->get_parameter ()) * 127.f;
		// TODO: deflect redux
		int val = std::min (127.f, std::max (0.f, rx));
		if (val != _last_redux) {
			_base.tx_midi2 (midi_ctrl_id (Redux, _id), val & 0x7f);
			_last_redux = val;
		}
	} else if (show_meters) {
		if (0 != _last_redux) {
			_base.tx_midi2 (midi_ctrl_id (Redux, _id), 0);
			_last_redux = 0;
		}
	}

	if (_displaymode == PluginParam) {
		if (_fader_ctrl) {
			set_bar_mode (2); // Fill
			set_text_line (2, value_as_string(_fader_ctrl->desc(), _fader_ctrl->get_value()));
			float barpos = _fader_ctrl->internal_to_interface (_fader_ctrl->get_value());
			int val = std::min (127.f, std::max (0.f, barpos * 128.f));
			if (val != _last_barpos) {
				_base.tx_midi3 (0xb0, midi_ctrl_id (BarVal, _id), val & 0x7f);
				_last_barpos = val;
			}
		} else {
			set_bar_mode (4); // Off
			set_text_line (2, "");
		}
	}
	else if (_displaymode == PluginSelect) {
		set_bar_mode (4); // Off
	}
	else if (_displaymode == SendDisplay) {
		set_bar_mode (4); // Off
		if (_fader_ctrl) {
			set_text_line (1, value_as_string(_fader_ctrl->desc(), _fader_ctrl->get_value()));
		} else {
			set_text_line (1, "");
		}
	} else if (_pan_ctrl) {
		have_panner = _base.show_panner ();
		float panpos = _pan_ctrl->internal_to_interface (_pan_ctrl->get_value(), true);
		int val = std::min (127.f, std::max (0.f, panpos * 128.f));
		set_bar_mode (have_panner ? 1 : 4); // Bipolar or Off
		if (val != _last_barpos && have_panner) {
			_base.tx_midi3 (0xb0, midi_ctrl_id (BarVal, _id), val & 0x7f);
			_last_barpos = val;
		}
		if (_base.twolinetext ()) {
			set_strip_name ();
		} else {
			set_text_line (1, _pan_ctrl->get_user_string ());
		}
	} else {
		set_bar_mode (4); // Off
		if (_base.twolinetext ()) {
			set_strip_name ();
		} else {
			set_text_line (1, "");
		}
	}

	if (_displaymode == SendDisplay || _displaymode == PluginParam) {
		set_strip_mode (2); // 4 lines of small text + value-bar
	}
	else if (have_meter && have_panner) {
		set_strip_mode (5); // small meters + 3 lines of text (3rd is large)  + value-bar
	}
	else if (have_meter) {
		/* we cannot use "big meters" mode 4, since that implies
		 * 2 "Large" (4char) text lines, followed by a HUGE 2 char line
		 * and hides timecode-clock */
		set_strip_mode (5);
	}
	else if (have_panner) {
		set_strip_mode (0); // 3 lines of text (3rd line is large + long) + value-bar
	} else {
		set_strip_mode (0); // 3 lines of text (3rd line is large + long) + value-bar
	}
}

void
FP8Strip::set_strip_mode (uint8_t strip_mode, bool clear)
{
	if (strip_mode == _strip_mode && !clear) {
		return;
	}

	_strip_mode = strip_mode;
	_base.tx_sysex (3, 0x13, _id, (_strip_mode & 0x07) | (clear ? 0x10 : 0));

	if (clear) {
		/* work-around, when swiching modes, the FP8 may not
		 * properly redraw long lines. Only update lines 0, 1
		 * (line 2 is timecode, line 3 may be inverted)
		 */
		_base.tx_text (_id, 0, 0x00, _last_line[0]);
		_base.tx_text (_id, 1, 0x00, _last_line[1]);
	}
}

void
FP8Strip::set_bar_mode (uint8_t bar_mode, bool force)
{
	if (bar_mode == _bar_mode && !force) {
		return;
	}

	if (bar_mode == 4) {
		_base.tx_midi3 (0xb0, midi_ctrl_id (BarVal, _id), 0);
		_last_barpos = 0xff;
	}

	_bar_mode = bar_mode;
	_base.tx_midi3 (0xb0, midi_ctrl_id (BarMode, _id), bar_mode);
}

void
FP8Strip::set_text_line (uint8_t line, std::string const& txt, bool inv)
{
	assert (line < 4);
	if (_last_line[line] == txt) {
		return;
	}
	_base.tx_text (_id, line, inv ? 0x04 : 0x00, txt);
	_last_line[line] = txt;
}

void
FP8Strip::periodic_update_timecode (uint32_t m)
{
	if (m == 0) {
		return;
	}
	if (m == 3) {
		bool mc = _id >= 4;
		std::string const& tc = mc ? _base.musical_time () : _base.timecode();
		std::string t;
		if (tc.size () == 12) {
			t = tc.substr (1 + (_id - (mc ? 4 : 0)) * 3, 2);
		}
		set_text_line (2, t);
	} else if (_id >= 2 && _id < 6) {
		std::string const& tc = (m == 2) ? _base.musical_time () : _base.timecode();
		//" HH:MM:SS:FF" or " BR|BT|TI|CK"
		std::string t;
		if (tc.size () == 12) {
			t = tc.substr (1 + (_id - 2) * 3, 2);
		}
		set_text_line (2, t);
	} else {
		set_text_line (2, "");
	}
}

void
FP8Strip::periodic ()
{
	periodic_update_fader ();
#ifndef FADERPORT2
	periodic_update_meter ();
	if (_displaymode != PluginSelect && _displaymode != PluginParam) {
		periodic_update_timecode (_base.clock_mode ());
	}
#endif
}
