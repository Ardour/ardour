/*
    Copyright (C) 2000 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#include <cmath>
#include <fstream>

#include <sigc++/bind.h>
#include <pbd/xml++.h>

#include <ardour/timestamps.h>
#include <ardour/audioengine.h>
#include <ardour/route.h>
#include <ardour/insert.h>
#include <ardour/send.h>
#include <ardour/session.h>
#include <ardour/utils.h>
#include <ardour/configuration.h>
#include <ardour/cycle_timer.h>
#include <ardour/route_group.h>
#include <ardour/port.h>
#include <ardour/ladspa_plugin.h>
#include <ardour/panner.h>
#include <ardour/dB.h>
#include <ardour/mix.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;


uint32_t Route::order_key_cnt = 0;


Route::Route (Session& sess, string name, int input_min, int input_max, int output_min, int output_max, Flag flg, Buffer::Type default_type)
	: IO (sess, name, input_min, input_max, output_min, output_max, default_type),
	  _flags (flg),
	  _midi_solo_control (*this, MIDIToggleControl::SoloControl, _session.midi_port()),
	  _midi_mute_control (*this, MIDIToggleControl::MuteControl, _session.midi_port())
{
	init ();
}

Route::Route (Session& sess, const XMLNode& node)
	: IO (sess, "route"),
	  _midi_solo_control (*this, MIDIToggleControl::SoloControl, _session.midi_port()),
	  _midi_mute_control (*this, MIDIToggleControl::MuteControl, _session.midi_port())
{
	init ();
	set_state (node);
}

void
Route::init ()
{
	redirect_max_outs = 0;
	_muted = false;
	_soloed = false;
	_solo_safe = false;
	_phase_invert = false;
	order_keys[N_("signal")] = order_key_cnt++;
	_active = true;
	_silent = false;
	_meter_point = MeterPostFader;
	_initial_delay = 0;
	_roll_delay = 0;
	_own_latency = 0;
	_have_internal_generator = false;
	_declickable = false;
	_pending_declick = true;
	_remote_control_id = 0;

	_edit_group = 0;
	_mix_group = 0;

	_mute_affects_pre_fader = Config->get_mute_affects_pre_fader();
	_mute_affects_post_fader = Config->get_mute_affects_post_fader();
	_mute_affects_control_outs = Config->get_mute_affects_control_outs();
	_mute_affects_main_outs = Config->get_mute_affects_main_outs();
	
	solo_gain = 1.0;
	desired_solo_gain = 1.0;
	mute_gain = 1.0;
	desired_mute_gain = 1.0;

	_control_outs = 0;

	input_changed.connect (mem_fun (this, &Route::input_change_handler));
	output_changed.connect (mem_fun (this, &Route::output_change_handler));

	reset_midi_control (_session.midi_port(), _session.get_midi_control());
}

Route::~Route ()
{
	GoingAway (); /* EMIT SIGNAL */
	clear_redirects (this);

	if (_control_outs) {
		delete _control_outs;
	}
}

void
Route::set_remote_control_id (uint32_t id)
{
	if (id != _remote_control_id) {
		_remote_control_id = id;
		RemoteControlIDChanged ();
	}
}

uint32_t
Route::remote_control_id() const
{
	return _remote_control_id;
}

long
Route::order_key (string name) const
{
	OrderKeys::const_iterator i;
	
	if ((i = order_keys.find (name)) == order_keys.end()) {
		return -1;
	}

	return (*i).second;
}

void
Route::set_order_key (string name, long n)
{
	order_keys[name] = n;
	_session.set_dirty ();
}

void
Route::inc_gain (gain_t fraction, void *src)
{
	IO::inc_gain (fraction, src);
}

void
Route::set_gain (gain_t val, void *src)
{
	if (src != 0 && _mix_group && src != _mix_group && _mix_group->is_active()) {
		
		if (_mix_group->is_relative()) {
			
			
			gain_t usable_gain  = gain();
			if (usable_gain < 0.000001f) {
				usable_gain=0.000001f;
			}
						
			gain_t delta = val;
			if (delta < 0.000001f) {
				delta=0.000001f;
			}

			delta -= usable_gain;

			if (delta == 0.0f) return;

			gain_t factor = delta / usable_gain;

			if (factor > 0.0f) {
				factor = _mix_group->get_max_factor(factor);
				if (factor == 0.0f) {
					gain_changed (src);
					return;
				}
			} else {
				factor = _mix_group->get_min_factor(factor);
				if (factor == 0.0f) {
					gain_changed (src);
					return;
				}
			}
					
			_mix_group->apply (&Route::inc_gain, factor, _mix_group);

		} else {
			
			_mix_group->apply (&Route::set_gain, val, _mix_group);
		}

		return;
	} 

	if (val == gain()) {
		return;
	}

	IO::set_gain (val, src);
}

void
Route::process_output_buffers (vector<Sample*>& bufs, uint32_t nbufs,
			       jack_nframes_t start_frame, jack_nframes_t end_frame, 
			       jack_nframes_t nframes, jack_nframes_t offset, bool with_redirects, int declick,
			       bool meter)
{
	uint32_t n;
	RedirectList::iterator i;
	bool post_fader_work = false;
	bool mute_declick_applied = false;
	gain_t dmg, dsg, dg;
	vector<Sample*>::iterator bufiter;
	IO *co;
	bool mute_audible;
	bool solo_audible;
	bool no_monitor = (Config->get_use_hardware_monitoring() || !Config->get_use_sw_monitoring ());
	gain_t* gab = _session.gain_automation_buffer();

	declick = _pending_declick;

	{
		Glib::Mutex::Lock cm (control_outs_lock, Glib::TRY_LOCK);
		
		if (cm.locked()) {
			co = _control_outs;
		} else {
			co = 0;
		}
	}
	
	{ 
		Glib::Mutex::Lock dm (declick_lock, Glib::TRY_LOCK);
		
		if (dm.locked()) {
			dmg = desired_mute_gain;
			dsg = desired_solo_gain;
			dg = _desired_gain;
		} else {
			dmg = mute_gain;
			dsg = solo_gain;
			dg = _gain;
		}
	}

	/* ----------------------------------------------------------------------------------------------------
	   GLOBAL DECLICK (for transport changes etc.)
	   -------------------------------------------------------------------------------------------------- */

	if (declick > 0) {
		apply_declick (bufs, nbufs, nframes, 0.0, 1.0, _phase_invert);
		_pending_declick = 0;
	} else if (declick < 0) {
		apply_declick (bufs, nbufs, nframes, 1.0, 0.0, _phase_invert);
		_pending_declick = 0;
	} else {

		/* no global declick */

		if (solo_gain != dsg) {
			apply_declick (bufs, nbufs, nframes, solo_gain, dsg, _phase_invert);
			solo_gain = dsg;
		}
	}


	/* ----------------------------------------------------------------------------------------------------
	   INPUT METERING & MONITORING
	   -------------------------------------------------------------------------------------------------- */

	if (meter && (_meter_point == MeterInput)) {
		for (n = 0; n < nbufs; ++n) {
			_peak_power[n] = Session::compute_peak (bufs[n], nframes, _peak_power[n]); 
		}
	}

	if (!_soloed && _mute_affects_pre_fader && (mute_gain != dmg)) {
		apply_declick (bufs, nbufs, nframes, mute_gain, dmg, _phase_invert);
		mute_gain = dmg;
		mute_declick_applied = true;
	}

	if ((_meter_point == MeterInput) && co) {
		
		solo_audible = dsg > 0;
		mute_audible = dmg > 0;// || !_mute_affects_pre_fader;
		
		if (    // muted by solo of another track
			
			!solo_audible || 
			
			// muted by mute of this track 
			
			!mute_audible ||
			
			// rec-enabled but not s/w monitoring 
			
			// TODO: this is probably wrong

			(no_monitor && record_enabled() && (!_session.get_auto_input() || _session.actively_recording()))

			) {
			
			co->silence (nframes, offset);
			
		} else {

			co->deliver_output (bufs, nbufs, nframes, offset);
			
		} 
	} 

	/* ----------------------------------------------------------------------------------------------------
	   PRE-FADER REDIRECTS
	   -------------------------------------------------------------------------------------------------- */

	if (with_redirects) {
		Glib::RWLock::ReaderLock rm (redirect_lock, Glib::TRY_LOCK);
		if (rm.locked()) {
			if (mute_gain > 0 || !_mute_affects_pre_fader) {
				for (i = _redirects.begin(); i != _redirects.end(); ++i) {
					switch ((*i)->placement()) {
					case PreFader:
						(*i)->run (bufs, nbufs, nframes, offset);
						break;
					case PostFader:
						post_fader_work = true;
						break;
					}
				}
			} else {
				for (i = _redirects.begin(); i != _redirects.end(); ++i) {
					switch ((*i)->placement()) {
					case PreFader:
						(*i)->silence (nframes, offset);
						break;
					case PostFader:
						post_fader_work = true;
						break;
					}
				}
			}
		} 
	}


	if (!_soloed && (mute_gain != dmg) && !mute_declick_applied && _mute_affects_post_fader) {
		apply_declick (bufs, nbufs, nframes, mute_gain, dmg, _phase_invert);
		mute_gain = dmg;
		mute_declick_applied = true;
	}

	/* ----------------------------------------------------------------------------------------------------
	   PRE-FADER METERING & MONITORING
	   -------------------------------------------------------------------------------------------------- */

	if (meter && (_meter_point == MeterPreFader)) {
		for (n = 0; n < nbufs; ++n) {
			_peak_power[n] = Session::compute_peak (bufs[n], nframes, _peak_power[n]);
		}
	}

	
	if ((_meter_point == MeterPreFader) && co) {
		
		solo_audible = dsg > 0;
		mute_audible = dmg > 0 || !_mute_affects_pre_fader;
		
		if ( // muted by solo of another track
			
			!solo_audible || 
			
			// muted by mute of this track 
			
			!mute_audible ||
			
			// rec-enabled but not s/w monitoring 
			
			(no_monitor && record_enabled() && (!_session.get_auto_input() || _session.actively_recording()))

			) {
			
			co->silence (nframes, offset);
			
		} else {

			co->deliver_output (bufs, nbufs, nframes, offset);
			
		} 
	} 
	
	/* ----------------------------------------------------------------------------------------------------
	   GAIN STAGE
	   -------------------------------------------------------------------------------------------------- */

	/* if not recording or recording and requiring any monitor signal, then apply gain */

	if ( // not recording 

		!(record_enabled() && _session.actively_recording()) || 
		
	    // OR recording 
		
		// h/w monitoring not in use 
		
		(!Config->get_use_hardware_monitoring() && 

		 // AND software monitoring required

		 Config->get_use_sw_monitoring())) { 
		
		if (apply_gain_automation) {
			
			if (_phase_invert) {
				for (n = 0; n < nbufs; ++n)  {
					Sample *sp = bufs[n];
					
					for (jack_nframes_t nx = 0; nx < nframes; ++nx) {
						sp[nx] *= -gab[nx];
					}
				}
			} else {
				for (n = 0; n < nbufs; ++n) {
					Sample *sp = bufs[n];
					
					for (jack_nframes_t nx = 0; nx < nframes; ++nx) {
						sp[nx] *= gab[nx];
					}
				}
			}
			
			if (apply_gain_automation && _session.transport_rolling() && nframes > 0) {
				_effective_gain = gab[nframes-1];
			}
			
		} else {
			
			/* manual (scalar) gain */
			
			if (_gain != dg) {
				
				apply_declick (bufs, nbufs, nframes, _gain, dg, _phase_invert);
				_gain = dg;
				
			} else if (_gain != 0 && (_phase_invert || _gain != 1.0)) {
				
				/* no need to interpolate current gain value,
				   but its non-unity, so apply it. if the gain
				   is zero, do nothing because we'll ship silence
				   below.
				*/

				gain_t this_gain;
				
				if (_phase_invert) {
					this_gain = -_gain;
				} else {
					this_gain = _gain;
				}
				
				for (n = 0; n < nbufs; ++n) {
					Sample *sp = bufs[n];
					apply_gain_to_buffer(sp,nframes,this_gain);
				}

			} else if (_gain == 0) {
				for (n = 0; n < nbufs; ++n) {
					memset (bufs[n], 0, sizeof (Sample) * nframes);
				}
			}
		}

	} else {

		/* actively recording, no monitoring required; leave buffers as-is to save CPU cycles */

	}

	/* ----------------------------------------------------------------------------------------------------
	   POST-FADER REDIRECTS
	   -------------------------------------------------------------------------------------------------- */

	/* note that post_fader_work cannot be true unless with_redirects was also true, so don't test both */

	if (post_fader_work) {

		Glib::RWLock::ReaderLock rm (redirect_lock, Glib::TRY_LOCK);
		if (rm.locked()) {
			if (mute_gain > 0 || !_mute_affects_post_fader) {
				for (i = _redirects.begin(); i != _redirects.end(); ++i) {
					switch ((*i)->placement()) {
					case PreFader:
						break;
					case PostFader:
						(*i)->run (bufs, nbufs, nframes, offset);
						break;
					}
				}
			} else {
				for (i = _redirects.begin(); i != _redirects.end(); ++i) {
					switch ((*i)->placement()) {
					case PreFader:
						break;
					case PostFader:
						(*i)->silence (nframes, offset);
						break;
					}
				}
			}
		} 
	}

	if (!_soloed && (mute_gain != dmg) && !mute_declick_applied && _mute_affects_control_outs) {
		apply_declick (bufs, nbufs, nframes, mute_gain, dmg, _phase_invert);
		mute_gain = dmg;
		mute_declick_applied = true;
	}

	/* ----------------------------------------------------------------------------------------------------
	   CONTROL OUTPUT STAGE
	   -------------------------------------------------------------------------------------------------- */

	if ((_meter_point == MeterPostFader) && co) {
		
		solo_audible = solo_gain > 0;
		mute_audible = dmg > 0 || !_mute_affects_control_outs;

		if ( // silent anyway

			(_gain == 0 && !apply_gain_automation) || 
		    
                     // muted by solo of another track

			!solo_audible || 
		    
                     // muted by mute of this track 

			!mute_audible ||

		    // recording but not s/w monitoring 
			
			(no_monitor && record_enabled() && (!_session.get_auto_input() || _session.actively_recording()))

			) {

			co->silence (nframes, offset);
			
		} else {

			co->deliver_output_no_pan (bufs, nbufs, nframes, offset);
		} 
	} 

	/* ----------------------------------------------------------------------
	   GLOBAL MUTE 
	   ----------------------------------------------------------------------*/

	if (!_soloed && (mute_gain != dmg) && !mute_declick_applied && _mute_affects_main_outs) {
		apply_declick (bufs, nbufs, nframes, mute_gain, dmg, _phase_invert);
		mute_gain = dmg;
		mute_declick_applied = true;
	}
	
	/* ----------------------------------------------------------------------------------------------------
	   MAIN OUTPUT STAGE
	   -------------------------------------------------------------------------------------------------- */

	solo_audible = dsg > 0;
	mute_audible = dmg > 0 || !_mute_affects_main_outs;
	
	if (n_outputs() == 0) {
	    
	    /* relax */

	} else if (no_monitor && record_enabled() && (!_session.get_auto_input() || _session.actively_recording())) {
		
		IO::silence (nframes, offset);
		
	} else {

		if ( // silent anyway

		    (_gain == 0 && !apply_gain_automation) ||
		    
		    // muted by solo of another track, but not using control outs for solo

		    (!solo_audible && (_session.solo_model() != Session::SoloBus)) ||
		    
		    // muted by mute of this track

		    !mute_audible

			) {

			/* don't use Route::silence() here, because that causes
			   all outputs (sends, port inserts, etc. to be silent).
			*/
			
			if (_meter_point == MeterPostFader) {
				reset_peak_meters ();
			}
			
			IO::silence (nframes, offset);
			
		} else {
			
			if (_session.transport_speed() > 1.5f || _session.transport_speed() < -1.5f) {
				pan (bufs, nbufs, nframes, offset, speed_quietning); 
			} else {
				// cerr << _name << " panner state = " << _panner->automation_state() << endl;
				if (!_panner->empty() &&
				    (_panner->automation_state() & Play ||
				     ((_panner->automation_state() & Touch) && !_panner->touching()))) {
					pan_automated (bufs, nbufs, start_frame, end_frame, nframes, offset);
				} else {
					pan (bufs, nbufs, nframes, offset, 1.0); 
				}
			}
		}

	}

	/* ----------------------------------------------------------------------------------------------------
	   POST-FADER METERING
	   -------------------------------------------------------------------------------------------------- */

	if (meter && (_meter_point == MeterPostFader)) {
//		cerr << "meter post" << endl;

		if ((_gain == 0 && !apply_gain_automation) || dmg == 0) {
			uint32_t no = n_outputs();
			for (n = 0; n < no; ++n) {
				_peak_power[n] = 0;
			} 
		} else {
			uint32_t no = n_outputs();
			for (n = 0; n < no; ++n) {
				_peak_power[n] = Session::compute_peak (output(n)->get_buffer (nframes) + offset, nframes, _peak_power[n]);
			}
		}
	}
}

uint32_t
Route::n_process_buffers ()
{
	return max (n_inputs(), redirect_max_outs);
}

void

Route::passthru (jack_nframes_t start_frame, jack_nframes_t end_frame, jack_nframes_t nframes, jack_nframes_t offset, int declick, bool meter_first)
{
	vector<Sample*>& bufs = _session.get_passthru_buffers();
	uint32_t limit = n_process_buffers ();

	_silent = false;

	collect_input (bufs, limit, nframes, offset);

#define meter_stream meter_first

	if (meter_first) {
		for (uint32_t n = 0; n < limit; ++n) {
			_peak_power[n] = Session::compute_peak (bufs[n], nframes, _peak_power[n]);
		}
		meter_stream = false;
	} else {
		meter_stream = true;
	}
		
	process_output_buffers (bufs, limit, start_frame, end_frame, nframes, offset, true, declick, meter_stream);

#undef meter_stream
}

void
Route::set_phase_invert (bool yn, void *src)
{
	if (_phase_invert != yn) {
		_phase_invert = yn;
	}
	//  phase_invert_changed (src); /* EMIT SIGNAL */
}

void
Route::set_solo (bool yn, void *src)
{
	if (_solo_safe) {
		return;
	}

	if (_mix_group && src != _mix_group && _mix_group->is_active()) {
		_mix_group->apply (&Route::set_solo, yn, _mix_group);
		return;
	}

	if (_soloed != yn) {
		_soloed = yn;
		 solo_changed (src); /* EMIT SIGNAL */

		 if (_session.get_midi_feedback()) {
			 _midi_solo_control.send_feedback (_soloed);
		 }
	}
}

void
Route::set_solo_mute (bool yn)
{
	Glib::Mutex::Lock lm (declick_lock);

	/* Called by Session in response to another Route being soloed.
	 */
	   
	desired_solo_gain = (yn?0.0:1.0);
}

void
Route::set_solo_safe (bool yn, void *src)
{
	if (_solo_safe != yn) {
		_solo_safe = yn;
		 solo_safe_changed (src); /* EMIT SIGNAL */
	}
}

void
Route::set_mute (bool yn, void *src)

{
	if (_mix_group && src != _mix_group && _mix_group->is_active()) {
		_mix_group->apply (&Route::set_mute, yn, _mix_group);
		return;
	}

	if (_muted != yn) {
		_muted = yn;
		mute_changed (src); /* EMIT SIGNAL */
		
		if (_session.get_midi_feedback()) {
			_midi_mute_control.send_feedback (_muted);
		}
		
		Glib::Mutex::Lock lm (declick_lock);
		desired_mute_gain = (yn?0.0f:1.0f);
	}
}

int
Route::add_redirect (Redirect *redirect, void *src, uint32_t* err_streams)
{
	uint32_t old_rmo = redirect_max_outs;

	if (!_session.engine().connected()) {
		return 1;
	}

	{
		Glib::RWLock::WriterLock lm (redirect_lock);

		PluginInsert* pi;
		PortInsert* porti;

		uint32_t potential_max_streams = 0;

		if ((pi = dynamic_cast<PluginInsert*>(redirect)) != 0) {
			pi->set_count (1);

			if (pi->input_streams() == 0) {
				/* instrument plugin */
				_have_internal_generator = true;
			}

			potential_max_streams = max(pi->input_streams(), pi->output_streams());

		} else if ((porti = dynamic_cast<PortInsert*>(redirect)) != 0) {

			/* force new port inserts to start out with an i/o configuration
			   that matches this route's i/o configuration.

			   the "inputs" for the port are supposed to match the output
			   of this route.

			   the "outputs" of the route should match the inputs of this
			   route. XXX shouldn't they match the number of active signal
			   streams at the point of insertion?
			   
			*/

			porti->ensure_io (n_outputs (), n_inputs(), false, this);
		}

		// Ensure peak vector sizes before the plugin is activated
		while (_peak_power.size() < potential_max_streams) {
			_peak_power.push_back(0);
		}
		while (_visible_peak_power.size() < potential_max_streams) {
			_visible_peak_power.push_back(0);
		}

		_redirects.push_back (redirect);

		if (_reset_plugin_counts (err_streams)) {
			_redirects.pop_back ();
			_reset_plugin_counts (0); // it worked before we tried to add it ...
			return -1;
		}

		redirect->activate ();
		redirect->active_changed.connect (mem_fun (*this, &Route::redirect_active_proxy));
	}
	
	if (redirect_max_outs != old_rmo || old_rmo == 0) {
		reset_panner ();
	}


	redirects_changed (src); /* EMIT SIGNAL */
	return 0;
}

int
Route::add_redirects (const RedirectList& others, void *src, uint32_t* err_streams)
{
	uint32_t old_rmo = redirect_max_outs;

	if (!_session.engine().connected()) {
		return 1;
	}

	{
		Glib::RWLock::WriterLock lm (redirect_lock);

		RedirectList::iterator existing_end = _redirects.end();
		--existing_end;

		uint32_t potential_max_streams = 0;

		for (RedirectList::const_iterator i = others.begin(); i != others.end(); ++i) {
			
			PluginInsert* pi;
			
			if ((pi = dynamic_cast<PluginInsert*>(*i)) != 0) {
				pi->set_count (1);
				
				uint32_t m = max(pi->input_streams(), pi->output_streams());
				if (m > potential_max_streams)
					potential_max_streams = m;
			}

			// Ensure peak vector sizes before the plugin is activated
			while (_peak_power.size() < potential_max_streams) {
				_peak_power.push_back(0);
			}
			while (_visible_peak_power.size() < potential_max_streams) {
				_visible_peak_power.push_back(0);
			}

			_redirects.push_back (*i);
			
			if (_reset_plugin_counts (err_streams)) {
				++existing_end;
				_redirects.erase (existing_end, _redirects.end());
				_reset_plugin_counts (0); // it worked before we tried to add it ...
				return -1;
			}
			
			(*i)->activate ();
			(*i)->active_changed.connect (mem_fun (*this, &Route::redirect_active_proxy));
		}
	}
	
	if (redirect_max_outs != old_rmo || old_rmo == 0) {
		reset_panner ();
	}

	redirects_changed (src); /* EMIT SIGNAL */
	return 0;
}

void
Route::clear_redirects (void *src)
{
	uint32_t old_rmo = redirect_max_outs;

	if (!_session.engine().connected()) {
		return;
	}

	{
		Glib::RWLock::WriterLock lm (redirect_lock);

		for (RedirectList::iterator i = _redirects.begin(); i != _redirects.end(); ++i) {
			delete *i;
		}

		_redirects.clear ();
	}

	if (redirect_max_outs != old_rmo) {
		reset_panner ();
	}
	
	redirect_max_outs = 0;
	_have_internal_generator = false;
	redirects_changed (src); /* EMIT SIGNAL */
}

int
Route::remove_redirect (Redirect *redirect, void *src, uint32_t* err_streams)
{
	uint32_t old_rmo = redirect_max_outs;

	if (!_session.engine().connected()) {
		return 1;
	}

	redirect_max_outs = 0;

	{
		Glib::RWLock::WriterLock lm (redirect_lock);
		RedirectList::iterator i;
		bool removed = false;

		for (i = _redirects.begin(); i != _redirects.end(); ++i) {
			if (*i == redirect) {

				RedirectList::iterator tmp;

				/* move along, see failure case for reset_plugin_counts()
				   where we may need to reinsert the redirect.
				*/

				tmp = i;
				++tmp;

				/* stop redirects that send signals to JACK ports
				   from causing noise as a result of no longer being
				   run.
				*/

				Send* send;
				PortInsert* port_insert;
				
				if ((send = dynamic_cast<Send*> (*i)) != 0) {
					send->disconnect_inputs (this);
					send->disconnect_outputs (this);
				} else if ((port_insert = dynamic_cast<PortInsert*> (*i)) != 0) {
					port_insert->disconnect_inputs (this);
					port_insert->disconnect_outputs (this);
				}

				_redirects.erase (i);

				i = tmp;
				removed = true;
				break;
			}
		}

		if (!removed) {
			/* what? */
			return 1;
		}

		if (_reset_plugin_counts (err_streams)) {
			/* get back to where we where */
			_redirects.insert (i, redirect);
			/* we know this will work, because it worked before :) */
			_reset_plugin_counts (0);
			return -1;
		}

		bool foo = false;

		for (i = _redirects.begin(); i != _redirects.end(); ++i) {
			PluginInsert* pi;

			if ((pi = dynamic_cast<PluginInsert*>(*i)) != 0) {
				if (pi->is_generator()) {
					foo = true;
				}
			}
		}

		_have_internal_generator = foo;
	}

	if (old_rmo != redirect_max_outs) {
		reset_panner ();
	}

	redirects_changed (src); /* EMIT SIGNAL */
	return 0;
}

int
Route::reset_plugin_counts (uint32_t* lpc)
{
	Glib::RWLock::WriterLock lm (redirect_lock);
	return _reset_plugin_counts (lpc);
}


int
Route::_reset_plugin_counts (uint32_t* err_streams)
{
	RedirectList::iterator r;
	uint32_t i_cnt;
	uint32_t s_cnt;
	map<Placement,list<InsertCount> > insert_map;
	jack_nframes_t initial_streams;

	redirect_max_outs = 0;
	i_cnt = 0;
	s_cnt = 0;

	/* divide inserts up by placement so we get the signal flow
	   properly modelled. we need to do this because the _redirects
	   list is not sorted by placement, and because other reasons may 
	   exist now or in the future for this separate treatment.
	*/
	
	for (r = _redirects.begin(); r != _redirects.end(); ++r) {

		Insert *insert;

		/* do this here in case we bomb out before we get to the end of
		   this function.
		*/

		redirect_max_outs = max ((*r)->output_streams (), redirect_max_outs);

		if ((insert = dynamic_cast<Insert*>(*r)) != 0) {
			++i_cnt;
			insert_map[insert->placement()].push_back (InsertCount (*insert));

			/* reset plugin counts back to one for now so
			   that we have a predictable, controlled
			   state to try to configure.
			*/

			PluginInsert* pi;
		
			if ((pi = dynamic_cast<PluginInsert*>(insert)) != 0) {
				pi->set_count (1);
			}

		} else if (dynamic_cast<Send*> (*r) != 0) {
			++s_cnt;
		}
	}
	
	if (i_cnt == 0) {
		if (s_cnt) {
			goto recompute;
		} else {
			return 0;
		}
	}

	/* Now process each placement in order, checking to see if we 
	   can really do what has been requested.
	*/

	/* A: PreFader */
	
	if (check_some_plugin_counts (insert_map[PreFader], n_inputs (), err_streams)) {
		return -1;
	}

	/* figure out the streams that will feed into PreFader */

	if (!insert_map[PreFader].empty()) {
		InsertCount& ic (insert_map[PreFader].back());
		initial_streams = ic.insert.compute_output_streams (ic.cnt);
	} else {
		initial_streams = n_inputs ();
	}

	/* B: PostFader */

	if (check_some_plugin_counts (insert_map[PostFader], initial_streams, err_streams)) {
		return -1;
	}

	/* OK, everything can be set up correctly, so lets do it */

	apply_some_plugin_counts (insert_map[PreFader]);
	apply_some_plugin_counts (insert_map[PostFader]);

	/* recompute max outs of any redirect */

  recompute:

	redirect_max_outs = 0;
	RedirectList::iterator prev = _redirects.end();

	for (r = _redirects.begin(); r != _redirects.end(); prev = r, ++r) {
		Send* s;

		if ((s = dynamic_cast<Send*> (*r)) != 0) {
			if (r == _redirects.begin()) {
				s->expect_inputs (n_inputs());
			} else {
				s->expect_inputs ((*prev)->output_streams());
			}
		}

		redirect_max_outs = max ((*r)->output_streams (), redirect_max_outs);
	}

	/* we're done */

	return 0;
}				   

int32_t
Route::apply_some_plugin_counts (list<InsertCount>& iclist)
{
	list<InsertCount>::iterator i;

	for (i = iclist.begin(); i != iclist.end(); ++i) {
		
		if ((*i).insert.configure_io ((*i).cnt, (*i).in, (*i).out)) {
			return -1;
		}
		/* make sure that however many we have, they are all active */
		(*i).insert.activate ();
	}

	return 0;
}

int32_t
Route::check_some_plugin_counts (list<InsertCount>& iclist, int32_t required_inputs, uint32_t* err_streams)
{
	list<InsertCount>::iterator i;
	
	for (i = iclist.begin(); i != iclist.end(); ++i) {

		if (((*i).cnt = (*i).insert.can_support_input_configuration (required_inputs)) < 0) {
			if (err_streams) {
				*err_streams = required_inputs;
			}
			return -1;
		}
		
		(*i).in = required_inputs;
		(*i).out = (*i).insert.compute_output_streams ((*i).cnt);

		required_inputs = (*i).out;
	}

	return 0;
}

int
Route::copy_redirects (const Route& other, Placement placement, uint32_t* err_streams)
{
	uint32_t old_rmo = redirect_max_outs;

	if (err_streams) {
		*err_streams = 0;
	}

	RedirectList to_be_deleted;

	{
		Glib::RWLock::WriterLock lm (redirect_lock);
		RedirectList::iterator tmp;
		RedirectList the_copy;

		the_copy = _redirects;
		
		/* remove all relevant redirects */

		for (RedirectList::iterator i = _redirects.begin(); i != _redirects.end(); ) {
			tmp = i;
			++tmp;

			if ((*i)->placement() == placement) {
				to_be_deleted.push_back (*i);
				_redirects.erase (i);
			}

			i = tmp;
		}

		/* now copy the relevant ones from "other" */
		
		for (RedirectList::const_iterator i = other._redirects.begin(); i != other._redirects.end(); ++i) {
			if ((*i)->placement() == placement) {
				_redirects.push_back (Redirect::clone (**i));
			}
		}

		/* reset plugin stream handling */

		if (_reset_plugin_counts (err_streams)) {

			/* FAILED COPY ATTEMPT: we have to restore order */

			/* delete all cloned redirects */

			for (RedirectList::iterator i = _redirects.begin(); i != _redirects.end(); ) {

				tmp = i;
				++tmp;

				if ((*i)->placement() == placement) {
					delete *i;
					_redirects.erase (i);
				}
				
				i = tmp;
			}

			/* restore the natural order */

			_redirects = the_copy;
			redirect_max_outs = old_rmo;

			/* we failed, even though things are OK again */

			return -1;

		} else {
			
			/* SUCCESSFUL COPY ATTEMPT: delete the redirects we removed pre-copy */

			for (RedirectList::iterator i = to_be_deleted.begin(); i != to_be_deleted.end(); ++i) {
				delete *i;
			}
		}
	}

	if (redirect_max_outs != old_rmo || old_rmo == 0) {
		reset_panner ();
	}

	redirects_changed (this); /* EMIT SIGNAL */
	return 0;
}

void
Route::all_redirects_flip ()
{
	Glib::RWLock::ReaderLock lm (redirect_lock);

	if (_redirects.empty()) {
		return;
	}

	bool first_is_on = _redirects.front()->active();
	
	for (RedirectList::iterator i = _redirects.begin(); i != _redirects.end(); ++i) {
		(*i)->set_active (!first_is_on, this);
	}
}

void
Route::all_redirects_active (bool state)
{
	Glib::RWLock::ReaderLock lm (redirect_lock);

	if (_redirects.empty()) {
		return;
	}

	for (RedirectList::iterator i = _redirects.begin(); i != _redirects.end(); ++i) {
		(*i)->set_active (state, this);
	}
}

struct RedirectSorter {
    bool operator() (const Redirect *a, const Redirect *b) {
	    return a->sort_key() < b->sort_key();
    }
};

int
Route::sort_redirects (uint32_t* err_streams)
{
	{
		RedirectSorter comparator;
		Glib::RWLock::WriterLock lm (redirect_lock);
		uint32_t old_rmo = redirect_max_outs;

		/* the sweet power of C++ ... */

		RedirectList as_it_was_before = _redirects;

		_redirects.sort (comparator);
	
		if (_reset_plugin_counts (err_streams)) {
			_redirects = as_it_was_before;
			redirect_max_outs = old_rmo;
			return -1;
		} 
	} 

	reset_panner ();
	redirects_changed (this); /* EMIT SIGNAL */

	return 0;
}

XMLNode&
Route::get_state()
{
	return state(true);
}

XMLNode&
Route::get_template()
{
	return state(false);
}

XMLNode&
Route::state(bool full_state)
{
	XMLNode *node = new XMLNode("Route");
	XMLNode *aevents;
	RedirectList:: iterator i;
	char buf[32];

	if (_flags) {
		snprintf (buf, sizeof (buf), "0x%x", _flags);
		node->add_property("flags", buf);
	}
	node->add_property("active", _active?"yes":"no");
	node->add_property("muted", _muted?"yes":"no");
	node->add_property("soloed", _soloed?"yes":"no");
	node->add_property("phase-invert", _phase_invert?"yes":"no");
	node->add_property("mute-affects-pre-fader", _mute_affects_pre_fader?"yes":"no"); 
	node->add_property("mute-affects-post-fader", _mute_affects_post_fader?"yes":"no"); 
	node->add_property("mute-affects-control-outs", _mute_affects_control_outs?"yes":"no"); 
	node->add_property("mute-affects-main-outs", _mute_affects_main_outs?"yes":"no"); 

	if (_edit_group) {
		node->add_property("edit-group", _edit_group->name());
	}
	if (_mix_group) {
		node->add_property("mix-group", _mix_group->name());
	}

	/* MIDI control */

	MIDI::channel_t chn;
	MIDI::eventType ev;
	MIDI::byte      additional;
	XMLNode*        midi_node = 0;
	XMLNode*        child;

	midi_node = node->add_child ("MIDI");
	
	if (_midi_mute_control.get_control_info (chn, ev, additional)) {
		child = midi_node->add_child ("mute");
		set_midi_node_info (child, ev, chn, additional);
	}
	if (_midi_solo_control.get_control_info (chn, ev, additional)) {
		child = midi_node->add_child ("solo");
		set_midi_node_info (child, ev, chn, additional);
	}

	
	string order_string;
	OrderKeys::iterator x = order_keys.begin(); 

	while (x != order_keys.end()) {
		order_string += (*x).first;
		order_string += '=';
		snprintf (buf, sizeof(buf), "%ld", (*x).second);
		order_string += buf;
		
		++x;

		if (x == order_keys.end()) {
			break;
		}

		order_string += ':';
	}
	node->add_property ("order-keys", order_string);

	node->add_child_nocopy (IO::state (full_state));

	if (_control_outs) {
		XMLNode* cnode = new XMLNode (X_("ControlOuts"));
		cnode->add_child_nocopy (_control_outs->state (full_state));
		node->add_child_nocopy (*cnode);
	}

	if (_comment.length()) {
		XMLNode *cmt = node->add_child ("Comment");
		cmt->add_content (_comment);
	}

	if (full_state) {
		string path;

		path = _session.snap_name();
		path += "-gain-";
		path += legalize_for_path (_name);
		path += ".automation";

		/* XXX we didn't ask for a state save, we asked for the current state.
		   FIX ME!
		*/

		if (save_automation (path)) {
			error << _("Could not get state of route.  Problem with save_automation") << endmsg;
		}

		aevents = node->add_child ("Automation");
		aevents->add_property ("path", path);
	}	

	for (i = _redirects.begin(); i != _redirects.end(); ++i) {
		node->add_child_nocopy((*i)->state (full_state));
	}

	if (_extra_xml){
		node->add_child_copy (*_extra_xml);
	}
	
	return *node;
}

void
Route::set_deferred_state ()
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;

	if (!deferred_state) {
		return;
	}

	nlist = deferred_state->children();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter){
		add_redirect_from_xml (**niter);
	}

	delete deferred_state;
	deferred_state = 0;
}

void
Route::add_redirect_from_xml (const XMLNode& node)
{
	const XMLProperty *prop;
	Insert *insert = 0;
	Send *send = 0;

	if (node.name() == "Send") {
		
		try {
			send = new Send (_session, node);
		} 
		
		catch (failed_constructor &err) {
			error << _("Send construction failed") << endmsg;
			return;
		}
		
		add_redirect (send, this);
		
	} else if (node.name() == "Insert") {
		
		try {
			if ((prop = node.property ("type")) != 0) {

				if (prop->value() == "ladspa" || prop->value() == "Ladspa" || prop->value() == "vst") {

					insert = new PluginInsert(_session, node);
					
				} else if (prop->value() == "port") {


					insert = new PortInsert (_session, node);

				} else {

					error << string_compose(_("unknown Insert type \"%1\"; ignored"), prop->value()) << endmsg;
				}

				add_redirect (insert, this);
				
			} else {
				error << _("Insert XML node has no type property") << endmsg;
			}
		}
		
		catch (failed_constructor &err) {
			warning << _("insert could not be created. Ignored.") << endmsg;
			return;
		}
	}
}

int
Route::set_state (const XMLNode& node)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	XMLNode *child;
	XMLPropertyList plist;
	const XMLProperty *prop;
	XMLNodeList midi_kids;


	if (node.name() != "Route"){
		error << string_compose(_("Bad node sent to Route::set_state() [%1]"), node.name()) << endmsg;
		return -1;
	}

	if ((prop = node.property ("flags")) != 0) {
		int x;
		sscanf (prop->value().c_str(), "0x%x", &x);
		_flags = Flag (x);
	} else {
		_flags = Flag (0);
	}

	if ((prop = node.property ("phase-invert")) != 0) {
		set_phase_invert(prop->value()=="yes"?true:false, this);
	}

	if ((prop = node.property ("active")) != 0) {
		set_active (prop->value() == "yes");
	}

	if ((prop = node.property ("muted")) != 0) {
		bool yn = prop->value()=="yes"?true:false; 

		/* force reset of mute status */

		_muted = !yn;
		set_mute(yn, this);
		mute_gain = desired_mute_gain;
	}

	if ((prop = node.property ("soloed")) != 0) {
		bool yn = prop->value()=="yes"?true:false; 

		/* force reset of solo status */

		_soloed = !yn;
		set_solo (yn, this);
		solo_gain = desired_solo_gain;
	}

	if ((prop = node.property ("mute-affects-pre-fader")) != 0) {
		_mute_affects_pre_fader = (prop->value()=="yes")?true:false;
	}

	if ((prop = node.property ("mute-affects-post-fader")) != 0) {
		_mute_affects_post_fader = (prop->value()=="yes")?true:false;
	}

	if ((prop = node.property ("mute-affects-control-outs")) != 0) {
		_mute_affects_control_outs = (prop->value()=="yes")?true:false;
	}

	if ((prop = node.property ("mute-affects-main-outs")) != 0) {
		_mute_affects_main_outs = (prop->value()=="yes")?true:false;
	}

	if ((prop = node.property ("edit-group")) != 0) {
		RouteGroup* edit_group = _session.edit_group_by_name(prop->value());
		if(edit_group == 0) {
			error << string_compose(_("Route %1: unknown edit group \"%2 in saved state (ignored)"), _name, prop->value()) << endmsg;
		} else {
			set_edit_group(edit_group, this);
		}
	}

	if ((prop = node.property ("order-keys")) != 0) {

		long n;

		string::size_type colon, equal;
		string remaining = prop->value();

		while (remaining.length()) {

			if ((equal = remaining.find_first_of ('=')) == string::npos || equal == remaining.length()) {
				error << string_compose (_("badly formed order key string in state file! [%1] ... ignored."), remaining)
				      << endmsg;
			} else {
				if (sscanf (remaining.substr (equal+1).c_str(), "%ld", &n) != 1) {
					error << string_compose (_("badly formed order key string in state file! [%1] ... ignored."), remaining)
					      << endmsg;
				} else {
					set_order_key (remaining.substr (0, equal), n);
				}
			}

			colon = remaining.find_first_of (':');

			if (colon != string::npos) {
				remaining = remaining.substr (colon+1);
			} else {
				break;
			}
		}
	}

	nlist = node.children();

	if (deferred_state) {
		delete deferred_state;
	}

	deferred_state = new XMLNode("deferred state");

	/* set parent class properties before anything else */

	for (niter = nlist.begin(); niter != nlist.end(); ++niter){

		child = *niter;

		if (child->name() == IO::state_node_name) {

			IO::set_state (*child);
			break;
		}
	}
			
	for (niter = nlist.begin(); niter != nlist.end(); ++niter){

		child = *niter;
			
		if (child->name() == "Send") {


			if (!IO::ports_legal) {

				deferred_state->add_child_copy (*child);

			} else {
				add_redirect_from_xml (*child);
			}

		} else if (child->name() == "Insert") {
			
			if (!IO::ports_legal) {
				
				deferred_state->add_child_copy (*child);

			} else {
				
				add_redirect_from_xml (*child);
			}

		} else if (child->name() == "Automation") {

			XMLPropertyList plist;
			XMLPropertyConstIterator piter;
			XMLProperty *prop;
			
			plist = child->properties();
			for (piter = plist.begin(); piter != plist.end(); ++piter) {
				prop = *piter;
				if (prop->name() == "path") {
					load_automation (prop->value());
				}
			}

		} else if (child->name() == "ControlOuts") {
			
			string coutname = _name;
			coutname += _("[control]");

			_control_outs = new IO (_session, coutname);
			_control_outs->set_state (**(child->children().begin()));

		} else if (child->name() == "Comment") {

			/* XXX this is a terrible API design in libxml++ */

			XMLNode *cmt = *(child->children().begin());
			_comment = cmt->content();

		} else if (child->name() == "extra") {
			_extra_xml = new XMLNode (*child);
		}
	}

	if ((prop = node.property ("mix-group")) != 0) {
		RouteGroup* mix_group = _session.mix_group_by_name(prop->value());
		if (mix_group == 0) {
			error << string_compose(_("Route %1: unknown mix group \"%2 in saved state (ignored)"), _name, prop->value()) << endmsg;
		}  else {
			set_mix_group(mix_group, this);
		}
	}

	midi_kids = node.children ("MIDI");
	
	for (niter = midi_kids.begin(); niter != midi_kids.end(); ++niter) {
	
		XMLNodeList kids;
		XMLNodeConstIterator miter;
		XMLNode*    child;

		kids = (*niter)->children ();

		for (miter = kids.begin(); miter != kids.end(); ++miter) {

			child =* miter;

			MIDI::eventType ev = MIDI::on; /* initialize to keep gcc happy */
			MIDI::byte additional = 0;  /* ditto */
			MIDI::channel_t chn = 0;    /* ditto */
			
			if (child->name() == "mute") {
			
				if (get_midi_node_info (child, ev, chn, additional)) {
					_midi_mute_control.set_control_type (chn, ev, additional);
				} else {
					error << string_compose(_("MIDI mute control specification for %1 is incomplete, so it has been ignored"), _name) << endmsg;
				}
			}
			else if (child->name() == "solo") {
			
				if (get_midi_node_info (child, ev, chn, additional)) {
					_midi_solo_control.set_control_type (chn, ev, additional);
				} else {
					error << string_compose(_("MIDI mute control specification for %1 is incomplete, so it has been ignored"), _name) << endmsg;
				}
			}

		}
	}

	
	return 0;
}

void
Route::curve_reallocate ()
{
//	_gain_automation_curve.finish_resize ();
//	_pan_automation_curve.finish_resize ();
}

void
Route::silence (jack_nframes_t nframes, jack_nframes_t offset)
{
	if (!_silent) {

		// reset_peak_meters ();
		
		IO::silence (nframes, offset);

		if (_control_outs) {
			_control_outs->silence (nframes, offset);
		}

		{ 
			Glib::RWLock::ReaderLock lm (redirect_lock, Glib::TRY_LOCK);
			
			if (lm.locked()) {
				for (RedirectList::iterator i = _redirects.begin(); i != _redirects.end(); ++i) {
					PluginInsert* pi;
					if (!_active && (pi = dynamic_cast<PluginInsert*> (*i)) != 0) {
						// skip plugins, they don't need anything when we're not active
						continue;
					}

					(*i)->silence (nframes, offset);
				}

				if (nframes == _session.get_block_size() && offset == 0) {
					// _silent = true;
				}
			}
		}
		
	}
}	

int
Route::set_control_outs (const vector<string>& ports)
{
	Glib::Mutex::Lock lm (control_outs_lock);
	vector<string>::const_iterator i;

 	if (_control_outs) {
 		delete _control_outs;
 		_control_outs = 0;
 	}
 	
 	if (ports.empty()) {
 		return 0;
 	}
 
 	string coutname = _name;
 	coutname += _("[control]");
 	
 	_control_outs = new IO (_session, coutname);

	/* our control outs need as many outputs as we
	   have outputs. we track the changes in ::output_change_handler().
	*/

	_control_outs->ensure_io (0, n_outputs(), true, this);
 
 	return 0;
}	

void
Route::set_edit_group (RouteGroup *eg, void *src)

{
	if (eg == _edit_group) {
		return;
	}

	if (_edit_group) {
		_edit_group->remove (this);
	}

	if ((_edit_group = eg) != 0) {
		_edit_group->add (this);
	}

	_session.set_dirty ();
	edit_group_changed (src); /* EMIT SIGNAL */
}

void
Route::drop_edit_group (void *src)
{
	_edit_group = 0;
	_session.set_dirty ();
	edit_group_changed (src); /* EMIT SIGNAL */
}

void
Route::set_mix_group (RouteGroup *mg, void *src)

{
	if (mg == _mix_group) {
		return;
	}

	if (_mix_group) {
		_mix_group->remove (this);
	}

	if ((_mix_group = mg) != 0) {
		_mix_group->add (this);
	}

	_session.set_dirty ();
	mix_group_changed (src); /* EMIT SIGNAL */
}

void
Route::drop_mix_group (void *src)
{
	_mix_group = 0;
	_session.set_dirty ();
	mix_group_changed (src); /* EMIT SIGNAL */
}

void
Route::set_comment (string cmt, void *src)
{
	_comment = cmt;
	comment_changed (src);
	_session.set_dirty ();
}

bool
Route::feeds (Route *o)
{
	uint32_t i, j;

	IO& other = *o;
	IO& self = *this;
	uint32_t no = self.n_outputs();
	uint32_t ni = other.n_inputs ();

	for (i = 0; i < no; ++i) {
		for (j = 0; j < ni; ++j) {
			if (self.output(i)->connected_to (other.input(j)->name())) {
				return true;
			}
		}
	}

	/* check Redirects which may also interconnect Routes */

	for (RedirectList::iterator r = _redirects.begin(); r != _redirects.end(); r++) {

		no = (*r)->n_outputs();

		for (i = 0; i < no; ++i) {
			for (j = 0; j < ni; ++j) {
				if ((*r)->output(i)->connected_to (other.input (j)->name())) {
					return true;
				}
			}
		}
	}

	/* check for control room outputs which may also interconnect Routes */

	if (_control_outs) {

		no = _control_outs->n_outputs();
		
		for (i = 0; i < no; ++i) {
			for (j = 0; j < ni; ++j) {
				if (_control_outs->output(i)->connected_to (other.input (j)->name())) {
					return true;
				}
			}
		}
	}

	return false;
}

void
Route::set_mute_config (mute_type t, bool onoff, void *src)
{
	switch (t) {
	case PRE_FADER:
		_mute_affects_pre_fader = onoff;
		 pre_fader_changed(src); /* EMIT SIGNAL */
		break;

	case POST_FADER:
		_mute_affects_post_fader = onoff;
		 post_fader_changed(src); /* EMIT SIGNAL */
		break;

	case CONTROL_OUTS:
		_mute_affects_control_outs = onoff;
		 control_outs_changed(src); /* EMIT SIGNAL */
		break;

	case MAIN_OUTS:
		_mute_affects_main_outs = onoff;
		 main_outs_changed(src); /* EMIT SIGNAL */
		break;
	}
}

bool
Route::get_mute_config (mute_type t)
{
	bool onoff = false;
	
	switch (t){
	case PRE_FADER:
		onoff = _mute_affects_pre_fader; 
		break;
	case POST_FADER:
		onoff = _mute_affects_post_fader;
		break;
	case CONTROL_OUTS:
		onoff = _mute_affects_control_outs;
		break;
	case MAIN_OUTS:
		onoff = _mute_affects_main_outs;
		break;
	}
	
	return onoff;
}

void
Route::set_active (bool yn)
{
	_active = yn; 
	 active_changed(); /* EMIT SIGNAL */
}

void
Route::handle_transport_stopped (bool abort_ignored, bool did_locate, bool can_flush_redirects)
{
	jack_nframes_t now = _session.transport_frame();

	{
		Glib::RWLock::ReaderLock lm (redirect_lock);

		if (!did_locate) {
			automation_snapshot (now);
		}

		for (RedirectList::iterator i = _redirects.begin(); i != _redirects.end(); ++i) {
			
			if (Config->get_plugins_stop_with_transport() && can_flush_redirects) {
				(*i)->deactivate ();
				(*i)->activate ();
			}
			
			(*i)->transport_stopped (now);
		}
	}

	IO::transport_stopped (now);
 
	_roll_delay = _initial_delay;
}

UndoAction
Route::get_memento() const
{
	void (Route::*pmf)(state_id_t) = &Route::set_state;
	return sigc::bind (mem_fun (*(const_cast<Route *>(this)), pmf), _current_state_id);
}

void
Route::set_state (state_id_t id)
{
	return;
}

void
Route::input_change_handler (IOChange change, void *ignored)
{
	if (change & ConfigurationChanged) {
		reset_plugin_counts (0);
	}
}

void
Route::output_change_handler (IOChange change, void *ignored)
{
	if (change & ConfigurationChanged) {
		if (_control_outs) {
			_control_outs->ensure_io (0, n_outputs(), true, this);
		}
		
		reset_plugin_counts (0);
	}
}

uint32_t
Route::pans_required () const
{
	if (n_outputs() < 2) {
		return 0;
	}
	
	return max (n_inputs (), redirect_max_outs);
}

int 
Route::no_roll (jack_nframes_t nframes, jack_nframes_t start_frame, jack_nframes_t end_frame, jack_nframes_t offset, 
		   bool session_state_changing, bool can_record, bool rec_monitors_input)
{
	if (n_outputs() == 0) {
		return 0;
	}

	if (session_state_changing || !_active)  {
		silence (nframes, offset);
		return 0;
	}

	apply_gain_automation = false;
	
	if (n_inputs()) {
		passthru (start_frame, end_frame, nframes, offset, 0, false);
	} else {
		silence (nframes, offset);
	}

	return 0;
}

jack_nframes_t
Route::check_initial_delay (jack_nframes_t nframes, jack_nframes_t& offset, jack_nframes_t& transport_frame)
{
	if (_roll_delay > nframes) {

		_roll_delay -= nframes;
		silence (nframes, offset);
		/* transport frame is not legal for caller to use */
		return 0;

	} else if (_roll_delay > 0) {

		nframes -= _roll_delay;

		silence (_roll_delay, offset);

		offset += _roll_delay;
		transport_frame += _roll_delay;

		_roll_delay = 0;
	}

	return nframes;
}

int
Route::roll (jack_nframes_t nframes, jack_nframes_t start_frame, jack_nframes_t end_frame, jack_nframes_t offset, int declick,
	     bool can_record, bool rec_monitors_input)
{
	{
		Glib::RWLock::ReaderLock lm (redirect_lock, Glib::TRY_LOCK);
		if (lm.locked()) {
			// automation snapshot can also be called from the non-rt context
			// and it uses the redirect list, so we take the lock out here
			automation_snapshot (_session.transport_frame());
		}
	}
		
	if ((n_outputs() == 0 && _redirects.empty()) || n_inputs() == 0 || !_active) {
		silence (nframes, offset);
		return 0;
	}
	
	jack_nframes_t unused = 0;

	if ((nframes = check_initial_delay (nframes, offset, unused)) == 0) {
		return 0;
	}

	_silent = false;

	apply_gain_automation = false;

	{ 
		Glib::Mutex::Lock am (automation_lock, Glib::TRY_LOCK);
		
		if (am.locked() && _session.transport_rolling()) {
			
			jack_nframes_t start_frame = end_frame - nframes;
			
			if (gain_automation_playback()) {
				apply_gain_automation = _gain_automation_curve.rt_safe_get_vector (start_frame, end_frame, _session.gain_automation_buffer(), nframes);
			}
		}
	}

	passthru (start_frame, end_frame, nframes, offset, declick, false);

	return 0;
}

int
Route::silent_roll (jack_nframes_t nframes, jack_nframes_t start_frame, jack_nframes_t end_frame, jack_nframes_t offset, 
		    bool can_record, bool rec_monitors_input)
{
	silence (nframes, offset);
	return 0;
}

void
Route::toggle_monitor_input ()
{
	for (vector<Port*>::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
		(*i)->request_monitor_input(!(*i)->monitoring_input());
	}
}

bool
Route::has_external_redirects () const
{
	const PortInsert* pi;
	
	for (RedirectList::const_iterator i = _redirects.begin(); i != _redirects.end(); ++i) {
		if ((pi = dynamic_cast<const PortInsert*>(*i)) != 0) {

			uint32_t no = pi->n_outputs();

			for (uint32_t n = 0; n < no; ++n) {
				
				string port_name = pi->output(n)->name();
				string client_name = port_name.substr (0, port_name.find(':'));

				/* only say "yes" if the redirect is actually in use */
				
				if (client_name != "ardour" && pi->active()) {
					return true;
				}
			}
		}
	}

	return false;
}

void
Route::reset_midi_control (MIDI::Port* port, bool on)
{
	MIDI::channel_t chn;
	MIDI::eventType ev;
	MIDI::byte extra;

	for (RedirectList::iterator i = _redirects.begin(); i != _redirects.end(); ++i) {
			(*i)->reset_midi_control (port, on);
	}

	IO::reset_midi_control (port, on);
	
	_midi_solo_control.get_control_info (chn, ev, extra);
	if (!on) {
		chn = -1;
	}
	_midi_solo_control.midi_rebind (port, chn);
	
	_midi_mute_control.get_control_info (chn, ev, extra);
	if (!on) {
		chn = -1;
	}
	_midi_mute_control.midi_rebind (port, chn);
}

void
Route::send_all_midi_feedback ()
{
	if (_session.get_midi_feedback()) {

		{
			Glib::RWLock::ReaderLock lm (redirect_lock);
			for (RedirectList::iterator i = _redirects.begin(); i != _redirects.end(); ++i) {
				(*i)->send_all_midi_feedback ();
			}
		}

		IO::send_all_midi_feedback();

		_midi_solo_control.send_feedback (_soloed);
		_midi_mute_control.send_feedback (_muted);
	}
}

MIDI::byte*
Route::write_midi_feedback (MIDI::byte* buf, int32_t& bufsize)
{
	buf = _midi_solo_control.write_feedback (buf, bufsize, _soloed);
	buf = _midi_mute_control.write_feedback (buf, bufsize, _muted);

	{
		Glib::RWLock::ReaderLock lm (redirect_lock);
		for (RedirectList::iterator i = _redirects.begin(); i != _redirects.end(); ++i) {
			buf = (*i)->write_midi_feedback (buf, bufsize);
		}
	}

	return IO::write_midi_feedback (buf, bufsize);
}

void
Route::flush_redirects ()
{
	/* XXX shouldn't really try to take this lock, since
	   this is called from the RT audio thread.
	*/

	Glib::RWLock::ReaderLock lm (redirect_lock);

	for (RedirectList::iterator i = _redirects.begin(); i != _redirects.end(); ++i) {
		(*i)->deactivate ();
		(*i)->activate ();
	}
}

void
Route::set_meter_point (MeterPoint p, void *src)
{
	if (_meter_point != p) {
		_meter_point = p;
		 meter_change (src); /* EMIT SIGNAL */
		_session.set_dirty ();
	}
}

jack_nframes_t
Route::update_total_latency ()
{
	_own_latency = 0;

	for (RedirectList::iterator i = _redirects.begin(); i != _redirects.end(); ++i) {
		if ((*i)->active ()) {
			_own_latency += (*i)->latency ();
		}
	}

	set_port_latency (_own_latency);

	/* this (virtual) function is used for pure Routes,
	   not derived classes like AudioTrack.  this means
	   that the data processed here comes from an input
	   port, not prerecorded material, and therefore we
	   have to take into account any input latency.
	*/

	_own_latency += input_latency ();

	return _own_latency;
}

void
Route::set_latency_delay (jack_nframes_t longest_session_latency)
{
	_initial_delay = longest_session_latency - _own_latency;

	if (_session.transport_stopped()) {
		_roll_delay = _initial_delay;
	}
}

void
Route::automation_snapshot (jack_nframes_t now)
{
	IO::automation_snapshot (now);

	for (RedirectList::iterator i = _redirects.begin(); i != _redirects.end(); ++i) {
		(*i)->automation_snapshot (now);
	}
}

Route::MIDIToggleControl::MIDIToggleControl (Route& s, ToggleType tp, MIDI::Port* port)
	: MIDI::Controllable (port, true), route (s), type(tp), setting(false)
{
	last_written = false; /* XXX need a good out-of-bound-value */
}

void
Route::MIDIToggleControl::set_value (float val)
{
	MIDI::eventType et;
	MIDI::channel_t chn;
	MIDI::byte additional;

	get_control_info (chn, et, additional);

	setting = true;

#ifdef HOLD_TOGGLE_VALUES
	if (et == MIDI::off || et == MIDI::on) {

		/* literal toggle */

		switch (type) {
		case MuteControl:
			route.set_mute (!route.muted(), this);
			break;
		case SoloControl:
			route.set_solo (!route.soloed(), this);
			break;
		default:
			break;
		}

	} else {
#endif

		/* map full control range to a boolean */

		bool bval = ((val >= 0.5f) ? true: false);
		
		switch (type) {
		case MuteControl:
			route.set_mute (bval, this);
			break;
		case SoloControl:
			route.set_solo (bval, this);
			break;
		default:
			break;
		}

#ifdef HOLD_TOGGLE_VALUES
	}
#endif

	setting = false;
}

void
Route::MIDIToggleControl::send_feedback (bool value)
{

	if (!setting && get_midi_feedback()) {
		MIDI::byte val = (MIDI::byte) (value ? 127: 0);
		MIDI::channel_t ch = 0;
		MIDI::eventType ev = MIDI::none;
		MIDI::byte additional = 0;
		MIDI::EventTwoBytes data;
	    
		if (get_control_info (ch, ev, additional)) {
			data.controller_number = additional;
			data.value = val;
			last_written = value;
			
			route._session.send_midi_message (get_port(), ev, ch, data);
		}
	}
	
}

MIDI::byte*
Route::MIDIToggleControl::write_feedback (MIDI::byte* buf, int32_t& bufsize, bool val, bool force)
{
	if (get_midi_feedback() && bufsize > 2) {
		MIDI::channel_t ch = 0;
		MIDI::eventType ev = MIDI::none;
		MIDI::byte additional = 0;

		if (get_control_info (ch, ev, additional)) {
			if (val != last_written || force) {
				*buf++ = (0xF0 & ev) | (0xF & ch);
				*buf++ = additional; /* controller number */
				*buf++ = (MIDI::byte) (val ? 127 : 0);
				bufsize -= 3;
				last_written = val;
			}
		}
	}

	return buf;
}

void 
Route::set_block_size (jack_nframes_t nframes)
{
	for (RedirectList::iterator i = _redirects.begin(); i != _redirects.end(); ++i) {
		(*i)->set_block_size (nframes);
	}
}

void
Route::redirect_active_proxy (Redirect* ignored, void* ignored_src)
{
	_session.update_latency_compensation (false, false);
}

void
Route::protect_automation ()
{
	switch (gain_automation_state()) {
	case Write:
	case Touch:
		set_gain_automation_state (Off);
		break;
	default:
		break;
	}

	switch (panner().automation_state ()) {
	case Write:
	case Touch:
		panner().set_automation_state (Off);
		break;
	default:
		break;
	}
	
	for (RedirectList::iterator i = _redirects.begin(); i != _redirects.end(); ++i) {
		PluginInsert* pi;
		if ((pi = dynamic_cast<PluginInsert*> (*i)) != 0) {
			pi->protect_automation ();
		}
	}
}

void
Route::set_pending_declick (int declick)
{
	if (_declickable) {
		/* this call is not allowed to turn off a pending declick unless "force" is true */
		if (declick) {
			_pending_declick = declick;
		}
		// cerr << _name << ": after setting to " << declick << " pending declick = " << _pending_declick << endl;
	} else {
		_pending_declick = 0;
	}

}
