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

#ifndef __ardour_butler_h__
#define __ardour_butler_h__

#include <pthread.h>

#include <glibmm/threads.h>

#include "pbd/crossthread.h"
#include "pbd/ringbuffer.h"
#include "pbd/pool.h"
#include "pbd/g_atomic_compat.h"

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/session_handle.h"

namespace ARDOUR {

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
	~Butler();

	int  start_thread();
	void terminate_thread();
	void schedule_transport_work();
	void summon();
	void stop();
	void wait_until_finished();
	bool transport_work_requested() const;
	void drop_references ();

        void map_parameters ();

	samplecnt_t audio_capture_buffer_size() const { return _audio_capture_buffer_size; }
	samplecnt_t audio_playback_buffer_size() const { return _audio_playback_buffer_size; }
	uint32_t midi_buffer_size()  const { return _midi_buffer_size; }

	static void* _thread_work(void *arg);
	void*         thread_work();

	struct Request {
		enum Type {
			Run,
			Pause,
			Quit
		};
	};

	pthread_t thread;
	bool      have_thread;

	Glib::Threads::Mutex      request_lock;
	Glib::Threads::Cond       paused;
	bool                      should_run;
	mutable GATOMIC_QUAL gint should_do_transport_work;

	samplecnt_t _audio_capture_buffer_size;
	samplecnt_t _audio_playback_buffer_size;
	uint32_t    _midi_buffer_size;

	PBD::RingBuffer<CrossThreadPool*> pool_trash;

private:
	void empty_pool_trash ();
	void config_changed (std::string);

	bool flush_tracks_to_disk_normal (boost::shared_ptr<RouteList>, uint32_t& errors);

	/**
	 * Add request to butler thread request queue
	 */
	void queue_request (Request::Type r);

	CrossThreadChannel _xthread;

};

} // namespace ARDOUR

#endif // __ardour_butler_h__
