/*
    Copyright (C) 2017 Tim Mayberry

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <cmath>
#include "ardour/lmath.h"

#include "pbd/cpus.h"

#include "ardour/audioregion.h"
#include "ardour/audiosource.h"

#include "waveview/wave_view_private.h"

namespace ArdourWaveView {

WaveViewProperties::WaveViewProperties (boost::shared_ptr<ARDOUR::AudioRegion> region)
    : region_start (region->start ())
    , region_end (region->start () + region->length ())
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
	assert (image_cache_size - bytes < image_cache_size);
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

WaveViewDrawRequest::WaveViewDrawRequest () : stop (0)
{

}

WaveViewDrawRequest::~WaveViewDrawRequest ()
{

}

void
WaveViewDrawRequestQueue::enqueue (boost::shared_ptr<WaveViewDrawRequest>& request)
{
	Glib::Threads::Mutex::Lock lm (_queue_mutex);

	_queue.push_back (request);
	_cond.broadcast ();
}

void
WaveViewDrawRequestQueue::wake_up ()
{
	boost::shared_ptr<WaveViewDrawRequest> null_ptr;
	// hack!?...wake up the drawing thread
	enqueue (null_ptr);
}

boost::shared_ptr<WaveViewDrawRequest>
WaveViewDrawRequestQueue::dequeue (bool block)
{
	if (block) {
		_queue_mutex.lock();
	} else {
		if (!_queue_mutex.trylock()) {
			return boost::shared_ptr<WaveViewDrawRequest>();
		}
	}

	// _queue_mutex is always held at this point

	if (_queue.empty()) {
		if (block) {
			_cond.wait (_queue_mutex);
		} else {
			_queue_mutex.unlock();
			return boost::shared_ptr<WaveViewDrawRequest>();
		}
	}

	boost::shared_ptr<WaveViewDrawRequest> req;

	if (!_queue.empty()) {
		req = _queue.front ();
		_queue.pop_front ();
	} else {
		// Queue empty, returning empty DrawRequest
	}

	_queue_mutex.unlock();

	return req;
}

/*-------------------------------------------------*/

WaveViewThreads::WaveViewThreads ()
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
	instance->_request_queue.enqueue (request);
}

boost::shared_ptr<WaveViewDrawRequest>
WaveViewThreads::dequeue_draw_request ()
{
	assert (instance);
	return instance->_request_queue.dequeue (true);
}

void
WaveViewThreads::wake_up ()
{
	assert (instance);
	return instance->_request_queue.wake_up ();
}

void
WaveViewThreads::start_threads ()
{
	assert (!_threads.size());

	int num_cpus = hardware_concurrency ();

	uint32_t num_threads = std::max (1, num_cpus - 1);

	for (uint32_t i = 0; i != num_threads; ++i) {
		boost::shared_ptr<WaveViewDrawingThread> new_thread (new WaveViewDrawingThread ());
		_threads.push_back(new_thread);
	}
}

void
WaveViewThreads::stop_threads ()
{
	assert (_threads.size());

	_threads.clear ();
}

/*-------------------------------------------------*/

WaveViewDrawingThread::WaveViewDrawingThread ()
		: _thread(0)
		, _quit(0)
{
	start ();
}

WaveViewDrawingThread::~WaveViewDrawingThread ()
{
	quit ();
}

void
WaveViewDrawingThread::start ()
{
	assert (!_thread);

	_thread = Glib::Threads::Thread::create (sigc::mem_fun (*this, &WaveViewDrawingThread::run));
}

void
WaveViewDrawingThread::quit ()
{
	assert (_thread);

	g_atomic_int_set (&_quit, 1);
	WaveViewThreads::wake_up ();
	_thread->join();
	_thread = 0;
}

void
WaveViewDrawingThread::run ()
{
	while (true) {

		if (g_atomic_int_get (&_quit)) {
			break;
		}

		// block until a request is available.
		boost::shared_ptr<WaveViewDrawRequest> req = WaveViewThreads::dequeue_draw_request ();

		if (req && !req->stopped()) {
			try {
				WaveView::process_draw_request (req);
			} catch (...) {
				/* just in case it was set before the exception, whatever it was */
				req->image->cairo_image.clear ();
			}
		} else {
			// null or stopped Request, processing skipped
		}
	}
}

}
