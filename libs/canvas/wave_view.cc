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

#include "gtkmm2ext/utils.h"

#include "pbd/compose.h"
#include "pbd/signals.h"
#include "pbd/stacktrace.h"

#include "ardour/types.h"
#include "ardour/dB.h"
#include "ardour/lmath.h"
#include "ardour/audioregion.h"

#include "canvas/wave_view.h"
#include "canvas/utils.h"
#include "canvas/canvas.h"
#include "canvas/colors.h"

#include <gdkmm/general.h>

#include "gtkmm2ext/gui_thread.h"

using namespace std;
using namespace ARDOUR;
using namespace ArdourCanvas;

#define CACHE_HIGH_WATER (2)

std::map <boost::shared_ptr<AudioSource>, std::vector<WaveView::CacheEntry> >  WaveView::_image_cache;
double WaveView::_global_gradient_depth = 0.6;
bool WaveView::_global_logscaled = false;
WaveView::Shape WaveView::_global_shape = WaveView::Normal;
bool WaveView::_global_show_waveform_clipping = true;
double WaveView::_clip_level = 0.98853;

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
	, _region_amplitude (_region->scale_amplitude ())
	, _start_shift (0.0)
	, _region_start (region->start())
{
	_region->DropReferences.connect (_source_invalidated_connection, MISSING_INVALIDATOR,
						     boost::bind (&ArdourCanvas::WaveView::invalidate_source,
								  this, boost::weak_ptr<AudioSource>(_region->audio_source())), gui_context());

	VisualPropertiesChanged.connect_same_thread (invalidation_connection, boost::bind (&WaveView::handle_visual_property_change, this));
	ClipLevelChanged.connect_same_thread (invalidation_connection, boost::bind (&WaveView::handle_clip_level_change, this));
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
	, _region_amplitude (_region->scale_amplitude ())
	, _region_start (region->start())
{
	_region->DropReferences.connect (_source_invalidated_connection, MISSING_INVALIDATOR,
						     boost::bind (&ArdourCanvas::WaveView::invalidate_source,
								  this, boost::weak_ptr<AudioSource>(_region->audio_source())), gui_context());

	VisualPropertiesChanged.connect_same_thread (invalidation_connection, boost::bind (&WaveView::handle_visual_property_change, this));
	ClipLevelChanged.connect_same_thread (invalidation_connection, boost::bind (&WaveView::handle_clip_level_change, this));
}

WaveView::~WaveView ()
{
	_source_invalidated_connection.disconnect();
	invalidate_image_cache ();
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

static inline double
window_to_image (double wave_origin, double image_start)
{
	return image_start - wave_origin;
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
WaveView::invalidate_source (boost::weak_ptr<AudioSource> src)
{
	if (boost::shared_ptr<AudioSource> source = src.lock()) {

		std::map <boost::shared_ptr<ARDOUR::AudioSource>, std::vector <CacheEntry> >::iterator i;
		for (i = _image_cache.begin (); i != _image_cache.end (); ++i) {
			if (i->first == source) {
				for (uint32_t n = 0; n < i->second.size (); ++n) {
					i->second[n].image.clear ();
				}
				i->second.clear ();
				_image_cache.erase (i->first);
			}
		}
	}
}

void
WaveView::invalidate_image_cache ()
{
	vector <uint32_t> deletion_list;
	vector <CacheEntry> caches;

	/* The source may have disappeared.*/

	if (_region->n_channels() == 0) {
		return;
	}

	if (_image_cache.find (_region->audio_source ()) != _image_cache.end ()) {
		caches = _image_cache.find (_region->audio_source ())->second;
	} else {
		return;
	}

	for (uint32_t i = 0; i < caches.size (); ++i) {

		if (_channel != caches[i].channel
		    || _height != caches[i].height
		    || _region_amplitude != caches[i].amplitude
		    || _fill_color != caches[i].fill_color) {

			continue;
		}

		deletion_list.push_back (i);
	}

	while (deletion_list.size() > 0) {
		caches[deletion_list.back ()].image.clear ();
		caches.erase (caches.begin() + deletion_list.back());
		deletion_list.pop_back();
	}

	if (caches.size () == 0) {
		_image_cache.erase(_region->audio_source ());
	} else {
		_image_cache[_region->audio_source ()] = caches;
	}
}

void
WaveView::consolidate_image_cache () const
{
	list <uint32_t> deletion_list;
	vector <CacheEntry> caches;
	uint32_t other_entries = 0;

	if (_image_cache.find (_region->audio_source ()) != _image_cache.end ()) {
		caches  = _image_cache.find (_region->audio_source ())->second;
	}

	for (uint32_t i = 0; i < caches.size (); ++i) {

		if (_channel != caches[i].channel
		    || _height != caches[i].height
		    || _region_amplitude != caches[i].amplitude
		    || _fill_color != caches[i].fill_color) {

			other_entries++;
			continue;
		}

		framepos_t segment_start = caches[i].start;
		framepos_t segment_end = caches[i].end;

		for (uint32_t j = i; j < caches.size (); ++j) {

			if (i == j || _channel != caches[j].channel
			    || _height != caches[i].height
			    || _region_amplitude != caches[i].amplitude
			    || _fill_color != caches[i].fill_color) {

				continue;
			}

			if (caches[j].start >= segment_start && caches[j].end <= segment_end) {

				deletion_list.push_back (j);
			}
		}
	}

	deletion_list.sort ();
	deletion_list.unique ();

	while (deletion_list.size() > 0) {
		caches[deletion_list.back ()].image.clear ();
		caches.erase (caches.begin() + deletion_list.back ());
		deletion_list.pop_back();
	}

	/* We don't care if this channel/height/amplitude has anything in the cache - just drop the Last Added entries
	   until we reach a size where there is a maximum of CACHE_HIGH_WATER + other entries.
	*/

	while (caches.size() > CACHE_HIGH_WATER + other_entries) {
		caches.front ().image.clear ();
		caches.erase(caches.begin ());
	}

	if (caches.size () == 0) {
		_image_cache.erase (_region->audio_source ());
	} else {
		_image_cache[_region->audio_source ()] = caches;
	}
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
WaveView::draw_image (Cairo::RefPtr<Cairo::ImageSurface>& image, PeakData* _peaks, int n_peaks) const
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
	Color alpha_one = rgba_to_color (0, 0, 0, 1.0);

	set_source_rgba (wave_context, alpha_one);
	set_source_rgba (outline_context, alpha_one);
	set_source_rgba (clip_context, alpha_one);
	set_source_rgba (zero_context, alpha_one);

	/* ensure single-pixel lines */

	wave_context->set_line_width (1.0);
	wave_context->translate (0.5, +0.5);

	outline_context->set_line_width (1.0);
	outline_context->translate (0.5, +0.5);

	clip_context->set_line_width (1.0);
	clip_context->translate (0.5, +0.5);

	zero_context->set_line_width (1.0);
	zero_context->translate (0.5, +0.5);

	/* the height of the clip-indicator should be at most 7 pixels,
	 * or 5% of the height of the waveview item.
	 */

	const double clip_height = min (7.0, ceil (_height * 0.05));
	bool draw_outline_as_wave = false;

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

			if (_global_show_waveform_clipping && (tips[i].clip_max)) {
				clip_context->move_to (i, tips[i].top);
				/* clip-indicating upper terminal line */
				clip_context->rel_line_to (0, min (clip_height, ceil(tips[i].spread + .5)));
			} else {
				outline_context->move_to (i, tips[i].top);
				/* normal upper terminal dot */
				outline_context->close_path ();
			}
		}

		wave_context->stroke ();
		clip_context->stroke ();
		outline_context->stroke ();

	} else {
		const double height_2 = (_height - 4.0) * .5;

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
				draw_outline_as_wave = false;
				/* lower outline/clip indicator */
				if (_global_show_waveform_clipping && tips[i].clip_min) {
					clip_context->move_to (i, tips[i].bot);
					/* clip-indicating lower terminal line */
					const double sign = tips[i].bot > height_2 ? -1 : 1;
					clip_context->rel_line_to (0, sign * min (clip_height, ceil (tips[i].spread + .5)));
				} else {
					outline_context->move_to (i, tips[i].bot + 0.5);
					/* normal lower terminal dot */
					outline_context->rel_line_to (0, -0.5);
				}
			} else {
				draw_outline_as_wave = true;
				if (tips[i].clip_min) {
					// make sure we draw the clip
					tips[i].clip_max = true;
				}
			}

			/* upper outline/clip indicator */
			if (_global_show_waveform_clipping && tips[i].clip_max) {
				clip_context->move_to (i, tips[i].top);
				/* clip-indicating upper terminal line */
				const double sign = tips[i].top > height_2 ? -1 : 1;
				clip_context->rel_line_to (0, sign * min(clip_height, ceil(tips[i].spread + .5)));
			} else {
				if (draw_outline_as_wave) {
					wave_context->move_to (i, tips[i].top + 0.5);
					/* special case where outline only is drawn.
					   is this correct? too short by 0.5?
					*/
					wave_context->rel_line_to (0, -0.5);
				} else {
					outline_context->move_to (i, tips[i].top + 0.5);
					/* normal upper terminal dot */
					outline_context->rel_line_to (0, -0.5);
				}
			}
		}

		wave_context->stroke ();
		outline_context->stroke ();
		clip_context->stroke ();
		zero_context->stroke ();
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

void
WaveView::get_image (Cairo::RefPtr<Cairo::ImageSurface>& image, framepos_t start, framepos_t end, double& image_offset) const
{
	vector <CacheEntry> caches;

	if (_image_cache.find (_region->audio_source ()) != _image_cache.end ()) {

		caches = _image_cache.find (_region->audio_source ())->second;
	}

	/* Find a suitable ImageSurface.
	*/
	for (uint32_t i = 0; i < caches.size (); ++i) {

		if (_channel != caches[i].channel
		    || _height != caches[i].height
		    || _region_amplitude != caches[i].amplitude
		    || _fill_color != caches[i].fill_color) {

			continue;
		}

		framepos_t segment_start = caches[i].start;
		framepos_t segment_end = caches[i].end;

		if (end <= segment_end && start >= segment_start) {
			image_offset = (segment_start - _region_start) / _samples_per_pixel;
			image = caches[i].image;

			return;
		}
	}

	consolidate_image_cache ();

	/* sample position is canonical here, and we want to generate
	 * an image that spans about twice the canvas width
	 */

	const framepos_t center = start + ((end - start) / 2);
	const framecnt_t canvas_samples = _canvas->visible_area().width() * _samples_per_pixel; /* one canvas width */

	/* we can request data from anywhere in the Source, between 0 and its length
	 */

	framepos_t sample_start = max ((framepos_t) 0, (center - canvas_samples));
	framepos_t sample_end = min (center + canvas_samples, _region->source_length (0));

	const int n_peaks = llrintf ((sample_end - sample_start)/ (double) _samples_per_pixel);

	boost::scoped_array<ARDOUR::PeakData> peaks (new PeakData[n_peaks]);

	framecnt_t peaks_read;
	peaks_read = _region->read_peaks (peaks.get(), n_peaks,
			     sample_start, sample_end - sample_start,
			     _channel,
			     _samples_per_pixel);

	image = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, n_peaks, _height);

	if (peaks_read > 0) {
		draw_image (image, peaks.get(), n_peaks);
	} else {
		draw_absent_image (image, peaks.get(), n_peaks);
	}

	_image_cache[_region->audio_source ()].push_back (CacheEntry (_channel, _height, _region_amplitude, _fill_color, sample_start,  sample_end, image));

	image_offset = (sample_start - _region->start()) / _samples_per_pixel;

	//cerr << "_image_cache size is : " << _image_cache.size() << " entries for this audiosource : " << _image_cache.find (_region->audio_source ())->second.size() << endl;

	return;
}

void
WaveView::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	assert (_samples_per_pixel != 0);

	if (!_region) {
		return;
	}

	Rect self = item_to_window (Rect (0.0, 0.0, _region->length() / _samples_per_pixel, _height));
	boost::optional<Rect> d = self.intersection (area);

	if (!d) {
		return;
	}

	Rect draw = d.get();

	/* window coordinates - pixels where x=0 is the left edge of the canvas
	 * window. We round down in case we were asked to
	 * draw "between" pixels at the start and/or end.
	 */
	
	double draw_start = floor (draw.x0);
	const double draw_end = floor (draw.x1);

	// cerr << "Need to draw " << draw_start << " .. " << draw_end << endl;

	/* image coordnates: pixels where x=0 is the start of this waveview,
	 * wherever it may be positioned. thus image_start=N means "an image
	 * that beings N pixels after the start of region that this waveview is
	 * representing.
	 */

	const framepos_t image_start = window_to_image (self.x0, draw_start);
	const framepos_t image_end = window_to_image (self.x0, draw_end);

	// cerr << "Image/WV space: " << image_start << " .. " << image_end << endl;

	/* sample coordinates - note, these are not subject to rounding error */
	framepos_t sample_start = _region_start + (image_start * _samples_per_pixel);
	framepos_t sample_end   = _region_start + (image_end * _samples_per_pixel);
	
	// cerr << "Sample space: " << sample_start << " .. " << sample_end << endl;

	Cairo::RefPtr<Cairo::ImageSurface> image;
	double image_offset = 0;

	get_image (image, sample_start, sample_end, image_offset);

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
	
	context->rectangle (draw_start, draw.y0, draw_end - draw_start, draw.height());

	/* round image origin position to an exact pixel in device space to
	 * avoid blurring
	 */

	double x  = self.x0 + image_offset;
	double y  = self.y0;
	context->user_to_device (x, y);
	x = round (x);
	y = round (y);
	context->device_to_user (x, y);

	context->set_source (image, x, y);
	context->fill ();

}

void
WaveView::compute_bounding_box () const
{
	if (_region) {
		_bounding_box = Rect (0.0, 0.0, _region->length() / _samples_per_pixel, _height);
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
	if (_amplitude_above_axis != a) {
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
	
