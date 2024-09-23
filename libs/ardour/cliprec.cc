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
#include "ardour/types.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;

ClipRecProcessor* ClipRecProcessor::currently_recording (nullptr);

ClipRecProcessor::ClipRecProcessor (Session& s, Track& t, std::string const & name, DataType dt, Temporal::TimeDomainProvider const & tdp)
	: DiskIOProcessor (s, t,name, DiskIOProcessor::Recordable, tdp)
	, _data_type (dt)
{
	_display_to_user = false;
}

SlotArmInfo::SlotArmInfo (Trigger& s)
	: slot (s)
	, start (0)
	, end (0)
{
}

SlotArmInfo::~SlotArmInfo()
{
	for (auto & ab : audio_buf) {
		delete ab;
	}
}

void
ClipRecProcessor::arm_from_another_thread (Trigger& slot, samplepos_t now, timecnt_t const & expected_duration, uint32_t chans)
{
	using namespace Temporal;

	SlotArmInfo* ai = new SlotArmInfo (slot);

	if (_data_type == DataType::MIDI) {
		ai->midi_buf.reset (new RTMidiBuffer);
		ai->midi_buf->resize (1024); // XXX Config->max_slot_midi_event_size
	} else {
		for (uint32_t n = 0; n < chans; ++n) {
			ai->audio_buf.push_back (new Sample[_session.sample_rate() * 30]); // XXX Config->max_slot_audio_duration
		}
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
	ai->end = tmap->sample_at (now_beats + Beats (16, 0)); // XXX slot duration/length

	set_armed (ai);
}

void
ClipRecProcessor::disarm ()
{
	set_armed (nullptr);
}

void
ClipRecProcessor::set_armed (SlotArmInfo* ai)
{
	/* Must disarm before rearming */

	if ((bool) _arm_info.load() == (bool) ai) {
		if (ai) {
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
	ArmedChanged (); // EMIT SIGNAL
}

void
ClipRecProcessor::finish_recording ()
{
	SlotArmInfo* ai = _arm_info.load ();
	assert (ai);

	ai->slot.captured (*ai);
	_arm_info = nullptr;
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
	SlotArmInfo* ai = _arm_info.load();

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
	return 1.0;
}

void
ClipRecProcessor::adjust_buffering ()
{
}

void
ClipRecProcessor::configuration_changed ()
{
	/* nothing to do */
}

XMLNode&
ClipRecProcessor::state () const
{
	XMLNode& node (DiskIOProcessor::state ());
	node.set_property (X_("type"), X_("cliprec"));
	return node;
}

int
ClipRecProcessor::set_state (const XMLNode& node, int version)
{
	if (DiskIOProcessor::set_state (node, version)) {
		return -1;
	}

	return 0;
}

std::string
ClipRecProcessor::display_name () const
{
	return std::string (_("Cue Recorder"));
}
