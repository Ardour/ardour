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

#include "pbd/stateful.h"
#include "pbd/ringbuffer.h"

#include "temporal/beats.h"
#include "temporal/bbt_time.h"
#include "temporal/tempo.h"

#include "ardour/processor.h"
#include "ardour/libardour_visibility.h"

class XMLNode;

namespace ARDOUR {
	namespace Properties {
		LIBARDOUR_API extern PBD::PropertyDescriptor<bool> running;
	}
}

namespace ARDOUR {

class Session;
class AudioRegion;
class TriggerBox;

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

	Trigger (size_t index, TriggerBox&);
	virtual ~Trigger() {}

	static void make_property_quarks ();

	/* semantics of "bang" depend on the trigger */
	void bang ();
	void unbang ();
	/* explicitly call for the trigger to stop */
	virtual void stop();
	/* explicitly call for the trigger to start */
	virtual void start();

	void process_state_requests ();

	bool active() const { return _state >= Running; }
	State state() const { return _state; }

	enum LaunchStyle {
		Loop,  /* runs till stopped, reclick just restarts */
		Gate,     /* runs till mouse up/note off then to next quantization */
		Toggle,   /* runs till "clicked" again */
		Repeat,   /* plays only quantization extent until mouse up/note off */
	};

	LaunchStyle launch_style() const { return _launch_style; }
	void set_launch_style (LaunchStyle);

	enum FollowAction {
		Stop,
		QueuedTrigger, /* DP-style */
		NextTrigger,   /* Live-style, and below */
		PrevTrigger,
		FirstTrigger,
		LastTrigger,
		AnyTrigger,
		OtherTrigger,
	};

	FollowAction follow_action() const { return _follow_action; }
	void set_follow_action (FollowAction);

	virtual int set_region (boost::shared_ptr<Region>) = 0;
	boost::shared_ptr<Region> region() const { return _region; }

	Temporal::BBT_Offset quantization() const;
	void set_quantization (Temporal::BBT_Offset const &);

	virtual void set_length (timecnt_t const &) = 0;
	virtual timecnt_t current_length() const = 0;
	virtual timecnt_t natural_length() const = 0;

	size_t index() const { return _index; }

	/* Managed by TriggerBox */
	samplepos_t bang_samples;
	Temporal::Beats bang_beats;

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

  protected:
	TriggerBox& _box;
	State _state;
	std::atomic<State> _requested_state;
	std::atomic<int> _bang;
	std::atomic<int> _unbang;
	size_t _index;
	int    _next_trigger;
	LaunchStyle  _launch_style;
	FollowAction _follow_action;
	boost::shared_ptr<Region> _region;
	Temporal::BBT_Offset _quantization;

	void set_region_internal (boost::shared_ptr<Region>);
	void request_state (State s);
	virtual void retrigger() = 0;
};

class LIBARDOUR_API AudioTrigger : public Trigger {
  public:
	AudioTrigger (size_t index, TriggerBox&);
	~AudioTrigger ();

	int run (BufferSet&, pframes_t nframes, pframes_t offset, bool first);

	void set_length (timecnt_t const &);
	timecnt_t current_length() const;
	timecnt_t natural_length() const;

	int set_region (boost::shared_ptr<Region>);

  protected:
	void retrigger ();

  private:
	std::vector<Sample*> data;
	samplecnt_t read_index;
	samplecnt_t data_length;

	void drop_data ();
	int load_data (boost::shared_ptr<AudioRegion>);
	RunResult at_end ();
};

class LIBARDOUR_API TriggerBox : public Processor
{
  public:
	TriggerBox (Session&, DataType dt);
	~TriggerBox ();

	void run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool result_required);
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);

	typedef std::vector<Trigger*> Triggers;

	Trigger* trigger (Triggers::size_type);

	bool bang_trigger (Trigger*);
	bool unbang_trigger (Trigger*);
	void add_trigger (Trigger*);

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	int set_from_path (size_t slot, std::string const & path);

	DataType data_type() const { return _data_type; }

	void stop_all ();

  private:
	PBD::RingBuffer<Trigger*> _bang_queue;
	PBD::RingBuffer<Trigger*> _unbang_queue;
	DataType _data_type;

	Glib::Threads::RWLock trigger_lock; /* protects all_triggers */
	Triggers all_triggers;

	/* These three are accessed (read/write) only from process() context */

	void drop_triggers ();
	void process_ui_trigger_requests ();
	void process_midi_trigger_requests (BufferSet&);
	void set_next_trigger (size_t n);

	void note_on (int note_number, int velocity);
	void note_off (int note_number, int velocity);

	typedef std::map<uint8_t,Triggers::size_type> MidiTriggerMap;
	MidiTriggerMap midi_trigger_map;
};

} // namespace ARDOUR

#endif /* __ardour_triggerbox_h__ */
