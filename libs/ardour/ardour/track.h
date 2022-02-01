/*
 * Copyright (C) 2006-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_track_h__
#define __ardour_track_h__

#include <boost/shared_ptr.hpp>

#include "pbd/enum_convert.h"

#include "ardour/interthread_info.h"
#include "ardour/recordable.h"
#include "ardour/route.h"

namespace ARDOUR {

class Session;
class Playlist;
class RouteGroup;
class Source;
class Region;
class DiskReader;
class DiskWriter;
class IO;
class RecordEnableControl;
class RecordSafeControl;
class MidiNoteTracker;

/** A track is an route (bus) with a recordable diskstream and
 * related objects relevant to recording, playback and editing.
 *
 * Specifically a track has a playlist object that describes material
 * to be played from disk, and modifies that object during recording and
 * editing.
 */
class LIBARDOUR_API Track : public Route, public Recordable
{
public:
	Track (Session&, std::string name, PresentationInfo::Flag f = PresentationInfo::Flag (0), TrackMode m = Normal, DataType default_type = DataType::AUDIO);
	virtual ~Track ();

	int init ();

	bool set_name (const std::string& str);
	int resync_take_name (std::string newname = "");

	TrackMode mode () const { return _mode; }

	bool set_processor_state (XMLNode const& node, int version, XMLProperty const* prop, ProcessorList& new_order, bool& must_configure);

	bool declick_in_progress () const;

	bool can_record();

	enum FreezeState {
		NoFreeze,
		Frozen,
		UnFrozen
	};

	FreezeState freeze_state() const;

	virtual void freeze_me (InterThreadInfo&) = 0;
	virtual void unfreeze () = 0;

	/** Test if the track can be bounced with the given settings.
	 * If sends/inserts/returns are present in the signal path or the given track
	 * has no audio outputs bouncing is not possible.
	 *
	 * @param endpoint the processor to tap the signal off (or nil for the top)
	 * @param include_endpoint include the given processor in the bounced audio.
	 * @return true if the track can be bounced, or false otherwise.
	 */
	virtual bool bounceable (boost::shared_ptr<Processor> endpoint, bool include_endpoint) const = 0;

	/** bounce track from session start to session end to new region
	 *
	 * @param itt asynchronous progress report and cancel
	 * @return a new audio region (or nil in case of error)
	 */
	virtual boost::shared_ptr<Region> bounce (InterThreadInfo& itt, std::string const& name) = 0;

	/** Bounce the given range to a new audio region.
	 * @param start start time (in samples)
	 * @param end end time (in samples)
	 * @param itt asynchronous progress report and cancel
	 * @param endpoint the processor to tap the signal off (or nil for the top)
	 * @param include_endpoint include the given processor in the bounced audio.
	 * @return a new audio region (or nil in case of error)
	 */
	virtual boost::shared_ptr<Region> bounce_range (samplepos_t start, samplepos_t end, InterThreadInfo& itt,
	                                                boost::shared_ptr<Processor> endpoint, bool include_endpoint,
	                                                std::string const& name) = 0;

	virtual int export_stuff (BufferSet& bufs, samplepos_t start_sample, samplecnt_t nframes,
	                          boost::shared_ptr<Processor> endpoint, bool include_endpoint, bool for_export, bool for_freeze,
	                          MidiNoteTracker&) = 0;

	virtual int set_state (const XMLNode&, int version);
	static void zero_diskstream_id_in_xml (XMLNode&);

	boost::shared_ptr<AutomationControl> rec_enable_control() const { return _record_enable_control; }
	boost::shared_ptr<AutomationControl> rec_safe_control() const { return _record_safe_control; }

	int prep_record_enabled (bool);
	bool can_be_record_enabled ();
	bool can_be_record_safe ();

	void use_captured_sources (SourceList&, CaptureInfos const &);

	void set_block_size (pframes_t);

	/* used by DiskReader request_overwrite_buffer(), to create
	 * a SessionEvent with weak_ptr<> reference
	 */
	boost::shared_ptr<Track> shared_ptr () {
		return boost::dynamic_pointer_cast<Track> (shared_from_this());
	}

	boost::shared_ptr<Playlist> playlist ();
	void request_input_monitoring (bool);
	void ensure_input_monitoring (bool);
	std::list<boost::shared_ptr<Source> > & last_capture_sources ();
	std::string steal_write_source_name ();
	void reset_write_sources (bool, bool force = false);
	float playback_buffer_load () const;
	float capture_buffer_load () const;
	int do_refill ();
	int do_flush (RunContext, bool force = false);
	void set_pending_overwrite (OverwriteReason);
	int seek (samplepos_t, bool complete_refill = false);
	bool can_internal_playback_seek (samplecnt_t);
	void internal_playback_seek (samplecnt_t);
	void non_realtime_locate (samplepos_t);
	bool overwrite_existing_buffers ();
	samplecnt_t get_captured_samples (uint32_t n = 0) const;
	void transport_looped (samplepos_t);
	void transport_stopped_wallclock (struct tm &, time_t, bool);
	void mark_capture_xrun ();
	bool pending_overwrite () const;
	void set_slaved (bool);
	ChanCount n_channels ();
	samplepos_t get_capture_start_sample (uint32_t n = 0) const;
	AlignStyle alignment_style () const;
	AlignChoice alignment_choice () const;
	samplepos_t current_capture_start () const;
	samplepos_t current_capture_end () const;
	void set_align_style (AlignStyle, bool force=false);
	void set_align_choice (AlignChoice, bool force=false);
	void playlist_modified ();
	int use_playlist (DataType, boost::shared_ptr<Playlist>, bool set_orig = true);
	int find_and_use_playlist (DataType, PBD::ID const &);
	int use_copy_playlist ();
	int use_new_playlist (DataType);
	int use_default_new_playlist () {
		return use_new_playlist (data_type());
	}
	void adjust_playback_buffering ();
	void adjust_capture_buffering ();

	PBD::Signal0<void> FreezeChange;
	PBD::Signal0<void> PlaylistChanged;
	PBD::Signal0<void> PlaylistAdded;
	PBD::Signal0<void> SpeedChanged;
	PBD::Signal0<void> AlignmentStyleChanged;
	PBD::Signal0<void> ChanCountChanged;

protected:
	XMLNode& state (bool save_template);

	boost::shared_ptr<Playlist>   _playlists[DataType::num_types];

	MeterPoint    _saved_meter_point;
	TrackMode     _mode;

	//private: (FIXME)
	struct FreezeRecordProcessorInfo {
		FreezeRecordProcessorInfo(XMLNode& st, boost::shared_ptr<Processor> proc)
			: state (st), processor (proc) {}

		XMLNode                      state;
		boost::shared_ptr<Processor> processor;
		PBD::ID                      id;
	};

	struct FreezeRecord {
		FreezeRecord()
			: have_mementos(false)
		{}

		~FreezeRecord();

		boost::shared_ptr<Playlist>        playlist;
		std::vector<FreezeRecordProcessorInfo*> processor_info;
		bool                               have_mementos;
		FreezeState                        state;
	};

	virtual void set_state_part_two () = 0;

	FreezeRecord _freeze_record;
	XMLNode*      pending_state;

	boost::shared_ptr<AutomationControl> _record_enable_control;
	boost::shared_ptr<AutomationControl> _record_safe_control;

	virtual void record_enable_changed (bool, PBD::Controllable::GroupControlDisposition);
	virtual void record_safe_changed (bool, PBD::Controllable::GroupControlDisposition);

	virtual void monitoring_changed (bool, PBD::Controllable::GroupControlDisposition);

	AlignChoice _alignment_choice;
	void set_align_choice_from_io ();

	void use_captured_audio_sources (SourceList&, CaptureInfos const &);
	void use_captured_midi_sources (SourceList&, CaptureInfos const &);

private:
	void parameter_changed (std::string const & p);
	void input_changed ();
	void chan_count_changed ();

	std::string _diskstream_name;
	bool        _pending_name_change;
};

}; /* namespace ARDOUR*/

namespace PBD {
	DEFINE_ENUM_CONVERT(ARDOUR::Track::FreezeState);
}

#endif /* __ardour_track_h__ */
