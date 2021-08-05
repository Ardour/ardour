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

#include <map>
#include <vector>
#include <string>
#include <exception>

#include <glibmm/threads.h>

#include "pbd/ringbuffer.h"

#include "temporal/beats.h"

#include "ardour/processor.h"
#include "ardour/libardour_visibility.h"

class XMLNode;

namespace ARDOUR {

class Session;
class AudioRegion;
class TriggerBox;

class LIBARDOUR_API Trigger {
  public:
	Trigger (size_t index);
	virtual ~Trigger() {}

	virtual void bang (TriggerBox&) = 0;
	virtual void unbang (TriggerBox&, Temporal::Beats const &, samplepos_t) = 0;

	bool running() const { return _running; }

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

	Temporal::Beats quantization() const;
	void set_quantization (Temporal::Beats const &);

	bool stop_requested() const { return _stop_requested; }
	virtual void stop();

	size_t index() const { return _index; }

	/* Managed by TriggerBox */
	samplepos_t fire_samples;
	Temporal::Beats fire_beats;

  protected:
	bool _running;
	bool _stop_requested;
	size_t _index;
	LaunchStyle  _launch_style;
	FollowAction _follow_action;
	boost::shared_ptr<Region> _region;
	Temporal::Beats _quantization;

	void set_region_internal (boost::shared_ptr<Region>);
};

class LIBARDOUR_API AudioTrigger : public Trigger {
  public:
	AudioTrigger (size_t index);
	~AudioTrigger ();

	void bang (TriggerBox&);
	void unbang (TriggerBox&, Temporal::Beats const & , samplepos_t);

	Sample* run (uint32_t channel, pframes_t& nframes, bool& need_butler);

	int set_region (boost::shared_ptr<Region>);

  private:
	std::vector<Sample*> data;
	std::vector<samplecnt_t> read_index;
	samplecnt_t length;

	void drop_data ();
	int load_data (boost::shared_ptr<AudioRegion>);
	void retrigger ();
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

	bool queue_trigger (Trigger*);
	void add_trigger (Trigger*);

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	int set_from_path (size_t slot, std::string const & path);

	DataType data_type() const { return _data_type; }

  private:
	PBD::RingBuffer<Trigger*> _trigger_queue;
	DataType _data_type;

	Glib::Threads::RWLock trigger_lock; /* protects all_triggers */
	Triggers all_triggers;

	/* These three are accessed (read/write) only from process() context */
	Triggers pending_on_triggers;
	Triggers pending_off_triggers;
	Triggers active_triggers;

	void drop_triggers ();
	void process_ui_trigger_requests ();
	void process_midi_trigger_requests (BufferSet&);

	void note_on (int note_number, int velocity);
	void note_off (int note_number, int velocity);

	typedef std::map<uint8_t,Triggers::size_type> MidiTriggerMap;
	MidiTriggerMap midi_trigger_map;

	void load_some_samples ();
};

} // namespace ARDOUR

#endif /* __ardour_triggerbox_h__ */
