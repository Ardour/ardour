/*
 * Copyright (C) 2017 Tim Mayberry <mojofunk@gmail.com>
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

#ifndef _WAVEVIEW_WAVE_VIEW_PRIVATE_H_
#define _WAVEVIEW_WAVE_VIEW_PRIVATE_H_

#include <deque>

#include "waveview/wave_view.h"

namespace ARDOUR {
	class AudioRegion;
}

namespace ArdourWaveView {

struct WaveViewProperties
{
public: // ctors
	WaveViewProperties (boost::shared_ptr<ARDOUR::AudioRegion> region);

	// WaveViewProperties (WaveViewProperties const& other) = default;

	// WaveViewProperties& operator=(WaveViewProperties const& other) = default;

public: // member variables

	samplepos_t            region_start;
	samplepos_t            region_end;
	uint16_t              channel;
	double                height;
	double                samples_per_pixel;
	double                amplitude;
	double                amplitude_above_axis;
	Gtkmm2ext::Color      fill_color;
	Gtkmm2ext::Color      outline_color;
	Gtkmm2ext::Color      zero_color;
	Gtkmm2ext::Color      clip_color;
	bool                  show_zero;
	bool                  logscaled;
	WaveView::Shape       shape;
	double                gradient_depth;
	double                start_shift;

private: // member variables

	samplepos_t            sample_start;
	samplepos_t            sample_end;

public: // methods

	bool is_valid () const
	{
		return (sample_end != 0 && samples_per_pixel != 0);
	}

	void set_width_samples (ARDOUR::samplecnt_t const width_samples)
	{
		assert (is_valid());
		assert (width_samples != 0);
		ARDOUR::samplecnt_t half_width = width_samples / 2;
		samplepos_t new_sample_start = std::max (region_start, get_center_sample () - half_width);
		samplepos_t new_sample_end = std::min (get_center_sample () + half_width, region_end);
		assert (new_sample_start <= new_sample_end);
		sample_start = new_sample_start;
		sample_end = new_sample_end;
	}

	uint64_t get_width_pixels () const
	{
		return (uint64_t)std::max (1LL, llrint (ceil (get_length_samples () / samples_per_pixel)));
	}


	void set_sample_offsets (samplepos_t const start, samplepos_t const end)
	{
		assert (start <= end);

		// sample_start and sample_end are bounded by the region limits.
		if (start < region_start) {
			sample_start = region_start;
		} else if (start > region_end) {
			sample_start = region_end;
		} else {
			sample_start = start;
		}

		if (end > region_end) {
			sample_end = region_end;
		} else if (end < region_start) {
			sample_end = region_start;
		} else {
			sample_end = end;
		}

		assert (sample_start <= sample_end);
	}

	samplepos_t get_sample_start () const
	{
		return sample_start;
	}

	samplepos_t get_sample_end () const
	{
		return sample_end;
	}

	void set_sample_positions_from_pixel_offsets (double start_pixel, double end_pixel)
	{
		assert (start_pixel <= end_pixel);
		/**
		 * It is possible for the new sample positions to be past the region_end,
		 * so we have to do bounds checking/adjustment for this in set_sample_offsets.
		 */
		samplepos_t new_sample_start = region_start + (start_pixel * samples_per_pixel);
		samplepos_t new_sample_end = region_start + (end_pixel * samples_per_pixel);
		set_sample_offsets (new_sample_start, new_sample_end);
	}

	ARDOUR::samplecnt_t get_length_samples () const
	{
		assert (sample_start <= sample_end);
		return sample_end - sample_start;
	}

	samplepos_t get_center_sample () const
	{
		return sample_start + (get_length_samples() / 2);
	}

	bool is_equivalent (WaveViewProperties const& other)
	{
		return (samples_per_pixel == other.samples_per_pixel &&
		        contains (other.sample_start, other.sample_end) && channel == other.channel &&
		        height == other.height && amplitude == other.amplitude &&
		        amplitude_above_axis == other.amplitude_above_axis && fill_color == other.fill_color &&
		        outline_color == other.outline_color && zero_color == other.zero_color &&
		        clip_color == other.clip_color && show_zero == other.show_zero &&
		        logscaled == other.logscaled && shape == other.shape &&
		        gradient_depth == other.gradient_depth);
		// region_start && start_shift??
	}

	bool contains (samplepos_t start, samplepos_t end)
	{
		return (sample_start <= start && end <= sample_end);
	}
};

struct WaveViewImage {
public: // ctors
	WaveViewImage (boost::shared_ptr<const ARDOUR::AudioRegion> const& region_ptr,
	               WaveViewProperties const& properties);

	~WaveViewImage ();

public: // member variables
	boost::weak_ptr<const ARDOUR::AudioRegion> region;
	WaveViewProperties props;
	Cairo::RefPtr<Cairo::ImageSurface> cairo_image;
	uint64_t timestamp;

public: // methods
	bool finished() { return static_cast<bool>(cairo_image); }

	bool
	contains_image_with_properties (WaveViewProperties const& other_props)
	{
		return cairo_image && props.is_equivalent (other_props);
	}

	bool is_valid () {
		return props.is_valid ();
	}

	size_t size_in_bytes ()
	{
		// 4 = bytes per FORMAT_ARGB32 pixel
		return props.height * props.get_width_pixels() * 4;
	}
};

struct WaveViewDrawRequest
{
public:
	WaveViewDrawRequest ();
	~WaveViewDrawRequest ();

	bool stopped() const { return (bool) g_atomic_int_get (&_stop); }
	void cancel() { g_atomic_int_set (&_stop, 1); }
	bool finished() { return image->finished(); }

	boost::shared_ptr<WaveViewImage> image;

	bool is_valid () {
		return (image && image->is_valid());
	}

private:
	GATOMIC_QUAL gint _stop; /* intended for atomic access */
};

class WaveViewCache;

class WaveViewCacheGroup
{
public:
	WaveViewCacheGroup (WaveViewCache& parent_cache);

	~WaveViewCacheGroup ();

public:

	// @return image with matching properties or null
	boost::shared_ptr<WaveViewImage> lookup_image (WaveViewProperties const&);

	void add_image (boost::shared_ptr<WaveViewImage>);

	bool full () const { return _cached_images.size() > max_size(); }

	static uint32_t max_size () { return 16; }

	void clear_cache ();

private:

	/**
	 * At time of writing we don't strictly need a reference to the parent cache
	 * as there is only a single global cache but if the image cache ever becomes
	 * a per canvas cache then a using a reference is handy.
	 */
	WaveViewCache& _parent_cache;

	typedef std::list<boost::shared_ptr<WaveViewImage> > ImageCache;
	ImageCache _cached_images;
};

class WaveViewCache
{
public:
	static WaveViewCache* get_instance ();

	uint64_t image_cache_threshold () const { return _image_cache_threshold; }
	void set_image_cache_threshold (uint64_t);

	void clear_cache ();

	boost::shared_ptr<WaveViewCacheGroup> get_cache_group (boost::shared_ptr<ARDOUR::AudioSource>);

	void reset_cache_group (boost::shared_ptr<WaveViewCacheGroup>&);

private:
	WaveViewCache();
	~WaveViewCache();

private:
	typedef std::map<boost::shared_ptr<ARDOUR::AudioSource>, boost::shared_ptr<WaveViewCacheGroup> >
	    CacheGroups;

	CacheGroups cache_group_map;

	uint64_t image_cache_size;
	uint64_t _image_cache_threshold;

private:
	friend class WaveViewCacheGroup;

	void increase_size (uint64_t bytes);
	void decrease_size (uint64_t bytes);

	bool full () { return image_cache_size > _image_cache_threshold; }
};

class WaveViewDrawingThread
{
public:
	WaveViewDrawingThread ();
	~WaveViewDrawingThread ();

private:
	void start ();
	void run ();

private:
	Glib::Threads::Thread* _thread;
};

class WaveViewThreads {
private:
	WaveViewThreads ();
	~WaveViewThreads ();

public:
	static void initialize ();
	static void deinitialize ();

	static bool enabled () { return (instance); }

	static void enqueue_draw_request (boost::shared_ptr<WaveViewDrawRequest>&);

private:
	friend class WaveViewDrawingThread;

	// will block until a request is available
	static boost::shared_ptr<WaveViewDrawRequest> dequeue_draw_request ();
	static void thread_proc ();

	boost::shared_ptr<WaveViewDrawRequest> _dequeue_draw_request ();
	void _enqueue_draw_request (boost::shared_ptr<WaveViewDrawRequest>&);
	void _thread_proc ();

	void start_threads ();
	void stop_threads ();

private:
	static uint32_t init_count;
	static WaveViewThreads* instance;

	// TODO use std::unique_ptr when possible
	typedef std::vector<boost::shared_ptr<WaveViewDrawingThread> > WaveViewThreadList;

	bool _quit;
	WaveViewThreadList _threads;


	mutable Glib::Threads::Mutex _queue_mutex;
	Glib::Threads::Cond _cond;

	typedef std::deque<boost::shared_ptr<WaveViewDrawRequest> > DrawRequestQueueType;
	DrawRequestQueueType _queue;
};


} /* namespace */

#endif
