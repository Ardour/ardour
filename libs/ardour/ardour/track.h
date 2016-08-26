/*
    Copyright (C) 2006 Paul Davis

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

#ifndef __ardour_track_h__
#define __ardour_track_h__

#include <boost/shared_ptr.hpp>

#include "pbd/enum_convert.h"

#include "ardour/interthread_info.h"
#include "ardour/recordable.h"
#include "ardour/route.h"
#include "ardour/public_diskstream.h"

namespace ARDOUR {

class Session;
class Playlist;
class RouteGroup;
class Source;
class Region;
class Diskstream;
class IO;
class MonitorControl;
class RecordEnableControl;
class RecordSafeControl;

/** A track is an route (bus) with a recordable diskstream and
 * related objects relevant to tracking, playback and editing.
 *
 * Specifically a track has regions and playlist objects.
 */
class LIBARDOUR_API Track : public Route, public Recordable, public PublicDiskstream
{
  public:
	Track (Session&, std::string name, PresentationInfo::Flag f = PresentationInfo::Flag (0), TrackMode m = Normal, DataType default_type = DataType::AUDIO);
	virtual ~Track ();

	int init ();

	bool set_name (const std::string& str);
	void resync_track_name ();

	TrackMode mode () const { return _mode; }
#ifdef XXX_OLD_DESTRUCTIVE_API_XXX
	virtual int set_mode (TrackMode /*m*/) { return false; }
	virtual bool can_use_mode (TrackMode /*m*/, bool& /*bounce_required*/) { return false; }
	PBD::Signal0<void> TrackModeChanged;
#endif

	boost::shared_ptr<MonitorControl> monitoring_control() const { return _monitoring_control; }

	MonitorState monitoring_state () const;
	MeterState metering_state () const;

	virtual int no_roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame,
	                     bool state_changing);

	int silent_roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame,
	                 bool& need_butler);

	virtual int roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame,
	                  int declick, bool& need_butler) = 0;

	bool needs_butler() const { return _needs_butler; }

	virtual DataType data_type () const = 0;

	bool can_record();

	void use_new_diskstream ();
	virtual boost::shared_ptr<Diskstream> create_diskstream() = 0;
	virtual void set_diskstream (boost::shared_ptr<Diskstream>);

	void set_latency_compensation (framecnt_t);

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
	virtual boost::shared_ptr<Region> bounce (InterThreadInfo& itt) = 0;

	/** Bounce the given range to a new audio region.
	 * @param start start time (in samples)
	 * @param end end time (in samples)
	 * @param itt asynchronous progress report and cancel
	 * @param endpoint the processor to tap the signal off (or nil for the top)
	 * @param include_endpoint include the given processor in the bounced audio.
	 * @return a new audio region (or nil in case of error)
	 */
	virtual boost::shared_ptr<Region> bounce_range (framepos_t start, framepos_t end, InterThreadInfo& itt,
							boost::shared_ptr<Processor> endpoint, bool include_endpoint) = 0;
	virtual int export_stuff (BufferSet& bufs, framepos_t start_frame, framecnt_t nframes,
				  boost::shared_ptr<Processor> endpoint, bool include_endpoint, bool for_export, bool for_freeze) = 0;

	XMLNode&    get_state();
	XMLNode&    get_template();
	virtual int set_state (const XMLNode&, int version);
	static void zero_diskstream_id_in_xml (XMLNode&);

	boost::shared_ptr<AutomationControl> rec_enable_control() const { return _record_enable_control; }
	boost::shared_ptr<AutomationControl> rec_safe_control() const { return _record_safe_control; }

	int prep_record_enabled (bool);
	bool can_be_record_enabled ();
	bool can_be_record_safe ();

	bool using_diskstream_id (PBD::ID) const;

	void set_block_size (pframes_t);

	/* PublicDiskstream interface */
	boost::shared_ptr<Playlist> playlist ();
	void request_input_monitoring (bool);
	void ensure_input_monitoring (bool);
	bool destructive () const;
	std::list<boost::shared_ptr<Source> > & last_capture_sources ();
	void set_capture_offset ();
	std::string steal_write_source_name ();
	void reset_write_sources (bool, bool force = false);
	float playback_buffer_load () const;
	float capture_buffer_load () const;
	int do_refill ();
	int do_flush (RunContext, bool force = false);
	void set_pending_overwrite (bool);
	int seek (framepos_t, bool complete_refill = false);
	bool hidden () const;
	int can_internal_playback_seek (framecnt_t);
	int internal_playback_seek (framecnt_t);
	void non_realtime_input_change ();
	void non_realtime_locate (framepos_t);
	void non_realtime_set_speed ();
	int overwrite_existing_buffers ();
	framecnt_t get_captured_frames (uint32_t n = 0) const;
	int set_loop (Location *);
	void transport_looped (framepos_t);
	bool realtime_set_speed (double, bool);
	void transport_stopped_wallclock (struct tm &, time_t, bool);
	bool pending_overwrite () const;
	double speed () const;
	void prepare_to_stop (framepos_t, framepos_t);
	void set_slaved (bool);
	ChanCount n_channels ();
	framepos_t get_capture_start_frame (uint32_t n = 0) const;
	AlignStyle alignment_style () const;
	AlignChoice alignment_choice () const;
	framepos_t current_capture_start () const;
	framepos_t current_capture_end () const;
	void playlist_modified ();
	int use_playlist (boost::shared_ptr<Playlist>);
	void set_align_style (AlignStyle, bool force=false);
	void set_align_choice (AlignChoice, bool force=false);
	int use_copy_playlist ();
	int use_new_playlist ();
	void adjust_playback_buffering ();
	void adjust_capture_buffering ();

	PBD::Signal0<void> DiskstreamChanged;
	PBD::Signal0<void> FreezeChange;
	/* Emitted when our diskstream is set to use a different playlist */
	PBD::Signal0<void> PlaylistChanged;
	PBD::Signal0<void> SpeedChanged;
	PBD::Signal0<void> AlignmentStyleChanged;

  protected:
	XMLNode& state (bool full);

	boost::shared_ptr<Diskstream> _diskstream;
	MeterPoint    _saved_meter_point;
	TrackMode     _mode;
	bool          _needs_butler;
	boost::shared_ptr<MonitorControl> _monitoring_control;

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

	FreezeRecord          _freeze_record;
	XMLNode*              pending_state;
	bool                  _destructive;

	void maybe_declick (BufferSet&, framecnt_t, int);

	boost::shared_ptr<AutomationControl> _record_enable_control;
	boost::shared_ptr<AutomationControl> _record_safe_control;

	virtual void record_enable_changed (bool, PBD::Controllable::GroupControlDisposition);
	virtual void record_safe_changed (bool, PBD::Controllable::GroupControlDisposition);

	framecnt_t check_initial_delay (framecnt_t nframes, framepos_t&);
	virtual void monitoring_changed (bool, PBD::Controllable::GroupControlDisposition);

private:

	virtual boost::shared_ptr<Diskstream> diskstream_factory (XMLNode const &) = 0;

	void diskstream_playlist_changed ();
	void diskstream_speed_changed ();
	void diskstream_alignment_style_changed ();
	void parameter_changed (std::string const & p);

	std::string _diskstream_name;
};

}; /* namespace ARDOUR*/

namespace PBD {
	DEFINE_ENUM_CONVERT(ARDOUR::Track::FreezeState);
}

#endif /* __ardour_track_h__ */
