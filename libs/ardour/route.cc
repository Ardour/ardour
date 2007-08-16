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

*/

#include <cmath>
#include <fstream>
#include <cassert>

#include <sigc++/bind.h>
#include <pbd/xml++.h>
#include <pbd/enumwriter.h>

#include <ardour/timestamps.h>
#include <ardour/audioengine.h>
#include <ardour/route.h>
#include <ardour/buffer.h>
#include <ardour/processor.h>
#include <ardour/plugin_insert.h>
#include <ardour/port_insert.h>
#include <ardour/send.h>
#include <ardour/session.h>
#include <ardour/utils.h>
#include <ardour/configuration.h>
#include <ardour/cycle_timer.h>
#include <ardour/route_group.h>
#include <ardour/port.h>
#include <ardour/audio_port.h>
#include <ardour/ladspa_plugin.h>
#include <ardour/panner.h>
#include <ardour/dB.h>
#include <ardour/amp.h>
#include <ardour/meter.h>
#include <ardour/buffer_set.h>
#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

uint32_t Route::order_key_cnt = 0;


Route::Route (Session& sess, string name, int input_min, int input_max, int output_min, int output_max, Flag flg, DataType default_type)
	: IO (sess, name, input_min, input_max, output_min, output_max, default_type),
	  _flags (flg),
	  _solo_control (new ToggleControllable (X_("solo"), *this, ToggleControllable::SoloControl)),
	  _mute_control (new ToggleControllable (X_("mute"), *this, ToggleControllable::MuteControl))
{
	init ();
}

Route::Route (Session& sess, const XMLNode& node, DataType default_type)
	: IO (sess, *node.child ("IO"), default_type),
	  _solo_control (new ToggleControllable (X_("solo"), *this, ToggleControllable::SoloControl)),
	  _mute_control (new ToggleControllable (X_("mute"), *this, ToggleControllable::MuteControl))
{
	init ();
	_set_state (node, false);
}

void
Route::init ()
{
	processor_max_outs.reset();
	_muted = false;
	_soloed = false;
	_solo_safe = false;
	_phase_invert = false;
	_denormal_protection = false;
	order_keys[strdup (N_("signal"))] = order_key_cnt++;
	_active = true;
	_silent = false;
	_meter_point = MeterPostFader;
	_initial_delay = 0;
	_roll_delay = 0;
	_own_latency = 0;
	_user_latency = 0;
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
}

Route::~Route ()
{
	clear_processors (PreFader);
	clear_processors (PostFader);

	for (OrderKeys::iterator i = order_keys.begin(); i != order_keys.end(); ++i) {
		free ((void*)(i->first));
	}

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
Route::order_key (const char* name) const
{
	OrderKeys::const_iterator i;
	
	for (i = order_keys.begin(); i != order_keys.end(); ++i) {
		if (!strcmp (name, i->first)) {
			return i->second;
		}
	}

	return -1;
}

void
Route::set_order_key (const char* name, long n)
{
	order_keys[strdup(name)] = n;
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
			
			gain_t usable_gain = gain();
			if (usable_gain < 0.000001f) {
				usable_gain = 0.000001f;
			}
						
			gain_t delta = val;
			if (delta < 0.000001f) {
				delta = 0.000001f;
			}

			delta -= usable_gain;

			if (delta == 0.0f)
				return;

			gain_t factor = delta / usable_gain;

			if (factor > 0.0f) {
				factor = _mix_group->get_max_factor(factor);
				if (factor == 0.0f) {
					_gain_control->Changed(); /* EMIT SIGNAL */
					return;
				}
			} else {
				factor = _mix_group->get_min_factor(factor);
				if (factor == 0.0f) {
					_gain_control->Changed(); /* EMIT SIGNAL */
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

/** Process this route for one (sub) cycle (process thread)
 *
 * @param bufs Scratch buffers to use for the signal path
 * @param start_frame Initial transport frame 
 * @param end_frame Final transport frame
 * @param nframes Number of frames to output (to ports)
 * @param offset Output offset (of port buffers, for split cycles)
 *
 * Note that (end_frame - start_frame) may not be equal to nframes when the
 * transport speed isn't 1.0 (eg varispeed).
 */
void
Route::process_output_buffers (BufferSet& bufs,
			       nframes_t start_frame, nframes_t end_frame, 
			       nframes_t nframes, nframes_t offset, bool with_processors, int declick,
			       bool meter)
{
	// This is definitely very audio-only for now
	assert(_default_type == DataType::AUDIO);
	
	ProcessorList::iterator i;
	bool post_fader_work = false;
	bool mute_declick_applied = false;
	gain_t dmg, dsg, dg;
	IO *co;
	bool mute_audible;
	bool solo_audible;
	bool no_monitor;
	gain_t* gab = _session.gain_automation_buffer();

	switch (Config->get_monitoring_model()) {
	case HardwareMonitoring:
	case ExternalMonitoring:
		no_monitor = true;
		break;
	default:
		no_monitor = false;
	}

	declick = _pending_declick;

	{
		Glib::Mutex::Lock cm (_control_outs_lock, Glib::TRY_LOCK);
		
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
		Amp::run_in_place (bufs, nframes, 0.0, 1.0, false);
		_pending_declick = 0;
	} else if (declick < 0) {
		Amp::run_in_place (bufs, nframes, 1.0, 0.0, false);
		_pending_declick = 0;
	} else {

		/* no global declick */

		if (solo_gain != dsg) {
			Amp::run_in_place (bufs, nframes, solo_gain, dsg, false);
			solo_gain = dsg;
		}
	}


	/* ----------------------------------------------------------------------------------------------------
	   INPUT METERING & MONITORING
	   -------------------------------------------------------------------------------------------------- */

	if (meter && (_meter_point == MeterInput)) {
		_meter->run(bufs, start_frame, end_frame, nframes, offset);
	}

	if (!_soloed && _mute_affects_pre_fader && (mute_gain != dmg)) {
		Amp::run_in_place (bufs, nframes, mute_gain, dmg, false);
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

			(no_monitor && record_enabled() && (!Config->get_auto_input() || _session.actively_recording()))

			) {
			
			co->silence (nframes, offset);
			
		} else {

			co->deliver_output (bufs, start_frame, end_frame, nframes, offset);
			
		} 
	} 

	/* -----------------------------------------------------------------------------------------------------
	   DENORMAL CONTROL
	   -------------------------------------------------------------------------------------------------- */

	if (_denormal_protection || Config->get_denormal_protection()) {

		for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
			Sample* const sp = i->data();
			
			for (nframes_t nx = offset; nx < nframes + offset; ++nx) {
				sp[nx] += 1.0e-27f;
			}
		}
	}

	/* ----------------------------------------------------------------------------------------------------
	   PRE-FADER REDIRECTS
	   -------------------------------------------------------------------------------------------------- */

	if (with_processors) {
		Glib::RWLock::ReaderLock rm (_processor_lock, Glib::TRY_LOCK);
		if (rm.locked()) {
			if (mute_gain > 0 || !_mute_affects_pre_fader) {
				for (i = _processors.begin(); i != _processors.end(); ++i) {
					switch ((*i)->placement()) {
					case PreFader:
						(*i)->run_in_place (bufs, start_frame, end_frame, nframes, offset);
						break;
					case PostFader:
						post_fader_work = true;
						break;
					}
				}
			} else {
				for (i = _processors.begin(); i != _processors.end(); ++i) {
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

	// This really should already be true...
	bufs.set_count(pre_fader_streams());
	
	if (!_soloed && (mute_gain != dmg) && !mute_declick_applied && _mute_affects_post_fader) {
		Amp::run_in_place (bufs, nframes, mute_gain, dmg, false);
		mute_gain = dmg;
		mute_declick_applied = true;
	}

	/* ----------------------------------------------------------------------------------------------------
	   PRE-FADER METERING & MONITORING
	   -------------------------------------------------------------------------------------------------- */

	if (meter && (_meter_point == MeterPreFader)) {
		_meter->run_in_place(bufs, start_frame, end_frame, nframes, offset);
	}

	
	if ((_meter_point == MeterPreFader) && co) {
		
		solo_audible = dsg > 0;
		mute_audible = dmg > 0 || !_mute_affects_pre_fader;
		
		if ( // muted by solo of another track
			
			!solo_audible || 
			
			// muted by mute of this track 
			
			!mute_audible ||
			
			// rec-enabled but not s/w monitoring 
			
			(no_monitor && record_enabled() && (!Config->get_auto_input() || _session.actively_recording()))

			) {
			
			co->silence (nframes, offset);
			
		} else {

			co->deliver_output (bufs, start_frame, end_frame, nframes, offset);
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
		
		(!Config->get_monitoring_model() == HardwareMonitoring && 

		 // AND software monitoring required

		 Config->get_monitoring_model() == SoftwareMonitoring)) { 
		
		if (apply_gain_automation) {
			
			if (_phase_invert) {
				for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
					Sample* const sp = i->data();
					
					for (nframes_t nx = 0; nx < nframes; ++nx) {
						sp[nx] *= -gab[nx];
					}
				}
			} else {
				for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
					Sample* const sp = i->data();
					
					for (nframes_t nx = 0; nx < nframes; ++nx) {
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
				
				Amp::run_in_place (bufs, nframes, _gain, dg, _phase_invert);
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
				
				for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
					Sample* const sp = i->data();
					apply_gain_to_buffer(sp,nframes,this_gain);
				}

			} else if (_gain == 0) {
				for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
					i->clear();
				}
			}
		}

	} else {

		/* actively recording, no monitoring required; leave buffers as-is to save CPU cycles */

	}

	/* ----------------------------------------------------------------------------------------------------
	   POST-FADER REDIRECTS
	   -------------------------------------------------------------------------------------------------- */

	/* note that post_fader_work cannot be true unless with_processors was also true, so don't test both */

	if (post_fader_work) {

		Glib::RWLock::ReaderLock rm (_processor_lock, Glib::TRY_LOCK);
		if (rm.locked()) {
			if (mute_gain > 0 || !_mute_affects_post_fader) {
				for (i = _processors.begin(); i != _processors.end(); ++i) {
					switch ((*i)->placement()) {
					case PreFader:
						break;
					case PostFader:
						(*i)->run_in_place (bufs, start_frame, end_frame, nframes, offset);
						break;
					}
				}
			} else {
				for (i = _processors.begin(); i != _processors.end(); ++i) {
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
		Amp::run_in_place (bufs, nframes, mute_gain, dmg, false);
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
			
			(no_monitor && record_enabled() && (!Config->get_auto_input() || _session.actively_recording()))

			) {
			
			co->silence (nframes, offset);
			
		} else {

			co->deliver_output (bufs, start_frame, end_frame, nframes, offset);
		} 
	} 

	/* ----------------------------------------------------------------------
	   GLOBAL MUTE 
	   ----------------------------------------------------------------------*/

	if (!_soloed && (mute_gain != dmg) && !mute_declick_applied && _mute_affects_main_outs) {
		Amp::run_in_place (bufs, nframes, mute_gain, dmg, false);
		mute_gain = dmg;
		mute_declick_applied = true;
	}
	
	/* ----------------------------------------------------------------------------------------------------
	   MAIN OUTPUT STAGE
	   -------------------------------------------------------------------------------------------------- */

	solo_audible = dsg > 0;
	mute_audible = dmg > 0 || !_mute_affects_main_outs;
	
	if (n_outputs().get(_default_type) == 0) {
	    
	    /* relax */

	} else if (no_monitor && record_enabled() && (!Config->get_auto_input() || _session.actively_recording())) {
		
		IO::silence (nframes, offset);
		
	} else {

		if ( // silent anyway

		    (_gain == 0 && !apply_gain_automation) ||
		    
		    // muted by solo of another track, but not using control outs for solo

		    (!solo_audible && (Config->get_solo_model() != SoloBus)) ||
		    
		    // muted by mute of this track

		    !mute_audible

			) {

			/* don't use Route::silence() here, because that causes
			   all outputs (sends, port processors, etc. to be silent).
			*/
			
			if (_meter_point == MeterPostFader) {
				peak_meter().reset();
			}

			IO::silence (nframes, offset);
			
		} else {
			
			deliver_output(bufs, start_frame, end_frame, nframes, offset);

		}

	}

	/* ----------------------------------------------------------------------------------------------------
	   POST-FADER METERING
	   -------------------------------------------------------------------------------------------------- */

	if (meter && (_meter_point == MeterPostFader)) {
		if ((_gain == 0 && !apply_gain_automation) || dmg == 0) {
			_meter->reset();
		} else {
			_meter->run_in_place(output_buffers(), start_frame, end_frame, nframes, offset);
		}
	}
}

ChanCount
Route::n_process_buffers ()
{
	return max (n_inputs(), processor_max_outs);
}

void
Route::passthru (nframes_t start_frame, nframes_t end_frame, nframes_t nframes, nframes_t offset, int declick, bool meter_first)
{
	BufferSet& bufs = _session.get_scratch_buffers(n_process_buffers());

	_silent = false;

	collect_input (bufs, nframes, offset);

	if (meter_first) {
		_meter->run_in_place(bufs, start_frame, end_frame, nframes, offset);
		meter_first = false;
	}
		
	process_output_buffers (bufs, start_frame, end_frame, nframes, offset, true, declick, meter_first);
}

void
Route::passthru_silence (nframes_t start_frame, nframes_t end_frame, nframes_t nframes, nframes_t offset, int declick, bool meter)
{
	process_output_buffers (_session.get_silent_buffers (n_process_buffers()), start_frame, end_frame, nframes, offset, true, declick, meter);
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
		_solo_control->Changed (); /* EMIT SIGNAL */
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
		
		_mute_control->Changed (); /* EMIT SIGNAL */
		
		Glib::Mutex::Lock lm (declick_lock);
		desired_mute_gain = (yn?0.0f:1.0f);
	}
}

int
Route::add_processor (boost::shared_ptr<Processor> processor, ProcessorStreams* err)
{
	ChanCount old_rmo = processor_max_outs;

	if (!_session.engine().connected()) {
		return 1;
	}

	{
		Glib::RWLock::WriterLock lm (_processor_lock);

		boost::shared_ptr<PluginInsert> pi;
		boost::shared_ptr<PortInsert> porti;

		//processor->set_default_type(_default_type);

		if ((pi = boost::dynamic_pointer_cast<PluginInsert>(processor)) != 0) {
			pi->set_count (1);

			if (pi->natural_input_streams() == ChanCount::ZERO) {
				/* generator plugin */
				_have_internal_generator = true;
			}
			
		}
		
		_processors.push_back (processor);

		// Set up processor list channels.  This will set processor->[input|output]_streams(),
		// configure redirect ports properly, etc.
		if (_reset_plugin_counts (err)) {
			_processors.pop_back ();
			_reset_plugin_counts (0); // it worked before we tried to add it ...
			return -1;
		}

		// Ensure peak vector sizes before the plugin is activated
		ChanCount potential_max_streams = max(processor->input_streams(), processor->output_streams());
		_meter->configure_io(potential_max_streams, potential_max_streams);

		processor->activate ();
		processor->ActiveChanged.connect (bind (mem_fun (_session, &Session::update_latency_compensation), false, false));

		_user_latency = 0;
	}
	
	if (processor_max_outs != old_rmo || old_rmo == ChanCount::ZERO) {
		reset_panner ();
	}


	processors_changed (); /* EMIT SIGNAL */
	return 0;
}

int
Route::add_processors (const ProcessorList& others, ProcessorStreams* err)
{
	ChanCount old_rmo = processor_max_outs;

	if (!_session.engine().connected()) {
		return 1;
	}

	{
		Glib::RWLock::WriterLock lm (_processor_lock);

		ProcessorList::iterator existing_end = _processors.end();
		--existing_end;

		ChanCount potential_max_streams;

		for (ProcessorList::const_iterator i = others.begin(); i != others.end(); ++i) {
			
			boost::shared_ptr<PluginInsert> pi;
			
			if ((pi = boost::dynamic_pointer_cast<PluginInsert>(*i)) != 0) {
				pi->set_count (1);
				
				ChanCount m = max(pi->input_streams(), pi->output_streams());
				if (m > potential_max_streams)
					potential_max_streams = m;
			}

			// Ensure peak vector sizes before the plugin is activated
			_meter->configure_io(potential_max_streams, potential_max_streams);

			_processors.push_back (*i);
			
			if (_reset_plugin_counts (err)) {
				++existing_end;
				_processors.erase (existing_end, _processors.end());
				_reset_plugin_counts (0); // it worked before we tried to add it ...
				return -1;
			}
			
			(*i)->activate ();
			(*i)->ActiveChanged.connect (bind (mem_fun (_session, &Session::update_latency_compensation), false, false));
		}

		_user_latency = 0;
	}
	
	if (processor_max_outs != old_rmo || old_rmo == ChanCount::ZERO) {
		reset_panner ();
	}

	processors_changed (); /* EMIT SIGNAL */
	return 0;
}

/** Turn off all processors with a given placement
 * @param p Placement of processors to disable
 */

void
Route::disable_processors (Placement p)
{
	Glib::RWLock::ReaderLock lm (_processor_lock);
	
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if ((*i)->placement() == p) {
			(*i)->set_active (false);
		}
	}

	_session.set_dirty ();
}

/** Turn off all redirects 
 */

void
Route::disable_processors ()
{
	Glib::RWLock::ReaderLock lm (_processor_lock);
	
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->set_active (false);
	}
	
	_session.set_dirty ();
}

/** Turn off all redirects with a given placement
 * @param p Placement of redirects to disable
 */

void
Route::disable_plugins (Placement p)
{
	Glib::RWLock::ReaderLock lm (_processor_lock);
	
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if (boost::dynamic_pointer_cast<PluginInsert> (*i) && (*i)->placement() == p) {
			(*i)->set_active (false);
		}
	}
	
	_session.set_dirty ();
}

/** Turn off all plugins
 */

void
Route::disable_plugins ()
{
	Glib::RWLock::ReaderLock lm (_processor_lock);
	
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if (boost::dynamic_pointer_cast<PluginInsert> (*i)) {
			(*i)->set_active (false);
		}
	}
	
	_session.set_dirty ();
}


void
Route::ab_plugins (bool forward)
{
	Glib::RWLock::ReaderLock lm (_processor_lock);
			
	if (forward) {

		/* forward = turn off all active redirects, and mark them so that the next time
		   we go the other way, we will revert them
		*/

		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
			if (!boost::dynamic_pointer_cast<PluginInsert> (*i)) {
				continue;
			}

			if ((*i)->active()) {
				(*i)->set_active (false);
				(*i)->set_next_ab_is_active (true);
			} else {
				(*i)->set_next_ab_is_active (false);
			}
		}

	} else {

		/* backward = if the redirect was marked to go active on the next ab, do so */

		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {

			if (!boost::dynamic_pointer_cast<PluginInsert> (*i)) {
				continue;
			}

			if ((*i)->get_next_ab_is_active()) {
				(*i)->set_active (true);
			} else {
				(*i)->set_active (false);
			}
		}
	}
	
	_session.set_dirty ();
}
	
	
/* Figure out the streams that will feed into PreFader */
ChanCount
Route::pre_fader_streams() const
{
	boost::shared_ptr<Processor> processor;

	// Find the last pre-fader redirect
	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if ((*i)->placement() == PreFader) {
			processor = *i;
		}
	}
	
	if (processor) {
		return processor->output_streams();
	} else {
		return n_inputs ();
	}
}


/** Remove processors with a given placement.
 * @param p Placement of processors to remove.
 */
void
Route::clear_processors (Placement p)
{
	const ChanCount old_rmo = processor_max_outs;

	if (!_session.engine().connected()) {
		return;
	}

	{
		Glib::RWLock::WriterLock lm (_processor_lock);
		ProcessorList new_list;
		
		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
			if ((*i)->placement() == p) {
				/* it's the placement we want to get rid of */
				(*i)->drop_references ();
			} else {
				/* it's a different placement, so keep it */
				new_list.push_back (*i);
			}
		}
		
		_processors = new_list;
	}

	/* FIXME: can't see how this test can ever fire */
	if (processor_max_outs != old_rmo) {
		reset_panner ();
	}
	
	processor_max_outs.reset();
	_have_internal_generator = false;
	processors_changed (); /* EMIT SIGNAL */
}

int
Route::remove_processor (boost::shared_ptr<Processor> processor, ProcessorStreams* err)
{
	ChanCount old_rmo = processor_max_outs;

	if (!_session.engine().connected()) {
		return 1;
	}

	processor_max_outs.reset();

	{
		Glib::RWLock::WriterLock lm (_processor_lock);
		ProcessorList::iterator i;
		bool removed = false;

		for (i = _processors.begin(); i != _processors.end(); ++i) {
			if (*i == processor) {

				ProcessorList::iterator tmp;

				/* move along, see failure case for reset_plugin_counts()
				   where we may need to reprocessor the processor.
				*/

				tmp = i;
				++tmp;

				/* stop redirects that send signals to JACK ports
				   from causing noise as a result of no longer being
				   run.
				*/

				boost::shared_ptr<IOProcessor> redirect;
				
				if ((redirect = boost::dynamic_pointer_cast<IOProcessor> (*i)) != 0) {
					redirect->io()->disconnect_inputs (this);
					redirect->io()->disconnect_outputs (this);
				}

				_processors.erase (i);

				i = tmp;
				removed = true;
				break;
			}

			_user_latency = 0;
		}

		if (!removed) {
			/* what? */
			return 1;
		}

		if (_reset_plugin_counts (err)) {
			/* get back to where we where */
			_processors.insert (i, processor);
			/* we know this will work, because it worked before :) */
			_reset_plugin_counts (0);
			return -1;
		}

		bool foo = false;

		for (i = _processors.begin(); i != _processors.end(); ++i) {
			boost::shared_ptr<PluginInsert> pi;
			
			if ((pi = boost::dynamic_pointer_cast<PluginInsert>(*i)) != 0) {
				if (pi->is_generator()) {
					foo = true;
				}
			}
		}

		_have_internal_generator = foo;
	}

	if (old_rmo != processor_max_outs) {
		reset_panner ();
	}

	processor->drop_references ();

	processors_changed (); /* EMIT SIGNAL */
	return 0;
}

int
Route::reset_plugin_counts (ProcessorStreams* err)
{
	Glib::RWLock::WriterLock lm (_processor_lock);
	return _reset_plugin_counts (err);
}


int
Route::_reset_plugin_counts (ProcessorStreams* err)
{
	ProcessorList::iterator r;
	map<Placement,list<ProcessorCount> > processor_map;
	ChanCount initial_streams;

	/* Process each placement in order, checking to see if we 
	   can really do what has been requested.
	*/
	
	/* divide processors up by placement so we get the signal flow
	   properly modelled. we need to do this because the _processors
	   list is not sorted by placement
	*/

	/* ... but it should/will be... */
	
	for (r = _processors.begin(); r != _processors.end(); ++r) {

		boost::shared_ptr<Processor> processor;

		if ((processor = boost::dynamic_pointer_cast<Processor>(*r)) != 0) {
			processor_map[processor->placement()].push_back (ProcessorCount (processor));
		}
	}
	

	/* A: PreFader */
	
	if ( ! check_some_plugin_counts (processor_map[PreFader], n_inputs (), err)) {
		return -1;
	}

	ChanCount post_fader_input = (err ? err->count : n_inputs());

	/* B: PostFader */

	if ( ! check_some_plugin_counts (processor_map[PostFader], post_fader_input, err)) {
		return -1;
	}

	/* OK, everything can be set up correctly, so lets do it */

	apply_some_plugin_counts (processor_map[PreFader]);
	apply_some_plugin_counts (processor_map[PostFader]);

	/* recompute max outs of any processor */

	processor_max_outs.reset();
	ProcessorList::iterator prev = _processors.end();

	for (r = _processors.begin(); r != _processors.end(); prev = r, ++r) {
		processor_max_outs = max ((*r)->output_streams (), processor_max_outs);
	}

	/* we're done */

	return 0;
}				   

int32_t
Route::apply_some_plugin_counts (list<ProcessorCount>& iclist)
{
	list<ProcessorCount>::iterator i;

	for (i = iclist.begin(); i != iclist.end(); ++i) {
		
		if ((*i).processor->configure_io ((*i).in, (*i).out)) {
			return -1;
		}
		/* make sure that however many we have, they are all active */
		(*i).processor->activate ();
	}

	return 0;
}

/** Returns whether \a iclist can be configured and run starting with
 * \a required_inputs at the first processor's inputs.
 * If false is returned, \a iclist can not be run with \a required_inputs, and \a err is set.
 * Otherwise, \a err is set to the output of the list.
 */
bool
Route::check_some_plugin_counts (list<ProcessorCount>& iclist, ChanCount required_inputs, ProcessorStreams* err)
{
	list<ProcessorCount>::iterator i;
	size_t index = 0;
			
	if (err) {
		err->index = 0;
		err->count = required_inputs;
	}

	for (i = iclist.begin(); i != iclist.end(); ++i) {

		if ((*i).processor->can_support_input_configuration (required_inputs) < 0) {
			if (err) {
				err->index = index;
				err->count = required_inputs;
			}
			return false;
		}
		
		(*i).in = required_inputs;
		(*i).out = (*i).processor->output_for_input_configuration (required_inputs);

		required_inputs = (*i).out;
		
		++index;
	}
			
	if (err) {
		if (!iclist.empty()) {
			err->index = index;
			err->count = iclist.back().processor->output_for_input_configuration(required_inputs);
		}
	}

	return true;
}

int
Route::copy_processors (const Route& other, Placement placement, ProcessorStreams* err)
{
	ChanCount old_rmo = processor_max_outs;

	ProcessorList to_be_deleted;

	{
		Glib::RWLock::WriterLock lm (_processor_lock);
		ProcessorList::iterator tmp;
		ProcessorList the_copy;

		the_copy = _processors;
		
		/* remove all relevant processors */

		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ) {
			tmp = i;
			++tmp;

			if ((*i)->placement() == placement) {
				to_be_deleted.push_back (*i);
				_processors.erase (i);
			}

			i = tmp;
		}

		/* now copy the relevant ones from "other" */
		
		for (ProcessorList::const_iterator i = other._processors.begin(); i != other._processors.end(); ++i) {
			if ((*i)->placement() == placement) {
				_processors.push_back (IOProcessor::clone (*i));
			}
		}

		/* reset plugin stream handling */

		if (_reset_plugin_counts (err)) {

			/* FAILED COPY ATTEMPT: we have to restore order */

			/* delete all cloned processors */

			for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ) {

				tmp = i;
				++tmp;

				if ((*i)->placement() == placement) {
					_processors.erase (i);
				}
				
				i = tmp;
			}

			/* restore the natural order */

			_processors = the_copy;
			processor_max_outs = old_rmo;

			/* we failed, even though things are OK again */

			return -1;

		} else {
			
			/* SUCCESSFUL COPY ATTEMPT: delete the processors we removed pre-copy */
			to_be_deleted.clear ();
			_user_latency = 0;
		}
	}

	if (processor_max_outs != old_rmo || old_rmo == ChanCount::ZERO) {
		reset_panner ();
	}

	processors_changed (); /* EMIT SIGNAL */
	return 0;
}

void
Route::all_processors_flip ()
{
	Glib::RWLock::ReaderLock lm (_processor_lock);

	if (_processors.empty()) {
		return;
	}

	bool first_is_on = _processors.front()->active();
	
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->set_active (!first_is_on);
	}
	
	_session.set_dirty ();
}

/** Set all processors with a given placement to a given active state.
 * @param p Placement of processors to change.
 * @param state New active state for those processors.
 */
void
Route::all_processors_active (Placement p, bool state)
{
	Glib::RWLock::ReaderLock lm (_processor_lock);

	if (_processors.empty()) {
		return;
	}

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if ((*i)->placement() == p) {
			(*i)->set_active (state);
		}
	}
	
	_session.set_dirty ();
}

struct ProcessorSorter {
    bool operator() (boost::shared_ptr<const Processor> a, boost::shared_ptr<const Processor> b) {
	    return a->sort_key() < b->sort_key();
    }
};

int
Route::sort_processors (ProcessorStreams* err)
{
	{
		ProcessorSorter comparator;
		Glib::RWLock::WriterLock lm (_processor_lock);
		ChanCount old_rmo = processor_max_outs;

		/* the sweet power of C++ ... */

		ProcessorList as_it_was_before = _processors;

		_processors.sort (comparator);
	
		if (_reset_plugin_counts (err)) {
			_processors = as_it_was_before;
			processor_max_outs = old_rmo;
			return -1;
		} 
	} 

	reset_panner ();
	processors_changed (); /* EMIT SIGNAL */

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
	ProcessorList::iterator i;
	char buf[32];

	if (_flags) {
		node->add_property("flags", enum_2_string (_flags));
	}
	
	node->add_property("default-type", _default_type.to_string());

	node->add_property("active", _active?"yes":"no");
	node->add_property("muted", _muted?"yes":"no");
	node->add_property("soloed", _soloed?"yes":"no");
	node->add_property("phase-invert", _phase_invert?"yes":"no");
	node->add_property("denormal-protection", _denormal_protection?"yes":"no");
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

	string order_string;
	OrderKeys::iterator x = order_keys.begin(); 

	while (x != order_keys.end()) {
		order_string += string ((*x).first);
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
	node->add_child_nocopy (_solo_control->get_state ());
	node->add_child_nocopy (_mute_control->get_state ());

	XMLNode* remote_control_node = new XMLNode (X_("remote_control"));
	snprintf (buf, sizeof (buf), "%d", _remote_control_id);
	remote_control_node->add_property (X_("id"), buf);
	node->add_child_nocopy (*remote_control_node);

	if (_control_outs) {
		XMLNode* cnode = new XMLNode (X_("ControlOuts"));
		cnode->add_child_nocopy (_control_outs->state (full_state));
		node->add_child_nocopy (*cnode);
	}

	if (_comment.length()) {
		XMLNode *cmt = node->add_child ("Comment");
		cmt->add_content (_comment);
	}

	for (i = _processors.begin(); i != _processors.end(); ++i) {
		node->add_child_nocopy((*i)->state (full_state));
	}

	if (_extra_xml){
		node->add_child_copy (*_extra_xml);
	}
	
	return *node;
}

XMLNode&
Route::get_processor_state ()
{
	XMLNode* root = new XMLNode (X_("redirects"));
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		root->add_child_nocopy ((*i)->state (true));
	}

	return *root;
}

int
Route::set_processor_state (const XMLNode& root)
{
	if (root.name() != X_("redirects")) {
		return -1;
	}

	XMLNodeList nlist;
	XMLNodeList nnlist;
	XMLNodeConstIterator iter;
	XMLNodeConstIterator niter;
	Glib::RWLock::ReaderLock lm (_processor_lock);

	nlist = root.children();
	
	for (iter = nlist.begin(); iter != nlist.end(); ++iter){

		/* iter now points to a IOProcessor state node */
		
		nnlist = (*iter)->children ();

		for (niter = nnlist.begin(); niter != nnlist.end(); ++niter) {

			/* find the IO child node, since it contains the ID we need */

			/* XXX OOP encapsulation violation, ugh */

			if ((*niter)->name() == IO::state_node_name) {

				XMLProperty* prop = (*niter)->property (X_("id"));
				
				if (!prop) {
					warning << _("IOProcessor node has no ID, ignored") << endmsg;
					break;
				}

				ID id = prop->value ();

				/* now look for a processor with that ID */
	
				for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
					if ((*i)->id() == id) {
						(*i)->set_state (**iter);
						break;
					}
				}
				
				break;
				
			}
		}

	}

	return 0;
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
		add_processor_from_xml (**niter);
	}

	delete deferred_state;
	deferred_state = 0;
}

void
Route::add_processor_from_xml (const XMLNode& node)
{
	const XMLProperty *prop;

	// legacy sessions use a different node name for sends
	if (node.name() == "Send") {
	
		try {
			boost::shared_ptr<Send> send (new Send (_session, node));
			add_processor (send);
		} 
		
		catch (failed_constructor &err) {
			error << _("Send construction failed") << endmsg;
			return;
		}
		
	// use "Processor" in XML?
	} else if (node.name() == "Processor") {
		
		try {
			if ((prop = node.property ("type")) != 0) {

				boost::shared_ptr<Processor> processor;

				if (prop->value() == "ladspa" || prop->value() == "Ladspa" || prop->value() == "vst") {

					processor.reset (new PluginInsert(_session, node));
					
				} else if (prop->value() == "port") {

					processor.reset (new PortInsert (_session, node));
				
				} else if (prop->value() == "send") {

					processor.reset (new Send (_session, node));

				} else {

					error << string_compose(_("unknown Processor type \"%1\"; ignored"), prop->value()) << endmsg;
				}

				add_processor (processor);
				
			} else {
				error << _("Processor XML node has no type property") << endmsg;
			}
		}
		
		catch (failed_constructor &err) {
			warning << _("processor could not be created. Ignored.") << endmsg;
			return;
		}
	}
}

int
Route::set_state (const XMLNode& node)
{
	return _set_state (node, true);
}

int
Route::_set_state (const XMLNode& node, bool call_base)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	XMLNode *child;
	XMLPropertyList plist;
	const XMLProperty *prop;

	if (node.name() != "Route"){
		error << string_compose(_("Bad node sent to Route::set_state() [%1]"), node.name()) << endmsg;
		return -1;
	}

	if ((prop = node.property (X_("flags"))) != 0) {
		_flags = Flag (string_2_enum (prop->value(), _flags));
	} else {
		_flags = Flag (0);
	}
	
	if ((prop = node.property (X_("default-type"))) != 0) {
		_default_type = DataType(prop->value());
		assert(_default_type != DataType::NIL);
	}

	if ((prop = node.property (X_("phase-invert"))) != 0) {
		set_phase_invert (prop->value()=="yes"?true:false, this);
	}

	if ((prop = node.property (X_("denormal-protection"))) != 0) {
		set_denormal_protection (prop->value()=="yes"?true:false, this);
	}

	if ((prop = node.property (X_("active"))) != 0) {
		set_active (prop->value() == "yes");
	}

	if ((prop = node.property (X_("muted"))) != 0) {
		bool yn = prop->value()=="yes"?true:false; 

		/* force reset of mute status */

		_muted = !yn;
		set_mute(yn, this);
		mute_gain = desired_mute_gain;
	}

	if ((prop = node.property (X_("soloed"))) != 0) {
		bool yn = prop->value()=="yes"?true:false; 

		/* force reset of solo status */

		_soloed = !yn;
		set_solo (yn, this);
		solo_gain = desired_solo_gain;
	}

	if ((prop = node.property (X_("mute-affects-pre-fader"))) != 0) {
		_mute_affects_pre_fader = (prop->value()=="yes")?true:false;
	}

	if ((prop = node.property (X_("mute-affects-post-fader"))) != 0) {
		_mute_affects_post_fader = (prop->value()=="yes")?true:false;
	}

	if ((prop = node.property (X_("mute-affects-control-outs"))) != 0) {
		_mute_affects_control_outs = (prop->value()=="yes")?true:false;
	}

	if ((prop = node.property (X_("mute-affects-main-outs"))) != 0) {
		_mute_affects_main_outs = (prop->value()=="yes")?true:false;
	}

	if ((prop = node.property (X_("edit-group"))) != 0) {
		RouteGroup* edit_group = _session.edit_group_by_name(prop->value());
		if(edit_group == 0) {
			error << string_compose(_("Route %1: unknown edit group \"%2 in saved state (ignored)"), _name, prop->value()) << endmsg;
		} else {
			set_edit_group(edit_group, this);
		}
	}

	if ((prop = node.property (X_("order-keys"))) != 0) {

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
					set_order_key (remaining.substr (0, equal).c_str(), n);
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

	deferred_state = new XMLNode(X_("deferred state"));

	/* set parent class properties before anything else */

	for (niter = nlist.begin(); niter != nlist.end(); ++niter){

		child = *niter;

		if (child->name() == IO::state_node_name && call_base) {

			IO::set_state (*child);
			break;
		}
	}

	XMLNodeList processor_nodes;
			
	for (niter = nlist.begin(); niter != nlist.end(); ++niter){

		child = *niter;
			
		if (child->name() == X_("Send") || child->name() == X_("Processor")) {
			processor_nodes.push_back(child);
		}

	}

	_set_processor_states(processor_nodes);


	for (niter = nlist.begin(); niter != nlist.end(); ++niter){
		child = *niter;
		// All processors have been applied already

		if (child->name() == X_("Automation")) {
			
			if ((prop = child->property (X_("path"))) != 0)  {
				load_automation (prop->value());
			}

		} else if (child->name() == X_("ControlOuts")) {
			
			string coutname = _name;
			coutname += _("[control]");

			_control_outs = new IO (_session, coutname);
			_control_outs->set_state (**(child->children().begin()));

		} else if (child->name() == X_("Comment")) {

			/* XXX this is a terrible API design in libxml++ */

			XMLNode *cmt = *(child->children().begin());
			_comment = cmt->content();

		} else if (child->name() == X_("extra")) {

			_extra_xml = new XMLNode (*child);

		} else if (child->name() == X_("controllable") && (prop = child->property("name")) != 0) {
			
			if (prop->value() == "solo") {
				_solo_control->set_state (*child);
				_session.add_controllable (_solo_control);
			}
			else if (prop->value() == "mute") {
				_mute_control->set_state (*child);
				_session.add_controllable (_mute_control);
			}
		}
		else if (child->name() == X_("remote_control")) {
			if ((prop = child->property (X_("id"))) != 0) {
				int32_t x;
				sscanf (prop->value().c_str(), "%d", &x);
				set_remote_control_id (x);
			}
		}
	}

	if ((prop = node.property (X_("mix-group"))) != 0) {
		RouteGroup* mix_group = _session.mix_group_by_name(prop->value());
		if (mix_group == 0) {
			error << string_compose(_("Route %1: unknown mix group \"%2 in saved state (ignored)"), _name, prop->value()) << endmsg;
		}  else {
			set_mix_group(mix_group, this);
		}
	}

	return 0;
}

void
Route::_set_processor_states(const XMLNodeList &nlist)
{
	XMLNodeConstIterator niter;
	char buf[64];

	ProcessorList::iterator i, o;

	// Iterate through existing processors, remove those which are not in the state list
	for (i = _processors.begin(); i != _processors.end(); ) {
		ProcessorList::iterator tmp = i;
		++tmp;

		bool processorInStateList = false;
	
		(*i)->id().print (buf, sizeof (buf));


		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

			// legacy sessions (IOProcessor as a child of Processor, both is-a IO)
			if (strncmp(buf,(*niter)->child(X_("IOProcessor"))->child(X_("IO"))->property(X_("id"))->value().c_str(), sizeof(buf)) == 0) {
				processorInStateList = true;
				break;
			} else if (strncmp(buf,(*niter)->property(X_("id"))->value().c_str(), sizeof(buf)) == 0) {
				processorInStateList = true;
				break;
			}
		}
		
		if (!processorInStateList) {
			remove_processor (*i);
		}


		i = tmp;
	}


	// Iterate through state list and make sure all processors are on the track and in the correct order,
	// set the state of existing processors according to the new state on the same go
	i = _processors.begin();
	for (niter = nlist.begin(); niter != nlist.end(); ++niter, ++i) {

		// Check whether the next processor in the list 
		o = i;

		while (o != _processors.end()) {
			(*o)->id().print (buf, sizeof (buf));
			if ( strncmp(buf, (*niter)->child(X_("IOProcessor"))->child(X_("IO"))->property(X_("id"))->value().c_str(), sizeof(buf)) == 0)
				break;
			else if (strncmp(buf,(*niter)->property(X_("id"))->value().c_str(), sizeof(buf)) == 0)
				break;
			
			++o;
		}

		if (o == _processors.end()) {
			// If the processor (*niter) is not on the route, we need to create it
			// and move it to the correct location

			ProcessorList::iterator prev_last = _processors.end();
			--prev_last; // We need this to check whether adding succeeded
			
			add_processor_from_xml (**niter);

			ProcessorList::iterator last = _processors.end();
			--last;

			if (prev_last == last) {
				cerr << "Could not fully restore state as some processors were not possible to create" << endl;
				continue;

			}

			boost::shared_ptr<Processor> tmp = (*last);
			// remove the processor from the wrong location
			_processors.erase(last);
			// processor the new processor at the current location
			_processors.insert(i, tmp);

			--i; // move pointer to the newly processored processor
			continue;
		}

		// We found the processor (*niter) on the route, first we must make sure the processor
		// is at the location provided in the XML state
		if (i != o) {
			boost::shared_ptr<Processor> tmp = (*o);
			// remove the old copy
			_processors.erase(o);
			// processor the processor at the correct location
			_processors.insert(i, tmp);

			--i; // move pointer so it points to the right processor
		}

		(*i)->set_state( (**niter) );
	}
	
	processors_changed ();
}

void
Route::curve_reallocate ()
{
//	_gain_automation_curve.finish_resize ();
//	_pan_automation_curve.finish_resize ();
}

void
Route::silence (nframes_t nframes, nframes_t offset)
{
	if (!_silent) {

		IO::silence (nframes, offset);

		if (_control_outs) {
			_control_outs->silence (nframes, offset);
		}

		{ 
			Glib::RWLock::ReaderLock lm (_processor_lock, Glib::TRY_LOCK);
			
			if (lm.locked()) {
				for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
					boost::shared_ptr<PluginInsert> pi;
					if (!_active && (pi = boost::dynamic_pointer_cast<PluginInsert> (*i)) != 0) {
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
	Glib::Mutex::Lock lm (_control_outs_lock);
	vector<string>::const_iterator i;
	size_t limit;
	
 	if (_control_outs) {
 		delete _control_outs;
 		_control_outs = 0;
 	}

	if (is_control() || is_master()) {
		/* no control outs for these two special busses */
		return 0;
	}
 	
 	if (ports.empty()) {
 		return 0;
 	}
 
 	string coutname = _name;
 	coutname += _("[control]");
 	
 	_control_outs = new IO (_session, coutname);

	/* our control outs need as many outputs as we
	   have audio outputs. we track the changes in ::output_change_handler().
	*/
	
	// XXX its stupid that we have to get this value twice

	limit = n_outputs().n_audio();
	
	if (_control_outs->ensure_io (ChanCount::ZERO, ChanCount (DataType::AUDIO, n_outputs().get (DataType::AUDIO)), true, this)) {
		return -1;
	}
	
	/* now connect to the named ports */
	
	for (size_t n = 0; n < limit; ++n) {
		if (_control_outs->connect_output (_control_outs->output (n), ports[n], this)) {
			error << string_compose (_("could not connect %1 to %2"), _control_outs->output(n)->name(), ports[n]) << endmsg;
			return -1;
		}
	}
 
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
Route::feeds (boost::shared_ptr<Route> other)
{
	uint32_t i, j;

	IO& self = *this;
	uint32_t no = self.n_outputs().n_total();
	uint32_t ni = other->n_inputs ().n_total();

	for (i = 0; i < no; ++i) {
		for (j = 0; j < ni; ++j) {
			if (self.output(i)->connected_to (other->input(j)->name())) {
				return true;
			}
		}
	}

	/* check IOProcessors which may also interconnect Routes */

	for (ProcessorList::iterator r = _processors.begin(); r != _processors.end(); r++) {

		boost::shared_ptr<IOProcessor> redirect = boost::dynamic_pointer_cast<IOProcessor>(*r);

		if ( ! redirect)
			continue;

		// TODO: support internal redirects here

		no = redirect->io()->n_outputs().n_total();

		for (i = 0; i < no; ++i) {
			for (j = 0; j < ni; ++j) {
				if (redirect->io()->output(i)->connected_to (other->input (j)->name())) {
					return true;
				}
			}
		}
	}

	/* check for control room outputs which may also interconnect Routes */

	if (_control_outs) {

		no = _control_outs->n_outputs().n_total();
		
		for (i = 0; i < no; ++i) {
			for (j = 0; j < ni; ++j) {
				if (_control_outs->output(i)->connected_to (other->input (j)->name())) {
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
Route::handle_transport_stopped (bool abort_ignored, bool did_locate, bool can_flush_processors)
{
	nframes_t now = _session.transport_frame();

	{
		Glib::RWLock::ReaderLock lm (_processor_lock);

		if (!did_locate) {
			automation_snapshot (now);
		}

		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
			
			if (Config->get_plugins_stop_with_transport() && can_flush_processors) {
				(*i)->deactivate ();
				(*i)->activate ();
			}
			
			(*i)->transport_stopped (now);
		}
	}

	IO::transport_stopped (now);
 
	_roll_delay = _initial_delay;
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
			_control_outs->ensure_io (ChanCount::ZERO, ChanCount(DataType::AUDIO, n_outputs().n_audio()), true, this);
		}
		
		reset_plugin_counts (0);
	}
}

uint32_t
Route::pans_required () const
{
	if (n_outputs().n_audio() < 2) {
		return 0;
	}
	
	return max (n_inputs ().n_audio(), processor_max_outs.n_audio());
}

int 
Route::no_roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, nframes_t offset, 
		   bool session_state_changing, bool can_record, bool rec_monitors_input)
{
	if (n_outputs().n_total() == 0) {
		return 0;
	}

	if (session_state_changing || !_active)  {
		silence (nframes, offset);
		return 0;
	}

	apply_gain_automation = false;
	
	if (n_inputs().n_total()) {
		passthru (start_frame, end_frame, nframes, offset, 0, false);
	} else {
		silence (nframes, offset);
	}

	return 0;
}

nframes_t
Route::check_initial_delay (nframes_t nframes, nframes_t& offset, nframes_t& transport_frame)
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
Route::roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, nframes_t offset, int declick,
	     bool can_record, bool rec_monitors_input)
{
	{
		Glib::RWLock::ReaderLock lm (_processor_lock, Glib::TRY_LOCK);
		if (lm.locked()) {
			// automation snapshot can also be called from the non-rt context
			// and it uses the processor list, so we take the lock out here
			automation_snapshot (_session.transport_frame());
		}
	}

	if ((n_outputs().n_total() == 0 && _processors.empty()) || n_inputs().n_total() == 0 || !_active) {
		silence (nframes, offset);
		return 0;
	}
	
	nframes_t unused = 0;

	if ((nframes = check_initial_delay (nframes, offset, unused)) == 0) {
		return 0;
	}

	_silent = false;

	apply_gain_automation = false;

	{ 
		Glib::Mutex::Lock am (_automation_lock, Glib::TRY_LOCK);
		
		if (am.locked() && _session.transport_rolling()) {
			
			if (_gain_control->list()->automation_playback()) {
				apply_gain_automation = _gain_control->list()->curve().rt_safe_get_vector (
						start_frame, end_frame, _session.gain_automation_buffer(), nframes);
			}
		}
	}

	passthru (start_frame, end_frame, nframes, offset, declick, false);

	return 0;
}

int
Route::silent_roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, nframes_t offset, 
		    bool can_record, bool rec_monitors_input)
{
	silence (nframes, offset);
	return 0;
}

void
Route::toggle_monitor_input ()
{
	for (PortSet::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
		i->ensure_monitor_input( ! i->monitoring_input());
	}
}

bool
Route::has_external_redirects () const
{
	// FIXME: what about sends?

	boost::shared_ptr<const PortInsert> pi;
	
	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if ((pi = boost::dynamic_pointer_cast<const PortInsert>(*i)) != 0) {

			for (PortSet::const_iterator port = pi->io()->outputs().begin();
					port != pi->io()->outputs().end(); ++port) {
				
				string port_name = port->name();
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
Route::flush_processors ()
{
	/* XXX shouldn't really try to take this lock, since
	   this is called from the RT audio thread.
	*/

	Glib::RWLock::ReaderLock lm (_processor_lock);

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
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

nframes_t
Route::update_total_latency ()
{
	nframes_t old = _own_latency;

	if (_user_latency) {
		_own_latency = _user_latency;
	} else {
		_own_latency = 0;

		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
			if ((*i)->active ()) {
				_own_latency += (*i)->signal_latency ();
			}
		}
	}

	set_port_latency (_own_latency);
	
	if (!_user_latency) {
		/* this (virtual) function is used for pure Routes,
		   not derived classes like AudioTrack.  this means
		   that the data processed here comes from an input
		   port, not prerecorded material, and therefore we
		   have to take into account any input latency.
		*/


		_own_latency += input_latency ();
	}

	if (old != _own_latency) {
		signal_latency_changed (); /* EMIT SIGNAL */
	}
	
	return _own_latency;
}

void
Route::set_user_latency (nframes_t nframes)
{
	Latent::set_user_latency (nframes);
	_session.update_latency_compensation (false, false);
}

void
Route::set_latency_delay (nframes_t longest_session_latency)
{
	nframes_t old = _initial_delay;

	if (_own_latency < longest_session_latency) {
		_initial_delay = longest_session_latency - _own_latency;
	} else {
		_initial_delay = 0;
	}

	if (_initial_delay != old) {
		initial_delay_changed (); /* EMIT SIGNAL */
	}

	if (_session.transport_stopped()) {
		_roll_delay = _initial_delay;
	}
}

void
Route::automation_snapshot (nframes_t now)
{
	IO::automation_snapshot (now);

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->automation_snapshot (now);
	}
}

Route::ToggleControllable::ToggleControllable (std::string name, Route& s, ToggleType tp)
	: Controllable (name), route (s), type(tp)
{
	
}

void
Route::ToggleControllable::set_value (float val)
{
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
}

float
Route::ToggleControllable::get_value (void) const
{
	float val = 0.0f;
	
	switch (type) {
	case MuteControl:
		val = route.muted() ? 1.0f : 0.0f;
		break;
	case SoloControl:
		val = route.soloed() ? 1.0f : 0.0f;
		break;
	default:
		break;
	}

	return val;
}

void 
Route::set_block_size (nframes_t nframes)
{
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->set_block_size (nframes);
	}
}

void
Route::protect_automation ()
{
	Automatable::protect_automation();
	
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i)
		(*i)->protect_automation();
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
