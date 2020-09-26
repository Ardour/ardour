/*
 * Copyright (C) 2011-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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

#include <boost/scoped_array.hpp>

#include <cairomm/cairomm.h>

#include <glibmm/threads.h>
#include <gdkmm/general.h>

#include "pbd/base_ui.h"
#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/signals.h"

#include "ardour/types.h"
#include "ardour/dB.h"
#include "ardour/lmath.h"
#include "ardour/audioregion.h"
#include "ardour/audiosource.h"
#include "ardour/session.h"

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "canvas/canvas.h"
#include "canvas/debug.h"

#include "waveview/wave_view.h"
#include "waveview/wave_view_private.h"

#ifdef __APPLE__
#define Rect ArdourCanvas::Rect
#endif

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace Gtkmm2ext;
using namespace ArdourCanvas;
using namespace ArdourWaveView;

double WaveView::_global_gradient_depth = 0.6;
bool WaveView::_global_logscaled = false;
WaveView::Shape WaveView::_global_shape = WaveView::Normal;
bool WaveView::_global_show_waveform_clipping = true;
double WaveView::_global_clip_level = 0.98853;

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
	, _props (new WaveViewProperties (region))
	, _shape_independent (false)
	, _logscaled_independent (false)
	, _gradient_depth_independent (false)
	, _draw_image_in_gui_thread (false)
	, _always_draw_image_in_gui_thread (false)
{
	init ();
}

WaveView::WaveView (Item* parent, boost::shared_ptr<ARDOUR::AudioRegion> region)
	: Item (parent)
	, _region (region)
	, _props (new WaveViewProperties (region))
	, _shape_independent (false)
	, _logscaled_independent (false)
	, _gradient_depth_independent (false)
	, _draw_image_in_gui_thread (false)
	, _always_draw_image_in_gui_thread (false)
{
	init ();
}

void
WaveView::init ()
{
#ifdef ENABLE_THREADED_WAVEFORM_RENDERING
	WaveViewThreads::initialize ();
#endif

	_props->fill_color = _fill_color;
	_props->outline_color = _outline_color;

	VisualPropertiesChanged.connect_same_thread (
	    invalidation_connection, boost::bind (&WaveView::handle_visual_property_change, this));
	ClipLevelChanged.connect_same_thread (invalidation_connection,
	                                      boost::bind (&WaveView::handle_clip_level_change, this));
}

WaveView::~WaveView ()
{
#ifdef ENABLE_THREADED_WAVEFORM_RENDERING
	WaveViewThreads::deinitialize ();
#endif

	reset_cache_group ();
}

string
WaveView::debug_name() const
{
	return _region->name () + string (":") + PBD::to_string (_props->channel + 1);
}

void
WaveView::set_always_get_image_in_thread (bool yn)
{
	_always_draw_image_in_gui_thread = yn;
}

void
WaveView::handle_visual_property_change ()
{
	bool changed = false;

	if (!_shape_independent && (_props->shape != global_shape())) {
		_props->shape = global_shape();
		changed = true;
	}

	if (!_logscaled_independent && (_props->logscaled != global_logscaled())) {
		_props->logscaled = global_logscaled();
		changed = true;
	}

	if (!_gradient_depth_independent && (_props->gradient_depth != global_gradient_depth())) {
		_props->gradient_depth = global_gradient_depth();
		changed = true;
	}

	if (changed) {
		begin_visual_change ();
		end_visual_change ();
	}
}

void
WaveView::handle_clip_level_change ()
{
	begin_visual_change ();
	end_visual_change ();
}

void
WaveView::set_fill_color (Color c)
{
	if (c != _fill_color) {
		begin_visual_change ();
		Fill::set_fill_color (c);
		_props->fill_color = _fill_color; // ugh
		end_visual_change ();
	}
}

void
WaveView::set_outline_color (Color c)
{
	if (c != _outline_color) {
		begin_visual_change ();
		Outline::set_outline_color (c);
		_props->outline_color = c;
		end_visual_change ();
	}
}

void
WaveView::set_samples_per_pixel (double samples_per_pixel)
{
	if (_props->samples_per_pixel != samples_per_pixel) {
		begin_change ();

		_props->samples_per_pixel = samples_per_pixel;
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
	if (_global_clip_level != clip_level) {
		_global_clip_level = clip_level;
		ClipLevelChanged ();
	}
}

boost::shared_ptr<WaveViewDrawRequest>
WaveView::create_draw_request (WaveViewProperties const& props) const
{
	assert (props.is_valid());

	boost::shared_ptr<WaveViewDrawRequest> request (new WaveViewDrawRequest);

	request->image = boost::shared_ptr<WaveViewImage> (new WaveViewImage (_region, props));
	return request;
}

void
WaveView::prepare_for_render (Rect const& area) const
{
	if (draw_image_in_gui_thread()) {
		// Drawing image in GUI thread in WaveView::render
		return;
	}

	Rect draw_rect;
	Rect self_rect;

	// all in window coordinate space
	if (!get_item_and_draw_rect_in_window_coords (area, self_rect, draw_rect)) {
		return;
	}

	double const image_start_pixel_offset = draw_rect.x0 - self_rect.x0;
	double const image_end_pixel_offset = draw_rect.x1 - self_rect.x0;

	WaveViewProperties required_props = *_props;

	required_props.set_sample_positions_from_pixel_offsets (image_start_pixel_offset,
	                                                        image_end_pixel_offset);

	if (!required_props.is_valid ()) {
		return;
	}

	if (_image) {
		if (_image->props.is_equivalent (required_props)) {
			return;
		} else {
			// Image does not contain sample area required
		}
	}

	boost::shared_ptr<WaveViewDrawRequest> request = create_draw_request (required_props);

	queue_draw_request (request);
}

bool
WaveView::get_item_and_draw_rect_in_window_coords (Rect const& canvas_rect, Rect& item_rect,
                                                   Rect& draw_rect) const
{
	/* a WaveView is intimately connected to an AudioRegion. It will
	 * display the waveform within the region, anywhere from the start of
	 * the region to its end.
	 *
	 * the area we've been asked to render may overlap with area covered
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

	double const width = region_length() / _props->samples_per_pixel;
	item_rect = item_to_window (Rect (0.0, 0.0, width, _props->height));

	/* Now lets get the intersection with the area we've been asked to draw */

	draw_rect = item_rect.intersection (canvas_rect);

	if (!draw_rect) {
		// No intersection with drawing area
		return false;
	}

	/* draw_rect now defines the rectangle we need to update/render the waveview
	 * into, in window coordinate space.
	 *
	 * We round down in case we were asked to draw "between" pixels at the start
	 * and/or end.
	 */
	draw_rect.x0 = floor (draw_rect.x0);
	draw_rect.x1 = floor (draw_rect.x1);

	return true;
}

void
WaveView::queue_draw_request (boost::shared_ptr<WaveViewDrawRequest> const& request) const
{
	// Don't enqueue any requests without a thread to dequeue them.
	assert (WaveViewThreads::enabled());

	if (!request || !request->is_valid()) {
		return;
	}

	if (current_request) {
		current_request->cancel ();
	}

	boost::shared_ptr<WaveViewImage> cached_image =
	    get_cache_group ()->lookup_image (request->image->props);

	if (cached_image) {
		// The image may not be finished at this point but that is fine, great in
		// fact as it means it should only need to be drawn once.
		request->image = cached_image;
		current_request = request;
	} else {
		// now we can finally set an optimal image now that we are not using the
		// properties for comparisons.
		request->image->props.set_width_samples (optimal_image_width_samples ());

		current_request = request;

		// Add it to the cache so that other WaveViews can refer to the same image
		get_cache_group()->add_image (current_request->image);

		WaveViewThreads::enqueue_draw_request (current_request);
	}
}

void
WaveView::compute_tips (ARDOUR::PeakData const& peak, WaveView::LineTips& tips,
                        double const effective_height)
{
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
WaveView::y_extent (double s, Shape const shape, double const height)
{
	assert (shape == Rectified);
	return floor ((1.0 - s) * height);
}

void
WaveView::draw_absent_image (Cairo::RefPtr<Cairo::ImageSurface>& image, PeakData* peaks, int n_peaks)
{
	const double height = image->get_height();

	Cairo::RefPtr<Cairo::ImageSurface> stripe = Cairo::ImageSurface::create (Cairo::FORMAT_A8, n_peaks, height);

	Cairo::RefPtr<Cairo::Context> stripe_context = Cairo::Context::create (stripe);
	stripe_context->set_antialias (Cairo::ANTIALIAS_NONE);

	uint32_t stripe_separation = 150;
	double start = - floor (height / stripe_separation) * stripe_separation;
	int stripe_x = 0;

	while (start < n_peaks) {

		stripe_context->move_to (start, 0);
		stripe_x = start + height;
		stripe_context->line_to (stripe_x, height);
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
WaveView::draw_image (Cairo::RefPtr<Cairo::ImageSurface>& image, PeakData* peaks, int n_peaks,
                      boost::shared_ptr<WaveViewDrawRequest> req)
{
	const double height = image->get_height();

	ImageSet images;

	images.wave = Cairo::ImageSurface::create (Cairo::FORMAT_A8, n_peaks, height);
	images.outline = Cairo::ImageSurface::create (Cairo::FORMAT_A8, n_peaks, height);
	images.clip = Cairo::ImageSurface::create (Cairo::FORMAT_A8, n_peaks, height);
	images.zero = Cairo::ImageSurface::create (Cairo::FORMAT_A8, n_peaks, height);

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

	const double clip_level = _global_clip_level * req->image->props.amplitude;

	const Shape shape = req->image->props.shape;
	const bool logscaled = req->image->props.logscaled;

	if (req->image->props.shape == WaveView::Rectified) {

		/* each peak is a line from the bottom of the waveview
		 * to a point determined by max (peaks[i].max,
		 * peaks[i].min)
		 */

		if (logscaled) {
			for (int i = 0; i < n_peaks; ++i) {

				tips[i].bot = height - 1.0;
				const double p = alt_log_meter (fast_coefficient_to_dB (max (fabs (peaks[i].max), fabs (peaks[i].min))));
				tips[i].top = y_extent (p, shape, height);
				tips[i].spread = p * height;

				if (peaks[i].max >= clip_level) {
					tips[i].clip_max = true;
				}

				if (-(peaks[i].min) >= clip_level) {
					tips[i].clip_min = true;
				}
			}

		} else {
			for (int i = 0; i < n_peaks; ++i) {

				tips[i].bot = height - 1.0;
				const double p = max(fabs (peaks[i].max), fabs (peaks[i].min));
				tips[i].top = y_extent (p, shape, height);
				tips[i].spread = p * height;
				if (p >= clip_level) {
					tips[i].clip_max = true;
				}
			}

		}

	} else {

		if (logscaled) {
			for (int i = 0; i < n_peaks; ++i) {
				PeakData p;
				p.max = peaks[i].max;
				p.min = peaks[i].min;

				if (peaks[i].max >= clip_level) {
					tips[i].clip_max = true;
				}
				if (-(peaks[i].min) >= clip_level) {
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

				compute_tips (p, tips[i], height);
				tips[i].spread = tips[i].bot - tips[i].top;
			}

		} else {
			for (int i = 0; i < n_peaks; ++i) {
				if (peaks[i].max >= clip_level) {
					tips[i].clip_max = true;
				}
				if (-(peaks[i].min) >= clip_level) {
					tips[i].clip_min = true;
				}

				compute_tips (peaks[i], tips[i], height);
				tips[i].spread = tips[i].bot - tips[i].top;
			}

		}
	}

	if (req->stopped()) {
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

	const double clip_height = min (7.0, ceil (height * 0.05));

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

	if (shape == WaveView::Rectified) {

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
		const int height_zero = floor(height * .5);

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
			bool const show_zero_line = req->image->props.show_zero;

			if (show_zero_line && ((tips[i].spread >= 5.0) || (tips[i].top > height_zero ) || (tips[i].bot < height_zero)) ) {
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

	if (req->stopped()) {
		return;
	}

	Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create (image);

	/* Here we set a source colour and use the various components as a mask. */

	const Color fill_color = req->image->props.fill_color;
	const double gradient_depth = req->image->props.gradient_depth;

	if (gradient_depth != 0.0) {

		Cairo::RefPtr<Cairo::LinearGradient> gradient (Cairo::LinearGradient::create (0, 0, 0, height));

		double stops[3];

		double r, g, b, a;


		if (shape == Rectified) {
			stops[0] = 0.1;
			stops[1] = 0.3;
			stops[2] = 0.9;
		} else {
			stops[0] = 0.1;
			stops[1] = 0.5;
			stops[2] = 0.9;
		}

		color_to_rgba (fill_color, r, g, b, a);
		gradient->add_color_stop_rgba (stops[1], r, g, b, a);
		/* generate a new color for the middle of the gradient */
		double h, s, v;
		color_to_hsv (fill_color, h, s, v);
		/* change v towards white */
		v *= 1.0 - gradient_depth;
		Color center = hsva_to_color (h, s, v, a);
		color_to_rgba (center, r, g, b, a);

		gradient->add_color_stop_rgba (stops[0], r, g, b, a);
		gradient->add_color_stop_rgba (stops[2], r, g, b, a);

		context->set_source (gradient);
	} else {
		set_source_rgba (context, fill_color);
	}

	if (req->stopped()) {
		return;
	}

	context->mask (images.wave, 0, 0);
	context->fill ();

	set_source_rgba (context, req->image->props.outline_color);
	context->mask (images.outline, 0, 0);
	context->fill ();

	set_source_rgba (context, req->image->props.clip_color);
	context->mask (images.clip, 0, 0);
	context->fill ();

	set_source_rgba (context, req->image->props.zero_color);
	context->mask (images.zero, 0, 0);
	context->fill ();
}

samplecnt_t
WaveView::optimal_image_width_samples () const
{
	/* Compute how wide the image should be in samples.
	 *
	 * The resulting image should be wider than the canvas width so that the
	 * image does not have to be redrawn each time the canvas offset changes, but
	 * drawing too much unnecessarily, for instance when zooming into the canvas
	 * the part of the image that is outside of the visible canvas area may never
	 * be displayed and will just increase apparent render time and reduce
	 * responsiveness in non-threaded rendering and cause "flashing" waveforms in
	 * threaded rendering mode.
	 *
	 * Another thing to consider is that if there are a number of waveforms on
	 * the canvas that are the width of the canvas then we don't want to have to
	 * draw the images for them all at once as it will cause a spike in render
	 * time, or in threaded rendering mode it will mean all the draw requests will
	 * the queued during the same sample/expose event. This issue can be
	 * alleviated by using an element of randomness in selecting the image width.
	 *
	 * If the value of samples per pixel is less than 1/10th of a second, use
	 * 1/10th of a second instead.
	 */

	samplecnt_t canvas_width_samples = _canvas->visible_area().width() * _props->samples_per_pixel;
	const samplecnt_t one_tenth_of_second = _region->session().sample_rate() / 10;

	/* If zoomed in where a canvas item interects with the canvas area but
	 * stretches for many pages either side, to avoid having draw all images when
	 * the canvas scrolls by a page width the multiplier would have to be a
	 * randomized amount centered around 3 times the visible canvas width, but
	 * for other operations like zooming or even with a stationary playhead it is
	 * a lot of extra drawing that can affect performance.
	 *
	 * So without making things too complicated with different widths for
	 * different operations, try to use a width that is a balance and will work
	 * well for scrolling(non-page width) so all the images aren't redrawn at the
	 * same time but also faster for sequential zooming operations.
	 *
	 * Canvas items that don't intersect with the edges of the visible canvas
	 * will of course only draw images that are the pixel width of the item.
	 *
	 * It is a perhaps a coincidence that these values are centered roughly
	 * around the golden ratio but they did work well in my testing.
	 */
	const double min_multiplier = 1.4;
	const double max_multiplier = 1.8;

	/**
	 * A combination of high resolution screens, high samplerates and high
	 * zoom levels(1 sample per pixel) can cause 1/10 of a second(in
	 * pixels) to exceed the cairo image size limit.
	 */
	const double cairo_image_limit = 32767.0;
	const double max_image_width = cairo_image_limit / max_multiplier;

	samplecnt_t max_width_samples = floor (max_image_width / _props->samples_per_pixel);

	const samplecnt_t one_tenth_of_second_limited = std::min (one_tenth_of_second, max_width_samples);

	samplecnt_t new_sample_count = std::max (canvas_width_samples, one_tenth_of_second_limited);

	const double multiplier = g_random_double_range (min_multiplier, max_multiplier);

	return new_sample_count * multiplier;
}

void
WaveView::set_image (boost::shared_ptr<WaveViewImage> img) const
{
	get_cache_group ()->add_image (img);
	_image = img;
}

void
WaveView::process_draw_request (boost::shared_ptr<WaveViewDrawRequest> req)
{
	boost::shared_ptr<const ARDOUR::AudioRegion> region = req->image->region.lock();

	if (!region) {
		return;
	}

	if (req->stopped()) {
		return;
	}

	WaveViewProperties const& props = req->image->props;

	const int n_peaks = props.get_width_pixels ();

	assert (n_peaks > 0 && n_peaks < 32767);

	boost::scoped_array<ARDOUR::PeakData> peaks (new PeakData[n_peaks]);

	/* Note that Region::read_peaks() takes a start position based on an
	   offset into the Region's **SOURCE**, rather than an offset into
	   the Region itself.
	*/

	samplecnt_t peaks_read =
	    region->read_peaks (peaks.get (), n_peaks, props.get_sample_start (),
	                        props.get_length_samples (), props.channel, props.samples_per_pixel);

	if (req->stopped()) {
		return;
	}

	Cairo::RefPtr<Cairo::ImageSurface> cairo_image =
	    Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, n_peaks, req->image->props.height);

	// http://cairographics.org/manual/cairo-Image-Surfaces.html#cairo-image-surface-create
	// This function always returns a valid pointer, but it will return a pointer to a "nil" surface..
	// but there's some evidence that req->image can be NULL.
	// http://tracker.ardour.org/view.php?id=6478
	assert (cairo_image);

	if (peaks_read > 0) {

		/* region amplitude will have been used to generate the
		 * peak values already, but not the visual-only
		 * amplitude_above_axis. So apply that here before
		 * rendering.
		 */

		const double amplitude_above_axis = props.amplitude_above_axis;

		if (amplitude_above_axis != 1.0) {
			for (samplecnt_t i = 0; i < n_peaks; ++i) {
				peaks[i].max *= amplitude_above_axis;
				peaks[i].min *= amplitude_above_axis;
			}
		}

		draw_image (cairo_image, peaks.get(), n_peaks, req);

	} else {
		draw_absent_image (cairo_image, peaks.get(), n_peaks);
	}

	if (req->stopped ()) {
		return;
	}

	// Assign now that we are sure all drawing is complete as that is what
	// determines whether a request was finished.
	req->image->cairo_image = cairo_image;
}

bool
WaveView::draw_image_in_gui_thread () const
{
	return _draw_image_in_gui_thread || _always_draw_image_in_gui_thread || !rendered () ||
	       !WaveViewThreads::enabled ();
}

void
WaveView::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	assert (_props->samples_per_pixel != 0);

	if (!_region) { // assert?
		return;
	}

	Rect draw;
	Rect self;

	if (!get_item_and_draw_rect_in_window_coords (area, self, draw)) {
		assert(false);
		return;
	}

	if (_props->height < 1) {
			if (_props->channel % 2) {
				return;
			}
			context->rectangle (draw.x0, draw.y0, draw.width (), draw.height ());
			if (1 == (_props->channel % 3)) {
				set_source_rgba (context, _props->zero_color);
			} else {
				set_source_rgba (context, _props->fill_color);
			}
			context->fill ();
		return;
	}

	double const image_start_pixel_offset = draw.x0 - self.x0;
	double const image_end_pixel_offset = draw.x1 - self.x0;

	if (image_start_pixel_offset == image_end_pixel_offset) {
		// this may happen if zoomed very far out with a small region
		return;
	}

	WaveViewProperties required_props = *_props;

	required_props.set_sample_positions_from_pixel_offsets (image_start_pixel_offset,
	                                                        image_end_pixel_offset);

	assert (required_props.is_valid());

	boost::shared_ptr<WaveViewImage> image_to_draw;

	if (current_request) {
		if (!current_request->image->props.is_equivalent (required_props)) {
			// The WaveView properties may have been updated during recording between
			// prepare_for_render and render calls and the new required props have
			// different end sample value.
			current_request->cancel ();
			current_request.reset ();
		} else if (current_request->finished ()) {
			image_to_draw = current_request->image;
			current_request.reset ();
		}
	} else {
		// No current Request
	}

	if (!image_to_draw && _image) {
		if (_image->props.is_equivalent (required_props)) {
			// Image contains required properties
			image_to_draw = _image;
		} else {
			// Image does not contain properties required
		}
	}

	if (!image_to_draw) {
		image_to_draw = get_cache_group ()->lookup_image (required_props);
		if (image_to_draw && !image_to_draw->finished ()) {
			// Found equivalent but unfinished Image in cache
			image_to_draw.reset ();
		}
	}

	if (!image_to_draw) {
		// No existing image to draw

		boost::shared_ptr<WaveViewDrawRequest> const request = create_draw_request (required_props);

		if (draw_image_in_gui_thread ()) {
			// now that we have to draw something, draw more than required.
			request->image->props.set_width_samples (optimal_image_width_samples ());

			process_draw_request (request);

			image_to_draw = request->image;

		} else if (current_request) {
			if (current_request->finished ()) {
				// There is a chance the request is now finished since checking above
				image_to_draw = current_request->image;
				current_request.reset ();
			} else if (_canvas->get_microseconds_since_render_start () < 15000) {
				current_request->cancel ();
				current_request.reset ();

				// Drawing image in GUI thread as we have time

				// now that we have to draw something, draw more than required.
				request->image->props.set_width_samples (optimal_image_width_samples ());

				process_draw_request (request);

				image_to_draw = request->image;
			} else {
				// Waiting for current request to finish
				redraw ();
				return;
			}
		} else {
			// Defer the rendering to another thread or perhaps render pass if
			// a thread cannot generate it in time.
			queue_draw_request (request);
			redraw ();
			return;
		}
	}

	/* reset this so that future missing images can be generated in a worker thread. */
	_draw_image_in_gui_thread = false;

	assert (image_to_draw);

	/* compute the first pixel of the image that should be used when we
	 * render the specified range.
	 */

	double image_origin_in_self_coordinates =
	    (image_to_draw->props.get_sample_start () - _props->region_start) / _props->samples_per_pixel;

	/* the image may only be a best-effort ... it may not span the entire
	 * range requested, though it is guaranteed to cover the start. So
	 * determine how many pixels we can actually draw.
	 */

	const double draw_start_pixel = draw.x0;
	const double draw_end_pixel = draw.x1;

	double draw_width_pixels = draw_end_pixel - draw_start_pixel;

	if (image_to_draw != _image) {

		/* the image is guaranteed to start at or before
		 * draw_start. But if it starts before draw_start, that reduces
		 * the maximum available width we can render with.
		 *
		 * so .. clamp the draw width to the smaller of what we need to
		 * draw or the available width of the image.
		 */
		draw_width_pixels = min ((double)image_to_draw->cairo_image->get_width (), draw_width_pixels);

		set_image (image_to_draw);
	}

	context->rectangle (draw_start_pixel, draw.y0, draw_width_pixels, draw.height());

	/* round image origin position to an exact pixel in device space to
	 * avoid blurring
	 */

	double x  = self.x0 + image_origin_in_self_coordinates;
	double y  = self.y0;
	context->user_to_device (x, y);
	x = floor (x);
	y = floor (y);
	context->device_to_user (x, y);

	/* the coordinates specify where in "user coordinates" (i.e. what we
	 * generally call "canvas coordinates" in this code) the image origin
	 * will appear. So specifying (10,10) will put the upper left corner of
	 * the image at (10,10) in user space.
	 */

	context->set_source (image_to_draw->cairo_image, x, y);
	context->fill ();
}

void
WaveView::compute_bounding_box () const
{
	if (_region) {
		_bounding_box = Rect (0.0, 0.0, region_length() / _props->samples_per_pixel, _props->height);
	} else {
		_bounding_box = Rect ();
	}

	_bounding_box_dirty = false;
}

void
WaveView::set_height (Distance height)
{
	if (_props->height != height) {
		begin_change ();

		_props->height = height;
		_draw_image_in_gui_thread = true;

		_bounding_box_dirty = true;
		end_change ();
	}
}

void
WaveView::set_channel (int channel)
{
	if (_props->channel != channel) {
		begin_change ();
		_props->channel = channel;
		reset_cache_group ();
		_bounding_box_dirty = true;
		end_change ();
	}
}

void
WaveView::set_logscaled (bool yn)
{
	if (_props->logscaled != yn) {
		begin_visual_change ();
		_props->logscaled = yn;
		end_visual_change ();
	}
}

void
WaveView::set_gradient_depth (double)
{
	// TODO ??
}

double
WaveView::gradient_depth () const
{
	return _props->gradient_depth;
}

void
WaveView::gain_changed ()
{
	begin_visual_change ();
	_props->amplitude = _region->scale_amplitude ();
	_draw_image_in_gui_thread = true;
	end_visual_change ();
}

void
WaveView::set_zero_color (Color c)
{
	if (_props->zero_color != c) {
		begin_visual_change ();
		_props->zero_color = c;
		end_visual_change ();
	}
}

void
WaveView::set_clip_color (Color c)
{
	if (_props->clip_color != c) {
		begin_visual_change ();
		_props->clip_color = c;
		end_visual_change ();
	}
}

void
WaveView::set_show_zero_line (bool yn)
{
	if (_props->show_zero != yn) {
		begin_visual_change ();
		_props->show_zero = yn;
		end_visual_change ();
	}
}

bool
WaveView::show_zero_line () const
{
	return _props->show_zero;
}

void
WaveView::set_shape (Shape s)
{
	if (_props->shape != s) {
		begin_visual_change ();
		_props->shape = s;
		end_visual_change ();
	}
}

void
WaveView::set_amplitude_above_axis (double a)
{
	if (fabs (_props->amplitude_above_axis - a) > 0.01) {
		begin_visual_change ();
		_props->amplitude_above_axis = a;
		_draw_image_in_gui_thread = true;
		end_visual_change ();
	}
}

double
WaveView::amplitude_above_axis () const
{
	return _props->amplitude_above_axis;
}

void
WaveView::set_global_shape (Shape s)
{
	if (_global_shape != s) {
		_global_shape = s;
		WaveViewCache::get_instance()->clear_cache ();
		VisualPropertiesChanged (); /* EMIT SIGNAL */
	}
}

void
WaveView::set_global_logscaled (bool yn)
{
	if (_global_logscaled != yn) {
		_global_logscaled = yn;
		WaveViewCache::get_instance()->clear_cache ();
		VisualPropertiesChanged (); /* EMIT SIGNAL */
	}
}

void
WaveView::clear_cache ()
{
	WaveViewCache::get_instance()->clear_cache ();
}

samplecnt_t
WaveView::region_length() const
{
	return _region->length_samples() - (_props->region_start - _region->start_sample());
}

samplepos_t
WaveView::region_end() const
{
	return _props->region_start + region_length();
}

void
WaveView::set_region_start (sampleoffset_t start)
{
	if (!_region) {
		return;
	}

	if (_props->region_start == start) {
		return;
	}

	begin_change ();
	_props->region_start = start;
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
	_props->region_start = _region->start_sample();
	_props->region_end = _region->start_sample() + _region->length_samples();
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
	//_start_shift = pixels;
	end_visual_change ();
}

void
WaveView::set_image_cache_size (uint64_t sz)
{
	WaveViewCache::get_instance()->set_image_cache_threshold (sz);
}

boost::shared_ptr<WaveViewCacheGroup>
WaveView::get_cache_group () const
{
	if (_cache_group) {
		return _cache_group;
	}

	boost::shared_ptr<AudioSource> source = _region->audio_source (_props->channel);
	assert (source);

	_cache_group = WaveViewCache::get_instance ()->get_cache_group (source);

	return _cache_group;
}

void
WaveView::reset_cache_group ()
{
	WaveViewCache::get_instance()->reset_cache_group (_cache_group);
}
