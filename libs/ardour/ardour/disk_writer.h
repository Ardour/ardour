/*
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_disk_writer_h__
#define __ardour_disk_writer_h__

#include <list>
#include <vector>
#include <boost/optional.hpp>

#include "pbd/g_atomic_compat.h"

#include "ardour/disk_io.h"
#include "ardour/midi_buffer.h"

namespace ARDOUR
{
class AudioFileSource;
class SMFSource;
class MidiSource;

class LIBARDOUR_API DiskWriter : public DiskIOProcessor
{
public:
	DiskWriter (Session&, Track&, std::string const& name,
	            DiskIOProcessor::Flag f = DiskIOProcessor::Flag (0));
	~DiskWriter ();

	bool set_name (std::string const& str);
	std::string display_name () const;

	bool recordable () const { return _flags & Recordable; }

	static samplecnt_t chunk_samples () { return _chunk_samples; }
	static samplecnt_t default_chunk_samples ();
	static void        set_chunk_samples (samplecnt_t n) { _chunk_samples = n; }

	void run (BufferSet& /*bufs*/, samplepos_t /*start_sample*/, samplepos_t /*end_sample*/,
	          double speed, pframes_t /*nframes*/, bool /*result_required*/);

	void non_realtime_locate (samplepos_t);
	void realtime_handle_transport_stopped ();

	int set_state (const XMLNode&, int version);

	bool set_write_source_name (const std::string& str);

	std::string write_source_name () const;

	boost::shared_ptr<AudioFileSource> audio_write_source (uint32_t n = 0) {
		boost::shared_ptr<ChannelList> c = channels.reader ();
		if (n < c->size ()) {
			return (*c)[n]->write_source;
		}
		return boost::shared_ptr<AudioFileSource> ();
	}

	boost::shared_ptr<SMFSource> midi_write_source () const { return _midi_write_source; }

	std::string steal_write_source_name ();
	int use_new_write_source (DataType, uint32_t n = 0);
	void reset_write_sources (bool, bool force = false);

	AlignStyle alignment_style () const { return _alignment_style; }
	void       set_align_style (AlignStyle, bool force = false);

	PBD::Signal0<void> AlignmentStyleChanged;

	bool configure_io (ChanCount in, ChanCount out);

	std::list<boost::shared_ptr<Source> >& last_capture_sources () { return _last_capture_sources; }

	bool record_enabled () const { return g_atomic_int_get (&_record_enabled); }
	bool record_safe () const { return g_atomic_int_get (&_record_safe); }

	void set_record_enabled (bool yn);
	void set_record_safe (bool yn);
	void mark_capture_xrun ();

	/** @return Start position of currently-running capture (in session samples) */
	samplepos_t current_capture_start () const;
	samplepos_t current_capture_end () const;

	samplepos_t get_capture_start_sample (uint32_t n = 0) const;
	samplecnt_t get_captured_samples (uint32_t n = 0) const;

	float buffer_load () const;

	int seek (samplepos_t sample, bool complete_refill);

	static PBD::Signal0<void> Overrun;

	void set_note_mode (NoteMode m);

	/** Emitted when some MIDI data has been received for recording.
	 *  Parameter is the source that it is destined for.
	 *  A caller can get a copy of the data with get_gui_feed_buffer ()
	 */
	PBD::Signal1<void, boost::weak_ptr<MidiSource> > DataRecorded;

	PBD::Signal0<void> RecordEnableChanged;
	PBD::Signal0<void> RecordSafeChanged;

	void transport_looped (samplepos_t transport_sample);
	void transport_stopped_wallclock (struct tm&, time_t, bool abort);

	void adjust_buffering ();

	boost::shared_ptr<MidiBuffer> get_gui_feed_buffer () const;

protected:
	friend class Track;

	struct WriterChannelInfo : public DiskIOProcessor::ChannelInfo {
		WriterChannelInfo (samplecnt_t buffer_size)
		        : DiskIOProcessor::ChannelInfo (buffer_size)
		{
			resize (buffer_size);
		}
		void resize (samplecnt_t);
	};

	virtual XMLNode& state ();

	int use_playlist (DataType, boost::shared_ptr<Playlist>);

	int do_flush (RunContext context, bool force = false);

	void configuration_changed ();

private:
	static samplecnt_t _chunk_samples;

	int add_channel_to (boost::shared_ptr<ChannelList>, uint32_t how_many);

	void engage_record_enable ();
	void disengage_record_enable ();
	void engage_record_safe ();
	void disengage_record_safe ();

	bool prep_record_enable ();
	bool prep_record_disable ();

	void calculate_record_range (Temporal::OverlapType ot, samplepos_t transport_sample,
	                             samplecnt_t nframes, samplecnt_t& rec_nframes,
	                             samplecnt_t& rec_offset);

	void check_record_status (samplepos_t transport_sample, double speed, bool can_record);
	void finish_capture (boost::shared_ptr<ChannelList> c);
	void reset_capture ();

	void loop (samplepos_t);

	CaptureInfos                 capture_info;
	mutable Glib::Threads::Mutex capture_info_lock;

	boost::optional<samplepos_t> _capture_start_sample;

	samplecnt_t   _capture_captured;
	bool          _was_recording;
	bool          _xrun_flag;
	XrunPositions _xruns;
	samplepos_t   _first_recordable_sample;
	samplepos_t   _last_recordable_sample;
	int           _last_possibly_recording;
	AlignStyle    _alignment_style;
	std::string   _write_source_name;
	NoteMode      _note_mode;
	samplepos_t   _accumulated_capture_offset;

	bool          _transport_looped;
	samplepos_t   _transport_loop_sample;

	GATOMIC_QUAL gint _record_enabled;
	GATOMIC_QUAL gint _record_safe;
	GATOMIC_QUAL gint _samples_pending_write;
	GATOMIC_QUAL gint _num_captured_loops;

	boost::shared_ptr<SMFSource> _midi_write_source;

	std::list<boost::shared_ptr<Source> >            _last_capture_sources;
	std::vector<boost::shared_ptr<AudioFileSource> > capturing_sources;

	/** A buffer that we use to put newly-arrived MIDI data in for
	 * the GUI to read (so that it can update itself).
	 */
	MidiBuffer                   _gui_feed_buffer;
	mutable Glib::Threads::Mutex _gui_feed_buffer_mutex;
};

} // namespace

#endif /* __ardour_disk_writer_h__ */
