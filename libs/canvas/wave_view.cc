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
#include "ardour/audioregion.h"

#include "canvas/wave_view.h"
#include "canvas/utils.h"
#include "canvas/canvas.h"

#include <gdkmm/general.h>

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
	, _region_start (region->start())
{
	VisualPropertiesChanged.connect_same_thread (invalidation_connection, boost::bind (&WaveView::handle_visual_property_change, this));
	ClipLevelChanged.connect_same_thread (invalidation_connection, boost::bind (&WaveView::handle_clip_level_change, this));
}

WaveView::WaveView (Group* g, boost::shared_ptr<ARDOUR::AudioRegion> region)
	: Item (g)
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
	VisualPropertiesChanged.connect_same_thread (invalidation_connection, boost::bind (&WaveView::handle_visual_property_change, this));
	ClipLevelChanged.connect_same_thread (invalidation_connection, boost::bind (&WaveView::handle_clip_level_change, this));
}

WaveView::~WaveView ()
{
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
image_to_window (double wave_origin, double image_start)
{
	return wave_origin + image_start;
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
WaveView::invalidate_image_cache ()
{
	vector <uint32_t> deletion_list;
	vector <CacheEntry> caches;

	if (_image_cache.find (_region->audio_source ()) != _image_cache.end ()) {
		caches = _image_cache.find (_region->audio_source ())->second;
	} else {
		return;
	}

	for (uint32_t i = 0; i < caches.size (); ++i) {

		if (_channel != caches[i].channel || _height != caches[i].height || _region_amplitude != caches[i].amplitude) {
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

		if (_channel != caches[i].channel || _height != caches[i].height || _region_amplitude != caches[i].amplitude) {
			other_entries++;
			continue;
		}

		framepos_t segment_start = caches[i].start;
		framepos_t segment_end = caches[i].end;

		for (uint32_t j = i; j < caches.size (); ++j) {

			if (i == j || _channel != caches[j].channel || _height != caches[i].height || _region_amplitude != caches[i].amplitude) {
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

struct LineTips {
	double top;
	double bot;
	bool clip_max;
	bool clip_min;
	
	LineTips() : top (0.0), bot (0.0), clip_max (false), clip_min (false) {}
};

void
WaveView::draw_image (Cairo::RefPtr<Cairo::ImageSurface>& image, PeakData* _peaks, int n_peaks) const
{
	Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create (image);
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
				tips[i].bot = height();
				tips[i].top = y_extent (alt_log_meter (fast_coefficient_to_dB (max (fabs (_peaks[i].max), fabs (_peaks[i].min)))));

				if (fabs (_peaks[i].max) >= clip_level) {
					tips[i].clip_max = true;
				}

				if (fabs (_peaks[i].min) >= clip_level) {
					tips[i].clip_min = true;
				}
			}
		} else {for (int i = 0; i < n_peaks; ++i) {
				tips[i].bot = height();
				tips[i].top = y_extent (max (fabs (_peaks[i].max), fabs (_peaks[i].min)));

				if (fabs (_peaks[i].max) >= clip_level) {
					tips[i].clip_max = true;
				}

				if (fabs (_peaks[i].min) >= clip_level) {
					tips[i].clip_min = true;
				}
			}
		}

	} else {

		if (_logscaled) {
			for (int i = 0; i < n_peaks; ++i) {
				Coord top = _peaks[i].min;
				Coord bot = _peaks[i].max;

				if (fabs (top) >= clip_level) {
					tips[i].clip_max = true;
				}

				if (fabs (bot) >= clip_level) {
					tips[i].clip_min = true;
				}

				if (top > 0.0) {
					top = y_extent (alt_log_meter (fast_coefficient_to_dB (top)));
				} else if (top < 0.0) {
					top = y_extent (-alt_log_meter (fast_coefficient_to_dB (-top)));
				} else {
					top = y_extent (0.0);
				}

				if (bot > 0.0) {
					bot = y_extent (alt_log_meter (fast_coefficient_to_dB (bot)));
				} else if (bot < 0.0) {
					bot = y_extent (-alt_log_meter (fast_coefficient_to_dB (-bot)));
				} else {
					bot = y_extent (0.0);
				}

				tips[i].top = top;
				tips[i].bot = bot;
			} 

		} else {
			for (int i = 0; i < n_peaks; ++i) {

				if (fabs (_peaks[i].max) >= clip_level) {
					tips[i].clip_max = true;
				}

				if (fabs (_peaks[i].min) >= clip_level) {
					tips[i].clip_min = true;
				}

				tips[i].top = y_extent (_peaks[i].min);
				tips[i].bot = y_extent (_peaks[i].max);


			}
		}
	}

	if (gradient_depth() != 0.0) {
			
		Cairo::RefPtr<Cairo::LinearGradient> gradient (Cairo::LinearGradient::create (0, 0, 0, _height));
			
		double stops[3];
			
		double r, g, b, a;

		if (_shape == Rectified) {
			stops[0] = 0.1;
			stops[0] = 0.3;
			stops[0] = 0.9;
		} else {
			stops[0] = 0.1;
			stops[1] = 0.5;
			stops[2] = 0.9;
		}

		color_to_rgba (_fill_color, r, g, b, a);
		gradient->add_color_stop_rgba (stops[0], r, g, b, a);
		gradient->add_color_stop_rgba (stops[2], r, g, b, a);

		/* generate a new color for the middle of the gradient */
		double h, s, v;
		color_to_hsv (_fill_color, h, s, v);
		/* change v towards white */
		v *= 1.0 - gradient_depth();
		Color center = hsv_to_color (h, s, v, a);
		color_to_rgba (center, r, g, b, a);
		gradient->add_color_stop_rgba (stops[1], r, g, b, a);
			
		context->set_source (gradient);
	} else {
		set_source_rgba (context, _fill_color);
	}

	/* ensure single-pixel lines */
		
	context->set_line_width (0.5);
	context->translate (0.5, 0.0);

	/* draw the lines */
	/* we add dots to the top and bottom of each line (this is
	 * modelled on pyramix, except that we add clipping indicators
	 * (see below).
	 */

	if (_shape == WaveView::Rectified) {
		for (int i = 0; i < n_peaks; ++i) {
			context->move_to (i, tips[i].top); /* down 1 pixel */
			if (!tips[i].clip_max && !tips[i].clip_min) {
				/* normal upper terminal dot */
				context->rel_line_to (0, 1.0);
			}
			context->move_to (i, tips[i].top);
			context->line_to (i, tips[i].bot);
		}
	} else {
		for (int i = 0; i < n_peaks; ++i) {
			context->move_to (i, tips[i].top);
			if (!tips[i].clip_max) {
				/* normal upper terminal dot */
				context->rel_line_to (0, 1.0);
			}
			context->move_to (i, tips[i].top);
			context->line_to (i, tips[i].bot);
			if (!tips[i].clip_min) {
				/* normal lower terminal dot */
				context->rel_line_to (0, -1.0);
			}
		}
	}

	context->stroke ();

	 /* the height of the clip-indicator should be at most 7 pixels,
	 * or 5% of the height of the waveview item.
	 */

	const double clip_height = min (7.0, ceil (_height * 0.05));

	set_source_rgba (context, _clip_color);
		
	for (int i = 0; i < n_peaks; ++i) {
		
		bool show_top_clip =   _global_show_waveform_clipping && 
			((_shape == WaveView::Rectified && (tips[i].clip_max || tips[i].clip_min)) ||
			 tips[i].clip_max);
			
		if (show_top_clip) {
			context->move_to (i, tips[i].top);
			context->rel_line_to (0, clip_height);
		}

		if (_global_show_waveform_clipping && _shape != WaveView::Rectified) {
			if (tips[i].clip_min) {
				context->move_to (i, tips[i].bot);
				context->rel_line_to (0, -clip_height);
			}
		}
	}
			
	context->stroke ();
		
	if (show_zero_line()) {
		set_source_rgba (context, _zero_color);
		context->set_line_width (1.0);
		context->move_to (0, y_extent (0.0) + 0.5);
		context->line_to (n_peaks, y_extent (0.0) + 0.5);
		context->stroke ();
	}
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

		if (_channel != caches[i].channel || _height != caches[i].height || _region_amplitude != caches[i].amplitude) {
			continue;
		}

		framepos_t segment_start = caches[i].start;
		framepos_t segment_end = caches[i].end;

		if (end <= segment_end && start >= segment_start) {
			image_offset = (segment_start - _region->start()) / _samples_per_pixel;
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

	_region->read_peaks (peaks.get(), n_peaks, 
			     sample_start, sample_end - sample_start,
			     _channel, 
			     _samples_per_pixel);

	image = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, ((double)(sample_end - sample_start)) / _samples_per_pixel, _height);

	draw_image (image, peaks.get(), n_peaks);

	_image_cache[_region->audio_source ()].push_back (CacheEntry (_channel, _height, _region_amplitude, sample_start,  sample_end, image));

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

	Rect self = item_to_window (Rect (0.5, 0.0, _region->length() / _samples_per_pixel, _height));
	boost::optional<Rect> d = self.intersection (area);

	if (!d) {
		return;
	}
	
	Rect draw = d.get();

	/* window coordinates - pixels where x=0 is the left edge of the canvas
	 * window. We round down in case we were asked to
	 * draw "between" pixels at the start and/or end.
	 */

	const double draw_start = floor (draw.x0);
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
WaveView::region_resized ()
{
	if (!_region) {
		return;
	}

	/* special: do not use _region->length() here to compute
	   bounding box because it will already have changed.
	   
	   if we have a bounding box, use it.
	*/

	_pre_change_bounding_box = _bounding_box;

	_bounding_box_dirty = true;
	compute_bounding_box ();

	end_change ();
}

Coord
WaveView::y_extent (double s) const
{
	/* it is important that this returns an integral value, so that we 
	   can ensure correct single pixel behaviour.
	 */

	Coord pos;

	switch (_shape) {
	case Rectified:
		pos = floor (_height - (s * _height));
		break;
	default:
		pos = floor ((1.0-s) * (_height / 2.0));
		break;
	}

	return min (_height, (max (0.0, pos)));
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
		VisualPropertiesChanged (); /* EMIT SIGNAL */
	}
}

