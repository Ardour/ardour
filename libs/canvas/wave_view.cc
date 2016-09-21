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

#include <cmath>
#include <cairomm/cairomm.h>

#include <glibmm/threads.h>

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/gui_thread.h"

#include "pbd/base_ui.h"
#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/signals.h"
#include "pbd/stacktrace.h"

#include "ardour/types.h"
#include "ardour/dB.h"
#include "ardour/lmath.h"
#include "ardour/audioregion.h"
#include "ardour/audiosource.h"
#include "ardour/session.h"

#include "canvas/canvas.h"
#include "canvas/colors.h"
#include "canvas/debug.h"
#include "canvas/utils.h"
#include "canvas/wave_view.h"

#include "evoral/Range.hpp"

#include <gdkmm/general.h>

#include "gtkmm2ext/gui_thread.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace ArdourCanvas;

double WaveView::_global_gradient_depth = 0.6;
bool WaveView::_global_logscaled = false;
WaveView::Shape WaveView::_global_shape = WaveView::Normal;
bool WaveView::_global_show_waveform_clipping = true;
double WaveView::_clip_level = 0.98853;

WaveViewCache* WaveView::images = 0;
gint WaveView::drawing_thread_should_quit = 0;
Glib::Threads::Mutex WaveView::request_queue_lock;
Glib::Threads::Mutex WaveView::current_image_lock;
Glib::Threads::Cond WaveView::request_cond;
Glib::Threads::Thread* WaveView::_drawing_thread = 0;
WaveView::DrawingRequestQueue WaveView::request_queue;

PBD::Signal0<void> WaveView::VisualPropertiesChanged;
PBD::Signal0<void> WaveView::ClipLevelChanged;

/* NO_THREAD_WAVEVIEWS is defined by the top level wscript
 * if --no-threaded-waveviws is provided at the configure step.
 */

#ifndef NO_THREADED_WAVEVIEWS
#define ENABLE_THREADED_WAVEFORM_RENDERING
#endif

WaveView::WaveView (Canvas* c, boost::shared_ptr<ARDOUR::AudioRegion> region)
	: Item (c)
	, _region (region)
	, _channel (0)
	, _samples_per_pixel (0)
	, _height (64)
	, _show_zero (false)
	, _zero_color (0xff0000ff)
	, _clip_color (0xff0000ff)
	, _logscaled (_global_logscaled)
	, _shape (_global_shape)
	, _gradient_depth (_global_gradient_depth)
	, _shape_independent (false)
	, _logscaled_independent (false)
	, _gradient_depth_independent (false)
	, _amplitude_above_axis (1.0)
	, _region_amplitude (region->scale_amplitude ())
	, _start_shift (0.0)
	, _region_start (region->start())
	, get_image_in_thread (false)
	, always_get_image_in_thread (false)
	, rendered (false)
{
	if (!images) {
		images = new WaveViewCache;
	}

	VisualPropertiesChanged.connect_same_thread (invalidation_connection, boost::bind (&WaveView::handle_visual_property_change, this));
	ClipLevelChanged.connect_same_thread (invalidation_connection, boost::bind (&WaveView::handle_clip_level_change, this));

	ImageReady.connect (image_ready_connection, invalidator (*this), boost::bind (&WaveView::image_ready, this), gui_context());
}

WaveView::WaveView (Item* parent, boost::shared_ptr<ARDOUR::AudioRegion> region)
	: Item (parent)
	, _region (region)
	, _channel (0)
	, _samples_per_pixel (0)
	, _height (64)
	, _show_zero (false)
	, _zero_color (0xff0000ff)
	, _clip_color (0xff0000ff)
	, _logscaled (_global_logscaled)
	, _shape (_global_shape)
	, _gradient_depth (_global_gradient_depth)
	, _shape_independent (false)
	, _logscaled_independent (false)
	, _gradient_depth_independent (false)
	, _amplitude_above_axis (1.0)
	, _region_amplitude (region->scale_amplitude ())
	, _start_shift (0.0)
	, _region_start (region->start())
	, get_image_in_thread (false)
	, always_get_image_in_thread (false)
	, rendered (false)
{
	if (!images) {
		images = new WaveViewCache;
	}

	VisualPropertiesChanged.connect_same_thread (invalidation_connection, boost::bind (&WaveView::handle_visual_property_change, this));
	ClipLevelChanged.connect_same_thread (invalidation_connection, boost::bind (&WaveView::handle_clip_level_change, this));

	ImageReady.connect (image_ready_connection, invalidator (*this), boost::bind (&WaveView::image_ready, this), gui_context());
}

WaveView::~WaveView ()
{
	invalidate_image_cache ();
	if (images ) {
		images->clear_cache ();
	}
}

string
WaveView::debug_name() const
{
	return _region->name() + string (":") + PBD::to_string (_channel+1);
}

void
WaveView::image_ready ()
{
	DEBUG_TRACE (DEBUG::WaveView, string_compose ("queue draw for %1 at %2 (vis = %3 CR %4)\n", this, g_get_monotonic_time(), visible(), current_request));
	redraw ();
}

void
WaveView::set_always_get_image_in_thread (bool yn)
{
	always_get_image_in_thread = yn;
}

void
WaveView::handle_visual_property_change ()
{
	bool changed = false;

	if (!_shape_independent && (_shape != global_shape())) {
		_shape = global_shape();
		changed = true;
	}

	if (!_logscaled_independent && (_logscaled != global_logscaled())) {
		_logscaled = global_logscaled();
		changed = true;
	}

	if (!_gradient_depth_independent && (_gradient_depth != global_gradient_depth())) {
		_gradient_depth = global_gradient_depth();
		changed = true;
	}

	if (changed) {
		begin_visual_change ();
		invalidate_image_cache ();
		end_visual_change ();
	}
}

void
WaveView::handle_clip_level_change ()
{
	begin_visual_change ();
	invalidate_image_cache ();
	end_visual_change ();
}

void
WaveView::set_fill_color (Color c)
{
	if (c != _fill_color) {
		begin_visual_change ();
		invalidate_image_cache ();
		Fill::set_fill_color (c);
		end_visual_change ();
	}
}

void
WaveView::set_outline_color (Color c)
{
	if (c != _outline_color) {
		begin_visual_change ();
		invalidate_image_cache ();
		Outline::set_outline_color (c);
		end_visual_change ();
	}
}

void
WaveView::set_samples_per_pixel (double samples_per_pixel)
{
	if (samples_per_pixel != _samples_per_pixel) {
		begin_change ();

		invalidate_image_cache ();
		_samples_per_pixel = samples_per_pixel;
		_bounding_box_dirty = true;

		end_change ();
	}
}

static inline float
_log_meter (float power, double lower_db, double upper_db, double non_linearity)
{
	return (power < lower_db ? 0.0 : pow((power-lower_db)/(upper_db-lower_db), non_linearity));
}

static inline float
alt_log_meter (float power)
{
	return _log_meter (power, -192.0, 0.0, 8.0);
}

void
WaveView::set_clip_level (double dB)
{
	const double clip_level = dB_to_coefficient (dB);
	if (clip_level != _clip_level) {
		_clip_level = clip_level;
		ClipLevelChanged ();
	}
}

void
WaveView::invalidate_image_cache ()
{
	DEBUG_TRACE (DEBUG::WaveView, string_compose ("%1 invalidates image cache and cancels current request\n", this));
	cancel_my_render_request ();
	Glib::Threads::Mutex::Lock lci (current_image_lock);
	_current_image.reset ();
}

void
WaveView::compute_tips (PeakData const & peak, WaveView::LineTips& tips) const
{
	const double effective_height  = _height;

	/* remember: canvas (and cairo) coordinate space puts the origin at the upper left.

	   So, a sample value of 1.0 (0dbFS) will be computed as:

	         (1.0 - 1.0) * 0.5 * effective_height

	   which evaluates to 0, or the top of the image.

	   A sample value of -1.0 will be computed as

	        (1.0 + 1.0) * 0.5 * effective height

           which evaluates to effective height, or the bottom of the image.
	*/

	const double pmax = (1.0 - peak.max) * 0.5 * effective_height;
	const double pmin = (1.0 - peak.min) * 0.5 * effective_height;

	/* remember that the bottom of the image (pmin) has larger y-coordinates
	   than the top (pmax).
	*/

	double spread = (pmin - pmax) * 0.5;

	/* find the nearest pixel to the nominal center. */
	const double center = round (pmin - spread);

	if (spread < 1.0) {
		/* minimum distance between line ends is 1 pixel, and we want it "centered" on a pixel,
		   as per cairo single-pixel line issues.

		   NOTE: the caller will not draw a line between these two points if the spread is
		   less than 2 pixels. So only the tips.top value matters, which is where we will
		   draw a single pixel as part of the outline.
		 */
		tips.top = center;
		tips.bot = center + 1.0;
	} else {
		/* round spread above and below center to an integer number of pixels */
		spread = round (spread);
		/* top and bottom are located equally either side of the center */
		tips.top = center - spread;
		tips.bot = center + spread;
	}

	tips.top = min (effective_height, max (0.0, tips.top));
	tips.bot = min (effective_height, max (0.0, tips.bot));
}


Coord
WaveView::y_extent (double s) const
{
	assert (_shape == Rectified);
	return floor ((1.0 - s) * _height);
}

void
WaveView::draw_absent_image (Cairo::RefPtr<Cairo::ImageSurface>& image, PeakData* _peaks, int n_peaks) const
{
	Cairo::RefPtr<Cairo::ImageSurface> stripe = Cairo::ImageSurface::create (Cairo::FORMAT_A8, n_peaks, _height);

	Cairo::RefPtr<Cairo::Context> stripe_context = Cairo::Context::create (stripe);
	stripe_context->set_antialias (Cairo::ANTIALIAS_NONE);

	uint32_t stripe_separation = 150;
	double start = - floor (_height / stripe_separation) * stripe_separation;
	int stripe_x = 0;

	while (start < n_peaks) {

		stripe_context->move_to (start, 0);
		stripe_x = start + _height;
		stripe_context->line_to (stripe_x, _height);
		start += stripe_separation;
	}

	stripe_context->set_source_rgba (1.0, 1.0, 1.0, 1.0);
	stripe_context->set_line_cap (Cairo::LINE_CAP_SQUARE);
	stripe_context->set_line_width(50);
	stripe_context->stroke();

	Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create (image);

	context->set_source_rgba (1.0, 1.0, 0.0, 0.3);
	context->mask (stripe, 0, 0);
	context->fill ();
}

struct ImageSet {
	Cairo::RefPtr<Cairo::ImageSurface> wave;
	Cairo::RefPtr<Cairo::ImageSurface> outline;
	Cairo::RefPtr<Cairo::ImageSurface> clip;
	Cairo::RefPtr<Cairo::ImageSurface> zero;

	ImageSet() :
		wave (0), outline (0), clip (0), zero (0) {}
};

void
WaveView::draw_image (Cairo::RefPtr<Cairo::ImageSurface>& image, PeakData* _peaks, int n_peaks, boost::shared_ptr<WaveViewThreadRequest> req) const
{

	ImageSet images;

	images.wave = Cairo::ImageSurface::create (Cairo::FORMAT_A8, n_peaks, _height);
	images.outline = Cairo::ImageSurface::create (Cairo::FORMAT_A8, n_peaks, _height);
	images.clip = Cairo::ImageSurface::create (Cairo::FORMAT_A8, n_peaks, _height);
	images.zero = Cairo::ImageSurface::create (Cairo::FORMAT_A8, n_peaks, _height);

	Cairo::RefPtr<Cairo::Context> wave_context = Cairo::Context::create (images.wave);
	Cairo::RefPtr<Cairo::Context> outline_context = Cairo::Context::create (images.outline);
	Cairo::RefPtr<Cairo::Context> clip_context = Cairo::Context::create (images.clip);
	Cairo::RefPtr<Cairo::Context> zero_context = Cairo::Context::create (images.zero);
	wave_context->set_antialias (Cairo::ANTIALIAS_NONE);
	outline_context->set_antialias (Cairo::ANTIALIAS_NONE);
	clip_context->set_antialias (Cairo::ANTIALIAS_NONE);
	zero_context->set_antialias (Cairo::ANTIALIAS_NONE);

	boost::scoped_array<LineTips> tips (new LineTips[n_peaks]);

	/* Clip level nominally set to -0.9dBFS to account for inter-sample
	   interpolation possibly clipping (value may be too low).

	   We adjust by the region's own gain (but note: not by any gain
	   automation or its gain envelope) so that clip indicators are closer
	   to providing data about on-disk data. This multiplication is
	   needed because the data we get from AudioRegion::read_peaks()
	   has been scaled by scale_amplitude() already.
	*/

	const double clip_level = _clip_level * _region_amplitude;

	if (_shape == WaveView::Rectified) {

		/* each peak is a line from the bottom of the waveview
		 * to a point determined by max (_peaks[i].max,
		 * _peaks[i].min)
		 */

		if (_logscaled) {
			for (int i = 0; i < n_peaks; ++i) {

				tips[i].bot = height() - 1.0;
				const double p = alt_log_meter (fast_coefficient_to_dB (max (fabs (_peaks[i].max), fabs (_peaks[i].min))));
				tips[i].top = y_extent (p);
				tips[i].spread = p * _height;

				if (_peaks[i].max >= clip_level) {
					tips[i].clip_max = true;
				}

				if (-(_peaks[i].min) >= clip_level) {
					tips[i].clip_min = true;
				}
			}

		} else {
			for (int i = 0; i < n_peaks; ++i) {

				tips[i].bot = height() - 1.0;
				const double p = max(fabs (_peaks[i].max), fabs (_peaks[i].min));
				tips[i].top = y_extent (p);
				tips[i].spread = p * _height;
				if (p >= clip_level) {
					tips[i].clip_max = true;
				}
			}

		}

	} else {

		if (_logscaled) {
			for (int i = 0; i < n_peaks; ++i) {
				PeakData p;
				p.max = _peaks[i].max;
				p.min = _peaks[i].min;

				if (_peaks[i].max >= clip_level) {
					tips[i].clip_max = true;
				}
				if (-(_peaks[i].min) >= clip_level) {
					tips[i].clip_min = true;
				}

				if (p.max > 0.0) {
					p.max = alt_log_meter (fast_coefficient_to_dB (p.max));
				} else if (p.max < 0.0) {
					p.max =-alt_log_meter (fast_coefficient_to_dB (-p.max));
				} else {
					p.max = 0.0;
				}

				if (p.min > 0.0) {
					p.min = alt_log_meter (fast_coefficient_to_dB (p.min));
				} else if (p.min < 0.0) {
					p.min = -alt_log_meter (fast_coefficient_to_dB (-p.min));
				} else {
					p.min = 0.0;
				}

				compute_tips (p, tips[i]);
				tips[i].spread = tips[i].bot - tips[i].top;
			}

		} else {
			for (int i = 0; i < n_peaks; ++i) {
				if (_peaks[i].max >= clip_level) {
					tips[i].clip_max = true;
				}
				if (-(_peaks[i].min) >= clip_level) {
					tips[i].clip_min = true;
				}

				compute_tips (_peaks[i], tips[i]);
				tips[i].spread = tips[i].bot - tips[i].top;
			}

		}
	}

	if (req->should_stop()) {
		return;
	}

	Color alpha_one = rgba_to_color (0, 0, 0, 1.0);

	set_source_rgba (wave_context, alpha_one);
	set_source_rgba (outline_context, alpha_one);
	set_source_rgba (clip_context, alpha_one);
	set_source_rgba (zero_context, alpha_one);

	/* ensure single-pixel lines */

	wave_context->set_line_width (1.0);
	wave_context->translate (0.5, 0.5);

	outline_context->set_line_width (1.0);
	outline_context->translate (0.5, 0.5);

	clip_context->set_line_width (1.0);
	clip_context->translate (0.5, 0.5);

	zero_context->set_line_width (1.0);
	zero_context->translate (0.5, 0.5);

	/* the height of the clip-indicator should be at most 7 pixels,
	 * or 5% of the height of the waveview item.
	 */

	const double clip_height = min (7.0, ceil (_height * 0.05));

	/* There are 3 possible components to draw at each x-axis position: the
	   waveform "line", the zero line and an outline/clip indicator.  We
	   have to decide which of the 3 to draw at each position, pixel by
	   pixel. This makes the rendering less efficient but it is the only
	   way I can see to do this correctly.

	   To avoid constant source swapping and stroking, we draw the components separately
	   onto four alpha only image surfaces for use as a mask.

	   With only 1 pixel of spread between the top and bottom of the line,
	   we just draw the upper outline/clip indicator.

	   With 2 pixels of spread, we draw the upper and lower outline clip
	   indicators.

	   With 3 pixels of spread we draw the upper and lower outline/clip
	   indicators and at least 1 pixel of the waveform line.

	   With 5 pixels of spread, we draw all components.

	   We can do rectified as two separate passes because we have a much
	   easier decision regarding whether to draw the waveform line. We
	   always draw the clip/outline indicators.
	*/

	if (_shape == WaveView::Rectified) {

		for (int i = 0; i < n_peaks; ++i) {

			/* waveform line */

			if (tips[i].spread >= 1.0) {
				wave_context->move_to (i, tips[i].top);
				wave_context->line_to (i, tips[i].bot);
			}

			/* clip indicator */

			if (_global_show_waveform_clipping && (tips[i].clip_max || tips[i].clip_min)) {
				clip_context->move_to (i, tips[i].top);
				/* clip-indicating upper terminal line */
				clip_context->rel_line_to (0, min (clip_height, ceil(tips[i].spread + .5)));
			} else {
				outline_context->move_to (i, tips[i].top);
				/* normal upper terminal dot */
				outline_context->rel_line_to (0, -1.0);
			}
		}

		wave_context->stroke ();
		clip_context->stroke ();
		outline_context->stroke ();

	} else {
		const int height_zero = floor( _height * .5);

		for (int i = 0; i < n_peaks; ++i) {

			/* waveform line */

			if (tips[i].spread >= 2.0) {
				wave_context->move_to (i, tips[i].top);
				wave_context->line_to (i, tips[i].bot);
			}

			/* draw square waves and other discontiguous points clearly */
			if (i > 0) {
				if (tips[i-1].top + 2 < tips[i].top) {
					wave_context->move_to (i-1, tips[i-1].top);
					wave_context->line_to (i-1, (tips[i].bot + tips[i-1].top)/2);
					wave_context->move_to (i, (tips[i].bot + tips[i-1].top)/2);
					wave_context->line_to (i, tips[i].top);
				} else if (tips[i-1].bot > tips[i].bot + 2) {
					wave_context->move_to (i-1, tips[i-1].bot);
					wave_context->line_to (i-1, (tips[i].top + tips[i-1].bot)/2);
					wave_context->move_to (i, (tips[i].top + tips[i-1].bot)/2);
					wave_context->line_to (i, tips[i].bot);
				}
			}

			/* zero line, show only if there is enough spread
			or the waveform line does not cross zero line */

			if (show_zero_line() && ((tips[i].spread >= 5.0) || (tips[i].top > height_zero ) || (tips[i].bot < height_zero)) ) {
				zero_context->move_to (i, height_zero);
				zero_context->rel_line_to (1.0, 0);
			}

			if (tips[i].spread > 1.0) {
				bool clipped = false;
				/* outline/clip indicators */
				if (_global_show_waveform_clipping && tips[i].clip_max) {
					clip_context->move_to (i, tips[i].top);
					/* clip-indicating upper terminal line */
					clip_context->rel_line_to (0, min (clip_height, ceil(tips[i].spread + 0.5)));
					clipped = true;
				}

				if (_global_show_waveform_clipping && tips[i].clip_min) {
					clip_context->move_to (i, tips[i].bot);
					/* clip-indicating lower terminal line */
					clip_context->rel_line_to (0, - min (clip_height, ceil(tips[i].spread + 0.5)));
					clipped = true;
				}

				if (!clipped && tips[i].spread > 2.0) {
					/* only draw the outline if the spread
					   implies 3 or more pixels (so that we see 1
					   white pixel in the middle).
					*/
					outline_context->move_to (i, tips[i].bot);
					/* normal lower terminal dot; line moves up */
					outline_context->rel_line_to (0, -1.0);

					outline_context->move_to (i, tips[i].top);
					/* normal upper terminal dot, line moves down */
					outline_context->rel_line_to (0, 1.0);
				}
			} else {
				bool clipped = false;
				/* outline/clip indicator */
				if (_global_show_waveform_clipping && (tips[i].clip_max || tips[i].clip_min)) {
					clip_context->move_to (i, tips[i].top);
					/* clip-indicating upper / lower terminal line */
					clip_context->rel_line_to (0, 1.0);
					clipped = true;
				}

				if (!clipped) {
					/* special case where only 1 pixel of
					 * the waveform line is drawn (and
					 * nothing else).
					 *
					 * we draw a 1px "line", pretending
					 * that the span is 1.0 (whether it is
					 * zero or 1.0)
					 */
					wave_context->move_to (i, tips[i].top);
					wave_context->rel_line_to (0, 1.0);
				}
			}
		}

		wave_context->stroke ();
		outline_context->stroke ();
		clip_context->stroke ();
		zero_context->stroke ();
	}

	if (req->should_stop()) {
		return;
	}

	Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create (image);

	/* Here we set a source colour and use the various components as a mask. */

	if (gradient_depth() != 0.0) {

		Cairo::RefPtr<Cairo::LinearGradient> gradient (Cairo::LinearGradient::create (0, 0, 0, _height));

		double stops[3];

		double r, g, b, a;

		if (_shape == Rectified) {
			stops[0] = 0.1;
			stops[1] = 0.3;
			stops[2] = 0.9;
		} else {
			stops[0] = 0.1;
			stops[1] = 0.5;
			stops[2] = 0.9;
		}

		color_to_rgba (_fill_color, r, g, b, a);
		gradient->add_color_stop_rgba (stops[1], r, g, b, a);
		/* generate a new color for the middle of the gradient */
		double h, s, v;
		color_to_hsv (_fill_color, h, s, v);
		/* change v towards white */
		v *= 1.0 - gradient_depth();
		Color center = hsva_to_color (h, s, v, a);
		color_to_rgba (center, r, g, b, a);

		gradient->add_color_stop_rgba (stops[0], r, g, b, a);
		gradient->add_color_stop_rgba (stops[2], r, g, b, a);

		context->set_source (gradient);
	} else {
		set_source_rgba (context, _fill_color);
	}

	if (req->should_stop()) {
		return;
	}

	context->mask (images.wave, 0, 0);
	context->fill ();

	set_source_rgba (context, _outline_color);
	context->mask (images.outline, 0, 0);
	context->fill ();

	set_source_rgba (context, _clip_color);
	context->mask (images.clip, 0, 0);
	context->fill ();

	set_source_rgba (context, _zero_color);
	context->mask (images.zero, 0, 0);
	context->fill ();
}

boost::shared_ptr<WaveViewCache::Entry>
WaveView::cache_request_result (boost::shared_ptr<WaveViewThreadRequest> req) const
{
	if (!req->image) {
		// cerr << "asked to cache null image!!!\n";
		return boost::shared_ptr<WaveViewCache::Entry> ();
	}

	boost::shared_ptr<WaveViewCache::Entry> ret (new WaveViewCache::Entry (req->channel,
	                                                                       req->height,
	                                                                       req->amplitude,
	                                                                       req->fill_color,
	                                                                       req->samples_per_pixel,
	                                                                       req->start,
	                                                                       req->end,
	                                                                       req->image));
	images->add (_region->audio_source (_channel), ret);

	/* consolidate cache first (removes fully-contained
	 * duplicate images)
	 */

	images->consolidate_image_cache (_region->audio_source (_channel),
	                                 req->channel, req->height, req->amplitude,
	                                 req->fill_color, req->samples_per_pixel);

	return ret;
}

boost::shared_ptr<WaveViewCache::Entry>
WaveView::get_image (framepos_t start, framepos_t end, bool& full_image) const
{
	boost::shared_ptr<WaveViewCache::Entry> ret;

	full_image = true;

	/* this is called from a ::render() call, when we need an image to
	   draw with.
	*/

	DEBUG_TRACE (DEBUG::WaveView, string_compose ("%1 needs image from %2 .. %3\n", name, start, end));


	{
		Glib::Threads::Mutex::Lock lmq (request_queue_lock);

		/* if there's a draw request outstanding, check to see if we
		 * have an image there. if so, use it (and put it in the cache
		 * while we're here.
		 */

		DEBUG_TRACE (DEBUG::WaveView, string_compose ("%1 CR %2 stop? %3 image %4\n", this, current_request,
					(current_request ? current_request->should_stop() : false),
					(current_request ? (current_request->image ? "yes" : "no") : "n/a")));

		if (current_request && !current_request->should_stop() && current_request->image) {

			/* put the image into the cache so that other
			 * WaveViews can use it if it is useful
			 */

			if (current_request->start <= start && current_request->end >= end) {

				ret.reset (new WaveViewCache::Entry (current_request->channel,
				                                     current_request->height,
				                                     current_request->amplitude,
				                                     current_request->fill_color,
				                                     current_request->samples_per_pixel,
				                                     current_request->start,
				                                     current_request->end,
				                                     current_request->image));

				cache_request_result (current_request);
				DEBUG_TRACE (DEBUG::WaveView, string_compose ("%1: got image from completed request, spans %2..%3\n",
				                                              name, current_request->start, current_request->end));
			}

			/* drop our handle on the current request */
			current_request.reset ();
		}
	}

	if (!ret) {

		/* no current image draw request, so look in the cache */

		ret = get_image_from_cache (start, end, full_image);
		DEBUG_TRACE (DEBUG::WaveView, string_compose ("%1: lookup from cache gave %2 (full %3)\n",
		                                              name, ret, full_image));

	}



	if (!ret || !full_image) {

#ifndef ENABLE_THREADED_WAVEFORM_RENDERING
		if (1)
#else
		if ((rendered && get_image_in_thread) || always_get_image_in_thread)
#endif
		{

			DEBUG_TRACE (DEBUG::WaveView, string_compose ("%1: generating image in caller thread\n", name));

			boost::shared_ptr<WaveViewThreadRequest> req (new WaveViewThreadRequest);

			req->type = WaveViewThreadRequest::Draw;
			req->start = start;
			req->end = end;
			req->samples_per_pixel = _samples_per_pixel;
			req->region = _region; /* weak ptr, to avoid storing a reference in the request queue */
			req->channel = _channel;
			req->height = _height;
			req->fill_color = _fill_color;
			req->amplitude = _region_amplitude * _amplitude_above_axis;
			req->width = desired_image_width ();

			/* draw image in this (the GUI thread) */

			generate_image (req, false);

			/* cache the result */

			ret = cache_request_result (req);

			/* reset this so that future missing images are
			 * generated in a a worker thread.
			 */

			get_image_in_thread = false;

		} else {
			queue_get_image (_region, start, end);
		}
	}

	if (ret) {
		DEBUG_TRACE (DEBUG::WaveView, string_compose ("%1 got an image from %2 .. %3 (full ? %4)\n", name, ret->start, ret->end, full_image));
	} else {
		DEBUG_TRACE (DEBUG::WaveView, string_compose ("%1 no useful image available\n", name));
	}

	return ret;
}

boost::shared_ptr<WaveViewCache::Entry>
WaveView::get_image_from_cache (framepos_t start, framepos_t end, bool& full) const
{
	if (!images) {
		return boost::shared_ptr<WaveViewCache::Entry>();
	}

	return images->lookup_image (_region->audio_source (_channel), start, end, _channel,
	                             _height, _region_amplitude * _amplitude_above_axis, _fill_color, _samples_per_pixel, full);
}

framecnt_t
WaveView::desired_image_width () const
{
	/* compute how wide the image should be, in samples.
	 *
	 * We want at least 1 canvas width's worth, but if that
	 * represents less than 1/10th of a second, use 1/10th of
	 * a second instead.
	 */

	framecnt_t canvas_width_samples = _canvas->visible_area().width() * _samples_per_pixel;
	const framecnt_t one_tenth_of_second = _region->session().frame_rate() / 10;

	if (canvas_width_samples > one_tenth_of_second) {
		return  canvas_width_samples;
	}

	return one_tenth_of_second;
}

void
WaveView::queue_get_image (boost::shared_ptr<const ARDOUR::Region> region, framepos_t start, framepos_t end) const
{
	boost::shared_ptr<WaveViewThreadRequest> req (new WaveViewThreadRequest);

	req->type = WaveViewThreadRequest::Draw;
	req->start = start;
	req->end = end;
	req->samples_per_pixel = _samples_per_pixel;
	req->region = _region; /* weak ptr, to avoid storing a reference in the request queue */
	req->channel = _channel;
	req->height = _height;
	req->fill_color = _fill_color;
	req->amplitude = _region_amplitude * _amplitude_above_axis;
	req->width = desired_image_width ();

	if (current_request) {
		/* this will stop rendering in progress (which might otherwise
		   be long lived) for any current request.
		*/
		Glib::Threads::Mutex::Lock lm (request_queue_lock);
		if (current_request) {
			current_request->cancel ();
		}
	}

	start_drawing_thread ();

	/* swap requests (protected by lock) */

	{
		Glib::Threads::Mutex::Lock lm (request_queue_lock);
		current_request = req;

		DEBUG_TRACE (DEBUG::WaveView, string_compose ("%1 now has current request %2\n", this, req));

		if (request_queue.insert (this).second) {
			/* this waveview was not already in the request queue, make sure we wake
				 the rendering thread in case it is asleep.
				 */
			request_cond.signal ();
		}
	}
}

void
WaveView::generate_image (boost::shared_ptr<WaveViewThreadRequest> req, bool in_render_thread) const
{
	if (!req->should_stop()) {

		/* sample position is canonical here, and we want to generate
		 * an image that spans about 3x the canvas width. We get to that
		 * width by using an image sample count of the screen width added
		 * on each side of the desired image center.
		 */

		const framepos_t center = req->start + ((req->end - req->start) / 2);
		const framecnt_t image_samples = req->width;

		/* we can request data from anywhere in the Source, between 0 and its length
		 */

		framepos_t sample_start = max (_region_start, (center - image_samples));
		framepos_t sample_end = min (center + image_samples, region_end());
		const int n_peaks = std::max (1LL, llrint (ceil ((sample_end - sample_start) / (req->samples_per_pixel))));

		assert (n_peaks > 0 && n_peaks < 32767);

		boost::scoped_array<ARDOUR::PeakData> peaks (new PeakData[n_peaks]);

		/* Note that Region::read_peaks() takes a start position based on an
		   offset into the Region's **SOURCE**, rather than an offset into
		   the Region itself.
		*/

		framecnt_t peaks_read = _region->read_peaks (peaks.get(), n_peaks,
		                                             sample_start, sample_end - sample_start,
		                                             req->channel,
		                                             req->samples_per_pixel);

		if (req->should_stop()) {
			// cerr << "Request stopped after reading peaks\n";
			return;
		}

		req->image = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, n_peaks, req->height);

		// http://cairographics.org/manual/cairo-Image-Surfaces.html#cairo-image-surface-create
		// This function always returns a valid pointer, but it will return a pointer to a "nil" surface..
		// but there's some evidence that req->image can be NULL.
		// http://tracker.ardour.org/view.php?id=6478
		assert (req->image);

		/* make sure we record the sample positions that were actually used */
		req->start = sample_start;
		req->end = sample_end;

		if (peaks_read > 0) {

			/* region amplitude will have been used to generate the
			 * peak values already, but not the visual-only
			 * amplitude_above_axis. So apply that here before
			 * rendering.
			 */

			if (_amplitude_above_axis != 1.0) {
				for (framecnt_t i = 0; i < n_peaks; ++i) {
					peaks[i].max *= _amplitude_above_axis;
					peaks[i].min *= _amplitude_above_axis;
				}
			}

			draw_image (req->image, peaks.get(), n_peaks, req);
		} else {
			draw_absent_image (req->image, peaks.get(), n_peaks);
		}
	} else {
		// cerr << "Request stopped before image generation\n";
	}

	if (in_render_thread && !req->should_stop()) {
		DEBUG_TRACE (DEBUG::WaveView, string_compose ("done with request for %1 at %2 CR %3 req %4 range %5 .. %6\n", this, g_get_monotonic_time(), current_request, req, req->start, req->end));
		const_cast<WaveView*>(this)->ImageReady (); /* emit signal */
	}

	return;
}

/** Given a waveform that starts at window x-coordinate @param wave_origin
 * and the first pixel that we will actually draw @param draw_start, return
 * the offset into an image of the entire waveform that we will need to use.
 *
 * Note: most of our cached images are NOT of the entire waveform, this is just
 * computationally useful when determining which the sample range span for
 * the image we need.
 */
static inline double
window_to_image (double wave_origin, double image_start)
{
	return image_start - wave_origin;
}

void
WaveView::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	assert (_samples_per_pixel != 0);

	if (!_region) {
		return;
	}

	DEBUG_TRACE (DEBUG::WaveView, string_compose ("render %1 at %2\n", this, g_get_monotonic_time()));

	/* a WaveView is intimately connected to an AudioRegion. It will
	 * display the waveform within the region, anywhere from the start of
	 * the region to its end.
	 *
	 * the area we've been aked to render may overlap with area covered
	 * by the region in any of the normal ways:
	 *
	 *  - it may begin and end within the area covered by the region
	 *  - it may start before and end after the area covered by region
	 *  - it may start before and end within the area covered by the region
	 *  - it may start within and end after the area covered by the region
	 *  - it may be precisely coincident with the area covered by region.
	 *
	 * So let's start by determining the area covered by the region, in
	 * window coordinates. It begins at zero (in item coordinates for this
	 * waveview, and extends to region_length() / _samples_per_pixel.
	 */

	Rect self = item_to_window (Rect (0.0, 0.0, region_length() / _samples_per_pixel, _height));

	// cerr << name << " RENDER " << area << " self = " << self << endl;

	/* Now lets get the intersection with the area we've been asked to draw */

	Rect d = self.intersection (area);

	if (!d) {
		return;
	}

	Rect draw = d;

	/* "draw" is now a rectangle that defines the rectangle we need to
	 * update/render the waveview into, in window coordinate space.
	 */

	/* window coordinates - pixels where x=0 is the left edge of the canvas
	 * window. We round down in case we were asked to
	 * draw "between" pixels at the start and/or end.
	 */

	double draw_start = floor (draw.x0);
	const double draw_end = floor (draw.x1);

	// cerr << "Need to draw " << draw_start << " .. " << draw_end << " vs. " << area << " and self = " << self << endl;

	/* image coordnates: pixels where x=0 is the start of this waveview,
	 * wherever it may be positioned. thus image_start=N means "an image
	 * that begins N pixels after the start of region that this waveview is
	 * representing.
	 */

	const framepos_t image_start = window_to_image (self.x0, draw_start);
	const framepos_t image_end = window_to_image (self.x0, draw_end);

	// cerr << "Image/WV space: " << image_start << " .. " << image_end << endl;

	/* sample coordinates - note, these are not subject to rounding error
	 *
	 * "sample_start = N" means "the first sample we need to represent is N
	 * samples after the first sample of the region"
	 */

	framepos_t sample_start = _region_start + (image_start * _samples_per_pixel);
	framepos_t sample_end   = _region_start + (image_end * _samples_per_pixel);

	// cerr << "Sample space: " << sample_start << " .. " << sample_end << " @ " << _samples_per_pixel << " rs = " << _region_start << endl;

	/* sample_start and sample_end are bounded by the region
	 * limits. sample_start, because of the was just computed, must already
	 * be greater than or equal to the _region_start value.
	 */

	sample_end = min (region_end(), sample_end);

	// cerr << debug_name() << " will need image spanning " << sample_start << " .. " << sample_end << " region spans " << _region_start << " .. " << region_end() << endl;

	double image_origin_in_self_coordinates;
	boost::shared_ptr<WaveViewCache::Entry> image_to_draw;

	Glib::Threads::Mutex::Lock lci (current_image_lock);
	if (_current_image) {

		/* check it covers the right sample range */

		if (_current_image->start > sample_start || _current_image->end < sample_end) {
			/* doesn't cover the area we need ... reset */
			_current_image.reset ();
		} else {
			/* timestamp our continuing use of this image/cache entry */
			images->use (_region->audio_source (_channel), _current_image);
			image_to_draw = _current_image;
		}
	}

	if (!image_to_draw) {

		/* look it up */

		bool full_image;
		image_to_draw = get_image (sample_start, sample_end, full_image);

		DEBUG_TRACE (DEBUG::WaveView, string_compose ("%1 image to draw = %2 (full? %3)\n", name, image_to_draw, full_image));

		if (!image_to_draw) {
			/* image not currently available. A redraw will be scheduled
			   when it is ready.
			*/
			return;
		}

		if (full_image) {
			/* found an image that covers our entire sample range,
			 * so keep a reference to it.
			 */
			_current_image = image_to_draw;
		}
	}

	/* compute the first pixel of the image that should be used when we
	 * render the specified range.
	 */

	image_origin_in_self_coordinates = (image_to_draw->start - _region_start) / _samples_per_pixel;

	if (_start_shift && (sample_start == _region_start) && (self.x0 == draw.x0)) {
		/* we are going to draw the first pixel for this region, but
		   we may not want this to overlap a border around the
		   waveform. If so, _start_shift will be set.
		*/
		//cerr << name.substr (23) << " ss = " << sample_start << " rs = " << _region_start << " sf = " << _start_shift << " ds = " << draw_start << " self = " << self << " draw = " << draw << endl;
		//draw_start += _start_shift;
		//image_origin_in_self_coordinates += _start_shift;
	}

	/* the image may only be a best-effort ... it may not span the entire
	 * range requested, though it is guaranteed to cover the start. So
	 * determine how many pixels we can actually draw.
	 */

	double draw_width;

	if (image_to_draw != _current_image) {
		lci.release ();

		/* the image is guaranteed to start at or before
		 * draw_start. But if it starts before draw_start, that reduces
		 * the maximum available width we can render with.
		 *
		 * so .. clamp the draw width to the smaller of what we need to
		 * draw or the available width of the image.
		 */

		draw_width = min ((double) image_to_draw->image->get_width(), (draw_end - draw_start));


		DEBUG_TRACE (DEBUG::WaveView, string_compose ("%1 draw just %2 of %3 @ %8 (iwidth %4 off %5 img @ %6 rs @ %7)\n", name, draw_width, (draw_end - draw_start),
		                                              image_to_draw->image->get_width(), image_origin_in_self_coordinates,
		                                              image_to_draw->start, _region_start, draw_start));
	} else {
		draw_width = draw_end - draw_start;
		DEBUG_TRACE (DEBUG::WaveView, string_compose ("use current image, span entire render width %1..%2\n", draw_start, draw_end));
	}

	context->rectangle (draw_start, draw.y0, draw_width, draw.height());

	/* round image origin position to an exact pixel in device space to
	 * avoid blurring
	 */

	double x  = self.x0 + image_origin_in_self_coordinates;
	double y  = self.y0;
	context->user_to_device (x, y);
	x = round (x);
	y = round (y);
	context->device_to_user (x, y);

	/* the coordinates specify where in "user coordinates" (i.e. what we
	 * generally call "canvas coordinates" in this code) the image origin
	 * will appear. So specifying (10,10) will put the upper left corner of
	 * the image at (10,10) in user space.
	 */

	context->set_source (image_to_draw->image, x, y);
	context->fill ();

	/* image obtained, some of it painted to display: we are rendered.
	   Future calls to get_image_in_thread are now meaningful.
	*/

	rendered = true;
}

void
WaveView::compute_bounding_box () const
{
	if (_region) {
		_bounding_box = Rect (0.0, 0.0, region_length() / _samples_per_pixel, _height);
	} else {
		_bounding_box = Rect ();
	}

	_bounding_box_dirty = false;
}

void
WaveView::set_height (Distance height)
{
	if (height != _height) {
		begin_change ();

		invalidate_image_cache ();
		_height = height;
		get_image_in_thread = true;

		_bounding_box_dirty = true;
		end_change ();
	}
}

void
WaveView::set_channel (int channel)
{
	if (channel != _channel) {
		begin_change ();

		invalidate_image_cache ();
		_channel = channel;

		_bounding_box_dirty = true;
		end_change ();
	}
}

void
WaveView::set_logscaled (bool yn)
{
	if (_logscaled != yn) {
		begin_visual_change ();
		invalidate_image_cache ();
		_logscaled = yn;
		end_visual_change ();
	}
}

void
WaveView::gain_changed ()
{
	begin_visual_change ();
	invalidate_image_cache ();
	_region_amplitude = _region->scale_amplitude ();
	get_image_in_thread = true;
	end_visual_change ();
}

void
WaveView::set_zero_color (Color c)
{
	if (_zero_color != c) {
		begin_visual_change ();
		invalidate_image_cache ();
		_zero_color = c;
		end_visual_change ();
	}
}

void
WaveView::set_clip_color (Color c)
{
	if (_clip_color != c) {
		begin_visual_change ();
		invalidate_image_cache ();
		_clip_color = c;
		end_visual_change ();
	}
}

void
WaveView::set_show_zero_line (bool yn)
{
	if (_show_zero != yn) {
		begin_visual_change ();
		invalidate_image_cache ();
		_show_zero = yn;
		end_visual_change ();
	}
}

void
WaveView::set_shape (Shape s)
{
	if (_shape != s) {
		begin_visual_change ();
		invalidate_image_cache ();
		_shape = s;
		end_visual_change ();
	}
}

void
WaveView::set_amplitude_above_axis (double a)
{
	if (fabs (_amplitude_above_axis - a) > 0.01) {
		begin_visual_change ();
		invalidate_image_cache ();
		_amplitude_above_axis = a;
		get_image_in_thread = true;
		end_visual_change ();
	}
}

void
WaveView::set_global_shape (Shape s)
{
	if (_global_shape != s) {
		_global_shape = s;
		if (images) {
			images->clear_cache ();
		}
		VisualPropertiesChanged (); /* EMIT SIGNAL */
	}
}

void
WaveView::set_global_logscaled (bool yn)
{
	if (_global_logscaled != yn) {
		_global_logscaled = yn;
		if (images) {
			images->clear_cache ();
		}
		VisualPropertiesChanged (); /* EMIT SIGNAL */
	}
}

framecnt_t
WaveView::region_length() const
{
	return _region->length() - (_region_start - _region->start());
}

framepos_t
WaveView::region_end() const
{
	return _region_start + region_length();
}

void
WaveView::set_region_start (frameoffset_t start)
{
	if (!_region) {
		return;
	}

	if (_region_start == start) {
		return;
	}

	begin_change ();
	_region_start = start;
	_bounding_box_dirty = true;
	end_change ();
}

void
WaveView::region_resized ()
{
	/* Called when the region start or end (thus length) has changed.
	*/

	if (!_region) {
		return;
	}

	begin_change ();
	_region_start = _region->start();
	_bounding_box_dirty = true;
	end_change ();
}

void
WaveView::set_global_gradient_depth (double depth)
{
	if (_global_gradient_depth != depth) {
		_global_gradient_depth = depth;
		VisualPropertiesChanged (); /* EMIT SIGNAL */
	}
}

void
WaveView::set_global_show_waveform_clipping (bool yn)
{
	if (_global_show_waveform_clipping != yn) {
		_global_show_waveform_clipping = yn;
		ClipLevelChanged ();
	}
}

void
WaveView::set_start_shift (double pixels)
{
	if (pixels < 0) {
		return;
	}

	begin_visual_change ();
	_start_shift = pixels;
	end_visual_change ();
}

void
WaveView::cancel_my_render_request () const
{
	if (!images) {
		return;
	}

	/* try to stop any current rendering of the request, or prevent it from
	 * ever starting up.
	 */

	Glib::Threads::Mutex::Lock lm (request_queue_lock);

	if (current_request) {
		current_request->cancel ();
	}

	/* now remove it from the queue and reset our request pointer so that
	   have no outstanding request (that we know about)
	*/

	request_queue.erase (this);
	current_request.reset ();
	DEBUG_TRACE (DEBUG::WaveView, string_compose ("%1 now has no request %2\n", this));

}

void
WaveView::set_image_cache_size (uint64_t sz)
{
	if (!images) {
		images = new WaveViewCache;
	}

	images->set_image_cache_threshold (sz);
}

/*-------------------------------------------------*/

void
WaveView::start_drawing_thread ()
{
	if (!_drawing_thread) {
		_drawing_thread = Glib::Threads::Thread::create (sigc::ptr_fun (WaveView::drawing_thread));
	}
}

void
WaveView::stop_drawing_thread ()
{
	while (_drawing_thread) {
		Glib::Threads::Mutex::Lock lm (request_queue_lock);
		g_atomic_int_set (&drawing_thread_should_quit, 1);
		request_cond.signal ();
	}
}

void
WaveView::drawing_thread ()
{
	using namespace Glib::Threads;

	WaveView const * requestor;
	Mutex::Lock lm (request_queue_lock);
	bool run = true;

	while (run) {

		/* remember that we hold the lock at this point, no matter what */

		if (g_atomic_int_get (&drawing_thread_should_quit)) {
			break;
		}

		if (request_queue.empty()) {
			request_cond.wait (request_queue_lock);
		}

		if (request_queue.empty()) {
			continue;
		}

		/* remove the request from the queue (remember: the "request"
		 * is just a pointer to a WaveView object)
		 */

		requestor = *(request_queue.begin());
		request_queue.erase (request_queue.begin());

		DEBUG_TRACE (DEBUG::WaveView, string_compose ("start request for %1 at %2\n", requestor, g_get_monotonic_time()));

		boost::shared_ptr<WaveViewThreadRequest> req = requestor->current_request;

		if (!req) {
			continue;
		}

		/* Generate an image. Unlock the request queue lock
		 * while we do this, so that other things can happen
		 * as we do rendering.
		 */

		lm.release (); /* some RAII would be good here */

		try {
			requestor->generate_image (req, true);
		} catch (...) {
			req->image.clear(); /* just in case it was set before the exception, whatever it was */
		}

		lm.acquire ();

		req.reset (); /* drop/delete request as appropriate */
	}

	/* thread is vanishing */
	_drawing_thread = 0;
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


boost::shared_ptr<WaveViewCache::Entry>
WaveViewCache::lookup_image (boost::shared_ptr<ARDOUR::AudioSource> src,
                             framepos_t start, framepos_t end,
                             int channel,
                             Coord height,
                             float amplitude,
                             Color fill_color,
                             double samples_per_pixel,
                             bool& full_coverage)
{
	ImageCache::iterator x;

	if ((x = cache_map.find (src)) == cache_map.end ()) {
		/* nothing in the cache for this audio source at all */
		return boost::shared_ptr<WaveViewCache::Entry> ();
	}

	CacheLine& caches = x->second;
	boost::shared_ptr<Entry> best_partial;
	framecnt_t max_coverage = 0;

	/* Find a suitable ImageSurface, if it exists.
	*/

	for (CacheLine::iterator c = caches.begin(); c != caches.end(); ++c) {

		boost::shared_ptr<Entry> e (*c);

		if (channel != e->channel
		    || height != e->height
		    || amplitude != e->amplitude
		    || samples_per_pixel != e->samples_per_pixel
		    || fill_color != e->fill_color) {
			continue;
		}

		switch (Evoral::coverage (start, end, e->start, e->end)) {
			case Evoral::OverlapExternal:  /* required range is inside image range */
				DEBUG_TRACE (DEBUG::WaveView, string_compose ("found image spanning %1..%2 covers %3..%4\n",
							e->start, e->end, start, end));
				use (src, e);
				full_coverage = true;
				return e;

			case Evoral::OverlapStart: /* required range start is covered by image range */
				if ((e->end - start) > max_coverage) {
					best_partial = e;
					max_coverage = e->end - start;
				}
				break;

			case Evoral::OverlapNone:
			case Evoral::OverlapEnd:
			case Evoral::OverlapInternal:
				break;
		}
	}

	if (best_partial) {
		DEBUG_TRACE (DEBUG::WaveView, string_compose ("found PARTIAL image spanning %1..%2 partially covers %3..%4\n",
		                                              best_partial->start, best_partial->end, start, end));
		use (src, best_partial);
		full_coverage = false;
		return best_partial;
	}

	return boost::shared_ptr<Entry> ();
}

void
WaveViewCache::consolidate_image_cache (boost::shared_ptr<ARDOUR::AudioSource> src,
                                        int channel,
                                        Coord height,
                                        float amplitude,
                                        Color fill_color,
                                        double samples_per_pixel)
{
	list <uint32_t> deletion_list;
	uint32_t other_entries = 0;
	ImageCache::iterator x;

	/* MUST BE CALLED FROM (SINGLE) GUI THREAD */

	if ((x = cache_map.find (src)) == cache_map.end ()) {
		return;
	}

	CacheLine& caches  = x->second;

	for (CacheLine::iterator c1 = caches.begin(); c1 != caches.end(); ) {

		CacheLine::iterator nxt = c1;
		++nxt;

		boost::shared_ptr<Entry> e1 (*c1);

		if (channel != e1->channel
		    || height != e1->height
		    || amplitude != e1->amplitude
		    || samples_per_pixel != e1->samples_per_pixel
		    || fill_color != e1->fill_color) {

			/* doesn't match current properties, ignore and move on
			 * to the next one.
			 */

			other_entries++;
			c1 = nxt;
			continue;
		}

		/* c1 now points to a cached image entry that matches current
		 * properties. Check all subsequent cached imaged entries to
		 * see if there are others that also match but represent
		 * subsets of the range covered by this one.
		 */

		for (CacheLine::iterator c2 = c1; c2 != caches.end(); ) {

			CacheLine::iterator nxt2 = c2;
			++nxt2;

			boost::shared_ptr<Entry> e2 (*c2);

			if (e1 == e2 || channel != e2->channel
			    || height != e2->height
			    || amplitude != e2->amplitude
			    || samples_per_pixel != e2->samples_per_pixel
			    || fill_color != e2->fill_color) {

				/* properties do not match, ignore for the
				 * purposes of consolidation.
				 */
				c2 = nxt2;
				continue;
			}

			if (e2->start >= e1->start && e2->end <= e1->end) {
				/* c2 is fully contained by c1, so delete it */
				caches.erase (c2);

				/* and re-start the whole iteration */
				nxt = caches.begin ();
				break;
			}

			c2 = nxt2;
		}

		c1 = nxt;
	}
}

void
WaveViewCache::use (boost::shared_ptr<ARDOUR::AudioSource> src, boost::shared_ptr<Entry> ce)
{
	ce->timestamp = g_get_monotonic_time ();
}

void
WaveViewCache::add (boost::shared_ptr<ARDOUR::AudioSource> src, boost::shared_ptr<Entry> ce)
{
	/* MUST BE CALLED FROM (SINGLE) GUI THREAD */

	Cairo::RefPtr<Cairo::ImageSurface> img (ce->image);

	image_cache_size += img->get_height() * img->get_width () * 4; /* 4 = bytes per FORMAT_ARGB32 pixel */

	if (cache_full()) {
		cache_flush ();
	}

	ce->timestamp = g_get_monotonic_time ();

	cache_map[src].push_back (ce);
}

uint64_t
WaveViewCache::compute_image_cache_size()
{
	uint64_t total = 0;
	for (ImageCache::iterator s = cache_map.begin(); s != cache_map.end(); ++s) {
		CacheLine& per_source_cache (s->second);
		for (CacheLine::iterator c = per_source_cache.begin(); c != per_source_cache.end(); ++c) {
			Cairo::RefPtr<Cairo::ImageSurface> img ((*c)->image);
			total += img->get_height() * img->get_width() * 4; /* 4 = bytes per FORMAT_ARGB32 pixel */
		}
	}
	return total;
}

bool
WaveViewCache::cache_full()
{
	return image_cache_size > _image_cache_threshold;
}

void
WaveViewCache::cache_flush ()
{
	/* Build a sortable list of all cache entries */

	CacheList cache_list;

	for (ImageCache::const_iterator cm = cache_map.begin(); cm != cache_map.end(); ++cm) {
		for (CacheLine::const_iterator cl = cm->second.begin(); cl != cm->second.end(); ++cl) {
			cache_list.push_back (make_pair (cm->first, *cl));
		}
	}

	/* sort list in LRU order */
	SortByTimestamp sorter;
	sort (cache_list.begin(), cache_list.end(), sorter);

	while (image_cache_size > _image_cache_threshold && !cache_map.empty() && !cache_list.empty()) {

		ListEntry& le (cache_list.front());

		ImageCache::iterator x;

		if ((x = cache_map.find (le.first)) != cache_map.end ()) {

			CacheLine& cl  = x->second;

			for (CacheLine::iterator c = cl.begin(); c != cl.end(); ++c) {

				if (*c == le.second) {

					DEBUG_TRACE (DEBUG::WaveView, string_compose ("Removing cache line entry for %1\n", x->first->name()));

					/* Remove this entry from this cache line */
					cl.erase (c);

					if (cl.empty()) {
						/* remove cache line from main cache: no more entries */
						cache_map.erase (x);
					}

					break;
				}
			}

			Cairo::RefPtr<Cairo::ImageSurface> img (le.second->image);
			uint64_t size = img->get_height() * img->get_width() * 4; /* 4 = bytes per FORMAT_ARGB32 pixel */

			if (image_cache_size > size) {
				image_cache_size -= size;
			} else {
				image_cache_size = 0;
			}
			DEBUG_TRACE (DEBUG::WaveView, string_compose ("cache shrunk to %1\n", image_cache_size));
		}

		/* Remove from the linear list, even if we didn't find it in
		 * the actual cache_mao
		 */
		cache_list.erase (cache_list.begin());
	}
}

void
WaveViewCache::clear_cache ()
{
	DEBUG_TRACE (DEBUG::WaveView, "clear cache\n");
	const uint64_t image_cache_threshold = _image_cache_threshold;
	_image_cache_threshold = 0;
	cache_flush ();
	_image_cache_threshold = image_cache_threshold;
}

void
WaveViewCache::set_image_cache_threshold (uint64_t sz)
{
	DEBUG_TRACE (DEBUG::WaveView, string_compose ("new image cache size %1\n", sz));
	_image_cache_threshold = sz;
	cache_flush ();
}
