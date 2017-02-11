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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <cmath>
#include <cassert>
#include <algorithm>

#include <glibmm.h>
#include <boost/algorithm/string.hpp>

#include "pbd/xml++.h"
#include "pbd/enumwriter.h"
#include "pbd/memento_command.h"
#include "pbd/stacktrace.h"
#include "pbd/convert.h"
#include "pbd/unwind.h"

#include "ardour/amp.h"
#include "ardour/audio_buffer.h"
#include "ardour/audio_track.h"
#include "ardour/audio_port.h"
#include "ardour/audioengine.h"
#include "ardour/boost_debug.h"
#include "ardour/buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/capturing_processor.h"
#include "ardour/debug.h"
#include "ardour/delivery.h"
#include "ardour/event_type_map.h"
#include "ardour/gain_control.h"
#include "ardour/internal_return.h"
#include "ardour/internal_send.h"
#include "ardour/meter.h"
#include "ardour/delayline.h"
#include "ardour/midi_buffer.h"
#include "ardour/midi_port.h"
#include "ardour/monitor_processor.h"
#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"
#include "ardour/parameter_descriptor.h"
#include "ardour/phase_control.h"
#include "ardour/plugin_insert.h"
#include "ardour/port.h"
#include "ardour/port_insert.h"
#include "ardour/processor.h"
#include "ardour/profile.h"
#include "ardour/route.h"
#include "ardour/route_group.h"
#include "ardour/send.h"
#include "ardour/session.h"
#include "ardour/solo_control.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/unknown_processor.h"
#include "ardour/utils.h"
#include "ardour/vca.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PBD::Signal3<int,boost::shared_ptr<Route>, boost::shared_ptr<PluginInsert>, Route::PluginSetupOptions > Route::PluginSetup;

/** Base class for all routable/mixable objects (tracks and busses) */
Route::Route (Session& sess, string name, PresentationInfo::Flag flag, DataType default_type)
	: Stripable (sess, name, PresentationInfo (flag))
	, GraphNode (sess._process_graph)
	, Muteable (sess, name)
	, Automatable (sess)
	, _active (true)
	, _signal_latency (0)
	, _signal_latency_at_amp_position (0)
	, _signal_latency_at_trim_position (0)
	, _initial_delay (0)
	, _roll_delay (0)
	, _pending_process_reorder (0)
	, _pending_signals (0)
	, _pending_declick (true)
	, _meter_point (MeterPostFader)
	, _pending_meter_point (MeterPostFader)
	, _meter_type (MeterPeak)
	, _denormal_protection (false)
	, _recordable (true)
	, _silent (false)
	, _declickable (false)
	, _have_internal_generator (false)
	, _default_type (default_type)
	, _track_number (0)
	, _in_configure_processors (false)
	, _initial_io_setup (false)
	, _in_sidechain_setup (false)
	, _strict_io (false)
	, _custom_meter_position_noted (false)
	, _pinmgr_proxy (0)
{
	processor_max_streams.reset();
}

boost::weak_ptr<Route>
Route::weakroute () {
	return boost::weak_ptr<Route> (shared_from_this ());
}

int
Route::init ()
{
	/* set default meter type */
	if (is_master()) {
		_meter_type = Config->get_meter_type_master ();
	}
	else if (dynamic_cast<Track*>(this)) {
		_meter_type = Config->get_meter_type_track ();
	} else {
		_meter_type = Config->get_meter_type_bus ();
	}

	/* add standard controls */

	_gain_control.reset (new GainControl (_session, GainAutomation));
	add_control (_gain_control);

	_trim_control.reset (new GainControl (_session, TrimAutomation));
	add_control (_trim_control);

	_solo_control.reset (new SoloControl (_session, X_("solo"), *this, *this));
	add_control (_solo_control);
	_solo_control->Changed.connect_same_thread (*this, boost::bind (&Route::solo_control_changed, this, _1, _2));

	_mute_control.reset (new MuteControl (_session, X_("mute"), *this));
	add_control (_mute_control);

	_phase_control.reset (new PhaseControl (_session, X_("phase")));
	add_control (_phase_control);

	_solo_isolate_control.reset (new SoloIsolateControl (_session, X_("solo-iso"), *this, *this));
	add_control (_solo_isolate_control);

	_solo_safe_control.reset (new SoloSafeControl (_session, X_("solo-safe")));
	add_control (_solo_safe_control);

	/* panning */

	if (!(_presentation_info.flags() & PresentationInfo::MonitorOut)) {
		_pannable.reset (new Pannable (_session));
	}

	/* input and output objects */

	_input.reset (new IO (_session, _name, IO::Input, _default_type));
	_output.reset (new IO (_session, _name, IO::Output, _default_type));

	_input->changed.connect_same_thread (*this, boost::bind (&Route::input_change_handler, this, _1, _2));
	_input->PortCountChanging.connect_same_thread (*this, boost::bind (&Route::input_port_count_changing, this, _1));

	_output->changed.connect_same_thread (*this, boost::bind (&Route::output_change_handler, this, _1, _2));
	_output->PortCountChanging.connect_same_thread (*this, boost::bind (&Route::output_port_count_changing, this, _1));

	/* add the amp/fader processor.
	 * it should be the first processor to be added on every route.
	 */

	_amp.reset (new Amp (_session, X_("Fader"), _gain_control, true));
	add_processor (_amp, PostFader);

	if (is_monitor ()) {
		_amp->set_display_name (_("Monitor"));
	}

#if 0 // not used - just yet
	if (!is_master() && !is_monitor() && !is_auditioner()) {
		_delayline.reset (new DelayLine (_session, _name));
		add_processor (_delayline, PreFader);
	}
#endif

	/* and input trim */

	_trim.reset (new Amp (_session, X_("Trim"), _trim_control, false));
	_trim->set_display_to_user (false);

	if (dynamic_cast<AudioTrack*>(this)) {
		/* we can't do this in the AudioTrack's constructor
		 * because _trim does not exit then
		 */
		_trim->activate();
	}
	else if (!dynamic_cast<Track*>(this) && ! ( is_monitor() || is_auditioner() )) {
		/* regular bus */
		_trim->activate();
	}

	/* create standard processors: meter, main outs, monitor out;
	   they will be added to _processors by setup_invisible_processors ()
	*/

	_meter.reset (new PeakMeter (_session, _name));
	_meter->set_owner (this);
	_meter->set_display_to_user (false);
	_meter->activate ();

	_main_outs.reset (new Delivery (_session, _output, _pannable, _mute_master, _name, Delivery::Main));
	_main_outs->activate ();

	if (is_monitor()) {
		/* where we listen to tracks */
		_intreturn.reset (new InternalReturn (_session));
		_intreturn->activate ();

		/* the thing that provides proper control over a control/monitor/listen bus
		   (such as per-channel cut, dim, solo, invert, etc).
		*/
		_monitor_control.reset (new MonitorProcessor (_session));
		_monitor_control->activate ();
	}

	/* now that we have _meter, its safe to connect to this */

	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		configure_processors (0);
	}

	return 0;
}

Route::~Route ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("route %1 destructor\n", _name));

	/* do this early so that we don't get incoming signals as we are going through destruction
	 */

	drop_connections ();

	/* don't use clear_processors here, as it depends on the session which may
	   be half-destroyed by now
	*/

	Glib::Threads::RWLock::WriterLock lm (_processor_lock);
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->drop_references ();
	}

	_processors.clear ();
}

string
Route::ensure_track_or_route_name(string name, Session &session)
{
	string newname = name;

	while (!session.io_name_is_legal (newname)) {
		newname = bump_name_once (newname, ' ');
	}

	return newname;
}

void
Route::set_trim (gain_t val, Controllable::GroupControlDisposition /* group override */)
{
	// TODO route group, see set_gain()
	// _trim_control->route_set_value (val);
}

void
Route::maybe_declick (BufferSet&, framecnt_t, int)
{
	/* this is the "bus" implementation and they never declick.
	 */
	return;
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
			       framepos_t start_frame, framepos_t end_frame, pframes_t nframes,
			       int declick, bool gain_automation_ok)
{
	/* Caller must hold process lock */
	assert (!AudioEngine::instance()->process_lock().trylock());

	Glib::Threads::RWLock::ReaderLock lm (_processor_lock, Glib::Threads::TRY_LOCK);
	if (!lm.locked()) {
		// can this actually happen? functions calling process_output_buffers()
		// already take a reader-lock.
		bufs.silence (nframes, 0);
		return;
	}

	_mute_control->automation_run (start_frame, nframes);

	/* figure out if we're going to use gain automation */
	if (gain_automation_ok) {
		_amp->set_gain_automation_buffer (_session.gain_automation_buffer ());
		_amp->setup_gain_automation (
				start_frame + _signal_latency_at_amp_position,
				end_frame + _signal_latency_at_amp_position,
				nframes);

		_trim->set_gain_automation_buffer (_session.trim_automation_buffer ());
		_trim->setup_gain_automation (
				start_frame + _signal_latency_at_trim_position,
				end_frame + _signal_latency_at_trim_position,
				nframes);
	} else {
		_amp->apply_gain_automation (false);
		_trim->apply_gain_automation (false);
	}

	/* Tell main outs what to do about monitoring.  We do this so that
	   on a transition between monitoring states we get a de-clicking gain
	   change in the _main_outs delivery, if config.get_use_monitor_fades()
	   is true.

	   We override this in the case where we have an internal generator.
	*/
	bool silence = _have_internal_generator ? false : (monitoring_state () == MonitoringSilence);

	_main_outs->no_outs_cuz_we_no_monitor (silence);

	/* -------------------------------------------------------------------------------------------
	   GLOBAL DECLICK (for transport changes etc.)
	   ----------------------------------------------------------------------------------------- */

	maybe_declick (bufs, nframes, declick);
	_pending_declick = 0;

	/* -------------------------------------------------------------------------------------------
	   DENORMAL CONTROL/PHASE INVERT
	   ----------------------------------------------------------------------------------------- */

	if (!_phase_control->none()) {

		int chn = 0;

		if (_denormal_protection || Config->get_denormal_protection()) {

			for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i, ++chn) {
				Sample* const sp = i->data();

				if (_phase_control->inverted (chn)) {
					for (pframes_t nx = 0; nx < nframes; ++nx) {
						sp[nx]  = -sp[nx];
						sp[nx] += 1.0e-27f;
					}
				} else {
					for (pframes_t nx = 0; nx < nframes; ++nx) {
						sp[nx] += 1.0e-27f;
					}
				}
			}

		} else {

			for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i, ++chn) {
				Sample* const sp = i->data();

				if (_phase_control->inverted (chn)) {
					for (pframes_t nx = 0; nx < nframes; ++nx) {
						sp[nx] = -sp[nx];
					}
				}
			}
		}

	} else {

		if (_denormal_protection || Config->get_denormal_protection()) {

			for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
				Sample* const sp = i->data();
				for (pframes_t nx = 0; nx < nframes; ++nx) {
					sp[nx] += 1.0e-27f;
				}
			}

		}
	}

	/* -------------------------------------------------------------------------------------------
	   and go ....
	   ----------------------------------------------------------------------------------------- */

	/* set this to be true if the meter will already have been ::run() earlier */
	bool const meter_already_run = metering_state() == MeteringInput;

	framecnt_t latency = 0;
	const double speed = _session.transport_speed ();

	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {

		if (meter_already_run && boost::dynamic_pointer_cast<PeakMeter> (*i)) {
			/* don't ::run() the meter, otherwise it will have its previous peak corrupted */
			continue;
		}

#ifndef NDEBUG
		/* if it has any inputs, make sure they match */
		if (boost::dynamic_pointer_cast<UnknownProcessor> (*i) == 0 && (*i)->input_streams() != ChanCount::ZERO) {
			if (bufs.count() != (*i)->input_streams()) {
				DEBUG_TRACE (
					DEBUG::Processors, string_compose (
						"input port mismatch %1 bufs = %2 input for %3 = %4\n",
						_name, bufs.count(), (*i)->name(), (*i)->input_streams()
						)
					);
			}
		}
#endif

		/* should we NOT run plugins here if the route is inactive?
		   do we catch route != active somewhere higher?
		*/

		if (boost::dynamic_pointer_cast<Send>(*i) != 0) {
			boost::dynamic_pointer_cast<Send>(*i)->set_delay_in(_signal_latency - latency);
		}
		if (boost::dynamic_pointer_cast<PluginInsert>(*i) != 0) {
			const framecnt_t longest_session_latency = _initial_delay + _signal_latency;
			boost::dynamic_pointer_cast<PluginInsert>(*i)->set_sidechain_latency (
					_initial_delay + latency, longest_session_latency - latency);
		}

		(*i)->run (bufs, start_frame - latency, end_frame - latency, speed, nframes, *i != _processors.back());
		bufs.set_count ((*i)->output_streams());

		if ((*i)->active ()) {
			latency += (*i)->signal_latency ();
		}
	}
}

void
Route::bounce_process (BufferSet& buffers, framepos_t start, framecnt_t nframes,
		boost::shared_ptr<Processor> endpoint,
		bool include_endpoint, bool for_export, bool for_freeze)
{
	/* If no processing is required, there's no need to go any further. */
	if (!endpoint && !include_endpoint) {
		return;
	}

	framecnt_t latency = bounce_get_latency(_amp, false, for_export, for_freeze);
	_amp->set_gain_automation_buffer (_session.gain_automation_buffer ());
	_amp->setup_gain_automation (start - latency, start - latency + nframes, nframes);

	/* trim is always at the top, for bounce no latency compensation is needed */
	_trim->set_gain_automation_buffer (_session.trim_automation_buffer ());
	_trim->setup_gain_automation (start, start + nframes, nframes);

	latency = 0;
	const double speed = _session.transport_speed ();
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {

		if (!include_endpoint && (*i) == endpoint) {
			break;
		}

		/* if we're *not* exporting, stop processing if we come across a routing processor. */
		if (!for_export && boost::dynamic_pointer_cast<PortInsert>(*i)) {
			break;
		}
		if (!for_export && for_freeze && (*i)->does_routing() && (*i)->active()) {
			break;
		}

		/* special case the panner (export outputs)
		 * Ideally we'd only run the panner, not the delivery itself...
		 * but panners need separate input/output buffers and some context
		 * (panshell, panner type, etc). AFAICT there is no ill side effect
		 * of re-using the main delivery when freewheeling/exporting a region.
		 */
		if ((*i) == _main_outs) {
			assert ((*i)->does_routing());
			(*i)->run (buffers, start - latency, start - latency + nframes, speed, nframes, true);
			buffers.set_count ((*i)->output_streams());
		}

		/* don't run any processors that do routing.
		 * Also don't bother with metering.
		 */
		if (!(*i)->does_routing() && !boost::dynamic_pointer_cast<PeakMeter>(*i)) {
			(*i)->run (buffers, start - latency, start - latency + nframes, 1.0, nframes, true);
			buffers.set_count ((*i)->output_streams());
			latency += (*i)->signal_latency ();
		}

		if ((*i) == endpoint) {
			break;
		}
	}
}

framecnt_t
Route::bounce_get_latency (boost::shared_ptr<Processor> endpoint,
		bool include_endpoint, bool for_export, bool for_freeze) const
{
	framecnt_t latency = 0;
	if (!endpoint && !include_endpoint) {
		return latency;
	}

	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if (!include_endpoint && (*i) == endpoint) {
			break;
		}
		if (!for_export && boost::dynamic_pointer_cast<PortInsert>(*i)) {
			break;
		}
		if (!for_export && for_freeze && (*i)->does_routing() && (*i)->active()) {
			break;
		}
		if (!(*i)->does_routing() && !boost::dynamic_pointer_cast<PeakMeter>(*i)) {
			latency += (*i)->signal_latency ();
		}
		if ((*i) == endpoint) {
			break;
		}
	}
	return latency;
}

ChanCount
Route::bounce_get_output_streams (ChanCount &cc, boost::shared_ptr<Processor> endpoint,
		bool include_endpoint, bool for_export, bool for_freeze) const
{
	if (!endpoint && !include_endpoint) {
		return cc;
	}

	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if (!include_endpoint && (*i) == endpoint) {
			break;
		}
		if (!for_export && boost::dynamic_pointer_cast<PortInsert>(*i)) {
			break;
		}
		if (!for_export && for_freeze && (*i)->does_routing() && (*i)->active()) {
			break;
		}
		if (!(*i)->does_routing() && !boost::dynamic_pointer_cast<PeakMeter>(*i)) {
			cc = (*i)->output_streams();
		}
		if ((*i) == endpoint) {
			break;
		}
	}
	return cc;
}

ChanCount
Route::n_process_buffers ()
{
	return max (_input->n_ports(), processor_max_streams);
}

void
Route::monitor_run (framepos_t start_frame, framepos_t end_frame, pframes_t nframes, int declick)
{
	assert (is_monitor());
	BufferSet& bufs (_session.get_route_buffers (n_process_buffers()));
	fill_buffers_with_input (bufs, _input, nframes);
	passthru (bufs, start_frame, end_frame, nframes, declick);
}

void
Route::passthru (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, pframes_t nframes, int declick)
{
	_silent = false;

	if (is_monitor() && _session.listening() && !_session.is_auditioning()) {

		/* control/monitor bus ignores input ports when something is
		   feeding the listen "stream". data will "arrive" into the
		   route from the intreturn processor element.
		*/

		bufs.silence (nframes, 0);
	}

	write_out_of_band_data (bufs, start_frame, end_frame, nframes);
	process_output_buffers (bufs, start_frame, end_frame, nframes, declick, true);
}

void
Route::passthru_silence (framepos_t start_frame, framepos_t end_frame, pframes_t nframes, int declick)
{
	BufferSet& bufs (_session.get_route_buffers (n_process_buffers(), true));

	bufs.set_count (_input->n_ports());
	write_out_of_band_data (bufs, start_frame, end_frame, nframes);
	process_output_buffers (bufs, start_frame, end_frame, nframes, declick, false);
}

void
Route::set_listen (bool yn)
{
	if (_monitor_send) {
		if (_monitor_send->active() == yn) {
			return;
		}
		if (yn) {
			_monitor_send->activate ();
		} else {
			_monitor_send->deactivate ();
		}
	}
}

void
Route::solo_control_changed (bool, Controllable::GroupControlDisposition)
{
	/* nothing to do if we're not using AFL/PFL. But if we are, we need
	   to alter the active state of the monitor send.
	*/

	if (Config->get_solo_control_is_listen_control ()) {
		set_listen (_solo_control->self_soloed() || _solo_control->get_masters_value());
	}
}

void
Route::push_solo_isolate_upstream (int32_t delta)
{
	/* forward propagate solo-isolate status to everything fed by this route, but not those via sends only */

	boost::shared_ptr<RouteList> routes = _session.get_routes ();
	for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {

		if ((*i).get() == this || !(*i)->can_solo()) {
			continue;
		}

		bool sends_only;
		bool does_feed = feeds (*i, &sends_only);

		if (does_feed && !sends_only) {
			(*i)->solo_isolate_control()->mod_solo_isolated_by_upstream (delta);
		}
	}
}

void
Route::push_solo_upstream (int delta)
{
	DEBUG_TRACE (DEBUG::Solo, string_compose("\t ... INVERT push from %1\n", _name));
	for (FedBy::iterator i = _fed_by.begin(); i != _fed_by.end(); ++i) {
		if (i->sends_only) {
			continue;
		}
		boost::shared_ptr<Route> sr (i->r.lock());
		if (sr) {
			sr->solo_control()->mod_solo_by_others_downstream (-delta);
		}
	}
}

#if 0
static void
dump_processors(const string& name, const list<boost::shared_ptr<Processor> >& procs)
{
	cerr << name << " {" << endl;
	for (list<boost::shared_ptr<Processor> >::const_iterator p = procs.begin();
			p != procs.end(); ++p) {
		cerr << "\t" << (*p)->name() << " ID = " << (*p)->id() << " @ " << (*p) << endl;
	}
	cerr << "}" << endl;
}
#endif

/** Supposing that we want to insert a Processor at a given Placement, return
 *  the processor to add the new one before (or 0 to add at the end).
 */
boost::shared_ptr<Processor>
Route::before_processor_for_placement (Placement p)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	ProcessorList::iterator loc;

	if (p == PreFader) {
		/* generic pre-fader: insert immediately before the amp */
		loc = find (_processors.begin(), _processors.end(), _amp);
	} else {
		/* generic post-fader: insert right before the main outs */
		loc = find (_processors.begin(), _processors.end(), _main_outs);
	}

	return loc != _processors.end() ? *loc : boost::shared_ptr<Processor> ();
}

/** Supposing that we want to insert a Processor at a given index, return
 *  the processor to add the new one before (or 0 to add at the end).
 */
boost::shared_ptr<Processor>
Route::before_processor_for_index (int index)
{
	if (index == -1) {
		return boost::shared_ptr<Processor> ();
	}

	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	ProcessorList::iterator i = _processors.begin ();
	int j = 0;
	while (i != _processors.end() && j < index) {
		if ((*i)->display_to_user()) {
			++j;
		}

		++i;
	}

	return (i != _processors.end() ? *i : boost::shared_ptr<Processor> ());
}

/** Add a processor either pre- or post-fader
 *  @return 0 on success, non-0 on failure.
 */
int
Route::add_processor (boost::shared_ptr<Processor> processor, Placement placement, ProcessorStreams* err, bool activation_allowed)
{
	return add_processor (processor, before_processor_for_placement (placement), err, activation_allowed);
}


/** Add a processor to a route such that it ends up with a given index into the visible processors.
 *  @param index Index to add the processor at, or -1 to add at the end of the list.
 *  @return 0 on success, non-0 on failure.
 */
int
Route::add_processor_by_index (boost::shared_ptr<Processor> processor, int index, ProcessorStreams* err, bool activation_allowed)
{
	return add_processor (processor, before_processor_for_index (index), err, activation_allowed);
}

/** Add a processor to the route.
 *  @param before An existing processor in the list, or 0; the new processor will be inserted immediately before it (or at the end).
 *  @return 0 on success, non-0 on failure.
 */
int
Route::add_processor (boost::shared_ptr<Processor> processor, boost::shared_ptr<Processor> before, ProcessorStreams* err, bool activation_allowed)
{
	assert (processor != _meter);
	assert (processor != _main_outs);

	DEBUG_TRACE (DEBUG::Processors, string_compose (
		             "%1 adding processor %2\n", name(), processor->name()));

	ProcessorList pl;

	pl.push_back (processor);
	int rv = add_processors (pl, before, err);

	if (rv) {
		return rv;
	}

	if (activation_allowed && (!_session.get_bypass_all_loaded_plugins () || !processor->display_to_user ())) {
		processor->activate ();
	}

	return 0;
}

void
Route::processor_selfdestruct (boost::weak_ptr<Processor> wp)
{
	/* We cannot destruct the processor here (usually RT-thread
	 * with various locks held - in case of sends also io_locks).
	 * Queue for deletion in low-priority thread.
	 */
	Glib::Threads::Mutex::Lock lx (selfdestruct_lock);
	selfdestruct_sequence.push_back (wp);
}

bool
Route::add_processor_from_xml_2X (const XMLNode& node, int version)
{
	XMLProperty const * prop;

	try {
		boost::shared_ptr<Processor> processor;

		/* bit of a hack: get the `placement' property from the <Redirect> tag here
		   so that we can add the processor in the right place (pre/post-fader)
		*/

		XMLNodeList const & children = node.children ();
		XMLNodeList::const_iterator i = children.begin ();

		while (i != children.end() && (*i)->name() != X_("Redirect")) {
			++i;
		}

		Placement placement = PreFader;

		if (i != children.end()) {
			if ((prop = (*i)->property (X_("placement"))) != 0) {
				placement = Placement (string_2_enum (prop->value(), placement));
			}
		}

		if (node.name() == "Insert") {

			if ((prop = node.property ("type")) != 0) {

				if (prop->value() == "ladspa" || prop->value() == "Ladspa" ||
						prop->value() == "lv2" ||
						prop->value() == "windows-vst" ||
						prop->value() == "mac-vst" ||
						prop->value() == "lxvst" ||
						prop->value() == "audiounit") {

					if (_session.get_disable_all_loaded_plugins ()) {
						processor.reset (new UnknownProcessor (_session, node));
					} else {
						processor.reset (new PluginInsert (_session));
						processor->set_owner (this);
					}

				} else {

					processor.reset (new PortInsert (_session, _pannable, _mute_master));
				}

			}

		} else if (node.name() == "Send") {

			boost::shared_ptr<Pannable> sendpan (new Pannable (_session));
			processor.reset (new Send (_session, sendpan, _mute_master));

		} else {

			error << string_compose(_("unknown Processor type \"%1\"; ignored"), node.name()) << endmsg;
			return false;
		}

		if (processor->set_state (node, version)) {
			return false;
		}

		//A2 uses the "active" flag in the toplevel redirect node, not in the child plugin/IO
		if (i != children.end()) {
			if ((prop = (*i)->property (X_("active"))) != 0) {
				if ( string_is_affirmative (prop->value()) && (!_session.get_bypass_all_loaded_plugins () || !processor->display_to_user () ) )
					processor->activate();
				else
					processor->deactivate();
			}
		}

		return (add_processor (processor, placement, 0, false) == 0);
	}

	catch (failed_constructor &err) {
		warning << _("processor could not be created. Ignored.") << endmsg;
		return false;
	}
}


inline Route::PluginSetupOptions operator|= (Route::PluginSetupOptions& a, const Route::PluginSetupOptions& b) {
	return a = static_cast<Route::PluginSetupOptions> (static_cast <int>(a) | static_cast<int> (b));
}

inline Route::PluginSetupOptions operator&= (Route::PluginSetupOptions& a, const Route::PluginSetupOptions& b) {
	return a = static_cast<Route::PluginSetupOptions> (static_cast <int>(a) & static_cast<int> (b));
}

int
Route::add_processors (const ProcessorList& others, boost::shared_ptr<Processor> before, ProcessorStreams* err)
{
	ProcessorList::iterator loc;
	boost::shared_ptr <PluginInsert> fanout;

	if (before) {
		loc = find(_processors.begin(), _processors.end(), before);
		if (loc == _processors.end ()) {
			return 1;
		}
	} else {
		/* nothing specified - at end */
		loc = _processors.end ();
	}

	if (!AudioEngine::instance()->connected()) {
		return 1;
	}

	if (others.empty()) {
		return 0;
	}

	ProcessorList to_skip;

	// check if there's an instrument to replace or configure
	for (ProcessorList::const_iterator i = others.begin(); i != others.end(); ++i) {
		boost::shared_ptr<PluginInsert> pi;
		if ((pi = boost::dynamic_pointer_cast<PluginInsert>(*i)) == 0) {
			continue;
		}
		if (!pi->plugin ()->get_info ()->is_instrument ()) {
			continue;
		}
		boost::shared_ptr<Processor> instrument = the_instrument ();
		ChanCount in (DataType::MIDI, 1);
		ChanCount out (DataType::AUDIO, 2); // XXX route's out?!

		PluginSetupOptions flags = None;
		if (instrument) {
			flags |= CanReplace;
			in = instrument->input_streams ();
			out = instrument->output_streams ();
		}
		if (pi->has_output_presets (in, out)) {
			flags |= MultiOut;
		}

		pi->set_strict_io (_strict_io);

		PluginSetupOptions mask = None;
		if (Config->get_ask_replace_instrument ()) {
			mask |= CanReplace;
		}
		if (Config->get_ask_setup_instrument ()) {
			mask |= MultiOut;
		}

		flags &= mask;

		if (flags != None) {
			boost::optional<int> rv = PluginSetup (shared_from_this (), pi, flags);  /* EMIT SIGNAL */
			int mode = rv.get_value_or (0);
			switch (mode & 3) {
				case 1:
					to_skip.push_back (*i); // don't add this one;
					break;
				case 2:
					replace_processor (instrument, *i, err);
					to_skip.push_back (*i);
					break;
				default:
					break;
			}
			if ((mode & 5) == 4) {
				fanout = pi;
			}
		}
	}

	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);
		ProcessorState pstate (this);

		for (ProcessorList::const_iterator i = others.begin(); i != others.end(); ++i) {

			if (*i == _meter) {
				continue;
			}
			ProcessorList::iterator check = find (to_skip.begin(), to_skip.end(), *i);
			if (check != to_skip.end()) {
				continue;
			}

			boost::shared_ptr<PluginInsert> pi;

			if ((pi = boost::dynamic_pointer_cast<PluginInsert>(*i)) != 0) {
				pi->set_strict_io (_strict_io);
			}

			if (*i == _amp) {
				/* Ensure that only one amp is in the list at any time */
				ProcessorList::iterator check = find (_processors.begin(), _processors.end(), *i);
				if (check != _processors.end()) {
					if (before == _amp) {
						/* Already in position; all is well */
						continue;
					} else {
						_processors.erase (check);
					}
				}
			}

			assert (find (_processors.begin(), _processors.end(), *i) == _processors.end ());

			_processors.insert (loc, *i);
			(*i)->set_owner (this);

			{
				if (configure_processors_unlocked (err, &lm)) {
					pstate.restore ();
					configure_processors_unlocked (0, &lm); // it worked before we tried to add it ...
					return -1;
				}
			}

			if (pi && pi->has_sidechain ()) {
				pi->sidechain_input ()->changed.connect_same_thread (*this, boost::bind (&Route::sidechain_change_handler, this, _1, _2));
			}

			if ((*i)->active()) {
				// emit ActiveChanged() and latency_changed() if needed
				(*i)->activate ();
			}

			(*i)->ActiveChanged.connect_same_thread (*this, boost::bind (&Session::update_latency_compensation, &_session, false));

			boost::shared_ptr<Send> send;
			if ((send = boost::dynamic_pointer_cast<Send> (*i))) {
				send->SelfDestruct.connect_same_thread (*this,
						boost::bind (&Route::processor_selfdestruct, this, boost::weak_ptr<Processor> (*i)));
			}
		}

		for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
			boost::shared_ptr<PluginInsert> pi;

			if ((pi = boost::dynamic_pointer_cast<PluginInsert>(*i)) != 0) {
				if (pi->has_no_inputs ()) {
					_have_internal_generator = true;
					break;
				}
			}
		}

		_output->set_user_latency (0);
	}

	reset_instrument_info ();
	processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
	set_processor_positions ();

	if (fanout && fanout->configured ()
			&& fanout->output_streams().n_audio() > 2
			&& boost::dynamic_pointer_cast<PluginInsert> (the_instrument ()) == fanout) {
		fan_out (); /* EMIT SIGNAL */
	}
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
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	ProcessorList::iterator start, end;
	placement_range(p, start, end);

	for (ProcessorList::iterator i = start; i != end; ++i) {
		(*i)->enable (false);
	}

	_session.set_dirty ();
}

/** Turn off all redirects
 */
void
Route::disable_processors ()
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->enable (false);
	}

	_session.set_dirty ();
}

/** Turn off all redirects with a given placement
 * @param p Placement of redirects to disable
 */
void
Route::disable_plugins (Placement p)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	ProcessorList::iterator start, end;
	placement_range(p, start, end);

	for (ProcessorList::iterator i = start; i != end; ++i) {
		if (boost::dynamic_pointer_cast<PluginInsert> (*i)) {
			(*i)->enable (false);
		}
	}

	_session.set_dirty ();
}

/** Turn off all plugins
 */
void
Route::disable_plugins ()
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if (boost::dynamic_pointer_cast<PluginInsert> (*i)) {
			(*i)->enable (false);
		}
	}

	_session.set_dirty ();
}


void
Route::ab_plugins (bool forward)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	if (forward) {

		/* forward = turn off all active redirects, and mark them so that the next time
		   we go the other way, we will revert them
		*/

		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
			if (!boost::dynamic_pointer_cast<PluginInsert> (*i)) {
				continue;
			}

			if ((*i)->enabled ()) {
				(*i)->enable (false);
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

			(*i)->enable ((*i)->get_next_ab_is_active ());
		}
	}

	_session.set_dirty ();
}


/** Remove processors with a given placement.
 * @param p Placement of processors to remove.
 */
void
Route::clear_processors (Placement p)
{
	if (!_session.engine().connected()) {
		return;
	}

	bool already_deleting = _session.deletion_in_progress();
	if (!already_deleting) {
		_session.set_deletion_in_progress();
	}

	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);
		ProcessorList new_list;
		ProcessorStreams err;
		bool seen_amp = false;

		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {

			if (*i == _amp) {
				seen_amp = true;
			}

			if ((*i) == _amp || (*i) == _meter || (*i) == _main_outs || (*i) == _delayline || (*i) == _trim) {

				/* you can't remove these */

				new_list.push_back (*i);

			} else {
				if (seen_amp) {

					switch (p) {
					case PreFader:
						new_list.push_back (*i);
						break;
					case PostFader:
						(*i)->drop_references ();
						break;
					}

				} else {

					switch (p) {
					case PreFader:
						(*i)->drop_references ();
						break;
					case PostFader:
						new_list.push_back (*i);
						break;
					}
				}
			}
		}

		_processors = new_list;
		configure_processors_unlocked (&err, &lm); // this can't fail
	}

	processor_max_streams.reset();
	_have_internal_generator = false;
	processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
	set_processor_positions ();

	reset_instrument_info ();

	if (!already_deleting) {
		_session.clear_deletion_in_progress();
	}
}

int
Route::remove_processor (boost::shared_ptr<Processor> processor, ProcessorStreams* err, bool need_process_lock)
{
	// TODO once the export point can be configured properly, do something smarter here
	if (processor == _capturing_processor) {
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock (), Glib::Threads::NOT_LOCK);
		if (need_process_lock) {
			lx.acquire();
		}

		_capturing_processor.reset();

		if (need_process_lock) {
			lx.release();
		}
	}

	/* these can never be removed */

	if (processor == _amp || processor == _meter || processor == _main_outs || processor == _delayline || processor == _trim) {
		return 0;
	}

	if (!_session.engine().connected()) {
		return 1;
	}

	processor_max_streams.reset();

	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock (), Glib::Threads::NOT_LOCK);
		if (need_process_lock) {
			lx.acquire();
		}

		/* Caller must hold process lock */
		assert (!AudioEngine::instance()->process_lock().trylock());

		Glib::Threads::RWLock::WriterLock lm (_processor_lock); // XXX deadlock after export

		ProcessorState pstate (this);

		ProcessorList::iterator i;
		bool removed = false;

		for (i = _processors.begin(); i != _processors.end(); ) {
			if (*i == processor) {

				/* move along, see failure case for configure_processors()
				   where we may need to reconfigure the processor.
				*/

				/* stop redirects that send signals to JACK ports
				   from causing noise as a result of no longer being
				   run.
				*/

				boost::shared_ptr<IOProcessor> iop = boost::dynamic_pointer_cast<IOProcessor> (*i);
				boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert>(*i);

				if (pi != 0) {
					assert (iop == 0);
					iop = pi->sidechain();
				}

				if (iop != 0) {
					iop->disconnect ();
				}

				i = _processors.erase (i);
				removed = true;
				break;

			} else {
				++i;
			}

			_output->set_user_latency (0);
		}

		if (!removed) {
			/* what? */
			return 1;
		}

		if (configure_processors_unlocked (err, &lm)) {
			pstate.restore ();
			/* we know this will work, because it worked before :) */
			configure_processors_unlocked (0, &lm);
			return -1;
		}

		_have_internal_generator = false;

		for (i = _processors.begin(); i != _processors.end(); ++i) {
			boost::shared_ptr<PluginInsert> pi;

			if ((pi = boost::dynamic_pointer_cast<PluginInsert>(*i)) != 0) {
				if (pi->has_no_inputs ()) {
					_have_internal_generator = true;
					break;
				}
			}
		}
		if (need_process_lock) {
			lx.release();
		}
	}

	reset_instrument_info ();
	processor->drop_references ();
	processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
	set_processor_positions ();

	return 0;
}

int
Route::replace_processor (boost::shared_ptr<Processor> old, boost::shared_ptr<Processor> sub, ProcessorStreams* err)
{
	/* these can never be removed */
	if (old == _amp || old == _meter || old == _main_outs || old == _delayline || old == _trim) {
		return 1;
	}
	/* and can't be used as substitute, either */
	if (sub == _amp || sub == _meter || sub == _main_outs || sub == _delayline || sub == _trim) {
		return 1;
	}

	/* I/Os are out, too */
	if (boost::dynamic_pointer_cast<IOProcessor> (old) || boost::dynamic_pointer_cast<IOProcessor> (sub)) {
		return 1;
	}

	/* this function cannot be used to swap/reorder processors */
	if (find (_processors.begin(), _processors.end(), sub) != _processors.end ()) {
		return 1;
	}

	if (!AudioEngine::instance()->connected() || !old || !sub) {
		return 1;
	}

	/* ensure that sub is not owned by another route */
	if (sub->owner ()) {
		return 1;
	}

	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);
		ProcessorState pstate (this);

		assert (find (_processors.begin(), _processors.end(), sub) == _processors.end ());

		ProcessorList::iterator i;
		bool replaced = false;
		bool enable = old->enabled ();

		for (i = _processors.begin(); i != _processors.end(); ) {
			if (*i == old) {
				i = _processors.erase (i);
				_processors.insert (i, sub);
				sub->set_owner (this);
				replaced = true;
				break;
			} else {
				++i;
			}
		}

		if (!replaced) {
			return 1;
		}

		if (_strict_io) {
			boost::shared_ptr<PluginInsert> pi;
			if ((pi = boost::dynamic_pointer_cast<PluginInsert>(sub)) != 0) {
				pi->set_strict_io (true);
			}
		}

		if (configure_processors_unlocked (err, &lm)) {
			pstate.restore ();
			configure_processors_unlocked (0, &lm);
			return -1;
		}

		_have_internal_generator = false;

		for (i = _processors.begin(); i != _processors.end(); ++i) {
			boost::shared_ptr<PluginInsert> pi;
			if ((pi = boost::dynamic_pointer_cast<PluginInsert>(*i)) != 0) {
				if (pi->has_no_inputs ()) {
					_have_internal_generator = true;
					break;
				}
			}
		}

		if (enable) {
			sub->enable (true);
		}

		sub->ActiveChanged.connect_same_thread (*this, boost::bind (&Session::update_latency_compensation, &_session, false));
		_output->set_user_latency (0);
	}

	reset_instrument_info ();
	old->drop_references ();
	processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
	set_processor_positions ();
	return 0;
}

int
Route::remove_processors (const ProcessorList& to_be_deleted, ProcessorStreams* err)
{
	ProcessorList deleted;

	if (!_session.engine().connected()) {
		return 1;
	}

	processor_max_streams.reset();

	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);
		ProcessorState pstate (this);

		ProcessorList::iterator i;
		boost::shared_ptr<Processor> processor;

		for (i = _processors.begin(); i != _processors.end(); ) {

			processor = *i;

			/* these can never be removed */

			if (processor == _amp || processor == _meter || processor == _main_outs || processor == _delayline || processor == _trim) {
				++i;
				continue;
			}

			/* see if its in the list of processors to delete */

			if (find (to_be_deleted.begin(), to_be_deleted.end(), processor) == to_be_deleted.end()) {
				++i;
				continue;
			}

			/* stop IOProcessors that send to JACK ports
			   from causing noise as a result of no longer being
			   run.
			*/

			boost::shared_ptr<IOProcessor> iop = boost::dynamic_pointer_cast<IOProcessor>(processor);
			boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert>(processor);
			if (pi != 0) {
				assert (iop == 0);
				iop = pi->sidechain();
			}

			if (iop != 0) {
				iop->disconnect ();
			}

			deleted.push_back (processor);
			i = _processors.erase (i);
		}

		if (deleted.empty()) {
			/* none of those in the requested list were found */
			return 0;
		}

		_output->set_user_latency (0);

		if (configure_processors_unlocked (err, &lm)) {
			pstate.restore ();
			/* we know this will work, because it worked before :) */
			configure_processors_unlocked (0, &lm);
			return -1;
		}
		//lx.unlock();

		_have_internal_generator = false;

		for (i = _processors.begin(); i != _processors.end(); ++i) {
			boost::shared_ptr<PluginInsert> pi;

			if ((pi = boost::dynamic_pointer_cast<PluginInsert>(*i)) != 0) {
				if (pi->has_no_inputs ()) {
					_have_internal_generator = true;
					break;
				}
			}
		}
	}

	/* now try to do what we need to so that those that were removed will be deleted */

	for (ProcessorList::iterator i = deleted.begin(); i != deleted.end(); ++i) {
		(*i)->drop_references ();
	}

	reset_instrument_info ();
	processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
	set_processor_positions ();

	return 0;
}

void
Route::reset_instrument_info ()
{
	boost::shared_ptr<Processor> instr = the_instrument();
	if (instr) {
		_instrument_info.set_internal_instrument (instr);
	}
}

/** Caller must hold process lock */
int
Route::configure_processors (ProcessorStreams* err)
{
#ifndef PLATFORM_WINDOWS
	assert (!AudioEngine::instance()->process_lock().trylock());
#endif

	if (!_in_configure_processors) {
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);
		return configure_processors_unlocked (err, &lm);
	}

	return 0;
}

ChanCount
Route::input_streams () const
{
	return _input->n_ports ();
}

list<pair<ChanCount, ChanCount> >
Route::try_configure_processors (ChanCount in, ProcessorStreams* err)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	return try_configure_processors_unlocked (in, err);
}

list<pair<ChanCount, ChanCount> >
Route::try_configure_processors_unlocked (ChanCount in, ProcessorStreams* err)
{
	// Check each processor in order to see if we can configure as requested
	ChanCount out;
	list<pair<ChanCount, ChanCount> > configuration;
	uint32_t index = 0;

	DEBUG_TRACE (DEBUG::Processors, string_compose ("%1: configure processors\n", _name));
	DEBUG_TRACE (DEBUG::Processors, "{\n");

	for (ProcessorList::iterator p = _processors.begin(); p != _processors.end(); ++p, ++index) {

		if ((*p)->can_support_io_configuration(in, out)) {

			if (boost::dynamic_pointer_cast<Delivery> (*p)
					&& boost::dynamic_pointer_cast<Delivery> (*p)->role() == Delivery::Main
					&& !is_auditioner()
					&& (is_monitor() || _strict_io || Profile->get_mixbus ())) {
				/* with strict I/O the panner + output are forced to
				 * follow the last processor's output.
				 *
				 * Delivery::can_support_io_configuration() will only add ports,
				 * but not remove excess ports.
				 *
				 * This works because the delivery only requires
				 * as many outputs as there are inputs.
				 * Delivery::configure_io() will do the actual removal
				 * by calling _output->ensure_io()
				 */
				if (!is_master() && _session.master_out () && in.n_audio() > 0) {
					/* ..but at least as many as there are master-inputs, if
					 * the delivery is dealing with audio */
					// XXX this may need special-casing for mixbus (master-outputs)
					// and should maybe be a preference anyway ?!
					out = ChanCount::max (in, _session.master_out ()->n_inputs ());
				} else {
					out = in;
				}
			}

			DEBUG_TRACE (DEBUG::Processors, string_compose ("\t%1 ID=%2 in=%3 out=%4\n",(*p)->name(), (*p)->id(), in, out));
			configuration.push_back(make_pair(in, out));

			if (is_monitor()) {
				// restriction for Monitor Section Processors
				if (in.n_audio() != out.n_audio() || out.n_midi() > 0) {
					/* Note: The Monitor follows the master-bus and has no panner.
					 *
					 * The general idea is to only allow plugins that retain the channel-count
					 * and plugins with MIDI in (e.g VSTs with control that will remain unconnected).
					 * Then again 5.1 in, monitor stereo is a valid use-case.
					 *
					 * and worse: we only refuse adding plugins *here*.
					 *
					 * 1) stereo-master, stereo-mon, add a stereo-plugin, OK
					 * 2) change master-bus, add a channel
					 * 2a) monitor-secion follows
					 * 3) monitor processors fail to re-reconfigure (stereo plugin)
					 * 4) re-load session, monitor-processor remains unconfigured, crash.
					 */
					DEBUG_TRACE (DEBUG::Processors, "Monitor: Channel configuration change.\n");
				}
				if (boost::dynamic_pointer_cast<InternalSend> (*p)) {
					// internal sends make no sense, only feedback
					DEBUG_TRACE (DEBUG::Processors, "Monitor: No Sends allowed.\n");
					return list<pair<ChanCount, ChanCount> > ();
				}
				if (boost::dynamic_pointer_cast<PortInsert> (*p)) {
					/* External Sends can be problematic. one can add/remove ports
					 * there signal leaves the DAW to external monitors anyway, so there's
					 * no real use for allowing them here anyway.
					 */
					DEBUG_TRACE (DEBUG::Processors, "Monitor: No External Sends allowed.\n");
					return list<pair<ChanCount, ChanCount> > ();
				}
				if (boost::dynamic_pointer_cast<Send> (*p)) {
					// ditto
					DEBUG_TRACE (DEBUG::Processors, "Monitor: No Sends allowed.\n");
					return list<pair<ChanCount, ChanCount> > ();
				}
			}
			in = out;
		} else {
			if (err) {
				err->index = index;
				err->count = in;
			}
			DEBUG_TRACE (DEBUG::Processors, "---- CONFIGURATION FAILED.\n");
			DEBUG_TRACE (DEBUG::Processors, string_compose ("---- %1 cannot support in=%2 out=%3\n", (*p)->name(), in, out));
			DEBUG_TRACE (DEBUG::Processors, "}\n");
			return list<pair<ChanCount, ChanCount> > ();
		}
	}

	DEBUG_TRACE (DEBUG::Processors, "}\n");

	return configuration;
}

/** Set the input/output configuration of each processor in the processors list.
 *  Caller must hold process lock.
 *  Return 0 on success, otherwise configuration is impossible.
 */
int
Route::configure_processors_unlocked (ProcessorStreams* err, Glib::Threads::RWLock::WriterLock* lm)
{
#ifndef PLATFORM_WINDOWS
	assert (!AudioEngine::instance()->process_lock().trylock());
#endif

	if (_in_configure_processors) {
		return 0;
	}

	/* put invisible processors where they should be */
	setup_invisible_processors ();

	_in_configure_processors = true;

	list<pair<ChanCount, ChanCount> > configuration = try_configure_processors_unlocked (input_streams (), err);

	if (configuration.empty ()) {
		_in_configure_processors = false;
		return -1;
	}

	ChanCount out;
	bool seen_mains_out = false;
	processor_out_streams = _input->n_ports();
	processor_max_streams.reset();

	/* processor configure_io() may result in adding ports
	 * e.g. Delivery::configure_io -> ARDOUR::IO::ensure_io ()
	 *
	 * with jack2 adding ports results in a graph-order callback,
	 * which calls Session::resort_routes() and eventually
	 * Route::direct_feeds_according_to_reality()
	 * which takes a ReaderLock (_processor_lock).
	 *
	 * so we can't hold a WriterLock here until jack2 threading
	 * is fixed.
	 *
	 * NB. we still hold the process lock
	 *
	 * (ardour's own engines do call graph-order from the
	 * process-thread and hence do not have this issue; besides
	 * merely adding ports won't trigger a graph-order, only
	 * making connections does)
	 */
	lm->release ();

	// TODO check for a potential ReaderLock after ReaderLock ??
	Glib::Threads::RWLock::ReaderLock lr (_processor_lock);

	list< pair<ChanCount,ChanCount> >::iterator c = configuration.begin();
	for (ProcessorList::iterator p = _processors.begin(); p != _processors.end(); ++p, ++c) {

		if (!(*p)->configure_io(c->first, c->second)) {
			DEBUG_TRACE (DEBUG::Processors, string_compose ("%1: configuration failed\n", _name));
			_in_configure_processors = false;
			lr.release ();
			lm->acquire ();
			return -1;
		}
		processor_max_streams = ChanCount::max(processor_max_streams, c->first);
		processor_max_streams = ChanCount::max(processor_max_streams, c->second);

		boost::shared_ptr<IOProcessor> iop;
		boost::shared_ptr<PluginInsert> pi;
		if ((pi = boost::dynamic_pointer_cast<PluginInsert>(*p)) != 0) {
			/* plugins connected via Split or Hide Match may have more channels.
			 * route/scratch buffers are needed for all of them
			 * The configuration may only be a subset (both input and output)
			 */
			processor_max_streams = ChanCount::max(processor_max_streams, pi->required_buffers());
		}
		else if ((iop = boost::dynamic_pointer_cast<IOProcessor>(*p)) != 0) {
			processor_max_streams = ChanCount::max(processor_max_streams, iop->natural_input_streams());
			processor_max_streams = ChanCount::max(processor_max_streams, iop->natural_output_streams());
		}
		out = c->second;

		if (boost::dynamic_pointer_cast<Delivery> (*p)
				&& boost::dynamic_pointer_cast<Delivery> (*p)->role() == Delivery::Main) {
			/* main delivery will increase port count to match input.
			 * the Delivery::Main is usually the last processor - followed only by
			 * 'MeterOutput'.
			 */
			seen_mains_out = true;
		}
		if (!seen_mains_out) {
			processor_out_streams = out;
		}
	}

	lr.release ();
	lm->acquire ();


	if (_meter) {
		_meter->set_max_channels (processor_max_streams);
	}

	/* make sure we have sufficient scratch buffers to cope with the new processor
	   configuration
	*/
	_session.ensure_buffers (n_process_buffers ());

	DEBUG_TRACE (DEBUG::Processors, string_compose ("%1: configuration complete\n", _name));

	_in_configure_processors = false;
	return 0;
}

/** Set all visible processors to a given active state (except Fader, whose state is not changed)
 *  @param state New active state for those processors.
 */
void
Route::all_visible_processors_active (bool state)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	if (_processors.empty()) {
		return;
	}

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if (!(*i)->display_to_user() || boost::dynamic_pointer_cast<Amp> (*i)) {
			continue;
		}
		(*i)->enable (state);
	}

	_session.set_dirty ();
}

bool
Route::processors_reorder_needs_configure (const ProcessorList& new_order)
{
	/* check if re-order requires re-configuration of any processors
	 * -> compare channel configuration for all processors
	 */
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	ChanCount c = input_streams ();

	for (ProcessorList::const_iterator j = new_order.begin(); j != new_order.end(); ++j) {
		bool found = false;
		if (c != (*j)->input_streams()) {
			return true;
		}
		for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
			if (*i == *j) {
				found = true;
				if ((*i)->input_streams() != c) {
					return true;
				}
				c = (*i)->output_streams();
				break;
			}
		}
		if (!found) {
			return true;
		}
	}
	return false;
}

#ifdef __clang__
__attribute__((annotate("realtime")))
#endif
void
Route::apply_processor_order (const ProcessorList& new_order)
{
	/* need to hold processor_lock; either read or write lock
	 * and the engine process_lock.
	 * Due to r/w lock ambiguity we can only assert the latter
	 */
	assert (!AudioEngine::instance()->process_lock().trylock());


	/* "new_order" is an ordered list of processors to be positioned according to "placement".
	 * NOTE: all processors in "new_order" MUST be marked as display_to_user(). There maybe additional
	 * processors in the current actual processor list that are hidden. Any visible processors
	 *  in the current list but not in "new_order" will be assumed to be deleted.
	 */

	/* "as_it_will_be" and "_processors" are lists of shared pointers.
	 * actual memory usage is small, but insert/erase is not actually rt-safe :(
	 * (note though that  ::processors_reorder_needs_configure() ensured that
	 * this function will only ever be called from the rt-thread if no processor were removed)
	 *
	 * either way, I can't proove it, but an x-run due to re-order here is less likley
	 * than an x-run-less 'ardour-silent cycle' both of which effectively "click".
	 */

	ProcessorList as_it_will_be;
	ProcessorList::iterator oiter;
	ProcessorList::const_iterator niter;

	oiter = _processors.begin();
	niter = new_order.begin();

	while (niter !=  new_order.end()) {

		/* if the next processor in the old list is invisible (i.e. should not be in the new order)
		   then append it to the temp list.

		   Otherwise, see if the next processor in the old list is in the new list. if not,
		   its been deleted. If its there, append it to the temp list.
		   */

		if (oiter == _processors.end()) {

			/* no more elements in the old list, so just stick the rest of
			   the new order onto the temp list.
			   */

			as_it_will_be.insert (as_it_will_be.end(), niter, new_order.end());
			while (niter != new_order.end()) {
				++niter;
			}
			break;

		} else {

			if (!(*oiter)->display_to_user()) {

				as_it_will_be.push_back (*oiter);

			} else {

				/* visible processor: check that its in the new order */

				if (find (new_order.begin(), new_order.end(), (*oiter)) == new_order.end()) {
					/* deleted: do nothing, shared_ptr<> will clean up */
				} else {
					/* ignore this one, and add the next item from the new order instead */
					as_it_will_be.push_back (*niter);
					++niter;
				}
			}

			/* now remove from old order - its taken care of no matter what */
			oiter = _processors.erase (oiter);
		}

	}
	_processors.insert (oiter, as_it_will_be.begin(), as_it_will_be.end());

	/* If the meter is in a custom position, find it and make a rough note of its position */
	maybe_note_meter_position ();
}

void
Route::move_instrument_down (bool postfader)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	ProcessorList new_order;
	boost::shared_ptr<Processor> instrument;
	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert>(*i);
		if (pi && pi->plugin ()->get_info ()->is_instrument ()) {
			instrument = *i;
		} else if (instrument && *i == _amp) {
			if (postfader) {
				new_order.push_back (*i);
				new_order.push_back (instrument);
			} else {
				new_order.push_back (instrument);
				new_order.push_back (*i);
			}
		} else {
			new_order.push_back (*i);
		}
	}
	if (!instrument) {
		return;
	}
	lm.release ();
	reorder_processors (new_order, 0);
}

int
Route::reorder_processors (const ProcessorList& new_order, ProcessorStreams* err)
{
	// it a change is already queued, wait for it
	// (unless engine is stopped. apply immediately and proceed
	while (g_atomic_int_get (&_pending_process_reorder)) {
		if (!AudioEngine::instance()->running()) {
			DEBUG_TRACE (DEBUG::Processors, "offline apply queued processor re-order.\n");
			Glib::Threads::RWLock::WriterLock lm (_processor_lock);

			apply_processor_order(_pending_processor_order);
			setup_invisible_processors ();

			g_atomic_int_set (&_pending_process_reorder, 0);

			processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
			set_processor_positions ();
		} else {
			// TODO rather use a semaphore or something.
			// but since ::reorder_processors() is called
			// from the GUI thread, this is fine..
			Glib::usleep(500);
		}
	}

	if (processors_reorder_needs_configure (new_order) || !AudioEngine::instance()->running()) {

		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);
		ProcessorState pstate (this);

		apply_processor_order (new_order);

		if (configure_processors_unlocked (err, &lm)) {
			pstate.restore ();
			return -1;
		}

		lm.release();
		lx.release();

		processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
		set_processor_positions ();

	} else {
		DEBUG_TRACE (DEBUG::Processors, "Queue clickless processor re-order.\n");
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

		// _pending_processor_order is protected by _processor_lock
		_pending_processor_order = new_order;
		g_atomic_int_set (&_pending_process_reorder, 1);
	}

	return 0;
}

bool
Route::add_remove_sidechain (boost::shared_ptr<Processor> proc, bool add)
{
	boost::shared_ptr<PluginInsert> pi;
	if ((pi = boost::dynamic_pointer_cast<PluginInsert>(proc)) == 0) {
		return false;
	}

	if (pi->has_sidechain () == add) {
		return true; // ?? call failed, but result is as expected.
	}

	{
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
		ProcessorList::iterator i = find (_processors.begin(), _processors.end(), proc);
		if (i == _processors.end ()) {
			return false;
		}
	}

	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ()); // take before Writerlock to avoid deadlock
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);
		PBD::Unwinder<bool> uw (_in_sidechain_setup, true);

		lx.release (); // IO::add_port() and ~IO takes process lock  - XXX check if this is safe
		if (add) {
			if (!pi->add_sidechain ()) {
				return false;
			}
		} else {
			if (!pi->del_sidechain ()) {
				return false;
			}
		}

		lx.acquire ();
		list<pair<ChanCount, ChanCount> > c = try_configure_processors_unlocked (n_inputs (), 0);
		lx.release ();

		if (c.empty()) {
			if (add) {
				pi->del_sidechain ();
			} else {
				pi->add_sidechain ();
				// TODO restore side-chain's state.
			}
			return false;
		}
		lx.acquire ();
		configure_processors_unlocked (0, &lm);
	}

	if (pi->has_sidechain ()) {
		pi->sidechain_input ()->changed.connect_same_thread (*this, boost::bind (&Route::sidechain_change_handler, this, _1, _2));
	}

	processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
	_session.set_dirty ();
	return true;
}

bool
Route::plugin_preset_output (boost::shared_ptr<Processor> proc, ChanCount outs)
{
	boost::shared_ptr<PluginInsert> pi;
	if ((pi = boost::dynamic_pointer_cast<PluginInsert>(proc)) == 0) {
		return false;
	}

	{
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
		ProcessorList::iterator i = find (_processors.begin(), _processors.end(), proc);
		if (i == _processors.end ()) {
			return false;
		}
	}

	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);

		const ChanCount& old (pi->preset_out ());
		if (!pi->set_preset_out (outs)) {
			return true; // no change, OK
		}

		list<pair<ChanCount, ChanCount> > c = try_configure_processors_unlocked (n_inputs (), 0);
		if (c.empty()) {
			/* not possible */
			pi->set_preset_out (old);
			return false;
		}
		configure_processors_unlocked (0, &lm);
	}

	processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
	_session.set_dirty ();
	return true;
}

bool
Route::reset_plugin_insert (boost::shared_ptr<Processor> proc)
{
	ChanCount unused;
	return customize_plugin_insert (proc, 0, unused, unused);
}

bool
Route::customize_plugin_insert (boost::shared_ptr<Processor> proc, uint32_t count, ChanCount outs, ChanCount sinks)
{
	boost::shared_ptr<PluginInsert> pi;
	if ((pi = boost::dynamic_pointer_cast<PluginInsert>(proc)) == 0) {
		return false;
	}

	{
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
		ProcessorList::iterator i = find (_processors.begin(), _processors.end(), proc);
		if (i == _processors.end ()) {
			return false;
		}
	}

	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);

		bool      old_cust  = pi->custom_cfg ();
		uint32_t  old_cnt   = pi->get_count ();
		ChanCount old_chan  = pi->output_streams ();
		ChanCount old_sinks = pi->natural_input_streams ();

		if (count == 0) {
			pi->set_custom_cfg (false);
		} else {
			pi->set_custom_cfg (true);
			pi->set_count (count);
			pi->set_outputs (outs);
			pi->set_sinks (sinks);
		}

		list<pair<ChanCount, ChanCount> > c = try_configure_processors_unlocked (n_inputs (), 0);
		if (c.empty()) {
			/* not possible */

			pi->set_count (old_cnt);
			pi->set_sinks (old_sinks);
			pi->set_outputs (old_chan);
			pi->set_custom_cfg (old_cust);

			return false;
		}
		configure_processors_unlocked (0, &lm);
	}

	processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
	_session.set_dirty ();
	return true;
}

bool
Route::set_strict_io (const bool enable)
{
	Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());

	if (_strict_io != enable) {
		_strict_io = enable;
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
		for (ProcessorList::iterator p = _processors.begin(); p != _processors.end(); ++p) {
			boost::shared_ptr<PluginInsert> pi;
			if ((pi = boost::dynamic_pointer_cast<PluginInsert>(*p)) != 0) {
				pi->set_strict_io (_strict_io);
			}
		}

		list<pair<ChanCount, ChanCount> > c = try_configure_processors_unlocked (n_inputs (), 0);

		if (c.empty()) {
			// not possible
			_strict_io = !enable; // restore old value
			for (ProcessorList::iterator p = _processors.begin(); p != _processors.end(); ++p) {
				boost::shared_ptr<PluginInsert> pi;
				if ((pi = boost::dynamic_pointer_cast<PluginInsert>(*p)) != 0) {
					pi->set_strict_io (_strict_io);
				}
			}
			return false;
		}
		lm.release ();

		configure_processors (0);
		lx.release ();

		processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
		_session.set_dirty ();
	}
	return true;
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
	LocaleGuard lg;
	if (!_session._template_state_dir.empty()) {
		foreach_processor (sigc::bind (sigc::mem_fun (*this, &Route::set_plugin_state_dir), _session._template_state_dir));
	}

	XMLNode *node = new XMLNode("Route");
	ProcessorList::iterator i;
	char buf[32];

	id().print (buf, sizeof (buf));
	node->add_property("id", buf);
	node->add_property ("name", _name);
	node->add_property("default-type", _default_type.to_string());
	node->add_property ("strict-io", _strict_io);

	node->add_child_nocopy (_presentation_info.get_state());

	node->add_property("active", _active?"yes":"no");
	string p;
	node->add_property("denormal-protection", _denormal_protection?"yes":"no");
	node->add_property("meter-point", enum_2_string (_meter_point));

	node->add_property("meter-type", enum_2_string (_meter_type));

	if (_route_group) {
		node->add_property("route-group", _route_group->name());
	}

	node->add_child_nocopy (_solo_control->get_state ());
	node->add_child_nocopy (_solo_isolate_control->get_state ());
	node->add_child_nocopy (_solo_safe_control->get_state ());

	node->add_child_nocopy (_input->state (full_state));
	node->add_child_nocopy (_output->state (full_state));
	node->add_child_nocopy (_mute_master->get_state ());

	node->add_child_nocopy (_mute_control->get_state ());
	node->add_child_nocopy (_phase_control->get_state ());

	if (full_state) {
		node->add_child_nocopy (Automatable::get_automation_xml_state ());
	}

	if (_comment.length()) {
		XMLNode *cmt = node->add_child ("Comment");
		cmt->add_content (_comment);
	}

	if (_pannable) {
		node->add_child_nocopy (_pannable->state (full_state));
	}

	{
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
		for (i = _processors.begin(); i != _processors.end(); ++i) {
			if (!full_state) {
				/* template save: do not include internal sends functioning as
					 aux sends because the chance of the target ID
					 in the session where this template is used
					 is not very likely.

					 similarly, do not save listen sends which connect to
					 the monitor section, because these will always be
					 added if necessary.
					 */
				boost::shared_ptr<InternalSend> is;

				if ((is = boost::dynamic_pointer_cast<InternalSend> (*i)) != 0) {
					if (is->role() == Delivery::Listen) {
						continue;
					}
				}
			}
			node->add_child_nocopy((*i)->state (full_state));
		}
	}

	if (_extra_xml) {
		node->add_child_copy (*_extra_xml);
	}

	if (_custom_meter_position_noted) {
		boost::shared_ptr<Processor> after = _processor_after_last_custom_meter.lock ();
		if (after) {
			after->id().print (buf, sizeof (buf));
			node->add_property (X_("processor-after-last-custom-meter"), buf);
		}
	}

	if (!_session._template_state_dir.empty()) {
		foreach_processor (sigc::bind (sigc::mem_fun (*this, &Route::set_plugin_state_dir), ""));
	}

	node->add_child_copy (Slavable::get_state());

	return *node;
}

int
Route::set_state (const XMLNode& node, int version)
{
	if (version < 3000) {
		return set_state_2X (node, version);
	}

	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	XMLNode *child;
	XMLProperty const * prop;

	if (node.name() != "Route"){
		error << string_compose(_("Bad node sent to Route::set_state() [%1]"), node.name()) << endmsg;
		return -1;
	}

	if ((prop = node.property (X_("name"))) != 0) {
		Route::set_name (prop->value());
	}

	set_id (node);
	_initial_io_setup = true;

	Stripable::set_state (node, version);

	if ((prop = node.property (X_("strict-io"))) != 0) {
		_strict_io = string_is_affirmative (prop->value());
	}

	if (is_monitor()) {
		/* monitor bus does not get a panner, but if (re)created
		   via XML, it will already have one by the time we
		   call ::set_state(). so ... remove it.
		*/
		unpan ();
	}

	/* add all processors (except amp, which is always present) */

	nlist = node.children();
	XMLNode processor_state (X_("processor_state"));

	Stateful::save_extra_xml (node);

	for (niter = nlist.begin(); niter != nlist.end(); ++niter){

		child = *niter;

		if (child->name() == IO::state_node_name) {
			if ((prop = child->property (X_("direction"))) == 0) {
				continue;
			}

			if (prop->value() == "Input") {
				_input->set_state (*child, version);
			} else if (prop->value() == "Output") {
				_output->set_state (*child, version);
			}

		} else if (child->name() == X_("Processor")) {
			processor_state.add_child_copy (*child);
		} else if (child->name() == X_("Pannable")) {
			if (_pannable) {
				_pannable->set_state (*child, version);
			} else {
				warning << string_compose (_("Pannable state found for route (%1) without a panner!"), name()) << endmsg;
			}
		} else if (child->name() == Slavable::xml_node_name) {
			Slavable::set_state (*child, version);
		}
	}

	if ((prop = node.property (X_("meter-point"))) != 0) {
		MeterPoint mp = MeterPoint (string_2_enum (prop->value (), _meter_point));
		set_meter_point (mp, true);
		if (_meter) {
			_meter->set_display_to_user (_meter_point == MeterCustom);
		}
	}

	if ((prop = node.property (X_("meter-type"))) != 0) {
		_meter_type = MeterType (string_2_enum (prop->value (), _meter_type));
	}

	_initial_io_setup = false;

	set_processor_state (processor_state);

	// this looks up the internal instrument in processors
	reset_instrument_info();

	if ((prop = node.property (X_("denormal-protection"))) != 0) {
		set_denormal_protection (string_is_affirmative (prop->value()));
	}

	if ((prop = node.property (X_("active"))) != 0) {
		bool yn = string_is_affirmative (prop->value());
		set_active (yn, this);
	}

	if ((prop = node.property (X_("processor-after-last-custom-meter"))) != 0) {
		PBD::ID id (prop->value ());
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
		ProcessorList::const_iterator i = _processors.begin ();
		while (i != _processors.end() && (*i)->id() != id) {
			++i;
		}

		if (i != _processors.end ()) {
			_processor_after_last_custom_meter = *i;
			_custom_meter_position_noted = true;
		}
	}

	for (niter = nlist.begin(); niter != nlist.end(); ++niter){
		child = *niter;

		if (child->name() == X_("Comment")) {

			/* XXX this is a terrible API design in libxml++ */

			XMLNode *cmt = *(child->children().begin());
			_comment = cmt->content();

		}  else if (child->name() == Controllable::xml_node_name) {
			if ((prop = child->property (X_("name"))) == 0) {
				continue;
			}

			if (prop->value() == _gain_control->name()) {
				_gain_control->set_state (*child, version);
			} else if (prop->value() == _solo_control->name()) {
				_solo_control->set_state (*child, version);
			} else if (prop->value() == _solo_safe_control->name()) {
				_solo_safe_control->set_state (*child, version);
			} else if (prop->value() == _solo_isolate_control->name()) {
				_solo_isolate_control->set_state (*child, version);
			} else if (prop->value() == _mute_control->name()) {
				_mute_control->set_state (*child, version);
			} else if (prop->value() == _phase_control->name()) {
				_phase_control->set_state (*child, version);
			} else {
				Evoral::Parameter p = EventTypeMap::instance().from_symbol (prop->value());
				if (p.type () >= MidiCCAutomation && p.type () < MidiSystemExclusiveAutomation) {
					boost::shared_ptr<AutomationControl> ac = automation_control (p, true);
					if (ac) {
						ac->set_state (*child, version);
					}
				}
			}
		} else if (child->name() == MuteMaster::xml_node_name) {
			_mute_master->set_state (*child, version);

		} else if (child->name() == Automatable::xml_node_name) {
			set_automation_xml_state (*child, Evoral::Parameter(NullAutomation));
		}
	}

	return 0;
}

int
Route::set_state_2X (const XMLNode& node, int version)
{
	LocaleGuard lg;
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	XMLNode *child;
	XMLProperty const * prop;

	/* 2X things which still remain to be handled:
	 * default-type
	 * automation
	 * controlouts
	 */

	if (node.name() != "Route") {
		error << string_compose(_("Bad node sent to Route::set_state() [%1]"), node.name()) << endmsg;
		return -1;
	}

	Stripable::set_state (node, version);

	if ((prop = node.property (X_("denormal-protection"))) != 0) {
		set_denormal_protection (string_is_affirmative (prop->value()));
	}

	if ((prop = node.property (X_("muted"))) != 0) {

		bool first = true;
		bool muted = string_is_affirmative (prop->value());

		if (muted) {

			string mute_point;

			if ((prop = node.property (X_("mute-affects-pre-fader"))) != 0) {

				if (string_is_affirmative (prop->value())){
					mute_point = mute_point + "PreFader";
					first = false;
				}
			}

			if ((prop = node.property (X_("mute-affects-post-fader"))) != 0) {

				if (string_is_affirmative (prop->value())){

					if (!first) {
						mute_point = mute_point + ",";
					}

					mute_point = mute_point + "PostFader";
					first = false;
				}
			}

			if ((prop = node.property (X_("mute-affects-control-outs"))) != 0) {

				if (string_is_affirmative (prop->value())){

					if (!first) {
						mute_point = mute_point + ",";
					}

					mute_point = mute_point + "Listen";
					first = false;
				}
			}

			if ((prop = node.property (X_("mute-affects-main-outs"))) != 0) {

				if (string_is_affirmative (prop->value())){

					if (!first) {
						mute_point = mute_point + ",";
					}

					mute_point = mute_point + "Main";
				}
			}

			_mute_master->set_mute_points (mute_point);
			_mute_master->set_muted_by_self (true);
		}
	}

	if ((prop = node.property (X_("meter-point"))) != 0) {
		_meter_point = MeterPoint (string_2_enum (prop->value (), _meter_point));
	}

	/* IOs */

	nlist = node.children ();
	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		child = *niter;

		if (child->name() == IO::state_node_name) {

			/* there is a note in IO::set_state_2X() about why we have to call
			   this directly.
			   */

			_input->set_state_2X (*child, version, true);
			_output->set_state_2X (*child, version, false);

			if ((prop = child->property (X_("name"))) != 0) {
				Route::set_name (prop->value ());
			}

			set_id (*child);

			if ((prop = child->property (X_("active"))) != 0) {
				bool yn = string_is_affirmative (prop->value());
				_active = !yn; // force switch
				set_active (yn, this);
			}

			if ((prop = child->property (X_("gain"))) != 0) {
				gain_t val;

				if (sscanf (prop->value().c_str(), "%f", &val) == 1) {
					_amp->gain_control()->set_value (val, Controllable::NoGroup);
				}
			}

			/* Set up Panners in the IO */
			XMLNodeList io_nlist = child->children ();

			XMLNodeConstIterator io_niter;
			XMLNode *io_child;

			for (io_niter = io_nlist.begin(); io_niter != io_nlist.end(); ++io_niter) {

				io_child = *io_niter;

				if (io_child->name() == X_("Panner")) {
					_main_outs->panner_shell()->set_state(*io_child, version);
				} else if (io_child->name() == X_("Automation")) {
					/* IO's automation is for the fader */
					_amp->set_automation_xml_state (*io_child, Evoral::Parameter (GainAutomation));
				}
			}
		}
	}

	XMLNodeList redirect_nodes;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter){

		child = *niter;

		if (child->name() == X_("Send") || child->name() == X_("Insert")) {
			redirect_nodes.push_back(child);
		}

	}

	set_processor_state_2X (redirect_nodes, version);

	Stateful::save_extra_xml (node);

	for (niter = nlist.begin(); niter != nlist.end(); ++niter){
		child = *niter;

		if (child->name() == X_("Comment")) {

			/* XXX this is a terrible API design in libxml++ */

			XMLNode *cmt = *(child->children().begin());
			_comment = cmt->content();

		} else if (child->name() == Controllable::xml_node_name && (prop = child->property("name")) != 0) {
			if (prop->value() == X_("solo")) {
				_solo_control->set_state (*child, version);
			} else if (prop->value() == X_("mute")) {
				_mute_control->set_state (*child, version);
			}

		}
	}

	return 0;
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

void
Route::set_processor_state_2X (XMLNodeList const & nList, int version)
{
	/* We don't bother removing existing processors not in nList, as this
	   method will only be called when creating a Route from scratch, not
	   for undo purposes.  Just put processors in at the appropriate place
	   in the list.
	*/

	for (XMLNodeConstIterator i = nList.begin(); i != nList.end(); ++i) {
		add_processor_from_xml_2X (**i, version);
	}
}

void
Route::set_processor_state (const XMLNode& node)
{
	const XMLNodeList &nlist = node.children();
	XMLNodeConstIterator niter;
	ProcessorList new_order;
	bool must_configure = false;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		XMLProperty* prop = (*niter)->property ("type");

		if (prop->value() == "amp") {
			_amp->set_state (**niter, Stateful::current_state_version);
			new_order.push_back (_amp);
		} else if (prop->value() == "trim") {
			_trim->set_state (**niter, Stateful::current_state_version);
			new_order.push_back (_trim);
		} else if (prop->value() == "meter") {
			_meter->set_state (**niter, Stateful::current_state_version);
			new_order.push_back (_meter);
		} else if (prop->value() == "delay") {
			if (_delayline) {
				_delayline->set_state (**niter, Stateful::current_state_version);
				new_order.push_back (_delayline);
			}
		} else if (prop->value() == "main-outs") {
			_main_outs->set_state (**niter, Stateful::current_state_version);
		} else if (prop->value() == "intreturn") {
			if (!_intreturn) {
				_intreturn.reset (new InternalReturn (_session));
				must_configure = true;
			}
			_intreturn->set_state (**niter, Stateful::current_state_version);
		} else if (is_monitor() && prop->value() == "monitor") {
			if (!_monitor_control) {
				_monitor_control.reset (new MonitorProcessor (_session));
				must_configure = true;
			}
			_monitor_control->set_state (**niter, Stateful::current_state_version);
		} else if (prop->value() == "capture") {
			/* CapturingProcessor should never be restored, it's always
			   added explicitly when needed */
		} else {
			ProcessorList::iterator o;

			for (o = _processors.begin(); o != _processors.end(); ++o) {
				XMLProperty const * id_prop = (*niter)->property(X_("id"));
				if (id_prop && (*o)->id() == id_prop->value()) {
					(*o)->set_state (**niter, Stateful::current_state_version);
					new_order.push_back (*o);
					break;
				}
			}

			// If the processor (*niter) is not on the route then create it

			if (o == _processors.end()) {

				boost::shared_ptr<Processor> processor;

				if (prop->value() == "intsend") {

					processor.reset (new InternalSend (_session, _pannable, _mute_master, boost::dynamic_pointer_cast<ARDOUR::Route>(shared_from_this()), boost::shared_ptr<Route>(), Delivery::Aux, true));

				} else if (prop->value() == "ladspa" || prop->value() == "Ladspa" ||
				           prop->value() == "lv2" ||
				           prop->value() == "windows-vst" ||
				           prop->value() == "mac-vst" ||
				           prop->value() == "lxvst" ||
				           prop->value() == "luaproc" ||
				           prop->value() == "audiounit") {

					if (_session.get_disable_all_loaded_plugins ()) {
						processor.reset (new UnknownProcessor (_session, **niter));
					} else {
						processor.reset (new PluginInsert (_session));
						processor->set_owner (this);
						if (_strict_io) {
							boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert>(processor);
							pi->set_strict_io (true);
						}

					}
				} else if (prop->value() == "port") {

					processor.reset (new PortInsert (_session, _pannable, _mute_master));

				} else if (prop->value() == "send") {

					processor.reset (new Send (_session, _pannable, _mute_master, Delivery::Send, true));
					boost::shared_ptr<Send> send = boost::dynamic_pointer_cast<Send> (processor);
					send->SelfDestruct.connect_same_thread (*this,
							boost::bind (&Route::processor_selfdestruct, this, boost::weak_ptr<Processor> (processor)));

				} else {
					error << string_compose(_("unknown Processor type \"%1\"; ignored"), prop->value()) << endmsg;
					continue;
				}

				if (processor->set_state (**niter, Stateful::current_state_version) != 0) {
					/* This processor could not be configured.  Turn it into a UnknownProcessor */
					processor.reset (new UnknownProcessor (_session, **niter));
				}

				/* subscribe to Sidechain IO changes */
				boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (processor);
				if (pi && pi->has_sidechain ()) {
					pi->sidechain_input ()->changed.connect_same_thread (*this, boost::bind (&Route::sidechain_change_handler, this, _1, _2));
				}

				/* we have to note the monitor send here, otherwise a new one will be created
				   and the state of this one will be lost.
				*/
				boost::shared_ptr<InternalSend> isend = boost::dynamic_pointer_cast<InternalSend> (processor);
				if (isend && isend->role() == Delivery::Listen) {
					_monitor_send = isend;
				}

				/* it doesn't matter if invisible processors are added here, as they
				   will be sorted out by setup_invisible_processors () shortly.
				*/

				new_order.push_back (processor);
				must_configure = true;
			}
		}
	}

	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);
		/* re-assign _processors w/o process-lock.
		 * if there's an IO-processor present in _processors but
		 * not in new_order, it will be deleted and ~IO takes
		 * a process lock.
		 */
		_processors = new_order;

		if (must_configure) {
			configure_processors_unlocked (0, &lm);
		}

		for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {

			(*i)->set_owner (this);
			(*i)->ActiveChanged.connect_same_thread (*this, boost::bind (&Session::update_latency_compensation, &_session, false));

			boost::shared_ptr<PluginInsert> pi;

			if ((pi = boost::dynamic_pointer_cast<PluginInsert>(*i)) != 0) {
				if (pi->has_no_inputs ()) {
					_have_internal_generator = true;
					break;
				}
			}
		}
	}

	reset_instrument_info ();
	processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
	set_processor_positions ();
}

void
Route::curve_reallocate ()
{
//	_gain_automation_curve.finish_resize ();
//	_pan_automation_curve.finish_resize ();
}

void
Route::silence (framecnt_t nframes)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock, Glib::Threads::TRY_LOCK);
	if (!lm.locked()) {
		return;
	}

	silence_unlocked (nframes);
}

void
Route::silence_unlocked (framecnt_t nframes)
{
	/* Must be called with the processor lock held */

	const framepos_t now = _session.transport_frame ();

	if (!_silent) {

		_output->silence (nframes);

		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
			boost::shared_ptr<PluginInsert> pi;

			if (!_active && (pi = boost::dynamic_pointer_cast<PluginInsert> (*i)) != 0) {
				// skip plugins, they don't need anything when we're not active
				continue;
			}

			(*i)->silence (nframes, now);
		}

		if (nframes == _session.get_block_size()) {
			// _silent = true;
		}
	}
}

void
Route::add_internal_return ()
{
	if (!_intreturn) {
		_intreturn.reset (new InternalReturn (_session));
		add_processor (_intreturn, PreFader);
	}
}

void
Route::add_send_to_internal_return (InternalSend* send)
{
	Glib::Threads::RWLock::ReaderLock rm (_processor_lock);

	for (ProcessorList::const_iterator x = _processors.begin(); x != _processors.end(); ++x) {
		boost::shared_ptr<InternalReturn> d = boost::dynamic_pointer_cast<InternalReturn>(*x);

		if (d) {
			return d->add_send (send);
		}
	}
}

void
Route::remove_send_from_internal_return (InternalSend* send)
{
	Glib::Threads::RWLock::ReaderLock rm (_processor_lock);

	for (ProcessorList::const_iterator x = _processors.begin(); x != _processors.end(); ++x) {
		boost::shared_ptr<InternalReturn> d = boost::dynamic_pointer_cast<InternalReturn>(*x);

		if (d) {
			return d->remove_send (send);
		}
	}
}

void
Route::enable_monitor_send ()
{
	/* Caller must hold process lock */
	assert (!AudioEngine::instance()->process_lock().trylock());

	/* master never sends to monitor section via the normal mechanism */
	assert (!is_master ());
	assert (!is_monitor ());

	/* make sure we have one */
	if (!_monitor_send) {
		_monitor_send.reset (new InternalSend (_session, _pannable, _mute_master, boost::dynamic_pointer_cast<ARDOUR::Route>(shared_from_this()), _session.monitor_out(), Delivery::Listen));
		_monitor_send->set_display_to_user (false);
	}

	/* set it up */
	configure_processors (0);
}

/** Add an aux send to a route.
 *  @param route route to send to.
 *  @param before Processor to insert before, or 0 to insert at the end.
 */
int
Route::add_aux_send (boost::shared_ptr<Route> route, boost::shared_ptr<Processor> before)
{
	assert (route != _session.monitor_out ());

	{
		Glib::Threads::RWLock::ReaderLock rm (_processor_lock);

		for (ProcessorList::iterator x = _processors.begin(); x != _processors.end(); ++x) {

			boost::shared_ptr<InternalSend> d = boost::dynamic_pointer_cast<InternalSend> (*x);

			if (d && d->target_route() == route) {
				/* already listening via the specified IO: do nothing */
				return 0;
			}
		}
	}

	try {

		boost::shared_ptr<InternalSend> listener;

		{
			Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
			listener.reset (new InternalSend (_session, _pannable, _mute_master, boost::dynamic_pointer_cast<ARDOUR::Route>(shared_from_this()), route, Delivery::Aux));
		}

		add_processor (listener, before);

	} catch (failed_constructor& err) {
		return -1;
	}

	return 0;
}

void
Route::remove_aux_or_listen (boost::shared_ptr<Route> route)
{
	ProcessorStreams err;
	ProcessorList::iterator tmp;

	{
		Glib::Threads::RWLock::ReaderLock rl(_processor_lock);

		/* have to do this early because otherwise processor reconfig
		 * will put _monitor_send back in the list
		 */

		if (route == _session.monitor_out()) {
			_monitor_send.reset ();
		}

	  again:
		for (ProcessorList::iterator x = _processors.begin(); x != _processors.end(); ++x) {

			boost::shared_ptr<InternalSend> d = boost::dynamic_pointer_cast<InternalSend>(*x);

			if (d && d->target_route() == route) {
				rl.release ();
				if (remove_processor (*x, &err, false) > 0) {
					rl.acquire ();
					continue;
				}
				rl.acquire ();

				/* list could have been demolished while we dropped the lock
				   so start over.
				*/
				if (_session.engine().connected()) {
					/* i/o processors cannot be removed if the engine is not running
					 * so don't live-loop in case the engine is N/A or dies
					 */
					goto again;
				}
			}
		}
	}
}

void
Route::set_comment (string cmt, void *src)
{
	_comment = cmt;
	comment_changed ();
	_session.set_dirty ();
}

bool
Route::add_fed_by (boost::shared_ptr<Route> other, bool via_sends_only)
{
	FeedRecord fr (other, via_sends_only);

	pair<FedBy::iterator,bool> result =  _fed_by.insert (fr);

	if (!result.second) {

		/* already a record for "other" - make sure sends-only information is correct */
		if (!via_sends_only && result.first->sends_only) {
			FeedRecord* frp = const_cast<FeedRecord*>(&(*result.first));
			frp->sends_only = false;
		}
	}

	return result.second;
}

void
Route::clear_fed_by ()
{
	_fed_by.clear ();
}

bool
Route::feeds (boost::shared_ptr<Route> other, bool* via_sends_only)
{
	const FedBy& fed_by (other->fed_by());

	for (FedBy::const_iterator f = fed_by.begin(); f != fed_by.end(); ++f) {
		boost::shared_ptr<Route> sr = f->r.lock();

		if (sr && (sr.get() == this)) {

			if (via_sends_only) {
				*via_sends_only = f->sends_only;
			}

			return true;
		}
	}

	return false;
}

IOVector
Route::all_inputs () const
{
	/* TODO, if this works as expected,
	 * cache the IOVector and maintain it via
	 * input_change_handler(), sidechain_change_handler() etc
	 */
	IOVector ios;
	ios.push_back (_input);

	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	for (ProcessorList::const_iterator r = _processors.begin(); r != _processors.end(); ++r) {

		boost::shared_ptr<IOProcessor> iop = boost::dynamic_pointer_cast<IOProcessor>(*r);
		boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert>(*r);
		if (pi != 0) {
			assert (iop == 0);
			iop = pi->sidechain();
		}

		if (iop != 0 && iop->input()) {
			ios.push_back (iop->input());
		}
	}
	return ios;
}

IOVector
Route::all_outputs () const
{
	IOVector ios;
	// _output is included via Delivery
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	for (ProcessorList::const_iterator r = _processors.begin(); r != _processors.end(); ++r) {
		boost::shared_ptr<IOProcessor> iop = boost::dynamic_pointer_cast<IOProcessor>(*r);
		if (iop != 0 && iop->output()) {
			ios.push_back (iop->output());
		}
	}
	return ios;
}

bool
Route::direct_feeds_according_to_reality (boost::shared_ptr<Route> other, bool* via_send_only)
{
	DEBUG_TRACE (DEBUG::Graph, string_compose ("Feeds? %1\n", _name));
	if (other->all_inputs().fed_by (_output)) {
		DEBUG_TRACE (DEBUG::Graph, string_compose ("\tdirect FEEDS %2\n", other->name()));
		if (via_send_only) {
			*via_send_only = false;
		}

		return true;
	}

	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	for (ProcessorList::iterator r = _processors.begin(); r != _processors.end(); ++r) {

		boost::shared_ptr<IOProcessor> iop = boost::dynamic_pointer_cast<IOProcessor>(*r);
		boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert>(*r);
		if (pi != 0) {
			assert (iop == 0);
			iop = pi->sidechain();
		}

		if (iop != 0) {
			boost::shared_ptr<const IO> iop_out = iop->output();
			if (other.get() == this && iop_out && iop->input() && iop_out->connected_to (iop->input())) {
				// TODO this needs a delaylines in the Insert to align connections (!)
				DEBUG_TRACE (DEBUG::Graph,  string_compose ("\tIOP %1 does feed its own return (%2)\n", iop->name(), other->name()));
				continue;
			}
			if ((iop_out && other->all_inputs().fed_by (iop_out)) || iop->feeds (other)) {
				DEBUG_TRACE (DEBUG::Graph,  string_compose ("\tIOP %1 does feed %2\n", iop->name(), other->name()));
				if (via_send_only) {
					*via_send_only = true;
				}
				return true;
			} else {
				DEBUG_TRACE (DEBUG::Graph,  string_compose ("\tIOP %1 does NOT feed %2\n", iop->name(), other->name()));
			}
		} else {
			DEBUG_TRACE (DEBUG::Graph,  string_compose ("\tPROC %1 is not an IOP\n", (*r)->name()));
		}

	}

	DEBUG_TRACE (DEBUG::Graph,  string_compose ("\tdoes NOT feed %1\n", other->name()));
	return false;
}

bool
Route::direct_feeds_according_to_graph (boost::shared_ptr<Route> other, bool* via_send_only)
{
	return _session._current_route_graph.has (shared_from_this (), other, via_send_only);
}

bool
Route::feeds_according_to_graph (boost::shared_ptr<Route> other)
{
	return _session._current_route_graph.feeds (shared_from_this (), other);
}

/** Called from the (non-realtime) butler thread when the transport is stopped */
void
Route::nonrealtime_handle_transport_stopped (bool /*abort_ignored*/, bool /*did_locate*/, bool can_flush_processors)
{
	framepos_t now = _session.transport_frame();

	{
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

		Automatable::transport_stopped (now);

		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {

			if (!_have_internal_generator && (Config->get_plugins_stop_with_transport() && can_flush_processors)) {
				(*i)->flush ();
			}

			(*i)->transport_stopped (now);
		}
	}

	_roll_delay = _initial_delay;
}

void
Route::input_change_handler (IOChange change, void * /*src*/)
{
	if ((change.type & IOChange::ConfigurationChanged)) {
		/* This is called with the process lock held if change
		   contains ConfigurationChanged
		*/
		configure_processors (0);
		_phase_control->resize (_input->n_ports().n_audio ());
		io_changed (); /* EMIT SIGNAL */
	}

	if (_solo_control->soloed_by_others_upstream() || _solo_isolate_control->solo_isolated_by_upstream()) {
		int sbou = 0;
		int ibou = 0;
		boost::shared_ptr<RouteList> routes = _session.get_routes ();
		if (_input->connected()) {
			for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
				if ((*i).get() == this || (*i)->is_master() || (*i)->is_monitor() || (*i)->is_auditioner()) {
					continue;
				}
				bool sends_only;
				bool does_feed = (*i)->direct_feeds_according_to_reality (shared_from_this(), &sends_only);
				if (does_feed && !sends_only) {
					if ((*i)->soloed()) {
						++sbou;
					}
					if ((*i)->solo_isolate_control()->solo_isolated()) {
						++ibou;
					}
				}
			}
		}

		int delta  = sbou - _solo_control->soloed_by_others_upstream();
		int idelta = ibou - _solo_isolate_control->solo_isolated_by_upstream();

		if (idelta < -1) {
			PBD::warning << string_compose (
			                _("Invalid Solo-Isolate propagation: from:%1 new:%2 - old:%3 = delta:%4"),
			                _name, ibou, _solo_isolate_control->solo_isolated_by_upstream(), idelta)
			             << endmsg;

		}

		if (_solo_control->soloed_by_others_upstream()) {
			// ignore new connections (they're not propagated)
			if (delta <= 0) {
				_solo_control->mod_solo_by_others_upstream (delta);
			}
		}

		if (_solo_isolate_control->solo_isolated_by_upstream()) {
			// solo-isolate currently only propagates downstream
			if (idelta < 0) {
				_solo_isolate_control->mod_solo_isolated_by_upstream (1);
			}
			//_solo_isolated_by_upstream = ibou;
		}

		// Session::route_solo_changed  does not propagate indirect solo-changes
		// propagate downstream to tracks
		for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
			if ((*i).get() == this || (*i)->is_master() || (*i)->is_monitor() || (*i)->is_auditioner()) {
				continue;
			}
			bool sends_only;
			bool does_feed = feeds (*i, &sends_only);
			if (delta <= 0 && does_feed && !sends_only) {
				(*i)->solo_control()->mod_solo_by_others_upstream (delta);
			}

			if (idelta < 0 && does_feed && !sends_only) {
				(*i)->solo_isolate_control()->mod_solo_isolated_by_upstream (-1);
			}
		}
	}
}

void
Route::output_change_handler (IOChange change, void * /*src*/)
{
	if (_initial_io_setup) {
		return;
	}

	if ((change.type & IOChange::ConfigurationChanged)) {
		/* This is called with the process lock held if change
		   contains ConfigurationChanged
		*/
		configure_processors (0);

		if (is_master()) {
			_session.reset_monitor_section();
		}

		io_changed (); /* EMIT SIGNAL */
	}

	if ((change.type & IOChange::ConnectionsChanged)) {

		/* do this ONLY if connections have changed. Configuration
		 * changes do not, by themselves alter solo upstream or
		 * downstream status.
		 */

		if (_solo_control->soloed_by_others_downstream()) {
			int sbod = 0;
			/* checking all all downstream routes for
			 * explicit of implict solo is a rather drastic measure,
			 * ideally the input_change_handler() of the other route
			 * would propagate the change to us.
			 */
			boost::shared_ptr<RouteList> routes = _session.get_routes ();
			if (_output->connected()) {
				for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
					if ((*i).get() == this || (*i)->is_master() || (*i)->is_monitor() || (*i)->is_auditioner()) {
						continue;
					}
					bool sends_only;
					bool does_feed = direct_feeds_according_to_reality (*i, &sends_only);
					if (does_feed && !sends_only) {
						if ((*i)->soloed()) {
							++sbod;
							break;
						}
					}
				}
			}

			int delta = sbod - _solo_control->soloed_by_others_downstream();
			if (delta <= 0) {
				// do not allow new connections to change implicit solo (no propagation)
				_solo_control->mod_solo_by_others_downstream (delta);
				// Session::route_solo_changed() does not propagate indirect solo-changes
				// propagate upstream to tracks
				boost::shared_ptr<Route> shared_this = shared_from_this();
				for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
					if ((*i).get() == this || !can_solo()) {
						continue;
					}
					bool sends_only;
					bool does_feed = (*i)->feeds (shared_this, &sends_only);
					if (delta != 0 && does_feed && !sends_only) {
						(*i)->solo_control()->mod_solo_by_others_downstream (delta);
					}
				}

			}
		}
	}
}

void
Route::sidechain_change_handler (IOChange change, void* src)
{
	if (_initial_io_setup || _in_sidechain_setup) {
		return;
	}

	input_change_handler (change, src);
}

uint32_t
Route::pans_required () const
{
	if (n_outputs().n_audio() < 2) {
		return 0;
	}

	return max (n_inputs ().n_audio(), processor_max_streams.n_audio());
}

void
Route::flush_processor_buffers_locked (framecnt_t nframes)
{
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		boost::shared_ptr<Delivery> d = boost::dynamic_pointer_cast<Delivery> (*i);
		if (d) {
			d->flush_buffers (nframes);
		} else {
			boost::shared_ptr<PortInsert> p = boost::dynamic_pointer_cast<PortInsert> (*i);
			if (p) {
				p->flush_buffers (nframes);
			}
		}
	}
}

int
Route::no_roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, bool session_state_changing)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		return 0;
	}

	if (n_outputs().n_total() == 0) {
		return 0;
	}

	if (!_active || n_inputs() == ChanCount::ZERO)  {
		silence_unlocked (nframes);
		return 0;
	}

	if (session_state_changing) {
		if (_session.transport_speed() != 0.0f) {
			/* we're rolling but some state is changing (e.g. our diskstream contents)
			   so we cannot use them. Be silent till this is over.

			   XXX note the absurdity of ::no_roll() being called when we ARE rolling!
			*/
			silence_unlocked (nframes);
			return 0;
		}
		/* we're really not rolling, so we're either delivery silence or actually
		   monitoring, both of which are safe to do while session_state_changing is true.
		*/
	}

	BufferSet& bufs = _session.get_route_buffers (n_process_buffers());

	fill_buffers_with_input (bufs, _input, nframes);

	if (_meter_point == MeterInput) {
		_meter->run (bufs, start_frame, end_frame, 0.0, nframes, true);
	}

	_amp->apply_gain_automation (false);
	_trim->apply_gain_automation (false);
	passthru (bufs, start_frame, end_frame, nframes, 0);

	flush_processor_buffers_locked (nframes);

	return 0;
}

int
Route::roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, int declick, bool& /* need_butler */)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock, Glib::Threads::TRY_LOCK);
	if (!lm.locked()) {
		return 0;
	}

	if (n_outputs().n_total() == 0) {
		return 0;
	}

	if (!_active || n_inputs().n_total() == 0) {
		silence_unlocked (nframes);
		return 0;
	}

	framepos_t unused = 0;

	if ((nframes = check_initial_delay (nframes, unused)) == 0) {
		return 0;
	}

	_silent = false;

	BufferSet& bufs = _session.get_route_buffers (n_process_buffers());

	fill_buffers_with_input (bufs, _input, nframes);

	if (_meter_point == MeterInput) {
		_meter->run (bufs, start_frame, end_frame, 1.0, nframes, true);
	}

	passthru (bufs, start_frame, end_frame, nframes, declick);

	flush_processor_buffers_locked (nframes);

	return 0;
}

int
Route::silent_roll (pframes_t nframes, framepos_t /*start_frame*/, framepos_t /*end_frame*/, bool& /* need_butler */)
{
	silence (nframes);
	flush_processor_buffers_locked (nframes);
	return 0;
}

void
Route::flush_processors ()
{
	/* XXX shouldn't really try to take this lock, since
	   this is called from the RT audio thread.
	*/

	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->flush ();
	}
}

#ifdef __clang__
__attribute__((annotate("realtime")))
#endif
bool
Route::apply_processor_changes_rt ()
{
	int emissions = EmitNone;

	if (_pending_meter_point != _meter_point) {
		Glib::Threads::RWLock::WriterLock pwl (_processor_lock, Glib::Threads::TRY_LOCK);
		if (pwl.locked()) {
			/* meters always have buffers for 'processor_max_streams'
			 * they can be re-positioned without re-allocation */
			if (set_meter_point_unlocked()) {
				emissions |= EmitMeterChanged | EmitMeterVisibilityChange;;
			} else {
				emissions |= EmitMeterChanged;
			}
		}
	}

	bool changed = false;

	if (g_atomic_int_get (&_pending_process_reorder)) {
		Glib::Threads::RWLock::WriterLock pwl (_processor_lock, Glib::Threads::TRY_LOCK);
		if (pwl.locked()) {
			apply_processor_order (_pending_processor_order);
			setup_invisible_processors ();
			changed = true;
			g_atomic_int_set (&_pending_process_reorder, 0);
			emissions |= EmitRtProcessorChange;
		}
	}
	if (changed) {
		set_processor_positions ();
	}
	if (emissions != 0) {
		g_atomic_int_set (&_pending_signals, emissions);
		return true;
	}
	return (!selfdestruct_sequence.empty ());
}

void
Route::emit_pending_signals ()
{
	int sig = g_atomic_int_and (&_pending_signals, 0);
	if (sig & EmitMeterChanged) {
		_meter->emit_configuration_changed();
		meter_change (); /* EMIT SIGNAL */
		if (sig & EmitMeterVisibilityChange) {
		processors_changed (RouteProcessorChange (RouteProcessorChange::MeterPointChange, true)); /* EMIT SIGNAL */
		} else {
		processors_changed (RouteProcessorChange (RouteProcessorChange::MeterPointChange, false)); /* EMIT SIGNAL */
		}
	}
	if (sig & EmitRtProcessorChange) {
		processors_changed (RouteProcessorChange (RouteProcessorChange::RealTimeChange)); /* EMIT SIGNAL */
	}

	/* this would be a job for the butler.
	 * Conceptually we should not take processe/processor locks here.
	 * OTOH its more efficient (less overhead for summoning the butler and
	 * telling her what do do) and signal emission is called
	 * directly after the process callback, which decreases the chance
	 * of x-runs when taking the locks.
	 */
	while (!selfdestruct_sequence.empty ()) {
		Glib::Threads::Mutex::Lock lx (selfdestruct_lock);
		if (selfdestruct_sequence.empty ()) { break; } // re-check with lock
		boost::shared_ptr<Processor> proc = selfdestruct_sequence.back ().lock ();
		selfdestruct_sequence.pop_back ();
		lx.release ();
		if (proc) {
			remove_processor (proc);
		}
	}
}

void
Route::set_meter_point (MeterPoint p, bool force)
{
	if (_pending_meter_point == p && !force) {
		return;
	}

	if (force || !AudioEngine::instance()->running()) {
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);
		_pending_meter_point = p;
		_meter->emit_configuration_changed();
		meter_change (); /* EMIT SIGNAL */
		if (set_meter_point_unlocked()) {
			processors_changed (RouteProcessorChange (RouteProcessorChange::MeterPointChange, true)); /* EMIT SIGNAL */
		} else {
			processors_changed (RouteProcessorChange (RouteProcessorChange::MeterPointChange, false)); /* EMIT SIGNAL */
		}
	} else {
		_pending_meter_point = p;
	}
}


#ifdef __clang__
__attribute__((annotate("realtime")))
#endif
bool
Route::set_meter_point_unlocked ()
{
#ifndef NDEBUG
	/* Caller must hold process and processor write lock */
	assert (!AudioEngine::instance()->process_lock().trylock());
	Glib::Threads::RWLock::WriterLock lm (_processor_lock, Glib::Threads::TRY_LOCK);
	assert (!lm.locked ());
#endif

	_meter_point = _pending_meter_point;

	bool meter_was_visible_to_user = _meter->display_to_user ();

	if (!_custom_meter_position_noted) {
		maybe_note_meter_position ();
	}

	if (_meter_point != MeterCustom) {

		_meter->set_display_to_user (false);

		setup_invisible_processors ();

	} else {
		_meter->set_display_to_user (true);

		/* If we have a previous position for the custom meter, try to put it there */
		boost::shared_ptr<Processor> after = _processor_after_last_custom_meter.lock ();
		if (after) {
			ProcessorList::iterator i = find (_processors.begin(), _processors.end(), after);
			if (i != _processors.end ()) {
				_processors.remove (_meter);
				_processors.insert (i, _meter);
			}
		} else {// at end, right before the mains_out/panner
			_processors.remove (_meter);
			ProcessorList::iterator main = _processors.end();
			_processors.insert (--main, _meter);
		}
	}

	/* Set up the meter for its new position */

	ProcessorList::iterator loc = find (_processors.begin(), _processors.end(), _meter);

	ChanCount m_in;

	if (loc == _processors.begin()) {
		m_in = _input->n_ports();
	} else {
		ProcessorList::iterator before = loc;
		--before;
		m_in = (*before)->output_streams ();
	}

	_meter->reflect_inputs (m_in);

	/* we do not need to reconfigure the processors, because the meter
	   (a) is always ready to handle processor_max_streams
	   (b) is always an N-in/N-out processor, and thus moving
	   it doesn't require any changes to the other processors.
	*/

	/* these should really be done after releasing the lock
	 * but all those signals are subscribed to with gui_thread()
	 * so we're safe.
	 */
	 return (_meter->display_to_user() != meter_was_visible_to_user);
}

void
Route::listen_position_changed ()
{
	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);
		ProcessorState pstate (this);

		if (configure_processors_unlocked (0, &lm)) {
			DEBUG_TRACE (DEBUG::Processors, "---- CONFIGURATION FAILED.\n");
			pstate.restore ();
			configure_processors_unlocked (0, &lm); // it worked before we tried to add it ...
			return;
		}
	}

	processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
	_session.set_dirty ();
}

boost::shared_ptr<CapturingProcessor>
Route::add_export_point()
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	if (!_capturing_processor) {
		lm.release();
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::WriterLock lw (_processor_lock);

		// this aligns all tracks; but not tracks + busses
		assert (_session.worst_track_latency () >= _initial_delay);
		_capturing_processor.reset (new CapturingProcessor (_session, _session.worst_track_latency () - _initial_delay));
		_capturing_processor->activate ();

		configure_processors_unlocked (0, &lw);

	}

	return _capturing_processor;
}

framecnt_t
Route::update_signal_latency ()
{
	framecnt_t l = _output->user_latency();
	framecnt_t lamp = 0;
	bool before_amp = true;
	framecnt_t ltrim = 0;
	bool before_trim = true;

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if ((*i)->active ()) {
			l += (*i)->signal_latency ();
		}
		if ((*i) == _amp) {
			before_amp = false;
		}
		if ((*i) == _trim) {
			before_amp = false;
		}
		if (before_amp) {
			lamp = l;
		}
		if (before_trim) {
			lamp = l;
		}
	}

	DEBUG_TRACE (DEBUG::Latency, string_compose ("%1: internal signal latency = %2\n", _name, l));

	// TODO: (lamp - _signal_latency) to sync to output (read-ahed),  currently _roll_delay shifts this around
	_signal_latency_at_amp_position = lamp;
	_signal_latency_at_trim_position = ltrim;

	if (_signal_latency != l) {
		_signal_latency = l;
		signal_latency_changed (); /* EMIT SIGNAL */
	}

	return _signal_latency;
}

void
Route::set_user_latency (framecnt_t nframes)
{
	_output->set_user_latency (nframes);
	_session.update_latency_compensation ();
}

void
Route::set_latency_compensation (framecnt_t longest_session_latency)
{
	framecnt_t old = _initial_delay;

	if (_signal_latency < longest_session_latency) {
		_initial_delay = longest_session_latency - _signal_latency;
	} else {
		_initial_delay = 0;
	}

	DEBUG_TRACE (DEBUG::Latency, string_compose (
		             "%1: compensate for maximum latency of %2,"
		             "given own latency of %3, using initial delay of %4\n",
		             name(), longest_session_latency, _signal_latency, _initial_delay));

	if (_initial_delay != old) {
		initial_delay_changed (); /* EMIT SIGNAL */
	}

	if (_session.transport_stopped()) {
		_roll_delay = _initial_delay;
	}
}

void
Route::set_block_size (pframes_t nframes)
{
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->set_block_size (nframes);
	}

	_session.ensure_buffers (n_process_buffers ());
}

void
Route::protect_automation ()
{
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i)
		(*i)->protect_automation();
}

/** @param declick 1 to set a pending declick fade-in,
 *                -1 to set a pending declick fade-out
 */
void
Route::set_pending_declick (int declick)
{
	if (_declickable) {
		/* this call is not allowed to turn off a pending declick */
		if (declick) {
			_pending_declick = declick;
		}
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
Route::shift (framepos_t pos, framecnt_t frames)
{
	/* gain automation */
	{
		boost::shared_ptr<AutomationControl> gc = _amp->gain_control();

		XMLNode &before = gc->alist()->get_state ();
		gc->alist()->shift (pos, frames);
		XMLNode &after = gc->alist()->get_state ();
		_session.add_command (new MementoCommand<AutomationList> (*gc->alist().get(), &before, &after));
	}

	/* gain automation */
	{
		boost::shared_ptr<AutomationControl> gc = _trim->gain_control();

		XMLNode &before = gc->alist()->get_state ();
		gc->alist()->shift (pos, frames);
		XMLNode &after = gc->alist()->get_state ();
		_session.add_command (new MementoCommand<AutomationList> (*gc->alist().get(), &before, &after));
	}

	// TODO mute automation ??

	/* pan automation */
	if (_pannable) {
		ControlSet::Controls& c (_pannable->controls());

		for (ControlSet::Controls::const_iterator ci = c.begin(); ci != c.end(); ++ci) {
			boost::shared_ptr<AutomationControl> pc = boost::dynamic_pointer_cast<AutomationControl> (ci->second);
			if (pc) {
				boost::shared_ptr<AutomationList> al = pc->alist();
				XMLNode& before = al->get_state ();
				al->shift (pos, frames);
				XMLNode& after = al->get_state ();
				_session.add_command (new MementoCommand<AutomationList> (*al.get(), &before, &after));
			}
		}
	}

	/* redirect automation */
	{
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
		for (ProcessorList::iterator i = _processors.begin (); i != _processors.end (); ++i) {

			set<Evoral::Parameter> parameters = (*i)->what_can_be_automated();

			for (set<Evoral::Parameter>::const_iterator p = parameters.begin (); p != parameters.end (); ++p) {
				boost::shared_ptr<AutomationControl> ac = (*i)->automation_control (*p);
				if (ac) {
					boost::shared_ptr<AutomationList> al = ac->alist();
					XMLNode &before = al->get_state ();
					al->shift (pos, frames);
					XMLNode &after = al->get_state ();
					_session.add_command (new MementoCommand<AutomationList> (*al.get(), &before, &after));
				}
			}
		}
	}
}

void
Route::set_plugin_state_dir (boost::weak_ptr<Processor> p, const std::string& d)
{
	boost::shared_ptr<Processor> processor (p.lock ());
	boost::shared_ptr<PluginInsert> pi  = boost::dynamic_pointer_cast<PluginInsert> (processor);
	if (!pi) {
		return;
	}
	pi->set_state_dir (d);
}

int
Route::save_as_template (const string& path, const string& name)
{
	std::string state_dir = path.substr (0, path.find_last_of ('.')); // strip template_suffix
	PBD::Unwinder<std::string> uw (_session._template_state_dir, state_dir);

	XMLNode& node (state (false));

	XMLTree tree;

	IO::set_name_in_state (*node.children().front(), name);

	tree.set_root (&node);

	/* return zero on success, non-zero otherwise */
	return !tree.write (path.c_str());
}


bool
Route::set_name (const string& str)
{
	if (str == name()) {
		return true;
	}

	string name = Route::ensure_track_or_route_name (str, _session);
	SessionObject::set_name (name);

	bool ret = (_input->set_name(name) && _output->set_name(name));

	if (ret) {
		/* rename the main outs. Leave other IO processors
		 * with whatever name they already have, because its
		 * just fine as it is (it will not contain the route
		 * name if its a port insert, port send or port return).
		 */

		if (_main_outs) {
			if (_main_outs->set_name (name)) {
				/* XXX returning false here is stupid because
				   we already changed the route name.
				*/
				return false;
			}
		}
	}

	return ret;
}

/** Set the name of a route in an XML description.
 *  @param node XML <Route> node to set the name in.
 *  @param name New name.
 */
void
Route::set_name_in_state (XMLNode& node, string const & name, bool rename_playlist)
{
	node.add_property (X_("name"), name);

	XMLNodeList children = node.children();
	for (XMLNodeIterator i = children.begin(); i != children.end(); ++i) {

		if ((*i)->name() == X_("IO")) {

			IO::set_name_in_state (**i, name);

		} else if ((*i)->name() == X_("Processor")) {

			XMLProperty const * role = (*i)->property (X_("role"));
			if (role && role->value() == X_("Main")) {
				(*i)->add_property (X_("name"), name);
			}

		} else if ((*i)->name() == X_("Diskstream")) {

			if (rename_playlist) {
				(*i)->add_property (X_("playlist"), string_compose ("%1.1", name).c_str());
			}
			(*i)->add_property (X_("name"), name);

		}
	}
}

boost::shared_ptr<Send>
Route::internal_send_for (boost::shared_ptr<const Route> target) const
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		boost::shared_ptr<InternalSend> send;

		if ((send = boost::dynamic_pointer_cast<InternalSend>(*i)) != 0) {
			if (send->target_route() == target) {
				return send;
			}
		}
	}

	return boost::shared_ptr<Send>();
}

void
Route::set_denormal_protection (bool yn)
{
	if (_denormal_protection != yn) {
		_denormal_protection = yn;
		denormal_protection_changed (); /* EMIT SIGNAL */
	}
}

bool
Route::denormal_protection () const
{
	return _denormal_protection;
}

void
Route::set_active (bool yn, void* src)
{
	if (_session.transport_rolling()) {
		return;
	}

	if (_route_group && src != _route_group && _route_group->is_active() && _route_group->is_route_active()) {
		_route_group->foreach_route (boost::bind (&Route::set_active, _1, yn, _route_group));
		return;
	}

	if (_active != yn) {
		_active = yn;
		_input->set_active (yn);
		_output->set_active (yn);
		active_changed (); // EMIT SIGNAL
		_session.set_dirty ();
	}
}

boost::shared_ptr<Pannable>
Route::pannable() const
{
	return _pannable;
}

boost::shared_ptr<Panner>
Route::panner() const
{
	/* may be null ! */
	return _main_outs->panner_shell()->panner();
}

boost::shared_ptr<PannerShell>
Route::panner_shell() const
{
	return _main_outs->panner_shell();
}

boost::shared_ptr<GainControl>
Route::gain_control() const
{
	return _gain_control;
}

boost::shared_ptr<GainControl>
Route::trim_control() const
{
	return _trim_control;
}

boost::shared_ptr<PhaseControl>
Route::phase_control() const
{
	return _phase_control;
}

boost::shared_ptr<AutomationControl>
Route::get_control (const Evoral::Parameter& param)
{
	/* either we own the control or .... */

	boost::shared_ptr<AutomationControl> c = boost::dynamic_pointer_cast<AutomationControl>(control (param));

	if (!c) {

		/* maybe one of our processors does or ... */

		Glib::Threads::RWLock::ReaderLock rm (_processor_lock);
		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
			if ((c = boost::dynamic_pointer_cast<AutomationControl>((*i)->control (param))) != 0) {
				break;
			}
		}
	}

	if (!c) {

		/* nobody does so we'll make a new one */

		c = boost::dynamic_pointer_cast<AutomationControl>(control_factory(param));
		add_control(c);
	}

	return c;
}

boost::shared_ptr<Processor>
Route::nth_plugin (uint32_t n) const
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	ProcessorList::const_iterator i;

	for (i = _processors.begin(); i != _processors.end(); ++i) {
		if (boost::dynamic_pointer_cast<PluginInsert> (*i)) {
			if (n-- == 0) {
				return *i;
			}
		}
	}

	return boost::shared_ptr<Processor> ();
}

boost::shared_ptr<Processor>
Route::nth_send (uint32_t n) const
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	ProcessorList::const_iterator i;

	for (i = _processors.begin(); i != _processors.end(); ++i) {
		if (boost::dynamic_pointer_cast<Send> (*i)) {

			if ((*i)->name().find (_("Monitor")) == 0) {
				/* send to monitor section is not considered
				   to be an accessible send.
				*/
				continue;
			}

			if (n-- == 0) {
				return *i;
			}
		}
	}

	return boost::shared_ptr<Processor> ();
}

bool
Route::has_io_processor_named (const string& name)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	ProcessorList::iterator i;

	for (i = _processors.begin(); i != _processors.end(); ++i) {
		if (boost::dynamic_pointer_cast<Send> (*i) ||
		    boost::dynamic_pointer_cast<PortInsert> (*i)) {
			if ((*i)->name() == name) {
				return true;
			}
		}
	}

	return false;
}

void
Route::set_processor_positions ()
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	bool had_amp = false;
	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->set_pre_fader (!had_amp);
		if (*i == _amp) {
			had_amp = true;
		}
	}
}

/** Called when there is a proposed change to the input port count */
bool
Route::input_port_count_changing (ChanCount to)
{
	list<pair<ChanCount, ChanCount> > c = try_configure_processors (to, 0);
	if (c.empty()) {
		/* The processors cannot be configured with the new input arrangement, so
		   block the change.
		*/
		return true;
	}

	/* The change is ok */
	return false;
}

/** Called when there is a proposed change to the output port count */
bool
Route::output_port_count_changing (ChanCount to)
{
	if (_strict_io && !_in_configure_processors) {
		return true;
	}
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		if (processor_out_streams.get(*t) > to.get(*t)) {
			return true;
		}
	}
	/* The change is ok */
	return false;
}

list<string>
Route::unknown_processors () const
{
	list<string> p;

	if (_session.get_disable_all_loaded_plugins ()) {
		// Do not list "missing plugins" if they are explicitly disabled
		return p;
	}

	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if (boost::dynamic_pointer_cast<UnknownProcessor const> (*i)) {
			p.push_back ((*i)->name ());
		}
	}

	return p;
}


framecnt_t
Route::update_port_latencies (PortSet& from, PortSet& to, bool playback, framecnt_t our_latency) const
{
	/* we assume that all our input ports feed all our output ports. its not
	   universally true, but the alternative is way too corner-case to worry about.
	*/

	LatencyRange all_connections;

	if (from.empty()) {
		all_connections.min = 0;
		all_connections.max = 0;
	} else {
		all_connections.min = ~((pframes_t) 0);
		all_connections.max = 0;

		/* iterate over all "from" ports and determine the latency range for all of their
		   connections to the "outside" (outside of this Route).
		*/

		for (PortSet::iterator p = from.begin(); p != from.end(); ++p) {

			LatencyRange range;

			p->get_connected_latency_range (range, playback);

			all_connections.min = min (all_connections.min, range.min);
			all_connections.max = max (all_connections.max, range.max);
		}
	}

	/* set the "from" port latencies to the max/min range of all their connections */

	for (PortSet::iterator p = from.begin(); p != from.end(); ++p) {
		p->set_private_latency_range (all_connections, playback);
	}

	/* set the ports "in the direction of the flow" to the same value as above plus our own signal latency */

	all_connections.min += our_latency;
	all_connections.max += our_latency;

	for (PortSet::iterator p = to.begin(); p != to.end(); ++p) {
		p->set_private_latency_range (all_connections, playback);
	}

	return all_connections.max;
}

framecnt_t
Route::set_private_port_latencies (bool playback) const
{
	framecnt_t own_latency = 0;

	/* Processor list not protected by lock: MUST BE CALLED FROM PROCESS THREAD
	   OR LATENCY CALLBACK.

	   This is called (early) from the latency callback. It computes the REAL
	   latency associated with each port and stores the result as the "private"
	   latency of the port. A later call to Route::set_public_port_latencies()
	   sets all ports to the same value to reflect the fact that we do latency
	   compensation and so all signals are delayed by the same amount as they
	   flow through ardour.
	*/

	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if ((*i)->active ()) {
			own_latency += (*i)->signal_latency ();
		}
	}

	if (playback) {
		/* playback: propagate latency from "outside the route" to outputs to inputs */
		return update_port_latencies (_output->ports (), _input->ports (), true, own_latency);
	} else {
		/* capture: propagate latency from "outside the route" to inputs to outputs */
		return update_port_latencies (_input->ports (), _output->ports (), false, own_latency);
	}
}

void
Route::set_public_port_latencies (framecnt_t value, bool playback) const
{
	/* this is called to set the JACK-visible port latencies, which take
	   latency compensation into account.
	*/

	LatencyRange range;

	range.min = value;
	range.max = value;

	{
		const PortSet& ports (_input->ports());
		for (PortSet::const_iterator p = ports.begin(); p != ports.end(); ++p) {
			p->set_public_latency_range (range, playback);
		}
	}

	{
		const PortSet& ports (_output->ports());
		for (PortSet::const_iterator p = ports.begin(); p != ports.end(); ++p) {
			p->set_public_latency_range (range, playback);
		}
	}
}

/** Put the invisible processors in the right place in _processors.
 *  Must be called with a writer lock on _processor_lock held.
 */
#ifdef __clang__
__attribute__((annotate("realtime")))
#endif
void
Route::setup_invisible_processors ()
{
#ifndef NDEBUG
	Glib::Threads::RWLock::WriterLock lm (_processor_lock, Glib::Threads::TRY_LOCK);
	assert (!lm.locked ());
#endif

	if (!_main_outs) {
		/* too early to be doing this stuff */
		return;
	}

	/* we'll build this new list here and then use it
	 *
	 * TODO put the ProcessorList is on the stack for RT-safety.
	 */

	ProcessorList new_processors;

	/* find visible processors */

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if ((*i)->display_to_user ()) {
			new_processors.push_back (*i);
		}
	}

	/* find the amp */

	ProcessorList::iterator amp = find (new_processors.begin(), new_processors.end(), _amp);

	if (amp == new_processors.end ()) {
		error << string_compose (_("Amp/Fader on Route '%1' went AWOL. Re-added."), name()) << endmsg;
		new_processors.push_front (_amp);
		amp = find (new_processors.begin(), new_processors.end(), _amp);
	}

	/* and the processor after the amp */

	ProcessorList::iterator after_amp = amp;
	++after_amp;

	/* METER */

	if (_meter) {
		switch (_meter_point) {
		case MeterInput:
			assert (!_meter->display_to_user ());
			new_processors.push_front (_meter);
			break;
		case MeterPreFader:
			assert (!_meter->display_to_user ());
			new_processors.insert (amp, _meter);
			break;
		case MeterPostFader:
			/* do nothing here */
			break;
		case MeterOutput:
			/* do nothing here */
			break;
		case MeterCustom:
			/* the meter is visible, so we don't touch it here */
			break;
		}
	}

	/* MAIN OUTS */

	assert (_main_outs);
	assert (!_main_outs->display_to_user ());
	new_processors.push_back (_main_outs);

	/* iterator for the main outs */

	ProcessorList::iterator main = new_processors.end();
	--main;

	/* OUTPUT METERING */

	if (_meter && (_meter_point == MeterOutput || _meter_point == MeterPostFader)) {
		assert (!_meter->display_to_user ());

		/* add the processor just before or just after the main outs */

		ProcessorList::iterator meter_point = main;

		if (_meter_point == MeterOutput) {
			++meter_point;
		}
		new_processors.insert (meter_point, _meter);
	}

	/* MONITOR SEND */

	if (_monitor_send && !is_monitor ()) {
		assert (!_monitor_send->display_to_user ());
		switch (Config->get_listen_position ()) {
		case PreFaderListen:
			switch (Config->get_pfl_position ()) {
			case PFLFromBeforeProcessors:
				new_processors.push_front (_monitor_send);
				break;
			case PFLFromAfterProcessors:
				new_processors.insert (amp, _monitor_send);
				break;
			}
			_monitor_send->set_can_pan (false);
			break;
		case AfterFaderListen:
			switch (Config->get_afl_position ()) {
			case AFLFromBeforeProcessors:
				new_processors.insert (after_amp, _monitor_send);
				break;
			case AFLFromAfterProcessors:
				new_processors.insert (new_processors.end(), _monitor_send);
				break;
			}
			_monitor_send->set_can_pan (true);
			break;
		}
	}

#if 0 // not used - just yet
	if (!is_master() && !is_monitor() && !is_auditioner()) {
		new_processors.push_front (_delayline);
	}
#endif

	/* MONITOR CONTROL */

	if (_monitor_control && is_monitor ()) {
		assert (!_monitor_control->display_to_user ());
		new_processors.insert (amp, _monitor_control);
	}

	/* TRIM CONTROL */

	if (_trim && _trim->active()) {
		assert (!_trim->display_to_user ());
		new_processors.push_front (_trim);
	}

	/* INTERNAL RETURN */

	/* doing this here means that any monitor control will come after
	   the return and trim.
	*/

	if (_intreturn) {
		assert (!_intreturn->display_to_user ());
		new_processors.push_front (_intreturn);
	}

	/* EXPORT PROCESSOR */

	if (_capturing_processor) {
		assert (!_capturing_processor->display_to_user ());
		new_processors.push_front (_capturing_processor);
	}

	_processors = new_processors;

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if (!(*i)->display_to_user () && !(*i)->enabled () && (*i) != _monitor_send) {
			(*i)->enable (true);
		}
	}

	DEBUG_TRACE (DEBUG::Processors, string_compose ("%1: setup_invisible_processors\n", _name));
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		DEBUG_TRACE (DEBUG::Processors, string_compose ("\t%1\n", (*i)->name ()));
	}
}

void
Route::unpan ()
{
	Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
	Glib::Threads::RWLock::ReaderLock lp (_processor_lock);

	_pannable.reset ();

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		boost::shared_ptr<Delivery> d = boost::dynamic_pointer_cast<Delivery>(*i);
		if (d) {
			d->unpan ();
		}
	}
}

/** If the meter point is `Custom', make a note of where the meter is.
 *  This is so that if the meter point is subsequently set to something else,
 *  and then back to custom, we can put the meter back where it was last time
 *  custom was enabled.
 *
 *  Must be called with the _processor_lock held.
 */
void
Route::maybe_note_meter_position ()
{
	if (_meter_point != MeterCustom) {
		return;
	}

	_custom_meter_position_noted = true;
	/* custom meter points range from after trim to before panner/main_outs
	 * this is a limitation by the current processor UI
	 */
	bool seen_trim = false;
	_processor_after_last_custom_meter.reset();
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if ((*i) == _trim) {
			seen_trim = true;
		}
		if ((*i) == _main_outs) {
			_processor_after_last_custom_meter = *i;
			break;
		}
		if (boost::dynamic_pointer_cast<PeakMeter> (*i)) {
			if (!seen_trim) {
				_processor_after_last_custom_meter = _trim;
			} else {
				ProcessorList::iterator j = i;
				++j;
				assert(j != _processors.end ()); // main_outs should be before
				_processor_after_last_custom_meter = *j;
			}
			break;
		}
	}
	assert(_processor_after_last_custom_meter.lock());
}

boost::shared_ptr<Processor>
Route::processor_by_id (PBD::ID id) const
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if ((*i)->id() == id) {
			return *i;
		}
	}

	return boost::shared_ptr<Processor> ();
}

/** @return the monitoring state, or in other words what data we are pushing
 *  into the route (data from the inputs, data from disk or silence)
 */
MonitorState
Route::monitoring_state () const
{
	return MonitoringInput;
}

/** @return what we should be metering; either the data coming from the input
 *  IO or the data that is flowing through the route.
 */
MeterState
Route::metering_state () const
{
	return MeteringRoute;
}

bool
Route::has_external_redirects () const
{
	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {

		/* ignore inactive processors and obviously ignore the main
		 * outs since everything has them and we don't care.
		 */

		if ((*i)->active() && (*i) != _main_outs && (*i)->does_routing()) {
			return true;;
		}
	}

	return false;
}

boost::shared_ptr<Processor>
Route::the_instrument () const
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	return the_instrument_unlocked ();
}

boost::shared_ptr<Processor>
Route::the_instrument_unlocked () const
{
	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert>(*i);
		if (pi && pi->plugin ()->get_info ()->is_instrument ()) {
			return (*i);
		}
	}
	return boost::shared_ptr<Processor>();
}



void
Route::non_realtime_locate (framepos_t pos)
{
	if (_pannable) {
		_pannable->transport_located (pos);
	}

	if (_delayline.get()) {
		_delayline.get()->flush();
	}

	{
		//Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
			(*i)->transport_located (pos);
		}
	}
	_roll_delay = _initial_delay;
}

void
Route::fill_buffers_with_input (BufferSet& bufs, boost::shared_ptr<IO> io, pframes_t nframes)
{
	size_t n_buffers;
	size_t i;

	/* MIDI
	 *
	 * We don't currently mix MIDI input together, so we don't need the
	 * complex logic of the audio case.
	 */

	n_buffers = bufs.count().n_midi ();

	for (i = 0; i < n_buffers; ++i) {

		boost::shared_ptr<MidiPort> source_port = io->midi (i);
		MidiBuffer& buf (bufs.get_midi (i));

		if (source_port) {
			buf.copy (source_port->get_midi_buffer(nframes));
		} else {
			buf.silence (nframes);
		}
	}

	/* AUDIO */

	n_buffers = bufs.count().n_audio();

	size_t n_ports = io->n_ports().n_audio();
	float scaling = 1.0f;

	if (n_ports > n_buffers) {
		scaling = ((float) n_buffers) / n_ports;
	}

	for (i = 0; i < n_ports; ++i) {

		/* if there are more ports than buffers, map them onto buffers
		 * in a round-robin fashion
		 */

		boost::shared_ptr<AudioPort> source_port = io->audio (i);
		AudioBuffer& buf (bufs.get_audio (i%n_buffers));


		if (i < n_buffers) {

			/* first time through just copy a channel into
			   the output buffer.
			*/

			buf.read_from (source_port->get_audio_buffer (nframes), nframes);

			if (scaling != 1.0f) {
				buf.apply_gain (scaling, nframes);
			}

		} else {

			/* on subsequent times around, merge data from
			 * the port with what is already there
			 */

			if (scaling != 1.0f) {
				buf.accumulate_with_gain_from (source_port->get_audio_buffer (nframes), nframes, 0, scaling);
			} else {
				buf.accumulate_from (source_port->get_audio_buffer (nframes), nframes);
			}
		}
	}

	/* silence any remaining buffers */

	for (; i < n_buffers; ++i) {
		AudioBuffer& buf (bufs.get_audio (i));
		buf.silence (nframes);
	}

	/* establish the initial setup of the buffer set, reflecting what was
	   copied into it. unless, of course, we are the auditioner, in which
	   case nothing was fed into it from the inputs at all.
	*/

	if (!is_auditioner()) {
		bufs.set_count (io->n_ports());
	}
}

boost::shared_ptr<AutomationControl>
Route::pan_azimuth_control() const
{
#ifdef MIXBUS
	boost::shared_ptr<ARDOUR::PluginInsert> plug = ch_post();
	if (!plug) {
		return boost::shared_ptr<AutomationControl>();
	}
	const uint32_t port_channel_post_pan = 2; // gtk2_ardour/mixbus_ports.h
	return boost::dynamic_pointer_cast<ARDOUR::AutomationControl> (plug->control (Evoral::Parameter (ARDOUR::PluginAutomation, 0, port_channel_post_pan)));
#else
	if (!_pannable || !panner()) {
		return boost::shared_ptr<AutomationControl>();
	}
	return _pannable->pan_azimuth_control;
#endif
}

boost::shared_ptr<AutomationControl>
Route::pan_elevation_control() const
{
	if (Profile->get_mixbus() || !_pannable || !panner()) {
		return boost::shared_ptr<AutomationControl>();
	}

	set<Evoral::Parameter> c = panner()->what_can_be_automated ();

	if (c.find (PanElevationAutomation) != c.end()) {
		return _pannable->pan_elevation_control;
	} else {
		return boost::shared_ptr<AutomationControl>();
	}
}
boost::shared_ptr<AutomationControl>
Route::pan_width_control() const
{
	if (Profile->get_mixbus() || !_pannable || !panner()) {
		return boost::shared_ptr<AutomationControl>();
	}

	set<Evoral::Parameter> c = panner()->what_can_be_automated ();

	if (c.find (PanWidthAutomation) != c.end()) {
		return _pannable->pan_width_control;
	} else {
		return boost::shared_ptr<AutomationControl>();
	}
}
boost::shared_ptr<AutomationControl>
Route::pan_frontback_control() const
{
	if (Profile->get_mixbus() || !_pannable || !panner()) {
		return boost::shared_ptr<AutomationControl>();
	}

	set<Evoral::Parameter> c = panner()->what_can_be_automated ();

	if (c.find (PanFrontBackAutomation) != c.end()) {
		return _pannable->pan_frontback_control;
	} else {
		return boost::shared_ptr<AutomationControl>();
	}
}
boost::shared_ptr<AutomationControl>
Route::pan_lfe_control() const
{
	if (Profile->get_mixbus() || !_pannable || !panner()) {
		return boost::shared_ptr<AutomationControl>();
	}

	set<Evoral::Parameter> c = panner()->what_can_be_automated ();

	if (c.find (PanLFEAutomation) != c.end()) {
		return _pannable->pan_lfe_control;
	} else {
		return boost::shared_ptr<AutomationControl>();
	}
}

uint32_t
Route::eq_band_cnt () const
{
	if (Profile->get_mixbus()) {
		return 3;
	} else {
		/* Ardour has no well-known EQ object */
		return 0;
	}
}

boost::shared_ptr<AutomationControl>
Route::eq_gain_controllable (uint32_t band) const
{
#ifdef MIXBUS
	boost::shared_ptr<PluginInsert> eq = ch_eq();

	if (!eq) {
		return boost::shared_ptr<AutomationControl>();
	}

	uint32_t port_number;
	switch (band) {
	case 0:
		if (is_master() || mixbus()) {
			port_number = 4;
		} else {
			port_number = 8;
		}
		break;
	case 1:
		if (is_master() || mixbus()) {
			port_number = 3;
		} else {
			port_number = 6;
		}
		break;
	case 2:
		if (is_master() || mixbus()) {
			port_number = 2;
		} else {
			port_number = 4;
		}
		break;
	default:
		return boost::shared_ptr<AutomationControl>();
	}

	return boost::dynamic_pointer_cast<ARDOUR::AutomationControl> (eq->control (Evoral::Parameter (ARDOUR::PluginAutomation, 0, port_number)));
#else
	return boost::shared_ptr<AutomationControl>();
#endif
}
boost::shared_ptr<AutomationControl>
Route::eq_freq_controllable (uint32_t band) const
{
#ifdef MIXBUS

	if (mixbus() || is_master()) {
		/* no frequency controls for mixbusses or master */
		return boost::shared_ptr<AutomationControl>();
	}

	boost::shared_ptr<PluginInsert> eq = ch_eq();

	if (!eq) {
		return boost::shared_ptr<AutomationControl>();
	}

	uint32_t port_number;
	switch (band) {
	case 0:
		port_number = 7;
		break;
	case 1:
		port_number = 5;
		break;
	case 2:
		port_number = 3;
		break;
	default:
		return boost::shared_ptr<AutomationControl>();
	}

	return boost::dynamic_pointer_cast<ARDOUR::AutomationControl> (eq->control (Evoral::Parameter (ARDOUR::PluginAutomation, 0, port_number)));
#else
	return boost::shared_ptr<AutomationControl>();
#endif
}

boost::shared_ptr<AutomationControl>
Route::eq_q_controllable (uint32_t band) const
{
	return boost::shared_ptr<AutomationControl>();
}

boost::shared_ptr<AutomationControl>
Route::eq_shape_controllable (uint32_t band) const
{
	return boost::shared_ptr<AutomationControl>();
}

boost::shared_ptr<AutomationControl>
Route::eq_enable_controllable () const
{
#ifdef MIXBUS
	boost::shared_ptr<PluginInsert> eq = ch_eq();

	if (!eq) {
		return boost::shared_ptr<AutomationControl>();
	}

	return boost::dynamic_pointer_cast<ARDOUR::AutomationControl> (eq->control (Evoral::Parameter (ARDOUR::PluginAutomation, 0, 1)));
#else
	return boost::shared_ptr<AutomationControl>();
#endif
}

boost::shared_ptr<AutomationControl>
Route::eq_hpf_controllable () const
{
#ifdef MIXBUS
	boost::shared_ptr<PluginInsert> eq = ch_eq();

	if (!eq) {
		return boost::shared_ptr<AutomationControl>();
	}

	return boost::dynamic_pointer_cast<ARDOUR::AutomationControl> (eq->control (Evoral::Parameter (ARDOUR::PluginAutomation, 0, 2)));
#else
	return boost::shared_ptr<AutomationControl>();
#endif
}

string
Route::eq_band_name (uint32_t band) const
{
	if (Profile->get_mixbus()) {
		switch (band) {
		case 0:
			return _("lo");
		case 1:
			return _("mid");
		case 2:
			return _("hi");
		default:
			return string();
		}
	} else {
		return string ();
	}
}

boost::shared_ptr<AutomationControl>
Route::comp_enable_controllable () const
{
#ifdef MIXBUS
	boost::shared_ptr<PluginInsert> comp = ch_comp();

	if (!comp) {
		return boost::shared_ptr<AutomationControl>();
	}

	return boost::dynamic_pointer_cast<ARDOUR::AutomationControl> (comp->control (Evoral::Parameter (ARDOUR::PluginAutomation, 0, 1)));
#else
	return boost::shared_ptr<AutomationControl>();
#endif
}
boost::shared_ptr<AutomationControl>
Route::comp_threshold_controllable () const
{
#ifdef MIXBUS
	boost::shared_ptr<PluginInsert> comp = ch_comp();

	if (!comp) {
		return boost::shared_ptr<AutomationControl>();
	}

	return boost::dynamic_pointer_cast<ARDOUR::AutomationControl> (comp->control (Evoral::Parameter (ARDOUR::PluginAutomation, 0, 2)));

#else
	return boost::shared_ptr<AutomationControl>();
#endif
}
boost::shared_ptr<AutomationControl>
Route::comp_speed_controllable () const
{
#ifdef MIXBUS
	boost::shared_ptr<PluginInsert> comp = ch_comp();

	if (!comp) {
		return boost::shared_ptr<AutomationControl>();
	}

	return boost::dynamic_pointer_cast<ARDOUR::AutomationControl> (comp->control (Evoral::Parameter (ARDOUR::PluginAutomation, 0, 3)));
#else
	return boost::shared_ptr<AutomationControl>();
#endif
}
boost::shared_ptr<AutomationControl>
Route::comp_mode_controllable () const
{
#ifdef MIXBUS
	boost::shared_ptr<PluginInsert> comp = ch_comp();

	if (!comp) {
		return boost::shared_ptr<AutomationControl>();
	}

	return boost::dynamic_pointer_cast<ARDOUR::AutomationControl> (comp->control (Evoral::Parameter (ARDOUR::PluginAutomation, 0, 4)));
#else
	return boost::shared_ptr<AutomationControl>();
#endif
}
boost::shared_ptr<AutomationControl>
Route::comp_makeup_controllable () const
{
#ifdef MIXBUS
	boost::shared_ptr<PluginInsert> comp = ch_comp();

	if (!comp) {
		return boost::shared_ptr<AutomationControl>();
	}

	return boost::dynamic_pointer_cast<ARDOUR::AutomationControl> (comp->control (Evoral::Parameter (ARDOUR::PluginAutomation, 0, 5)));
#else
	return boost::shared_ptr<AutomationControl>();
#endif
}
boost::shared_ptr<AutomationControl>
Route::comp_redux_controllable () const
{
#ifdef MIXBUS
	boost::shared_ptr<PluginInsert> comp = ch_comp();

	if (!comp) {
		return boost::shared_ptr<AutomationControl>();
	}

	return boost::dynamic_pointer_cast<ARDOUR::AutomationControl> (comp->control (Evoral::Parameter (ARDOUR::PluginAutomation, 0, 6)));
#else
	return boost::shared_ptr<AutomationControl>();
#endif
}

string
Route::comp_mode_name (uint32_t mode) const
{
#ifdef MIXBUS
	switch (mode) {
	case 0:
		return _("Leveler");
	case 1:
		return _("Compressor");
	case 2:
		return _("Limiter");
	case 3:
		return mixbus() ? _("Sidechain") : _("Limiter");
	}

	return _("???");
#else
	return _("???");
#endif
}

string
Route::comp_speed_name (uint32_t mode) const
{
#ifdef MIXBUS
	switch (mode) {
	case 0:
		return _("Attk");
	case 1:
		return _("Ratio");
	case 2:
	case 3:
		return _("Rels");
	}
	return _("???");
#else
	return _("???");
#endif
}

boost::shared_ptr<AutomationControl>
Route::send_level_controllable (uint32_t n) const
{
#ifdef  MIXBUS
	boost::shared_ptr<ARDOUR::PluginInsert> plug = ch_post();
	if (!plug) {
		return boost::shared_ptr<AutomationControl>();
	}

	if (n >= 8) {
		/* no such bus */
		return boost::shared_ptr<AutomationControl>();
	}

	const uint32_t port_id = port_channel_post_aux1_level + (2*n); // gtk2_ardour/mixbus_ports.h
	return boost::dynamic_pointer_cast<ARDOUR::AutomationControl> (plug->control (Evoral::Parameter (ARDOUR::PluginAutomation, 0, port_id)));
#else
	boost::shared_ptr<Send> s = boost::dynamic_pointer_cast<Send>(nth_send (n));
	if (!s) {
		return boost::shared_ptr<AutomationControl>();
	}
	return s->gain_control ();
#endif
}

boost::shared_ptr<AutomationControl>
Route::send_enable_controllable (uint32_t n) const
{
#ifdef  MIXBUS
	boost::shared_ptr<ARDOUR::PluginInsert> plug = ch_post();
	if (!plug) {
		return boost::shared_ptr<AutomationControl>();
	}

	if (n >= 8) {
		/* no such bus */
		return boost::shared_ptr<AutomationControl>();
	}

	const uint32_t port_id = port_channel_post_aux1_asgn + (2*n); // gtk2_ardour/mixbus_ports.h
	return boost::dynamic_pointer_cast<ARDOUR::AutomationControl> (plug->control (Evoral::Parameter (ARDOUR::PluginAutomation, 0, port_id)));
#else
	/* although Ardour sends have enable/disable as part of the Processor
	   API, it is not exposed as a controllable.

	   XXX: we should fix this.
	*/
	return boost::shared_ptr<AutomationControl>();
#endif
}

string
Route::send_name (uint32_t n) const
{
#ifdef MIXBUS
	if (n >= 8) {
		return string();
	}
	boost::shared_ptr<Route> r = _session.get_mixbus (n);
	assert (r);
	return r->name();
#else
	boost::shared_ptr<Processor> p = nth_send (n);
	if (p) {
		return p->name();
	} else {
		return string();
	}
#endif
}

boost::shared_ptr<AutomationControl>
Route::master_send_enable_controllable () const
{
#ifdef  MIXBUS
	boost::shared_ptr<ARDOUR::PluginInsert> plug = ch_post();
	if (!plug) {
		return boost::shared_ptr<AutomationControl>();
	}
	return boost::dynamic_pointer_cast<ARDOUR::AutomationControl> (plug->control (Evoral::Parameter (ARDOUR::PluginAutomation, 0, port_channel_post_mstr_assign)));
#else
	return boost::shared_ptr<AutomationControl>();
#endif
}

bool
Route::slaved () const
{
	if (!_gain_control) {
		return false;
	}
	/* just test one particular control, not all of them */
	return _gain_control->slaved ();
}

bool
Route::slaved_to (boost::shared_ptr<VCA> vca) const
{
	if (!vca || !_gain_control) {
		return false;
	}

	/* just test one particular control, not all of them */

	return _gain_control->slaved_to (vca->gain_control());
}

bool
Route::muted_by_others_soloing () const
{
	if (!can_be_muted_by_others ()) {
		return false;
	}

	return  _session.soloing() && !_solo_control->soloed() && !_solo_isolate_control->solo_isolated();
}

void
Route::clear_all_solo_state ()
{
	_solo_control->clear_all_solo_state ();
}
