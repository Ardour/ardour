#include <iostream>

#include <glibmm.h>

#include <rubberband/RubberBandStretcher.h>

#include "pbd/basename.h"
#include "pbd/failed_constructor.h"

#include "temporal/tempo.h"

#include "ardour/audioregion.h"
#include "ardour/audio_buffer.h"
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

bool
TriggerBox::bang_trigger (Trigger* trigger)
{
	return  _bang_queue.write (&trigger, 1) == 1;
}

bool
TriggerBox::unbang_trigger (Trigger* trigger)
{
	return  _unbang_queue.write (&trigger, 1) == 1;
}

void
TriggerBox::process_ui_trigger_requests ()
{
	/* bangs */

	RingBuffer<Trigger*>::rw_vector vec;
	_bang_queue.get_read_vector (&vec);

	for (uint32_t n = 0; n < vec.len[0]; ++n) {
		Trigger* t = vec.buf[0][n];
		pending_on_triggers.push_back (t);
	}

	for (uint32_t n = 0; n < vec.len[1]; ++n) {
		Trigger* t = vec.buf[1][n];
		pending_on_triggers.push_back (t);
	}

	if (vec.len[0] || vec.len[1]) {

		if (!_session.transport_state_rolling()) {
			_session.start_transport_from_processor ();
		}
	}

	_bang_queue.increment_read_idx (vec.len[0] + vec.len[1]);

	/* unbangs */

	_unbang_queue.get_read_vector (&vec);

	for (uint32_t n = 0; n < vec.len[0]; ++n) {
		Trigger* t = vec.buf[0][n];
		pending_off_triggers.push_back (t);
	}

	for (uint32_t n = 0; n < vec.len[1]; ++n) {
		Trigger* t = vec.buf[1][n];
		pending_off_triggers.push_back (t);
	}

	_unbang_queue.increment_read_idx (vec.len[0] + vec.len[1]);
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

				if (!t->running()) {
					if (find (pending_on_triggers.begin(), pending_on_triggers.end(), t) == pending_on_triggers.end()) {
						pending_on_triggers.push_back (t);
					}
				} else {
					t->bang (*this);
				}

				if (!_session.transport_state_rolling()) {
					_session.start_transport_from_processor ();
				}

			} else if ((*ev).is_note_off()) {

				if (t->running() && t->launch_style() == Trigger::Gate) {
					pending_off_triggers.push_back (t);
				}

			}
		}
	}
}

void
TriggerBox::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool result_required)
{
	static int64_t rcnt = 0;

	rcnt++;

	if (start_sample < 0) {
		return;
	}

	process_ui_trigger_requests ();
	process_midi_trigger_requests (bufs);

	if (active_triggers.empty() && pending_on_triggers.empty()) {
		/* nothing to do */
		return;
	}

	timepos_t start (start_sample);
	timepos_t end (end_sample);
	Temporal::Beats start_beats (start.beats());
	Temporal::Beats end_beats (end.beats());
	Temporal::TempoMap::SharedPtr tmap (Temporal::TempoMap::use());

	for (Triggers::iterator t = pending_on_triggers.begin(); t != pending_on_triggers.end(); ) {

		Temporal::BBT_Offset q = (*t)->quantization ();
		timepos_t fire_at (Temporal::BeatTime);

		if (q.bars == 0) {
			fire_at = timepos_t (start_beats.snap_to (Temporal::Beats (q.beats, q.ticks)));
		} else {
			/* XXX not yet handled */
		}

		if ((*t)->running()) {

			if (fire_at >= start_beats && fire_at < end_beats) {
				(*t)->bang (*this);
				t = pending_on_triggers.erase (t);
			} else {
				++t;
			}

		} else if (fire_at >= start_beats && fire_at < end_beats) {

			(*t)->bang_samples = fire_at.samples();
			(*t)->bang_beats = fire_at.beats();
			active_triggers.push_back (*t);
			t = pending_on_triggers.erase (t);

		} else {
			++t;
		}
	}

	for (Triggers::iterator t = pending_off_triggers.begin(); t != pending_off_triggers.end(); ) {

		Temporal::BBT_Offset q = (*t)->quantization ();
		timepos_t off_at (Temporal::BeatTime);

		if (q.bars == 0) {
			off_at = timepos_t (start_beats.snap_to (Temporal::Beats (q.beats, q.ticks)));
		} else {
			/* XXX not yet handled */
		}

		if (off_at >= start_beats && off_at < end_beats) {
			(*t)->bang_samples = off_at.samples();
			(*t)->bang_beats = off_at.beats();
			(*t)->unbang (*this, (*t)->bang_beats, (*t)->bang_samples);
			t = pending_off_triggers.erase (t);
		} else {
			++t;
		}
	}

	size_t max_chans = 0;

	for (Triggers::iterator t = active_triggers.begin(); t != active_triggers.end(); ) {

		Trigger* trigger = (*t);
		boost::shared_ptr<Region> r = trigger->region();

		if (!r) {
			t = active_triggers.erase (t);
			continue;
		}

		sampleoffset_t dest_offset = 0;
		pframes_t trigger_samples = nframes;

		if (trigger->stop_requested()) {

			/* bang_samples says when to stop, so compute an offset
			 *into the nframes we've been asked to provide.
			 */

			trigger_samples = nframes - (trigger->bang_samples - start_sample);

		} else if (!trigger->running()) {

			trigger->bang (*this);
			dest_offset = std::max (samplepos_t (0), trigger->bang_samples - start_sample);
			trigger_samples = nframes - dest_offset;
		}

		AudioTrigger* at = dynamic_cast<AudioTrigger*> (trigger);
		bool err = false;

		if (at) {
			boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (r);
			const bool first = (t == active_triggers.begin());
			const size_t nchans = ar->n_channels ();

			max_chans = std::max (max_chans, nchans);

			for (uint32_t chan = 0; chan < nchans; ++chan) {

				if (at->run (bufs.get_audio (chan), chan, trigger_samples, dest_offset, first)) {
					err = true;
					break;
				}
			}
		} else {

			/* XXX to be written */

		}

		if (err) {
			t = active_triggers.erase (t);
		} else {
			++t;
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

Trigger::Trigger (size_t n, TriggerBox& b)
	: _box (b)
	, _running (false)
	, _stop_requested (false)
	, _index (n)
	, _launch_style (Loop)
	, _follow_action (Stop)
	, _quantization (Temporal::BBT_Offset (0, 1, 0))
{
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
	_stop_requested = true;
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

	const samplecnt_t block_size = 8192;
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

	_running = true;
}

void
AudioTrigger::bang (TriggerBox& /*proc*/)
{
	/* user "hit" the trigger in a way that means "start" */

	switch (_launch_style) {
	case Loop:
		retrigger ();
		break;
	case Gate:
		retrigger ();
		break;
	case Toggle:
		if (_running) {
			_stop_requested = true;
		} else {
			retrigger ();
		}
		break;
	case Repeat:
		retrigger ();
		break;
	}

	_running = true;
}

void
AudioTrigger::unbang (TriggerBox& /*proc*/, Temporal::Beats const &, samplepos_t)
{
	/* user "hit" the trigger in a way that means "stop" */

	switch (_launch_style) {
	case Loop:
		/* do nothing, wait for next "start" */
		break;
	case Gate:
		_stop_requested = true;
		break;
	case Toggle:
		/* do nothing ... wait for next "start" */
		break;
	case Repeat:
		_stop_requested = true;
		break;
	}
}

int
AudioTrigger::run (AudioBuffer& buf, uint32_t channel, pframes_t nframes, pframes_t dest_offset, bool first)
{
	if (!_running) {
		return -1;
	}

	if (read_index[channel] >= data_length) {
		_running = false;
		return -1;
	}

	if (_stop_requested) {
		/* XXX need fade out machinery */
		_running = false;
		_stop_requested = false;
		return 0;
	}

	channel %= data.size();

  read_more:
	pframes_t nf = (pframes_t) std::min ((samplecnt_t) nframes, (data_length - read_index[channel]));
	Sample* src = data[channel] + read_index[channel];

	if (first) {
		buf.read_from (src, nf, dest_offset);
		if ((nf + dest_offset) < nframes) {
			buf.silence (nframes - nf, nf + dest_offset);
		}
	} else {
		buf.accumulate_from (src, nf);
	}

	read_index[channel] += nf;

	if (nf < nframes) {

		nframes -= nf;

		switch (launch_style()) {
		case Trigger::Loop:
			retrigger();
			goto read_more;
			break;
		case Trigger::Gate:
		case Trigger::Toggle:
		case Trigger::Repeat:
			return -1;
			break;
		}
	}

	return 0;
}
