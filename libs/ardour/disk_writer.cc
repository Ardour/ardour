/*
    Copyright (C) 2009-2016 Paul Davis

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

#include "ardour/analyser.h"
#include "ardour/audioengine.h"
#include "ardour/audiofilesource.h"
#include "ardour/audio_buffer.h"
#include "ardour/audioplaylist.h"
#include "ardour/audioregion.h"
#include "ardour/butler.h"
#include "ardour/debug.h"
#include "ardour/disk_writer.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_source.h"
#include "ardour/midi_track.h"
#include "ardour/port.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/smf_source.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

ARDOUR::samplecnt_t DiskWriter::_chunk_samples = DiskWriter::default_chunk_samples ();
PBD::Signal0<void> DiskWriter::Overrun;

DiskWriter::DiskWriter (Session& s, string const & str, DiskIOProcessor::Flag f)
	: DiskIOProcessor (s, str, f)
	, _record_enabled (0)
	, _record_safe (0)
	, capture_start_sample (0)
	, capture_captured (0)
	, was_recording (false)
	, first_recordable_sample (max_samplepos)
	, last_recordable_sample (max_samplepos)
	, last_possibly_recording (0)
	, _alignment_style (ExistingMaterial)
	, _num_captured_loops (0)
	, _accumulated_capture_offset (0)
	, _gui_feed_buffer(AudioEngine::instance()->raw_buffer_size (DataType::MIDI))
{
	DiskIOProcessor::init ();
}

DiskWriter::~DiskWriter ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("DiskWriter %1 @ %2 deleted\n", _name, this));

	boost::shared_ptr<ChannelList> c = channels.reader();

	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
		(*chan)->write_source.reset ();
	}
}

samplecnt_t
DiskWriter::default_chunk_samples ()
{
	return 65536;
}

void
DiskWriter::WriterChannelInfo::resize (samplecnt_t bufsize)
{
	if (!capture_transition_buf) {
		capture_transition_buf = new RingBufferNPT<CaptureTransition> (256);
	}
	delete wbuf;
	wbuf = new RingBufferNPT<Sample> (bufsize);
	/* touch memory to lock it */
	memset (wbuf->buffer(), 0, sizeof (Sample) * wbuf->bufsize());
}

int
DiskWriter::add_channel_to (boost::shared_ptr<ChannelList> c, uint32_t how_many)
{
	while (how_many--) {
		c->push_back (new WriterChannelInfo (_session.butler()->audio_diskstream_capture_buffer_size()));
		DEBUG_TRACE (DEBUG::DiskIO, string_compose ("%1: new writer channel, write space = %2 read = %3\n",
		                                            name(),
		                                            c->back()->wbuf->write_space(),
		                                            c->back()->wbuf->read_space()));
	}

	return 0;
}



bool
DiskWriter::set_write_source_name (string const & str)
{
	_write_source_name = str;
	return true;
}

void
DiskWriter::check_record_status (samplepos_t transport_sample, double speed, bool can_record)
{
	int possibly_recording;
	const int transport_rolling = 0x4;
	const int track_rec_enabled = 0x2;
	const int global_rec_enabled = 0x1;
	const int fully_rec_enabled = (transport_rolling |track_rec_enabled | global_rec_enabled);

	/* merge together the 3 factors that affect record status, and compute what has changed. */

	possibly_recording = (speed != 0.0f ? 4 : 0)  | (record_enabled() ? 2 : 0) | (can_record ? 1 : 0);

	if (possibly_recording == last_possibly_recording) {
		return;
	}

	if (possibly_recording == fully_rec_enabled) {

		if (last_possibly_recording == fully_rec_enabled) {
			return;
		}

		Location* loc;
		if  (_session.config.get_punch_in () && 0 != (loc = _session.locations()->auto_punch_location ())) {
			capture_start_sample = loc->start ();
		} else {
			capture_start_sample = _session.transport_sample ();
		}

		first_recordable_sample = capture_start_sample;

		if (_alignment_style == ExistingMaterial) {
			first_recordable_sample += _capture_offset + _playback_offset;
		}

		if  (_session.config.get_punch_out () && 0 != (loc = _session.locations()->auto_punch_location ())) {
			/* this freezes the punch-out point when starting to record.
			 *
			 * We should allow to move it or at least allow to disable punch-out
			 * while rolling..
			 */
			last_recordable_sample = loc->end ();
			if (_alignment_style == ExistingMaterial) {
				last_recordable_sample += _capture_offset + _playback_offset;
			}
		} else {
			last_recordable_sample = max_samplepos;
		}

		DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1: @ %2 (STS: %3) CS:%4 FRS: %5 IL: %7, OL: %8 CO: %r9 PO: %10 WOL: %11 WIL: %12\n",
		                                                      name(),
		                                                      transport_sample,
		                                                      _session.transport_sample(),
																													capture_start_sample,
																													first_recordable_sample,
																													last_recordable_sample,
		                                                      _input_latency,
		                                                      _output_latency,
		                                                      _capture_offset,
		                                                      _playback_offset,
		                                                      _session.worst_output_latency(),
		                                                      _session.worst_input_latency()));


		prepare_record_status (capture_start_sample);

	}

	last_possibly_recording = possibly_recording;
}

void
DiskWriter::calculate_record_range (Evoral::OverlapType ot, samplepos_t transport_sample, samplecnt_t nframes, samplecnt_t & rec_nframes, samplecnt_t & rec_offset)
{
	switch (ot) {
	case Evoral::OverlapNone:
		rec_nframes = 0;
		break;

	case Evoral::OverlapInternal:
		/*     ----------    recrange
		 *       |---|       transrange
		 */
		rec_nframes = nframes;
		rec_offset = 0;
		break;

	case Evoral::OverlapStart:
		/*    |--------|    recrange
		 *  -----|          transrange
		 */
		rec_nframes = transport_sample + nframes - first_recordable_sample;
		if (rec_nframes) {
			rec_offset = first_recordable_sample - transport_sample;
		}
		break;

	case Evoral::OverlapEnd:
		/*    |--------|    recrange
		 *       |--------  transrange
		 */
		rec_nframes = last_recordable_sample - transport_sample;
		rec_offset = 0;
		break;

	case Evoral::OverlapExternal:
		/*    |--------|    recrange
		 *  --------------  transrange
		 */
		rec_nframes = last_recordable_sample - first_recordable_sample;
		rec_offset = first_recordable_sample - transport_sample;
		break;
	}

	DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1 rec? %2 @ %3 (for %4) FRF %5 LRF %6 : rf %7 @ %8\n",
	                                                      _name, enum_2_string (ot), transport_sample, nframes,
	                                                      first_recordable_sample, last_recordable_sample, rec_nframes, rec_offset));
}

void
DiskWriter::engage_record_enable ()
{
	g_atomic_int_set (&_record_enabled, 1);
}

void
DiskWriter::disengage_record_enable ()
{
	g_atomic_int_set (&_record_enabled, 0);
}

void
DiskWriter::engage_record_safe ()
{
	g_atomic_int_set (&_record_safe, 1);
}

void
DiskWriter::disengage_record_safe ()
{
	g_atomic_int_set (&_record_safe, 0);
}

/** Get the start position (in session samples) of the nth capture in the current pass */
ARDOUR::samplepos_t
DiskWriter::get_capture_start_sample (uint32_t n) const
{
	Glib::Threads::Mutex::Lock lm (capture_info_lock);

	if (capture_info.size() > n) {
		/* this is a completed capture */
		return capture_info[n]->start;
	} else {
		/* this is the currently in-progress capture */
		return capture_start_sample;
	}
}

ARDOUR::samplecnt_t
DiskWriter::get_captured_samples (uint32_t n) const
{
	Glib::Threads::Mutex::Lock lm (capture_info_lock);

	if (capture_info.size() > n) {
		/* this is a completed capture */
		return capture_info[n]->samples;
	} else {
		/* this is the currently in-progress capture */
		return capture_captured;
	}
}

void
DiskWriter::set_align_style (AlignStyle a, bool force)
{
	if (record_enabled() && _session.actively_recording()) {
		return;
	}

	if ((a != _alignment_style) || force) {
		_alignment_style = a;
		AlignmentStyleChanged ();
	}
}

XMLNode&
DiskWriter::state ()
{
	XMLNode& node (DiskIOProcessor::state ());
	node.set_property (X_("type"), X_("diskwriter"));
	node.set_property (X_("record-safe"), (_record_safe ? X_("yes" : "no")));
	return node;
}

int
DiskWriter::set_state (const XMLNode& node, int version)
{
	if (DiskIOProcessor::set_state (node, version)) {
		return -1;
	}

	if (!node.get_property (X_("record-safe"), _record_safe)) {
		_record_safe = false;
	}

	reset_write_sources (false, true);

	return 0;
}

void
DiskWriter::non_realtime_locate (samplepos_t position)
{
	if (_midi_write_source) {
		_midi_write_source->set_timeline_position (position);
	}

	DiskIOProcessor::non_realtime_locate (position);
}


void
DiskWriter::prepare_record_status (samplepos_t capture_start_sample)
{
	if (recordable() && destructive()) {
		boost::shared_ptr<ChannelList> c = channels.reader ();
		for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {

			RingBufferNPT<CaptureTransition>::rw_vector transitions;
			(*chan)->capture_transition_buf->get_write_vector (&transitions);

			if (transitions.len[0] > 0) {
				transitions.buf[0]->type = CaptureStart;
				transitions.buf[0]->capture_val = capture_start_sample;
				(*chan)->capture_transition_buf->increment_write_ptr(1);
			} else {
				// bad!
				fatal << X_("programming error: capture_transition_buf is full on rec start!  inconceivable!")
					<< endmsg;
			}
		}
	}
}


/** Do some record stuff [not described in this comment!]
 *
 *  Also:
 *    - Setup playback_distance with the nframes, or nframes adjusted
 *      for current varispeed, if appropriate.
 *    - Setup current_playback_buffer in each ChannelInfo to point to data
 *      that someone can read playback_distance worth of data from.
 */
void
DiskWriter::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample,
                 double speed, pframes_t nframes, bool result_required)
{
	if (!_active && !_pending_active) {
		return;
	}
	_active = _pending_active;

	uint32_t n;
	boost::shared_ptr<ChannelList> c = channels.reader();
	ChannelList::iterator chan;

	samplecnt_t rec_offset = 0;
	samplecnt_t rec_nframes = 0;
	bool nominally_recording;

	bool re = record_enabled ();
	bool punch_in = _session.config.get_punch_in () && _session.locations()->auto_punch_location ();
	bool can_record = _session.actively_recording ();
	can_record |= speed != 0 && _session.get_record_enabled () && punch_in && _session.transport_sample () <= _session.locations()->auto_punch_location ()->start ();

	_need_butler = false;

#ifndef NDEBUG
	if (speed != 0 && re) {
		DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1: run() start: %2 end: %3 NF: %4\n", _name, start_sample, end_sample, nframes));
	}
#endif

	check_record_status (start_sample, speed, can_record);

	if (nframes == 0) {
		return;
	}

	nominally_recording = (can_record && re);

	// Safeguard against situations where process() goes haywire when autopunching
	// and last_recordable_sample < first_recordable_sample

	if (last_recordable_sample < first_recordable_sample) {
		last_recordable_sample = max_samplepos;
	}

	const Location* const loop_loc    = _loop_location;
	samplepos_t           loop_start  = 0;
	samplepos_t           loop_end    = 0;
	samplepos_t           loop_length = 0;

	if (loop_loc) {
		get_location_times (loop_loc, &loop_start, &loop_end, &loop_length);
	}

	if (nominally_recording || (re && was_recording && _session.get_record_enabled() && punch_in)) {

		Evoral::OverlapType ot = Evoral::coverage (first_recordable_sample, last_recordable_sample, start_sample, end_sample);
		// XXX should this be transport_sample + nframes - 1 ? coverage() expects its parameter ranges to include their end points
		// XXX also, first_recordable_sample & last_recordable_sample may both be == max_samplepos: coverage() will return OverlapNone in that case. Is thak OK?
		calculate_record_range (ot, start_sample, nframes, rec_nframes, rec_offset);

		DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1: this time record %2 of %3 samples, offset %4\n", _name, rec_nframes, nframes, rec_offset));

		if (rec_nframes && !was_recording) {
			capture_captured = 0;

			if (loop_loc) {
				/* Loop recording, so pretend the capture started at the loop
				   start rgardless of what time it is now, so the source starts
				   at the loop start and can handle time wrapping around.
				   Otherwise, start the source right now as usual.
				*/
				capture_captured     = start_sample - loop_start;
				capture_start_sample = loop_start;
			}

			if (_midi_write_source) {
				_midi_write_source->mark_write_starting_now (capture_start_sample, capture_captured, loop_length);
			}

			g_atomic_int_set (const_cast<gint*> (&_samples_pending_write), 0);
			g_atomic_int_set (const_cast<gint*> (&_num_captured_loops), 0);

			was_recording = true;

		}

		/* For audio: not writing samples to the capture ringbuffer offsets
		 * the recording. For midi: we need to keep track of the record range
		 * and subtract the accumulated difference from the event time.
		 */
		if (rec_nframes) {
			_accumulated_capture_offset += rec_offset;
		} else {
			_accumulated_capture_offset += nframes;
		}

	}

	if (can_record && !_last_capture_sources.empty ()) {
		_last_capture_sources.clear ();
	}

	if (rec_nframes) {

		/* AUDIO */

		const size_t n_buffers = bufs.count().n_audio();

		for (chan = c->begin(), n = 0; chan != c->end(); ++chan, ++n) {

			ChannelInfo* chaninfo (*chan);
			AudioBuffer& buf (bufs.get_audio (n%n_buffers));

			chaninfo->wbuf->get_write_vector (&chaninfo->rw_vector);

			if (rec_nframes <= (samplecnt_t) chaninfo->rw_vector.len[0]) {

				Sample *incoming = buf.data (rec_offset);
				memcpy (chaninfo->rw_vector.buf[0], incoming, sizeof (Sample) * rec_nframes);

			} else {

				samplecnt_t total = chaninfo->rw_vector.len[0] + chaninfo->rw_vector.len[1];

				if (rec_nframes > total) {
					DEBUG_TRACE (DEBUG::Butler, string_compose ("%1 overrun in %2, rec_nframes = %3 total space = %4\n",
					                                            DEBUG_THREAD_SELF, name(), rec_nframes, total));
					Overrun ();
					return;
				}

				Sample *incoming = buf.data (rec_offset);
				samplecnt_t first = chaninfo->rw_vector.len[0];

				memcpy (chaninfo->rw_vector.buf[0], incoming, sizeof (Sample) * first);
				memcpy (chaninfo->rw_vector.buf[1], incoming + first, sizeof (Sample) * (rec_nframes - first));
			}

			chaninfo->wbuf->increment_write_ptr (rec_nframes);

		}

		/* MIDI */

		// Pump entire port buffer into the ring buffer (TODO: split cycles?)
		MidiBuffer& buf    = bufs.get_midi (0);
		boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack>(_route);
		MidiChannelFilter* filter = mt ? &mt->capture_filter() : 0;

		for (MidiBuffer::iterator i = buf.begin(); i != buf.end(); ++i) {
			Evoral::Event<MidiBuffer::TimeType> ev (*i, false);
			if (ev.time() + rec_offset > rec_nframes) {
				break;
			}
#ifndef NDEBUG
			if (DEBUG_ENABLED(DEBUG::MidiIO)) {
				const uint8_t* __data = ev.buffer();
				DEBUG_STR_DECL(a);
				DEBUG_STR_APPEND(a, string_compose ("mididiskstream %1 capture event @ %2 + %3 sz %4 ", this, ev.time(), start_sample, ev.size()));
				for (size_t i=0; i < ev.size(); ++i) {
					DEBUG_STR_APPEND(a,hex);
					DEBUG_STR_APPEND(a,"0x");
					DEBUG_STR_APPEND(a,(int)__data[i]);
					DEBUG_STR_APPEND(a,' ');
				}
				DEBUG_STR_APPEND(a,'\n');
				DEBUG_TRACE (DEBUG::MidiIO, DEBUG_STR(a).str());
			}
#endif
			/* Write events to the capture buffer in samples from session start,
			   but ignoring looping so event time progresses monotonically.
			   The source knows the loop length so it knows exactly where the
			   event occurs in the series of recorded loops and can implement
			   any desirable behaviour.  We don't want to send event with
			   transport time here since that way the source can not
			   reconstruct their actual time; future clever MIDI looping should
			   probably be implemented in the source instead of here.
			*/
			const samplecnt_t loop_offset = _num_captured_loops * loop_length;
			const samplepos_t event_time = start_sample + loop_offset - _accumulated_capture_offset + ev.time();
			if (event_time < 0 || event_time < first_recordable_sample) {
				/* Event out of range, skip */
				continue;
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
			if (skip_event) {
				continue;
			}

			if (!filter || !filter->filter(ev.buffer(), ev.size())) {
				_midi_buf->write (event_time, ev.event_type(), ev.size(), ev.buffer());
			}
		}
		g_atomic_int_add (const_cast<gint*>(&_samples_pending_write), nframes);

		if (buf.size() != 0) {
			Glib::Threads::Mutex::Lock lm (_gui_feed_buffer_mutex, Glib::Threads::TRY_LOCK);

			if (lm.locked ()) {
				/* Copy this data into our GUI feed buffer and tell the GUI
				   that it can read it if it likes.
				*/
				_gui_feed_buffer.clear ();

				for (MidiBuffer::iterator i = buf.begin(); i != buf.end(); ++i) {
					/* This may fail if buf is larger than _gui_feed_buffer, but it's not really
					   the end of the world if it does.
					*/
					_gui_feed_buffer.push_back ((*i).time() + start_sample, (*i).size(), (*i).buffer());
				}
			}

			DataRecorded (_midi_write_source); /* EMIT SIGNAL */
		}

		capture_captured += rec_nframes;
		DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1 now captured %2 (by %3)\n", name(), capture_captured, rec_nframes));

	} else {

		/* not recording this time, but perhaps we were before .. */

		if (was_recording) {
			finish_capture (c);
			_accumulated_capture_offset = 0;
		}
	}

	/* AUDIO BUTLER REQUIRED CODE */

	if (_playlists[DataType::AUDIO] && !c->empty()) {
		if (((samplecnt_t) c->front()->wbuf->read_space() >= _chunk_samples)) {
			_need_butler = true;
		}
	}

	/* MIDI BUTLER REQUIRED CODE */

	if (_playlists[DataType::MIDI] && (_midi_buf->read_space() < _midi_buf->bufsize() / 2)) {
		_need_butler = true;
	}

	// DEBUG_TRACE (DEBUG::Butler, string_compose ("%1 writer run, needs butler = %2\n", name(), _need_butler));
}

void
DiskWriter::finish_capture (boost::shared_ptr<ChannelList> c)
{
	was_recording = false;
	first_recordable_sample = max_samplepos;
	last_recordable_sample = max_samplepos;

	if (capture_captured == 0) {
		return;
	}

	if (recordable() && destructive()) {
		for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {

			RingBufferNPT<CaptureTransition>::rw_vector transvec;
			(*chan)->capture_transition_buf->get_write_vector(&transvec);

			if (transvec.len[0] > 0) {
				transvec.buf[0]->type = CaptureEnd;
				transvec.buf[0]->capture_val = capture_captured;
				(*chan)->capture_transition_buf->increment_write_ptr(1);
			}
			else {
				// bad!
				fatal << string_compose (_("programmer error: %1"), X_("capture_transition_buf is full when stopping record!  inconceivable!")) << endmsg;
			}
		}
	}


	CaptureInfo* ci = new CaptureInfo;

	ci->start =  capture_start_sample;
	ci->samples = capture_captured;

	DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("Finish capture, add new CI, %1 + %2\n", ci->start, ci->samples));

	/* XXX theoretical race condition here. Need atomic exchange ?
	   However, the circumstances when this is called right
	   now (either on record-disable or transport_stopped)
	   mean that no actual race exists. I think ...
	   We now have a capture_info_lock, but it is only to be used
	   to synchronize in the transport_stop and the capture info
	   accessors, so that invalidation will not occur (both non-realtime).
	*/

	capture_info.push_back (ci);
	capture_captured = 0;

	/* now we've finished a capture, reset first_recordable_sample for next time */
	first_recordable_sample = max_samplepos;
}

boost::shared_ptr<MidiBuffer>
DiskWriter::get_gui_feed_buffer () const
{
	boost::shared_ptr<MidiBuffer> b (new MidiBuffer (AudioEngine::instance()->raw_buffer_size (DataType::MIDI)));

	Glib::Threads::Mutex::Lock lm (_gui_feed_buffer_mutex);
	b->copy (_gui_feed_buffer);
	return b;
}

void
DiskWriter::set_record_enabled (bool yn)
{
	if (!recordable() || !_session.record_enabling_legal() || record_safe ()) {
		return;
	}

	/* can't rec-enable in destructive mode if transport is before start */

	if (destructive() && yn && _session.transport_sample() < _session.current_start_sample()) {
		return;
	}

	/* yes, i know that this not proof against race conditions, but its
	   good enough. i think.
	*/

	if (record_enabled() != yn) {
		if (yn) {
			engage_record_enable ();
		} else {
			disengage_record_enable ();
		}

		RecordEnableChanged (); /* EMIT SIGNAL */
	}
}

void
DiskWriter::set_record_safe (bool yn)
{
	if (!recordable() || !_session.record_enabling_legal() || channels.reader()->empty()) {
		return;
	}

	/* can't rec-safe in destructive mode if transport is before start ????
	 REQUIRES REVIEW */

	if (destructive() && yn && _session.transport_sample() < _session.current_start_sample()) {
		return;
	}

	/* yes, i know that this not proof against race conditions, but its
	 good enough. i think.
	 */

	if (record_safe () != yn) {
		if (yn) {
			engage_record_safe ();
		} else {
			disengage_record_safe ();
		}

		RecordSafeChanged (); /* EMIT SIGNAL */
	}
}

bool
DiskWriter::prep_record_enable ()
{
	if (!recordable() || !_session.record_enabling_legal() || channels.reader()->empty() || record_safe ()) { // REQUIRES REVIEW "|| record_safe ()"
		return false;
	}

	/* can't rec-enable in destructive mode if transport is before start */

	if (destructive() && _session.transport_sample() < _session.current_start_sample()) {
		return false;
	}

	boost::shared_ptr<ChannelList> c = channels.reader();

	capturing_sources.clear ();

	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
		capturing_sources.push_back ((*chan)->write_source);
		Source::Lock lock((*chan)->write_source->mutex());
		(*chan)->write_source->mark_streaming_write_started (lock);
	}

	return true;
}

bool
DiskWriter::prep_record_disable ()
{
	capturing_sources.clear ();
	return true;
}

float
DiskWriter::buffer_load () const
{
	boost::shared_ptr<ChannelList> c = channels.reader();

	if (c->empty ()) {
		return 1.0;
	}

	return (float) ((double) c->front()->wbuf->write_space()/
			(double) c->front()->wbuf->bufsize());
}

void
DiskWriter::set_note_mode (NoteMode m)
{
	_note_mode = m;

	boost::shared_ptr<MidiPlaylist> mp = boost::dynamic_pointer_cast<MidiPlaylist> (_playlists[DataType::MIDI]);

	if (mp) {
		mp->set_note_mode (m);
	}

	if (_midi_write_source && _midi_write_source->model())
		_midi_write_source->model()->set_note_mode(m);
}

int
DiskWriter::seek (samplepos_t sample, bool complete_refill)
{
	uint32_t n;
	ChannelList::iterator chan;
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (n = 0, chan = c->begin(); chan != c->end(); ++chan, ++n) {
		(*chan)->wbuf->reset ();
	}

	_midi_buf->reset ();
	g_atomic_int_set(&_samples_read_from_ringbuffer, 0);
	g_atomic_int_set(&_samples_written_to_ringbuffer, 0);

	/* can't rec-enable in destructive mode if transport is before start */

	if (destructive() && record_enabled() && sample < _session.current_start_sample()) {
		disengage_record_enable ();
	}

	playback_sample = sample;

	return 0;
}

int
DiskWriter::do_flush (RunContext ctxt, bool force_flush)
{
	uint32_t to_write;
	int32_t ret = 0;
	RingBufferNPT<Sample>::rw_vector vector;
	RingBufferNPT<CaptureTransition>::rw_vector transvec;
	samplecnt_t total;

	transvec.buf[0] = 0;
	transvec.buf[1] = 0;
	vector.buf[0] = 0;
	vector.buf[1] = 0;

	boost::shared_ptr<ChannelList> c = channels.reader();
	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {

		(*chan)->wbuf->get_read_vector (&vector);

		total = vector.len[0] + vector.len[1];

		if (total == 0 || (total < _chunk_samples && !force_flush && was_recording)) {
			goto out;
		}

		/* if there are 2+ chunks of disk i/o possible for
		   this track, let the caller know so that it can arrange
		   for us to be called again, ASAP.

		   if we are forcing a flush, then if there is* any* extra
		   work, let the caller know.

		   if we are no longer recording and there is any extra work,
		   let the caller know too.
		*/

		if (total >= 2 * _chunk_samples || ((force_flush || !was_recording) && total > _chunk_samples)) {
			ret = 1;
		}

		to_write = min (_chunk_samples, (samplecnt_t) vector.len[0]);

		// check the transition buffer when recording destructive
		// important that we get this after the capture buf

		if (destructive()) {
			(*chan)->capture_transition_buf->get_read_vector(&transvec);
			size_t transcount = transvec.len[0] + transvec.len[1];
			size_t ti;

			for (ti=0; ti < transcount; ++ti) {
				CaptureTransition & captrans = (ti < transvec.len[0]) ? transvec.buf[0][ti] : transvec.buf[1][ti-transvec.len[0]];

				if (captrans.type == CaptureStart) {
					// by definition, the first data we got above represents the given capture pos

					(*chan)->write_source->mark_capture_start (captrans.capture_val);
					(*chan)->curr_capture_cnt = 0;

				} else if (captrans.type == CaptureEnd) {

					// capture end, the capture_val represents total samples in capture

					if (captrans.capture_val <= (*chan)->curr_capture_cnt + to_write) {

						// shorten to make the write a perfect fit
						uint32_t nto_write = (captrans.capture_val - (*chan)->curr_capture_cnt);

						if (nto_write < to_write) {
							ret = 1; // should we?
						}
						to_write = nto_write;

						(*chan)->write_source->mark_capture_end ();

						// increment past this transition, but go no further
						++ti;
						break;
					}
					else {
						// actually ends just beyond this chunk, so force more work
						ret = 1;
						break;
					}
				}
			}

			if (ti > 0) {
				(*chan)->capture_transition_buf->increment_read_ptr(ti);
			}
		}

		if ((!(*chan)->write_source) || (*chan)->write_source->write (vector.buf[0], to_write) != to_write) {
			error << string_compose(_("AudioDiskstream %1: cannot write to disk"), id()) << endmsg;
			return -1;
		}

		(*chan)->wbuf->increment_read_ptr (to_write);
		(*chan)->curr_capture_cnt += to_write;

		if ((to_write == vector.len[0]) && (total > to_write) && (to_write < _chunk_samples) && !destructive()) {

			/* we wrote all of vector.len[0] but it wasn't an entire
			   disk_write_chunk_samples of data, so arrange for some part
			   of vector.len[1] to be flushed to disk as well.
			*/

			to_write = min ((samplecnt_t)(_chunk_samples - to_write), (samplecnt_t) vector.len[1]);

                        DEBUG_TRACE (DEBUG::Butler, string_compose ("%1 additional write of %2\n", name(), to_write));

			if ((*chan)->write_source->write (vector.buf[1], to_write) != to_write) {
				error << string_compose(_("AudioDiskstream %1: cannot write to disk"), id()) << endmsg;
				return -1;
			}

			(*chan)->wbuf->increment_read_ptr (to_write);
			(*chan)->curr_capture_cnt += to_write;
		}
	}

	/* MIDI*/

	if (_midi_write_source) {

		const samplecnt_t total = g_atomic_int_get(const_cast<gint*> (&_samples_pending_write));

		if (total == 0 ||
		    _midi_buf->read_space() == 0 ||
		    (!force_flush && (total < _chunk_samples) && was_recording)) {
			goto out;
		}

		/* if there are 2+ chunks of disk i/o possible for
		   this track), let the caller know so that it can arrange
		   for us to be called again, ASAP.

		   if we are forcing a flush, then if there is* any* extra
		   work, let the caller know.

		   if we are no longer recording and there is any extra work,
		   let the caller know too.
		*/

		if (total >= 2 * _chunk_samples || ((force_flush || !was_recording) && total > _chunk_samples)) {
			ret = 1;
		}

		if (force_flush) {
			/* push out everything we have, right now */
			to_write = UINT32_MAX;
		} else {
			to_write = _chunk_samples;
		}

		if (record_enabled() && ((total > _chunk_samples) || force_flush)) {
			Source::Lock lm(_midi_write_source->mutex());
			if (_midi_write_source->midi_write (lm, *_midi_buf, get_capture_start_sample (0), to_write) != to_write) {
				error << string_compose(_("MidiDiskstream %1: cannot write to disk"), id()) << endmsg;
				return -1;
			}
			g_atomic_int_add(const_cast<gint*> (&_samples_pending_write), -to_write);
		}
	}

  out:
	return ret;

}

void
DiskWriter::reset_write_sources (bool mark_write_complete, bool /*force*/)
{
	ChannelList::iterator chan;
	boost::shared_ptr<ChannelList> c = channels.reader();
	uint32_t n;

	if (!_session.writable() || !recordable()) {
		return;
	}

	capturing_sources.clear ();

	for (chan = c->begin(), n = 0; chan != c->end(); ++chan, ++n) {

		if (!destructive()) {

			if ((*chan)->write_source) {

				if (mark_write_complete) {
					Source::Lock lock((*chan)->write_source->mutex());
					(*chan)->write_source->mark_streaming_write_completed (lock);
					(*chan)->write_source->done_with_peakfile_writes ();
				}

				if ((*chan)->write_source->removable()) {
					(*chan)->write_source->mark_for_remove ();
					(*chan)->write_source->drop_references ();
				}

				(*chan)->write_source.reset ();
			}

			use_new_write_source (DataType::AUDIO, n);

			if (record_enabled()) {
				capturing_sources.push_back ((*chan)->write_source);
			}

		} else {

			if ((*chan)->write_source == 0) {
				use_new_write_source (DataType::AUDIO, n);
			}
		}
	}

	if (_midi_write_source) {
		if (mark_write_complete) {
			Source::Lock lm(_midi_write_source->mutex());
			_midi_write_source->mark_streaming_write_completed (lm);
		}
	}

	if (_playlists[DataType::MIDI]) {
		use_new_write_source (DataType::MIDI);
	}

	if (destructive() && !c->empty ()) {

		/* we now have all our write sources set up, so create the
		   playlist's single region.
		*/

		if (_playlists[DataType::MIDI]->empty()) {
			setup_destructive_playlist ();
		}
	}
}

int
DiskWriter::use_new_write_source (DataType dt, uint32_t n)
{
	if (dt == DataType::MIDI) {

		_accumulated_capture_offset = 0;
		_midi_write_source.reset();

		try {
			_midi_write_source = boost::dynamic_pointer_cast<SMFSource>(
				_session.create_midi_source_for_session (write_source_name ()));

			if (!_midi_write_source) {
				throw failed_constructor();
			}
		}

		catch (failed_constructor &err) {
			error << string_compose (_("%1:%2 new capture file not initialized correctly"), _name, n) << endmsg;
			_midi_write_source.reset();
			return -1;
		}
	} else {
		boost::shared_ptr<ChannelList> c = channels.reader();

		if (!recordable()) {
			return 1;
		}

		if (n >= c->size()) {
			error << string_compose (_("AudioDiskstream: channel %1 out of range"), n) << endmsg;
			return -1;
		}

		ChannelInfo* chan = (*c)[n];

		try {
			if ((chan->write_source = _session.create_audio_source_for_session (
				     c->size(), write_source_name(), n, destructive())) == 0) {
				throw failed_constructor();
			}
		}

		catch (failed_constructor &err) {
			error << string_compose (_("%1:%2 new capture file not initialized correctly"), _name, n) << endmsg;
			chan->write_source.reset ();
			return -1;
		}

		/* do not remove destructive files even if they are empty */

		chan->write_source->set_allow_remove_if_empty (!destructive());
	}

	return 0;
}

void
DiskWriter::transport_stopped_wallclock (struct tm& when, time_t twhen, bool abort_capture)
{
	bool more_work = true;
	int err = 0;
	samplecnt_t total_capture;
	SourceList audio_srcs;
	SourceList midi_srcs;
	ChannelList::iterator chan;
	vector<CaptureInfo*>::iterator ci;
	boost::shared_ptr<ChannelList> c = channels.reader();
	uint32_t n = 0;
	bool mark_write_completed = false;

	finish_capture (c);


	/* butler is already stopped, but there may be work to do
	   to flush remaining data to disk.
	*/

	while (more_work && !err) {
		switch (do_flush (TransportContext, true)) {
		case 0:
			more_work = false;
			break;
		case 1:
			break;
		case -1:
			error << string_compose(_("AudioDiskstream \"%1\": cannot flush captured data to disk!"), _name) << endmsg;
			err++;
		}
	}

	/* XXX is there anything we can do if err != 0 ? */
	Glib::Threads::Mutex::Lock lm (capture_info_lock);

	if (capture_info.empty()) {
		return;
	}

	if (abort_capture) {

		if (destructive()) {
			goto outout;
		}

		for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {

			if ((*chan)->write_source) {

				(*chan)->write_source->mark_for_remove ();
				(*chan)->write_source->drop_references ();
				(*chan)->write_source.reset ();
			}

			/* new source set up in "out" below */
		}

		if (_midi_write_source) {
			_midi_write_source->mark_for_remove ();
			_midi_write_source->drop_references ();
			_midi_write_source.reset();
		}

		goto out;
	}

	for (total_capture = 0, ci = capture_info.begin(); ci != capture_info.end(); ++ci) {
		total_capture += (*ci)->samples;
	}

	/* figure out the name for this take */

	for (n = 0, chan = c->begin(); chan != c->end(); ++chan, ++n) {

		boost::shared_ptr<AudioFileSource> as = (*chan)->write_source;

		if (as) {
			audio_srcs.push_back (as);
			as->update_header (capture_info.front()->start, when, twhen);
			as->set_captured_for (_name.val());
			as->mark_immutable ();

			if (Config->get_auto_analyse_audio()) {
				Analyser::queue_source_for_analysis (as, true);
			}

			DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("newly captured source %1 length %2\n", as->path(), as->length (0)));
		}

		if (_midi_write_source) {
			midi_srcs.push_back (_midi_write_source);
		}
	}


	/* MIDI */

	if (_midi_write_source) {

		if (_midi_write_source->length (capture_info.front()->start) == 0) {
			/* No data was recorded, so this capture will
			   effectively be aborted; do the same as we
			   do for an explicit abort.
			*/
			if (_midi_write_source) {
				_midi_write_source->mark_for_remove ();
				_midi_write_source->drop_references ();
				_midi_write_source.reset();
			}

			goto out;
		}

		/* phew, we have data */

		Source::Lock source_lock(_midi_write_source->mutex());

		/* figure out the name for this take */

		midi_srcs.push_back (_midi_write_source);

		_midi_write_source->set_timeline_position (capture_info.front()->start);
		_midi_write_source->set_captured_for (_name);

		/* set length in beats to entire capture length */

		BeatsSamplesConverter converter (_session.tempo_map(), capture_info.front()->start);
		const Temporal::Beats total_capture_beats = converter.from (total_capture);
		_midi_write_source->set_length_beats (total_capture_beats);

		/* flush to disk: this step differs from the audio path,
		   where all the data is already on disk.
		*/

		_midi_write_source->mark_midi_streaming_write_completed (source_lock, Evoral::Sequence<Temporal::Beats>::ResolveStuckNotes, total_capture_beats);
	}

	_last_capture_sources.insert (_last_capture_sources.end(), audio_srcs.begin(), audio_srcs.end());
	_last_capture_sources.insert (_last_capture_sources.end(), midi_srcs.begin(), midi_srcs.end());


	if (_route) {
		_route->use_captured_sources (audio_srcs, capture_info);
		_route->use_captured_sources (midi_srcs, capture_info);
	}

	mark_write_completed = true;

  out:
	reset_write_sources (mark_write_completed);

  outout:

	for (ci = capture_info.begin(); ci != capture_info.end(); ++ci) {
		delete *ci;
	}

	capture_info.clear ();
	capture_start_sample = 0;
}

void
DiskWriter::transport_looped (samplepos_t transport_sample)
{
	if (was_recording) {
		// all we need to do is finish this capture, with modified capture length
		boost::shared_ptr<ChannelList> c = channels.reader();

		finish_capture (c);

		// the next region will start recording via the normal mechanism
		// we'll set the start position to the current transport pos
		// no latency adjustment or capture offset needs to be made, as that already happened the first time
		capture_start_sample = transport_sample;
		first_recordable_sample = transport_sample; // mild lie
		last_recordable_sample = max_samplepos;
		was_recording = true;

		if (recordable() && destructive()) {
			for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {

				RingBufferNPT<CaptureTransition>::rw_vector transvec;
				(*chan)->capture_transition_buf->get_write_vector(&transvec);

				if (transvec.len[0] > 0) {
					transvec.buf[0]->type = CaptureStart;
					transvec.buf[0]->capture_val = capture_start_sample;
					(*chan)->capture_transition_buf->increment_write_ptr(1);
				}
				else {
					// bad!
					fatal << X_("programming error: capture_transition_buf is full on rec loop!  inconceivable!")
					      << endmsg;
				}
			}
		}

	}

	/* Here we only keep track of the number of captured loops so monotonic
	   event times can be delivered to the write source in process().  Trying
	   to be clever here is a world of trouble, it is better to simply record
	   the input in a straightforward non-destructive way.  In the future when
	   we want to implement more clever MIDI looping modes it should be done in
	   the Source and/or entirely after the capture is finished.
	*/
	if (was_recording) {
		g_atomic_int_add(const_cast<gint*> (&_num_captured_loops), 1);
	}
}

void
DiskWriter::setup_destructive_playlist ()
{
	SourceList srcs;
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
		srcs.push_back ((*chan)->write_source);
	}

	/* a single full-sized region */

	assert (!srcs.empty ());

	PropertyList plist;
	plist.add (Properties::name, _name.val());
	plist.add (Properties::start, 0);
	plist.add (Properties::length, max_samplepos - srcs.front()->natural_position());

	boost::shared_ptr<Region> region (RegionFactory::create (srcs, plist));
	_playlists[DataType::AUDIO]->add_region (region, srcs.front()->natural_position());

	/* apply region properties and update write sources */
	use_destructive_playlist();
}

void
DiskWriter::use_destructive_playlist ()
{
	/* this is called from the XML-based constructor or ::set_destructive. when called,
	   we already have a playlist and a region, but we need to
	   set up our sources for write. we use the sources associated
	   with the (presumed single, full-extent) region.
	*/

	boost::shared_ptr<Region> rp;
	{
		const RegionList& rl (_playlists[DataType::AUDIO]->region_list_property().rlist());
		if (rl.size() > 0) {
			/* this can happen when dragging a region onto a tape track */
			assert((rl.size() == 1));
			rp = rl.front();
		}
	}

	if (!rp) {
		reset_write_sources (false, true);
		return;
	}

	boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion> (rp);

	if (region == 0) {
		throw failed_constructor();
	}

	/* be sure to stretch the region out to the maximum length (non-musical)*/

	region->set_length (max_samplepos - region->position(), 0);

	uint32_t n;
	ChannelList::iterator chan;
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (n = 0, chan = c->begin(); chan != c->end(); ++chan, ++n) {
		(*chan)->write_source = boost::dynamic_pointer_cast<AudioFileSource>(region->source (n));
		assert((*chan)->write_source);
		(*chan)->write_source->set_allow_remove_if_empty (false);

		// should be set when creating the source or loading the state
		assert ((*chan)->write_source->destructive());
	}

	/* the source list will never be reset for a destructive track */
}

void
DiskWriter::adjust_buffering ()
{
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
		(*chan)->resize (_session.butler()->audio_diskstream_capture_buffer_size());
	}
}

void
DiskWriter::realtime_handle_transport_stopped ()
{
}

bool
DiskWriter::set_name (string const & str)
{
	string my_name = X_("recorder:");
	my_name += str;

	if (_name != my_name) {
		SessionObject::set_name (my_name);
	}

	return true;
}

std::string
DiskWriter::steal_write_source_name ()
{
	if (_playlists[DataType::MIDI]) {
		string our_old_name = _midi_write_source->name();

		/* this will bump the name of the current write source to the next one
		 * (e.g. "MIDI 1-1" gets renamed to "MIDI 1-2"), thus leaving the
		 * current write source name (e.g. "MIDI 1-1" available). See the
		 * comments in Session::create_midi_source_by_stealing_name() about why
		 * we do this.
		 */

		try {
			string new_path = _session.new_midi_source_path (name());

			if (_midi_write_source->rename (new_path)) {
				return string();
			}
		} catch (...) {
			return string ();
		}

		return our_old_name;
	}

	return std::string();
}

bool
DiskWriter::configure_io (ChanCount in, ChanCount out)
{
	if (!DiskIOProcessor::configure_io (in, out)) {
		return false;
	}

	reset_write_sources (false, true);

	return true;
}
