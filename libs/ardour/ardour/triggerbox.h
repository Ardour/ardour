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
	Trigger (boost::shared_ptr<Region>);
	virtual ~Trigger() {}

	virtual void bang (TriggerBox&, Temporal::Beats const &, samplepos_t) = 0;
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

  protected:
	bool _running;
	bool _stop_requested;
	LaunchStyle  _launch_style;
	FollowAction _follow_action;
	boost::shared_ptr<Region> _region;

	void set_region_internal (boost::shared_ptr<Region>);
};

class LIBARDOUR_API AudioTrigger : public Trigger {
  public:
	AudioTrigger (boost::shared_ptr<AudioRegion>);
	~AudioTrigger ();

	void bang (TriggerBox&, Temporal::Beats const & , samplepos_t);
	void unbang (TriggerBox&, Temporal::Beats const & , samplepos_t);

	Sample* run (uint32_t channel, pframes_t& nframes, samplepos_t start_frame, samplepos_t end_frame, bool& need_butler);

	int set_region (boost::shared_ptr<Region>);

  private:
	std::vector<Sample*> data;
	std::vector<samplecnt_t> read_index;
	samplecnt_t length;

	void drop_data ();
	int load_data (boost::shared_ptr<AudioRegion>);
};

class LIBARDOUR_API TriggerBox : public Processor
{
  public:
	TriggerBox (Session&);
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

  private:
	PBD::RingBuffer<Trigger*> _trigger_queue;

	Triggers active_triggers;
	Glib::Threads::RWLock trigger_lock;
	Triggers all_triggers;

	void drop_triggers ();
	void process_trigger_requests (Temporal::Beats const &, samplepos_t);

	void note_on (int note_number, int velocity);
	void note_off (int note_number, int velocity);

	typedef std::map<uint8_t,Triggers::size_type> MidiTriggerMap;
	MidiTriggerMap midi_trigger_map;
};

} // namespace ARDOUR

#endif /* __ardour_triggerbox_h__ */
