/*
    Copyright (C) 2011-2013 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/scoped_array.hpp>

#include "pbd/properties.h"

#include "ardour/types.h"

#include <glibmm/refptr.h>

#include "canvas/visibility.h"
#include "canvas/item.h"
#include "canvas/fill.h"
#include "canvas/outline.h"

namespace ARDOUR {
	class AudioRegion;
}

namespace Gdk {
	class Pixbuf;
}

class WaveViewTest;
	
namespace ArdourCanvas {

struct LIBCANVAS_API WaveViewThreadRequest
{
  public:
        enum RequestType {
	        Quit,
	        Cancel,
	        Draw
        };
        
	WaveViewThreadRequest  () : stop (0) {}
	
	bool should_stop () const { return (bool) g_atomic_int_get (const_cast<gint*>(&stop)); }
	void cancel() { g_atomic_int_set (&stop, 1); }
	
	RequestType type;
	framepos_t start;
	framepos_t end;
	double     width;
	double     height;
	double     samples_per_pixel;
	uint16_t   channel;
	double     amplitude;
	Color      fill_color;
	boost::weak_ptr<const ARDOUR::Region> region;

	/* resulting image, after request has been satisfied */
	
	Cairo::RefPtr<Cairo::ImageSurface> image;
	
  private:
	gint stop; /* intended for atomic access */
};

class LIBCANVAS_API WaveView;

class LIBCANVAS_API WaveViewCache
{
  public:
	WaveViewCache();
	~WaveViewCache();
	
	struct Entry {

		/* these properties define the cache entry as unique.

		   If an image is in use by a WaveView and any of these
		   properties are modified on the WaveView, the image can no
		   longer be used (or may no longer be usable for start/end
		   parameters). It will remain in the cache until flushed for
		   some reason (typically the cache is full).
		*/

		int channel;
		Coord height;
		float amplitude;
		Color fill_color;
		double samples_per_pixel;
		framepos_t start;
		framepos_t end;

		/* the actual image referred to by the cache entry */

		Cairo::RefPtr<Cairo::ImageSurface> image;

		/* last time the cache entry was used */
		uint64_t timestamp;
		
		Entry (int chan, Coord hght, float amp, Color fcl, double spp, framepos_t strt, framepos_t ed,
		       Cairo::RefPtr<Cairo::ImageSurface> img) 
			: channel (chan)
			, height (hght)
			, amplitude (amp)
			, fill_color (fcl)
			, samples_per_pixel (spp)
			, start (strt)
			, end (ed)
			, image (img) {}
	};

	uint64_t image_cache_threshold () const { return _image_cache_threshold; }
	void set_image_cache_threshold (uint64_t);
	
	void add (boost::shared_ptr<ARDOUR::AudioSource>, boost::shared_ptr<Entry>);
	void use (boost::shared_ptr<ARDOUR::AudioSource>, boost::shared_ptr<Entry>);
	
        void consolidate_image_cache (boost::shared_ptr<ARDOUR::AudioSource>,
                                      int channel,
                                      Coord height,
                                      float amplitude,
                                      Color fill_color,
                                      double samples_per_pixel);

        boost::shared_ptr<Entry> lookup_image (boost::shared_ptr<ARDOUR::AudioSource>,
                                               framepos_t start, framepos_t end,
                                               int _channel,
                                               Coord height,
                                               float amplitude,
                                               Color fill_color,
                                               double samples_per_pixel,
                                               bool& full_image);

  private:
        /* an unsorted, unindexd collection of cache entries associated with
           a particular AudioSource. All cache Entries in the collection
           share the AudioSource in common, but represent different parameter
           settings (e.g. height, color, samples per pixel etc.)
        */
        typedef std::vector<boost::shared_ptr<Entry> > CacheLine;
        /* Indexed, non-sortable structure used to lookup images associated
         * with a particular AudioSource
         */
        typedef std::map <boost::shared_ptr<ARDOUR::AudioSource>,CacheLine> ImageCache;
        ImageCache cache_map;

        /* Linear, sortable structure used when we need to do a timestamp-based
         * flush of entries from the cache.
         */
        typedef std::pair<boost::shared_ptr<ARDOUR::AudioSource>,boost::shared_ptr<Entry> > ListEntry;
        typedef std::vector<ListEntry> CacheList;
 
        struct SortByTimestamp {
	        bool operator() (const WaveViewCache::ListEntry& a, const WaveViewCache::ListEntry& b) {
		        return a.second->timestamp < b.second->timestamp;
	        }
        };
        friend struct SortByTimestamp;
        
        uint64_t image_cache_size;
        uint64_t _image_cache_threshold;

        uint64_t compute_image_cache_size ();
        void cache_flush ();
        bool cache_full ();
};

class LIBCANVAS_API WaveView : public Item, public sigc::trackable
{
public:

        enum Shape { 
		Normal,
		Rectified
        };

        std::string debug_name() const;

        /* final ImageSurface rendered with colours */

	Cairo::RefPtr<Cairo::ImageSurface> _image;
	PBD::Signal0<void> ImageReady;
	
	/* Displays a single channel of waveform data for the given Region.

	   x = 0 in the waveview corresponds to the first waveform datum taken
	   from region->start() samples into the source data.
	   
	   x = N in the waveview corresponds to the (N * spp)'th sample 
	   measured from region->start() into the source data.
	   
	   when drawing, we will map the zeroth-pixel of the waveview
	   into a window. 
	   
	   The waveview itself contains a set of pre-rendered Cairo::ImageSurfaces
	   that cache sections of the display. This is filled on-demand and
	   never cleared until something explicitly marks the cache invalid
	   (such as a change in samples_per_pixel, the log scaling, rectified or
	   other view parameters).
	*/

	WaveView (Canvas *, boost::shared_ptr<ARDOUR::AudioRegion>);
	WaveView (Item*, boost::shared_ptr<ARDOUR::AudioRegion>);
       ~WaveView ();

	void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box () const;
    
	void set_samples_per_pixel (double);
	void set_height (Distance);
	void set_channel (int);
	void set_region_start (ARDOUR::frameoffset_t);

	/** Change the first position drawn by @param pixels.
	 * @param pixels must be positive. This is used by
	 * AudioRegionViews in Ardour to avoid drawing the
	 * first pixel of a waveform, and exists in case
	 * there are uses for WaveView where we do not
	 * want this behaviour.
	 */
	void set_start_shift (double pixels);
	
        void set_fill_color (Color);
        void set_outline_color (Color);
	
	void region_resized ();
        void gain_changed ();

        void set_show_zero_line (bool);
        bool show_zero_line() const { return _show_zero; }
        void set_zero_color (Color);
        void set_clip_color (Color);
        void set_logscaled (bool);
        void set_gradient_depth (double);
        double gradient_depth() const { return _gradient_depth; }
        void set_shape (Shape);

        void set_always_get_image_in_thread (bool yn);
        
        /* currently missing because we don't need them (yet):
	   set_shape_independent();
	   set_logscaled_independent()
        */

        static void set_global_gradient_depth (double);
        static void set_global_logscaled (bool);
        static void set_global_shape (Shape);
        static void set_global_show_waveform_clipping (bool);
    
        static double  global_gradient_depth()  { return _global_gradient_depth; }
        static bool    global_logscaled()  { return _global_logscaled; }
        static Shape   global_shape()  { return _global_shape; }

        void set_amplitude_above_axis (double v);
        double amplitude_above_axis () const { return _amplitude_above_axis; }

	static void set_clip_level (double dB);
	static PBD::Signal0<void> ClipLevelChanged;

	static void start_drawing_thread ();
	static void stop_drawing_thread ();

	static void set_image_cache_size (uint64_t);
	
#ifdef CANVAS_COMPATIBILITY	
	void*& property_gain_src () {
		return _foo_void;
	}
	void*& property_gain_function () {
		return _foo_void;
	}
  private:
	void* _foo_void;

#endif

  private:
        friend class ::WaveViewTest;
        friend class WaveViewThreadClient;

        void invalidate_image_cache ();

	boost::shared_ptr<ARDOUR::AudioRegion> _region;
	int    _channel;
	double _samples_per_pixel;
	Coord  _height;
        bool   _show_zero;
        Color  _zero_color;
        Color  _clip_color;
        bool   _logscaled;
        Shape  _shape;
        double _gradient_depth;
        bool   _shape_independent;
        bool   _logscaled_independent;
        bool   _gradient_depth_independent;
        double _amplitude_above_axis;
	float  _region_amplitude;
	double _start_shift;
	
	/** The `start' value to use for the region; we can't use the region's
	 *  value as the crossfade editor needs to alter it.
	 */
	ARDOUR::frameoffset_t _region_start;

	/** Under almost conditions, this is going to return _region->length(),
	 * but if _region_start has been reset, then we need
	 * to use this modified computation.
	 */
	ARDOUR::framecnt_t region_length() const;
	/** Under almost conditions, this is going to return _region->start() +
	 * _region->length(), but if _region_start has been reset, then we need
	 * to use this modified computation.
	 */
	ARDOUR::framepos_t region_end() const;

	/** If true, calls to get_image() will render a missing wave image
	   in the calling thread. Generally set to false, but true after a
	   call to set_height().
	*/
	mutable bool get_image_in_thread;

	/** If true, calls to get_image() will render a missing wave image
	   in the calling thread. Set true for waveviews we expect to 
	   keep updating (e.g. while recording)
	*/
	bool always_get_image_in_thread;
	
	/** Set to true by render(). Used so that we know if the wave view
	 * has actually been displayed on screen. ::set_height() when this
	 * is true does not use get_image_in_thread, because it implies
	 * that the height is being set BEFORE the waveview is drawn.
	 */
	mutable bool rendered;
	
        PBD::ScopedConnectionList invalidation_connection;
        PBD::ScopedConnection     image_ready_connection;

        static double _global_gradient_depth;
        static bool   _global_logscaled;
        static Shape  _global_shape;
        static bool   _global_show_waveform_clipping;
	static double _clip_level;

        static PBD::Signal0<void> VisualPropertiesChanged;

        void handle_visual_property_change ();
        void handle_clip_level_change ();

        boost::shared_ptr<WaveViewCache::Entry> get_image (framepos_t start, framepos_t end, bool& full_image) const;
        boost::shared_ptr<WaveViewCache::Entry> get_image_from_cache (framepos_t start, framepos_t end, bool& full_image) const;
	
        struct LineTips {
	        double top;
	        double bot;
	        double spread;
	        bool clip_max;
	        bool clip_min;
	        
	        LineTips() : top (0.0), bot (0.0), clip_max (false), clip_min (false) {}
        };

        ArdourCanvas::Coord y_extent (double) const;
        void compute_tips (ARDOUR::PeakData const & peak, LineTips& tips) const;

        ARDOUR::framecnt_t desired_image_width () const;

        void draw_image (Cairo::RefPtr<Cairo::ImageSurface>&, ARDOUR::PeakData*, int n_peaks, boost::shared_ptr<WaveViewThreadRequest>) const;
	void draw_absent_image (Cairo::RefPtr<Cairo::ImageSurface>&, ARDOUR::PeakData*, int) const;
	
        void cancel_my_render_request () const;

        void queue_get_image (boost::shared_ptr<const ARDOUR::Region> region, framepos_t start, framepos_t end) const;
        void generate_image (boost::shared_ptr<WaveViewThreadRequest>, bool in_render_thread) const;
        boost::shared_ptr<WaveViewCache::Entry> cache_request_result (boost::shared_ptr<WaveViewThreadRequest> req) const;
        
        void image_ready ();
        
        mutable boost::shared_ptr<WaveViewCache::Entry> _current_image;
        
	mutable boost::shared_ptr<WaveViewThreadRequest> current_request;
	
	static WaveViewCache* images;

	static void drawing_thread ();

        static gint drawing_thread_should_quit;
        static Glib::Threads::Mutex request_queue_lock;
        static Glib::Threads::Cond request_cond;
        static Glib::Threads::Thread* _drawing_thread;
        typedef std::set<WaveView const *> DrawingRequestQueue;
        static DrawingRequestQueue request_queue;
};

}
