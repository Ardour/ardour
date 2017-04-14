/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "ardour/automation_control.h"
#include "ardour/gain_control.h"
#include "ardour/meter.h"
#include "ardour/mute_control.h"
#include "ardour/plugin_insert.h"
#include "ardour/session.h"
#include "ardour/solo_control.h"
#include "ardour/stripable.h"
#include "ardour/track.h"
#include "ardour/value_as_string.h"

#include "control_protocol/control_protocol.h"

#include "fp8_strip.h"

using namespace ARDOUR;
using namespace ArdourSurface;
using namespace ArdourSurface::FP8Types;

FP8Strip::FP8Strip (FP8Base& b, uint8_t id)
	: _base (b)
	, _id (id)
	, _solo   (b, 0x08 + id)
	, _mute   (b, 0x10 + id)
	, _selrec (b, 0x18 + id, true)
	, _touching (false)
	, _strip_mode (0)
	, _bar_mode (0)
	, _displaymode (Stripables)
{
	assert (id < 8);

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
	_fader_connection.disconnect ();
	_mute_connection.disconnect ();
	_solo_connection.disconnect ();
	_rec_connection.disconnect ();
	_pan_connection.disconnect ();

	_fader_ctrl.reset ();
	_mute_ctrl.reset ();
	_solo_ctrl.reset ();
	_rec_ctrl.reset ();
	_pan_ctrl.reset ();

	_base_connection.disconnect ();
	_button_connections.drop_connections ();
}

void
FP8Strip::initialize ()
{
	/* this is called once midi transmission is possible,
	 * ie from FaderPort8::connected()
	 */
	_solo.set_active (false);
	_mute.set_active (false);

	/* reset momentary button state */
	_mute.reset ();
	_solo.reset ();

	/* clear cached values */
	_last_fader = 65535;
	_last_meter = _last_redux = _last_barpos = 0xff;

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

	_base.tx_midi2 (0xd0 + _id, 0); // reset meter
	_base.tx_midi2 (0xd8 + _id, 0); // reset redux

	_base.tx_midi3 (0xe0 + _id, 0, 0); // fader
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

#undef GENERATE_SET_CTRL_FUNCTION

void
FP8Strip::unset_controllables (int which)
{
	_peak_meter = boost::shared_ptr<ARDOUR::PeakMeter>();
	_redux_ctrl = boost::shared_ptr<ARDOUR::ReadOnlyControl>();

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
		_select_plugin_functor.clear ();
		select_button ().set_color (0xffffffff);
		select_button ().set_active (false);
		select_button ().set_blinking (false);
	}
	if (which & CTRL_TEXT0) {
		set_text_line (0x00, "");
	}
	if (which & CTRL_TEXT1) {
		set_text_line (0x01, "");
	}
	if (which & CTRL_TEXT2) {
		set_text_line (0x02, "");
	}
	if (which & CTRL_TEXT3) {
		set_text_line (0x03, "");
	}
	set_bar_mode (4); // Off
}

void
FP8Strip::set_stripable (boost::shared_ptr<Stripable> s, bool panmode)
{
	assert (s);

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

	_select_plugin_functor.clear ();
	select_button ().set_active (s->is_selected ());
	select_button ().set_color (s->presentation_info ().color());
	//select_button ().set_blinking (false);

	set_strip_mode (0x05);
	set_text_line (0x00, s->name ());
	set_text_line (0x01, _pan_ctrl ? _pan_ctrl->get_user_string () : "");
	set_text_line (0x02, "");
	set_text_line (0x03, "");
}

void
FP8Strip::set_select_cb (boost::function<void ()>& functor)
{
	_select_plugin_functor.clear ();
	_select_plugin_functor = functor;
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
	if (t) {
		if (!ac->touching ()) {
			ac->start_touch (ac->session().transport_frame());
		}
	} else {
		ac->stop_touch (true, ac->session().transport_frame());
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
	if (!ac->touching ()) {
		ac->start_touch (ac->session().transport_frame());
	}
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
	if (_mute_ctrl) {
		if (!_mute_ctrl->touching ()) {
			_mute_ctrl->start_touch (_mute_ctrl->session().transport_frame());
		}
		_mute_ctrl->set_value (on ? 1.0 : 0.0, group_mode ());
	}
}

void
FP8Strip::set_solo (bool on)
{
	if (_solo_ctrl) {
		if (!_solo_ctrl->touching ()) {
			_solo_ctrl->start_touch (_solo_ctrl->session().transport_frame());
		}
		_solo_ctrl->set_value (on ? 1.0 : 0.0, group_mode ());
	}
}

void
FP8Strip::set_recarm ()
{
	if (_rec_ctrl) {
		const bool on = !recarm_button().is_active();
		_rec_ctrl->set_value (on ? 1.0 : 0.0, group_mode ());
	}
}


void
FP8Strip::set_select ()
{
	if (!_select_plugin_functor.empty ()) {
		_select_plugin_functor ();
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
		val = ac->internal_to_interface (ac->get_value()) * 16368.f; /* 16 * 1023 */
	}
	unsigned short mv = lrintf (val);
	if (mv == _last_fader) {
		return;
	}
	_last_fader = mv;
	_base.tx_midi3 (0xe0 + _id, (mv & 0x7f), (mv >> 7) & 0x7f);
}

void
FP8Strip::notify_solo_changed ()
{
	if (_solo_ctrl) {
		_solo.set_active (_solo_ctrl->get_value () > 0);
	} else {
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

	ARDOUR::AutoState state = ac->automation_state();
	if (state == Touch || state == Play) {
		notify_fader_changed ();
	}
}

void
FP8Strip::set_periodic_display_mode (DisplayMode m) {
	_displaymode = m;
	if (_displaymode == SendDisplay) {
		// need to change to 4 lines before calling set_text()
		set_strip_mode (2); // 4 lines of small text
	}
}

void
FP8Strip::periodic_update_meter ()
{
	bool have_meter = false;
	bool have_panner = false;

	if (_peak_meter) {
		have_meter = true;
		float dB = _peak_meter->meter_level (0, MeterMCP);
		// TODO: deflect meter
		int val = std::min (127.f, std::max (0.f, 2.f * dB + 127.f));
		if (val != _last_meter || val > 0) {
			_base.tx_midi2 (0xd0 + _id, val & 0x7f); // falls off automatically
			_last_meter = val;
		}

	} else {
		if (0 != _last_meter) {
			_base.tx_midi2 (0xd0 + _id, 0);
			_last_meter = 0;
		}
	}

	// show redux only if there's a meter, too  (strip display mode 5)
	if (_peak_meter && _redux_ctrl) {
		float rx = (1.f - _redux_ctrl->get_parameter ()) * 127.f;
		// TODO: deflect redux
		int val = std::min (127.f, std::max (0.f, rx));
		if (val != _last_redux) {
			_base.tx_midi2 (0xd8 + _id, val & 0x7f);
			_last_redux = val;
		}
	} else {
		if (0 != _last_redux) {
			_base.tx_midi2 (0xd8 + _id, 0);
			_last_redux = 0;
		}
	}

	if (_displaymode == PluginParam) {
		if (_fader_ctrl) {
			set_bar_mode (2); // Fill
			set_text_line (0x01, value_as_string(_fader_ctrl->desc(), _fader_ctrl->get_value()));
			float barpos = _fader_ctrl->internal_to_interface (_fader_ctrl->get_value());
			int val = std::min (127.f, std::max (0.f, barpos * 128.f));
			if (val != _last_barpos) {
				_base.tx_midi3 (0xb0, 0x30 + _id, val & 0x7f);
				_last_barpos = val;
			}
		} else {
			set_bar_mode (4); // Off
			set_text_line (0x01, "");
		}
	}
	else if (_displaymode == SendDisplay) {
		set_bar_mode (4); // Off
		if (_fader_ctrl) {
			set_text_line (0x01, value_as_string(_fader_ctrl->desc(), _fader_ctrl->get_value()));
		} else {
			set_text_line (0x01, "");
		}
	} else if (_pan_ctrl) {
		have_panner = true;
		float panpos = _pan_ctrl->internal_to_interface (_pan_ctrl->get_value());
		int val = std::min (127.f, std::max (0.f, panpos * 128.f));
		set_bar_mode (1); // Bipolar
		if (val != _last_barpos) {
			_base.tx_midi3 (0xb0, 0x30 + _id, val & 0x7f);
			_last_barpos = val;
		}
		set_text_line (0x01, _pan_ctrl->get_user_string ());
	} else {
		set_bar_mode (4); // Off
	}

	if (_displaymode == SendDisplay) {
		set_strip_mode (2); // 4 lines of small text + value-bar
	}
	else if (have_meter && have_panner) {
		set_strip_mode (5); // small meters + 3 lines of text (3rd is large)  + value-bar
	}
	else if (have_meter) {
		set_strip_mode (4); // big meters + 3 lines of text (3rd line is large)
	}
	else if (have_panner) {
		set_strip_mode (0); // 3 lines of text (3rd line is large) + value-bar
	} else {
		set_strip_mode (0); // 3 lines of text (3rd line is large) + value-bar
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
	//_base.tx_midi3 (0xb0, 0x38 + _id, _bar_mode);
}

void
FP8Strip::set_bar_mode (uint8_t bar_mode)
{
	if (bar_mode == _bar_mode) {
		return;
	}
	_bar_mode = bar_mode;
	_base.tx_midi3 (0xb0, 0x38 + _id, bar_mode);
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
FP8Strip::periodic_update_timecode ()
{
	if (_id >= 2 && _id < 6) {
		std::string const& tc = _base.timecode();
		//" HH:MM:SS:FF"
		std::string t;
		if (tc.size () == 12) {
			t = tc.substr (1 + (_id - 2) * 3, 2);
		}
		set_text_line (0x02, t);
	}
}

void
FP8Strip::periodic ()
{
	periodic_update_fader ();
	periodic_update_meter ();
	if (_displaymode != PluginSelect) {
		periodic_update_timecode ();
	}
}
