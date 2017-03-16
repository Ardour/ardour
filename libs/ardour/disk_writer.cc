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

#include "ardour/audioengine.h"
#include "ardour/audio_buffer.h"
#include "ardour/audiofilesource.h"
#include "ardour/debug.h"
#include "ardour/disk_writer.h"
#include "ardour/port.h"
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
	bool can_record = _session.actively_recording ();

	check_record_status (start_frame, can_record);

	if (nframes == 0) {
		return;
	}

	Glib::Threads::Mutex::Lock sm (state_lock, Glib::Threads::TRY_LOCK);

	if (!sm.locked()) {
		return;
	}

	// Safeguard against situations where process() goes haywire when autopunching
	// and last_recordable_frame < first_recordable_frame

	if (last_recordable_frame < first_recordable_frame) {
		last_recordable_frame = max_framepos;
	}

	if (record_enabled()) {

		Evoral::OverlapType ot = Evoral::coverage (first_recordable_frame, last_recordable_frame, start_frame, end_frame);
		// XXX should this be transport_frame + nframes - 1 ? coverage() expects its parameter ranges to include their end points
		// XXX also, first_recordable_frame & last_recordable_frame may both be == max_framepos: coverage() will return OverlapNone in that case. Is thak OK?
		calculate_record_range (ot, start_frame, nframes, rec_nframes, rec_offset);

		DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1: this time record %2 of %3 frames, offset %4\n", _name, rec_nframes, nframes, rec_offset));

		if (rec_nframes && !was_recording) {
			capture_captured = 0;
			was_recording = true;
		}
	}

	if (can_record && !_last_capture_sources.empty()) {
		_last_capture_sources.clear ();
	}

	if (rec_nframes) {

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
		}

	} else {

		if (was_recording) {
			finish_capture (c);
		}

	}

	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
		if (rec_nframes) {
			(*chan)->buf->increment_write_ptr (rec_nframes);
		}
	}

	if (rec_nframes != 0) {
		capture_captured += rec_nframes;
		DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1 now captured %2 (by %3)\n", name(), capture_captured, rec_nframes));
	}

	if (!c->empty()) {
		if (_slaved) {
			if (c->front()->buf->write_space() >= c->front()->buf->bufsize() / 2) {
				_need_butler = true;
			}
		} else {
			if (((framecnt_t) c->front()->buf->read_space() >= _chunk_frames)) {
				_need_butler = true;
			}
		}
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
