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
#include "pbd/xml++.h"
#include "pbd/enumwriter.h"
#include "pbd/stacktrace.h"
#include "pbd/memento_command.h"

#include "evoral/Curve.hpp"

#include "ardour/amp.h"
#include "ardour/audio_port.h"
#include "ardour/audioengine.h"
#include "ardour/buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/configuration.h"
#include "ardour/control_outputs.h"
#include "ardour/cycle_timer.h"
#include "ardour/dB.h"
#include "ardour/ladspa_plugin.h"
#include "ardour/meter.h"
#include "ardour/mix.h"
#include "ardour/panner.h"
#include "ardour/plugin_insert.h"
#include "ardour/port.h"
#include "ardour/port_insert.h"
#include "ardour/processor.h"
#include "ardour/profile.h"
#include "ardour/route.h"
#include "ardour/route_group.h"
#include "ardour/send.h"
#include "ardour/session.h"
#include "ardour/timestamps.h"
#include "ardour/utils.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

uint32_t Route::order_key_cnt = 0;
sigc::signal<void,const char*> Route::SyncOrderKeys;

Route::Route (Session& sess, string name, Flag flg,
		DataType default_type, ChanCount in, ChanCount out)
	: IO (sess, name, default_type, in, ChanCount::INFINITE, out, ChanCount::INFINITE)
	, _flags (flg)
	, _solo_control (new ToggleControllable (X_("solo"), *this, ToggleControllable::SoloControl))
	, _mute_control (new ToggleControllable (X_("mute"), *this, ToggleControllable::MuteControl))
{
	_configured_inputs = in;
	_configured_outputs = out;
	init ();
}

Route::Route (Session& sess, const XMLNode& node, DataType default_type)
	: IO (sess, *node.child ("IO"), default_type)
	, _solo_control (new ToggleControllable (X_("solo"), *this, ToggleControllable::SoloControl))
	, _mute_control (new ToggleControllable (X_("mute"), *this, ToggleControllable::MuteControl))
{
	init ();
	_set_state (node, false);
}

void
Route::init ()
{
	processor_max_streams.reset();
	_muted = false;
	_soloed = false;
	_solo_safe = false;
	_recordable = true;
	_active = true;
	_phase_invert = false;
	_denormal_protection = false;
	order_keys[strdup (N_("signal"))] = order_key_cnt++;
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
	_in_configure_processors = false;
	
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

	input_changed.connect (mem_fun (this, &Route::input_change_handler));
	output_changed.connect (mem_fun (this, &Route::output_change_handler));

	_amp->set_sort_key (0);
	_meter->set_sort_key (1);
	add_processor (_amp, NULL);
	add_processor (_meter, NULL);
}

Route::~Route ()
{
	clear_processors (PreFader);
	clear_processors (PostFader);

	for (OrderKeys::iterator i = order_keys.begin(); i != order_keys.end(); ++i) {
		free ((void*)(i->first));
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

	if (Config->get_sync_all_route_ordering()) {
		for (OrderKeys::iterator x = order_keys.begin(); x != order_keys.end(); ++x) {
			x->second = n;
		}
	} 

	_session.set_dirty ();
}

void
Route::sync_order_keys (const char* base)
{
	if (order_keys.empty()) {
		return;
	}

	OrderKeys::iterator i;
	uint32_t key;

	if ((i = order_keys.find (base)) == order_keys.end()) {
		/* key doesn't exist, use the first existing key (during session initialization) */
		i = order_keys.begin();
		key = i->second;
		++i;
	} else {
		/* key exists - use it and reset all others (actually, itself included) */
		key = i->second;
		i = order_keys.begin();
	}

	for (; i != order_keys.end(); ++i) {
		i->second = key;
	}
}

string
Route::ensure_track_or_route_name(string name, Session &session)
{
	string newname = name;

	while (session.route_by_name (newname) != NULL) {
		newname = bump_name_once (newname);
	}

	return newname;
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
 *
 * Note that (end_frame - start_frame) may not be equal to nframes when the
 * transport speed isn't 1.0 (eg varispeed).
 */
void
Route::process_output_buffers (BufferSet& bufs,
		nframes_t start_frame, nframes_t end_frame, nframes_t nframes,
		bool with_processors, int declick)
{
	ProcessorList::iterator i;
	bool mute_declick_applied = false;
	gain_t dmg, dsg, dg;
	bool no_monitor;

	bufs.is_silent(false);

	switch (Config->get_monitoring_model()) {
	case HardwareMonitoring:
	case ExternalMonitoring:
		no_monitor = true;
		break;
	default:
		no_monitor = false;
	}

	declick = _pending_declick;
	
	const bool recording_without_monitoring = no_monitor && record_enabled()
			&& (!Config->get_auto_input() || _session.actively_recording());
	

	/* -------------------------------------------------------------------------------------------
	   SET UP GAIN (FADER)
	   ----------------------------------------------------------------------------------------- */

	{ 
		Glib::Mutex::Lock dm (declick_lock, Glib::TRY_LOCK);
		
		if (dm.locked()) {
			dmg = desired_mute_gain;
			dsg = desired_solo_gain;
			dg = _gain_control->user_float();
		} else {
			dmg = mute_gain;
			dsg = solo_gain;
			dg = _gain;
		}
	}
	
	// apply gain at the amp if...
	_amp->apply_gain(
			// we're not recording
			!(record_enabled() && _session.actively_recording())
			// or (we are recording, and) software monitoring is required
			|| Config->get_monitoring_model() == SoftwareMonitoring);
	
	// mute at the amp if...
	_amp->apply_mute(
		!_soloed && (mute_gain != dmg) && !mute_declick_applied && _mute_affects_post_fader,
		mute_gain, dmg);

	_amp->set_gain (_gain, dg);
	

	/* -------------------------------------------------------------------------------------------
	   SET UP CONTROL OUTPUTS
	   ----------------------------------------------------------------------------------------- */

	boost::shared_ptr<ControlOutputs> co = _control_outs;
	if (co) {
		// deliver control outputs unless we're ...
		co->deliver (!(
				dsg == 0 || // muted by solo of another track
				(dmg == 0 && _mute_affects_control_outs) || // or muted by mute of this track
				!recording_without_monitoring )); // or rec-enabled w/o s/w monitoring 
	}
	

	/* -------------------------------------------------------------------------------------------
	   GLOBAL DECLICK (for transport changes etc.)
	   ----------------------------------------------------------------------------------------- */

	if (declick > 0) {
		Amp::apply_gain (bufs, nframes, 0.0, 1.0, false);
		_pending_declick = 0;
	} else if (declick < 0) {
		Amp::apply_gain (bufs, nframes, 1.0, 0.0, false);
		_pending_declick = 0;
	} else { // no global declick
		if (solo_gain != dsg) {
			Amp::apply_gain (bufs, nframes, solo_gain, dsg, false);
			solo_gain = dsg;
		}
	}


	/* -------------------------------------------------------------------------------------------
	   PRE-FADER MUTING
	   ----------------------------------------------------------------------------------------- */

	if (!_soloed && _mute_affects_pre_fader && (mute_gain != dmg)) {
		Amp::apply_gain (bufs, nframes, mute_gain, dmg, false);
		mute_gain = dmg;
		mute_declick_applied = true;
	}
	if (mute_gain == 0.0f && dmg == 0.0f) {
		bufs.is_silent(true);
	}


	/* -------------------------------------------------------------------------------------------
	   DENORMAL CONTROL
	   ----------------------------------------------------------------------------------------- */

	if (_denormal_protection || Config->get_denormal_protection()) {

		for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
			Sample* const sp = i->data();
			
			for (nframes_t nx = 0; nx < nframes; ++nx) {
				sp[nx] += 1.0e-27f;
			}
		}
	}


	/* -------------------------------------------------------------------------------------------
	   PROCESSORS (including Amp (fader) and Meter)
	   ----------------------------------------------------------------------------------------- */

	if (with_processors) {
		Glib::RWLock::ReaderLock rm (_processor_lock, Glib::TRY_LOCK);
		if (rm.locked()) {
			//if (!bufs.is_silent()) {
				for (i = _processors.begin(); i != _processors.end(); ++i) {
					bufs.set_count(ChanCount::max(bufs.count(), (*i)->input_streams()));
					(*i)->run_in_place (bufs, start_frame, end_frame, nframes);
					bufs.set_count(ChanCount::max(bufs.count(), (*i)->output_streams()));
				}
			/*} else {
				for (i = _processors.begin(); i != _processors.end(); ++i) {
					(*i)->silence (nframes);
					bufs.set_count(ChanCount::max(bufs.count(), (*i)->output_streams()));
				}
			}*/
		} 

		if (!_processors.empty()) {
			bufs.set_count(ChanCount::max(bufs.count(), _processors.back()->output_streams()));
		}
	}
	

	/* -------------------------------------------------------------------------------------------
	   POST-FADER MUTING
	   ----------------------------------------------------------------------------------------- */

	if (!_soloed && (mute_gain != dmg) && !mute_declick_applied && _mute_affects_main_outs) {
		Amp::apply_gain (bufs, nframes, mute_gain, dmg, false);
		mute_gain = dmg;
		mute_declick_applied = true;
	}
	if (mute_gain == 0.0f && dmg == 0.0f) {
		bufs.is_silent(true);
	}
	

	/* -------------------------------------------------------------------------------------------
	   MAIN OUTPUT STAGE
	   ----------------------------------------------------------------------------------------- */

	bool solo_audible = dsg > 0;
	bool mute_audible = dmg > 0 || !_mute_affects_main_outs;
	
	if (n_outputs().get(_default_type) == 0) {
	    
	    /* relax */

	} else if (recording_without_monitoring) {

		IO::silence (nframes);

	} else {

		if ( // we're silent anyway
			(_gain == 0 && !_amp->apply_gain_automation()) ||
			
			// or muted by solo of another track, but not using control outs for solo
			(!solo_audible && (Config->get_solo_model() != SoloBus)) ||
			
			// or muted by mute of this track
			!mute_audible
			) {

			/* don't use Route::silence() here, because that causes
			   all outputs (sends, port processors, etc. to be silent).
			*/
			IO::silence (nframes);
			
		} else {
			
			deliver_output(bufs, start_frame, end_frame, nframes);

		}

	}

	/* -------------------------------------------------------------------------------------------
	   POST-FADER METERING
	   ----------------------------------------------------------------------------------------- */

	/* TODO: Processor-list-ification needs to go further for this to be cleanly possible...
	if (meter && (_meter_point == MeterPostFader)) {
		if ((_gain == 0 && !apply_gain_automation) || dmg == 0) {
			_meter->reset();
		} else {
			_meter->run_in_place(output_buffers(), start_frame, end_frame, nframes);
		}
	}*/

	// at this point we've reached the desired mute gain regardless
	mute_gain = dmg;
}

ChanCount
Route::n_process_buffers ()
{
	return max (n_inputs(), processor_max_streams);
}

void
Route::setup_peak_meters()
{
	ChanCount max_streams = std::max (_inputs.count(), _outputs.count());
	max_streams = std::max (max_streams, processor_max_streams);
	_meter->configure_io (max_streams, max_streams);
}

void
Route::passthru (nframes_t start_frame, nframes_t end_frame, nframes_t nframes, int declick)
{
	BufferSet& bufs = _session.get_scratch_buffers(n_process_buffers());

	_silent = false;

	collect_input (bufs, nframes);

	process_output_buffers (bufs, start_frame, end_frame, nframes, true, declick);
}

void
Route::passthru_silence (nframes_t start_frame, nframes_t end_frame, nframes_t nframes, int declick)
{
	process_output_buffers (_session.get_silent_buffers (n_process_buffers()), start_frame, end_frame, nframes, true, declick);
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
	
	catch_up_on_solo_mute_override ();
}

void
Route::catch_up_on_solo_mute_override ()
{
	if (Config->get_solo_model() != InverseMute) {
		return;
	}
	
	{
		Glib::Mutex::Lock lm (declick_lock);
		
		if (_muted) {
			if (Config->get_solo_mute_override()) {
				desired_mute_gain = (_soloed?1.0:0.0);
			} else {
				desired_mute_gain = 0.0;
			}
		} else {
			desired_mute_gain = 1.0;
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
		
		_mute_control->Changed (); /* EMIT SIGNAL */
		
		Glib::Mutex::Lock lm (declick_lock);
		
		if (_soloed && Config->get_solo_mute_override()) {
			desired_mute_gain = 1.0f;
		} else {
			desired_mute_gain = (yn?0.0f:1.0f);
		}
	}
}

static void
dump_processors(const string& name, const list<boost::shared_ptr<Processor> >& procs)
{
	cerr << name << " {" << endl;
	for (list<boost::shared_ptr<Processor> >::const_iterator p = procs.begin();
			p != procs.end(); ++p) {
		cerr << "\t" << (*p)->sort_key() << ": " << (*p)->name() << endl;
	}
	cerr << "}" << endl;
}

/** Add a processor to the route.
 * If @a iter is not NULL, it must point to an iterator in _processors and the new
 * processor will be inserted immediately before this location.  Otherwise,
 * @a position is used.
 */
int
Route::add_processor (boost::shared_ptr<Processor> processor, ProcessorStreams* err, ProcessorList::iterator* iter, Placement placement)
{
	ChanCount old_pms = processor_max_streams;

	if (!_session.engine().connected() || !processor) {
		return 1;
	}

	{
		Glib::RWLock::WriterLock lm (_processor_lock);

		boost::shared_ptr<PluginInsert> pi;
		boost::shared_ptr<PortInsert> porti;

		ProcessorList::iterator loc = find(_processors.begin(), _processors.end(), processor);

		if (processor == _amp || processor == _meter) {
			// Ensure only one amp and one meter are in the list at any time
			if (loc != _processors.end()) {
			   	if (iter) {
					if (*iter == loc) { // Already in place, do nothing
						return 0;
					} else { // New position given, relocate
						_processors.erase(loc);
					}
				} else { // Insert at end
					_processors.erase(loc);
					loc = _processors.end();
				}
			}

		} else {
			if (loc != _processors.end()) {
				cerr << "ERROR: Processor added to route twice!" << endl;
				return 1;
			}
		}

		// Use position given by user
		if (iter) {
			loc = *iter;

		// Insert immediately before the amp
		} else if (placement == PreFader) {
			loc = find(_processors.begin(), _processors.end(), _amp);

		// Insert at end
		} else {
			loc = _processors.end();
		}
		
		// Update sort keys
		if (loc == _processors.end()) {
			processor->set_sort_key(_processors.size());
		} else {
			processor->set_sort_key((*loc)->sort_key());
			for (ProcessorList::iterator p = loc; p != _processors.end(); ++p) {
				(*p)->set_sort_key((*p)->sort_key() + 1);
			}
		}

		_processors.insert(loc, processor);

		// Set up processor list channels.  This will set processor->[input|output]_streams(),
		// configure redirect ports properly, etc.
		if (configure_processors_unlocked (err)) {
			dump_processors(_name, _processors);
			ProcessorList::iterator ploc = loc;
			--ploc;
			_processors.erase(ploc);
			configure_processors_unlocked (0); // it worked before we tried to add it ...
			return -1;
		}
	
		if ((pi = boost::dynamic_pointer_cast<PluginInsert>(processor)) != 0) {
			
			if (pi->natural_input_streams() == ChanCount::ZERO) {
				/* generator plugin */
				_have_internal_generator = true;
			}
			
		}
		
		// Ensure peak vector sizes before the plugin is activated
		ChanCount potential_max_streams = ChanCount::max(
				processor->input_streams(), processor->output_streams());

		_meter->configure_io (potential_max_streams, potential_max_streams);

		// XXX: do we want to emit the signal here ? change call order.
		processor->activate ();
		processor->ActiveChanged.connect (bind (mem_fun (_session, &Session::update_latency_compensation), false, false));

		_user_latency = 0;
	}
	
	if (processor_max_streams != old_pms || old_pms == ChanCount::ZERO) {
		reset_panner ();
	}

	dump_processors (_name, _processors);
	processors_changed (); /* EMIT SIGNAL */

	
	return 0;
}

int
Route::add_processors (const ProcessorList& others, ProcessorStreams* err, Placement placement)
{
	/* NOTE: this is intended to be used ONLY when copying
	   processors from another Route. Hence the subtle
	   differences between this and ::add_processor()
	*/

	ChanCount old_pms = processor_max_streams;

	if (!_session.engine().connected()) {
		return 1;
	}

	{
		Glib::RWLock::WriterLock lm (_processor_lock);

		ProcessorList::iterator existing_end = _processors.end();
		--existing_end;

		ChanCount potential_max_streams = ChanCount::max(input_minimum(), output_minimum());

		for (ProcessorList::const_iterator i = others.begin(); i != others.end(); ++i) {
			
			// Ensure meter only appears in the list once
			if (*i == _meter) {
				ProcessorList::iterator m = find(_processors.begin(), _processors.end(), *i);
				if (m != _processors.end()) {
					_processors.erase(m);
				}
			}
			
			boost::shared_ptr<PluginInsert> pi;
			
			if ((pi = boost::dynamic_pointer_cast<PluginInsert>(*i)) != 0) {
				pi->set_count (1);
				
				ChanCount m = max(pi->input_streams(), pi->output_streams());
				if (m > potential_max_streams)
					potential_max_streams = m;
			}

			// Ensure peak vector sizes before the plugin is activated
			_meter->configure_io (potential_max_streams, potential_max_streams);

			ProcessorList::iterator loc = (placement == PreFader)
					? find(_processors.begin(), _processors.end(), _amp)
					: _processors.end();

			_processors.insert (loc, *i);
			
			if (configure_processors_unlocked (err)) {
				++existing_end;
				_processors.erase (existing_end, _processors.end());
				configure_processors_unlocked (0); // it worked before we tried to add it ...
				return -1;
			}
			
			(*i)->ActiveChanged.connect (bind (mem_fun (_session, &Session::update_latency_compensation), false, false));
		}

		_user_latency = 0;
	}
	
	if (processor_max_streams != old_pms || old_pms == ChanCount::ZERO) {
		reset_panner ();
	}
	
	dump_processors (_name, _processors);
	processors_changed (); /* EMIT SIGNAL */

	return 0;
}

void
Route::placement_range(Placement p, ProcessorList::iterator& start, ProcessorList::iterator& end)
{
	if (p == PreFader) {
		start = _processors.begin();
		end = find(_processors.begin(), _processors.end(), _amp);
	} else {
		start = find(_processors.begin(), _processors.end(), _amp);
		++start;
		end = _processors.end();
	}
}

/** Turn off all processors with a given placement
 * @param p Placement of processors to disable
 */
void
Route::disable_processors (Placement p)
{
	Glib::RWLock::ReaderLock lm (_processor_lock);
	
	ProcessorList::iterator start, end;
	placement_range(p, start, end);
	
	for (ProcessorList::iterator i = start; i != end; ++i) {
		(*i)->deactivate ();
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
		(*i)->deactivate ();
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
	
	ProcessorList::iterator start, end;
	placement_range(p, start, end);
	
	for (ProcessorList::iterator i = start; i != end; ++i) {
		if (boost::dynamic_pointer_cast<PluginInsert> (*i)) {
			(*i)->deactivate ();
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
			(*i)->deactivate ();
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
				(*i)->deactivate ();
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
				(*i)->activate ();
			} else {
				(*i)->deactivate ();
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

	/* Find the last pre-fader redirect that isn't a send; sends don't affect the number
	 * of streams. */
	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if ((*i) == _amp) {
			break;
		}
		if (boost::dynamic_pointer_cast<Send> (*i) == 0) {
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
	const ChanCount old_pms = processor_max_streams;

	if (!_session.engine().connected()) {
		return;
	}
	
	bool already_deleting = _session.deletion_in_progress();
	if (!already_deleting) {
		_session.set_deletion_in_progress();
	}

	{
		Glib::RWLock::WriterLock lm (_processor_lock);
		ProcessorList new_list;
		
		ProcessorList::iterator amp_loc = find(_processors.begin(), _processors.end(), _amp);
		if (p == PreFader) {
			// Get rid of PreFader processors
			for (ProcessorList::iterator i = _processors.begin(); i != amp_loc; ++i) {
				(*i)->drop_references ();
			}
			// Keep the rest
			for (ProcessorList::iterator i = amp_loc; i != _processors.end(); ++i) {
				new_list.push_back (*i);
			}
		} else {
			// Keep PreFader processors
			for (ProcessorList::iterator i = _processors.begin(); i != amp_loc; ++i) {
				new_list.push_back (*i);
			}
			new_list.push_back (_amp);
			// Get rid of PostFader processors
			for (ProcessorList::iterator i = amp_loc; i != _processors.end(); ++i) {
				(*i)->drop_references ();
			}
		}

		_processors = new_list;
	}

	/* FIXME: can't see how this test can ever fire */
	if (processor_max_streams != old_pms) {
		reset_panner ();
	}
	
	processor_max_streams.reset();
	_have_internal_generator = false;
	processors_changed (); /* EMIT SIGNAL */

	if (!already_deleting) {
		_session.clear_deletion_in_progress();
	}
}

int
Route::remove_processor (boost::shared_ptr<Processor> processor, ProcessorStreams* err)
{
	if (processor == _amp || processor == _meter) {
		return 0;
	}

	ChanCount old_pms = processor_max_streams;

	if (!_session.engine().connected()) {
		return 1;
	}

	processor_max_streams.reset();

	{
		Glib::RWLock::WriterLock lm (_processor_lock);
		ProcessorList::iterator i;
		bool removed = false;

		for (i = _processors.begin(); i != _processors.end(); ++i) {
			if (*i == processor) {

				ProcessorList::iterator tmp;

				/* move along, see failure case for configure_processors()
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

		if (configure_processors_unlocked (err)) {
			/* get back to where we where */
			_processors.insert (i, processor);
			/* we know this will work, because it worked before :) */
			configure_processors_unlocked (0);
			return -1;
		}

		_have_internal_generator = false;

		for (i = _processors.begin(); i != _processors.end(); ++i) {
			boost::shared_ptr<PluginInsert> pi;
			
			if ((pi = boost::dynamic_pointer_cast<PluginInsert>(*i)) != 0) {
				if (pi->is_generator()) {
					_have_internal_generator = true;
					break;
				}
			}
		}
	}

	if (old_pms != processor_max_streams) {
		reset_panner ();
	}

	processor->drop_references ();
	
	dump_processors (_name, _processors);
	processors_changed (); /* EMIT SIGNAL */

	return 0;
}

int
Route::configure_processors (ProcessorStreams* err)
{
	if (!_in_configure_processors) {
		Glib::RWLock::WriterLock lm (_processor_lock);
		return configure_processors_unlocked (err);
	}
	return 0;
}

/** Configure the input/output configuration of each processor in the processors list.
 * Return 0 on success, otherwise configuration is impossible.
 */
int
Route::configure_processors_unlocked (ProcessorStreams* err)
{
	if (_in_configure_processors) {
	   return 0;
	}

	_in_configure_processors = true;

	// Check each processor in order to see if we can configure as requested
	ChanCount in = _configured_inputs;
	ChanCount out;
	list< pair<ChanCount,ChanCount> > configuration;
	uint32_t index = 0;
	for (ProcessorList::iterator p = _processors.begin(); p != _processors.end(); ++p, ++index) {
		(*p)->set_sort_key(index);
		if ((*p)->can_support_io_configuration(in, out)) {
			configuration.push_back(make_pair(in, out));
			in = out;
		} else {
			if (err) {
				err->index = index;
				err->count = in;
			}
			_in_configure_processors = false;
			return -1;
		}
	}
	
	// We can, so configure everything
	list< pair<ChanCount,ChanCount> >::iterator c = configuration.begin();
	for (ProcessorList::iterator p = _processors.begin(); p != _processors.end(); ++p, ++c) {
		(*p)->configure_io(c->first, c->second);
		(*p)->activate();
		processor_max_streams = ChanCount::max(processor_max_streams, c->first);
		processor_max_streams = ChanCount::max(processor_max_streams, c->second);
		out = c->second;
	}

	// Ensure route outputs match last processor's outputs
	if (out != n_outputs()) {
		ensure_io(_configured_inputs, out, false, this);
	}

	_in_configure_processors = false;
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
		if (first_is_on) {
			(*i)->deactivate ();
		} else {
			(*i)->activate ();
		}
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
	ProcessorList::iterator start, end;
	placement_range(p, start, end);

	bool before_amp = true;
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if ((*i) == _amp) {
			before_amp = false;
			continue;
		}
		if (p == PreFader && before_amp) {
			if (state) {
				(*i)->activate ();
			} else {
				(*i)->deactivate ();
			}
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
		ChanCount old_pms = processor_max_streams;

		/* the sweet power of C++ ... */

		ProcessorList as_it_was_before = _processors;

		_processors.sort (comparator);
	
		if (configure_processors_unlocked (err)) {
			_processors = as_it_was_before;
			processor_max_streams = old_pms;
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
	node->add_property("meter-point", enum_2_string (_meter_point));

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

	XMLNode* remote_control_node = new XMLNode (X_("RemoteControl"));
	snprintf (buf, sizeof (buf), "%d", _remote_control_id);
	remote_control_node->add_property (X_("id"), buf);
	node->add_child_nocopy (*remote_control_node);

	if (_control_outs) {
		XMLNode* cnode = new XMLNode (X_("ControlOuts"));
		cnode->add_child_nocopy (_control_outs->io()->state (full_state));
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

bool
Route::add_processor_from_xml (const XMLNode& node, ProcessorList::iterator* iter)
{
	const XMLProperty *prop;

	// legacy sessions use a different node name for sends
	if (node.name() == "Send") {
	
		try {
			boost::shared_ptr<Send> send (new Send (_session, node));
			add_processor (send, 0, iter);
			return true;
		} 
		
		catch (failed_constructor &err) {
			error << _("Send construction failed") << endmsg;
			return false;
		}
		
	} else if (node.name() == "Processor") {
		
		try {
			if ((prop = node.property ("type")) != 0) {

				boost::shared_ptr<Processor> processor;
				bool have_insert = false;

				if (prop->value() == "ladspa" || prop->value() == "Ladspa" || 
				    prop->value() == "lv2" ||
				    prop->value() == "vst" ||
				    prop->value() == "audiounit") {
					
					processor.reset (new PluginInsert(_session, node));
					have_insert = true;
					
				} else if (prop->value() == "port") {

					processor.reset (new PortInsert (_session, node));
				
				} else if (prop->value() == "send") {

					processor.reset (new Send (_session, node));
					have_insert = true;
				
				} else if (prop->value() == "meter") {

					processor = _meter;
				
				} else if (prop->value() == "amp") {
					
					processor = _amp;

				} else {

					error << string_compose(_("unknown Processor type \"%1\"; ignored"), prop->value()) << endmsg;
				}
				
				return (add_processor (processor, 0, iter) == 0);
				
			} else {
				error << _("Processor XML node has no type property") << endmsg;
			}
		}

		catch (failed_constructor &err) {
			warning << _("processor could not be created. Ignored.") << endmsg;
			return false;
		}
	}
	return false;
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
	
	_active = true;
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

	if ((prop = node.property (X_("meter-point"))) != 0) {
		_meter_point = MeterPoint (string_2_enum (prop->value (), _meter_point));
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

	delete deferred_state;
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
	bool has_meter_processor = false; // legacy sessions don't
			
	for (niter = nlist.begin(); niter != nlist.end(); ++niter){

		child = *niter;
			
		if (child->name() == X_("Send") || child->name() == X_("Processor")) {
			processor_nodes.push_back(child);
			if ((prop = child->property (X_("type"))) != 0 && prop->value() == "meter")  {
				has_meter_processor = true;
			}
		}

	}

	_set_processor_states(processor_nodes);
	if (!has_meter_processor) {
		set_meter_point(_meter_point, NULL);
	}
	processors_changed ();

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

			_control_outs = boost::shared_ptr<ControlOutputs> (
					new ControlOutputs (_session, new IO (_session, coutname)));
			
			/* fix up the control out name in the XML before setting it.
			   Otherwise track templates don't work because the control
			   outs end up with the stored template name, rather than
			   the new name of the track based on the template.
			*/
			
			XMLProperty* prop = (*child->children().begin())->property ("name");
			if (prop) {
				prop->set_value (coutname);
			}
			
			_control_outs->io()->set_state (**(child->children().begin()));
			_control_outs->set_sort_key (_meter->sort_key() + 1);
			add_processor (_control_outs, 0);

		} else if (child->name() == X_("Comment")) {

			/* XXX this is a terrible API design in libxml++ */

			XMLNode *cmt = *(child->children().begin());
			_comment = cmt->content();

		} else if (child->name() == X_("Extra")) {

			_extra_xml = new XMLNode (*child);

		} else if (child->name() == X_("Controllable") && (prop = child->property("name")) != 0) {
			
			if (prop->value() == "solo") {
				_solo_control->set_state (*child);
				_session.add_controllable (_solo_control);
			} else if (prop->value() == "mute") {
				_mute_control->set_state (*child);
				_session.add_controllable (_mute_control);
			}
		} else if (child->name() == X_("RemoteControl")) {
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
			XMLNode* ioproc_node = (*niter)->child(X_("IOProcessor"));
			if (ioproc_node && strncmp(buf, ioproc_node->child(X_("IO"))->property(X_("id"))->value().c_str(), sizeof(buf)) == 0) {
				processorInStateList = true;
				break;
			} else {
				XMLProperty* id_prop = (*niter)->property(X_("id"));
				if (id_prop && strncmp(buf, id_prop->value().c_str(), sizeof(buf)) == 0) {
					processorInStateList = true;
				}
				break;
			}
		}
		
		if (!processorInStateList) {
			remove_processor (*i);
		}

		i = tmp;
	}

	Placement placement = PreFader;

	// Iterate through state list and make sure all processors are on the track and in the correct order,
	// set the state of existing processors according to the new state on the same go
	i = _processors.begin();
	for (niter = nlist.begin(); niter != nlist.end(); ++niter, ++i) {

		// Check whether the next processor in the list 
		o = i;

		while (o != _processors.end()) {
			(*o)->id().print (buf, sizeof (buf));
			XMLNode* ioproc_node = (*niter)->child(X_("IOProcessor"));
			if (ioproc_node && strncmp(buf, ioproc_node->child(X_("IO"))->property(X_("id"))->value().c_str(), sizeof(buf)) == 0) {
				break;
			} else {
				XMLProperty* id_prop = (*niter)->property(X_("id"));
				if (id_prop && strncmp(buf, id_prop->value().c_str(), sizeof(buf)) == 0) {
					break;
				}
			}

			++o;
		}

		// If the processor (*niter) is not on the route,
		// create it and move it to the correct location
		if (o == _processors.end()) {
			if (add_processor_from_xml (**niter, &i)) {
				--i; // move iterator to the newly inserted processor
			} else {
				cerr << "Error restoring route: unable to restore processor" << endl;
			}

		// Otherwise, we found the processor (*niter) on the route,
		// ensure it is at the location provided in the XML state
		} else {

			if (i != o) {
				boost::shared_ptr<Processor> tmp = (*o);
				_processors.erase(o); // remove the old copy
				_processors.insert(i, tmp); // insert the processor at the correct location
				--i; // move iterator to the correct processor
			}

			(*i)->set_state((**niter));
		}

		if (*i == _amp) {
			placement = PostFader;
		}
	}
}

void
Route::curve_reallocate ()
{
//	_gain_automation_curve.finish_resize ();
//	_pan_automation_curve.finish_resize ();
}

void
Route::silence (nframes_t nframes)
{
	if (!_silent) {

		IO::silence (nframes);

		if (_control_outs) {
			_control_outs->io()->silence (nframes);
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

					(*i)->silence (nframes);
				}

				if (nframes == _session.get_block_size()) {
					// _silent = true;
				}
			}
		}
		
	}
}	

int
Route::set_control_outs (const vector<string>& ports)
{
	vector<string>::const_iterator i;
	
	if (is_control() || is_master()) {
		/* no control outs for these two special busses */
		return 0;
	}
 	
 	if (ports.empty()) {
 		return 0;
 	}
 
 	string coutname = _name;
 	coutname += _("[control]");
 	
	IO* out_io = new IO (_session, coutname);
	boost::shared_ptr<ControlOutputs> out_proc(new ControlOutputs (_session, out_io));

	/* As an IO, our control outs need as many IO outputs as we have outputs
	 *   (we track the changes in ::output_change_handler()).
	 * As a processor, the control outs is an identity processor
	 *   (i.e. it does not modify its input buffers whatsoever)
	 */
	if (out_io->ensure_io (ChanCount::ZERO, n_outputs(), true, this)) {
		return -1;
	}

	/* now connect to the named ports */
	
	for (size_t n = 0; n < n_outputs().n_total(); ++n) {
		if (out_io->connect_output (out_io->output (n), ports[n % ports.size()], this)) {
			error << string_compose (_("could not connect %1 to %2"),
					out_io->output(n)->name(), ports[n]) << endmsg;
			return -1;
		}
	}

	_control_outs = out_proc;
	_control_outs->set_sort_key (_meter->sort_key() + 1);
	add_processor (_control_outs, NULL);
 
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

		no = _control_outs->io()->n_outputs().n_total();
		
		for (i = 0; i < no; ++i) {
			for (j = 0; j < ni; ++j) {
				if (_control_outs->io()->output(i)->connected_to (other->input (j)->name())) {
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
Route::handle_transport_stopped (bool abort_ignored, bool did_locate, bool can_flush_processors)
{
	nframes_t now = _session.transport_frame();

	{
		Glib::RWLock::ReaderLock lm (_processor_lock);

		if (!did_locate) {
			automation_snapshot (now, true);
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
Route::input_change_handler (IOChange change, void *src)
{
	if ((change & ConfigurationChanged)) {
		configure_processors (0);
	}
}

void
Route::output_change_handler (IOChange change, void *src)
{
	if ((change & ConfigurationChanged)) {
		if (_control_outs) {
			_control_outs->io()->ensure_io (ChanCount::ZERO, n_outputs(), true, this);
		}
		
		configure_processors (0);
	}
}

uint32_t
Route::pans_required () const
{
	if (n_outputs().n_audio() < 2) {
		return 0;
	}
	
	return max (n_inputs ().n_audio(), processor_max_streams.n_audio());
}

int 
Route::no_roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame,  
		bool session_state_changing, bool can_record, bool rec_monitors_input)
{
	if (n_outputs().n_total() == 0) {
		return 0;
	}

	if (session_state_changing || !_active)  {
		silence (nframes);
		return 0;
	}

	_amp->apply_gain_automation(false);
	
	if (n_inputs() != ChanCount::ZERO) {
		passthru (start_frame, end_frame, nframes, 0);
	} else {
		silence (nframes);
	}

	return 0;
}

nframes_t
Route::check_initial_delay (nframes_t nframes, nframes_t& transport_frame)
{
	if (_roll_delay > nframes) {

		_roll_delay -= nframes;
		silence (nframes);
		/* transport frame is not legal for caller to use */
		return 0;

	} else if (_roll_delay > 0) {

		nframes -= _roll_delay;
		silence (_roll_delay);
		/* we've written _roll_delay of samples into the 
		   output ports, so make a note of that for
		   future reference.
		*/
		increment_output_offset (_roll_delay);
		transport_frame += _roll_delay;

		_roll_delay = 0;
	}

	return nframes;
}

int
Route::roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, int declick,
	     bool can_record, bool rec_monitors_input)
{
	{
		Glib::RWLock::ReaderLock lm (_processor_lock, Glib::TRY_LOCK);
		if (lm.locked()) {
			// automation snapshot can also be called from the non-rt context
			// and it uses the processor list, so we take the lock out here
			automation_snapshot (_session.transport_frame(), false);
		}
	}

	if ((n_outputs().n_total() == 0 && _processors.empty()) || n_inputs().n_total() == 0 || !_active) {
		silence (nframes);
		return 0;
	}
	
	nframes_t unused = 0;

	if ((nframes = check_initial_delay (nframes, unused)) == 0) {
		return 0;
	}

	_silent = false;

	_amp->apply_gain_automation(false);

	{ 
		Glib::Mutex::Lock am (data().control_lock(), Glib::TRY_LOCK);
		
		if (am.locked() && _session.transport_rolling()) {
			
			if (_gain_control->automation_playback()) {
				_amp->apply_gain_automation(
						_gain_control->list()->curve().rt_safe_get_vector (
							start_frame, end_frame, _session.gain_automation_buffer(), nframes));
			}
		}
	}

	passthru (start_frame, end_frame, nframes, declick);

	return 0;
}

int
Route::silent_roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, 
		    bool can_record, bool rec_monitors_input)
{
	silence (nframes);
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

		// Move meter in the processors list
		ProcessorList::iterator loc = find(_processors.begin(), _processors.end(), _meter);
		_processors.erase(loc);
		switch (p) {
		case MeterInput:
			loc = _processors.begin();
			break;
		case MeterPreFader:
			loc = find(_processors.begin(), _processors.end(), _amp);
			break;
		case MeterPostFader:
			loc = _processors.end();
			break;
		}
		_processors.insert(loc, _meter);
		
		// Update sort key
		if (loc == _processors.end()) {
			_meter->set_sort_key(_processors.size());
		} else {
			_meter->set_sort_key((*loc)->sort_key());
			for (ProcessorList::iterator p = loc; p != _processors.end(); ++p) {
				(*p)->set_sort_key((*p)->sort_key() + 1);
			}
		}

		 meter_change (src); /* EMIT SIGNAL */
		processors_changed (); /* EMIT SIGNAL */
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

#undef DEBUG_LATENCY
#ifdef DEBUG_LATENCY
	cerr << _name << ": internal redirect latency = " << _own_latency << endl;
#endif

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
	
#ifdef DEBUG_LATENCY
	cerr << _name << ": input latency = " << input_latency() << " total = "
	     << _own_latency << endl;
#endif

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
Route::automation_snapshot (nframes_t now, bool force)
{
	if (!force && !should_snapshot(now)) {
		return;
	}

	IO::automation_snapshot (now, force);

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->automation_snapshot (now, force);
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
	_session.ensure_buffers(processor_max_streams);
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

/** Shift automation forwards from a particular place, thereby inserting time.
 *  Adds undo commands for any shifts that are performed.
 *
 * @param pos Position to start shifting from.
 * @param frames Amount to shift forwards by.
 */

void
Route::shift (nframes64_t pos, nframes64_t frames)
{
#ifdef THIS_NEEDS_FIXING_FOR_V3

	/* gain automation */
	XMLNode &before = _gain_control->get_state ();
	_gain_control->shift (pos, frames);
	XMLNode &after = _gain_control->get_state ();
	_session.add_command (new MementoCommand<AutomationList> (_gain_automation_curve, &before, &after));

	/* pan automation */
	for (std::vector<StreamPanner*>::iterator i = _panner->begin (); i != _panner->end (); ++i) {
		Curve & c = (*i)->automation ();
		XMLNode &before = c.get_state ();
		c.shift (pos, frames);
		XMLNode &after = c.get_state ();
		_session.add_command (new MementoCommand<AutomationList> (c, &before, &after));
	}

	/* redirect automation */
	{
		Glib::RWLock::ReaderLock lm (redirect_lock);
		for (RedirectList::iterator i = _redirects.begin (); i != _redirects.end (); ++i) {
			
			set<uint32_t> a;
			(*i)->what_has_automation (a);
			
			for (set<uint32_t>::const_iterator j = a.begin (); j != a.end (); ++j) {
				AutomationList & al = (*i)->automation_list (*j);
				XMLNode &before = al.get_state ();
				al.shift (pos, frames);
				XMLNode &after = al.get_state ();
				_session.add_command (new MementoCommand<AutomationList> (al, &before, &after));
			}
		}
	}
#endif

}


int
Route::save_as_template (const string& path, const string& name)
{
	XMLNode& node (state (false));
	XMLTree tree;
	
	IO::set_name_in_state (*node.children().front(), name);
	
	tree.set_root (&node);
	return tree.write (path.c_str());
}
