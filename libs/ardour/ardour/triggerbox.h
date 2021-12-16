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

#include "ardour/midi_model.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/processor.h"
#include "ardour/segment_descriptor.h"

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
		Stopping
	};

	Trigger (uint64_t index, TriggerBox&);
	virtual ~Trigger() {}

	static void make_property_quarks ();

	void set_name (std::string const &);
	std::string name() const { return _name; }

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
	                       pframes_t nframes, pframes_t offset, bool first, double bpm) = 0;
	virtual void set_start (timepos_t const &) = 0;
	virtual void set_end (timepos_t const &) = 0;
	virtual void set_length (timecnt_t const &) = 0;
	virtual void reload (BufferSet&, void*) = 0;

	virtual double position_as_fraction() const = 0;
	virtual void set_expected_end_sample (Temporal::TempoMap::SharedPtr const &, Temporal::BBT_Time const &) = 0;

	void set_use_follow (bool yn);
	bool use_follow() const { return _use_follow; }

	timepos_t start_offset () const; /* offset from start of data */
	virtual timepos_t current_length() const = 0; /* offset from start() */
	virtual timepos_t natural_length() const = 0; /* offset from start() */

	void process_state_requests ();

	bool active() const { return _state >= Running; }
	State state() const { return _state; }

	enum LaunchStyle {
		OneShot,  /* mouse down/NoteOn starts; mouse up/NoteOff ignored */
		Gate,     /* runs till mouse up/note off then to next quantization */
		Toggle,   /* runs till next mouse down/NoteOn */
		Repeat,   /* plays only quantization extent until mouse up/note off */
	};

	LaunchStyle launch_style() const { return _launch_style; }
	void set_launch_style (LaunchStyle);

	enum FollowAction {
		None,
		Stop,
		Again,
		QueuedTrigger, /* DP-style */
		NextTrigger,   /* Live-style, and below */
		PrevTrigger,
		FirstTrigger,
		LastTrigger,
		AnyTrigger,
		OtherTrigger,
	};

	FollowAction follow_action (uint64_t n) const { assert (n < 2); return _follow_action[n]; }
	void set_follow_action (FollowAction, uint64_t n);

	void set_region (boost::shared_ptr<Region>);
	virtual int set_region_threaded (boost::shared_ptr<Region>) = 0;
	boost::shared_ptr<Region> region() const { return _region; }

	Temporal::BBT_Offset quantization() const;
	void set_quantization (Temporal::BBT_Offset const &);


	uint64_t index() const { return _index; }

	/* Managed by TriggerBox, these record the time that the trigger is
	 * scheduled to start or stop at. Computed in
	 * Trigger::maybe_compute_next_transition().
	 */
	samplepos_t transition_samples;
	Temporal::Beats transition_beats;

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	pframes_t maybe_compute_next_transition (samplepos_t start_sample, Temporal::Beats const & start, Temporal::Beats const & end, pframes_t dest_offset, bool passthru);

	void set_next_trigger (int n);
	int next_trigger() const { return _next_trigger; }

	void set_follow_action_probability (int zero_to_a_hundred);
	int  follow_action_probability() const { return _follow_action_probability; }

	virtual void set_legato_offset (timepos_t const & offset) = 0;
	virtual timepos_t current_pos() const = 0;
	void set_legato (bool yn);
	bool legato () const { return _legato; }

	virtual void startup ();
	virtual void shutdown ();
	virtual void jump_start ();
	virtual void jump_stop ();
	void begin_stop (bool explicit_stop = false);

	bool explicitly_stopped() const { return _explicitly_stopped; }

	uint32_t loop_count() const { return _loop_cnt; }
	uint32_t follow_count() const { return _follow_count; }
	void set_follow_count (uint32_t n);

	void set_ui (void*);
	void* ui () const { return _ui; }

	TriggerBox const & box() const { return _box; }

	gain_t gain() const { return _pending_gain; }
	void set_gain (gain_t);

	float midi_velocity_effect() const { return _midi_velocity_effect; }
	void set_midi_velocity_effect (float);

	double apparent_tempo() const { return _apparent_tempo; }
	double set_tempo (double t);

	virtual SegmentDescriptor get_segment_descriptor () const = 0;

  protected:
	struct UIRequests {
		std::atomic<bool> stop;
		UIRequests() : stop (false) {}
	};

	boost::shared_ptr<Region> _region;
	TriggerBox&               _box;
	UIRequests                _requests;
	State                     _state;
	std::atomic<int>          _bang;
	std::atomic<int>          _unbang;
	uint64_t                  _index;
	int                       _next_trigger;
	LaunchStyle               _launch_style;
	PBD::Property<bool>       _use_follow;
	FollowAction              _follow_action[2];
	PBD::Property<int>        _follow_action_probability;
	uint32_t                  _loop_cnt; /* how many times in a row has this played */
	PBD::Property<int>        _follow_count;
	Temporal::BBT_Offset      _quantization;
	PBD::Property<bool>       _legato;
	std::string               _name;
	double                    _barcnt; /* our estimate of the number of bars in the region */
	double                    _apparent_tempo;
	gain_t                    _gain;
	gain_t                    _pending_gain;
	PBD::Property<float>      _midi_velocity_effect;
	void*                     _ui;
	samplepos_t                expected_end_sample;
	PBD::Property<bool>       _stretchable;
	bool                      _explicitly_stopped;

	void set_region_internal (boost::shared_ptr<Region>);
	virtual void retrigger() = 0;
	virtual void set_usable_length () = 0;
};

class LIBARDOUR_API AudioTrigger : public Trigger {
  public:
	AudioTrigger (uint64_t index, TriggerBox&);
	~AudioTrigger ();

	pframes_t run (BufferSet&, samplepos_t start_sample, samplepos_t end_sample, Temporal::Beats const & start, Temporal::Beats const & end, pframes_t nframes, pframes_t offset, bool first, double bpm);

	void set_start (timepos_t const &);
	void set_end (timepos_t const &);
	void set_legato_offset (timepos_t const &);
	timepos_t current_pos() const;
	void set_length (timecnt_t const &);
	timepos_t start_offset () const { return timepos_t (_start_offset); } /* offset from start of data */
	timepos_t current_length() const; /* offset from start of data */
	timepos_t natural_length() const; /* offset from start of data */
	void reload (BufferSet&, void*);

	double position_as_fraction() const;

	int set_region_threaded (boost::shared_ptr<Region>);
	void startup ();
	void jump_start ();
	void jump_stop ();

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	RubberBand::RubberBandStretcher* stretcher() { return (_stretcher); }

	SegmentDescriptor get_segment_descriptor () const;
	void set_expected_end_sample (Temporal::TempoMap::SharedPtr const &, Temporal::BBT_Time const &);

	void set_stretchable (bool yn);
	bool stretchable () const { return _stretchable; }

	bool stretching () const;

  protected:
	void retrigger ();
	void set_usable_length ();

  private:
	PBD::ID     data_source;
	std::vector<Sample*> data;
	samplecnt_t read_index;
	samplecnt_t process_index;
	samplecnt_t data_length;
	samplepos_t _start_offset;
	samplepos_t _legato_offset;
	samplecnt_t usable_length;
	samplepos_t last_sample;
	samplecnt_t retrieved;
	RubberBand::RubberBandStretcher*  _stretcher;
	samplecnt_t got_stretcher_padding;
	samplecnt_t to_pad;
	samplecnt_t to_drop;

	void drop_data ();
	int load_data (boost::shared_ptr<AudioRegion>);
	void determine_tempo ();
	void setup_stretcher ();
};


class LIBARDOUR_API MIDITrigger : public Trigger {
  public:
	MIDITrigger (uint64_t index, TriggerBox&);
	~MIDITrigger ();

	pframes_t run (BufferSet&, samplepos_t start_sample, samplepos_t end_sample, Temporal::Beats const & start_beats, Temporal::Beats const & end_beats, pframes_t nframes, pframes_t offset, bool passthru, double bpm);

	void set_start (timepos_t const &);
	void set_end (timepos_t const &);
	void set_legato_offset (timepos_t const &);
	timepos_t current_pos() const;
	void set_length (timecnt_t const &);
	timepos_t start_offset () const;
	timepos_t end() const;            /* offset from start of data */
	timepos_t current_length() const; /* offset from start of data */
	timepos_t natural_length() const; /* offset from start of data */
	void reload (BufferSet&, void*);

	double position_as_fraction() const;

	int set_region_threaded (boost::shared_ptr<Region>);
	void startup ();
	void jump_start ();
	void jump_stop ();

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	SegmentDescriptor get_segment_descriptor () const;
	void set_expected_end_sample (Temporal::TempoMap::SharedPtr const &, Temporal::BBT_Time const &);

  protected:
	void retrigger ();
	void set_usable_length ();

  private:
	PBD::ID data_source;
	MidiStateTracker tracker;
	PBD::ScopedConnection content_connection;

	Temporal::DoubleableBeats data_length;   /* using timestamps from data */
	Temporal::DoubleableBeats usable_length; /* using timestamps from data */
	Temporal::DoubleableBeats last_event_beats;

	Temporal::BBT_Offset _start_offset;
	Temporal::BBT_Offset _legato_offset;

	MidiModel::const_iterator iter;
	boost::shared_ptr<MidiModel> model;

	int load_data (boost::shared_ptr<MidiRegion>);
	void compute_and_set_length ();
};


class LIBARDOUR_API TriggerBoxThread
{
  public:
	TriggerBoxThread ();
	~TriggerBoxThread();

	static void init_request_pool() { Request::init_pool(); }

	void set_region (Trigger*, boost::shared_ptr<Region>);

	void summon();
	void stop();
	void wait_until_finished();

  private:
	static void* _thread_work(void *arg);
	void*         thread_work();

	enum RequestType {
		Quit,
		SetRegion
	};

	struct Request {

		Request (RequestType t) : type (t) {}

		RequestType type;
		/* for set region */
		Trigger* trig; /* XXX lifetime mgmt issues */
		boost::shared_ptr<Region> region;

		void* operator new (size_t);
		void  operator delete (void* ptr, size_t);

		static MultiAllocSingleReleasePool* pool;
		static void init_pool ();
	};

	pthread_t thread;
	PBD::RingBuffer<Request*>  requests;

	CrossThreadChannel _xthread;
	void queue_request (Request*);
};


class LIBARDOUR_API TriggerBox : public Processor
{
  public:
	TriggerBox (Session&, DataType dt);
	~TriggerBox ();

	void run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool result_required);
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);

	int32_t order() const { return _order; }
	void set_order(int32_t n);

	typedef std::vector<Trigger*> Triggers;

	Trigger* trigger (Triggers::size_type);

	bool bang_trigger (Trigger*);
	bool unbang_trigger (Trigger*);
	void add_trigger (Trigger*);

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	void set_from_path (uint64_t slot, std::string const & path);
	void set_from_selection (uint64_t slot, boost::shared_ptr<Region>);

	DataType data_type() const { return _data_type; }

	void request_stop_all ();

	Trigger* currently_playing() const { return _currently_playing; }

	/* Returns a negative value is there is no active Trigger, or a value between 0
	 * and 1.0 if there is, corresponding to the value of position_as_fraction() for
	 *  the active Trigger.
	 */
	double position_as_fraction() const;

	void queue_explict (Trigger*);
	Trigger* get_next_trigger ();
	Trigger* peek_next_trigger ();

	void add_midi_sidechain (std::string const & name);

	bool pass_thru() const { return _pass_thru; }
	void set_pass_thru (bool yn);

	void request_reload (int32_t slot, void*);
	void request_use (int32_t slot, Trigger&);

	enum TriggerMidiMapMode {
		AbletonPush,
		SequentialNote,
		ByMidiChannel
	};

	static Temporal::BBT_Offset assumed_trigger_duration () { return _assumed_trigger_duration; }
	static void set_assumed_trigger_duration (Temporal::BBT_Offset const &);

	static TriggerMidiMapMode midi_map_mode () { return _midi_map_mode; }
	static void set_midi_map_mode (TriggerMidiMapMode m);

	static int first_midi_note() { return _first_midi_note; }
	static void set_first_midi_note (int n);

	static void maybe_find_scene_bang ();
	static void clear_scene_bang ();
	static void scene_bang (uint32_t scene_number);
	static void scene_unbang (uint32_t scene_number);

	static void init ();

	static const int32_t default_triggers_per_box;

	static TriggerBoxThread* worker;

	static void start_transport_stop (Session&);

  private:
	struct Requests {
		std::atomic<bool> stop_all;
		std::atomic<bool> pass_thru;

		Requests () : stop_all (false), pass_thru (true) {}
	};

	static Temporal::BBT_Offset _assumed_trigger_duration;

	PBD::RingBuffer<Trigger*> _bang_queue;
	PBD::RingBuffer<Trigger*> _unbang_queue;
	DataType _data_type;
	int32_t _order;
	Glib::Threads::RWLock trigger_lock; /* protects all_triggers */
	Triggers all_triggers;
	PBD::RingBuffer<Trigger*> explicit_queue; /* user queued triggers */
	Trigger* _currently_playing;
	Requests _requests;
	bool _stop_all;
	bool _pass_thru;

	boost::shared_ptr<SideChain> _sidechain;

	PBD::PCGRand _pcg;

	/* These four are accessed (read/write) only from process() context */

	void drop_triggers ();
	void process_ui_trigger_requests ();
	void process_midi_trigger_requests (BufferSet&);
	int determine_next_trigger (uint64_t n);
	void stop_all ();

	int note_to_trigger (int node, int channel);

	void note_on (int note_number, int velocity);
	void note_off (int note_number, int velocity);

	void reconnect_to_default ();
	void parameter_changed (std::string const &);

	static int _first_midi_note;
	static TriggerMidiMapMode _midi_map_mode;
	static std::atomic<int32_t> _pending_scene;
	static std::atomic<int32_t> _active_scene;

	struct Request {
		enum Type {
			Use,
			Reload,
		};

		Type type;

		union {
			Trigger* trigger;
			void* ptr;
		};

		union {
			int32_t slot;
		};

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
	static PBD::Signal0<void> StopAllTriggers;
};

namespace Properties {
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> use_follow;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> running;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> passthru;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> legato;
	LIBARDOUR_API extern PBD::PropertyDescriptor<Temporal::BBT_Offset> quantization;
	LIBARDOUR_API extern PBD::PropertyDescriptor<Trigger::LaunchStyle> launch_style;
	LIBARDOUR_API extern PBD::PropertyDescriptor<Trigger::FollowAction> follow_action0;
	LIBARDOUR_API extern PBD::PropertyDescriptor<Trigger::FollowAction> follow_action1;
	LIBARDOUR_API extern PBD::PropertyDescriptor<int> follow_count;
	LIBARDOUR_API extern PBD::PropertyDescriptor<int> follow_action_probability;
	LIBARDOUR_API extern PBD::PropertyDescriptor<float> velocity_effect;
	LIBARDOUR_API extern PBD::PropertyDescriptor<gain_t> gain;
	LIBARDOUR_API extern PBD::PropertyDescriptor<Trigger*> currently_playing;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> stretchable;
}


} // namespace ARDOUR

namespace PBD {
DEFINE_ENUM_CONVERT(ARDOUR::Trigger::FollowAction);
DEFINE_ENUM_CONVERT(ARDOUR::Trigger::LaunchStyle);
} /* namespace PBD */


#endif /* __ardour_triggerbox_h__ */
