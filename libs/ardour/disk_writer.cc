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

#include "pbd/i18n.h"

#include "ardour/analyser.h"
#include "ardour/audioengine.h"
#include "ardour/audiofilesource.h"
#include "ardour/audio_buffer.h"
#include "ardour/audioplaylist.h"
#include "ardour/audioregion.h"
#include "ardour/debug.h"
#include "ardour/disk_writer.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_source.h"
#include "ardour/midi_track.h"
#include "ardour/port.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/smf_source.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

ARDOUR::framecnt_t DiskWriter::_chunk_frames = DiskWriter::default_chunk_frames ();
PBD::Signal0<void> DiskWriter::Overrun;

DiskWriter::DiskWriter (Session& s, string const & str, DiskIOProcessor::Flag f)
	: DiskIOProcessor (s, str, f)
        , capture_start_frame (0)
        , capture_captured (0)
        , was_recording (false)
        , adjust_capture_position (0)
        , _capture_offset (0)
        , first_recordable_frame (max_framepos)
        , last_recordable_frame (max_framepos)
        , last_possibly_recording (0)
        , _alignment_style (ExistingMaterial)
        , _alignment_choice (Automatic)
	, _num_captured_loops (0)
	, _accumulated_capture_offset (0)
	, _gui_feed_buffer(AudioEngine::instance()->raw_buffer_size (DataType::MIDI))
{
	DiskIOProcessor::init ();
}

framecnt_t
DiskWriter::default_chunk_frames ()
{
	return 65536;
}

bool
DiskWriter::set_write_source_name (string const & str)
{
	_write_source_name = str;
	return true;
}

void
DiskWriter::check_record_status (framepos_t transport_frame, bool can_record)
{
	int possibly_recording;
	int rolling;
	int change;
	const int transport_rolling = 0x4;
	const int track_rec_enabled = 0x2;
	const int global_rec_enabled = 0x1;
	const int fully_rec_enabled = (transport_rolling|track_rec_enabled|global_rec_enabled);

	/* merge together the 3 factors that affect record status, and compute
	 * what has changed.
	 */

	rolling = _session.transport_speed() != 0.0f;
	possibly_recording = (rolling << 2) | ((int)record_enabled() << 1) | (int)can_record;
	change = possibly_recording ^ last_possibly_recording;

	if (possibly_recording == last_possibly_recording) {
		return;
	}

	const framecnt_t existing_material_offset = _session.worst_playback_latency();

	if (possibly_recording == fully_rec_enabled) {

		if (last_possibly_recording == fully_rec_enabled) {
			return;
		}

		capture_start_frame = _session.transport_frame();
		first_recordable_frame = capture_start_frame + _capture_offset;
		last_recordable_frame = max_framepos;

                DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1: @ %7 (%9) FRF = %2 CSF = %4 CO = %5, EMO = %6 RD = %8 WOL %10 WTL %11\n",
                                                                      name(), first_recordable_frame, last_recordable_frame, capture_start_frame,
                                                                      _capture_offset,
                                                                      existing_material_offset,
                                                                      transport_frame,
                                                                      _session.transport_frame(),
                                                                      _session.worst_output_latency(),
                                                                      _session.worst_track_latency()));


                if (_alignment_style == ExistingMaterial) {
                        first_recordable_frame += existing_material_offset;
                        DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("\tshift FRF by EMO %1\n",
                                                                              first_recordable_frame));
                }

		prepare_record_status (capture_start_frame);

	} else {

		if (last_possibly_recording == fully_rec_enabled) {

			/* we were recording last time */

			if (change & transport_rolling) {

				/* transport-change (stopped rolling): last_recordable_frame was set in ::prepare_to_stop(). We
				 * had to set it there because we likely rolled past the stopping point to declick out,
				 * and then backed up.
				 */

			} else {
				/* punch out */

				last_recordable_frame = _session.transport_frame() + _capture_offset;

				if (_alignment_style == ExistingMaterial) {
					last_recordable_frame += existing_material_offset;
				}
			}
		}
	}

	last_possibly_recording = possibly_recording;
}

void
DiskWriter::calculate_record_range (Evoral::OverlapType ot, framepos_t transport_frame, framecnt_t nframes,
				    framecnt_t & rec_nframes, framecnt_t & rec_offset)
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
		rec_nframes = transport_frame + nframes - first_recordable_frame;
		if (rec_nframes) {
			rec_offset = first_recordable_frame - transport_frame;
		}
		break;

	case Evoral::OverlapEnd:
		/*    |--------|    recrange
		 *       |--------  transrange
		 */
		rec_nframes = last_recordable_frame - transport_frame;
		rec_offset = 0;
		break;

	case Evoral::OverlapExternal:
		/*    |--------|    recrange
		 *  --------------  transrange
		 */
		rec_nframes = last_recordable_frame - first_recordable_frame;
		rec_offset = first_recordable_frame - transport_frame;
		break;
	}

        DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1 rec? %2 @ %3 (for %4) FRF %5 LRF %6 : rf %7 @ %8\n",
                                                              _name, enum_2_string (ot), transport_frame, nframes,
                                                              first_recordable_frame, last_recordable_frame, rec_nframes, rec_offset));
}

void
DiskWriter::prepare_to_stop (framepos_t transport_frame, framepos_t audible_frame)
{
	switch (_alignment_style) {
	case ExistingMaterial:
		last_recordable_frame = transport_frame + _capture_offset;
		DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose("%1: prepare to stop sets last recordable frame to %2 + %3 = %4\n", _name, transport_frame, _capture_offset, last_recordable_frame));
		break;

	case CaptureTime:
		last_recordable_frame = audible_frame; // note that capture_offset is zero
		/* we may already have captured audio before the last_recordable_frame (audible frame),
		   so deal with this.
		*/
		if (last_recordable_frame > capture_start_frame) {
			capture_captured = min (capture_captured, last_recordable_frame - capture_start_frame);
		}
		DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose("%1: prepare to stop sets last recordable frame to audible frame @ %2\n", _name, audible_frame));
		break;
	}

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

/** Get the start position (in session frames) of the nth capture in the current pass */
ARDOUR::framepos_t
DiskWriter::get_capture_start_frame (uint32_t n) const
{
	Glib::Threads::Mutex::Lock lm (capture_info_lock);

	if (capture_info.size() > n) {
		/* this is a completed capture */
		return capture_info[n]->start;
	} else {
		/* this is the currently in-progress capture */
		return capture_start_frame;
	}
}

ARDOUR::framecnt_t
DiskWriter::get_captured_frames (uint32_t n) const
{
	Glib::Threads::Mutex::Lock lm (capture_info_lock);

	if (capture_info.size() > n) {
		/* this is a completed capture */
		return capture_info[n]->frames;
	} else {
		/* this is the currently in-progress capture */
		return capture_captured;
	}
}

void
DiskWriter::set_input_latency (framecnt_t l)
{
	_input_latency = l;
}

void
DiskWriter::set_capture_offset ()
{
	switch (_alignment_style) {
	case ExistingMaterial:
		_capture_offset = _input_latency;
		break;

	case CaptureTime:
	default:
		_capture_offset = 0;
		break;
	}

        DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1: using IO latency, capture offset set to %2 with style = %3\n", name(), _capture_offset, enum_2_string (_alignment_style)));
}


void
DiskWriter::set_align_style (AlignStyle a, bool force)
{
	if (record_enabled() && _session.actively_recording()) {
		return;
	}

	if ((a != _alignment_style) || force) {
		_alignment_style = a;
		set_capture_offset ();
		AlignmentStyleChanged ();
	}
}

void
DiskWriter::set_align_style_from_io ()
{
	bool have_physical = false;

	if (_alignment_choice != Automatic) {
		return;
	}

	if (!_route) {
		return;
	}

	boost::shared_ptr<IO> input = _route->input ();

	if (input) {
		uint32_t n = 0;
		vector<string> connections;
		boost::shared_ptr<ChannelList> c = channels.reader();

		for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan, ++n) {

			if ((input->nth (n).get()) && (input->nth (n)->get_connections (connections) == 0)) {
				if (AudioEngine::instance()->port_is_physical (connections[0])) {
					have_physical = true;
					break;
				}
			}

			connections.clear ();
		}
	}

#ifdef MIXBUS
	// compensate for latency when bouncing from master or mixbus.
	// we need to use "ExistingMaterial" to pick up the master bus' latency
	// see also Route::direct_feeds_according_to_reality
	IOVector ios;
	ios.push_back (_io);
	if (_session.master_out() && ios.fed_by (_session.master_out()->output())) {
		have_physical = true;
	}
	for (uint32_t n = 0; n < NUM_MIXBUSES && !have_physical; ++n) {
		if (_session.get_mixbus (n) && ios.fed_by (_session.get_mixbus(n)->output())) {
			have_physical = true;
		}
	}
#endif

	if (have_physical) {
		set_align_style (ExistingMaterial);
	} else {
		set_align_style (CaptureTime);
	}
}

void
DiskWriter::set_align_choice (AlignChoice a, bool force)
{
	if (record_enabled() && _session.actively_recording()) {
		return;
	}

	if ((a != _alignment_choice) || force) {
		_alignment_choice = a;

		switch (_alignment_choice) {
			case Automatic:
				set_align_style_from_io ();
				break;
			case UseExistingMaterial:
				set_align_style (ExistingMaterial);
				break;
			case UseCaptureTime:
				set_align_style (CaptureTime);
				break;
		}
	}
}

XMLNode&
DiskWriter::state (bool full)
{
	XMLNode& node (DiskIOProcessor::state (full));
	node.add_property (X_("capture-alignment"), enum_2_string (_alignment_choice));
	node.add_property (X_("record-safe"), (_record_safe ? X_("yes" : "no")));
	return node;
}

int
DiskWriter::set_state (const XMLNode& node, int version)
{
	XMLProperty const * prop;

	if (DiskIOProcessor::set_state (node, version)) {
		return -1;
	}

	if ((prop = node.property (X_("capture-alignment"))) != 0) {
                set_align_choice (AlignChoice (string_2_enum (prop->value(), _alignment_choice)), true);
        } else {
                set_align_choice (Automatic, true);
        }


	if ((prop = node.property ("record-safe")) != 0) {
	    _record_safe = PBD::string_is_affirmative (prop->value()) ? 1 : 0;
	}

	return 0;
}

void
DiskWriter::non_realtime_locate (framepos_t position)
{
	if (_midi_write_source) {
		_midi_write_source->set_timeline_position (position);
	}

	DiskIOProcessor::non_realtime_locate (position);
}


void
DiskWriter::prepare_record_status(framepos_t capture_start_frame)
{
	if (recordable() && destructive()) {
		boost::shared_ptr<ChannelList> c = channels.reader();
		for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {

			RingBufferNPT<CaptureTransition>::rw_vector transitions;
			(*chan)->capture_transition_buf->get_write_vector (&transitions);

			if (transitions.len[0] > 0) {
				transitions.buf[0]->type = CaptureStart;
				transitions.buf[0]->capture_val = capture_start_frame;
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
DiskWriter::run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame,
                 double speed, pframes_t nframes, bool result_required)

/*	(BufferSet& bufs, framepos_t transport_frame, pframes_t nframes, framecnt_t& playback_distance, bool need_disk_signal)
 */
{
	uint32_t n;
	boost::shared_ptr<ChannelList> c = channels.reader();
	ChannelList::iterator chan;
	framecnt_t rec_offset = 0;
	framecnt_t rec_nframes = 0;
	bool nominally_recording;
	bool re = record_enabled ();
	bool can_record = _session.actively_recording ();

	check_record_status (start_frame, can_record);

	if (nframes == 0) {
		return;
	}

	nominally_recording = (can_record && re);

	Glib::Threads::Mutex::Lock sm (state_lock, Glib::Threads::TRY_LOCK);

	if (!sm.locked()) {
		return;
	}

	// Safeguard against situations where process() goes haywire when autopunching
	// and last_recordable_frame < first_recordable_frame

	if (last_recordable_frame < first_recordable_frame) {
		last_recordable_frame = max_framepos;
	}

	const Location* const loop_loc    = loop_location;
	framepos_t            loop_start  = 0;
	framepos_t            loop_end    = 0;
	framepos_t            loop_length = 0;

	if (loop_loc) {
		get_location_times (loop_loc, &loop_start, &loop_end, &loop_length);
	}

	adjust_capture_position = 0;

	if (nominally_recording || (re && was_recording && _session.get_record_enabled() && (_session.config.get_punch_in() || _session.preroll_record_punch_enabled()))) {

		Evoral::OverlapType ot = Evoral::coverage (first_recordable_frame, last_recordable_frame, start_frame, end_frame);
		// XXX should this be transport_frame + nframes - 1 ? coverage() expects its parameter ranges to include their end points
		// XXX also, first_recordable_frame & last_recordable_frame may both be == max_framepos: coverage() will return OverlapNone in that case. Is thak OK?
		calculate_record_range (ot, start_frame, nframes, rec_nframes, rec_offset);

		DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1: this time record %2 of %3 frames, offset %4\n", _name, rec_nframes, nframes, rec_offset));

		if (rec_nframes && !was_recording) {
			capture_captured = 0;

			if (loop_loc) {
				/* Loop recording, so pretend the capture started at the loop
				   start rgardless of what time it is now, so the source starts
				   at the loop start and can handle time wrapping around.
				   Otherwise, start the source right now as usual.
				*/
				capture_captured    = start_frame - loop_start;
				capture_start_frame = loop_start;
			}

			_midi_write_source->mark_write_starting_now (capture_start_frame, capture_captured, loop_length);

			g_atomic_int_set(const_cast<gint*> (&_frames_pending_write), 0);
			g_atomic_int_set(const_cast<gint*> (&_num_captured_loops), 0);

			was_recording = true;

		}

		/* For audio: not writing frames to the capture ringbuffer offsets
		 * the recording. For midi: we need to keep track of the record range
		 * and subtract the accumulated difference from the event time.
		 */
		if (rec_nframes) {
			_accumulated_capture_offset += rec_offset;
		} else {
			_accumulated_capture_offset += nframes;
		}

	}

	if (can_record && !_last_capture_sources.empty()) {
		_last_capture_sources.clear ();
	}

	if (rec_nframes) {

		/* AUDIO */

		const size_t n_buffers = bufs.count().n_audio();

		for (n = 0; chan != c->end(); ++chan, ++n) {

			ChannelInfo* chaninfo (*chan);
			AudioBuffer& buf (bufs.get_audio (n%n_buffers));

			chaninfo->buf->get_write_vector (&chaninfo->rw_vector);

			if (rec_nframes <= (framecnt_t) chaninfo->rw_vector.len[0]) {

				Sample *incoming = buf.data (rec_offset);
				memcpy (chaninfo->rw_vector.buf[0], incoming, sizeof (Sample) * rec_nframes);

			} else {

				framecnt_t total = chaninfo->rw_vector.len[0] + chaninfo->rw_vector.len[1];

				if (rec_nframes > total) {
                                        DEBUG_TRACE (DEBUG::Butler, string_compose ("%1 overrun in %2, rec_nframes = %3 total space = %4\n",
                                                                                    DEBUG_THREAD_SELF, name(), rec_nframes, total));
                                        Overrun ();
					return;
				}

				Sample *incoming = buf.data (rec_offset);
				framecnt_t first = chaninfo->rw_vector.len[0];

				memcpy (chaninfo->rw_vector.buf[0], incoming, sizeof (Sample) * first);
				memcpy (chaninfo->rw_vector.buf[1], incoming + first, sizeof (Sample) * (rec_nframes - first));
			}

			chaninfo->buf->increment_write_ptr (rec_nframes);

		}

		/* MIDI */

		// Pump entire port buffer into the ring buffer (TODO: split cycles?)
		MidiBuffer& buf    = bufs.get_midi (0);
		boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack>(_route);
		MidiChannelFilter* filter = mt ? &mt->capture_filter() : 0;

		for (MidiBuffer::iterator i = buf.begin(); i != buf.end(); ++i) {
			Evoral::Event<MidiBuffer::TimeType> ev(*i, false);
			if (ev.time() + rec_offset > rec_nframes) {
				break;
			}
#ifndef NDEBUG
			if (DEBUG_ENABLED(DEBUG::MidiIO)) {
				const uint8_t* __data = ev.buffer();
				DEBUG_STR_DECL(a);
				DEBUG_STR_APPEND(a, string_compose ("mididiskstream %1 capture event @ %2 + %3 sz %4 ", this, ev.time(), start_frame, ev.size()));
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
			/* Write events to the capture buffer in frames from session start,
			   but ignoring looping so event time progresses monotonically.
			   The source knows the loop length so it knows exactly where the
			   event occurs in the series of recorded loops and can implement
			   any desirable behaviour.  We don't want to send event with
			   transport time here since that way the source can not
			   reconstruct their actual time; future clever MIDI looping should
			   probably be implemented in the source instead of here.
			*/
			const framecnt_t loop_offset = _num_captured_loops * loop_length;
			const framepos_t event_time = start_frame + loop_offset - _accumulated_capture_offset + ev.time();
			if (event_time < 0 || event_time < first_recordable_frame) {
				/* Event out of range, skip */
				continue;
			}

			if (!filter || !filter->filter(ev.buffer(), ev.size())) {
				_midi_buf->write (event_time, ev.event_type(), ev.size(), ev.buffer());
			}
		}
		g_atomic_int_add(const_cast<gint*>(&_frames_pending_write), nframes);

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
					_gui_feed_buffer.push_back ((*i).time() + start_frame, (*i).size(), (*i).buffer());
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

	if (!c->empty()) {
		if (((framecnt_t) c->front()->buf->read_space() >= _chunk_frames)) {
			_need_butler = true;
		}
	}

	/* MIDI BUTLER REQUIRED CODE */

	if (_midi_buf->read_space() < _midi_buf->bufsize() / 2) {
		_need_butler = true;
	}
}

void
DiskWriter::finish_capture (boost::shared_ptr<ChannelList> c)
{
	was_recording = false;
	first_recordable_frame = max_framepos;
	last_recordable_frame = max_framepos;

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

	ci->start =  capture_start_frame;
	ci->frames = capture_captured;

	/* XXX theoretical race condition here. Need atomic exchange ?
	   However, the circumstances when this is called right
	   now (either on record-disable or transport_stopped)
	   mean that no actual race exists. I think ...
	   We now have a capture_info_lock, but it is only to be used
	   to synchronize in the transport_stop and the capture info
	   accessors, so that invalidation will not occur (both non-realtime).
	*/

	DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("Finish capture, add new CI, %1 + %2\n", ci->start, ci->frames));

	capture_info.push_back (ci);
	capture_captured = 0;

	/* now we've finished a capture, reset first_recordable_frame for next time */
	first_recordable_frame = max_framepos;
}

void
DiskWriter::set_record_enabled (bool yn)
{
	if (!recordable() || !_session.record_enabling_legal() || record_safe ()) {
		return;
	}

	/* can't rec-enable in destructive mode if transport is before start */

	if (destructive() && yn && _session.transport_frame() < _session.current_start_frame()) {
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

	if (destructive() && yn && _session.transport_frame() < _session.current_start_frame()) {
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

	if (destructive() && _session.transport_frame() < _session.current_start_frame()) {
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

	return (float) ((double) c->front()->buf->write_space()/
			(double) c->front()->buf->bufsize());
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
DiskWriter::seek (framepos_t frame, bool complete_refill)
{
	uint32_t n;
	ChannelList::iterator chan;
	boost::shared_ptr<ChannelList> c = channels.reader();

	Glib::Threads::Mutex::Lock lm (state_lock);

	for (n = 0, chan = c->begin(); chan != c->end(); ++chan, ++n) {
		(*chan)->buf->reset ();
	}

	_midi_buf->reset ();
	g_atomic_int_set(&_frames_read_from_ringbuffer, 0);
	g_atomic_int_set(&_frames_written_to_ringbuffer, 0);

	/* can't rec-enable in destructive mode if transport is before start */

	if (destructive() && record_enabled() && frame < _session.current_start_frame()) {
		disengage_record_enable ();
	}

	playback_sample = frame;
	file_frame = frame;

	return 0;
}

int
DiskWriter::do_flush (RunContext ctxt, bool force_flush)
{
	uint32_t to_write;
	int32_t ret = 0;
	RingBufferNPT<Sample>::rw_vector vector;
	RingBufferNPT<CaptureTransition>::rw_vector transvec;
	framecnt_t total;

	transvec.buf[0] = 0;
	transvec.buf[1] = 0;
	vector.buf[0] = 0;
	vector.buf[1] = 0;

	boost::shared_ptr<ChannelList> c = channels.reader();
	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {

		(*chan)->buf->get_read_vector (&vector);

		total = vector.len[0] + vector.len[1];

		if (total == 0 || (total < _chunk_frames && !force_flush && was_recording)) {
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

		if (total >= 2 * _chunk_frames || ((force_flush || !was_recording) && total > _chunk_frames)) {
			ret = 1;
		}

		to_write = min (_chunk_frames, (framecnt_t) vector.len[0]);

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

					// capture end, the capture_val represents total frames in capture

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

		(*chan)->buf->increment_read_ptr (to_write);
		(*chan)->curr_capture_cnt += to_write;

		if ((to_write == vector.len[0]) && (total > to_write) && (to_write < _chunk_frames) && !destructive()) {

			/* we wrote all of vector.len[0] but it wasn't an entire
			   disk_write_chunk_frames of data, so arrange for some part
			   of vector.len[1] to be flushed to disk as well.
			*/

			to_write = min ((framecnt_t)(_chunk_frames - to_write), (framecnt_t) vector.len[1]);

                        DEBUG_TRACE (DEBUG::Butler, string_compose ("%1 additional write of %2\n", name(), to_write));

			if ((*chan)->write_source->write (vector.buf[1], to_write) != to_write) {
				error << string_compose(_("AudioDiskstream %1: cannot write to disk"), id()) << endmsg;
				return -1;
			}

			(*chan)->buf->increment_read_ptr (to_write);
			(*chan)->curr_capture_cnt += to_write;
		}
	}

	/* MIDI*/

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

		use_new_write_source (DataType::MIDI);

		if (destructive() && !c->empty ()) {

			/* we now have all our write sources set up, so create the
			   playlist's single region.
			*/

			if (_playlists[DataType::MIDI]->empty()) {
				setup_destructive_playlist ();
			}
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
	uint32_t buffer_position;
	bool more_work = true;
	int err = 0;
	boost::shared_ptr<AudioRegion> region;
	framecnt_t total_capture;
	SourceList srcs;
	SourceList::iterator src;
	ChannelList::iterator chan;
	vector<CaptureInfo*>::iterator ci;
	boost::shared_ptr<ChannelList> c = channels.reader();
	uint32_t n = 0;
	bool mark_write_completed = false;

	finish_capture (c);

	boost::shared_ptr<AudioPlaylist> pl = boost::dynamic_pointer_cast<AudioPlaylist> (_playlists[DataType::AUDIO]);

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

		goto out;
	}

	for (total_capture = 0, ci = capture_info.begin(); ci != capture_info.end(); ++ci) {
		total_capture += (*ci)->frames;
	}

	/* figure out the name for this take */

	for (n = 0, chan = c->begin(); chan != c->end(); ++chan, ++n) {

		boost::shared_ptr<AudioFileSource> s = (*chan)->write_source;

		if (s) {
			srcs.push_back (s);
			s->update_header (capture_info.front()->start, when, twhen);
			s->set_captured_for (_name.val());
			s->mark_immutable ();

			if (Config->get_auto_analyse_audio()) {
				Analyser::queue_source_for_analysis (s, true);
			}

			DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("newly captured source %1 length %2\n", s->path(), s->length (0)));
		}
	}

	if (!pl) {
		goto midi;
	}

	/* destructive tracks have a single, never changing region */

	if (destructive()) {

		/* send a signal that any UI can pick up to do the right thing. there is
		   a small problem here in that a UI may need the peak data to be ready
		   for the data that was recorded and this isn't interlocked with that
		   process. this problem is deferred to the UI.
		 */

		pl->LayeringChanged(); // XXX this may not get the UI to do the right thing

	} else {

		string whole_file_region_name;
		whole_file_region_name = region_name_from_path (c->front()->write_source->name(), true);

		/* Register a new region with the Session that
		   describes the entire source. Do this first
		   so that any sub-regions will obviously be
		   children of this one (later!)
		*/

		try {
			PropertyList plist;

			plist.add (Properties::start, c->front()->write_source->last_capture_start_frame());
			plist.add (Properties::length, total_capture);
			plist.add (Properties::name, whole_file_region_name);
			boost::shared_ptr<Region> rx (RegionFactory::create (srcs, plist));
			rx->set_automatic (true);
			rx->set_whole_file (true);

			region = boost::dynamic_pointer_cast<AudioRegion> (rx);
			region->special_set_position (capture_info.front()->start);
		}


		catch (failed_constructor& err) {
			error << string_compose(_("%1: could not create region for complete audio file"), _name) << endmsg;
			/* XXX what now? */
		}

		_last_capture_sources.insert (_last_capture_sources.end(), srcs.begin(), srcs.end());

		pl->clear_changes ();
		pl->set_capture_insertion_in_progress (true);
		pl->freeze ();

		const framepos_t preroll_off = _session.preroll_record_trim_len ();
		for (buffer_position = c->front()->write_source->last_capture_start_frame(), ci = capture_info.begin(); ci != capture_info.end(); ++ci) {

			string region_name;

			RegionFactory::region_name (region_name, whole_file_region_name, false);

			DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1 capture bufpos %5 start @ %2 length %3 add new region %4\n",
			                                                      _name, (*ci)->start, (*ci)->frames, region_name, buffer_position));

			try {

				PropertyList plist;

				plist.add (Properties::start, buffer_position);
				plist.add (Properties::length, (*ci)->frames);
				plist.add (Properties::name, region_name);

				boost::shared_ptr<Region> rx (RegionFactory::create (srcs, plist));
				region = boost::dynamic_pointer_cast<AudioRegion> (rx);
				if (preroll_off > 0) {
					region->trim_front (buffer_position + preroll_off);
				}
			}

			catch (failed_constructor& err) {
				error << _("AudioDiskstream: could not create region for captured audio!") << endmsg;
				continue; /* XXX is this OK? */
			}

			i_am_the_modifier++;

			pl->add_region (region, (*ci)->start + preroll_off, 1, non_layered());
			pl->set_layer (region, DBL_MAX);
			i_am_the_modifier--;

			buffer_position += (*ci)->frames;
		}

		pl->thaw ();
		pl->set_capture_insertion_in_progress (false);
		_session.add_command (new StatefulDiffCommand (pl));
	}

	mark_write_completed = true;

  out:
	reset_write_sources (mark_write_completed);

  outout:

	for (ci = capture_info.begin(); ci != capture_info.end(); ++ci) {
		delete *ci;
	}

	capture_info.clear ();
	capture_start_frame = 0;

  midi:
	return;
}

#if 0 // MIDI PART
void
DiskWriter::transport_stopped_wallclock (struct tm& /*when*/, time_t /*twhen*/, bool abort_capture)
{
	bool more_work = true;
	int err = 0;
	boost::shared_ptr<MidiRegion> region;
	MidiRegion::SourceList srcs;
	MidiRegion::SourceList::iterator src;
	vector<CaptureInfo*>::iterator ci;

	finish_capture ();

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
			error << string_compose(_("MidiDiskstream \"%1\": cannot flush captured data to disk!"), _name) << endmsg;
			err++;
		}
	}

	/* XXX is there anything we can do if err != 0 ? */
	Glib::Threads::Mutex::Lock lm (capture_info_lock);

	if (capture_info.empty()) {
		goto no_capture_stuff_to_do;
	}

	if (abort_capture) {

		if (_write_source) {
			_write_source->mark_for_remove ();
			_write_source->drop_references ();
			_write_source.reset();
		}

		/* new source set up in "out" below */

	} else {

		framecnt_t total_capture = 0;
		for (ci = capture_info.begin(); ci != capture_info.end(); ++ci) {
			total_capture += (*ci)->frames;
		}

		if (_write_source->length (capture_info.front()->start) != 0) {

			/* phew, we have data */

			Source::Lock source_lock(_write_source->mutex());

			/* figure out the name for this take */

			srcs.push_back (_write_source);

			_write_source->set_timeline_position (capture_info.front()->start);
			_write_source->set_captured_for (_name);

			/* set length in beats to entire capture length */

			BeatsFramesConverter converter (_session.tempo_map(), capture_info.front()->start);
			const Evoral::Beats total_capture_beats = converter.from (total_capture);
			_write_source->set_length_beats (total_capture_beats);

			/* flush to disk: this step differs from the audio path,
			   where all the data is already on disk.
			*/

			_write_source->mark_midi_streaming_write_completed (source_lock, Evoral::Sequence<Evoral::Beats>::ResolveStuckNotes, total_capture_beats);

			/* we will want to be able to keep (over)writing the source
			   but we don't want it to be removable. this also differs
			   from the audio situation, where the source at this point
			   must be considered immutable. luckily, we can rely on
			   MidiSource::mark_streaming_write_completed() to have
			   already done the necessary work for that.
			*/

			string whole_file_region_name;
			whole_file_region_name = region_name_from_path (_write_source->name(), true);

			/* Register a new region with the Session that
			   describes the entire source. Do this first
			   so that any sub-regions will obviously be
			   children of this one (later!)
			*/

			try {
				PropertyList plist;

				plist.add (Properties::name, whole_file_region_name);
				plist.add (Properties::whole_file, true);
				plist.add (Properties::automatic, true);
				plist.add (Properties::start, 0);
				plist.add (Properties::length, total_capture);
				plist.add (Properties::layer, 0);

				boost::shared_ptr<Region> rx (RegionFactory::create (srcs, plist));

				region = boost::dynamic_pointer_cast<MidiRegion> (rx);
				region->special_set_position (capture_info.front()->start);
			}


			catch (failed_constructor& err) {
				error << string_compose(_("%1: could not create region for complete midi file"), _name) << endmsg;
				/* XXX what now? */
			}

			_last_capture_sources.insert (_last_capture_sources.end(), srcs.begin(), srcs.end());

			_playlist->clear_changes ();
			_playlist->freeze ();

			/* Session frame time of the initial capture in this pass, which is where the source starts */
			framepos_t initial_capture = 0;
			if (!capture_info.empty()) {
				initial_capture = capture_info.front()->start;
			}

			const framepos_t preroll_off = _session.preroll_record_trim_len ();
			for (ci = capture_info.begin(); ci != capture_info.end(); ++ci) {

				string region_name;

				RegionFactory::region_name (region_name, _write_source->name(), false);

				DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1 capture start @ %2 length %3 add new region %4\n",
							_name, (*ci)->start, (*ci)->frames, region_name));


				// cerr << _name << ": based on ci of " << (*ci)->start << " for " << (*ci)->frames << " add a region\n";

				try {
					PropertyList plist;

					/* start of this region is the offset between the start of its capture and the start of the whole pass */
					plist.add (Properties::start, (*ci)->start - initial_capture);
					plist.add (Properties::length, (*ci)->frames);
					plist.add (Properties::length_beats, converter.from((*ci)->frames).to_double());
					plist.add (Properties::name, region_name);

					boost::shared_ptr<Region> rx (RegionFactory::create (srcs, plist));
					region = boost::dynamic_pointer_cast<MidiRegion> (rx);
					if (preroll_off > 0) {
						region->trim_front ((*ci)->start - initial_capture + preroll_off);
					}
				}

				catch (failed_constructor& err) {
					error << _("MidiDiskstream: could not create region for captured midi!") << endmsg;
					continue; /* XXX is this OK? */
				}

				// cerr << "add new region, buffer position = " << buffer_position << " @ " << (*ci)->start << endl;

				i_am_the_modifier++;
				_playlist->add_region (region, (*ci)->start + preroll_off);
				i_am_the_modifier--;
			}

			_playlist->thaw ();
			_session.add_command (new StatefulDiffCommand(_playlist));

		} else {

			/* No data was recorded, so this capture will
			   effectively be aborted; do the same as we
			   do for an explicit abort.
			*/

			if (_write_source) {
				_write_source->mark_for_remove ();
				_write_source->drop_references ();
				_write_source.reset();
			}
		}

	}

	use_new_write_source (0);

	for (ci = capture_info.begin(); ci != capture_info.end(); ++ci) {
		delete *ci;
	}

	capture_info.clear ();
	capture_start_frame = 0;

  no_capture_stuff_to_do:

	reset_tracker ();
}
#endif

void
DiskWriter::transport_looped (framepos_t transport_frame)
{
	if (was_recording) {
		// all we need to do is finish this capture, with modified capture length
		boost::shared_ptr<ChannelList> c = channels.reader();

		finish_capture (c);

		// the next region will start recording via the normal mechanism
		// we'll set the start position to the current transport pos
		// no latency adjustment or capture offset needs to be made, as that already happened the first time
		capture_start_frame = transport_frame;
		first_recordable_frame = transport_frame; // mild lie
		last_recordable_frame = max_framepos;
		was_recording = true;

		if (recordable() && destructive()) {
			for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {

				RingBufferNPT<CaptureTransition>::rw_vector transvec;
				(*chan)->capture_transition_buf->get_write_vector(&transvec);

				if (transvec.len[0] > 0) {
					transvec.buf[0]->type = CaptureStart;
					transvec.buf[0]->capture_val = capture_start_frame;
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
	plist.add (Properties::length, max_framepos - srcs.front()->natural_position());

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

	region->set_length (max_framepos - region->position(), 0);

	uint32_t n;
	ChannelList::iterator chan;
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (n = 0, chan = c->begin(); chan != c->end(); ++chan, ++n) {
		(*chan)->write_source = boost::dynamic_pointer_cast<AudioFileSource>(region->source (n));
		assert((*chan)->write_source);
		(*chan)->write_source->set_allow_remove_if_empty (false);

		/* this might be false if we switched modes, so force it */

#ifdef XXX_OLD_DESTRUCTIVE_API_XXX
		(*chan)->write_source->set_destructive (true);
#else
		// should be set when creating the source or loading the state
		assert ((*chan)->write_source->destructive());
#endif
	}

	/* the source list will never be reset for a destructive track */
}
