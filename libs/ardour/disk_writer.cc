/*
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2018 Ben Loftis <ben@harrisonconsoles.com>
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

#include <glibmm/datetime.h>

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

DiskWriter::DiskWriter (Session& s, Track& t, string const & str, DiskIOProcessor::Flag f)
        : DiskIOProcessor (s, t, X_("recorder:") + str, f, Config->get_default_automation_time_domain())
	, _capture_captured (0)
	, _was_recording (false)
	, _xrun_flag (false)
	, _first_recordable_sample (max_samplepos)
	, _last_recordable_sample (max_samplepos)
	, _last_possibly_recording (0)
	, _alignment_style (ExistingMaterial)
	, _note_mode (Sustained)
	, _accumulated_capture_offset (0)
	, _transport_looped (false)
	, _transport_loop_sample (0)
	, _gui_feed_buffer(AudioEngine::instance()->raw_buffer_size (DataType::MIDI))
{
	DiskIOProcessor::init ();
	_xruns.reserve (128);

	g_atomic_int_set (&_record_enabled, 0);
	g_atomic_int_set (&_record_safe, 0);
	g_atomic_int_set (&_samples_pending_write, 0);
	g_atomic_int_set (&_num_captured_loops, 0);
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

std::string
DiskWriter::display_name () const
{
	return std::string (_("Recorder"));
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
		c->push_back (new WriterChannelInfo (_session.butler()->audio_capture_buffer_size()));
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

	reset_write_sources (false);

	return true;
}

std::string
DiskWriter::write_source_name () const
{
	if (!_write_source_name.empty ()) {
		return _write_source_name;
	}

	std::string const& n (name ());
	if (n.find (X_("recorder:")) == 0 && n.size () > 9) {
		return n.substr (9);
	}
	return n;
}

void
DiskWriter::check_record_status (samplepos_t transport_sample, double speed, bool can_record)
{
	static const int transport_rolling = 0x4;
	static const int track_rec_enabled = 0x2;
	static const int global_rec_enabled = 0x1;

	static const int rec_ready = (track_rec_enabled | global_rec_enabled);
	static const int fully_rec_enabled = (transport_rolling |track_rec_enabled | global_rec_enabled);

	/* merge together the 3 factors that affect record status, and compute what has changed. */

	int possibly_recording = (speed != 0.0f ? 4 : 0)  | (record_enabled() ? 2 : 0) | (can_record ? 1 : 0);

	if (possibly_recording == _last_possibly_recording) {
		return;
	}

	if (possibly_recording == fully_rec_enabled) {

		if (_last_possibly_recording == fully_rec_enabled) {
			return;
		}

		Location* loc;
		if  (_session.config.get_punch_in () && 0 != (loc = _session.locations()->auto_punch_location ())) {
			_capture_start_sample = loc->start_sample ();
		} else if (_loop_location) {
			_capture_start_sample = _loop_location->start_sample ();
			if (_last_possibly_recording & transport_rolling) {
				_accumulated_capture_offset = _playback_offset + transport_sample - _session.transport_sample (); // + rec_offset;
			}

		} else {
			_capture_start_sample = _session.transport_sample ();
		}

		_first_recordable_sample = _capture_start_sample.value ();

		if (_alignment_style == ExistingMaterial) {
			_first_recordable_sample += _capture_offset + _playback_offset;
		}

		if  (_session.config.get_punch_out () && 0 != (loc = _session.locations()->auto_punch_location ())) {
			/* this freezes the punch-out point when starting to record.
			 *
			 * We should allow to move it or at least allow to disable punch-out
			 * while rolling..
			 */
			_last_recordable_sample = loc->end_sample ();
			if (_alignment_style == ExistingMaterial) {
				_last_recordable_sample += _capture_offset + _playback_offset;
			}
		} else {
			_last_recordable_sample = max_samplepos;
		}

		DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1: @ %2 (STS: %3) CS:%4 FRS: %5 IL: %7, OL: %8 CO: %9 PO: %10 WOL: %11 WIL: %12\n",
		                                                      name(),
		                                                      transport_sample,
		                                                      _session.transport_sample(),
																													_capture_start_sample.value (),
																													_first_recordable_sample,
																													_last_recordable_sample,
		                                                      _input_latency,
		                                                      _output_latency,
		                                                      _capture_offset,
		                                                      _playback_offset,
		                                                      _session.worst_output_latency(),
		                                                      _session.worst_input_latency()));


	} else if  (!_capture_start_sample) {
		/* set _capture_start_sample early on to calculate MIDI _accumulated_capture_offset */
		Location* loc;
		if  (_session.config.get_punch_in () && 0 != (loc = _session.locations()->auto_punch_location ())) {
			_capture_start_sample = loc->start_sample ();
		} else if (_loop_location) {
			_capture_start_sample = _loop_location->start_sample ();
		} else if ((possibly_recording & rec_ready) == rec_ready) {
			/* count-in, pre-roll */
			_capture_start_sample = _session.transport_sample ();
		} else if (possibly_recording) {
			/* already rolling, manual punch rec-arm/rec-en */
			_accumulated_capture_offset = _playback_offset;
		}
	}

	_last_possibly_recording = possibly_recording;
}

void
DiskWriter::calculate_record_range (Temporal::OverlapType ot, samplepos_t transport_sample, samplecnt_t nframes, samplecnt_t & rec_nframes, samplecnt_t & rec_offset)
{
	switch (ot) {
	case Temporal::OverlapNone:
		rec_nframes = 0;
		break;

	case Temporal::OverlapInternal:
		/*     ----------    recrange
		 *       |---|       transrange
		 */
		rec_nframes = nframes;
		rec_offset = 0;
		break;

	case Temporal::OverlapStart:
		/*    |--------|    recrange
		 *  -----|          transrange
		 */
		rec_nframes = transport_sample + nframes - _first_recordable_sample;
		if (rec_nframes) {
			rec_offset = _first_recordable_sample - transport_sample;
		}
		break;

	case Temporal::OverlapEnd:
		/*    |--------|    recrange
		 *       |--------  transrange
		 */
		rec_nframes = _last_recordable_sample - transport_sample;
		rec_offset = 0;
		break;

	case Temporal::OverlapExternal:
		/*    |--------|    recrange
		 *  --------------  transrange
		 */
		rec_nframes = _last_recordable_sample - _first_recordable_sample;
		rec_offset = _first_recordable_sample - transport_sample;
		break;
	}

	DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1 rec? %2 @ %3 (for %4) FRF %5 LRF %6 : rf %7 @ %8\n",
	                                                      _name, enum_2_string (ot), transport_sample, nframes,
	                                                      _first_recordable_sample, _last_recordable_sample, rec_nframes, rec_offset));
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
	} else if (_capture_start_sample) {
		/* this is the currently in-progress capture */
		return _capture_start_sample.value ();
	} else {
		/* pre-roll, count-in etc */
		return _session.transport_sample(); /* mild lie */
	}
}

samplepos_t
DiskWriter::current_capture_start () const
{
	if (!_capture_start_sample) {
		return _session.transport_sample(); /* mild lie */
	}
	return _capture_start_sample.value ();
}

samplepos_t
DiskWriter::current_capture_end () const
{
	return current_capture_start () + _capture_captured;
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
		return _capture_captured;
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
	node.set_property (X_("record-safe"), record_safe ());
	return node;
}

int
DiskWriter::set_state (const XMLNode& node, int version)
{
	if (DiskIOProcessor::set_state (node, version)) {
		return -1;
	}

	int rec_safe = 0;
	node.get_property (X_("record-safe"), rec_safe);
	g_atomic_int_set (&_record_safe, rec_safe);

	reset_write_sources (false, true);

	return 0;
}

void
DiskWriter::non_realtime_locate (samplepos_t position)
{
	if (_midi_write_source) {
		timepos_t pos;

		if (time_domain() == Temporal::AudioTime) {
			pos = timepos_t (position);
		} else {
			const timepos_t b (position);
			pos = timepos_t (b.beats());
		}

		_midi_write_source->set_natural_position (pos);
	}

	DiskIOProcessor::non_realtime_locate (position);
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
	if (!check_active()) {
		_xrun_flag = false;
		return;
	}

	uint32_t n;
	boost::shared_ptr<ChannelList> c = channels.reader();
	ChannelList::iterator chan;

	samplecnt_t rec_offset = 0;
	samplecnt_t rec_nframes = 0;
	bool nominally_recording;

	bool re = record_enabled ();
	bool punch_in = _session.config.get_punch_in () && _session.locations()->auto_punch_location ();
	bool can_record = _session.actively_recording ();
	can_record |= speed != 0 && _session.get_record_enabled () && punch_in && _session.transport_sample () <= _session.locations()->auto_punch_location ()->start_sample ();

	_need_butler = false;

	const Location* const loop_loc = _loop_location;
	timepos_t loop_start;
	timepos_t loop_end;
	timecnt_t loop_length;

	if (_transport_looped && _capture_captured == 0) {
		_transport_looped = false;
	}

	if (loop_loc) {
		get_location_times (loop_loc, &loop_start, &loop_end, &loop_length);

		if (_was_recording && _transport_looped && _capture_captured >= loop_length.samples()) {
			samplecnt_t remain = _capture_captured - loop_length.samples();
			_capture_captured = loop_length.samples();
			loop (_transport_loop_sample);
			_capture_captured = remain;
		}

	} else {
		_transport_looped = false;
	}

#ifndef NDEBUG
	if (speed != 0 && re) {
		DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1: run() start: %2 end: %3 NF: %4\n", _name, start_sample, end_sample, nframes));
	}
#endif

	check_record_status (start_sample, speed, can_record);

	if (nframes == 0) {
		_xrun_flag = false;
		return;
	}

	nominally_recording = (can_record && re);

	// Safeguard against situations where process() goes haywire when autopunching
	// and last_recordable_sample < first_recordable_sample

	if (_last_recordable_sample < _first_recordable_sample) {
		_last_recordable_sample = max_samplepos;
	}

	if (nominally_recording || (re && _was_recording && _session.get_record_enabled() && punch_in)) {

		Temporal::OverlapType ot = Temporal::coverage_exclusive_ends (_first_recordable_sample, _last_recordable_sample, start_sample, end_sample);
		// XXX should this be transport_sample + nframes - 1 ? coverage() expects its parameter ranges to include their end points
		// XXX also, first_recordable_sample & last_recordable_sample may both be == max_samplepos: coverage() will return OverlapNone in that case. Is thak OK?
		calculate_record_range (ot, start_sample, nframes, rec_nframes, rec_offset);

		DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1: this time record %2 of %3 samples, offset %4\n", _name, rec_nframes, nframes, rec_offset));

		if (rec_nframes && !_was_recording) {
			_capture_captured = 0;
			_xrun_flag = false;

			if (loop_loc) {
				/* Loop recording, so pretend the capture started at the loop
				   start rgardless of what time it is now, so the source starts
				   at the loop start and can handle time wrapping around.
				   Otherwise, start the source right now as usual.
				*/

				_capture_captured     = start_sample - loop_start.samples() + rec_offset;
				_capture_start_sample = loop_start.samples();
				_first_recordable_sample = loop_start.samples();

				if (_alignment_style == ExistingMaterial) {
					_capture_captured  -= _playback_offset + _capture_offset;
				}

				if (_capture_captured > 0) {
					/* when enabling record while already looping,
					 * zero fill region back to loop-start.
					 */
					for (chan = c->begin(), n = 0; chan != c->end(); ++chan, ++n) {
						ChannelInfo* chaninfo (*chan);
						for (samplecnt_t s = 0; s < _capture_captured; ++s) {
							chaninfo->wbuf->write_one (0); // TODO: optimize
						}
					}
				}
			}

			if (_midi_write_source) {
				assert (_capture_start_sample);

				timepos_t start (_capture_start_sample.get());

				if (time_domain() != Temporal::AudioTime) {
					start = timepos_t (start.beats());
				}

				_midi_write_source->mark_write_starting_now (start, _capture_captured);
			}

			g_atomic_int_set (&_samples_pending_write, 0);
			g_atomic_int_set (&_num_captured_loops, 0);

			_was_recording = true;

		}

		/* For audio: not writing samples to the capture ringbuffer offsets
		 * the recording. For midi: we need to keep track of the record range
		 * and subtract the accumulated difference from the event time.
		 */
		if (rec_nframes) {
			_accumulated_capture_offset += rec_offset;
		} else if (_capture_start_sample && start_sample >= _capture_start_sample.value ()) {
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
					_xruns.push_back (_capture_captured);
					_xrun_flag = false;
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

		if (_midi_buf) {

			// Pump entire port buffer into the ring buffer (TODO: split cycles?)
			MidiBuffer& buf    = bufs.get_midi (0);
			MidiTrack* mt = dynamic_cast<MidiTrack*>(&_track);
			MidiChannelFilter* filter = mt ? &mt->capture_filter() : 0;

			assert (buf.size() == 0 || _midi_buf);

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
				const samplecnt_t loop_offset = g_atomic_int_get (&_num_captured_loops) * loop_length.samples();
				const samplepos_t event_time = start_sample + loop_offset - _accumulated_capture_offset + ev.time();
				if (event_time < 0 || event_time < _first_recordable_sample) {
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

			g_atomic_int_add (&_samples_pending_write, nframes);

			if (buf.size() != 0) {
				Glib::Threads::Mutex::Lock lm (_gui_feed_buffer_mutex, Glib::Threads::TRY_LOCK);

				if (lm.locked ()) {
					/* Copy this data into our GUI feed buffer and tell the GUI
					   that it can read it if it likes.
					*/
					_gui_feed_buffer.clear ();

					for (MidiBuffer::iterator i = buf.begin(); i != buf.end(); ++i) {
						/* This may fail if buf is larger than _gui_feed_buffer, but it's not really
						 * the end of the world if it does.
						 */
						samplepos_t mpos = (*i).time() + start_sample - _accumulated_capture_offset;
						if (mpos >= _first_recordable_sample) {
							_gui_feed_buffer.push_back (mpos, Evoral::MIDI_EVENT, (*i).size(), (*i).buffer());
						}
					}
				}

				DataRecorded (_midi_write_source); /* EMIT SIGNAL */
			}
		}

		if (_xrun_flag) {
			/* There still are `Port::resampler_quality () -1` samples in the resampler
			 * buffer from before the xrun. */
			_xruns.push_back (_capture_captured + Port::resampler_quality () - 1);
		}

		_capture_captured += rec_nframes;
		DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1 now captured %2 (by %3)\n", name(), _capture_captured, rec_nframes));

	} else {

		/* not recording this time, but perhaps we were before .. */

		if (_was_recording) {
			finish_capture (c);
			_accumulated_capture_offset = 0;
			_capture_start_sample.reset ();
			_last_possibly_recording = 0; // re-init
		}
	}

	/* clear xrun flag */
	_xrun_flag = false;

	/* AUDIO BUTLER REQUIRED CODE */

	if (_playlists[DataType::AUDIO] && !c->empty()) {
		if (((samplecnt_t) c->front()->wbuf->read_space() >= _chunk_samples)) {
			_need_butler = true;
		}
	}

	/* MIDI BUTLER REQUIRED CODE */

	if (_playlists[DataType::MIDI] && _midi_buf && (_midi_buf->read_space() >= _midi_buf->bufsize() / 2)) {
		_need_butler = true;
	}

	// DEBUG_TRACE (DEBUG::Butler, string_compose ("%1 writer run, needs butler = %2\n", name(), _need_butler));
}

void
DiskWriter::finish_capture (boost::shared_ptr<ChannelList> c)
{
	_was_recording = false;
	_xrun_flag = false;
	_first_recordable_sample = max_samplepos;
	_last_recordable_sample = max_samplepos;

	if (_capture_captured == 0) {
		return;
	}

	CaptureInfo* ci = new CaptureInfo ();

	assert (_capture_start_sample);
	ci->start   =  _capture_start_sample.value ();
	ci->samples = _capture_captured;
	ci->xruns   = _xruns;
	_xruns.clear ();

	if (_loop_location) {
		timepos_t loop_start;
		timepos_t loop_end;
		timecnt_t loop_length;
		get_location_times (_loop_location, &loop_start, &loop_end, &loop_length);
	        ci->loop_offset = g_atomic_int_get (&_num_captured_loops) * loop_length.samples();
	} else {
		ci->loop_offset = 0;
	}

	DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("Finish capture, add new CI, %1 + %2 Loop-off %3\n", ci->start, ci->samples, ci->loop_offset));

	/* XXX theoretical race condition here. Need atomic exchange ?
	   However, the circumstances when this is called right
	   now (either on record-disable or transport_stopped)
	   mean that no actual race exists. I think ...
	   We now have a capture_info_lock, but it is only to be used
	   to synchronize in the transport_stop and the capture info
	   accessors, so that invalidation will not occur (both non-realtime).
	*/

	capture_info.push_back (ci);
	_capture_captured = 0;

	/* now we've finished a capture, reset first_recordable_sample for next time */
	_first_recordable_sample = max_samplepos;
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
DiskWriter::mark_capture_xrun ()
{
	_xrun_flag = true;
}

void
DiskWriter::set_record_enabled (bool yn)
{
	if (!recordable() || !_session.record_enabling_legal() || record_safe ()) {
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
	if (!recordable() || !_session.record_enabling_legal() || (channels.reader()->empty() && !_midi_buf)  || record_safe ()) { // REQUIRES REVIEW "|| record_safe ()"
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

void
DiskWriter::configuration_changed ()
{
	seek (_session.transport_sample(), false);
}

int
DiskWriter::seek (samplepos_t /*sample*/, bool /*complete_refill*/)
{
	reset_capture ();
	return 0;
}

void
DiskWriter::reset_capture ()
{
	uint32_t n;
	ChannelList::iterator chan;
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (n = 0, chan = c->begin(); chan != c->end(); ++chan, ++n) {
		(*chan)->wbuf->reset ();
	}

	if (_midi_buf) {
		_midi_buf->reset ();
	}

	_accumulated_capture_offset = 0;
	_capture_start_sample.reset ();
}

int
DiskWriter::do_flush (RunContext ctxt, bool force_flush)
{
	uint32_t to_write;
	int32_t ret = 0;
	RingBufferNPT<Sample>::rw_vector vector;
	samplecnt_t total;

	vector.buf[0] = 0;
	vector.buf[1] = 0;

	boost::shared_ptr<ChannelList> c = channels.reader();
	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {

		(*chan)->wbuf->get_read_vector (&vector);

		total = vector.len[0] + vector.len[1];

		if (total == 0 || (total < _chunk_samples && !force_flush && _was_recording)) {
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

		if (total >= 2 * _chunk_samples || ((force_flush || !_was_recording) && total > _chunk_samples)) {
			ret = 1;
		}

		to_write = min (_chunk_samples, (samplecnt_t) vector.len[0]);

		if ((!(*chan)->write_source) || (*chan)->write_source->write (vector.buf[0], to_write) != to_write) {
			error << string_compose(_("AudioDiskstream %1: cannot write to disk"), id()) << endmsg;
			return -1;
		}

		(*chan)->wbuf->increment_read_ptr (to_write);
		(*chan)->curr_capture_cnt += to_write;

		if ((to_write == vector.len[0]) && (total > to_write) && (to_write < _chunk_samples)) {

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

	if (_midi_write_source && _midi_buf) {

		const samplecnt_t total = g_atomic_int_get(&_samples_pending_write);

		if (total == 0 ||
		    _midi_buf->read_space() == 0 ||
		    (!force_flush && (total < _chunk_samples) && _was_recording)) {
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

		if (total >= 2 * _chunk_samples || ((force_flush || !_was_recording) && total > _chunk_samples)) {
			ret = 1;
		}

		if (force_flush) {
			/* push out everything we have, right now */
			to_write = UINT32_MAX;
		} else {
			to_write = _chunk_samples;
		}

		if ((total > _chunk_samples) || force_flush) {
			Source::Lock lm(_midi_write_source->mutex());
			if (_midi_write_source->midi_write (lm, *_midi_buf, timepos_t (get_capture_start_sample (0)), timecnt_t (to_write)) != to_write) {
				error << string_compose(_("MidiDiskstream %1: cannot write to disk"), id()) << endmsg;
				return -1;
			}
			g_atomic_int_add(&_samples_pending_write, -to_write);
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
}

int
DiskWriter::use_new_write_source (DataType dt, uint32_t n)
{
	_accumulated_capture_offset = 0;

	if (dt == DataType::MIDI) {
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
				     c->size(), write_source_name(), n)) == 0) {
				throw failed_constructor();
			}
		}

		catch (failed_constructor &err) {
			error << string_compose (_("%1:%2 new capture file not initialized correctly"), _name, n) << endmsg;
			chan->write_source.reset ();
			return -1;
		}

		chan->write_source->set_allow_remove_if_empty (true);
	}

	return 0;
}

void
DiskWriter::transport_stopped_wallclock (struct tm& when, time_t twhen, bool abort_capture)
{
	bool more_work = true;
	int err = 0;
	SourceList audio_srcs;
	SourceList midi_srcs;
	ChannelList::iterator chan;
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
		_xruns.clear ();

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

	/* figure out the name for this take */

	for (n = 0, chan = c->begin(); chan != c->end(); ++chan, ++n) {

		boost::shared_ptr<AudioFileSource> as = (*chan)->write_source;

		if (as) {
			audio_srcs.push_back (as);
			as->update_header (capture_info.front()->start, when, twhen);
			as->set_captured_for (_track.name());
			as->mark_immutable ();

			Glib::DateTime tm (Glib::DateTime::create_now_local (mktime (&when)));
			as->set_take_id (tm.format ("%F %H.%M.%S"));

			if (Config->get_auto_analyse_audio()) {
				Analyser::queue_source_for_analysis (as, true);
			}

			DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("newly captured source %1 length %2\n", as->path(), as->length ()));
		}

		if (_midi_write_source) {
			midi_srcs.push_back (_midi_write_source);
			_midi_write_source->set_captured_for (_track.name());
		}

		(*chan)->write_source->stamp (twhen);
		(*chan)->write_source->set_captured_xruns (capture_info.front()->xruns);
	}


	/* MIDI */

	if (_midi_write_source) {

		if (_midi_write_source->empty()) {
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

		_midi_write_source->set_natural_position (timepos_t (capture_info.front()->start));
		_midi_write_source->set_captured_for (_track.name());

		Glib::DateTime tm (Glib::DateTime::create_now_local (mktime (&when)));
		_midi_write_source->set_take_id (tm.format ("%F %H.%M.%S"));

		/* flush to disk: this step differs from the audio path,
		   where all the data is already on disk.
		*/

		timecnt_t total_capture (0, timepos_t (capture_info.front()->start));
		for (vector<CaptureInfo*>::iterator ci = capture_info.begin(); ci != capture_info.end(); ++ci) {
			total_capture += timecnt_t ((*ci)->samples);
		}

		_midi_write_source->mark_midi_streaming_write_completed (source_lock, Evoral::Sequence<Temporal::Beats>::ResolveStuckNotes, total_capture.beats());
	}

	_last_capture_sources.insert (_last_capture_sources.end(), audio_srcs.begin(), audio_srcs.end());
	_last_capture_sources.insert (_last_capture_sources.end(), midi_srcs.begin(), midi_srcs.end());


	_track.use_captured_sources (audio_srcs, capture_info);
	_track.use_captured_sources (midi_srcs, capture_info);

	mark_write_completed = true;

  out:
	reset_write_sources (mark_write_completed);

	for (vector<CaptureInfo*>::iterator ci = capture_info.begin(); ci != capture_info.end(); ++ci) {
		delete *ci;
	}

	capture_info.clear ();
	reset_capture ();
}

void
DiskWriter::transport_looped (samplepos_t transport_sample)
{
	if (_capture_captured) {
		_transport_looped = true;
		_transport_loop_sample = transport_sample;
	}
}

void
DiskWriter::loop (samplepos_t transport_sample)
{
	_transport_looped = false;
	if (_was_recording) {
		// all we need to do is finish this capture, with modified capture length
		boost::shared_ptr<ChannelList> c = channels.reader();

		finish_capture (c);

		// the next region will start recording via the normal mechanism
		// we'll set the start position to the current transport pos
		// no latency adjustment or capture offset needs to be made, as that already happened the first time
		_capture_start_sample = transport_sample;
		_first_recordable_sample = transport_sample; // mild lie
		_last_recordable_sample = max_samplepos;
		_was_recording = true;
	}

	/* Here we only keep track of the number of captured loops so monotonic
	   event times can be delivered to the write source in process().  Trying
	   to be clever here is a world of trouble, it is better to simply record
	   the input in a straightforward non-destructive way.  In the future when
	   we want to implement more clever MIDI looping modes it should be done in
	   the Source and/or entirely after the capture is finished.
	*/
	if (_was_recording) {
		g_atomic_int_add (&_num_captured_loops, 1);
	}
}

void
DiskWriter::adjust_buffering ()
{
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
		(*chan)->resize (_session.butler()->audio_capture_buffer_size());
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
			string new_path = _session.new_midi_source_path (write_source_name ());

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
	bool changed = false;
	{
		boost::shared_ptr<ChannelList> c = channels.reader();
		if (in.n_audio() != c->size()) {
			changed = true;
		}
		if ((0 == in.n_midi ()) != (0 == _midi_buf)) {
			changed = true;
		}
	}


	if (!DiskIOProcessor::configure_io (in, out)) {
		return false;
	}

	if (record_enabled() || changed) {
		reset_write_sources (false, true);
	}

	return true;
}

int
DiskWriter::use_playlist (DataType dt, boost::shared_ptr<Playlist> playlist)
{
	bool reset_ws = _playlists[dt] != playlist;

	if (DiskIOProcessor::use_playlist (dt, playlist)) {
		return -1;
	}
	if (reset_ws) {
		reset_write_sources (false, true);
	}
	return 0;
}
