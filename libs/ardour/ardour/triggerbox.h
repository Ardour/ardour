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

#include <atomic>
#include <map>
#include <vector>
#include <string>
#include <exception>

#include <glibmm/threads.h>

#include "pbd/pcg_rand.h"
#include "pbd/properties.h"
#include "pbd/ringbuffer.h"
#include "pbd/stateful.h"

#include "temporal/beats.h"
#include "temporal/bbt_time.h"
#include "temporal/tempo.h"

#include "ardour/midi_state_tracker.h"
#include "ardour/processor.h"
#include "ardour/rt_midibuffer.h"

#include "ardour/libardour_visibility.h"

class XMLNode;


namespace ARDOUR {

class Session;
class AudioRegion;
class MidiRegion;
class TriggerBox;
class SideChain;

class LIBARDOUR_API Trigger : public PBD::Stateful {
  public:
	enum State {
		None = 0, /* mostly for _requested_state */
		Stopped = 1,
		WaitingToStart = 2,
		Running = 3,
		WaitingForRetrigger = 4,
		WaitingToStop = 5,
		Stopping = 6
	};

	Trigger (uint64_t index, TriggerBox&);
	virtual ~Trigger() {}

	static void make_property_quarks ();

	void set_name (std::string const &);
	std::string name() const { return _name; }

	/* semantics of "bang" depend on the trigger */
	void bang ();
	void unbang ();
	/* explicitly call for the trigger to stop */
	virtual void stop (int next_to_run);

	virtual void set_start (timepos_t const &) = 0;
	virtual void set_end (timepos_t const &) = 0;
	virtual void set_length (timecnt_t const &) = 0;
	virtual void tempo_map_change () = 0;

	virtual double position_as_fraction() const = 0;

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

	virtual int set_region (boost::shared_ptr<Region>) = 0;
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

	enum RunResult {
		Relax = 0,
		RemoveTrigger = 0x1,
		ReadMore = 0x2,
		FillSilence = 0x4,
		ChangeTriggers = 0x8
	};

	enum RunType {
		RunEnd,
		RunStart,
		RunAll,
		RunNone,
	};

	RunType maybe_compute_next_transition (Temporal::Beats const & start, Temporal::Beats const & end);

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
	virtual void start_stop ();

	uint32_t follow_count() const { return _follow_count; }
	void set_follow_count (uint32_t n);

	void set_ui (void*);
	void* ui () const { return _ui; }

	TriggerBox const & box() const { return _box; }

	gain_t gain() const { return _pending_gain; }
	void set_gain (gain_t);

	float midi_velocity_effect() const { return _midi_velocity_effect; }
	void set_midi_velocity_effect (float);

  protected:
	TriggerBox& _box;
	State _state;
	std::atomic<State> _requested_state;
	std::atomic<int> _bang;
	std::atomic<int> _unbang;
	uint64_t _index;
	int    _next_trigger;
	LaunchStyle  _launch_style;
	PBD::Property<bool>  _use_follow;
	FollowAction _follow_action[2];
	int _follow_action_probability;
	uint32_t _loop_cnt;
	uint32_t _follow_count;
	boost::shared_ptr<Region> _region;
	Temporal::BBT_Offset _quantization;
	PBD::Property<bool> _legato;
	std::string _name;
	double _stretch;
	double _barcnt; /* our estimate of the number of bars in the region */
	void* _ui;
	gain_t _gain;
	gain_t _pending_gain;
	float _midi_velocity_effect;

	void set_region_internal (boost::shared_ptr<Region>);
	void request_state (State s);
	virtual void retrigger() = 0;
	virtual void set_usable_length () = 0;
};

class LIBARDOUR_API AudioTrigger : public Trigger {
  public:
	AudioTrigger (uint64_t index, TriggerBox&);
	~AudioTrigger ();

	int run (BufferSet&, pframes_t nframes, pframes_t offset, bool first);

	void set_start (timepos_t const &);
	void set_end (timepos_t const &);
	void set_legato_offset (timepos_t const &);
	timepos_t current_pos() const;
	void set_length (timecnt_t const &);
	timepos_t start_offset () const { return timepos_t (_start_offset); } /* offset from start of data */
	timepos_t current_length() const; /* offset from start of data */
	timepos_t natural_length() const; /* offset from start of data */

	double position_as_fraction() const;

	int set_region (boost::shared_ptr<Region>);
	void startup ();
	void jump_start ();
	void jump_stop ();

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	void tempo_map_change ();

  protected:
	void retrigger ();
	void set_usable_length ();

  private:
	PBD::ID data_source;
	std::vector<Sample*> data;
	samplecnt_t read_index;
	samplecnt_t data_length;
	samplepos_t _start_offset;
	samplepos_t _legato_offset;
	samplecnt_t usable_length;
	samplepos_t last_sample;

	void drop_data ();
	int load_data (boost::shared_ptr<AudioRegion>);
	RunResult at_end ();
	void compute_and_set_length ();
};


class LIBARDOUR_API MIDITrigger : public Trigger {
  public:
	MIDITrigger (uint64_t index, TriggerBox&);
	~MIDITrigger ();

	int run (BufferSet&, samplepos_t start_sample, samplepos_t end_sample, pframes_t nframes, pframes_t offset, bool first);

	void set_start (timepos_t const &);
	void set_end (timepos_t const &);
	void set_legato_offset (timepos_t const &);
	timepos_t current_pos() const;
	void set_length (timecnt_t const &);
	timepos_t start_offset () const;
	timepos_t end() const;            /* offset from start of data */
	timepos_t current_length() const; /* offset from start of data */
	timepos_t natural_length() const; /* offset from start of data */

	double position_as_fraction() const;

	int set_region (boost::shared_ptr<Region>);
	void startup ();
	void jump_start ();
	void jump_stop ();

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	void tempo_map_change ();

  protected:
	void retrigger ();
	void set_usable_length ();

  private:
	PBD::ID data_source;
	RTMidiBuffer data;
	MidiStateTracker tracker;

	size_t read_index;          /* index into data */
	samplecnt_t data_length;   /* using timestamps from data */
	samplecnt_t usable_length; /* using timestamps from data */

	Temporal::BBT_Offset _start_offset;
	Temporal::BBT_Offset _legato_offset;

	void drop_data ();
	int load_data (boost::shared_ptr<MidiRegion>);
	RunResult at_end ();
	void compute_and_set_length ();
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

	int set_from_path (uint64_t slot, std::string const & path);
	int set_from_selection (uint64_t slot, boost::shared_ptr<Region>);

	DataType data_type() const { return _data_type; }

	void request_stop_all ();

	/* only valid when called by Triggers from within ::process_state_requests() */
	bool currently_running() const { return currently_playing; }
	void set_next (uint64_t which);

	void queue_explict (Trigger*);
	void queue_implicit (Trigger*);
	void clear_implicit ();
	Trigger* get_next_trigger ();
	Trigger* peek_next_trigger ();
	void prepare_next (uint64_t current);

	void add_midi_sidechain (std::string const & name);

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

  private:
	static Temporal::BBT_Offset _assumed_trigger_duration;

	PBD::RingBuffer<Trigger*> _bang_queue;
	PBD::RingBuffer<Trigger*> _unbang_queue;
	DataType _data_type;
	int32_t _order;
	Glib::Threads::RWLock trigger_lock; /* protects all_triggers */
	Triggers all_triggers;
	PBD::RingBuffer<Trigger*> explicit_queue; /* user queued triggers */
	Trigger* up_next;
	Trigger* currently_playing;
	std::atomic<bool> _stop_all;

	boost::shared_ptr<SideChain> _sidechain;

	PBD::PCGRand _pcg;

	/* These four are accessed (read/write) only from process() context */

	void drop_triggers ();
	void process_ui_trigger_requests ();
	void process_midi_trigger_requests (BufferSet&);
	int determine_next_trigger (uint64_t n);
	void stop_all ();

	int note_to_trigger (int node, int channel);

	void tempo_map_change ();
	PBD::ScopedConnection tempo_map_connection;

	void note_on (int note_number, int velocity);
	void note_off (int note_number, int velocity);

	void reconnect_to_default ();
	void parameter_changed (std::string const &);

	static const uint64_t default_triggers_per_box;
	static int _first_midi_note;
	static TriggerMidiMapMode _midi_map_mode;
	static std::atomic<int32_t> _pending_scene;
	static std::atomic<int32_t> _active_scene;
};

namespace Properties {
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> use_follow;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> running;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> legato;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> quantization;
	LIBARDOUR_API extern PBD::PropertyDescriptor<Trigger::LaunchStyle> launch_style;
	LIBARDOUR_API extern PBD::PropertyDescriptor<Trigger::FollowAction> follow_action0;
	LIBARDOUR_API extern PBD::PropertyDescriptor<Trigger::FollowAction> follow_action1;
}


} // namespace ARDOUR

namespace PBD {
DEFINE_ENUM_CONVERT(ARDOUR::Trigger::FollowAction);
DEFINE_ENUM_CONVERT(ARDOUR::Trigger::LaunchStyle);
} /* namespace PBD */


#endif /* __ardour_triggerbox_h__ */
