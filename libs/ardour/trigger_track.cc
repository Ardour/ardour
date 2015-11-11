/*
    Copyright (C) 2015 Paul Davis

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

#include <algorithm>

#include "ardour/audio_buffer.h"
#include "ardour/audioengine.h"
#include "ardour/audio_diskstream.h"
#include "ardour/audio_port.h"
#include "ardour/audioregion.h"
#include "ardour/io.h"
#include "ardour/trigger_track.h"

using namespace ARDOUR;

TriggerTrack::TriggerTrack (Session& s, std::string name, Route::Flag f, TrackMode m)
	: Track (s, name, f, m)
	, _trigger_queue (1024)
{
}

TriggerTrack::~TriggerTrack ()
{
}

int
TriggerTrack::init ()
{
	if (Track::init ()) {
		return -1;
	}

	boost::shared_ptr<Port> p = AudioEngine::instance()->register_input_port (DataType::MIDI, name(), false);
	if (!p) {
		return -1;
	}

	_midi_port = boost::dynamic_pointer_cast<MidiPort> (p);

	return 0;
}

bool
TriggerTrack::queue_trigger (Trigger* trigger)
{
	return _trigger_queue.write (&trigger, 1) == 1;
}

int
TriggerTrack::roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, int declick, bool& need_butler)
{
	/* get tempo map */

	/* find offset to next bar * and beat start
	 */

	framepos_t next_beat = 0;
	Evoral::Beats beats_now;

	/* if next beat occurs in this process cycle, see if we have any triggers waiting
	*/

	bool run_beats = false;
	// bool run_bars = false;

	if (next_beat >= start_frame && next_beat < end_frame) {
		run_beats = true;
	}

	/* if there are any triggers queued, run them.
	*/

	RingBuffer<Trigger*>::rw_vector vec;
	_trigger_queue.get_read_vector (&vec);

	for (uint32_t n = 0; n < vec.len[0]; ++n) {
		Trigger* t = vec.buf[0][n];
		t->bang (*this, beats_now, start_frame);
	}

	for (uint32_t n = 0; n < vec.len[1]; ++n) {
		Trigger* t = vec.buf[1][n];
		t->bang (*this, beats_now, start_frame);
	}

	_trigger_queue.decrement_read_idx (vec.len[0] + vec.len[1]);


	/* now run all active triggers */

	need_butler = false;

	uint32_t nchans = _diskstream->n_channels().n_audio();
	boost::shared_ptr<AudioDiskstream> ads = boost::dynamic_pointer_cast<AudioDiskstream> (_diskstream);

	if (!ads) {
		return 0;
	}

	bool err = false;

	for (uint32_t n = 0; !err && n < nchans; ++n) {

		AudioBuffer& port_buffer (_output->audio (n % _output->n_ports().n_audio())->get_audio_buffer (nframes));
		port_buffer.silence (nframes);

		for (Triggers::iterator t = active_triggers.begin(); !err && t != active_triggers.end(); ) {

			AudioTrigger* at = dynamic_cast<AudioTrigger*> (*t);

			if (!at) {
				continue;
			}
			pframes_t to_copy = nframes;

			Sample* data = at->run (n, to_copy, start_frame, end_frame, need_butler);

			if (!data) {
				/* XXX need to delete the trigger/put it back in the pool */
				t = active_triggers.erase (t);
				err = true;
			} else {
				if (t == active_triggers.begin()) {
					port_buffer.read_from (data, to_copy);
				} else {
					port_buffer.accumulate_from (data, to_copy);
				}
			}
		}
	}

	return 0;
}

void
TriggerTrack::realtime_handle_transport_stopped ()
{
}

void
TriggerTrack::realtime_locate ()
{
}

void
TriggerTrack::non_realtime_locate (framepos_t)
{
}

boost::shared_ptr<Diskstream>
TriggerTrack::create_diskstream ()
{
	return boost::shared_ptr<Diskstream> ();
}

void
TriggerTrack::set_diskstream (boost::shared_ptr<Diskstream>)
{
}

int
TriggerTrack::set_state (const XMLNode& root, int version)
{
	return Track::set_state (root, version);
}

XMLNode&
TriggerTrack::state (bool full_state)
{
	XMLNode& root (Track::state(full_state));
	return root;
}

int
TriggerTrack::no_roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, bool state_changing)
{
	return 0;
}

/*--------------------*/

AudioTrigger::AudioTrigger (boost::shared_ptr<AudioRegion> r)
	: region (r)
	, running (false)
	, data (0)
	, read_index (0)
	, length (0)
{
	/* XXX catch region going away */

	const uint32_t nchans = region->n_channels();

	for (uint32_t n = 0; n < nchans; ++n) {
		data.push_back (new Sample (region->length()));;
		region->read (data[n], 0, region->length(), 0);
	}

	length = region->length();
}

AudioTrigger::~AudioTrigger ()
{
	for (std::vector<Sample*>::iterator d = data.begin(); d != data.end(); ++d) {
		delete *d;
	}
}

void
AudioTrigger::bang (TriggerTrack& /*track*/, Evoral::Beats bangpos, framepos_t framepos)
{
	/* user triggered this, and we need to get things set up for calls to
	 * run()
	 */

	read_index = 0;
	running = true;
}

Sample*
AudioTrigger::run (uint32_t channel, pframes_t& nframes, framepos_t start_frame, framepos_t end_frame, bool& need_butler)
{
	if (!running) {
		return 0;
	}

	if (read_index > length) {
		return 0;
	}

	if (channel >= data.size()) {
		return 0;
	}

	nframes = (pframes_t) std::min ((framecnt_t) nframes, (length - read_index));

	return data[channel];
}
