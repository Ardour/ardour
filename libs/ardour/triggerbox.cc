#include <iostream>
#include <cstdlib>

#include <glibmm.h>

#include <rubberband/RubberBandStretcher.h>

#include "pbd/basename.h"
#include "pbd/compose.h"
#include "pbd/failed_constructor.h"

#include "temporal/tempo.h"

#include "ardour/audioregion.h"
#include "ardour/audio_buffer.h"
#include "ardour/debug.h"
#include "ardour/midi_buffer.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/session_object.h"
#include "ardour/sndfilesource.h"
#include "ardour/triggerbox.h"

using namespace PBD;
using namespace ARDOUR;
using std::string;
using std::cerr;
using std::endl;

namespace ARDOUR {
	namespace Properties {
		PBD::PropertyDescriptor<bool> running;
	}
}

Trigger::Trigger (size_t n, TriggerBox& b)
	: _box (b)
	, _state (Stopped)
	, _requested_state (None)
	, _bang (0)
	, _unbang (0)
	, _index (n)
	, _next_trigger (-1)
	, _launch_style (Toggle)
	, _follow_action { NextTrigger, Stop }
	, _follow_action_probability (100)
	, _quantization (Temporal::BBT_Offset (0, 1, 0))
	, _legato (false)
{
}

void
Trigger::set_name (std::string const & str)
{
	_name = str;
}

void
Trigger::bang ()
{
	_bang.fetch_add (1);
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("bang on %1\n", _index));
}

void
Trigger::unbang ()
{
	_unbang.fetch_add (1);
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("un-bang on %1\n", _index));
}

void
Trigger::set_follow_action (FollowAction f, size_t n)
{
	assert (n < 2);
	_follow_action[n] = f;
}

void
Trigger::set_launch_style (LaunchStyle l)
{
	_launch_style = l;

	set_usable_length ();
}

XMLNode&
Trigger::get_state (void)
{
	XMLNode* node = new XMLNode (X_("Trigger"));
	return *node;
}

int
Trigger::set_state (const XMLNode&, int version)
{
	return 0;
}

void
Trigger::set_follow_action_probability (int n)
{
	n = std::min (100, n);
	n = std::max (0, n);

	_follow_action_probability = n;
}

void
Trigger::set_quantization (Temporal::BBT_Offset const & q)
{
	_quantization = q;
	set_usable_length ();
}

void
Trigger::set_region_internal (boost::shared_ptr<Region> r)
{
	_region = r;
}

Temporal::BBT_Offset
Trigger::quantization () const
{
	return _quantization;
}

void
Trigger::stop ()
{
	_next_trigger = -1;
	request_state (Stopped);
}

void
Trigger::start ()
{
	request_state (Running);
}

void
Trigger::request_state (State s)
{
	_requested_state.store (s);
}

void
Trigger::process_state_requests ()
{
	State new_state = _requested_state.exchange (None);

	if (new_state != None && new_state != _state) {

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 requested state %2\n", index(), enum_2_string (new_state)));

		switch (new_state) {
		case Stopped:
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 %2 -> %3\n", index(), enum_2_string (_state), enum_2_string (WaitingToStop)));
			_state = WaitingToStop;
			PropertyChanged (ARDOUR::Properties::running);
			break;
		case Running:
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 %2 -> %3\n", index(), enum_2_string (_state), enum_2_string (WaitingToStart)));
			_state = WaitingToStart;
			PropertyChanged (ARDOUR::Properties::running);
			break;
		default:
			break;
		}
	}

	/* now check bangs/unbangs */

	int x;

	while ((x = _bang.load ())) {

		_bang.fetch_sub (1);

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 handling bang with state = %2\n", index(), enum_2_string (_state)));

		switch (_state) {
		case None:
			abort ();
			break;

		case Running:
			switch (launch_style()) {
			case OneShot:
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 %2 -> %3\n", index(), enum_2_string (Running), enum_2_string (WaitingForRetrigger)));
				_state = WaitingForRetrigger;
				PropertyChanged (ARDOUR::Properties::running);
				break;
			case Gate:
			case Toggle:
			case Repeat:
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 %2 -> %3\n", index(), enum_2_string (Running), enum_2_string (Stopped)));
				_state = WaitingToStop;
				PropertyChanged (ARDOUR::Properties::running);
			}
			break;

		case Stopped:
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 %2 -> %3\n", index(), enum_2_string (Stopped), enum_2_string (WaitingToStart)));
			_state = WaitingToStart;
			PropertyChanged (ARDOUR::Properties::running);
			break;

		case WaitingToStart:
		case WaitingToStop:
		case WaitingForRetrigger:
		case Stopping:
			break;
		}
	}


	while ((x = _unbang.load ())) {

		_unbang.fetch_sub (1);

		if (_launch_style == Gate || _launch_style == Repeat) {
			switch (_state) {
			case Running:
				_state = WaitingToStop;
				PropertyChanged (ARDOUR::Properties::running);
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 unbanged, now in WaitingToStop\n", index()));
				break;
			default:
				/* didn't even get started */
				_state = Stopped;
				PropertyChanged (ARDOUR::Properties::running);
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 unbanged, never started, now stopped\n", index()));
			}
		}
	}
}

Trigger::RunType
Trigger::maybe_compute_next_transition (Temporal::Beats const & start, Temporal::Beats const & end)
{
	/* In these states, we are not waiting for a transition */

	switch (_state) {
	case Stopped:
		return RunNone;
	case Running:
		return RunAll;
	case Stopping:
		return RunAll;
	default:
		break;
	}

	timepos_t ev_time (Temporal::BeatTime);

	if (_quantization.bars == 0) {
		ev_time = timepos_t (start.snap_to (Temporal::Beats (_quantization.beats, _quantization.ticks)));
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 quantized with %5 start at %2, sb %3 eb %4\n", index(), ev_time.beats(), start, end, _quantization));
	} else {
		/* XXX not yet handled */
	}

	if (ev_time.beats() >= start && ev_time < end) {

		bang_samples = ev_time.samples();
		bang_beats = ev_time.beats ();

		if (_state == WaitingToStop) {
			_state = Stopping;
			PropertyChanged (ARDOUR::Properties::running);
			return RunEnd;
		} else if (_state == WaitingToStart) {
			retrigger ();
			_state = Running;
			PropertyChanged (ARDOUR::Properties::running);
			return RunStart;
		} else if (_state == WaitingForRetrigger) {
			retrigger ();
			_state = Running;
			PropertyChanged (ARDOUR::Properties::running);
			return RunAll;
		}
	} else {
		if (_state == WaitingForRetrigger || _state == WaitingToStop) {
			/* retrigger time has not been reached, just continue
			   to play normally until then.
			*/
			return RunAll;
		}
	}

	return RunNone;
}


void
Trigger::set_next_trigger (int n)
{
	_next_trigger = n;
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 : set %2 as next\n", index(), n));
}

/*--------------------*/

AudioTrigger::AudioTrigger (size_t n, TriggerBox& b)
	: Trigger (n, b)
	, data (0)
	, read_index (0)
	, data_length (0)
	, _start_offset (0)
	, _legato_offset (0)
	, usable_length (0)
	, last_sample (0)
{
}

AudioTrigger::~AudioTrigger ()
{
	for (std::vector<Sample*>::iterator d = data.begin(); d != data.end(); ++d) {
		delete *d;
	}
}

void
AudioTrigger::set_start (timepos_t const & s)
{
	_start_offset = s.samples ();
}

void
AudioTrigger::set_end (timepos_t const & e)
{
	set_length (timepos_t (e.samples() - _start_offset));
}

void
AudioTrigger::set_legato_offset (timepos_t const & offset)
{
	_legato_offset = offset.samples();
}

timepos_t
AudioTrigger::current_pos() const
{
	return timepos_t (read_index);
}

timepos_t
AudioTrigger::end() const
{
	return timepos_t (_start_offset + usable_length);
}

void
AudioTrigger::set_length (timepos_t const & newlen)
{
	using namespace RubberBand;
	using namespace Temporal;

	if (!_region) {
		return;
	}

	boost::shared_ptr<AudioRegion> ar (boost::dynamic_pointer_cast<AudioRegion> (_region));

	/* load raw data */

	load_data (ar);

	if (newlen == timepos_t (_region->length_samples())) {
		/* no stretch required */
		return;
	}

	/* offline stretch */

	/* study */

	const uint32_t nchans = ar->n_channels();

	RubberBandStretcher::Options options = RubberBandStretcher::Option (RubberBandStretcher::OptionProcessOffline|RubberBandStretcher::OptionStretchPrecise);
	RubberBandStretcher stretcher (_box.session().sample_rate(), nchans, options, 1.0, 1.0);

	/* Compute stretch ratio */

	double new_ratio;

	if (newlen.time_domain() == AudioTime) {
		new_ratio = (double) newlen.samples() / data_length;
	} else {
		/* XXX what to use for position ??? */
		timecnt_t l (newlen, timepos_t (AudioTime));
		const timecnt_t dur = TempoMap::use()->convert_duration (l, timepos_t (0), AudioTime);
		new_ratio = (double) dur.samples() / data_length;
	}

	stretcher.setTimeRatio (new_ratio);

	const samplecnt_t expected_length = ceil (data_length * new_ratio) + 16; /* extra space for safety */
	std::vector<Sample*> stretched;

	for (uint32_t n = 0; n < nchans; ++n) {
		stretched.push_back (new Sample[expected_length]);
	}

	/* RB expects array-of-ptr-to-Sample, so set one up */

	std::vector<Sample*> raw(nchans);
	std::vector<Sample*> results(nchans);

	/* study, then process */

	const samplecnt_t block_size = 16384;
	samplecnt_t read = 0;

	stretcher.setDebugLevel (0);
	stretcher.setMaxProcessSize (block_size);
	stretcher.setExpectedInputDuration (data_length);

	while (read < data_length) {

		for (uint32_t n = 0; n < nchans; ++n) {
			raw[n] = data[n] + read;
		}

		samplecnt_t to_read = std::min (block_size, data_length - read);
		read += to_read;

		stretcher.study (&raw[0], to_read, (read >= data_length));
	}

	read = 0;

	samplecnt_t processed = 0;
	samplecnt_t avail;

	while (read < data_length) {

		for (uint32_t n = 0; n < nchans; ++n) {
			raw[n] = data[n] + read;
		}

		samplecnt_t to_read = std::min (block_size, data_length - read);
		read += to_read;

		stretcher.process (&raw[0], to_read, (read >= data_length));

		while ((avail = stretcher.available()) > 0) {

			for (uint32_t n = 0; n < nchans; ++n) {
				results[n] = stretched[n] + processed;
			}

			processed += stretcher.retrieve (&results[0], avail);
		}
	}

	/* collect final chunk of data, possible delayed by thread activity in stretcher */

	while ((avail = stretcher.available()) >= 0) {

		if (avail == 0) {
			Glib::usleep (10000);
			continue;
		}

		for (uint32_t n = 0; n < nchans; ++n) {
			results[n] = stretched[n] + processed;
		}

		processed += stretcher.retrieve (&results[0], avail);
	}

	/* allocate new data buffers */

	drop_data ();
	data = stretched;
	data_length = processed;
	if (!usable_length || usable_length > data_length) {
		usable_length = data_length;
		last_sample = _start_offset + usable_length;
	}
}

void
AudioTrigger::set_usable_length ()
{
	if (!_region) {
		return;
	}

	switch (_launch_style) {
	case Repeat:
		break;
	default:
		usable_length = data_length;
		last_sample = _start_offset + usable_length;
		return;
	}

	if (_quantization == Temporal::BBT_Offset ()) {
		usable_length = data_length;
		last_sample = _start_offset + usable_length;
		return;
	}

	/* XXX MUST HANDLE BAR-LEVEL QUANTIZATION */

	timecnt_t len (Temporal::Beats (_quantization.beats, _quantization.ticks), timepos_t (Temporal::Beats()));
	usable_length = len.samples();
	last_sample = _start_offset + usable_length;
}

timepos_t
AudioTrigger::current_length() const
{
	if (_region) {
		return timepos_t (data_length);
	}
	return timepos_t (Temporal::BeatTime);
}

timepos_t
AudioTrigger::natural_length() const
{
	if (_region) {
		return timepos_t::from_superclock (_region->length().magnitude());
	}
	return timepos_t (Temporal::BeatTime);
}

int
AudioTrigger::set_region (boost::shared_ptr<Region> r)
{
	boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (r);

	if (!ar) {
		return -1;
	}

	set_region_internal (r);

	/* this will load data, but won't stretch it for now */

	set_length (timepos_t::from_superclock (r->length ().magnitude()));

	PropertyChanged (ARDOUR::Properties::name);

	return 0;
}

void
AudioTrigger::drop_data ()
{
	for (uint32_t n = 0; n < data.size(); ++n) {
		delete [] data[n];
	}
	data.clear ();
}

int
AudioTrigger::load_data (boost::shared_ptr<AudioRegion> ar)
{
	const uint32_t nchans = ar->n_channels();

	data_length = ar->length_samples();

	/* if usable length was already set, only adjust it if it is too large */
	if (!usable_length || usable_length > data_length) {
		usable_length = data_length;
		last_sample = _start_offset + usable_length;
	}

	drop_data ();

	try {
		for (uint32_t n = 0; n < nchans; ++n) {
			data.push_back (new Sample[data_length]);
			ar->read (data[n], 0, data_length, n);
		}

		set_name (ar->name());

	} catch (...) {
		drop_data ();
		return -1;
	}

	return 0;
}

void
AudioTrigger::retrigger ()
{
	read_index = _start_offset + _legato_offset;
}

int
AudioTrigger::run (BufferSet& bufs, pframes_t nframes, pframes_t dest_offset, bool first)
{
	boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion>(_region);
	const bool long_enough_to_fade = (nframes >= 64);

	assert (ar);
	assert (active());

	while (nframes) {

		pframes_t this_read = (pframes_t) std::min ((samplecnt_t) nframes, (last_sample - read_index));

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 grab %2 @ %3 dest offset %4\n", index(), this_read, read_index, dest_offset));

		for (size_t chn = 0; chn < ar->n_channels(); ++chn) {

			size_t channel = chn %  data.size();
			Sample* src = data[channel] + read_index;
			AudioBuffer& buf (bufs.get_audio (chn));


			if (first) {
				buf.read_from (src, this_read, dest_offset);
			} else {
				buf.accumulate_from (src, this_read, dest_offset);
			}
		}

		read_index += this_read;

		if (read_index >= last_sample) {

			/* We reached the end */

			if ((_launch_style == Repeat) || ((_next_trigger > 0) && (size_t) _next_trigger == _index)) { /* self repeat */
				nframes -= this_read;
				dest_offset += this_read;
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached end, but set to loop, so retrigger\n", index()));
				retrigger ();
				/* and go around again */
				continue;

			} else {

				if (this_read < nframes) {

					for (size_t chn = 0; chn < ar->n_channels(); ++chn) {
						size_t channel = chn %  data.size();
						AudioBuffer& buf (bufs.get_audio (channel));
						DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 short fill, do silent fill\n", index()));
						buf.silence (nframes - this_read, dest_offset + this_read);
					}
				}
				_state = Stopped;
				PropertyChanged (ARDOUR::Properties::running);
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached end, now stopped\n", index()));
				break;
			}
		}

		nframes -= this_read;
	}

	if (_state == Stopping && long_enough_to_fade) {
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 was stopping, now stopped\n", index()));
		_state = Stopped;
		PropertyChanged (ARDOUR::Properties::running);
	}

	return 0;
}

/**************/

void
Trigger::make_property_quarks ()
{
	Properties::muted.property_id = g_quark_from_static_string (X_("running"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for running = %1\n", Properties::running.property_id));
}

const size_t TriggerBox::default_triggers_per_box = 8;

TriggerBox::TriggerBox (Session& s, DataType dt)
	: Processor (s, _("TriggerBox"), Temporal::BeatTime)
	, _bang_queue (1024)
	, _unbang_queue (1024)
	, _data_type (dt)
{

	/* default number of possible triggers. call ::add_trigger() to increase */

	if (_data_type == DataType::AUDIO) {
		for (size_t n = 0; n < default_triggers_per_box; ++n) {
			all_triggers.push_back (new AudioTrigger (n, *this));
		}
	}

	midi_trigger_map.insert (midi_trigger_map.end(), std::make_pair (uint8_t (60), 0));
	midi_trigger_map.insert (midi_trigger_map.end(), std::make_pair (uint8_t (61), 1));
	midi_trigger_map.insert (midi_trigger_map.end(), std::make_pair (uint8_t (62), 2));
	midi_trigger_map.insert (midi_trigger_map.end(), std::make_pair (uint8_t (63), 3));
	midi_trigger_map.insert (midi_trigger_map.end(), std::make_pair (uint8_t (64), 4));
	midi_trigger_map.insert (midi_trigger_map.end(), std::make_pair (uint8_t (65), 5));
	midi_trigger_map.insert (midi_trigger_map.end(), std::make_pair (uint8_t (66), 6));
	midi_trigger_map.insert (midi_trigger_map.end(), std::make_pair (uint8_t (67), 7));
	midi_trigger_map.insert (midi_trigger_map.end(), std::make_pair (uint8_t (68), 8));
	midi_trigger_map.insert (midi_trigger_map.end(), std::make_pair (uint8_t (69), 9));
}

int
TriggerBox::set_from_path (size_t slot, std::string const & path)
{
	assert (slot < all_triggers.size());

	try {
		SoundFileInfo info;
		string errmsg;

		if (!SndFileSource::get_soundfile_info (path, info, errmsg)) {
			error << string_compose (_("Cannot get info from audio file %1 (%2)"), path, errmsg) << endmsg;
			return -1;
		}

		SourceList src_list;

		for (uint16_t n = 0; n < info.channels; ++n) {
			boost::shared_ptr<Source> source (new SndFileSource (_session, path, n, Source::Flag (0)));
			src_list.push_back (source);
		}

		PropertyList plist;

		plist.add (Properties::start, 0);
		plist.add (Properties::length, src_list.front()->length ());
		plist.add (Properties::name, basename_nosuffix (path));
		plist.add (Properties::layer, 0);
		plist.add (Properties::layering_index, 0);

		boost::shared_ptr<Region> the_region (RegionFactory::create (src_list, plist, false));

		all_triggers[slot]->set_region (the_region);

		/* XXX catch region going away */

	} catch (std::exception& e) {
		cerr << "loading sample from " << path << " failed: " << e.what() << endl;
		return -1;
	}

	return 0;
}

TriggerBox::~TriggerBox ()
{
	drop_triggers ();
}

void
TriggerBox::stop_all ()
{
	/* XXX needs to be done with mutex or via thread-safe queue */

	for (size_t n = 0; n < all_triggers.size(); ++n) {
		all_triggers[n]->stop ();
	}
}

void
TriggerBox::drop_triggers ()
{
	Glib::Threads::RWLock::WriterLock lm (trigger_lock);

	for (Triggers::iterator t = all_triggers.begin(); t != all_triggers.end(); ++t) {
		if (*t) {
			delete *t;
			(*t) = 0;
		}
	}
}

Trigger*
TriggerBox::trigger (Triggers::size_type n)
{
	Glib::Threads::RWLock::ReaderLock lm (trigger_lock);

	if (n >= all_triggers.size()) {
		return 0;
	}

	return all_triggers[n];
}

bool
TriggerBox::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	if (in.get(DataType::MIDI) < 1) {
		return false;
	}

	out = ChanCount::max (out, ChanCount (DataType::AUDIO, 2));
	return true;
}

bool
TriggerBox::configure_io (ChanCount in, ChanCount out)
{
	return Processor::configure_io (in, out);
}

void
TriggerBox::add_trigger (Trigger* trigger)
{
	Glib::Threads::RWLock::WriterLock lm (trigger_lock);
	all_triggers.push_back (trigger);
}

void
TriggerBox::process_midi_trigger_requests (BufferSet& bufs)
{
	/* check MIDI port input buffers for triggers */

	for (BufferSet::midi_iterator mi = bufs.midi_begin(); mi != bufs.midi_end(); ++mi) {
		MidiBuffer& mb (*mi);

		for (MidiBuffer::iterator ev = mb.begin(); ev != mb.end(); ++ev) {

			if (!(*ev).is_note()) {
				continue;
			}

			MidiTriggerMap::iterator mt = midi_trigger_map.find ((*ev).note());
			Trigger* t = 0;

			if (mt != midi_trigger_map.end()) {

				assert (mt->second < all_triggers.size());

				t = all_triggers[mt->second];

				if (!t) {
					continue;
				}
			}

			if ((*ev).is_note_on()) {

				t->bang ();

			} else if ((*ev).is_note_off()) {

				t->unbang ();
			}
		}
	}
}

void
TriggerBox::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool result_required)
{
	if (start_sample < 0) {
		/* we can't do anything under these conditions (related to
		   latency compensation
		*/
		return;
	}

	process_midi_trigger_requests (bufs);

	std::vector<size_t> to_run;

	for (size_t n = 0; n < all_triggers.size(); ++n) {
		all_triggers[n]->process_state_requests ();

		/* now recheck all states so that we know if we have any
		 * active triggers.
		 */

		switch (all_triggers[n]->state()) {
		case Trigger::Running:
		case Trigger::WaitingToStart:
		case Trigger::WaitingToStop:
		case Trigger::WaitingForRetrigger:
		case Trigger::Stopping:
			to_run.push_back (n);
		default:
			break;
		}
	}

	if (to_run.empty()) {
		/* this saves us from the cost of the tempo map lookups.
		   XXX if these were passed in to ::run(), we could possibly
		   skip this condition.
		*/
		return;
	}

	/* transport must be active for triggers */

	if (!_session.transport_state_rolling()) {
		_session.start_transport_from_processor ();
	}

	timepos_t start (start_sample);
	timepos_t end (end_sample);
	Temporal::Beats start_beats (start.beats());
	Temporal::Beats end_beats (end.beats());
	Temporal::TempoMap::SharedPtr tmap (Temporal::TempoMap::use());
	size_t max_chans = 0;
	bool first = false;

	/* Now actually run all currently active triggers */

	for (size_t n = 0; n < to_run.size(); ++n) {

		Trigger& trigger (*all_triggers[to_run[n]]);

		assert (trigger.state() >= Trigger::WaitingToStart);

		boost::shared_ptr<Region> r = trigger.region();

		sampleoffset_t dest_offset;
		pframes_t trigger_samples;
		Trigger::RunType rt;
		bool was_waiting_to_start = (trigger.state() == Trigger::WaitingToStart);

		switch (trigger.state()) {
		case Trigger::WaitingToStop:
		case Trigger::WaitingToStart:
		case Trigger::WaitingForRetrigger:
			rt = trigger.maybe_compute_next_transition (start_beats, end_beats);
			break;
		default:
			rt = Trigger::RunAll;
		}

		if (rt == Trigger::RunEnd) {


			/* trigger will reach it's end somewhere within this
			 * process cycle, so compute the number of samples it
			 * should generate.
			 */

			trigger_samples = nframes - (trigger.bang_samples - start_sample);
			dest_offset = 0;

		} else if (rt == Trigger::RunStart) {

			/* trigger will start somewhere within this process
			 * cycle. Compute the sample offset where any audio
			 * should end up, and the number of samples it should generate.
			 */

			dest_offset = std::max (samplepos_t (0), trigger.bang_samples - start_sample);
			trigger_samples = nframes - dest_offset;

		} else if (rt == Trigger::RunAll) {

			/* trigger is just running normally, and will fill
			 * buffers entirely.
			 */

			dest_offset = 0;
			trigger_samples = nframes;

		} else if (rt == Trigger::RunNone) {
			continue;
		}

		if (was_waiting_to_start) {
			set_next_trigger (trigger.index());
		}

		AudioTrigger* at = dynamic_cast<AudioTrigger*> (&trigger);

		if (at) {

			boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (r);
			const size_t nchans = ar->n_channels ();

			max_chans = std::max (max_chans, nchans);

			at->run (bufs, trigger_samples, dest_offset, first);

			first = false;

		} else {

			/* XXX MIDI triggers to be implemented */

		}

		if (trigger.state() == Trigger::Stopped) {

			cerr << "stopped, check trigger " << trigger.next_trigger() << std::endl;

			if (trigger.next_trigger() != -1) {

				int nxt = trigger.next_trigger();

				if (nxt >= 0 && (size_t) nxt < all_triggers.size() && !all_triggers[nxt]->active()) {
					DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 switching to %2\n", trigger.index(), nxt));
					if (all_triggers[nxt]->legato()) {
						all_triggers[nxt]->set_legato_offset (trigger.current_pos());
					}
					/* start it up */
					all_triggers[nxt]->bang ();
					all_triggers[nxt]->process_state_requests ();
					/* make sure we run it this process cycle */
					to_run.push_back (nxt);
				}
			}
		}
	}

	ChanCount cc (DataType::AUDIO, max_chans);
	cc.set_midi (bufs.count().n_midi());
	bufs.set_count (cc);
}

void
TriggerBox::set_next_trigger (size_t current)
{
	size_t n;
	size_t runnable = 0;

	for (size_t n = 0; n < all_triggers.size(); ++n) {
		if (all_triggers[n]->region()) {
			runnable++;
		}
	}

	int which_follow_action;
	int r = _pcg.rand (100); // 0 .. 99

	if (r <= all_triggers[current]->follow_action_probability()) {
		which_follow_action = 0;
	} else {
		which_follow_action = 1;
	}

	switch (all_triggers[current]->follow_action (which_follow_action)) {

	case Trigger::Stop:
		all_triggers[current]->set_next_trigger (-1);
		return;

	case Trigger::QueuedTrigger:
		/* XXX implement me */
		return;
	default:
		if (runnable == 1) {
			all_triggers[current]->set_next_trigger (current);
			return;
		}
	}

	switch (all_triggers[current]->follow_action (which_follow_action)) {

	case Trigger::Again:
		all_triggers[current]->set_next_trigger (current);
		return;

	case Trigger::NextTrigger:
		n = current;
		while (true) {
			++n;

			if (n >= all_triggers.size()) {
				n = 0;
			}

			if (n == current) {
				break;
			}

			if (all_triggers[n]->region() && !all_triggers[n]->active()) {
				all_triggers[current]->set_next_trigger (n);
				return;
			}
		}
		break;
	case Trigger::PrevTrigger:
		n = current;
		while (true) {
			if (n == 0) {
				n = all_triggers.size() - 1;
			} else {
				n -= 1;
			}

			if (n == current) {
				break;
			}

			if (all_triggers[n]->region() && !all_triggers[n]->active ()) {
				all_triggers[current]->set_next_trigger (n);
				return;
			}
		}
		break;

	case Trigger::FirstTrigger:
		for (n = 0; n < all_triggers.size(); ++n) {
			if (all_triggers[n]->region() && !all_triggers[n]->active ()) {
				all_triggers[current]->set_next_trigger (n);
				return;
			}
		}
		break;
	case Trigger::LastTrigger:
		for (int i = all_triggers.size() - 1; i >= 0; --i) {
			if (all_triggers[i]->region() && !all_triggers[i]->active ()) {
				all_triggers[current]->set_next_trigger (i);
				return;
			}
		}
		break;

	case Trigger::AnyTrigger:
		while (true) {
			n = _pcg.rand (all_triggers.size());
			if (!all_triggers[n]->region()) {
				continue;
			}
			if (all_triggers[n]->active()) {
				continue;
			}
			break;
		}
		all_triggers[current]->set_next_trigger (n);
		return;

	case Trigger::OtherTrigger:
		while (true) {
			n = _pcg.rand (all_triggers.size());
			if ((size_t) n == current) {
				continue;
			}
			if (!all_triggers[n]->region()) {
				continue;
			}
			if (all_triggers[n]->active()) {
				continue;
			}
			break;
		}
		all_triggers[current]->set_next_trigger (n);
		return;

	/* NOTREACHED */
	case Trigger::Stop:
	case Trigger::QueuedTrigger:
		break;

	}
	all_triggers[current]->set_next_trigger (current);
}

XMLNode&
TriggerBox::get_state (void)
{
	return Processor::get_state ();
}

int
TriggerBox::set_state (const XMLNode&, int version)
{
	return 0;
}

/*--------------------*/
