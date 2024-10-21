/*
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#pragma once

#include <atomic>

#include <pthread.h>

#include <glibmm/threads.h>

#include "pbd/crossthread.h"
#include "pbd/pool.h"
#include "pbd/ringbuffer.h"
#include "pbd/mpmc_queue.h"

#include "ardour/libardour_visibility.h"
#include "ardour/session_handle.h"
#include "ardour/types.h"

namespace ARDOUR
{
/**
 *  One of the Butler's functions is to clean up (ie delete) unused CrossThreadPools.
 *  When a thread with a CrossThreadPool terminates, its CTP is added to pool_trash.
 *  When the Butler thread wakes up, we check this trash buffer for CTPs, and if they
 *  are empty they are deleted.
 */

class LIBARDOUR_API Butler : public SessionHandleRef
{
public:
	Butler (Session& session);
	~Butler ();

	int  start_thread ();
	void terminate_thread ();
	void schedule_transport_work ();
	void summon ();
	void stop ();
	void wait_until_finished ();
	bool transport_work_requested () const;
	void drop_references ();

	void map_parameters ();

	bool delegate (sigc::slot<void> const& work) {
		bool rv = _delegated_work.push_back (work);
		summon ();
		return rv;
	}
	samplecnt_t audio_capture_buffer_size () const
	{
		return _audio_capture_buffer_size;
	}
	samplecnt_t audio_playback_buffer_size () const
	{
		return _audio_playback_buffer_size;
	}
	uint32_t midi_buffer_size () const
	{
		return _midi_buffer_size;
	}

	mutable std::atomic<int> should_do_transport_work;

private:
	struct Request {
		enum Type {
			Run,
			Pause,
			Quit
		};
	};


	static void* _thread_work (void* arg);

	void* thread_work ();

	void empty_pool_trash ();
	void process_delegated_work ();
	void config_changed (std::string);
	bool flush_tracks_to_disk_normal (std::shared_ptr<RouteList const>, uint32_t& errors);
	void queue_request (Request::Type r);

	pthread_t thread;
	bool      have_thread;

	Glib::Threads::Mutex request_lock;
	Glib::Threads::Cond  paused;
	bool                 should_run;

	samplecnt_t _audio_capture_buffer_size;
	samplecnt_t _audio_playback_buffer_size;
	uint32_t    _midi_buffer_size;

	PBD::RingBuffer<PBD::CrossThreadPool*> pool_trash;
	CrossThreadChannel                    _xthread;
	PBD::MPMCQueue<sigc::slot<void> >     _delegated_work;
};

} // namespace ARDOUR

