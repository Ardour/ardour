#include <iostream>

#include "pbd/basename.h"
#include "pbd/failed_constructor.h"

#include "ardour/audioregion.h"
#include "ardour/audio_buffer.h"
#include "ardour/midi_buffer.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/sndfilesource.h"
#include "ardour/triggerbox.h"

using namespace PBD;
using namespace ARDOUR;
using std::string;
using std::cerr;
using std::endl;

TriggerBox::TriggerBox (Session& s)
	: Processor (s, _("TriggerBox"), Temporal::BeatTime)
	, _trigger_queue (1024)
{

	/* default number of possible triggers. call ::add_trigger() to increase */

	all_triggers.resize (16, 0);

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


	load_some_samples ();
}

void
TriggerBox::load_some_samples ()
{
	/* XXX TESTING ONLY */

	char const * paths[] = {
		"AS_AP_Perc_Loop_01_125bpm.wav",
		"AS_AP_Perc_Loop_05_125bpm.wav",
		"AS_AP_Perc_Loop_09_125bpm.wav",
		"AS_AP_Perc_Loop_02_125bpm.wav",
		"AS_AP_Perc_Loop_06_125bpm.wav",
		"AS_AP_Perc_Loop_10_125bpm.wav",
		"AS_AP_Perc_Loop_03_125bpm.wav",
		"AS_AP_Perc_Loop_07_125bpm.wav",
		"AS_AP_Perc_Loop_04_125bpm.wav",
		"AS_AP_Perc_Loop_08_125bpm.wav",
		0
	};

	try {
		for (size_t n = 0; paths[n]; ++n) {

			string dir = "/usr/local/music/samples/Loops (WAV)/ASHRAM Afro Percussion Loops/";
			string path = dir + paths[n];


			SoundFileInfo info;
			string errmsg;
			if (!SndFileSource::get_soundfile_info (path, info, errmsg)) {
				error << string_compose (_("Cannot get info from audio file %1 (%2)"), path, errmsg) << endmsg;
				continue;
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

			all_triggers[n] = new AudioTrigger (n, boost::dynamic_pointer_cast<AudioRegion> (the_region));
		}
	} catch (std::exception& e) {
		cerr << "loading samples failed: " << e.what() << endl;
	}
}

TriggerBox::~TriggerBox ()
{
	drop_triggers ();
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
TriggerBox::queue_trigger (Trigger* trigger)
{
	return  _trigger_queue.write (&trigger, 1) == 1;
}

void
TriggerBox::process_ui_trigger_requests ()
{
		/* if there are any triggers queued, make them active
	*/

	RingBuffer<Trigger*>::rw_vector vec;
	_trigger_queue.get_read_vector (&vec);

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

	_trigger_queue.increment_read_idx (vec.len[0] + vec.len[1]);
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

	for (Triggers::iterator t = pending_on_triggers.begin(); t != pending_on_triggers.end(); ) {

		Temporal::Beats q = (*t)->quantization ();
		timepos_t fire_at (start_beats.snap_to (q));

		if (fire_at >= start_beats && fire_at < end_beats) {
			(*t)->fire_samples = fire_at.samples();
			(*t)->fire_beats = fire_at.beats();
			active_triggers.push_back (*t);
			t = pending_on_triggers.erase (t);
		} else {
			++t;
		}
	}

	for (Triggers::iterator t = pending_off_triggers.begin(); t != pending_off_triggers.end(); ) {

		Temporal::Beats q = (*t)->quantization ();
		timepos_t off_at (start_beats.snap_to (q));

		if (off_at >= start_beats && off_at < end_beats) {
			(*t)->fire_samples = off_at.samples();
			(*t)->fire_beats = off_at.beats();
			(*t)->unbang (*this, (*t)->fire_beats, (*t)->fire_samples);
			t = pending_off_triggers.erase (t);
		} else {
			++t;
		}
	}

	bool err = false;
	bool need_butler = false;
	size_t max_chans = 0;

	for (Triggers::iterator t = active_triggers.begin(); !err && t != active_triggers.end(); ++t) {

		Trigger* trigger = (*t);
		sampleoffset_t dest_offset = 0;
		pframes_t trigger_samples = nframes;

		if (trigger->stop_requested()) {
			trigger_samples = nframes - (trigger->fire_samples - start_sample);
		} else if (!trigger->running()) {
			trigger->bang (*this);
			dest_offset = std::max (samplepos_t (0), trigger->fire_samples - start_sample);
			trigger_samples = nframes - dest_offset;
		}

		AudioTrigger* at = dynamic_cast<AudioTrigger*> (trigger);

		boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (trigger->region());
		const size_t nchans = ar->n_channels ();
		max_chans = std::max (max_chans, nchans);

		for (uint32_t chan = 0; !err && chan < nchans; ++chan) {

			AudioBuffer& buf = bufs.get_audio (chan);

			pframes_t to_copy = trigger_samples;
			Sample* data = at->run (chan, to_copy, need_butler);

			if (!data) {
				/* XXX need to delete the trigger/put it back in the pool */
				t = active_triggers.erase (t);
				err = true;
			} else {
				if (t == active_triggers.begin()) {
					buf.read_from (data, to_copy, dest_offset);
					if ((to_copy + dest_offset) < nframes) {
						buf.silence (nframes - to_copy, to_copy + dest_offset);
					}
				} else {
					buf.accumulate_from (data, to_copy);
				}
			}

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

Trigger::Trigger (size_t n, boost::shared_ptr<Region> r)
	: _running (false)
	, _stop_requested (false)
	, _index (n)
	, _launch_style (Loop)
	, _follow_action (Stop)
	, _region (r)
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

void
Trigger::set_region_internal (boost::shared_ptr<Region> r)
{
	_region = r;
}

Temporal::Beats
Trigger::quantization () const
{
	if (_quantization == Temporal::Beats()) {
		return Temporal::Beats (1, 0);
	}

	return _quantization;
}

void
Trigger::stop ()
{
	_stop_requested = true;
}

/*--------------------*/

AudioTrigger::AudioTrigger (size_t n, boost::shared_ptr<AudioRegion> r)
	: Trigger (n, r)
	, data (0)
	, length (0)
{
	/* XXX catch region going away */

	if (load_data (r)) {
		throw failed_constructor ();
	}
}

AudioTrigger::~AudioTrigger ()
{
	for (std::vector<Sample*>::iterator d = data.begin(); d != data.end(); ++d) {
		delete *d;
	}
}

int
AudioTrigger::set_region (boost::shared_ptr<Region> r)
{
	boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (r);

	if (!ar) {
		return -1;
	}

	set_region_internal (r);

	if (load_data (ar)) {
		return -1;
	}

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

	length = ar->length_samples();

	drop_data ();

	try {
		for (uint32_t n = 0; n < nchans; ++n) {
			data.push_back (new Sample[length]);
			read_index.push_back (0);
			ar->read (data[n], 0, length, n);
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

Sample*
AudioTrigger::run (uint32_t channel, pframes_t& nframes, bool& /* need_butler */)
{
	if (!_running) {
		return 0;
	}

	if (read_index[channel] >= length) {
		_running = false;
		return 0;
	}

	if (_stop_requested) {
		/* XXX need fade out machinery */
		_running = false;
		_stop_requested = false;
		return 0;
	}

	channel %= data.size();

	nframes = (pframes_t) std::min ((samplecnt_t) nframes, (length - read_index[channel]));
	read_index[channel] += nframes;

	return data[channel] + read_index[channel];
}
