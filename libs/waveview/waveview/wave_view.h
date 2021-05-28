/*
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef _WAVEVIEW_WAVE_VIEW_H_
#define _WAVEVIEW_WAVE_VIEW_H_

#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>

#include <glibmm/refptr.h>

#include "ardour/types.h"
#include "canvas/item.h"
#include "waveview/visibility.h"

namespace ARDOUR {
	class AudioRegion;
}

namespace Gdk {
	class Pixbuf;
}

namespace ArdourWaveView {

class WaveViewCacheGroup;
class WaveViewDrawRequest;
class WaveViewDrawRequestQueue;
class WaveViewImage;
class WaveViewProperties;
class WaveViewDrawingThread;

class LIBWAVEVIEW_API WaveView : public ArdourCanvas::Item, public sigc::trackable
{
public:
	enum Shape { Normal, Rectified };

	std::string debug_name () const;

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

	WaveView (ArdourCanvas::Canvas*, boost::shared_ptr<ARDOUR::AudioRegion>);
	WaveView (Item*, boost::shared_ptr<ARDOUR::AudioRegion>);
	~WaveView ();

	virtual void prepare_for_render (ArdourCanvas::Rect const& window_area) const;

	virtual void render (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context>) const;

	void compute_bounding_box () const;

	void set_samples_per_pixel (double);
	void set_height (ArdourCanvas::Distance);
	void set_channel (int);
	void set_region_start (ARDOUR::sampleoffset_t);

	/** Change the first position drawn by \p pixels .
	 * \p pixels must be positive. This is used by
	 * AudioRegionViews in Ardour to avoid drawing the
	 * first pixel of a waveform, and exists in case
	 * there are uses for WaveView where we do not
	 * want this behaviour.
	 */
	void set_start_shift (double pixels);

	void set_fill_color (Gtkmm2ext::Color);
	void set_outline_color (Gtkmm2ext::Color);

	void region_resized ();
	void gain_changed ();

	void set_show_zero_line (bool);
	bool show_zero_line () const;

	void set_zero_color (Gtkmm2ext::Color);
	void set_clip_color (Gtkmm2ext::Color);
	void set_logscaled (bool);

	void set_gradient_depth (double);
	double gradient_depth () const;

	void set_shape (Shape);

	void set_always_get_image_in_thread (bool yn);

	/* currently missing because we don't need them (yet):
	 * set_shape_independent();
	 * set_logscaled_independent();
	 */

	static void set_global_gradient_depth (double);
	static void set_global_logscaled (bool);
	static void set_global_shape (Shape);
	static void set_global_show_waveform_clipping (bool);
	static void clear_cache ();

	static double global_gradient_depth () { return _global_gradient_depth; }

	static bool global_logscaled () { return _global_logscaled; }

	static Shape global_shape () { return _global_shape; }

	void set_amplitude_above_axis (double v);

	double amplitude_above_axis () const;

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
	friend class WaveViewThreadClient;
	friend class WaveViewThreads;

	boost::shared_ptr<ARDOUR::AudioRegion> _region;

	boost::scoped_ptr<WaveViewProperties> _props;

	mutable boost::shared_ptr<WaveViewImage> _image;

	mutable boost::shared_ptr<WaveViewCacheGroup> _cache_group;

	bool _shape_independent;
	bool _logscaled_independent;
	bool _gradient_depth_independent;

	/** Under almost conditions, this is going to return _region->length(),
	 * but if region_start has been reset, then we need to use this modified
	 * computation.
	 */
	ARDOUR::samplecnt_t region_length () const;

	/** Under almost conditions, this is going to return _region->start() +
	 * _region->length(), but if region_start has been reset, then we need to use
	 * this modified computation.
	 */
	ARDOUR::samplepos_t region_end () const;

	/**
	 * _image stays non-null after the first time it is set
	 */
	bool rendered () const { return _image.get(); }

	bool draw_image_in_gui_thread () const;

	/** If true, calls to render() will render a missing wave image in the GUI
	 * thread. Generally set to false, but true after a call to set_height().
	 */
	mutable bool _draw_image_in_gui_thread;

	/** If true, calls to render() will render a missing wave image in the GUI
	 * thread. Set true for waveviews we expect to keep updating (e.g. while
	 * recording)
	 */
	bool _always_draw_image_in_gui_thread;

	void init();

	mutable boost::shared_ptr<WaveViewDrawRequest> current_request;

	PBD::ScopedConnectionList invalidation_connection;

	static double _global_gradient_depth;
	static bool _global_logscaled;
	static Shape _global_shape;
	static bool _global_show_waveform_clipping;
	static double _global_clip_level;

	static PBD::Signal0<void> VisualPropertiesChanged;

	void handle_visual_property_change ();
	void handle_clip_level_change ();

	struct LineTips {
		double top;
		double bot;
		double spread;
		bool clip_max;
		bool clip_min;

		LineTips () : top (0.0), bot (0.0), clip_max (false), clip_min (false) {}
	};

	static ArdourCanvas::Coord y_extent (double, Shape const, double const height);

	static void compute_tips (ARDOUR::PeakData const& peak, LineTips& tips, double const effective_height);

	static void draw_image (Cairo::RefPtr<Cairo::ImageSurface>&, ARDOUR::PeakData*, int n_peaks,
	                        boost::shared_ptr<WaveViewDrawRequest>);
	static void draw_absent_image (Cairo::RefPtr<Cairo::ImageSurface>&, ARDOUR::PeakData*, int);

	ARDOUR::samplecnt_t optimal_image_width_samples () const;

	void set_image (boost::shared_ptr<WaveViewImage> img) const;

	// @return true if item area intersects with draw area
	bool get_item_and_draw_rect_in_window_coords (ArdourCanvas::Rect const& canvas_rect,
	                                              ArdourCanvas::Rect& item_area,
	                                              ArdourCanvas::Rect& draw_rect) const;

	boost::shared_ptr<WaveViewDrawRequest> create_draw_request (WaveViewProperties const&) const;

	void queue_draw_request (boost::shared_ptr<WaveViewDrawRequest> const&) const;

	static void process_draw_request (boost::shared_ptr<WaveViewDrawRequest>);

	boost::shared_ptr<WaveViewCacheGroup> get_cache_group () const;

	/**
	 * Notify the Cache that we are dropping our reference to the
	 * CacheGroup so it can also do so if it is the only reference holder
	 * of the cache group.
	 */
	void reset_cache_group ();
};

} /* namespace */

#endif
