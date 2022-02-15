/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_triggerbox_h__
#define __ardour_triggerbox_h__

#include <pthread.h>

#include <atomic>
#include <map>
#include <vector>
#include <string>
#include <exception>

#include <glibmm/threads.h>

#include "pbd/crossthread.h"
#include "pbd/pcg_rand.h"
#include "pbd/pool.h"
#include "pbd/properties.h"
#include "pbd/ringbuffer.h"
#include "pbd/stateful.h"

#include "temporal/beats.h"
#include "temporal/bbt_time.h"
#include "temporal/tempo.h"

#include "evoral/PatchChange.h"

#include "ardour/midi_model.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/processor.h"
#include "ardour/segment_descriptor.h"
#include "ardour/types.h"
#include "ardour/types_convert.h"

#include "ardour/libardour_visibility.h"

class XMLNode;

namespace RubberBand {
	class RubberBandStretcher;
}

namespace ARDOUR {

class Session;
class AudioRegion;
class MidiRegion;
class TriggerBox;
class SideChain;

typedef uint32_t color_t;

LIBARDOUR_API std::string cue_marker_name (int32_t);

class LIBARDOUR_API Trigger : public PBD::Stateful {
  public:
	enum State {
		/* This is the initial state for a Trigger, and means that it is not
		 *doing anything at all
		 */
		Stopped,
		/* A Trigger in this state has been chosen by its parent TriggerBox
		 * (e.g. because of a bang() call that put it in the queue), a Trigger in
		 * this state is waiting for the next occurence of its quantization to
		 *  occur before transitioning to Running
		 */
		WaitingToStart,
		/* a Trigger in this state is going to deliver data during calls
		 *  to its ::run() method.
		 */
		Running,
		/* a Trigger in this state was running, has been re-triggered e.g. by a
		 *  ::bang() call with LaunchStyle set to Repeat, and is waiting for the
		 *  next occurence of its quantization to occur before transitioning
		 *  back to Running.
		 */
		WaitingForRetrigger,
		/* a Trigger in this state is delivering data during calls to ::run(), but
		 *  is waiting for the next occurence of its quantization to occur when it will
		 *transition to Stopping and then Stopped.
		 */
		WaitingToStop,
		/* a Trigger in this state was Running but noticed that it should stop
		 * during the current call to ::run(). By the end of that call, it will
		 * have transitioned to Stopped.
		 */
		Stopping,
		/* a Trigger in this state has played all of its data and is
		 * now silent-filling until we reach the "true end" of the trigger
		 */
		Playout,
	};

	enum LaunchStyle {
		OneShot,  /* mouse down/NoteOn starts; mouse up/NoteOff ignored */
		ReTrigger, /* mouse down/NoteOn starts or retriggers; mouse up/NoteOff */
		Gate,     /* runs till mouse up/note off then to next quantization */
		Toggle,   /* runs till next mouse down/NoteOn */
		Repeat,   /* plays only quantization extent until mouse up/note off */
	};

	enum StretchMode { /* currently mapped to the matching RubberBand::RubberBandStretcher::Option  */
		Crisp,
		Mixed,
		Smooth,
	};

	Trigger (uint32_t index, TriggerBox&);
	virtual ~Trigger() {}

	static void make_property_quarks ();

  protected:
	/* properties controllable by the user */

	PBD::Property<LaunchStyle>          _launch_style;
	PBD::Property<FollowAction>         _follow_action0;
	PBD::Property<FollowAction>         _follow_action1;
	PBD::Property<int>                  _follow_action_probability; /* 1 .. 100 */
	PBD::Property<uint32_t>             _follow_count;
	PBD::Property<Temporal::BBT_Offset> _quantization;
	PBD::Property<Temporal::BBT_Offset> _follow_length;
	PBD::Property<bool>                 _use_follow_length;
	PBD::Property<bool>                 _legato;
	PBD::Property<gain_t>               _gain;
	PBD::Property<float>                _velocity_effect;
	PBD::Property<bool>                 _stretchable;
	PBD::Property<bool>                 _cue_isolated;
	PBD::Property<StretchMode>          _stretch_mode;

	/* Properties that are not CAS-updated at retrigger */

	PBD::Property<std::string>          _name;
	PBD::Property<color_t>              _color;

  public:
	/* this is positioner here so that we can easily keep it in sync
	   with the properties list above.
	*/
	struct UIState {
		std::atomic<unsigned int> generation; /* used for CAS */

		LaunchStyle launch_style = OneShot;
		FollowAction follow_action0 = FollowAction (FollowAction::Again);
		FollowAction follow_action1 = FollowAction (FollowAction::Stop);
		int follow_action_probability = 0;
		uint32_t follow_count = 1;
		Temporal::BBT_Offset quantization = Temporal::BBT_Offset (1, 0, 0);
		Temporal::BBT_Offset follow_length = Temporal::BBT_Offset (1, 0, 0);
		bool use_follow_length = false;
		bool legato = false;
		gain_t gain = 1.0;
		float velocity_effect = 0;
		bool stretchable = true;
		bool cue_isolated = false;
		StretchMode stretch_mode = Trigger::Crisp;

		std::string  name = "";
		color_t      color = 0xBEBEBEFF;
		double       tempo = 0;  //unset

		UIState() : generation (0) {}

		UIState& operator= (UIState const & other) {

			/* we do not copy generation */

			generation = 0;

			launch_style = other.launch_style;
			follow_action0 = other.follow_action0;
			follow_action1 = other.follow_action1;
			follow_action_probability = other.follow_action_probability;
			follow_count = other.follow_count;
			quantization = other.quantization;
			follow_length = other.follow_length;
			use_follow_length = other.use_follow_length;
			legato = other.legato;
			gain = other.gain;
			velocity_effect = other.velocity_effect;
			stretchable = other.stretchable;
			cue_isolated = other.cue_isolated;
			stretch_mode = other.stretch_mode;

			name = other.name;
			color = other.color;
			tempo = other.tempo;

			return *this;
		}
	};

#define TRIGGERBOX_PROPERTY_DECL(name,type) void set_ ## name (type); type name () const;
#define TRIGGERBOX_PROPERTY_DECL_CONST_REF(name,type) void set_ ## name (type const &); type name () const

	TRIGGERBOX_PROPERTY_DECL (launch_style, LaunchStyle);
	TRIGGERBOX_PROPERTY_DECL_CONST_REF (follow_action0, FollowAction);
	TRIGGERBOX_PROPERTY_DECL_CONST_REF (follow_action1, FollowAction);
	TRIGGERBOX_PROPERTY_DECL (follow_action_probability, int);
	TRIGGERBOX_PROPERTY_DECL (follow_count, uint32_t);
	TRIGGERBOX_PROPERTY_DECL_CONST_REF (quantization, Temporal::BBT_Offset);
	TRIGGERBOX_PROPERTY_DECL_CONST_REF (follow_length, Temporal::BBT_Offset);
	TRIGGERBOX_PROPERTY_DECL (use_follow_length, bool);
	TRIGGERBOX_PROPERTY_DECL (legato, bool);
	TRIGGERBOX_PROPERTY_DECL (gain, gain_t);
	TRIGGERBOX_PROPERTY_DECL (velocity_effect, float);
	TRIGGERBOX_PROPERTY_DECL (stretchable, bool);
	TRIGGERBOX_PROPERTY_DECL (cue_isolated, bool);
	TRIGGERBOX_PROPERTY_DECL (stretch_mode, StretchMode);
	TRIGGERBOX_PROPERTY_DECL (color, color_t);
	TRIGGERBOX_PROPERTY_DECL_CONST_REF (name, std::string);

#undef TRIGGERBOX_PROPERTY_DECL
#undef TRIGGERBOX_PROPERTY_DECL_CONST_REF

	/* Calling ::bang() will cause this Trigger to be placed in its owning
	   TriggerBox's queue.
	*/
	void bang ();

	/* Calling ::unbang() will cause a running Trigger to begin the process
	   of stopping. If the Trigger is not running, it will move it to a
	   full Stopped state.
	*/
	void unbang ();

	/* Calling ::request_stop() to stop a Trigger at the earliest possible
	 * opportunity, rather than at the next quantization point.
	 */
	void request_stop ();

	virtual pframes_t run (BufferSet&, samplepos_t start_sample, samplepos_t end_sample,
	                       Temporal::Beats const & start, Temporal::Beats const & end,
	                       pframes_t nframes, pframes_t offset, double bpm) = 0;
	virtual void set_start (timepos_t const &) = 0;
	virtual void set_end (timepos_t const &) = 0;
	virtual void set_length (timecnt_t const &) = 0;
	virtual void reload (BufferSet&, void*) = 0;
	virtual void io_change () {}
	virtual void set_legato_offset (timepos_t const & offset) = 0;

	timepos_t current_pos() const;
	double position_as_fraction() const;

	Temporal::BBT_Time compute_start (Temporal::TempoMap::SharedPtr const &, samplepos_t start, samplepos_t end, Temporal::BBT_Offset const & q, samplepos_t& start_samples, bool& will_start);
	virtual timepos_t compute_end (Temporal::TempoMap::SharedPtr const &, Temporal::BBT_Time const &, samplepos_t) = 0;
	virtual void start_and_roll_to (samplepos_t start, samplepos_t position) = 0;

	/* because follow actions involve probability is it easier to code the will-not-follow case */

	bool will_not_follow() const;
	bool will_follow() const { return !will_not_follow(); }

	/* assumes that this is currently playing but does not enforce it */
	bool cue_launched() const { return _cue_launched; }

	virtual bool probably_oneshot () const = 0;

	virtual timepos_t start_offset () const = 0; /* offset from start of data */
	virtual timepos_t current_length() const = 0; /* offset from start() */
	virtual timepos_t natural_length() const = 0; /* offset from start() */

	void process_state_requests (BufferSet& bufs, pframes_t dest_offset);

	bool active() const { return _state >= Running; }
	State state() const { return _state; }

	void set_region (boost::shared_ptr<Region>, bool use_thread = true);
	void clear_region ();
	virtual int set_region_in_worker_thread (boost::shared_ptr<Region>) = 0;
	boost::shared_ptr<Region> region() const { return _region; }

	uint32_t index() const { return _index; }

	/* Managed by TriggerBox, these record the time that the trigger is
	 * scheduled to start or stop at. Computed in
	 * Trigger::maybe_compute_next_transition().
	 */
	samplepos_t transition_samples;
	Temporal::Beats transition_beats;

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	void maybe_compute_next_transition (samplepos_t start_sample, Temporal::Beats const & start, Temporal::Beats const & end, pframes_t& nframes, pframes_t& dest_offset);


	bool compute_quantized_transition (samplepos_t start_sample, Temporal::Beats const & start, Temporal::Beats const & end,
	                                   Temporal::BBT_Time& t_bbt, Temporal::Beats& t_beats, samplepos_t& t_samples,
	                                   Temporal::TempoMap::SharedPtr const & tmap, Temporal::BBT_Offset const & q);

	pframes_t compute_next_transition (samplepos_t start_sample, Temporal::Beats const & start, Temporal::Beats const & end, pframes_t nframes,
	                                   Temporal::BBT_Time& t_bbt, Temporal::Beats& t_beats, samplepos_t& t_samples,
	                                   Temporal::TempoMap::SharedPtr const & tmap);


	template<typename TriggerType>
		void start_and_roll_to (samplepos_t start_pos, samplepos_t end_position, TriggerType& trigger,
		                        pframes_t (TriggerType::*run_method) (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample,
		                                                              Temporal::Beats const & start_beats, Temporal::Beats const & end_beats,
		                                                              pframes_t nframes, pframes_t dest_offset, double bpm));
	void set_next_trigger (int n);
	int next_trigger() const { return _next_trigger; }

	/* any non-zero value will work for the default argument, and means
	   "use your own launch quantization". BBT_Offset (0, 0, 0) means what
	   it says: start immediately
	*/
	void startup (BufferSet&, pframes_t dest_offset, Temporal::BBT_Offset const & start_quantization = Temporal::BBT_Offset (9, 3,0));
	virtual void shutdown (BufferSet& bufs, pframes_t dest_offset);
	virtual void jump_start ();
	virtual void jump_stop (BufferSet& bufs, pframes_t dest_offset);
	void begin_stop (bool explicit_stop = false);

	bool explicitly_stopped() const { return _explicitly_stopped; }

	uint32_t loop_count() const { return _loop_cnt; }

	void set_ui (void*);
	void* ui () const { return _ui; }

	TriggerBox& box() const { return _box; }

	double estimated_tempo() const { return _estimated_tempo; }
	virtual double segment_tempo() const = 0;
	virtual void set_segment_tempo (double t) = 0;

	virtual void setup_stretcher () = 0;

	Temporal::Meter meter() const { return _meter; }

	void set_velocity_gain (gain_t g) {_pending_velocity_gain=g;}

	void set_pending (Trigger*);
	Trigger* swap_pending (Trigger*);

	void update_properties ();

	static Trigger * const MagicClearPointerValue;

	virtual SegmentDescriptor get_segment_descriptor () const = 0;

	static void request_trigger_delete (Trigger* t);

	/* these operations are provided to get/set all the "user visible" trigger properties at once */
	/* examples: drag+dropping from slot to slot, or "Range->Bounce to Slot", where a single operation sets many  */
	void get_ui_state (UIState &state) const;
	void set_ui_state (UIState &state);

  protected:
	struct UIRequests {
		std::atomic<bool> stop;
		UIRequests() : stop (false) {}
	};

	boost::shared_ptr<Region> _region;
	samplecnt_t                process_index;
	samplepos_t                final_processed_sample;  /* where we stop playing, in process time, compare with process_index */
	UIState                    ui_state;
	TriggerBox&               _box;
	UIRequests                _requests;
	State                     _state;
	std::atomic<int>          _bang;
	std::atomic<int>          _unbang;
	uint32_t                  _index;
	int                       _next_trigger;
	uint32_t                  _loop_cnt; /* how many times in a row has this played */
	void*                     _ui;
	bool                      _explicitly_stopped;
	gain_t                    _pending_velocity_gain;
	gain_t                    _velocity_gain;
	bool                      _cue_launched;

	void copy_to_ui_state ();


	/* computed from data */

	double                    _estimated_tempo;  //TODO:  this should come from the MIDI file
	double                    _segment_tempo;  //TODO: this will likely get stored in the SegmentDescriptor for audio triggers

	/* basic process is :
	   1) when a file is loaded, we infer its bpm either by minibpm's estimate, a flag in the filename, metadata (TBD) or other means
	   2) we assume the clip must have an integer number of beats in it  (simplest case is a one-bar loop with 4 beats in it)
	   3) ...so we round to the nearest beat length, and set the tempo to *exactly* fit the sample-length into the assumed beat-length
	   4) the user may recognize a problem:  "this was a 3/4 beat, which was rounded to 4 beats but it should have been 3"
	   5) if the user changes the beat-length, then the tempo is recalculated for use during stretching
	   6) someday, we will also allow the sample start and length to be adjusted in a trimmer, and that will also adjust the tempo
	   7) in all cases the user should be in final control; but our "internal" value for stretching are just sample-start and BPM, end of story
	*/
	double                    _beatcnt;
	Temporal::Meter           _meter;

	samplepos_t                expected_end_sample;
	Temporal::BBT_Offset      _start_quantization;
	std::atomic<Trigger*>     _pending;
	std::atomic<unsigned int>  last_property_generation;

	void when_stopped_during_run (BufferSet& bufs, pframes_t dest_offset);
	void set_region_internal (boost::shared_ptr<Region>);
	virtual void retrigger();
	virtual void _startup (BufferSet&, pframes_t dest_offset, Temporal::BBT_Offset const &);

	bool internal_use_follow_length() const;
	void send_property_change (PBD::PropertyChange pc);
};

typedef boost::shared_ptr<Trigger> TriggerPtr;

class LIBARDOUR_API AudioTrigger : public Trigger {
  public:
	AudioTrigger (uint32_t index, TriggerBox&);
	~AudioTrigger ();

	template<bool actually_run>  pframes_t audio_run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample,
	                                                  Temporal::Beats const & start, Temporal::Beats const & end,
	                                                  pframes_t nframes, pframes_t dest_offset, double bpm);

	pframes_t run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, Temporal::Beats const & start, Temporal::Beats const & end, pframes_t nframes, pframes_t dest_offset, double bpm) {
		return audio_run<true> (bufs, start_sample, end_sample, start, end, nframes, dest_offset, bpm);
	}

	StretchMode stretch_mode() const { return _stretch_mode; }
	void set_stretch_mode (StretchMode);

	double segment_tempo() const { return _segment_tempo; }
	void set_segment_tempo (double t);

	double segment_beatcnt () { return _beatcnt; }
	void set_segment_beatcnt (double count);

	void set_start (timepos_t const &);
	void set_end (timepos_t const &);
	void set_legato_offset (timepos_t const &);
	void set_length (timecnt_t const &);
	timepos_t start_offset () const; /* offset from start of data */
	timepos_t current_length() const; /* offset from start of data */
	timepos_t natural_length() const; /* offset from start of data */
	void reload (BufferSet&, void*);
	void io_change ();
	bool probably_oneshot () const;

	int set_region_in_worker_thread (boost::shared_ptr<Region>);
	void jump_start ();
	void jump_stop (BufferSet& bufs, pframes_t dest_offset);

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	RubberBand::RubberBandStretcher* stretcher() { return (_stretcher); }

	SegmentDescriptor get_segment_descriptor () const;
	timepos_t compute_end (Temporal::TempoMap::SharedPtr const &, Temporal::BBT_Time const &, samplepos_t);
	void start_and_roll_to (samplepos_t start, samplepos_t position);

	bool stretching () const;

  protected:
	void retrigger ();

  private:
	struct Data : std::vector<Sample*> {
		samplecnt_t length;

		Data () : length (0) {}
	};

	Data        data;
	RubberBand::RubberBandStretcher*  _stretcher;
	samplepos_t _start_offset;


	/* computed during run */

	samplecnt_t read_index;
	samplepos_t last_readable_sample;   /* where the data runs out, relative to the start of the data, compare with read_index */
	samplepos_t _legato_offset;
	samplecnt_t retrieved;
	samplecnt_t got_stretcher_padding;
	samplecnt_t to_pad;
	samplecnt_t to_drop;

	virtual void setup_stretcher ();

	void drop_data ();
	int load_data (boost::shared_ptr<AudioRegion>);
	void estimate_tempo ();
	void reset_stretcher ();
	void _startup (BufferSet&, pframes_t dest_offset, Temporal::BBT_Offset const &);
};


class LIBARDOUR_API MIDITrigger : public Trigger {
  public:
	MIDITrigger (uint32_t index, TriggerBox&);
	~MIDITrigger ();

	template<bool actually_run> pframes_t midi_run (BufferSet&, samplepos_t start_sample, samplepos_t end_sample,
	                                                Temporal::Beats const & start_beats, Temporal::Beats const & end_beats, pframes_t nframes, pframes_t offset, double bpm);

	pframes_t run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, Temporal::Beats const & start, Temporal::Beats const & end, pframes_t nframes, pframes_t dest_offset, double bpm) {
		return midi_run<true> (bufs, start_sample, end_sample, start, end, nframes, dest_offset, bpm);
	}

	void set_start (timepos_t const &);
	void set_end (timepos_t const &);
	void set_legato_offset (timepos_t const &);
	void set_length (timecnt_t const &);
	timepos_t start_offset () const;
	timepos_t end() const;            /* offset from start of data */
	timepos_t current_length() const; /* offset from start of data */
	timepos_t natural_length() const; /* offset from start of data */
	void reload (BufferSet&, void*);
	bool probably_oneshot () const;

	int set_region_in_worker_thread (boost::shared_ptr<Region>);
	void jump_start ();
	void shutdown (BufferSet& bufs, pframes_t dest_offset);
	void jump_stop (BufferSet& bufs, pframes_t dest_offset);

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	SegmentDescriptor get_segment_descriptor () const;
	timepos_t compute_end (Temporal::TempoMap::SharedPtr const &, Temporal::BBT_Time const &, samplepos_t);
	void start_and_roll_to (samplepos_t start, samplepos_t position);

	void set_patch_change (Evoral::PatchChange<MidiBuffer::TimeType> const &);
	Evoral::PatchChange<MidiBuffer::TimeType> const & patch_change (uint8_t) const;
	void unset_patch_change (uint8_t channel);
	void unset_all_patch_changes ();
	bool patch_change_set (uint8_t channel) const;

	/* theoretically, MIDI files can have a dedicated tempo outside the session tempo map (*un-stretched*) but this is currently unimplemented */
	/* boilerplate tempo functions are provided here so we don't have to do constant dynamic_cast checks to use the tempo+stretch APIs */
	virtual double segment_tempo() const {return 120.0;}
	virtual void set_segment_tempo (double t) {}
	virtual void setup_stretcher () {}

	void set_channel_map (int channel, int target);
	void unset_channel_map (int channel);
	int channel_map (int channel);
	std::vector<int> const & channel_map() const { return _channel_map; }

  protected:
	void retrigger ();

  private:
	PBD::ID data_source;
	PBD::ScopedConnection content_connection;

	Temporal::Beats final_beat;

	Temporal::DoubleableBeats data_length;   /* using timestamps from data */
	Temporal::DoubleableBeats last_event_beats;

	Temporal::BBT_Offset _start_offset;
	Temporal::BBT_Offset _legato_offset;

	MidiModel::const_iterator iter;
	boost::shared_ptr<MidiModel> model;

	Evoral::PatchChange<MidiBuffer::TimeType> _patch_change[16];
	std::vector<int> _channel_map;

	int load_data (boost::shared_ptr<MidiRegion>);
	void compute_and_set_length ();
	void _startup (BufferSet&, pframes_t dest_offset, Temporal::BBT_Offset const &);
};

class LIBARDOUR_API TriggerBoxThread
{
  public:
	TriggerBoxThread ();
	~TriggerBoxThread();

	static void init_request_pool() { Request::init_pool(); }

	void set_region (TriggerBox&, uint32_t slot, boost::shared_ptr<Region>);
	void request_delete_trigger (Trigger* t);

	void summon();
	void stop();
	void wait_until_finished();

  private:
	static void* _thread_work(void *arg);
	void*         thread_work();

	enum RequestType {
		Quit,
		SetRegion,
		DeleteTrigger
	};

	struct Request {

		Request (RequestType t) : type (t) {}

		RequestType type;
		/* for set region */
		TriggerBox* box;
		uint32_t slot;
		boost::shared_ptr<Region> region;
		/* for DeleteTrigger */
		Trigger* trigger;

		void* operator new (size_t);
		void  operator delete (void* ptr, size_t);

		static MultiAllocSingleReleasePool* pool;
		static void init_pool ();
	};

	pthread_t thread;
	PBD::RingBuffer<Request*>  requests;

	CrossThreadChannel _xthread;
	void queue_request (Request*);
	void delete_trigger (Trigger*);
};

struct CueRecord {
	int32_t cue_number;
	samplepos_t when;

	CueRecord (int32_t cn, samplepos_t t): cue_number (cn), when (t) {}
	CueRecord () : cue_number (0), when (0) {}
};

typedef PBD::RingBuffer<CueRecord> CueRecords;

class LIBARDOUR_API TriggerBox : public Processor
{
  public:
	TriggerBox (Session&, DataType dt);
	~TriggerBox ();

	static CueRecords cue_records;
	static bool cue_recording () { return _cue_recording; }
	static void set_cue_recording (bool yn);
	static PBD::Signal0<void> CueRecordingChanged;

	void run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool result_required);
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);

	bool empty() const { return _active_slots == 0; }
	PBD::Signal0<void> EmptyStatusChanged;

	int32_t order() const { return _order; }
	void set_order(int32_t n);

	typedef std::vector<TriggerPtr> Triggers;

	TriggerPtr trigger (Triggers::size_type);

	bool bang_trigger (TriggerPtr);
	bool unbang_trigger (TriggerPtr);
	void add_trigger (TriggerPtr);

	void fast_forward (CueEvents const &, samplepos_t transport_postiion);
	bool fast_forwarding() const { return _fast_fowarding; }

	void set_pending (uint32_t slot, Trigger*);

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	void set_from_path (uint32_t slot, std::string const & path);
	void set_from_selection (uint32_t slot, boost::shared_ptr<Region>);

	DataType data_type() const { return _data_type; }

	void stop_all_immediately ();
	void stop_all_quantized ();

	TriggerPtr currently_playing() const { return _currently_playing; }

	TriggerPtr trigger_by_id (PBD::ID);

	void clear_all_triggers ();
	void set_all_follow_action (ARDOUR::FollowAction const &, uint32_t n=0);
	void set_all_launch_style (ARDOUR::Trigger::LaunchStyle);
	void set_all_quantization (Temporal::BBT_Offset const&);
	void set_all_probability (int zero_to_a_hundred);

	/* Returns a negative value is there is no active Trigger, or a value between 0
	 * and 1.0 if there is, corresponding to the value of position_as_fraction() for
	 *  the active Trigger.
	 */
	double position_as_fraction() const;

	void queue_explict (uint32_t);
	TriggerPtr get_next_trigger ();
	TriggerPtr peek_next_trigger ();

	void add_midi_sidechain ();
	void update_sidechain_name ();

	void set_ignore_patch_changes (bool);
	bool ignore_patch_changes () const { return _ignore_patch_changes; }

	void request_reload (int32_t slot, void*);
	void set_region (uint32_t slot, boost::shared_ptr<Region> region);

	void non_realtime_transport_stop (samplepos_t now, bool flush);
	void non_realtime_locate (samplepos_t now);
	void realtime_handle_transport_stopped ();

	void enqueue_trigger_state_for_region (boost::shared_ptr<Region>, boost::shared_ptr<Trigger::UIState>);

	/* valid only within the ::run() call tree */
	int32_t active_scene() const { return _active_scene; }

	PBD::Signal1<void,uint32_t> TriggerSwapped;

	enum TriggerMidiMapMode {
		AbletonPush,
		SequentialNote,
		ByMidiChannel
	};

	/* This is null for TriggerBoxen constructed with DataType::AUDIO */
	MidiStateTracker* tracker;

	static Temporal::BBT_Offset assumed_trigger_duration () { return _assumed_trigger_duration; }
	static void set_assumed_trigger_duration (Temporal::BBT_Offset const &);

	static TriggerMidiMapMode midi_map_mode () { return _midi_map_mode; }
	static void set_midi_map_mode (TriggerMidiMapMode m);

	static int first_midi_note() { return _first_midi_note; }
	static void set_first_midi_note (int n);

	static void init ();

	static TriggerBoxThread* worker;

	static void start_transport_stop (Session&);

  private:
	struct Requests {
		std::atomic<bool> stop_all;

		Requests () : stop_all (false) {}
	};

	static Temporal::BBT_Offset _assumed_trigger_duration;

	DataType _data_type;
	int32_t _order;
	Glib::Threads::RWLock trigger_lock; /* protects all_triggers */
	Triggers all_triggers;

	typedef std::vector<Trigger*> PendingTriggers;
	PendingTriggers pending;

	PBD::RingBuffer<uint32_t> explicit_queue; /* user queued triggers */
	TriggerPtr               _currently_playing;
	Requests                 _requests;
	bool                     _stop_all;
	int32_t                  _active_scene;
	int32_t                  _active_slots;
	bool                     _ignore_patch_changes;
	bool                     _locate_armed;
	bool                     _fast_fowarding;

	boost::shared_ptr<SideChain> _sidechain;

	PBD::PCGRand _pcg;

	/* These four are accessed (read/write) only from process() context */

	void drop_triggers ();
	void process_ui_trigger_requests ();
	void process_midi_trigger_requests (BufferSet&);
	int determine_next_trigger (uint32_t n);
	void stop_all ();

	void maybe_swap_pending (uint32_t);

	int note_to_trigger (int node, int channel);

	void note_on (int note_number, int velocity);
	void note_off (int note_number, int velocity);

	void reconnect_to_default ();
	void parameter_changed (std::string const &);

	static int _first_midi_note;
	static TriggerMidiMapMode _midi_map_mode;

	struct Request {
		enum Type {
			Use,
			Reload,
		};

		Type type;

		/* cannot use a union here because we need Request to have a
		 * "trivial" constructor.
		 */

		TriggerPtr trigger;
		void* ptr;
		int32_t slot;

		Request (Type t) : type (t) {}

		static MultiAllocSingleReleasePool* pool;
		static void init_pool();

		void* operator new (size_t);
		void  operator delete (void* ptr, size_t);
	};

	typedef PBD::RingBuffer<Request*> RequestBuffer;
	RequestBuffer requests;

	void process_requests (BufferSet&);
	void process_request (BufferSet&, Request*);

	void reload (BufferSet& bufs, int32_t slot, void* ptr);

	PBD::ScopedConnection stop_all_connection;

	static void init_pool();

	static std::atomic<int> active_trigger_boxes;
	static std::atomic<bool> _cue_recording;
};

class TriggerReference
{
public:
	TriggerReference () : box (0), slot (0) {}
	TriggerReference (ARDOUR::TriggerBox& b, uint32_t s) : box (&b), slot (s) {}

	boost::shared_ptr<ARDOUR::Trigger> trigger() const { assert (box); return box->trigger (slot); }

	ARDOUR::TriggerBox* box;
	uint32_t            slot;
};

namespace Properties {
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> running;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> legato;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> use_follow_length;
	LIBARDOUR_API extern PBD::PropertyDescriptor<Temporal::BBT_Offset> quantization;
	LIBARDOUR_API extern PBD::PropertyDescriptor<Temporal::BBT_Offset> follow_length;
	LIBARDOUR_API extern PBD::PropertyDescriptor<Trigger::LaunchStyle> launch_style;
	LIBARDOUR_API extern PBD::PropertyDescriptor<FollowAction> follow_action0;
	LIBARDOUR_API extern PBD::PropertyDescriptor<FollowAction> follow_action1;
	LIBARDOUR_API extern PBD::PropertyDescriptor<Trigger::StretchMode> stretch_mode;
	LIBARDOUR_API extern PBD::PropertyDescriptor<uint32_t> follow_count;
	LIBARDOUR_API extern PBD::PropertyDescriptor<int> follow_action_probability;
	LIBARDOUR_API extern PBD::PropertyDescriptor<float> velocity_effect;
	LIBARDOUR_API extern PBD::PropertyDescriptor<gain_t> gain;
	LIBARDOUR_API extern PBD::PropertyDescriptor<uint32_t> currently_playing;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> stretchable;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> cue_isolated;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> patch_change; /* type not important */
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> channel_map; /* type not important */

	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> tempo_meter; /* only used to transmit changes, not storage */
}


} // namespace ARDOUR

namespace PBD {
DEFINE_ENUM_CONVERT(ARDOUR::FollowAction::Type);
DEFINE_ENUM_CONVERT(ARDOUR::Trigger::LaunchStyle);
DEFINE_ENUM_CONVERT(ARDOUR::Trigger::StretchMode);
} /* namespace PBD */


#endif /* __ardour_triggerbox_h__ */
