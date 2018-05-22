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

#ifndef __ardour_disk_reader_h__
#define __ardour_disk_reader_h__

#include "pbd/i18n.h"

#include "ardour/disk_io.h"
#include "ardour/midi_buffer.h"

namespace ARDOUR
{

class Playlist;
class AudioPlaylist;
class MidiPlaylist;
template<typename T> class MidiRingBuffer;

class LIBARDOUR_API DiskReader : public DiskIOProcessor
{
public:
	DiskReader (Session&, std::string const & name, DiskIOProcessor::Flag f = DiskIOProcessor::Flag (0));
	~DiskReader ();

	bool set_name (std::string const & str);
	std::string display_name() const { return std::string (_("player")); }

	static samplecnt_t chunk_samples() { return _chunk_samples; }
	static samplecnt_t default_chunk_samples ();
	static void set_chunk_samples (samplecnt_t n) { _chunk_samples = n; }

	void run (BufferSet& /*bufs*/, samplepos_t /*start_sample*/, samplepos_t /*end_sample*/, double speed, pframes_t /*nframes*/, bool /*result_required*/);
	void realtime_handle_transport_stopped ();
	void realtime_locate ();
	int overwrite_existing_buffers ();
	void set_pending_overwrite (bool yn);

	int set_state (const XMLNode&, int version);

	PBD::Signal0<void>            AlignmentStyleChanged;

	float buffer_load() const;

	void move_processor_automation (boost::weak_ptr<Processor>, std::list<Evoral::RangeMove<samplepos_t> > const &);

	/* called by the Butler in a non-realtime context */

	int do_refill () {
		return refill (_mixdown_buffer, _gain_buffer, 0);
	}

	/** For non-butler contexts (allocates temporary working buffers)
	 *
	 * This accessible method has a default argument; derived classes
	 * must inherit the virtual method that we call which does NOT
	 * have a default argument, to avoid complications with inheritance
	 */
	int do_refill_with_alloc (bool partial_fill = true) {
		return _do_refill_with_alloc (partial_fill);
	}

	bool pending_overwrite () const { return _pending_overwrite; }

	// Working buffers for do_refill (butler thread)
	static void allocate_working_buffers();
	static void free_working_buffers();

	void adjust_buffering ();

	int can_internal_playback_seek (samplecnt_t distance);
	int internal_playback_seek (samplecnt_t distance);
	int seek (samplepos_t sample, bool complete_refill = false);

	static PBD::Signal0<void> Underrun;

	void playlist_modified ();
	void reset_tracker ();

	static void set_midi_readahead_samples (samplecnt_t samples_ahead) { midi_readahead = samples_ahead; }

	static void set_no_disk_output (bool yn);
	static bool no_disk_output() { return _no_disk_output; }

protected:
	friend class Track;
	friend class MidiTrack;

	struct ReaderChannelInfo : public DiskIOProcessor::ChannelInfo {
		ReaderChannelInfo (samplecnt_t buffer_size)
			: DiskIOProcessor::ChannelInfo::ChannelInfo (buffer_size)
		{
			resize (buffer_size);
		}
		void resize (samplecnt_t);
	};

	XMLNode& state ();

	void resolve_tracker (Evoral::EventSink<samplepos_t>& buffer, samplepos_t time);

	void playlist_changed (const PBD::PropertyChange&);
	int use_playlist (DataType, boost::shared_ptr<Playlist>);
	void playlist_ranges_moved (std::list< Evoral::RangeMove<samplepos_t> > const &, bool);

	int add_channel_to (boost::shared_ptr<ChannelList>, uint32_t how_many);

private:
	/** The number of samples by which this diskstream's output should be delayed
	    with respect to the transport sample.  This is used for latency compensation.
	*/
	samplepos_t   overwrite_sample;
	off_t         overwrite_offset;
	bool          _pending_overwrite;
	bool          overwrite_queued;
	IOChange      input_change_pending;
	samplepos_t   file_sample[DataType::num_types];

	int _do_refill_with_alloc (bool partial_fill);

	static samplecnt_t _chunk_samples;
	static samplecnt_t midi_readahead;
	static bool       _no_disk_output;

	int audio_read (Sample* buf, Sample* mixdown_buffer, float* gain_buffer,
	                samplepos_t& start, samplecnt_t cnt,
	                int channel, bool reversed);
	int midi_read (samplepos_t& start, samplecnt_t cnt, bool reversed);

	static Sample* _mixdown_buffer;
	static gain_t* _gain_buffer;

	int refill (Sample* mixdown_buffer, float* gain_buffer, samplecnt_t fill_level);
	int refill_audio (Sample *mixdown_buffer, float *gain_buffer, samplecnt_t fill_level);
	int refill_midi ();

	sampleoffset_t calculate_playback_distance (pframes_t);

	void get_midi_playback (MidiBuffer& dst, samplepos_t start_sample, samplepos_t end_sample, MonitorState, BufferSet&, double speed, samplecnt_t distance);
};

} // namespace

#endif /* __ardour_disk_reader_h__ */
