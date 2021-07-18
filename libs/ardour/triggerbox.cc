#include <iostream>

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
	PropertyList plist;

	the_source.reset (new SndFileSource (_session, "/music/misc/La_Voz_Del_Rio.wav", 0, Source::Flag (0)));

	plist.add (Properties::start, 0);
	plist.add (Properties::length, the_source->length ());
	plist.add (Properties::name, string ("bang"));
	plist.add (Properties::layer, 0);
	plist.add (Properties::layering_index, 0);

	boost::shared_ptr<Region> r = RegionFactory::create (the_source, plist, false);
	the_region = boost::dynamic_pointer_cast<AudioRegion> (r);

	/* XXX the_region/trigger will be looked up in a
	   std::map<MIDI::byte,Trigger>
	*/
	the_trigger = new AudioTrigger (the_region);
	add_trigger (the_trigger);
}

TriggerBox::~TriggerBox ()
{
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
	cerr << "TB: " << in << " / " << out << endl;
	return Processor::configure_io (in, out);
}

void
TriggerBox::add_trigger (Trigger* trigger)
{
	Glib::Threads::Mutex::Lock lm (trigger_lock);
	all_triggers.push_back (trigger);
	cerr << "Now have " << all_triggers.size() << " of all possible triggers\n";
}

bool
TriggerBox::queue_trigger (Trigger* trigger)
{
	return _trigger_queue.write (&trigger, 1) == 1;
}

void
TriggerBox::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool result_required)
{
	samplepos_t next_beat = 0;
	Temporal::Beats beats_now;

	/* check MIDI port input buffers for triggers */

	for (BufferSet::midi_iterator mi = bufs.midi_begin(); mi != bufs.midi_end(); ++mi) {
		MidiBuffer& mb (*mi);

		for (MidiBuffer::iterator ev = mb.begin(); ev != mb.end(); ++ev) {
			if ((*ev).is_note_on()) {

				if (!the_trigger->running()) {
					active_triggers.push_back (the_trigger);
				}

				the_trigger->bang (*this, beats_now, start_sample);
			}
		}
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

	/* if there are any triggers queued, make them active
	*/

	RingBuffer<Trigger*>::rw_vector vec;
	_trigger_queue.get_read_vector (&vec);

	for (uint32_t n = 0; n < vec.len[0]; ++n) {
		Trigger* t = vec.buf[0][n];
		t->bang (*this, beats_now, start_sample);
		active_triggers.push_back (t);
	}

	for (uint32_t n = 0; n < vec.len[1]; ++n) {
		Trigger* t = vec.buf[1][n];
		t->bang (*this, beats_now, start_sample);
		active_triggers.push_back (t);
	}

	_trigger_queue.increment_read_idx (vec.len[0] + vec.len[1]);

	bool err = false;
	const size_t nchans = the_region->n_channels ();
	bool need_butler = false;

	for (uint32_t chan = 0; chan < nchans; ++chan) {
		AudioBuffer& buf = bufs.get_audio (chan);
		buf.silence (nframes, 0);
	}

	for (Triggers::iterator t = active_triggers.begin(); !err && t != active_triggers.end(); ++t) {

		AudioTrigger* at = dynamic_cast<AudioTrigger*> (*t);

		if (!at) {
			continue;
		}

		for (uint32_t chan = 0; !err && chan < nchans; ++chan) {

			AudioBuffer& buf = bufs.get_audio (chan);

			pframes_t to_copy = nframes;
			Sample* data = at->run (chan, to_copy, start_sample, end_sample, need_butler);

			if (!data) {
				/* XXX need to delete the trigger/put it back in the pool */
				cerr << "trigger complete\n";
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

	ChanCount cc (DataType::AUDIO, nchans);
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

AudioTrigger::AudioTrigger (boost::shared_ptr<AudioRegion> r)
	: region (r)
	, data (0)
	, length (0)
{
	/* XXX catch region going away */

	const uint32_t nchans = region->n_channels();

	length = region->length_samples();

	for (uint32_t n = 0; n < nchans; ++n) {
		data.push_back (new Sample[length]);;
		read_index.push_back (0);
		region->read (data[n], 0, length, n);
	}
}

AudioTrigger::~AudioTrigger ()
{
	for (std::vector<Sample*>::iterator d = data.begin(); d != data.end(); ++d) {
		delete *d;
	}
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

Sample*
AudioTrigger::run (uint32_t channel, pframes_t& nframes, samplepos_t /*start_sample*/, samplepos_t /*end_sample*/, bool& /* need_butler */)
{
	if (!_running) {
		return 0;
	}

	if (read_index[channel] >= length) {
		return 0;
	}

	channel %= data.size();

	nframes = (pframes_t) std::min ((samplecnt_t) nframes, (length - read_index[channel]));
	read_index[channel] += nframes;

	return data[channel] + read_index[channel];
}
