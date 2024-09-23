/*
 * Copyright (C) 2023 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "pbd/compose.h"
#include "pbd/debug.h"
#include "pbd/pthread_utils.h"
#include "pbd/semutils.h"
#include "pbd/types_convert.h"

#include "temporal/beats.h"

#include "ardour/audio_buffer.h"
#include "ardour/audiofilesource.h"
#include "ardour/butler.h"
#include "ardour/cliprec.h"
#include "ardour/debug.h"
#include "ardour/midi_track.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;

PBD::Thread* ClipRecProcessor::_thread (0);
bool ClipRecProcessor::thread_should_run (false);
PBD::Semaphore* ClipRecProcessor::_semaphore (0);
ClipRecProcessor* ClipRecProcessor::currently_recording (nullptr);

ClipRecProcessor::ClipRecProcessor (Session& s, Track& t, std::string const & name, Temporal::TimeDomainProvider const & tdp)
	: DiskIOProcessor (s, t,name, DiskIOProcessor::Recordable, tdp)
{
	if (!_thread) {
		thread_should_run = true;
		_semaphore = new PBD::Semaphore (X_("cliprec"), 0);
		_thread = PBD::Thread::create (&ClipRecProcessor::thread_work);
	}
}

ClipRecProcessor::ArmInfo::ArmInfo (Trigger& s)
	: slot (s)
	, start (0)
	, end (0)
{
}

ClipRecProcessor::ArmInfo::~ArmInfo()
{
	for (auto & ab : audio_buf) {
		delete ab;
	}
}

void
ClipRecProcessor::arm_from_another_thread (Trigger& slot, samplepos_t now, timecnt_t const & expected_duration, uint32_t chans)
{
	using namespace Temporal;

	ArmInfo* ai = new ArmInfo (slot);
	ai->midi_buf.reset (new RTMidiBuffer);

	ai->midi_buf->resize (1024); // XXX Config->max_slot_midi_event_size

	for (uint32_t n = 0; n < chans; ++n) {
		ai->audio_buf.push_back (new Sample[10000]);
	}

	Beats start_b;
	Beats end_b;
	BBT_Argument t_bbt;
	Beats t_beats;
	samplepos_t t_samples;
	TempoMap::SharedPtr tmap (TempoMap::use());
	Beats now_beats = tmap->quarters_at (now);

	slot.compute_quantized_transition (now, now_beats, std::numeric_limits<Beats>::max(),
	                                   t_bbt, t_beats, t_samples, tmap, slot.quantization());

	ai->start = t_samples;
	ai->end = tmap->sample_at (now_beats + Beats (16, 0));

	set_armed (ai);
}

void
ClipRecProcessor::disarm ()
{
	set_armed (nullptr);
}

void
ClipRecProcessor::set_armed (ArmInfo* ai)
{
	if ((bool) _arm_info.load() == (bool) ai) {
		if (_arm_info.load()) {
			assert (currently_recording == this);
		}
		return;
	}

	if (!ai) {
		finish_recording ();
		assert (currently_recording == this);
		delete _arm_info;
		_arm_info = nullptr;
		currently_recording = nullptr;
		ArmedChanged (); // EMIT SIGNAL
		return;
	}

	if (currently_recording) {
		currently_recording->set_armed (nullptr);
		currently_recording = 0;
	}

	_arm_info = ai;
	currently_recording = this;
	start_recording ();
	ArmedChanged (); // EMIT SIGNAL
}

void
ClipRecProcessor::start_recording ()
{
}

void
ClipRecProcessor::finish_recording ()
{
	/* XXXX do something */
#if 0
	std::shared_ptr<ChannelList const> c = channels.reader();
	for (auto & chan : *c) {
		Source::WriterLock lock((chan)->write_source->mutex());
		(chan)->write_source->mark_streaming_write_completed (lock);
		(chan)->write_source->done_with_peakfile_writes ();
	}
#endif
}

void
ClipRecProcessor::thread_work ()
{
	while (thread_should_run) {
		_semaphore->wait ();
		ClipRecProcessor* crp = currently_recording;
		if (crp) {
			(void) crp->pull_data ();
		}
	}
}

int
ClipRecProcessor::pull_data ()
{
	int ret = 0;

#if 0
	uint32_t to_write;

	RingBufferNPT<Sample>::rw_vector vector;

	vector.buf[0] = 0;
	vector.buf[1] = 0;

	std::shared_ptr<ChannelList const> c = channels.reader();
	for (ChannelList::const_iterator chan = c->begin(); chan != c->end(); ++chan) {

		(*chan)->wbuf->get_read_vector (&vector);

		if (vector.len[0] + vector.len[1] == 0) {
			goto out;
		}

		to_write = vector.len[0];

		if ((!(*chan)->write_source) || (*chan)->write_source->write (vector.buf[0], to_write) != to_write) {
			// error << string_compose(_("AudioDiskstream %1: cannot write to disk"), id()) << endmsg;
			return -1;
		}

		(*chan)->wbuf->increment_read_ptr (to_write);
		// (*chan)->curr_capture_cnt += to_write;

		to_write = vector.len[1];

		DEBUG_TRACE (DEBUG::ClipRecording, string_compose ("%1 additional write of %2\n", name(), to_write));

		if ((*chan)->write_source->write (vector.buf[1], to_write) != to_write) {
			// error << string_compose(_("AudioDiskstream %1: cannot write to disk"), id()) << endmsg;
			return -1;
		}

		(*chan)->wbuf->increment_read_ptr (to_write);
		//(*chan)->curr_capture_cnt += to_write;
	}

	/* MIDI*/

	if (_midi_write_source && _midi_buf) {

		const samplecnt_t total = g_atomic_int_get (&_samples_pending_write);

		if (total == 0 || _midi_buf->read_space() == 0)
		    (!force_flush && (total < _chunk_samples) && _was_recording)) {
			goto out;
		}

	}
#endif

  out:
	return ret;
}

bool
ClipRecProcessor::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	if (in.n_midi() != 0 && in.n_midi() != 1) {
		/* we only support zero or 1 MIDI stream */
		return false;
	}

	/* currently no way to deliver different channels that we receive */
	out = in;

	return true;
}

void
ClipRecProcessor::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool result_required)
{
	if (!check_active()) {
		return;
	}

	const size_t n_buffers = bufs.count().n_audio();
	std::shared_ptr<ChannelList const> c = channels.reader();
	ChannelList::const_iterator chan;
	size_t n;
	ArmInfo* ai = _arm_info.load();

	if (!ai) {
		return;
	}

	/* Audio */

	if (n_buffers) {

		/* AUDIO */

		for (chan = c->begin(), n = 0; chan != c->end(); ++chan, ++n) {
			assert (ai->audio_buf.size() >= n);
			AudioBuffer& buf (bufs.get_audio (n%n_buffers));
			memcpy (buf.data(), ai->audio_buf[n], sizeof (Sample) * nframes);
		}
	}

	/* MIDI */

	MidiBuffer& buf    = bufs.get_midi (0);
	MidiTrack* mt = dynamic_cast<MidiTrack*>(&_track);
	MidiChannelFilter* filter = mt ? &mt->capture_filter() : 0;

	assert (buf.size() == 0 || _midi_buf);

	for (MidiBuffer::iterator i = buf.begin(); i != buf.end(); ++i) {
		Evoral::Event<MidiBuffer::TimeType> ev (*i, false);
		if (ev.time() > nframes) {
			break;
		}

		bool skip_event = false;

		if (mt) {
			/* skip injected immediate/out-of-band events */
			MidiBuffer const& ieb (mt->immediate_event_buffer());
			for (MidiBuffer::const_iterator j = ieb.begin(); j != ieb.end(); ++j) {
				if (*j == ev) {
					skip_event = true;
				}
			}
		}

		if (!skip_event && (!filter || !filter->filter(ev.buffer(), ev.size()))) {
			const samplepos_t event_time = start_sample + ev.time();
			ai->midi_buf->write (event_time,  ev.event_type(), ev.size(), ev.buffer());
		}
	}
}

float
ClipRecProcessor::buffer_load () const
{
	std::shared_ptr<ChannelList const> c = channels.reader();

	if (c->empty ()) {
		return 1.0;
	}

	return (float) ((double) c->front()->wbuf->write_space()/
			(double) c->front()->wbuf->bufsize());
}

void
ClipRecProcessor::adjust_buffering ()
{
	std::shared_ptr<ChannelList const> c = channels.reader();

	for (ChannelList::const_iterator chan = c->begin(); chan != c->end(); ++chan) {
		(*chan)->resize (_session.butler()->audio_capture_buffer_size());
	}
}

void
ClipRecProcessor::configuration_changed ()
{
	/* nothing to do */
}
