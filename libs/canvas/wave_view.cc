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

#include "canvas/wave_view.h"
#include "canvas/utils.h"
#include "canvas/canvas.h"
#include "canvas/colors.h"

#include <gdkmm/general.h>

#include "gtkmm2ext/gui_thread.h"

using namespace std;
using namespace ARDOUR;
using namespace ArdourCanvas;

double WaveView::_global_gradient_depth = 0.6;
bool WaveView::_global_logscaled = false;
WaveView::Shape WaveView::_global_shape = WaveView::Normal;
bool WaveView::_global_show_waveform_clipping = true;
double WaveView::_clip_level = 0.98853;

WaveViewCache* WaveView::images = 0;
gint WaveView::drawing_thread_should_quit = 0;
Glib::Threads::Mutex WaveView::request_queue_lock;
Glib::Threads::Cond WaveView::request_cond;
Glib::Threads::Thread* WaveView::_drawing_thread = 0;
WaveView::DrawingRequestQueue WaveView::request_queue;

PBD::Signal0<void> WaveView::VisualPropertiesChanged;
PBD::Signal0<void> WaveView::ClipLevelChanged;

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
	VisualPropertiesChanged.connect_same_thread (invalidation_connection, boost::bind (&WaveView::handle_visual_property_change, this));
	ClipLevelChanged.connect_same_thread (invalidation_connection, boost::bind (&WaveView::handle_clip_level_change, this));

	ImageReady.connect (image_ready_connection, MISSING_INVALIDATOR, boost::bind (&WaveView::image_ready, this), gui_context());
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
	, _region_start (region->start())
	, get_image_in_thread (false)
	, always_get_image_in_thread (false)
	, rendered (false)
{
	VisualPropertiesChanged.connect_same_thread (invalidation_connection, boost::bind (&WaveView::handle_visual_property_change, this));
	ClipLevelChanged.connect_same_thread (invalidation_connection, boost::bind (&WaveView::handle_clip_level_change, this));

	ImageReady.connect (image_ready_connection, MISSING_INVALIDATOR, boost::bind (&WaveView::image_ready, this), gui_context());
}

WaveView::~WaveView ()
{
	invalidate_image_cache ();
}

string
WaveView::debug_name() const
{
	return _region->name() + string (":") + PBD::to_string (_channel+1, std::dec);
}

void
WaveView::image_ready ()
{
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
	cancel_my_render_request ();
	_current_image.reset ();
}

Coord
WaveView::y_extent (double s, bool /*round_to_lower_edge*/) const
{
	/* it is important that this returns an integral value, so that we
	 * can ensure correct single pixel behaviour.
	 *
	 * we need (_height - max(wave_line_width))
	 * wave_line_width == 1 IFF top==bottom (1 sample per pixel or flat line)
	 * wave_line_width == 2 otherwise
	 * then round away from the zero line, towards peak
	 */
	if (_shape == Rectified) {
		// we only ever have 1 point and align to the bottom (not center)
		return floor ((1.0 - s) * (_height - 2.0));
	} else {
		/* currently canvas rectangle is off-by-one and we
		 * cannot draw a pixel at 0 (-.5 .. +.5) without it being
		 * clipped. A value 1.0 (ideally one point at y=0) ends
		 * up a pixel down. and a value of -1.0 (ideally y = _height-1)
		 * currently is on the bottom separator line :(
		 * So to make the complete waveform appear centered in
		 * a region, we translate by +.5 (instead of -.5)
		 * and waste two pixel of height: -4 (instad of -2)
		 *
		 * This needs fixing in canvas/rectangle the intersect
		 * functions and probably a couple of other places as well...
		 */
		Coord pos;
		if (s < 0) {
			pos = ceil  ((1.0 - s) * .5 * (_height - 4.0));
		} else {
			pos = floor ((1.0 - s) * .5 * (_height - 4.0));
		}
		return min (_height - 4.0, (max (0.0, pos)));
	}
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

struct LineTips {
	double top;
	double bot;
	double spread;
	bool clip_max;
	bool clip_min;

	LineTips() : top (0.0), bot (0.0), clip_max (false), clip_min (false) {}
};

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
				tips[i].top = y_extent (p, false);
				tips[i].spread = p * (_height - 1.0);

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
				tips[i].top = y_extent (p, false);
				tips[i].spread = p * (_height - 2.0);
				if (p >= clip_level) {
					tips[i].clip_max = true;
				}
			}

		}

	} else {

		if (_logscaled) {
			for (int i = 0; i < n_peaks; ++i) {
				double top = _peaks[i].max;
				double bot = _peaks[i].min;

				if (_peaks[i].max >= clip_level) {
						tips[i].clip_max = true;
				}
				if (-(_peaks[i].min) >= clip_level) {
					tips[i].clip_min = true;
				}

				if (top > 0.0) {
					top = alt_log_meter (fast_coefficient_to_dB (top));
				} else if (top < 0.0) {
					top =-alt_log_meter (fast_coefficient_to_dB (-top));
				} else {
					top = 0.0;
				}

				if (bot > 0.0) {
					bot = alt_log_meter (fast_coefficient_to_dB (bot));
				} else if (bot < 0.0) {
					bot = -alt_log_meter (fast_coefficient_to_dB (-bot));
				} else {
					bot = 0.0;
				}

				tips[i].top = y_extent (top, false);
				tips[i].bot = y_extent (bot, true);
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

				tips[i].top = y_extent (_peaks[i].max, false);
				tips[i].bot = y_extent (_peaks[i].min, true);
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
	wave_context->translate (0.5, +1.5);

	outline_context->set_line_width (1.0);
	outline_context->translate (0.5, +1.5);

	clip_context->set_line_width (1.0);
	clip_context->translate (0.5, +1.5);

	zero_context->set_line_width (1.0);
	zero_context->translate (0.5, +1.5);

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
		const double height_2 = (_height - 2.5) * .5;

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

			/* zero line */

			if (tips[i].spread >= 5.0 && show_zero_line()) {
				zero_context->move_to (i, floor(height_2));
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

				if (!clipped) {
					outline_context->move_to (i, tips[i].bot + 1.0);
					/* normal lower terminal dot */
					outline_context->rel_line_to (0, -1.0);

					outline_context->move_to (i, tips[i].top - 1.0);
					/* normal upper terminal dot */
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
					wave_context->move_to (i, tips[i].top);
					/* special case where outline only is drawn.
					 * we draw a 1px "line", pretending that the span is 1.0
					*/
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
	boost::shared_ptr<WaveViewCache::Entry> ret (new WaveViewCache::Entry (req->channel,
	                                                                       req->height,
	                                                                       req->region_amplitude,
	                                                                       req->fill_color,
	                                                                       req->samples_per_pixel,
	                                                                       req->start,
	                                                                       req->end,
	                                                                       req->image));
	if (!images) {
		images = new WaveViewCache;
	}

	images->add (_region->audio_source (_channel), ret);
	
	/* consolidate cache first (removes fully-contained
	 * duplicate images)
	 */
	
	images->consolidate_image_cache (_region->audio_source (_channel),
	                                 _channel, _height, _region_amplitude,
	                                 _fill_color, _samples_per_pixel);

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

	{
		Glib::Threads::Mutex::Lock lmq (request_queue_lock);

		/* if there's a draw request outstanding, check to see if we
		 * have an image there. if so, use it (and put it in the cache
		 * while we're here.
		 */
		
		if (current_request && !current_request->should_stop() && current_request->image) {

			/* put the image into the cache so that other
			 * WaveViews can use it if it is useful
			 */

			if (current_request->start <= start && current_request->end >= end) {
				
				ret.reset (new WaveViewCache::Entry (current_request->channel,
				                                     current_request->height,
				                                     current_request->region_amplitude,
				                                     current_request->fill_color,
				                                     current_request->samples_per_pixel,
				                                     current_request->start,
				                                     current_request->end,
				                                     current_request->image));
	
				cache_request_result (current_request);
			}

			/* drop our handle on the current request */
			current_request.reset ();
		}
	}

	if (!ret) {

		/* no current image draw request, so look in the cache */
		
		ret = get_image_from_cache (start, end, full_image);

	}

	if (!ret || !full_image) {

		if (get_image_in_thread || always_get_image_in_thread) {

			boost::shared_ptr<WaveViewThreadRequest> req (new WaveViewThreadRequest);

			req->type = WaveViewThreadRequest::Draw;
			req->start = start;
			req->end = end;
			req->samples_per_pixel = _samples_per_pixel;
			req->region = _region; /* weak ptr, to avoid storing a reference in the request queue */
			req->channel = _channel;
			req->width = _canvas->visible_area().width();
			req->height = _height;
			req->fill_color = _fill_color;
			req->region_amplitude = _region_amplitude;

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

	return ret;
}

boost::shared_ptr<WaveViewCache::Entry>
WaveView::get_image_from_cache (framepos_t start, framepos_t end, bool& full) const
{
	if (!images) {
		return boost::shared_ptr<WaveViewCache::Entry>();
	}

	return images->lookup_image (_region->audio_source (_channel), start, end, _channel,
	                             _height, _region_amplitude, _fill_color, _samples_per_pixel, full);
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
	req->width = _canvas->visible_area().width();
	req->height = _height;
	req->fill_color = _fill_color;
	req->region_amplitude = _region_amplitude;

	send_request (req);
}

void
WaveView::generate_image (boost::shared_ptr<WaveViewThreadRequest> req, bool in_render_thread) const
{
	if (!req->should_stop()) {

		/* sample position is canonical here, and we want to generate
		 * an image that spans about twice the canvas width
		 */
		
		const framepos_t center = req->start + ((req->end - req->start) / 2);
		const framecnt_t image_samples = req->width * req->samples_per_pixel; /* one canvas width */
		
		/* we can request data from anywhere in the Source, between 0 and its length
		 */
		
		framepos_t sample_start = max (_region_start, (center - image_samples));
		framepos_t sample_end = min (center + image_samples, region_end());
		const int n_peaks = llrintf ((sample_end - sample_start)/ (req->samples_per_pixel));
		
		boost::scoped_array<ARDOUR::PeakData> peaks (new PeakData[n_peaks]);

		/* Note that Region::read_peaks() takes a start position based on an
		   offset into the Region's **SOURCE**, rather than an offset into
		   the Region itself.
		*/
	                             
		framecnt_t peaks_read = _region->read_peaks (peaks.get(), n_peaks,
		                                             sample_start, sample_end - sample_start,
		                                             req->channel,
		                                             req->samples_per_pixel);
		
		// apply waveform amplitude zoom multiplier

		req->image = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, n_peaks, req->height);
		/* make sure we record the sample positions that were actually used */
		req->start = sample_start;
		req->end = sample_end;
		
		if (peaks_read > 0) {

			for (framecnt_t i = 0; i < n_peaks; ++i) {
				peaks[i].max *= _amplitude_above_axis;
				peaks[i].min *= _amplitude_above_axis;
			}

			draw_image (req->image, peaks.get(), n_peaks, req);
		} else {
			draw_absent_image (req->image, peaks.get(), n_peaks);
		}
	}
	
	if (in_render_thread && !req->should_stop()) {
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

	if (!rendered) {
		/* first image generation should happen in RENDER thread */
		get_image_in_thread = false;
		rendered = true; /* comments in header file */
	}
	
	if (!_region) {
		return;
	}

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

	/* Now lets get the intersection with the area we've been asked to draw */

	boost::optional<Rect> d = self.intersection (area);

	if (!d) {
		return;
	}

	Rect draw = d.get();

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

	double image_offset;
	boost::shared_ptr<WaveViewCache::Entry> image_to_draw;
	
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

	image_offset = (image_to_draw->start - _region_start) / _samples_per_pixel;
	
	// cerr << "Offset into image to place at zero: " << image_offset << endl;

	if (_start_shift && (sample_start == _region_start) && (self.x0 == draw.x0)) {
		/* we are going to draw the first pixel for this region, but 
		   we may not want this to overlap a border around the
		   waveform. If so, _start_shift will be set.
		*/
		//cerr << name.substr (23) << " ss = " << sample_start << " rs = " << _region_start << " sf = " << _start_shift << " ds = " << draw_start << " self = " << self << " draw = " << draw << endl;
		//draw_start += _start_shift;
		//image_offset += _start_shift;
	}
	
	/* the image may only be a best-effort ... it may not span the entire
	 * range requested, though it is guaranteed to cover the start. So
	 * determine how many pixels we can actually draw.
	 */

	double draw_width;
	
	if (image_to_draw != _current_image) {
		draw_width = min ((double) image_to_draw->image->get_width() - image_offset, (draw_end - draw_start));
	} else {
		draw_width = draw_end - draw_start;
	}

	context->rectangle (draw_start, draw.y0, draw_width, draw.height());

	/* round image origin position to an exact pixel in device space to
	 * avoid blurring
	 */

	double x  = self.x0 + image_offset;
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

}

void
WaveView::compute_bounding_box () const
{
	if (_region) {
		_bounding_box = Rect (0.0, 0.0, region_length() / _samples_per_pixel, _height);
	} else {
		_bounding_box = boost::optional<Rect> ();
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
		end_visual_change ();
	}
}

void
WaveView::set_global_shape (Shape s)
{
	if (_global_shape != s) {
		_global_shape = s;
		VisualPropertiesChanged (); /* EMIT SIGNAL */
	}
}

void
WaveView::set_global_logscaled (bool yn)
{
	if (_global_logscaled != yn) {
		_global_logscaled = yn;
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
	
	if (current_request) {
		current_request->cancel ();
	}
	
	Glib::Threads::Mutex::Lock lm (request_queue_lock);

	/* now remove it from the queue and reset our request pointer so that
	   have no outstanding request (that we know about)
	*/
	
	request_queue.erase (this);
	current_request.reset ();
}

void
WaveView::send_request (boost::shared_ptr<WaveViewThreadRequest> req) const
{	
	if (req->type == WaveViewThreadRequest::Draw && current_request) {
		/* this will stop rendering in progress (which might otherwise
		   be long lived) for any current request.
		*/
		current_request->cancel ();
	}

	start_drawing_thread ();

	Glib::signal_idle().connect (sigc::bind (sigc::mem_fun (*this, &WaveView::idle_send_request), req));
}

bool
WaveView::idle_send_request (boost::shared_ptr<WaveViewThreadRequest> req) const
{
	{
		Glib::Threads::Mutex::Lock lm (request_queue_lock);
		/* swap requests (protected by lock) */
		current_request = req;
		request_queue.insert (this);
	}

	request_cond.signal (); /* wake thread */
	
	return false; /* do not call from idle again */
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
	if (_drawing_thread) {
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

		/* remove the request from the queue (remember: the "request"
		 * is just a pointer to a WaveView object)
		 */
		
		requestor = *(request_queue.begin());
		request_queue.erase (request_queue.begin());

		boost::shared_ptr<WaveViewThreadRequest> req = requestor->current_request;

		if (!req) {
			continue;
		}

		/* Generate an image. Unlock the request queue lock
		 * while we do this, so that other things can happen
		 * as we do rendering.
		 */

		request_queue_lock.unlock (); /* some RAII would be good here */

		try {
			requestor->generate_image (req, true);
		} catch (...) {
			req->image.clear(); /* just in case it was set before the exception, whatever it was */
		}

		request_queue_lock.lock ();

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

		if (end <= e->end && start >= e->start) {
			/* found an image that covers the range we need */
			use (src, e);
			full_coverage = true;
			return e;
		}

		if (start >= e->start) {
			/* found an image that covers the start, but not the
			 * end. See if it is longer than any other similar
			 * partial image that we've found so far.
			 */

			if ((e->end - e->start) > max_coverage) {
				best_partial = e;
				max_coverage = e->end - e->start;
			}
		}
	}

	if (best_partial) {
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
				c2 = caches.erase (c2);
				continue;
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
	cache_list.push_back (make_pair (src, ce));
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
	SortByTimestamp sorter;

	/* sort list in LRU order */

	sort (cache_list.begin(), cache_list.end(), sorter);

	while (image_cache_size > _image_cache_threshold) {

		ListEntry& le (cache_list.front());

		ImageCache::iterator x;
		
		if ((x = cache_map.find (le.first)) == cache_map.end ()) {
			/* wierd ... no entry for this AudioSource */
			continue;
		}
		
		CacheLine& cl  = x->second;

		for (CacheLine::iterator c = cl.begin(); c != cl.end(); ++c) {

			if (*c == le.second) {

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
      
		/* Remove from the linear list */
		cache_list.erase (cache_list.begin());
	}
}

void
WaveViewCache::set_image_cache_threshold (uint64_t sz)
{
	_image_cache_threshold = sz;
	cache_flush ();
}
