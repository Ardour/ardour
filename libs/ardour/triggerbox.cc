#include <iostream>

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
	, _launch_style (Loop)
	, _follow_action (Stop)
	, _quantization (Temporal::BBT_Offset (0, 1, 0))
{
}

void
Trigger::bang ()
{
	_bang.fetch_add (1);
	std::cerr << "trigger " << index() << " banged to " << _bang.load() << std::endl;
}

void
Trigger::unbang ()
{
	_unbang.fetch_add (1);
}

void
Trigger::set_follow_action (FollowAction f)
{
	_follow_action = f;
}

void
Trigger::set_launch_style (LaunchStyle l)
{
	_launch_style = l;
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
Trigger::set_quantization (Temporal::BBT_Offset const & q)
{
	_quantization = q;
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

	if (new_state == _state) {
		return;
	}

	switch (new_state) {
	case Stopped:
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 %2 -> %3\n", index(), enum_2_string (_state), enum_2_string (WaitingToStop)));
		_state = WaitingToStop;
		break;
	case Running:
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 %2 -> %3\n", index(), enum_2_string (_state), enum_2_string (WaitingToStart)));
		_state = WaitingToStart;
		break;
	default:
		break;
	}

	/* now check bangs/unbangs */

	int x;

	while ((x = _bang.load ())) {

		_bang.fetch_sub (1);

		switch (_state) {
		case None:
			abort ();
			break;

		case Running:
			switch (launch_style()) {
			case Loop:
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 retrigger\n", index()));
				retrigger ();
				break;
			case Gate:
			case Toggle:
			case Repeat:
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 %2 -> %3\n", index(), enum_2_string (Running), enum_2_string (Stopped)));
				_state = Stopped;
			}
			break;

		case Stopped:
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 %2 -> %3\n", index(), enum_2_string (Stopped), enum_2_string (WaitingToStart)));
			_state = WaitingToStart;
			break;

		case WaitingToStart:
			break;

		case WaitingToStop:
			break;

		case Stopping:
			break;
		}
	}


	while ((x = _unbang.load ())) {

		_unbang.fetch_sub (1);

		if (active()  &&launch_style() == Trigger::Gate) {
			_state = WaitingToStop;
		}
	}
}

bool
Trigger::maybe_compute_start_or_stop (Temporal::Beats const & start, Temporal::Beats const & end)
{
	timepos_t ev_time (Temporal::BeatTime);

	if (_quantization.bars == 0) {
		ev_time = timepos_t (start.snap_to (Temporal::Beats (_quantization.beats, _quantization.ticks)));
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 quantized with %5 start at %2, sb %3 eb %4\n", index(), ev_time, start, end, _quantization));
	} else {
		/* XXX not yet handled */
	}

	if (ev_time.beats() >= start && ev_time < end) {
		bang_samples = ev_time.samples();
		bang_beats = ev_time.beats ();

		if (_state == WaitingToStop) {
			_state = Stopping;
		} else {
			retrigger ();
			_state = Running;
		}

		return true;
	}


	return false;
}

/*--------------------*/

AudioTrigger::AudioTrigger (size_t n, TriggerBox& b)
	: Trigger (n, b)
	, data (0)
	, data_length (0)
{
}

AudioTrigger::~AudioTrigger ()
{
	for (std::vector<Sample*>::iterator d = data.begin(); d != data.end(); ++d) {
		delete *d;
	}
}

void
AudioTrigger::set_length (timecnt_t const & newlen)
{
	using namespace RubberBand;
	using namespace Temporal;

	if (!_region) {
		return;
	}

	boost::shared_ptr<AudioRegion> ar (boost::dynamic_pointer_cast<AudioRegion> (_region));

	/* load raw data */

	load_data (ar);

	if (newlen == _region->length()) {
		/* no stretch required */
		return;
	}

	std::cerr << " sl to " << newlen << " from " << _region->length() << endl;

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
		const timecnt_t dur = TempoMap::use()->convert_duration (newlen, timepos_t (0), AudioTime);
		std::cerr << "new dur = " << dur << " S " << dur.samples() << " vs " << data_length << endl;
		new_ratio = (double) dur.samples() / data_length;
	}

	stretcher.setTimeRatio (new_ratio);

	const samplecnt_t expected_length = ceil (data_length * new_ratio) + 16; /* extra space for safety */
	std::vector<Sample*> stretched;

	for (uint32_t n = 0; n < nchans; ++n) {
		stretched.push_back (new Sample[expected_length]);
	}

	/* RB expects array-of-ptr-to-Sample, so set one up */

	Sample* raw[nchans];
	Sample* results[nchans];

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

		stretcher.study (raw, to_read, (read >= data_length));
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

		stretcher.process (raw, to_read, (read >= data_length));

		while ((avail = stretcher.available()) > 0) {

			for (uint32_t n = 0; n < nchans; ++n) {
				results[n] = stretched[n] + processed;
			}

			processed += stretcher.retrieve (results, avail);
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

		processed += stretcher.retrieve (results, avail);
	}

	/* allocate new data buffers */

	drop_data ();
	data = stretched;
	data_length = processed;
}

timecnt_t
AudioTrigger::current_length() const
{
	if (_region) {
		return timecnt_t (data_length);
	}
	return timecnt_t (Temporal::BeatTime);
}

timecnt_t
AudioTrigger::natural_length() const
{
	if (_region) {
		return _region->length();
	}
	return timecnt_t (Temporal::BeatTime);
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

	set_length (r->length ());

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

	drop_data ();

	try {
		for (uint32_t n = 0; n < nchans; ++n) {
			data.push_back (new Sample[data_length]);
			read_index.push_back (0);
			ar->read (data[n], 0, data_length, n);
		}
	} catch (...) {
		drop_data ();
		return -1;
	}

	return 0;
}

void
AudioTrigger::retrigger ()
{
	for (std::vector<samplecnt_t>::iterator ri = read_index.begin(); ri != read_index.end(); ++ri) {
		(*ri) = 0;
	}
}

Trigger::RunResult
AudioTrigger::run (AudioBuffer& buf, uint32_t channel, pframes_t& nframes, pframes_t dest_offset, bool first)
{
	if (read_index[channel] >= data_length) {
		return RemoveTrigger;
	}

	if (!active()) {
		return RemoveTrigger;
	}


	channel %= data.size();

	pframes_t nf = (pframes_t) std::min ((samplecnt_t) nframes, (data_length - read_index[channel]));
	Sample* src = data[channel] + read_index[channel];

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 running with nf %2\n", index(), nf));

	if (first) {
		buf.read_from (src, nf, dest_offset);
	} else {
		buf.accumulate_from (src, nf);
	}

	read_index[channel] += nf;

	if ((nframes - nf) != 0) {
		/* did not get all samples, must have reached the end, figure out what do to */
		nframes = nf;
		return at_end ();
	}

	return Relax;
}

Trigger::RunResult
AudioTrigger::at_end ()
{
	switch (launch_style()) {
	case Trigger::Loop:
		retrigger();
		return ReadMore;
	default:
		break;
	}

	if (follow_action() == Stop) {
		return RunResult (RemoveTrigger|FillSilence);
	}

	return RunResult (RemoveTrigger|ChangeTriggers|FillSilence);
}

/**************/

void
Trigger::make_property_quarks ()
{
	Properties::muted.property_id = g_quark_from_static_string (X_("running"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for running = %1\n", Properties::running.property_id));
}

TriggerBox::TriggerBox (Session& s, DataType dt)
	: Processor (s, _("TriggerBox"), Temporal::BeatTime)
	, _bang_queue (1024)
	, _unbang_queue (1024)
	, _data_type (dt)
{

	/* default number of possible triggers. call ::add_trigger() to increase */

	if (_data_type == DataType::AUDIO) {
		for (size_t n = 0; n < 16; ++n) {
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

	for (Triggers::iterator t = active_triggers.begin(); t != active_triggers.end(); ++t) {
		(*t)->stop ();
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

	size_t run_cnt = 0;

	for (size_t n = 0; n < all_triggers.size(); ++n) {
		all_triggers[n]->process_state_requests ();

		/* now recheck all states so that we know if we have any
		 * active triggers.
		 */

		switch (all_triggers[n]->state()) {
		case Trigger::Running:
		case Trigger::WaitingToStart:
		case Trigger::WaitingToStop:
		case Trigger::Stopping:
			run_cnt++;
		default:
			break;
		}
	}

	if (run_cnt == 0) {
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
	bool will_start = false;
	bool will_stop = false;

	for (size_t n = 0; n < all_triggers.size(); ++n) {

	}

	size_t max_chans = 0;
	Trigger::RunResult rr;
	bool first = false;

	/* Now actually run all currently active triggers */

	for (size_t n = 0; n < all_triggers.size(); ++n) {

		Trigger& trigger (*all_triggers[n]);

		if (trigger.state() < Trigger::WaitingToStart) {
			continue;
		}

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 might run, state %2\n", trigger.index(), enum_2_string (trigger.state())));

		boost::shared_ptr<Region> r = trigger.region();

		sampleoffset_t dest_offset;
		pframes_t trigger_samples;

		switch (trigger.state()) {
		case Trigger::None:
		case Trigger::Stopped:
			abort ();
			break;

		case Trigger::Running:
		case Trigger::Stopping:
			break;

		case Trigger::WaitingToStop:
			will_stop = trigger.maybe_compute_start_or_stop (start_beats, end_beats);
			break;

		case Trigger::WaitingToStart:
			will_start = trigger.maybe_compute_start_or_stop (start_beats, end_beats);
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 waiting to start, will start? %2\n", trigger.index(), will_start));
			break;
		}

		if (will_stop) {

			/* trigger will reach it's end somewhere within this
			 * process cycle, so compute the number of samples it
			 * should generate.
			 */

			trigger_samples = nframes - (trigger.bang_samples - start_sample);
			dest_offset = 0;

		} else if (will_start) {

			/* trigger will start somewhere within this process
			 * cycle. Compute the sample offset where any audio
			 * should end up, and the number of samples it should generate.
			 */

			dest_offset = std::max (samplepos_t (0), trigger.bang_samples - start_sample);
			trigger_samples = nframes - dest_offset;

		} else {

			/* trigger is just running normally, and will fill
			 * buffers entirely.
			 */

			dest_offset = 0;
			trigger_samples = nframes;
		}

		AudioTrigger* at = dynamic_cast<AudioTrigger*> (&trigger);

		if (at) {

			boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (r);
			const size_t nchans = ar->n_channels ();
			pframes_t nf = trigger_samples;

			max_chans = std::max (max_chans, nchans);

			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 run with ts %2 do %3\n", trigger.index(), trigger_samples, dest_offset));

		  read_more:
			for (uint32_t chan = 0; chan < nchans; ++chan) {

				/* we assume the result will be the same for all channels */

				AudioBuffer& buf (bufs.get_audio (chan));

				rr = at->run (buf, chan, nf, dest_offset, first);

				/* nf is now the number of samples that were
				 * actually processed/generated/written
				 */

				if (rr & Trigger::FillSilence) {
					buf.silence (trigger_samples - nf, nf + dest_offset);
				}
			}

			first = false;

			if (rr == Trigger::ReadMore) {
				trigger_samples -= nf;
				goto read_more;
			}

		} else {

			/* XXX MIDI triggers to be implemented */

		}

		if (rr & Trigger::ChangeTriggers) {
			/* XXX do this! */
			std::cerr << "Should change triggers!\n";
		}
	}

	ChanCount cc (DataType::AUDIO, max_chans);
	cc.set_midi (bufs.count().n_midi());
	bufs.set_count (cc);
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
