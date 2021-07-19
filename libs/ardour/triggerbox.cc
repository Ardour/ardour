#include <iostream>

#include "pbd/failed_constructor.h"

#include "ardour/audioregion.h"
#include "ardour/audio_buffer.h"
#include "ardour/midi_buffer.h"
#include "ardour/region_factory.h"
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

	boost::shared_ptr<Source> the_source (new SndFileSource (_session, "/music/misc/La_Voz_Del_Rio.wav", 0, Source::Flag (0)));

	PropertyList plist;

	plist.add (Properties::start, 0);
	plist.add (Properties::length, the_source->length ());
	plist.add (Properties::name, string ("bang"));
	plist.add (Properties::layer, 0);
	plist.add (Properties::layering_index, 0);

	boost::shared_ptr<Region> the_region (RegionFactory::create (the_source, plist, false));

	all_triggers[0] = new AudioTrigger (boost::dynamic_pointer_cast<AudioRegion> (the_region));
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
	return _trigger_queue.write (&trigger, 1) == 1;
}

void
TriggerBox::process_trigger_requests (Temporal::Beats const & beats_now, samplepos_t samples_now)
{
		/* if there are any triggers queued, make them active
	*/

	RingBuffer<Trigger*>::rw_vector vec;
	_trigger_queue.get_read_vector (&vec);

	for (uint32_t n = 0; n < vec.len[0]; ++n) {
		Trigger* t = vec.buf[0][n];
		t->bang (*this, beats_now, samples_now);
		active_triggers.push_back (t);
	}

	for (uint32_t n = 0; n < vec.len[1]; ++n) {
		Trigger* t = vec.buf[1][n];
		t->bang (*this, beats_now, samples_now);
		active_triggers.push_back (t);
	}

	_trigger_queue.increment_read_idx (vec.len[0] + vec.len[1]);
}

void
TriggerBox::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool result_required)
{
	samplepos_t next_beat = 0;
	Temporal::Beats beats_now;

	process_trigger_requests (beats_now, start_sample);

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
					active_triggers.push_back (t);
				}

				t->bang (*this, beats_now, start_sample);

			} else if ((*ev).is_note_off()) {

				if (t->running() && t->launch_style() == Trigger::Gate) {
					t->unbang (*this, beats_now, start_sample);
				}

			}
		}
	}

	if (active_triggers.empty()) {
		/* nothing to do */
		return;
	}

	/* get tempo map */

	/* find offset to next bar * and beat start
	 */

	/* if next beat occurs in this process cycle, see if we have any triggers waiting
	*/

	// bool run_beats = false;
	// bool run_bars = false;

	//if (next_beat >= start_frame && next_beat < end_sample) {
	//run_beats = true;
	//}

	bool err = false;
	bool need_butler = false;
	size_t max_chans = 0;

	for (Triggers::iterator t = active_triggers.begin(); !err && t != active_triggers.end(); ++t) {

		AudioTrigger* at = dynamic_cast<AudioTrigger*> (*t);

		if (!at) {
			continue;
		}

		boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (at->region());
		const size_t nchans = ar->n_channels ();
		max_chans = std::max (max_chans, nchans);

		for (uint32_t chan = 0; !err && chan < nchans; ++chan) {

			AudioBuffer& buf = bufs.get_audio (chan);

			pframes_t to_copy = nframes;
			Sample* data = at->run (chan, to_copy, start_sample, end_sample, need_butler);

			if (!data) {
				/* XXX need to delete the trigger/put it back in the pool */
				t = active_triggers.erase (t);
				err = true;
			} else {
				if (t == active_triggers.begin()) {
					buf.read_from (data, to_copy);
					if (to_copy < nframes) {
						buf.silence (nframes - to_copy, to_copy);
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

Trigger::Trigger (boost::shared_ptr<Region> r)
	: _running (false)
	, _stop_requested (false)
	, _launch_style (Gate)
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

/*--------------------*/

AudioTrigger::AudioTrigger (boost::shared_ptr<AudioRegion> r)
	: Trigger (r)
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
AudioTrigger::bang (TriggerBox& /*proc*/, Temporal::Beats const &, samplepos_t)
{
	/* user triggered this, and we need to get things set up for calls to
	 * run()
	 */

	for (std::vector<samplecnt_t>::iterator ri = read_index.begin(); ri != read_index.end(); ++ri) {
		(*ri) = 0;
	}

	_running = true;
}

void
AudioTrigger::unbang (TriggerBox& /*proc*/, Temporal::Beats const &, samplepos_t)
{
	_stop_requested = true;
}

Sample*
AudioTrigger::run (uint32_t channel, pframes_t& nframes, samplepos_t /*start_sample*/, samplepos_t /*end_sample*/, bool& /* need_butler */)
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
