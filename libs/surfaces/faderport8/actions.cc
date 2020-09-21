/*
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Ben Loftis <ben@harrisonconsoles.com>
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

/* Faderport 8 Control Surface
 * This is the button "Controller" of the MVC surface inteface,
 * see callbacks.cc for the "View".
 */

#include "ardour/dB.h"
#include "ardour/plugin_insert.h"
#include "ardour/session.h"
#include "ardour/session_configuration.h"
#include "ardour/track.h"
#include "ardour/types.h"

#include "gtkmm2ext/actions.h"

#include "faderport8.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace ArdourSurface::FP_NAMESPACE;
using namespace ArdourSurface::FP_NAMESPACE::FP8Types;

#define BindMethod(ID, CB) \
	_ctrls.button (FP8Controls::ID).released.connect_same_thread (button_connections, boost::bind (&FaderPort8:: CB, this));

#define BindMethod2(ID, ACT, CB) \
	_ctrls.button (FP8Controls::ID). ACT .connect_same_thread (button_connections, boost::bind (&FaderPort8:: CB, this));

#define BindFunction(ID, ACT, CB, ...) \
	_ctrls.button (FP8Controls::ID). ACT .connect_same_thread (button_connections, boost::bind (&FaderPort8:: CB, this, __VA_ARGS__));

#define BindAction(ID, GRP, ITEM) \
	_ctrls.button (FP8Controls::ID).released.connect_same_thread (button_connections, boost::bind (&FaderPort8::button_action, this, GRP, ITEM));

#define BindUserAction(ID) \
	_ctrls.button (ID).pressed.connect_same_thread (button_connections, boost::bind (&FaderPort8::button_user, this, true, ID)); \
_ctrls.button (ID).released.connect_same_thread (button_connections, boost::bind (&FaderPort8::button_user, this, false, ID));


/* Bind button signals (press, release) to callback methods
 * (called once after constructing buttons).
 * Bound actions are handled the the ctrl-surface thread.
 */
void
FaderPort8::setup_actions ()
{
	BindMethod2 (BtnPlay, pressed, button_play);
	BindMethod2 (BtnStop, pressed, button_stop);
	BindMethod2 (BtnLoop, pressed, button_loop);
	BindMethod2 (BtnRecord, pressed, button_record);
	BindMethod2 (BtnClick, pressed, button_metronom);
	BindAction (BtnRedo, "Editor", "redo");

	BindAction (BtnSave, "Common", "Save");
	BindAction (BtnUndo, "Editor", "undo");
	BindAction (BtnRedo, "Editor", "redo");

#ifdef FP8_MUTESOLO_UNDO
	BindMethod (BtnSoloClear, button_solo_clear);
#else
	BindAction (BtnSoloClear, "Main", "cancel-solo");
#endif
	BindMethod (BtnMuteClear, button_mute_clear);

	BindMethod (FP8Controls::BtnArmAll, button_arm_all);

	BindFunction (BtnRewind, pressed, button_varispeed, false);
	BindFunction (BtnFastForward, pressed, button_varispeed, true);

	BindFunction (BtnPrev, released, button_prev_next, false);
	BindFunction (BtnNext, released, button_prev_next, true);

	BindFunction (BtnArm, pressed, button_arm, true);
	BindFunction (BtnArm, released, button_arm, false);

	BindFunction (BtnAOff, released, button_automation, ARDOUR::Off);
	BindFunction (BtnATouch, released, button_automation, ARDOUR::Touch);
	BindFunction (BtnARead, released, button_automation, ARDOUR::Play);
	BindFunction (BtnAWrite, released, button_automation, ARDOUR::Write);
	BindFunction (BtnALatch, released, button_automation, ARDOUR::Latch);

	_ctrls.button (FP8Controls::BtnEncoder).pressed.connect_same_thread (button_connections, boost::bind (&FaderPort8::button_encoder, this));
#ifdef FADERPORT2
	_ctrls.button (FP8Controls::BtnParam).pressed.connect_same_thread (button_connections, boost::bind (&FaderPort8::button_encoder, this));
#else
	_ctrls.button (FP8Controls::BtnParam).pressed.connect_same_thread (button_connections, boost::bind (&FaderPort8::button_parameter, this));
#endif


	BindMethod (BtnBypass, button_bypass);
	BindAction (BtnBypassAll, "Mixer", "ab-plugins");

	BindAction (BtnMacro, "Common", "toggle-editor-and-mixer");
	BindMethod (BtnOpen, button_open);

	BindMethod (BtnLink, button_link);
	BindMethod (BtnLock, button_lock);

#ifdef FADERPORT2
	BindMethod (BtnChanLock, button_chanlock);
	BindMethod (BtnFlip, button_flip);
#endif

	// user-specific
	for (FP8Controls::UserButtonMap::const_iterator i = _ctrls.user_buttons ().begin ();
			i != _ctrls.user_buttons ().end (); ++i) {
		BindUserAction ((*i).first);
	}
}

/* ****************************************************************************
 * Direct control callback Actions
 */

void
FaderPort8::button_play ()
{
	if (transport_rolling ()) {
		if (get_transport_speed() != 1.0) {
			session->request_roll (TRS_UI);
		} else {
			transport_stop ();
		}
	} else {
		transport_play ();
	}
}

void
FaderPort8::button_stop ()
{
	if (transport_rolling ()) {
		transport_stop ();
	} else {
		AccessAction ("Transport", "GotoStart");
	}
}

void
FaderPort8::button_record ()
{
	set_record_enable (!get_record_enabled ());
}

void
FaderPort8::button_loop ()
{
	loop_toggle ();
}

void
FaderPort8::button_metronom ()
{
	Config->set_clicking (!Config->get_clicking ());
}

void
FaderPort8::button_bypass ()
{
	boost::shared_ptr<PluginInsert> pi = _plugin_insert.lock();
	if (pi) {
		pi->enable (! pi->enabled ());
	} else {
		AccessAction ("Mixer", "ab-plugins");
	}
}

void
FaderPort8::button_open ()
{
	boost::shared_ptr<PluginInsert> pi = _plugin_insert.lock();
	if (pi) {
		pi->ToggleUI (); /* EMIT SIGNAL */
	} else {
		AccessAction ("Common", "addExistingAudioFiles");
	}
}

void
FaderPort8::button_chanlock ()
{
	_chan_locked = !_chan_locked;

	_ctrls.button (FP8Controls::BtnChannel).set_blinking (_chan_locked);
}

void
FaderPort8::button_flip ()
{
}

void
FaderPort8::button_lock ()
{
	if (!_link_enabled) {
		AccessAction ("Editor", "lock");
		return;
	}
	if (_link_locked) {
		unlock_link ();
	} else if (!_link_control.expired ()) {
		lock_link ();
	}
}

void
FaderPort8::button_link ()
{
	switch (_ctrls.fader_mode()) {
		case ModeTrack:
		case ModePan:
			if (_link_enabled) {
				stop_link ();
			} else {
				start_link ();
			}
			break;
		default:
			//AccessAction ("Window", "show-mixer");
			break;
	}
}

void
FaderPort8::button_automation (ARDOUR::AutoState as)
{
	FaderMode fadermode = _ctrls.fader_mode ();
	switch (fadermode) {
		case ModePlugins:
#if 0 // Plugin Control Automation Mode
			for (std::list <ProcessorCtrl>::iterator i = _proc_params.begin(); i != _proc_params.end(); ++i) {
				((*i).ac)->set_automation_state (as);
			}
#endif
			return;
		case ModeSend:
			if (first_selected_stripable()) {
#if 0 // Send Level Automation
				boost::shared_ptr<Stripable> s = first_selected_stripable();
				boost::shared_ptr<AutomationControl> send;
				uint32_t i = 0;
				while (0 != (send = s->send_level_controllable (i))) {
					send->set_automation_state (as);
					++i;
				}
#endif
			}
			return;
		default:
			break;
	}

	// TODO link/lock control automation?

	// apply to all selected tracks
	StripableList all;
	session->get_stripables (all);
	for (StripableList::const_iterator i = all.begin(); i != all.end(); ++i) {
		if ((*i)->is_master() || (*i)->is_monitor()) {
			continue;
		}
		if (!(*i)->is_selected()) {
			continue;
		}
		boost::shared_ptr<AutomationControl> ac;
		switch (fadermode) {
			case ModeTrack:
				ac = (*i)->gain_control ();
				break;
			case ModePan:
				ac = (*i)->pan_azimuth_control ();
				break;
			default:
				break;
		}
		if (ac) {
			ac->set_automation_state (as);
		}
	}
}

void
FaderPort8::button_varispeed (bool ffw)
{
	/* pressing both rew + ffwd -> return to zero */
	FP8ButtonInterface& b_rew = _ctrls.button (FP8Controls::BtnRewind);
	FP8ButtonInterface& b_ffw = _ctrls.button (FP8Controls::BtnFastForward);
	if (b_rew.is_pressed () && b_ffw.is_pressed ()){
		// stop key-repeat
		dynamic_cast<FP8RepeatButton*>(&b_ffw)->stop_repeat();
		dynamic_cast<FP8RepeatButton*>(&b_rew)->stop_repeat();
		session->request_locate (0, MustStop);
		return;
	}

	BasicUI::button_varispeed (ffw);
}

#ifdef FP8_MUTESOLO_UNDO
void
FaderPort8::button_solo_clear ()
{
	bool soloing = session->soloing() || session->listening();
#ifdef MIXBUS
	soloing |= session->mixbus_soloed();
#endif
	if (soloing) {
		StripableList all;
		session->get_stripables (all);
		for (StripableList::const_iterator i = all.begin(); i != all.end(); ++i) {
			if ((*i)->is_master() || (*i)->is_auditioner() || (*i)->is_monitor()) {
				continue;
			}
			boost::shared_ptr<SoloControl> sc = (*i)->solo_control();
			if (sc && sc->self_soloed ()) {
				_solo_state.push_back (boost::weak_ptr<AutomationControl>(sc));
			}
		}
		cancel_all_solo (); // AccessAction ("Main", "cancel-solo");
	} else {
		/* restore solo */
		boost::shared_ptr<ControlList> cl (new ControlList);
		for (std::vector <boost::weak_ptr<AutomationControl> >::const_iterator i = _solo_state.begin(); i != _solo_state.end(); ++i) {
			boost::shared_ptr<AutomationControl> ac = (*i).lock();
			if (!ac) {
				continue;
			}
			ac->start_touch (timepos_t (ac->session().transport_sample()));
			cl->push_back (ac);
		}
		if (!cl->empty()) {
			session->set_controls (cl, 1.0, PBD::Controllable::NoGroup);
		}
	}
}
#endif

void
FaderPort8::button_mute_clear ()
{
#ifdef FP8_MUTESOLO_UNDO
	if (session->muted ()) {
		_mute_state = session->cancel_all_mute ();
	} else {
		/* restore mute */
		boost::shared_ptr<ControlList> cl (new ControlList);
		for (std::vector <boost::weak_ptr<AutomationControl> >::const_iterator i = _mute_state.begin(); i != _mute_state.end(); ++i) {
			boost::shared_ptr<AutomationControl> ac = (*i).lock();
			if (!ac) {
				continue;
			}
			cl->push_back (ac);
			ac->start_touch (timepos_t (ac->session().transport_sample()));
		}
		if (!cl->empty()) {
			session->set_controls (cl, 1.0, PBD::Controllable::NoGroup);
		}
	}
#else
	session->cancel_all_mute ();
#endif
}

void
FaderPort8::button_arm_all ()
{
	BasicUI::all_tracks_rec_in ();
}

/* access generic action */
void
FaderPort8::button_action (const std::string& group, const std::string& item)
{
	AccessAction (group, item);
}

/* ****************************************************************************
 * Control Interaction (encoder)
 */

void
FaderPort8::handle_encoder_pan (int steps)
{
	boost::shared_ptr<Stripable> s = first_selected_stripable();
	if (s) {
		boost::shared_ptr<AutomationControl> ac;
		if (shift_mod () || _ctrls.fader_mode() == ModePan) {
			ac = s->pan_width_control ();
		} else {
			ac = s->pan_azimuth_control ();
		}
		if (ac) {
			ac->start_touch (timepos_t (ac->session().transport_sample()));
			if (steps == 0) {
				ac->set_value (ac->normal(), PBD::Controllable::UseGroup);
			} else {
				double v = ac->internal_to_interface (ac->get_value(), true);
				v = std::max (0.0, std::min (1.0, v + steps * .01));
				ac->set_value (ac->interface_to_internal(v, true), PBD::Controllable::UseGroup);
			}
		}
	}
}

void
FaderPort8::handle_encoder_link (int steps)
{
	if (_link_control.expired ()) {
		return;
	}
	boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl> (_link_control.lock ());
	if (!ac) {
		return;
	}

	double v = ac->internal_to_interface (ac->get_value(), true);
	ac->start_touch (timepos_t (ac->session().transport_sample()));

	if (steps == 0) {
		ac->set_value (ac->normal(), PBD::Controllable::UseGroup);
		return;
	}

	if (ac->desc().toggled) {
		v = v > 0 ? 0. : 1.;
	} else if (ac->desc().integer_step) {
		v += steps / (1.f + ac->desc().upper - ac->desc().lower);
	} else if (ac->desc().enumeration) {
		ac->set_value (ac->desc().step_enum (ac->get_value(), steps < 0), PBD::Controllable::UseGroup);
		return;
	} else {
		v = std::max (0.0, std::min (1.0, v + steps * .01));
	}
	ac->set_value (ac->interface_to_internal(v, true), PBD::Controllable::UseGroup);
}


/* ****************************************************************************
 * Mode specific and internal callbacks
 */

/* handle "ARM" press -- act like shift, change "Select" button mode */
void
FaderPort8::button_arm (bool press)
{
#ifdef FADERPORT2
	boost::shared_ptr<Stripable> s = first_selected_stripable();
	if (press && s) {
		boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track>(s);
		if (t) {
			t->rec_enable_control()->set_value (!t->rec_enable_control()->get_value(), PBD::Controllable::UseGroup);
		}
	}
#else
	FaderMode fadermode = _ctrls.fader_mode ();
	if (fadermode == ModeTrack || fadermode == ModePan) {
		_ctrls.button (FP8Controls::BtnArm).set_active (press);
		ARMButtonChange (press); /* EMIT SIGNAL */
	}
#endif
}

void
FaderPort8::button_prev_next (bool next)
{
	switch (_ctrls.nav_mode()) {
		case NavChannel:
#ifndef FADERPORT2
			select_prev_next (next);
			break;
#endif
		case NavMaster:
		case NavScroll:
		case NavPan:
			bank (!next, false);
			break;
		case NavBank:
			bank (!next, true);
			break;
		case NavZoom:
			if (next) {
				VerticalZoomInSelected ();
			} else {
				VerticalZoomOutSelected ();
			}
			break;
		case NavSection:
			if (next) {
				AccessAction ("Region", "nudge-forward");
			} else {
				AccessAction ("Region", "nudge-backward");
			}
			break;
		case NavMarker:
			if (next) {
				next_marker ();
			} else {
				prev_marker ();
			}
			break;
	}
}

/* handle navigation encoder press */
void
FaderPort8::button_encoder ()
{
	/* special-case metronome level */
	if (_ctrls.button (FP8Controls::BtnClick).is_pressed ()) {
		Config->set_click_gain (1.0);
		_ctrls.button (FP8Controls::BtnClick).ignore_release();
		return;
	}
	switch (_ctrls.nav_mode()) {
		case NavZoom:
			ZoomToSession (); // XXX undo zoom
			break;
		case NavScroll:
			ZoomToSession ();
			break;
		case NavChannel:
			AccessAction ("Editor", "select-topmost");
			break;
		case NavBank:
			move_selected_into_view ();
			break;
		case NavMaster:
			{
				/* master || monitor level -- reset to 0dB */
				boost::shared_ptr<AutomationControl> ac;
				if (session->monitor_active() && !_ctrls.button (FP8Controls::BtnMaster).is_pressed ()) {
					ac = session->monitor_out()->gain_control ();
				} else if (session->master_out()) {
					ac = session->master_out()->gain_control ();
				}
				if (ac) {
					ac->start_touch (timepos_t (ac->session().transport_sample()));
					ac->set_value (ac->normal(), PBD::Controllable::NoGroup);
				}
			}
			break;
		case NavPan:
			break;
		case NavSection:
			// TODO nudge
			break;
		case NavMarker:
			{
				string markername;
				/* Don't add another mark if one exists within 1/100th of a second of
				 * the current position and we're not rolling.
				 */
				samplepos_t where = session->audible_sample();
				if (session->transport_stopped_or_stopping() && session->locations()->mark_at (timepos_t (where), timecnt_t (session->sample_rate() / 100.0))) {
					return;
				}

				session->locations()->next_available_name (markername,"mark");
				add_marker (markername);
			}
			break;
	}
}

/* handle navigation encoder turn */
void
FaderPort8::encoder_navigate (bool neg, int steps)
{
	/* special-case metronome level */
	if (_ctrls.button (FP8Controls::BtnClick).is_pressed ()) {
		// compare to ARDOUR_UI::click_button_scroll()
		gain_t gain = Config->get_click_gain();
		float gain_db = accurate_coefficient_to_dB (gain);
		gain_db += (neg ? -1.f : 1.f) * steps;
		gain_db = std::max (-60.f, gain_db);
		gain = dB_to_coefficient (gain_db);
		gain = std::min (gain, Config->get_max_gain());
		Config->set_click_gain (gain);
		_ctrls.button (FP8Controls::BtnClick).ignore_release();
		return;
	}

	switch (_ctrls.nav_mode()) {
		case NavChannel:
			if (neg) {
				AccessAction ("Mixer", "scroll-left");
				AccessAction ("Editor", "step-tracks-up");
			} else {
				AccessAction ("Mixer", "scroll-right");
				AccessAction ("Editor", "step-tracks-down");
			}
			break;
		case NavZoom:
			if (neg) {
				ZoomOut ();
			} else {
				ZoomIn ();
			}
			break;
		case NavMarker:
		case NavScroll:
			ScrollTimeline ((neg ? -1.f : 1.f) * steps / (shift_mod() ? 1024.f : 256.f));
			break;
		case NavBank:
			bank (neg, false);
			break;
		case NavMaster:
			{
				/* master || monitor level */
				boost::shared_ptr<AutomationControl> ac;
				if (session->monitor_active() && !_ctrls.button (FP8Controls::BtnMaster).is_pressed ()) {
					ac = session->monitor_out()->gain_control ();
				} else if (session->master_out()) {
					ac = session->master_out()->gain_control ();
				}
				if (ac) {
					double v = ac->internal_to_interface (ac->get_value());
					v = std::max (0.0, std::min (1.0, v + steps * (neg ? -.01 : .01)));
					ac->start_touch (timepos_t (ac->session().transport_sample()));
					ac->set_value (ac->interface_to_internal(v), PBD::Controllable::NoGroup);
				}
			}
			break;
		case NavSection:
			if (neg) {
				AccessAction ("Common", "nudge-playhead-backward");
			} else {
				AccessAction ("Common", "nudge-playhead-forward");
			}
			break;
		case NavPan:
			abort(); /*NOTREACHED*/
			break;
	}
}

/* handle pan/param encoder press */
void
FaderPort8::button_parameter ()
{
	switch (_ctrls.fader_mode()) {
		case ModeTrack:
		case ModePan:
			if (_link_enabled || _link_locked) {
				handle_encoder_link (0);
			} else {
				handle_encoder_pan (0);
			}
			break;
		case ModePlugins:
			toggle_preset_param_mode ();
			break;
		case ModeSend:
			break;
	}
}

/* handle pan/param encoder turn */
void
FaderPort8::encoder_parameter (bool neg, int steps)
{
	switch (_ctrls.fader_mode()) {
		case ModeTrack:
		case ModePan:
			if (steps != 0) {
				if (_link_enabled || _link_locked) {
					handle_encoder_link (neg ? -steps : steps);
				} else {
					handle_encoder_pan (neg ? -steps : steps);
				}
			}
			break;
		case ModePlugins:
		case ModeSend:
			while (steps > 0) {
				bank_param (neg, shift_mod());
				--steps;
			}
			break;
	}
}

/* handle user-specific actions */
void
FaderPort8::button_user (bool press, FP8Controls::ButtonId btn)
{
	_user_action_map[btn].call (*this, press);
}
