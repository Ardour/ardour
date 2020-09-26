/*
 * Copyright (C) 2017 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#include <cmath>
#include "ardour/lmath.h"

#include "pbd/cpus.h"
#include "pbd/pthread_utils.h"

#include "ardour/audioregion.h"
#include "ardour/audiosource.h"

#include "waveview/wave_view_private.h"

namespace ArdourWaveView {

WaveViewProperties::WaveViewProperties (boost::shared_ptr<ARDOUR::AudioRegion> region)
    : region_start (region->start_sample ())
    , region_end (region->start_sample () + region->length_samples ())
    , channel (0)
    , height (64)
    , samples_per_pixel (0)
    , amplitude (region->scale_amplitude ())
    , amplitude_above_axis (1.0)
    , fill_color (0x000000ff)
    , outline_color (0xff0000ff)
    , zero_color (0xff0000ff)
    , clip_color (0xff0000ff)
    , show_zero (false)
    , logscaled (WaveView::global_logscaled())
    , shape (WaveView::global_shape())
    , gradient_depth (WaveView::global_gradient_depth ())
    , start_shift (0.0) // currently unused
    , sample_start (0)
    , sample_end (0)
{

}

/*-------------------------------------------------*/

WaveViewImage::WaveViewImage (boost::shared_ptr<const ARDOUR::AudioRegion> const& region_ptr,
                              WaveViewProperties const& properties)
	: region (region_ptr)
	, props (properties)
	, timestamp (0)
{

}

WaveViewImage::~WaveViewImage ()
{

}

/*-------------------------------------------------*/

WaveViewCacheGroup::WaveViewCacheGroup (WaveViewCache& parent_cache)
	: _parent_cache (parent_cache)
{

}

WaveViewCacheGroup::~WaveViewCacheGroup ()
{
	clear_cache ();
}

void
WaveViewCacheGroup::add_image (boost::shared_ptr<WaveViewImage> image)
{
	if (!image) {
		// Not adding invalid image to cache
		return;
	}

	ImageCache::iterator oldest_image_it = _cached_images.begin();
	ImageCache::iterator second_oldest_image_it = _cached_images.end();

	for (ImageCache::iterator it = _cached_images.begin (); it != _cached_images.end (); ++it) {
		if ((*it) == image) {
			// Must never be more than one instance of the image in the cache
			(*it)->timestamp = g_get_monotonic_time ();
			return;
		} else if ((*it)->props.is_equivalent (image->props)) {
			// Equivalent Image already in cache, updating timestamp
			(*it)->timestamp = g_get_monotonic_time ();
			return;
		}

		if ((*it)->timestamp < (*oldest_image_it)->timestamp) {
			second_oldest_image_it = oldest_image_it;
			oldest_image_it = it;
		}
	}

	// no duplicate or equivalent image so we are definitely adding it to cache
	image->timestamp = g_get_monotonic_time ();

	if (_parent_cache.full () || full ()) {
		if (oldest_image_it != _cached_images.end()) {
			// Replacing oldest Image in cache
			_parent_cache.decrease_size ((*oldest_image_it)->size_in_bytes ());
			*oldest_image_it = image;
			_parent_cache.increase_size (image->size_in_bytes ());

			if (second_oldest_image_it != _cached_images.end ()) {
				// Removing second oldest Image in cache
				_parent_cache.decrease_size ((*second_oldest_image_it)->size_in_bytes ());
				_cached_images.erase (second_oldest_image_it);
			}
			return;
		} else {
			/**
			 * Add the image to the cache even if the threshold is exceeded so that
			 * new WaveViews can still cache images with a full cache, the size of
			 * the cache will quickly equalize back to the threshold as new images
			 * are added and the size of the cache is reduced.
			 */
		}
	}

	_cached_images.push_back (image);
	_parent_cache.increase_size (image->size_in_bytes ());
}

boost::shared_ptr<WaveViewImage>
WaveViewCacheGroup::lookup_image (WaveViewProperties const& props)
{
	for (ImageCache::iterator i = _cached_images.begin (); i != _cached_images.end (); ++i) {
		if ((*i)->props.is_equivalent (props)) {
			return (*i);
		}
	}
	return boost::shared_ptr<WaveViewImage>();
}

void
WaveViewCacheGroup::clear_cache ()
{
	// Tell the parent cache about the images we are about to drop references to
	for (ImageCache::iterator it = _cached_images.begin (); it != _cached_images.end (); ++it) {
		_parent_cache.decrease_size ((*it)->size_in_bytes ());
	}
	_cached_images.clear ();
}

/*-------------------------------------------------*/

WaveViewCache::WaveViewCache ()
	: image_cache_size (0)
	, _image_cache_threshold (100 * 1048576) /* bytes */
{

}

WaveViewCache::~WaveViewCache ()
{
}

WaveViewCache*
WaveViewCache::get_instance ()
{
	static WaveViewCache* instance = new WaveViewCache;
	return instance;
}

void
WaveViewCache::increase_size (uint64_t bytes)
{
	image_cache_size += bytes;
}

void
WaveViewCache::decrease_size (uint64_t bytes)
{
	assert (bytes > 0);
	assert (bytes <= image_cache_size);
	image_cache_size -= bytes;
}

boost::shared_ptr<WaveViewCacheGroup>
WaveViewCache::get_cache_group (boost::shared_ptr<ARDOUR::AudioSource> source)
{
	CacheGroups::iterator it = cache_group_map.find (source);

	if (it != cache_group_map.end()) {
		// Found existing CacheGroup for AudioSource
		return it->second;
	}

	boost::shared_ptr<WaveViewCacheGroup> new_group (new WaveViewCacheGroup (*this));

	bool inserted = cache_group_map.insert (std::make_pair (source, new_group)).second;

	assert (inserted);

	return new_group;
}

void
WaveViewCache::reset_cache_group (boost::shared_ptr<WaveViewCacheGroup>& group)
{
	if (!group) {
		return;
	}

	CacheGroups::iterator it = cache_group_map.begin();

	while (it != cache_group_map.end()) {
		if (it->second == group) {
			break;
		}
		++it;
	}

	assert (it != cache_group_map.end ());

	group.reset();

	if (it->second.unique()) {
		cache_group_map.erase (it);
	}
}

void
WaveViewCache::clear_cache ()
{
	for (CacheGroups::iterator it = cache_group_map.begin (); it != cache_group_map.end (); ++it) {
		(*it).second->clear_cache ();
	}
}

void
WaveViewCache::set_image_cache_threshold (uint64_t sz)
{
	_image_cache_threshold = sz;
}

/*-------------------------------------------------*/

WaveViewThreads::WaveViewThreads ()
	: _quit (false)
{
}

WaveViewThreads::~WaveViewThreads ()
{
}

uint32_t WaveViewThreads::init_count = 0;

WaveViewThreads* WaveViewThreads::instance = 0;

void
WaveViewThreads::initialize ()
{
	// no need for atomics as only called from GUI thread
	if (++init_count == 1) {
		assert(!instance);
		instance = new WaveViewThreads;
		instance->start_threads();
	}
}

void
WaveViewThreads::deinitialize ()
{
	if (--init_count == 0) {
		instance->stop_threads();
		delete instance;
		instance = 0;
	}
}

void
WaveViewThreads::enqueue_draw_request (boost::shared_ptr<WaveViewDrawRequest>& request)
{
	assert (instance);
	instance->_enqueue_draw_request (request);
}

void
WaveViewThreads::_enqueue_draw_request (boost::shared_ptr<WaveViewDrawRequest>& request)
{
	Glib::Threads::Mutex::Lock lm (_queue_mutex);
	_queue.push_back (request);
	/* wake one (random) thread */
	_cond.signal ();
}

boost::shared_ptr<WaveViewDrawRequest>
WaveViewThreads::dequeue_draw_request ()
{
	assert (instance);
	return instance->_dequeue_draw_request ();
}

boost::shared_ptr<WaveViewDrawRequest>
WaveViewThreads::_dequeue_draw_request ()
{
	/* _queue_mutex must be held at this point */

	assert (!_queue_mutex.trylock());

	if (_queue.empty()) {
		_cond.wait (_queue_mutex);
	}

	boost::shared_ptr<WaveViewDrawRequest> req;

	/* queue could be empty at this point because an already running thread
	 * pulled the request before we were fully awake and reacquired the mutex.
	 */

	if (!_queue.empty()) {
		req = _queue.front ();
		_queue.pop_front ();
	}

	return req;
}

void
WaveViewThreads::start_threads ()
{
	assert (!_threads.size());

	const int num_cpus = hardware_concurrency ();

	/* the upper limit of 8 here is entirely arbitrary. It just doesn't
	 * seem worthwhile having "ncpus" of low priority threads for
	 * rendering waveforms into the cache.
	 */

	uint32_t num_threads = std::min (8, std::max (1, num_cpus - 1));

	for (uint32_t i = 0; i != num_threads; ++i) {
		boost::shared_ptr<WaveViewDrawingThread> new_thread (new WaveViewDrawingThread ());
		_threads.push_back(new_thread);
	}
}

void
WaveViewThreads::stop_threads ()
{
	assert (_threads.size());

	{
		Glib::Threads::Mutex::Lock lm (_queue_mutex);
		_quit = true;
		_cond.broadcast ();
	}

	/* Deleting the WaveViewThread objects will force them to join() with
	 * their underlying (p)threads, and thus cleanup. The threads will
	 * all be woken by the condition broadcast above.
	 */

	_threads.clear ();
}

/*-------------------------------------------------*/
WaveViewDrawRequest::WaveViewDrawRequest ()
{
	g_atomic_int_set (&_stop, 0);
}

WaveViewDrawRequest::~WaveViewDrawRequest ()
{

}

/*-------------------------------------------------*/

WaveViewDrawingThread::WaveViewDrawingThread ()
		: _thread(0)
{
	start ();
}

WaveViewDrawingThread::~WaveViewDrawingThread ()
{
	_thread->join ();
}

void
WaveViewDrawingThread::start ()
{
	assert (!_thread);

	_thread = Glib::Threads::Thread::create (sigc::ptr_fun (&WaveViewThreads::thread_proc));
}

void
WaveViewThreads::thread_proc ()
{
	assert (instance);
	instance->_thread_proc ();
}

/* Notes on thread/sync design:
 *
 *
 * the worker threads do not hold the _queue_mutex while doing work. This means
 * that an attempt to signal them using a condition variable and the
 * _queue_mutex is not guaranteed to work - they may not be either (a) holding
 * the lock or (b) waiting on condition variable (having gone to sleep on the
 * mutex).
 *
 * Instead, when the signalling thread takes the mutex, they may be busy
 * working, and will therefore miss the signal.
 *
 * This is fine for handling requests - worker threads will just loop around,
 * check the request queue again, and behave appropriately (i.e. do more more
 * work, or go to sleep waiting on condition variable.
 *
 * But it's not fine when we need to tell the threads to quit. We can't do this
 * with requests, because there's no way to ensure that each thread will pick
 * up a request. So we have a bool member, _quit, which we set to indicate
 * that threads should exit. This integer is protected by the _queue_mutex. If
 * it was not (and was instead just an atomic integer), we would get a race
 * condition where a worker thread checks _quit, finds it is still false, then
 * takes the mutex in order to check the request queue, gets blocked there
 * because a signalling thread has acquired the mutex (and broadcasts the
 * condition), then the worker continues (now holding the mutex), finds no
 * requests, and goes to sleep, never to be woken again.
 *
 *      Signalling Thread                 Worker Thread
 *      =================                 =============
 *                                        _quit == true ? => false
 *      _quit = true
 *      acquire _queue_mutex
 *      cond.broadcast()                  acquire _queue_mutex => sleep
 *      release _queue_mutex              sleep
 *                                        wake
 *                                        check request queue => empty
 *                                        sleep on cond, FOREVER
 *
 * This was the design until 166ac63924c2b. Now we acquire the mutex in the
 * classic thread synchronization manner, and there is no race:
 *
 *      Signalling Thread                 Worker Thread
 *      =================                 =============
 *
 *      acquire _queue_mutex              acquire _queue_mutex => sleep
 *      _quit = true
 *      release _queue_mutex
 *      cond.broadcast()
 *      release _queue_mutex
 *                                        wake
 *                                        _quit == true ? => true
 *                                        exit
 *
 * If worker threads held the mutex while working, a slightly different design
 * would be correct, but because there is a single queue protected by the
 * mutex, that would effectively serialize all worker threads which would be
 * pointless.
 */

void
WaveViewThreads::_thread_proc ()
{
	pthread_set_name ("WaveViewDrawing");

	while (true) {

		_queue_mutex.lock ();

		if (_quit) {
			/* time to die */
			_queue_mutex.unlock ();
			break;
		}

		/* try to fetch a request from the queue. If none are
		 * immediately available, we will block until woken by a
		 * new request, but that request might be handled by an already
		 * running thread, so the return here may be null (that is not
		 * an error). We may also be woken by cond.broadcast(), in
		 * which case there will be no request in the queue, but we are
		 * supposed to loop around and check _quit.
		 */

		boost::shared_ptr<WaveViewDrawRequest> req = WaveViewThreads::dequeue_draw_request ();

		_queue_mutex.unlock ();

		if (req && !req->stopped()) {
			try {
				WaveView::process_draw_request (req);
			} catch (...) {
				/* just in case it was set before the exception, whatever it was */
				req->image->cairo_image.clear ();
			}
		}
	}
}

} /* namespace */
