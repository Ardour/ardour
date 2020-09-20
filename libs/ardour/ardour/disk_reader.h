/*
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_disk_reader_h_
#define _ardour_disk_reader_h_

#include <boost/optional.hpp>

#include "pbd/g_atomic_compat.h"

#include "evoral/Curve.h"

#include "ardour/disk_io.h"
#include "ardour/midi_buffer.h"
#include "ardour/midi_state_tracker.h"

namespace ARDOUR
{
class Playlist;
class AudioPlaylist;
class MidiPlaylist;

template <typename T> class MidiRingBuffer;

class LIBARDOUR_API DiskReader : public DiskIOProcessor
{
public:
	DiskReader (Session&, Track&, std::string const& name, DiskIOProcessor::Flag f = DiskIOProcessor::Flag (0));
	~DiskReader ();

	bool set_name (std::string const& str);

	std::string display_name () const;

	static samplecnt_t chunk_samples ()
	{
		return _chunk_samples;
	}

	static void set_chunk_samples (samplecnt_t n)
	{
		_chunk_samples = n;
	}

	static samplecnt_t default_chunk_samples ();

	void run (BufferSet& /*bufs*/, samplepos_t /*start_sample*/, samplepos_t /*end_sample*/, double speed, pframes_t /*nframes*/, bool /*result_required*/);
	void realtime_handle_transport_stopped ();
	void realtime_locate (bool);
	bool overwrite_existing_buffers ();
	void set_pending_overwrite (OverwriteReason);
	void set_loop (Location*);

	int set_state (const XMLNode&, int version);

	PBD::Signal0<void> AlignmentStyleChanged;

	float buffer_load () const;

	void move_processor_automation (boost::weak_ptr<Processor>, std::list<Temporal::RangeMove> const&);

	/* called by the Butler in a non-realtime context as part of its normal
	 * buffer refill loop (not due to transport-mechanism requests like
	 * locate)
	 */
	int do_refill ();

	/** For contexts outside the normal butler refill loop (allocates temporary working buffers) */
	int do_refill_with_alloc (bool partial_fill, bool reverse);

	bool pending_overwrite () const;

	/* Working buffers for do_refill (butler thread) */
	static void allocate_working_buffers ();
	static void free_working_buffers ();

	void adjust_buffering ();

	bool can_internal_playback_seek (sampleoffset_t distance);
	void internal_playback_seek (sampleoffset_t distance);
	int  seek (samplepos_t sample, bool complete_refill = false);

	static PBD::Signal0<void> Underrun;

	void playlist_modified ();
	void reset_tracker ();

	bool declick_in_progress () const;

	/* inc/dec variants MUST be called as part of the process call tree, before any
	 * disk readers are invoked. We use it when the session needs the
	 * transport (and thus effective read position for DiskReaders) to keep
	 * advancing as part of syncing up with a transport master, but we
	 * don't want any actual disk output yet because we are still not
	 * synced.
	 */
	static void inc_no_disk_output ();
	static void dec_no_disk_output ();
	static bool no_disk_output ()
	{
		return g_atomic_int_get (&_no_disk_output);
	}
	static void reset_loop_declick (Location*, samplecnt_t sample_rate);
	static void alloc_loop_declick (samplecnt_t sample_rate);

protected:
	friend class Track;
	friend class MidiTrack;

	struct ReaderChannelInfo : public DiskIOProcessor::ChannelInfo {
		ReaderChannelInfo (samplecnt_t buffer_size, samplecnt_t preloop_size)
			: DiskIOProcessor::ChannelInfo (buffer_size)
			, pre_loop_buffer (0)
			, pre_loop_buffer_size (0)
			, initialized (false)
		{
			resize (buffer_size);
		}

		~ReaderChannelInfo ()
		{
			delete[] pre_loop_buffer;
		}

		void resize (samplecnt_t);
		void resize_preloop (samplecnt_t);

		Sample*     pre_loop_buffer;
		samplecnt_t pre_loop_buffer_size;
		bool        initialized;
	};

	XMLNode& state ();

	void resolve_tracker (Evoral::EventSink<samplepos_t>& buffer, samplepos_t time);

	int  use_playlist (DataType, boost::shared_ptr<Playlist>);
	void playlist_ranges_moved (std::list<Temporal::RangeMove> const&, bool);

	int add_channel_to (boost::shared_ptr<ChannelList>, uint32_t how_many);

	class DeclickAmp
	{
	public:
		DeclickAmp (samplecnt_t sample_rate);

		void apply_gain (AudioBuffer& buf, samplecnt_t n_samples, const float target, sampleoffset_t buffer_offset = 0);

		float gain () const
		{
			return _g;
		}
		void set_gain (float g)
		{
			_g = g;
		}

	private:
		float _a;
		float _l;
		float _g;
	};

	class Declicker
	{
	public:
		Declicker ();
		~Declicker ();

		void alloc (samplecnt_t sr, bool fadein, bool linear);

		void run (Sample* buf, samplepos_t start, samplepos_t end);
		void reset (samplepos_t start, samplepos_t end, bool fadein, samplecnt_t sr);

		samplepos_t fade_start;
		samplepos_t fade_end;
		samplecnt_t fade_length;
		Sample*     vec;
	};

private:
	samplepos_t    overwrite_sample;
	sampleoffset_t overwrite_offset;
	samplepos_t    new_file_sample;
	bool           run_must_resolve;
	IOChange       input_change_pending;
	samplepos_t    file_sample[DataType::num_types];

	mutable GATOMIC_QUAL gint _pending_overwrite;

	DeclickAmp            _declick_amp;
	sampleoffset_t        _declick_offs;
	bool                  _declick_enabled;
	MidiStateTracker      _tracker;
	boost::optional<bool> _last_read_reversed;
	boost::optional<bool> _last_read_loop;

	static samplecnt_t _chunk_samples;

	static GATOMIC_QUAL gint _no_disk_output;

	static Declicker   loop_declick_in;
	static Declicker   loop_declick_out;
	static samplecnt_t loop_fade_length;

	samplecnt_t audio_read (Sample*      sum_buffer,
	                        Sample*      mixdown_buffer,
	                        float*       gain_buffer,
	                        samplepos_t& start, samplecnt_t cnt,
	                        ReaderChannelInfo* rci,
	                        int                channel,
	                        bool               reversed);

	static Sample* _sum_buffer;
	static Sample* _mixdown_buffer;
	static gain_t* _gain_buffer;

	int refill (Sample* sum_buffer, Sample* mixdown_buffer, float* gain_buffer, samplecnt_t fill_level, bool reversed);
	int refill_audio (Sample* sum_buffer, Sample* mixdown_buffer, float* gain_buffer, samplecnt_t fill_level, bool reversed);

	sampleoffset_t calculate_playback_distance (pframes_t);

	RTMidiBuffer* rt_midibuffer ();

	void get_midi_playback (MidiBuffer& dst, samplepos_t start_sample, samplepos_t end_sample, MonitorState, BufferSet&, double speed, samplecnt_t distance);
	void maybe_xfade_loop (Sample*, samplepos_t read_start, samplepos_t read_end, ReaderChannelInfo*);

	void configuration_changed ();

	bool overwrite_existing_audio ();
	bool overwrite_existing_midi ();

	samplepos_t last_refill_loop_start;
	void setup_preloop_buffer ();
};

} // namespace ARDOUR

#endif /* _ardour_disk_reader_h_ */
