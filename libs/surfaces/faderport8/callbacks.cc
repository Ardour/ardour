/* Faderport 8 Control Surface
 * This is the button "View" of the MVC surface inteface,
 * see actions.cc for the "Controller"
 *
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

#include "ardour/session.h"
#include "ardour/session_configuration.h"

#include "gtkmm2ext/actions.h"

#include "faderport8.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourSurface;
using namespace ArdourSurface::FP8Types;

void
FaderPort8::connect_session_signals ()
{
	 session->RouteAdded.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::notify_stripable_added_or_removed, this), this);
	 PresentationInfo::Change.connect(session_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::notify_pi_property_changed, this, _1), this);

	Config->ParameterChanged.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::notify_parameter_changed, this, _1), this);
	session->config.ParameterChanged.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::notify_parameter_changed, this, _1), this);

	session->TransportStateChange.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::notify_transport_state_changed, this), this);
	session->TransportLooped.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::notify_loop_state_changed, this), this);
	session->RecordStateChanged.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::notify_record_state_changed, this), this);

	session->DirtyChanged.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::notify_session_dirty_changed, this), this);
	session->SoloChanged.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::notify_solo_changed, this), this);
	session->MuteChanged.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::notify_mute_changed, this), this);
	session->history().Changed.connect (session_connections, MISSING_INVALIDATOR, boost::bind (&FaderPort8::notify_history_changed, this), this);
}

void
FaderPort8::send_session_state ()
{
	notify_transport_state_changed ();
	notify_record_state_changed ();
	notify_session_dirty_changed ();
	notify_history_changed ();
	notify_solo_changed ();
	notify_mute_changed ();
	notify_parameter_changed ("clicking");

	notify_automation_mode_changed (); // XXX (stip specific, see below)
}

// TODO: AutomationState display of plugin & send automation ?!
void
FaderPort8::notify_automation_mode_changed ()
{
	boost::shared_ptr<Stripable> s = first_selected_stripable();
	boost::shared_ptr<AutomationControl> ac;
	if (s) {
		switch (_ctrls.fader_mode ()) {
			case ModeTrack:
				ac = s->gain_control();
				break;
			case ModePan:
				ac = s->pan_azimuth_control();
				break;
			default:
				break;
		}
	}
	if (!s || !ac) {
		_ctrls.button (FP8Controls::BtnALatch).set_active (false);
		_ctrls.button (FP8Controls::BtnATrim).set_active (false);
		_ctrls.button (FP8Controls::BtnAOff).set_active (false);
		_ctrls.button (FP8Controls::BtnATouch).set_active (false);
		_ctrls.button (FP8Controls::BtnARead).set_active (false);
		_ctrls.button (FP8Controls::BtnAWrite).set_active (false);
		return;
	}

	ARDOUR::AutoState as = ac->automation_state();
	_ctrls.button (FP8Controls::BtnAOff).set_active (as == Off);
	_ctrls.button (FP8Controls::BtnATouch).set_active (as == Touch);
	_ctrls.button (FP8Controls::BtnARead).set_active (as == Play);
	_ctrls.button (FP8Controls::BtnAWrite).set_active (as == Write);
}

void
FaderPort8::notify_parameter_changed (std::string param)
{
	if (param == "clicking") {
		_ctrls.button (FP8Controls::BtnClick).set_active (Config->get_clicking ());
	}
}

void
FaderPort8::notify_transport_state_changed ()
{
	if (session->transport_rolling ()) {
		_ctrls.button (FP8Controls::BtnPlay).set_active (true);
		_ctrls.button (FP8Controls::BtnStop).set_active (false);
	} else {
		_ctrls.button (FP8Controls::BtnPlay).set_active (false);
		_ctrls.button (FP8Controls::BtnStop).set_active (true);
	}

	/* set rewind/fastforward lights */
	const float ts = session->transport_speed ();
	FP8ButtonInterface& b_rew = _ctrls.button (FP8Controls::BtnRewind);
	FP8ButtonInterface& b_ffw = _ctrls.button (FP8Controls::BtnFastForward);

	const bool rew = (ts < 0.f);
	const bool ffw = (ts > 0.f && ts != 1.f);
	if (b_rew.is_active() != rew) {
		b_rew.set_active (rew);
	}
	if (b_ffw.is_active() != ffw) {
		b_ffw.set_active (ffw);
	}

	notify_loop_state_changed ();
}

void
FaderPort8::notify_record_state_changed ()
{
	switch (session->record_status ()) {
		case Session::Disabled:
			_ctrls.button (FP8Controls::BtnRecord).set_active (0);
			_ctrls.button (FP8Controls::BtnRecord).set_blinking (false);
			break;
		case Session::Enabled:
			_ctrls.button (FP8Controls::BtnRecord).set_active (true);
			_ctrls.button (FP8Controls::BtnRecord).set_blinking (true);
			break;
		case Session::Recording:
			_ctrls.button (FP8Controls::BtnRecord).set_active (true);
			_ctrls.button (FP8Controls::BtnRecord).set_blinking (false);
			break;
	}
}

void
FaderPort8::notify_loop_state_changed ()
{
	bool looping = false;
	Location* looploc = session->locations ()->auto_loop_location ();
	if (looploc && session->get_play_loop ()) {
		looping = true;
	}
	_ctrls.button (FP8Controls::BtnLoop).set_active (looping);
}

void
FaderPort8::notify_session_dirty_changed ()
{
	const bool is_dirty = session->dirty ();
	_ctrls.button (FP8Controls::BtnSave).set_active (is_dirty);
	_ctrls.button (FP8Controls::BtnSave).set_color (is_dirty ? 0xff0000ff : 0x00ff00ff);
}

void
FaderPort8::notify_history_changed ()
{
	_ctrls.button (FP8Controls::BtnRedo).set_active (session->redo_depth() > 0);
	_ctrls.button (FP8Controls::BtnUndo).set_active (session->undo_depth() > 0);
}

void
FaderPort8::notify_solo_changed ()
{
	bool soloing = session->soloing() || session->listening();
	_ctrls.button (FP8Controls::BtnSoloClear).set_active (soloing);
#ifdef FP8_MUTESOLO_UNDO
	if (soloing) {
		_solo_state.clear ();
	}
#endif
}

void
FaderPort8::notify_mute_changed ()
{
	bool muted = false;
	StripableList all;
	session->get_stripables (all);
	for (StripableList::const_iterator i = all.begin(); i != all.end(); ++i) {
		if ((*i)->is_auditioner() || (*i)->is_monitor()) {
			continue;
		}
		boost::shared_ptr<Route> r = boost::dynamic_pointer_cast<Route>(*i);
		if (r && !r->active()) {
			continue;
		}
		boost::shared_ptr<MuteControl> mc = (*i)->mute_control();
		if (mc && mc->muted ()) {
			muted = true;
			break;
		}
	}
#ifdef FP8_MUTESOLO_UNDO
	if (muted) {
		_mute_state.clear ();
	}
#endif
	_ctrls.button (FP8Controls::BtnMuteClear).set_active (muted);
}
