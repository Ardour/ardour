/*
 * Copyright (C) 2023 Paul Davis <paul@linuxaudiosystems.com>
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

#include <atomic>

#ifndef __ardour_cliprec_h__
#define __ardour_cliprec_h__


#include "pbd/ringbufferNPT.h"
#include "pbd/signals.h"

#include "temporal/timeline.h"

#include "ardour/disk_io.h"
#include "ardour/rt_midibuffer.h"

namespace PBD {
class Thread;
class Semaphore;
}

namespace ARDOUR {

class AudioFileSource;
class Session;
class Track;

template<typename T> class MidiRingBuffer;

class LIBARDOUR_API ClipRecProcessor : public DiskIOProcessor
{
  public:
	ClipRecProcessor (Session&, Track&, std::string const & name, Temporal::TimeDomainProvider const &);
	void run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool result_required);
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);

	float buffer_load () const;
	void adjust_buffering ();
	void configuration_changed ();

	struct ArmInfo {
		Temporal::timepos_t start;
		Temporal::timepos_t end;
	};

	void set_armed (ArmInfo*);
	bool armed() const { return (bool) _arm_info.load(); }
	PBD::Signal0<void> ArmedChanged;

  private:
	std::atomic<ArmInfo*> _arm_info;
	std::shared_ptr<RTMidiBuffer> rt_midibuffer;

	/* private (to class) butler thread */

	static PBD::Thread* _thread;
	static PBD::Semaphore* _semaphore;
	static bool thread_should_run;
	static void thread_work ();
	static ClipRecProcessor* currently_recording;

	int pull_data ();
	void start_recording ();
	void finish_recording ();
};

} /* namespace */

#endif /* __ardour_cliprec_h__ */
