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
	Trigger() {}
	virtual ~Trigger() {}

	virtual void bang (TriggerBox&, Temporal::Beats const &, samplepos_t) = 0;
};

class LIBARDOUR_API AudioTrigger : public Trigger {
  public:
	AudioTrigger (boost::shared_ptr<AudioRegion>);
	~AudioTrigger ();

	void bang (TriggerBox&, Temporal::Beats const & , samplepos_t);
	Sample* run (uint32_t channel, pframes_t& nframes, samplepos_t start_frame, samplepos_t end_frame, bool& need_butler);

  private:
	boost::shared_ptr<AudioRegion> region;
	bool running;
	std::vector<Sample*> data;
	samplecnt_t read_index;
	samplecnt_t length;
};

class LIBARDOUR_API TriggerBox : public Processor
{
  public:
	TriggerBox (Session&);
	~TriggerBox ();

	void run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool result_required);
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);

	bool queue_trigger (Trigger*);
	void add_trigger (Trigger*);

  private:
	PBD::RingBuffer<Trigger*> _trigger_queue;

	typedef std::vector<Trigger*> Triggers;
	Triggers active_triggers;
	Glib::Threads::Mutex trigger_lock;
	Triggers all_triggers;

	void note_on (int note_number, int velocity);
	void note_off (int note_number, int velocity);

	/* XXX for initial testing only */

	boost::shared_ptr<Source> the_source;
	boost::shared_ptr<AudioRegion> the_region;
	AudioTrigger* the_trigger;
};

} // namespace ARDOUR

#endif /* __ardour_triggerbox_h__ */
