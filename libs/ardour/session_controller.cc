/*
 * Copyright (C) 2006-2021 David Robillard <d@drobilla.net>
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

#include "ardour/session_controller.h"

#include "ardour/location.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/transport_master_manager.h"
#include "ardour/utils.h"
#include "pbd/i18n.h"
#include "pbd/memento_command.h"

using namespace ARDOUR;

/* Transport Control */

void
SessionController::loop_toggle ()
{
	if (!_session) {
		return;
	}

	Location* looploc = _session->locations ()->auto_loop_location ();

	if (!looploc) {
		return;
	}

	if (_session->get_play_loop ()) {
		/* looping enabled, our job is to disable it */

		_session->request_play_loop (false);

	} else {
		/* looping not enabled, our job is to enable it.

		   loop-is-NOT-mode: this action always starts the transport rolling.
		   loop-IS-mode:     this action simply sets the loop play mechanism,
		   but does not start transport.
		*/
		if (Config->get_loop_is_mode ()) {
			_session->request_play_loop (true, false);
		} else {
			_session->request_play_loop (true, true);
		}
	}

	// show the loop markers
	looploc->set_hidden (false, this);
}

void
SessionController::loop_location (samplepos_t start, samplepos_t end)
{
	Location* const tll = _session->locations ()->auto_loop_location ();
	if (!tll) {
		Location* loc = new Location (
		  *_session, start, end, _ ("Loop"), Location::IsAutoLoop);

		_session->locations ()->add (loc, true);
		_session->set_auto_loop_location (loc);
	} else {
		tll->set_hidden (false, this);
		tll->set (start, end);
	}
}

void
SessionController::button_varispeed (bool fwd)
{
	// incrementally increase speed by semitones
	// (keypress auto-repeat is 100ms)
	const float maxspeed        = Config->get_shuttle_max_speed ();
	float       semitone_ratio  = exp2f (1.0f / 12.0f);
	const float octave_down     = pow (1.0 / semitone_ratio, 12.0);
	float       transport_speed = get_transport_speed ();
	float       speed;

	if (Config->get_rewind_ffwd_like_tape_decks ()) {
		if (fwd) {
			if (transport_speed <= 0) {
				_session->request_transport_speed (1.0, false);
				_session->request_roll (TRS_UI);
				return;
			}
		} else {
			if (transport_speed >= 0) {
				_session->request_transport_speed (-1.0, false);
				_session->request_roll (TRS_UI);
				return;
			}
		}

	} else {
		if (fabs (transport_speed) <= 0.1) {
			/* close to zero, maybe flip direction */

			if (fwd) {
				if (transport_speed <= 0) {
					_session->request_transport_speed (1.0, false);
					_session->request_roll (TRS_UI);
				}
			} else {
				if (transport_speed >= 0) {
					_session->request_transport_speed (-1.0, false);
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
	_session->request_transport_speed (speed, false);
	_session->request_roll (TRS_UI);
}

void
SessionController::rewind ()
{
	button_varispeed (false);
}

void
SessionController::ffwd ()
{
	button_varispeed (true);
}

void
SessionController::transport_stop ()
{
	_session->request_stop ();
}

void
SessionController::transport_play (bool from_last_start)
{
	/* ::toggle_roll() is smarter and preferred */

	if (!_session) {
		return;
	}

	if (_session->is_auditioning ()) {
		return;
	}

	bool rolling = transport_rolling ();

	if (_session->get_play_loop ()) {
		/* If loop playback is not a mode, then we should cancel
		   it when this action is requested. If it is a mode
		   we just leave it in place.
		*/

		if (!Config->get_loop_is_mode ()) {
			/* XXX it is not possible to just leave seamless loop and keep
			   playing at present (nov 4th 2009)
			*/
			if (rolling) {
				/* stop loop playback but keep rolling */
				_session->request_play_loop (false, false);
			}
		}

	} else if (_session->get_play_range ()) {
		/* stop playing a range if we currently are */
		_session->request_play_range (0, true);
	}

	if (rolling) {
		_session->request_transport_speed (1.0, false, TRS_UI);
	} else {
		_session->request_roll ();
	}
}

void
SessionController::set_transport_speed (double speed)
{
	_session->request_transport_speed (speed);
}

void
SessionController::toggle_roll (bool with_abort, bool roll_out_of_bounded_mode)
{
	/* TO BE KEPT IN SYNC WITH ARDOUR_UI::toggle_roll() */

	if (!_session) {
		return;
	}

	if (_session->is_auditioning ()) {
		_session->cancel_audition ();
		return;
	}

	if (_session->config.get_external_sync ()) {
		switch (TransportMasterManager::instance ().current ()->type ()) {
		case Engine:
			break;
		default:
			/* transport controlled by the master */
			return;
		}
	}

	bool rolling = transport_rolling ();

	if (rolling) {
		if (roll_out_of_bounded_mode) {
			/* drop out of loop/range playback but leave transport rolling */

			if (_session->get_play_loop ()) {
				if (_session->actively_recording ()) {
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
			_session->request_stop (with_abort, true);
		}

	} else { /* not rolling */

		if (with_abort) { // Command was intended to stop transport, not start
			return;
		}

		if (_session->get_play_loop () && Config->get_loop_is_mode ()) {
			_session->request_locate (
			  _session->locations ()->auto_loop_location ()->start (),
			  MustRoll);
		} else {
			_session->request_roll (TRS_UI);
		}
	}
}

void
SessionController::stop_forget ()
{
	_session->request_stop (true, true);
}

double
SessionController::get_transport_speed () const
{
	return _session->actual_speed ();
}

bool
SessionController::transport_rolling () const
{
	return !_session->transport_stopped_or_stopping ();
}

samplepos_t
SessionController::transport_sample () const
{
	return _session->transport_sample ();
}

/* Markers */

void
SessionController::add_marker (const std::string& markername)
{
	const samplepos_t where = _session->audible_sample ();

	Location* location =
	  new Location (*_session, where, where, markername, Location::IsMark);

	_session->begin_reversible_command (_ ("add marker"));

	XMLNode& before = _session->locations ()->get_state ();
	_session->locations ()->add (location, true);
	XMLNode& after = _session->locations ()->get_state ();

	_session->add_command (new MementoCommand<Locations> (
	  *(_session->locations ()), &before, &after));

	_session->commit_reversible_command ();
}

void
SessionController::remove_marker_at_playhead ()
{
	if (_session) {
		// set up for undo
		XMLNode& before  = _session->locations ()->get_state ();
		bool     removed = false;

		// find location(s) at this time
		Locations::LocationList locs;
		_session->locations ()->find_all_between (_session->audible_sample (),
		                                          _session->audible_sample () +
		                                            1,
		                                          locs,
		                                          Location::Flags (0));
		for (Locations::LocationList::iterator i = locs.begin ();
		     i != locs.end ();
		     ++i) {
			if ((*i)->is_mark ()) {
				_session->locations ()->remove (*i);
				removed = true;
			}
		}

		// store undo
		if (removed) {
			_session->begin_reversible_command (_ ("remove marker"));
			XMLNode& after = _session->locations ()->get_state ();
			_session->add_command (new MementoCommand<Locations> (
			  *(_session->locations ()), &before, &after));
			_session->commit_reversible_command ();
		}
	}
}

/* Locating */

void
SessionController::goto_zero ()
{
	_session->request_locate (0);
}

void
SessionController::goto_start (bool and_roll)
{
	_session->goto_start (and_roll);
}

void
SessionController::goto_end ()
{
	_session->goto_end ();
}

struct SortLocationsByPosition {
	bool operator() (Location* a, Location* b)
	{
		return a->start () < b->start ();
	}
};

void
SessionController::goto_nth_marker (int n)
{
	if (!_session) {
		return;
	}

	const Locations::LocationList& l (_session->locations ()->list ());
	Locations::LocationList        ordered;
	ordered = l;

	SortLocationsByPosition cmp;
	ordered.sort (cmp);

	for (Locations::LocationList::iterator i = ordered.begin ();
	     n >= 0 && i != ordered.end ();
	     ++i) {
		if ((*i)->is_mark () && !(*i)->is_hidden () &&
		    !(*i)->is_session_range ()) {
			if (n == 0) {
				_session->request_locate ((*i)->start ());
				break;
			}
			--n;
		}
	}
}

void
SessionController::jump_by_seconds (double secs, LocateTransportDisposition ltd)
{
	samplepos_t current = _session->transport_sample ();
	double      s = (double)current / (double)_session->nominal_sample_rate ();

	s += secs;
	if (s < 0) {
		s = 0;
	}
	s = s * _session->nominal_sample_rate ();

	_session->request_locate (floor (s), ltd);
}

void
SessionController::jump_by_bars (double bars, LocateTransportDisposition ltd)
{
	TempoMap&          tmap (_session->tempo_map ());
	Timecode::BBT_Time bbt (tmap.bbt_at_sample (_session->transport_sample ()));

	bars += bbt.bars;
	if (bars < 0) {
		bars = 0;
	}

	AnyTime any;
	any.type     = AnyTime::BBT;
	any.bbt.bars = bars;

	_session->request_locate (_session->convert_to_samples (any), ltd);
}

void
SessionController::jump_by_beats (double beats, LocateTransportDisposition ltd)
{
	TempoMap& tmap (_session->tempo_map ());
	double    qn_goal =
	  tmap.quarter_note_at_sample (_session->transport_sample ()) + beats;
	if (qn_goal < 0.0) {
		qn_goal = 0.0;
	}
	_session->request_locate (tmap.sample_at_quarter_note (qn_goal), ltd);
}

void
SessionController::locate (samplepos_t where, LocateTransportDisposition ltd)
{
	_session->request_locate (where, ltd);
}

void
SessionController::locate (samplepos_t where, bool roll)
{
	_session->request_locate (where, roll ? MustRoll : RollIfAppropriate);
}

void
SessionController::prev_marker ()
{
	samplepos_t pos =
	  _session->locations ()->first_mark_before (_session->transport_sample ());

	if (pos >= 0) {
		_session->request_locate (pos);
	} else {
		_session->goto_start ();
	}
}

void
SessionController::next_marker ()
{
	samplepos_t pos =
	  _session->locations ()->first_mark_after (_session->transport_sample ());

	if (pos >= 0) {
		_session->request_locate (pos);
	} else {
		_session->goto_end ();
	}
}

bool
SessionController::locating () const
{
	return _session->locate_pending ();
}

bool
SessionController::locked () const
{
	return _session->transport_locked ();
}

/* State */

void
SessionController::save_state ()
{
	_session->save_state ("");
}

/* Monitoring */

void
SessionController::toggle_click ()
{
	bool state = !Config->get_clicking ();
	Config->set_clicking (state);
}

void
SessionController::midi_panic ()
{
	_session->midi_panic ();
}

void
SessionController::toggle_monitor_mute ()
{
	if (_session->monitor_out ()) {
		boost::shared_ptr<MonitorProcessor> mon =
		  _session->monitor_out ()->monitor_control ();
		if (mon->cut_all ()) {
			mon->set_cut_all (false);
		} else {
			mon->set_cut_all (true);
		}
	}
}

void
SessionController::toggle_monitor_dim ()
{
	if (_session->monitor_out ()) {
		boost::shared_ptr<MonitorProcessor> mon =
		  _session->monitor_out ()->monitor_control ();
		if (mon->dim_all ()) {
			mon->set_dim_all (false);
		} else {
			mon->set_dim_all (true);
		}
	}
}

void
SessionController::toggle_monitor_mono ()
{
	if (_session->monitor_out ()) {
		boost::shared_ptr<MonitorProcessor> mon =
		  _session->monitor_out ()->monitor_control ();
		if (mon->mono ()) {
			mon->set_mono (false);
		} else {
			mon->set_mono (true);
		}
	}
}

void
SessionController::cancel_all_solo ()
{
	if (_session) {
		_session->cancel_all_solo ();
	}
}

/* Recording */

void
SessionController::toggle_punch_in ()
{
	_session->config.set_punch_in (!_session->config.get_punch_in ());
}

void
SessionController::toggle_punch_out ()
{
	_session->config.set_punch_out (!_session->config.get_punch_out ());
}

void
SessionController::set_record_enable (bool yn)
{
	if (yn) {
		_session->maybe_enable_record ();
	} else {
		_session->disable_record (false, true);
	}
}

void
SessionController::rec_enable_toggle ()
{
	switch (_session->record_status ()) {
	case (RecordState)Disabled:
		if (_session->ntracks () > 0) {
			_session->maybe_enable_record ();
		}
		break;
	case (RecordState)Recording:
	case (RecordState)Enabled:
		_session->disable_record (false, true);
	}
}

void
SessionController::toggle_all_rec_enables ()
{
	if (_session->get_record_enabled ()) {
		// _session->record_disenable_all ();
	} else {
		// _session->record_enable_all ();
	}
}

void
SessionController::all_tracks_rec_in ()
{
	_session->set_all_tracks_record_enabled (true);
}

void
SessionController::all_tracks_rec_out ()
{
	_session->set_all_tracks_record_enabled (false);
}

bool
SessionController::get_record_enabled () const
{
	return _session->get_record_enabled ();
}

/* Time */

void
SessionController::timecode_time (samplepos_t where, Timecode::Time& timecode)
{
	_session->timecode_time (where, *((Timecode::Time*)&timecode));
}
