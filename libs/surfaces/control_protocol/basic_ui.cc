/*
 * Copyright (C) 2006-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2017 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2017-2019 Johannes Mueller <github@johannes-mueller.org>
 * Copyright (C) 2017 Len Ovens <len@ovenwerks.net>
 * Copyright (C) 2017 Tim Mayberry <mojofunk@gmail.com>
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

#include "pbd/pthread_utils.h"
#include "pbd/memento_command.h"

#include "temporal/tempo.h"

#include "ardour/audioengine.h"
#include "ardour/session.h"
#include "ardour/location.h"
#include "ardour/tempo.h"
#include "ardour/transport_master_manager.h"
#include "ardour/utils.h"

#include "control_protocol/basic_ui.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Temporal;

PBD::Signal2<void,std::string,std::string> BasicUI::AccessAction;

BasicUI::BasicUI (Session& s)
	: _session (&s)
{
}

BasicUI::~BasicUI ()
{

}

void
BasicUI::register_thread (std::string name)
{
	std::string pool_name = name;
	pool_name += " events";

	SessionEvent::create_per_thread_pool (pool_name, 64);
}

void
BasicUI::access_action ( std::string action_path )
{
	int split_at = action_path.find( "/" );
	std::string group = action_path.substr( 0, split_at );
	std::string item = action_path.substr( split_at + 1 );

	AccessAction( group, item );
}

void
BasicUI::loop_toggle ()
{
	if (!_session) {
		return;
	}

	Location * looploc = _session->locations()->auto_loop_location();

	if (!looploc) {
		return;
	}

	if (_session->get_play_loop()) {

		/* looping enabled, our job is to disable it */

		_session->request_play_loop (false);

	} else {

		/* looping not enabled, our job is to enable it.

		   loop-is-NOT-mode: this action always starts the transport rolling.
		   loop-IS-mode:     this action simply sets the loop play mechanism, but
		                        does not start transport.
		*/
		if (Config->get_loop_is_mode()) {
			_session->request_play_loop (true, false);
		} else {
			_session->request_play_loop (true, true);
		}
	}

	//show the loop markers
	looploc->set_hidden (false, this);
}

void
BasicUI::loop_location (timepos_t const & start, timepos_t const & end)
{
	Location* tll;
	if ((tll = _session->locations()->auto_loop_location()) == 0) {
		Location* loc = new Location (*_session, start, end, _("Loop"),  Location::IsAutoLoop);
		_session->locations()->add (loc, true);
		_session->set_auto_loop_location (loc);
	} else {
		tll->set_hidden (false, this);
		tll->set (start, end);
	}
}

void
BasicUI::goto_start (bool and_roll)
{
	_session->goto_start (and_roll);
}

void
BasicUI::goto_zero ()
{
	_session->request_locate (0);
}

void
BasicUI::goto_end ()
{
	_session->goto_end ();
}

void
BasicUI::add_marker (const std::string& markername)
{
	timepos_t where (_session->audible_sample());
	Location *location = new Location (*_session, where, where, markername, Location::IsMark);
	_session->begin_reversible_command (_("add marker"));
	XMLNode &before = _session->locations()->get_state();
	_session->locations()->add (location, true);
	XMLNode &after = _session->locations()->get_state();
	_session->add_command (new MementoCommand<Locations>(*(_session->locations()), &before, &after));
	_session->commit_reversible_command ();
}

void
BasicUI::remove_marker_at_playhead ()
{
	if (_session) {
		//set up for undo
		XMLNode &before = _session->locations()->get_state();
		bool removed = false;

		//find location(s) at this time
		Locations::LocationList locs;
		_session->locations()->find_all_between (timepos_t (_session->audible_sample()), timepos_t (_session->audible_sample()+1), locs, Location::Flags(0));
		for (Locations::LocationList::iterator i = locs.begin(); i != locs.end(); ++i) {
			if ((*i)->is_mark()) {
				_session->locations()->remove (*i);
				removed = true;
			}
		}

		//store undo
		if (removed) {
			_session->begin_reversible_command (_("remove marker"));
			XMLNode &after = _session->locations()->get_state();
			_session->add_command(new MementoCommand<Locations>(*(_session->locations()), &before, &after));
			_session->commit_reversible_command ();
		}
	}
}

void
BasicUI::button_varispeed (bool fwd)
{
	// incrementally increase speed by semitones
	// (keypress auto-repeat is 100ms)
	const float maxspeed = Config->get_shuttle_max_speed();
	float semitone_ratio = exp2f (1.0f/12.0f);
	const float octave_down = pow (1.0/semitone_ratio, 12.0);
	float transport_speed = get_transport_speed ();
	float speed;

	if (Config->get_rewind_ffwd_like_tape_decks()) {

		if (fwd) {
			if (transport_speed <= 0) {
				_session->request_transport_speed (1.0);
				_session->request_roll (TRS_UI);
				return;
			}
		} else {
			if (transport_speed >= 0) {
				_session->request_transport_speed (-1.0);
				_session->request_roll (TRS_UI);
				return;
			}
		}


	} else {

		if (fabs (transport_speed) <= 0.1) {

			/* close to zero, maybe flip direction */

			if (fwd) {
				if (transport_speed <= 0) {
					_session->request_transport_speed (1.0);
					_session->request_roll (TRS_UI);
				}
			} else {
				if (transport_speed >= 0) {
					_session->request_transport_speed (-1.0);
					_session->request_roll (TRS_UI);
				}
			}

			/* either we've just started, or we're moving as slowly as we
			 * ever should
			 */

			return;
		}

		if (fwd) {
			if (transport_speed < 0.f) {
				if (fabs (transport_speed) < octave_down) {
					/* we need to move the speed back towards zero */
					semitone_ratio = powf (1.f / semitone_ratio, 4.f);
				} else {
					semitone_ratio = 1.f / semitone_ratio;
				}
			} else {
				if (fabs (transport_speed) < octave_down) {
					/* moving very slowly, use 4 semitone steps */
					semitone_ratio = powf (semitone_ratio, 4.f);
				}
			}
		} else {
			if (transport_speed > 0.f) {
				/* we need to move the speed back towards zero */

				if (transport_speed < octave_down) {
					semitone_ratio = powf (1.f / semitone_ratio, 4.f);
				} else {
					semitone_ratio = 1.f / semitone_ratio;
				}
			} else {
				if (fabs (transport_speed) < octave_down) {
					/* moving very slowly, use 4 semitone steps */
					semitone_ratio = powf (semitone_ratio, 4.f);
				}
			}
		}
	}

	speed = semitone_ratio * transport_speed;
	speed = std::max (-maxspeed, std::min (maxspeed, speed));
	_session->request_transport_speed (speed);
	_session->request_roll (TRS_UI);
}

void
BasicUI::rewind ()
{
	button_varispeed (false);
}

void
BasicUI::ffwd ()
{
	button_varispeed (true);
}

void
BasicUI::transport_stop ()
{
	_session->request_stop ();
}

bool
BasicUI::stop_button_onoff () const
{
	return _session->transport_stopped_or_stopping ();
}

bool
BasicUI::play_button_onoff () const
{
	return get_transport_speed() == 1.0;
}

bool
BasicUI::ffwd_button_onoff () const
{
	return get_transport_speed() > 1.0;
}

bool
BasicUI::rewind_button_onoff () const
{
	return get_transport_speed() < 0.0;
}

bool
BasicUI::loop_button_onoff () const
{
	return _session->get_play_loop();
}

void
BasicUI::transport_play (bool from_last_start)
{
	/* ::toggle_roll() is smarter and preferred */

	if (!_session) {
		return;
	}

	if (_session->is_auditioning()) {
		return;
	}

	bool rolling = !transport_stopped_or_stopping();

	if (_session->get_play_loop()) {

		/* If loop playback is not a mode, then we should cancel
		   it when this action is requested. If it is a mode
		   we just leave it in place.
		*/

		if (!Config->get_loop_is_mode()) {
			/* XXX it is not possible to just leave seamless loop and keep
			   playing at present (nov 4th 2009)
			*/
			if (rolling) {
				/* stop loop playback but keep rolling */
				_session->request_play_loop (false, false);
			}
		}

	} else if (_session->get_play_range () ) {
		/* stop playing a range if we currently are */
		_session->request_play_range (0, true);
	}

	if (rolling) {
		_session->request_transport_speed (1.0, TRS_UI);
	} else {
		_session->request_roll ();
	}
}

void
BasicUI::rec_enable_toggle ()
{
	switch (_session->record_status()) {
	case RecordState::Disabled:
		if (_session->ntracks() == 0) {
			// string txt = _("Please create 1 or more track\nbefore trying to record.\nCheck the Session menu.");
			// MessageDialog msg (*editor, txt);
			// msg.run ();
			return;
		}
		_session->maybe_enable_record ();
		break;
	case RecordState::Recording:
	case RecordState::Enabled:
		_session->disable_record (false, true);
	}
}

void
BasicUI::all_tracks_rec_in ()
{
	_session->set_all_tracks_record_enabled (true);
}

void
BasicUI::all_tracks_rec_out ()
{
	_session->set_all_tracks_record_enabled (false);
}

void
BasicUI::save_state ()
{
	_session->save_state ("");
}

void
BasicUI::prev_marker ()
{
	timepos_t pos = _session->locations()->first_mark_before (timepos_t (_session->transport_sample()));

	if (pos >= 0) {
		_session->request_locate (pos.samples());
	} else {
		_session->goto_start ();
	}
}

void
BasicUI::next_marker ()
{
	timepos_t pos = _session->locations()->first_mark_after (timepos_t (_session->transport_sample()));

	if (pos >= 0) {
		_session->request_locate (pos.samples());
	} else {
		_session->goto_end();
	}
}

void
BasicUI::set_transport_speed (double speed)
{
	_session->request_transport_speed (speed);
}

double
BasicUI::get_transport_speed () const
{
	return _session->actual_speed ();
}

bool
BasicUI::transport_rolling () const
{
	return _session->transport_rolling ();
}

bool
BasicUI::transport_stopped_or_stopping () const
{
	return _session->transport_stopped_or_stopping ();
}

bool
BasicUI::get_play_loop () const
{
	return _session->get_play_loop ();
}

void
BasicUI::undo ()
{
	access_action ("Editor/undo");
}

void
BasicUI::redo ()
{
	access_action ("Editor/redo");
}

void
BasicUI::toggle_all_rec_enables ()
{
	if (_session->get_record_enabled()) {
		// _session->record_disenable_all ();
	} else {
		// _session->record_enable_all ();
	}
}

void
BasicUI::toggle_punch_in ()
{
	_session->config.set_punch_in (!_session->config.get_punch_in());
}

void
BasicUI::toggle_punch_out ()
{
	_session->config.set_punch_out (!_session->config.get_punch_out());
}

bool
BasicUI::get_record_enabled ()
{
	return _session->get_record_enabled();
}

RecordState
BasicUI::record_status () const
{
	return _session->record_status ();
}

bool
BasicUI::have_rec_enabled_track () const
{
	return _session->have_rec_enabled_track ();
}

void
BasicUI::set_record_enable (bool yn)
{
	if (yn) {
		_session->maybe_enable_record ();
	} else {
		_session->disable_record (false, true);
	}
}

samplepos_t
BasicUI::transport_sample () const
{
	return _session->transport_sample();
}

void
BasicUI::locate (samplepos_t where, LocateTransportDisposition ltd)
{
	_session->request_locate (where, ltd);
}

void
BasicUI::locate (samplepos_t where, bool roll)
{
	_session->request_locate (where, roll ? MustRoll : RollIfAppropriate);
}

void
BasicUI::jump_by_seconds (double secs, LocateTransportDisposition ltd)
{
	samplepos_t current = _session->transport_sample();
	double s = (double) current / (double) _session->nominal_sample_rate();

	s+= secs;
	if (s < 0) {
		s = 0;
	}
	s = s * _session->nominal_sample_rate();

	_session->request_locate (floor(s), ltd);
}

void
BasicUI::jump_by_bars (int bars, LocateTransportDisposition ltd)
{
	TempoMap::SharedPtr tmap (TempoMap::use());
	Temporal::BBT_Time bbt (tmap->bbt_at (timepos_t (_session->transport_sample())));

	bars += bbt.bars;
	if (bars < 0) {
		bars = 0;
	}

	AnyTime any;
	any.type = AnyTime::BBT;
	any.bbt.bars = bars;

	_session->request_locate (_session->convert_to_samples (any), ltd);
}

void
BasicUI::jump_by_beats (int beats, LocateTransportDisposition ltd)
{
	Beats qn_goal = timepos_t (_session->transport_sample ()).beats() + Beats (beats, 0);

	if (qn_goal < Beats()) {
		qn_goal = Beats();
	}
	_session->request_locate (timepos_t (qn_goal).samples());
}

void
BasicUI::toggle_monitor_mute ()
{
	if (_session->monitor_out()) {
		boost::shared_ptr<MonitorProcessor> mon = _session->monitor_out()->monitor_control();
		if (mon->cut_all ()) {
			mon->set_cut_all (false);
		} else {
			mon->set_cut_all (true);
		}
	}
}

void
BasicUI::toggle_monitor_dim ()
{
	if (_session->monitor_out()) {
		boost::shared_ptr<MonitorProcessor> mon = _session->monitor_out()->monitor_control();
		if (mon->dim_all ()) {
			mon->set_dim_all (false);
		} else {
			mon->set_dim_all (true);
		}
	}
}

void
BasicUI::toggle_monitor_mono ()
{
	if (_session->monitor_out()) {
		boost::shared_ptr<MonitorProcessor> mon = _session->monitor_out()->monitor_control();
		if (mon->mono()) {
			mon->set_mono (false);
		} else {
			mon->set_mono (true);
		}
	}
}

void
BasicUI::midi_panic ()
{
	_session->midi_panic ();
}

void
BasicUI::toggle_click ()
{
	bool state = !Config->get_clicking();
	Config->set_clicking (state);
}

void
BasicUI::toggle_roll (bool roll_out_of_bounded_mode)
{
	/* TO BE KEPT IN SYNC WITH ARDOUR_UI::toggle_roll() */

	if (!_session) {
		return;
	}

	if (_session->is_auditioning()) {
		_session->cancel_audition ();
		return;
	}

	if (_session->config.get_external_sync()) {
		switch (TransportMasterManager::instance().current()->type()) {
		case Engine:
			break;
		default:
			/* transport controlled by the master */
			return;
		}
	}

	bool rolling = !transport_stopped_or_stopping();

	if (rolling) {

		if (roll_out_of_bounded_mode) {
			/* drop out of loop/range playback but leave transport rolling */

			if (_session->get_play_loop()) {

				if (_session->actively_recording()) {
					/* actually stop transport because
					   otherwise the captured data will make
					   no sense.
					*/
					_session->request_play_loop (false, true);

				} else {
					_session->request_play_loop (false, false);
				}

			} else if (_session->get_play_range ()) {

				_session->request_cancel_play_range ();
			}

		} else {
			_session->request_stop (true, true);
		}

	} else { /* not rolling */

		if (_session->get_play_loop() && Config->get_loop_is_mode()) {
			_session->request_locate (_session->locations()->auto_loop_location()->start().samples(), MustRoll);
		} else {
			_session->request_roll (TRS_UI);
		}
	}
}

void
BasicUI::stop_forget ()
{
	_session->request_stop (true, true);
}

void BasicUI::mark_in () { access_action("Common/start-range-from-playhead"); }
void BasicUI::mark_out () { access_action("Common/finish-range-from-playhead"); }

void BasicUI::set_punch_range () { access_action("Editor/set-punch-from-edit-range"); }
void BasicUI::set_loop_range () { access_action("Editor/set-loop-from-edit-range"); }
void BasicUI::set_session_range () { access_action("Editor/set-session-from-edit-range"); }

void BasicUI::quick_snapshot_stay () { access_action("Main/QuickSnapshotStay"); }
void BasicUI::quick_snapshot_switch () { access_action("Main/QuickSnapshotSwitch"); }

void BasicUI::fit_1_track() { access_action("Editor/fit_1_track"); }
void BasicUI::fit_2_tracks() { access_action("Editor/fit_2_tracks"); }
void BasicUI::fit_4_tracks() { access_action("Editor/fit_4_tracks"); }
void BasicUI::fit_8_tracks() { access_action("Editor/fit_8_tracks"); }
void BasicUI::fit_16_tracks() { access_action("Editor/fit_16_tracks"); }
void BasicUI::fit_32_tracks() { access_action("Editor/fit_32_tracks"); }
void BasicUI::fit_all_tracks() { access_action("Editor/fit_all_tracks"); }

void BasicUI::zoom_10_ms() { access_action("Editor/zoom_10_ms"); }
void BasicUI::zoom_100_ms() { access_action("Editor/zoom_100_ms"); }
void BasicUI::zoom_1_sec() { access_action("Editor/zoom_1_sec"); }
void BasicUI::zoom_10_sec() { access_action("Editor/zoom_10_sec"); }
void BasicUI::zoom_1_min() { access_action("Editor/zoom_1_min"); }
void BasicUI::zoom_5_min() { access_action("Editor/zoom_5_min"); }
void BasicUI::zoom_10_min() { access_action("Editor/zoom_10_min"); }
void BasicUI::zoom_to_session() { access_action("Editor/zoom-to-session"); }
void BasicUI::temporal_zoom_in() { access_action("Editor/temporal-zoom-in"); }
void BasicUI::temporal_zoom_out() { access_action("Editor/temporal-zoom-out"); }

void BasicUI::scroll_up_1_track() { access_action("Editor/step-tracks-up"); }
void BasicUI::scroll_dn_1_track() { access_action("Editor/step-tracks-down"); }
void BasicUI::scroll_up_1_page() { access_action("Editor/scroll-tracks-up"); }
void BasicUI::scroll_dn_1_page() { access_action("Editor/scroll-tracks-down"); }


bool
BasicUI::locating ()
{
	return _session->locate_pending();
}

bool
BasicUI::locked ()
{
	return _session->transport_locked ();
}

ARDOUR::samplepos_t
BasicUI::engine_sample_time ()
{
	return _session->engine ().sample_time ();
}

ARDOUR::samplecnt_t
BasicUI::timecode_frames_per_hour ()
{
	return _session->timecode_frames_per_hour ();
}

void
BasicUI::timecode_time (samplepos_t where, Timecode::Time& timecode)
{
	_session->timecode_time (where, *((Timecode::Time *) &timecode));
}

void
BasicUI::timecode_to_sample (Timecode::Time& timecode, samplepos_t & sample, bool use_offset, bool use_subframes) const
{
	_session->timecode_to_sample (*((Timecode::Time*)&timecode), sample, use_offset, use_subframes);
}

void
BasicUI::sample_to_timecode (samplepos_t sample, Timecode::Time& timecode, bool use_offset, bool use_subframes) const
{
	_session->sample_to_timecode (sample, *((Timecode::Time*)&timecode), use_offset, use_subframes);
}

std::vector<boost::weak_ptr<AutomationControl>>
BasicUI::cancel_all_mute ()
{
	if (_session) {
		return _session->cancel_all_mute ();
	}

	return std::vector<boost::weak_ptr<AutomationControl>>{};
}

void
BasicUI::cancel_all_solo ()
{
	if (_session) {
		_session->cancel_all_solo ();
	}
}

struct SortLocationsByPosition {
    bool operator() (Location* a, Location* b) {
	    return a->start() < b->start();
    }
};

void
BasicUI::goto_nth_marker (int n)
{
	if (!_session) {
		return;
	}

	const Locations::LocationList& l (_session->locations()->list());
	Locations::LocationList ordered;
	ordered = l;

	SortLocationsByPosition cmp;
	ordered.sort (cmp);

	for (Locations::LocationList::iterator i = ordered.begin(); n >= 0 && i != ordered.end(); ++i) {
		if ((*i)->is_mark() && !(*i)->is_hidden() && !(*i)->is_session_range()) {
			if (n == 0) {
				_session->request_locate ((*i)->start().samples());
				break;
			}
			--n;
		}
	}
}

std::string
BasicUI::make_port_name_non_relative (const std::string& name) const
{
	return _session->engine ().make_port_name_non_relative (name);
}

const SessionConfiguration&
BasicUI::config () const
{
	return _session->config;
}

SessionConfiguration&
BasicUI::config ()
{
	return _session->config;
}

void
BasicUI::set_controls (
  boost::shared_ptr<ControlList>                   control_list,
  const double                                     val,
  const PBD::Controllable::GroupControlDisposition disposition)
{
	return _session->set_controls (control_list, val, disposition);
}

void
BasicUI::set_control (
  boost::shared_ptr<AutomationControl>             control,
  const double                                     val,
  const PBD::Controllable::GroupControlDisposition disposition)
{
	return _session->set_control (control, val, disposition);
}

boost::shared_ptr<Stripable>
BasicUI::monitor_out () const
{
	return _session->monitor_out ();
}

boost::shared_ptr<Stripable>
BasicUI::master_out () const
{
	return _session->master_out ();
}

boost::shared_ptr<Amp>
BasicUI::click_gain ()
{
	return _session->click_gain ();
}

const Locations*
BasicUI::locations () const
{
	return _session->locations ();
}

Locations*
BasicUI::locations ()
{
	return _session->locations ();
}

/* Signals */

PBD::Signal0<void>&
BasicUI::BundleAddedOrRemoved ()
{
	return _session->BundleAddedOrRemoved;
}

PBD::Signal0<void>&
BasicUI::DirtyChanged ()
{
	return _session->DirtyChanged;
}

PBD::Signal2<void, std::string, std::string>&
BasicUI::Exported ()
{
	return _session->Exported;
}

PBD::Signal1<void, bool>&
BasicUI::SoloActive ()
{
	return _session->SoloActive;
}

PBD::Signal0<void>&
BasicUI::SoloChanged ()
{
	return _session->SoloChanged;
}

PBD::Signal0<void>&
BasicUI::MuteChanged ()
{
	return _session->MuteChanged;
}

PBD::Signal0<void>&
BasicUI::RecordStateChanged ()
{
	return _session->RecordStateChanged;
}

PBD::Signal1<void, RouteList&>&
BasicUI::RouteAdded ()
{
	return _session->RouteAdded;
}

PBD::Signal1<void, RouteGroup*>&
BasicUI::RouteGroupPropertyChanged ()
{
	return _session->RouteGroupPropertyChanged;
}

PBD::Signal1<void, std::string>&
BasicUI::StateSaved ()
{
	return _session->StateSaved;
}

PBD::Signal0<void>&
BasicUI::TransportLooped ()
{
	return _session->TransportLooped;
}

PBD::Signal0<void>&
BasicUI::TransportStateChange ()
{
	return _session->TransportStateChange;
}
