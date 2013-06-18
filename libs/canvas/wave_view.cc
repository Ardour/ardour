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

double WaveView::_global_gradient_depth = 0.6;
bool WaveView::_global_logscaled = false;
WaveView::Shape WaveView::_global_shape = WaveView::Normal;

PBD::Signal0<void> WaveView::VisualPropertiesChanged;

WaveView::WaveView (Group* parent, boost::shared_ptr<ARDOUR::AudioRegion> region)
	: Item (parent)
	, Outline (parent)
	, Fill (parent)
	, _region (region)
	, _channel (0)
	, _samples_per_pixel (0)
	, _height (64)
	, _wave_color (0xffffffff)
	, _show_zero (true)
	, _zero_color (0xff0000ff)
	, _clip_color (0xff0000ff)
	, _logscaled (_global_logscaled)
	, _shape (_global_shape)
	, _gradient_depth (_global_gradient_depth)
	, _shape_independent (false)
	, _logscaled_independent (false)
	, _gradient_depth_independent (false)
	, _amplitude_above_axis (1.0)
	, _region_start (region->start())
{
	VisualPropertiesChanged.connect_same_thread (invalidation_connection, boost::bind (&WaveView::handle_visual_property_change, this));
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
		invalidate_image_cache ();
	}
}

void
WaveView::set_fill_color (Color c)
{
	if (c != _fill_color) {
		invalidate_image_cache ();
		Fill::set_fill_color (c);
	}
}

void
WaveView::set_outline_color (Color c)
{
	if (c != _outline_color) {
		invalidate_image_cache ();
		Outline::set_outline_color (c);
	}
}

void
WaveView::set_samples_per_pixel (double samples_per_pixel)
{
	if (samples_per_pixel != _samples_per_pixel) {
		begin_change ();

		_samples_per_pixel = samples_per_pixel;
		
		_bounding_box_dirty = true;
		
		end_change ();
		
		invalidate_whole_cache ();
	}
}

static inline double
to_src_sample_offset (frameoffset_t src_sample_start, double pixel_offset, double spp)
{
	return llrintf (src_sample_start + (pixel_offset * spp));
}

static inline double
to_pixel_offset (frameoffset_t src_sample_start, double sample_offset, double spp)
{
	return llrintf ((sample_offset - src_sample_start) / spp);
}

void
WaveView::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	assert (_samples_per_pixel != 0);

	if (!_region) {
		return;
	}

	Rect self = item_to_window (Rect (0.0, 0.0, _region->length() / _samples_per_pixel, _height));
	boost::optional<Rect> draw = self.intersection (area);

	if (!draw) {
		return;
	}

	/* pixel coordinates */

	double       start = draw->x0;
	double const end   = draw->x1;

	list<CacheEntry*>::iterator cache = _cache.begin ();

	// cerr << name << " draw " << area << "self = " << self << "\n\twill use " << draw.get() << endl;
#if 0
	cerr << "  Cache contains " << _cache.size() << endl;
	while (cache != _cache.end()) {
		cerr << "\tsample span " << (*cache)->start() << " .. " << (*cache)->end()
		     << " pixel span " 
		     << to_pixel_offset (_region_start, (*cache)->start(), _samples_per_pixel)
		     << " .. "
		     << to_pixel_offset (_region_start, (*cache)->end(), _samples_per_pixel)
		     << endl;
		++cache;
	}
	cache = _cache.begin();
#endif

	while ((end - start) > 1.0) {

		// cerr << "***** RANGE = " << start << " .. " << end << " = " << end - start << endl;

		frameoffset_t start_sample_offset = to_src_sample_offset (_region_start, start, _samples_per_pixel);

		/* Step through cache entries that end at or before our current position */

		while (cache != _cache.end() && (*cache)->end() <= start_sample_offset) {
			++cache;
		}

		/* Now either:
		   1. we have run out of cache entries
		   2. the one we are looking at finishes after start(_sample_offset) but also starts after start(_sample_offset).
		   3. the one we are looking at finishes after start(_sample_offset) and starts before start(_sample_offset).

		   Set up a pointer to the cache entry that we will use on this iteration.
		*/

		CacheEntry* image = 0;

		if (cache == _cache.end ()) {

			/* Case 1: we have run out of cache entries, so make a new one for
			   the whole required area and put it in the list.
			   
			   We would like to avoid lots of little images in the
			   cache, so when we create a new one, make it as wide
			   as possible, within a sensible limit (here, the
			   visible width of the canvas we're on). 

			   However, we don't want to try to make it larger than 
			   the region actually is, so clamp with that too.
			*/

			double const rend  = _region->length() / _samples_per_pixel;
			double const endpoint = min (rend, max (end, start + _canvas->visible_area().width()));

			CacheEntry* c = new CacheEntry (this, 
							start_sample_offset,
							to_src_sample_offset (_region_start, endpoint, _samples_per_pixel),
							endpoint - start);
			_cache.push_back (c);
			image = c;

		} else if ((*cache)->start() > start_sample_offset) {

			/* Case 2: we have a cache entry, but it starts after
			 * start(_sample_offset), so we need another one for
			 * the missing bit.
			 *  
			 * Create a new cached image that extends as far as the
			 * next cached image's start, or the end of the region,
			 * or the end of the render area, whichever comes first.
			 */

			double    end_pixel;
			double end_sample_offset;
			int npeaks;

			if (end_sample_offset < (*cache)->start()) {
				double const rend  = _region->length() / _samples_per_pixel;
				end_sample_offset = to_src_sample_offset (_region_start, end_pixel, _samples_per_pixel);
				end_pixel = min (rend, end);
			} else {
				end_sample_offset = (*cache)->start();
				end_pixel = to_pixel_offset (_region_start, end_sample_offset, _samples_per_pixel);
			}
		
			npeaks = end_pixel - start;
			assert (npeaks > 0);
			
			CacheEntry* c = new CacheEntry (this,
							start_sample_offset,
							end_sample_offset,
							npeaks);

			cache = _cache.insert (cache, c);
			++cache;
			image = c;

		} else {

			/* Case 3: we have a cache entry that will do at least some of what
			   we have left, so render it.
			*/

			image = *cache;
			++cache;

		}

		

		double this_end = min (end, to_pixel_offset (_region_start, image->end (), _samples_per_pixel));
		double const image_origin = to_pixel_offset (_region_start, image->start(), _samples_per_pixel);
#if 0
		cerr << "\t\tDraw image between "
		     << start
		     << " .. "
		     << this_end
		     << " using image spanning "
		     << image->start()
		     << " .. "
		     << image->end ()
		     << " pixels "
		     << to_pixel_offset (_region_start, image->start(), _samples_per_pixel)
		     << " .. "
		     << to_pixel_offset (_region_start, image->end(), _samples_per_pixel)
		     << endl;
#endif

		// cerr << "Fill rect " << draw->x0 << ", " << self.y0 << ' ' << draw->width() << " x " << draw->height() << endl;

		context->rectangle (start, draw->y0, this_end - start, _height);
		context->set_source (image->image(), self.x0 - image_origin, self.y0);
		context->fill ();
		
		start = this_end;
	}
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
		
		_height = height;
		
		_bounding_box_dirty = true;
		end_change ();
		
		invalidate_image_cache ();
	}
}

void
WaveView::set_channel (int channel)
{
	if (channel != _channel) {
		begin_change ();
		
		_channel = channel;
		
		_bounding_box_dirty = true;
		end_change ();
		
		invalidate_whole_cache ();
	}
}

void
WaveView::invalidate_whole_cache ()
{
	begin_visual_change ();

	for (list<CacheEntry*>::iterator i = _cache.begin(); i != _cache.end(); ++i) {
		delete *i;
	}

	_cache.clear ();

	end_visual_change ();
}

void
WaveView::invalidate_image_cache ()
{
	begin_visual_change ();
	
	for (list<CacheEntry*>::iterator i = _cache.begin(); i != _cache.end(); ++i) {
		(*i)->clear_image ();
	}
	
	end_visual_change ();
}

void
WaveView::set_logscaled (bool yn)
{
	if (_logscaled != yn) {
		_logscaled = yn;
		invalidate_image_cache ();
	}
}

void
WaveView::gain_changed ()
{
	invalidate_whole_cache ();
}

void
WaveView::set_zero_color (Color c)
{
	if (_zero_color != c) {
		_zero_color = c;
		invalidate_image_cache ();
	}
}

void
WaveView::set_clip_color (Color c)
{
	if (_clip_color != c) {
		_clip_color = c;
		invalidate_image_cache ();
	}
}

void
WaveView::set_show_zero_line (bool yn)
{
	if (_show_zero != yn) {
		_show_zero = yn;
		invalidate_image_cache ();
	}
}

void
WaveView::set_shape (Shape s)
{
	if (_shape != s) {
		_shape = s;
		invalidate_image_cache ();
	}
}

void
WaveView::set_amplitude_above_axis (double a)
{
	if (_amplitude_above_axis != a) {
		_amplitude_above_axis = a;
		invalidate_image_cache ();
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

	frameoffset_t s = _region->start();

	if (s != _region_start) {
		/* if the region start changes, the information we have 
		   in the image cache is out of date and not useful
		   since it will fragmented into little pieces. invalidate
		   the cache.
		*/
		_region_start = _region->start();
		invalidate_whole_cache ();
	}

	_bounding_box_dirty = true;
	compute_bounding_box ();

	end_change ();
}

WaveView::CacheEntry::CacheEntry (WaveView const * wave_view, double start, double end, int npeaks)
	: _wave_view (wave_view)
	, _start (start)
	, _end (end)
	, _n_peaks (npeaks)
{
	_peaks.reset (new PeakData[_n_peaks]);

	_wave_view->_region->read_peaks (_peaks.get(), _n_peaks, 
					 (framecnt_t) floor (_start), 
					 (framecnt_t) ceil (_end - _start), 
					 _wave_view->_channel, _wave_view->_samples_per_pixel);
}

WaveView::CacheEntry::~CacheEntry ()
{
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

Cairo::RefPtr<Cairo::ImageSurface>
WaveView::CacheEntry::image ()
{
	if (!_image) {

		_image = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, _n_peaks, _wave_view->_height);
		Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create (_image);

		/* Draw the edge of the waveform, top half first, the loop back
		 * for the bottom half to create a clockwise path
		 */

		context->begin_new_path();

		if (_wave_view->_shape == WaveView::Rectified) {

			/* top edge of waveform is based on max (fabs (peak_min, peak_max))
			 */

			if (_wave_view->_logscaled) {
				for (int i = 0; i < _n_peaks; ++i) {
					context->line_to (i + 0.5, position (alt_log_meter (fast_coefficient_to_dB (
												    max (fabs (_peaks[i].max), fabs (_peaks[i].min))))));
				}
			} else {
				for (int i = 0; i < _n_peaks; ++i) {
					context->line_to (i + 0.5, position (max (fabs (_peaks[i].max), fabs (_peaks[i].min))));
				}
			}

		} else {
			if (_wave_view->_logscaled) {
				for (int i = 0; i < _n_peaks; ++i) {
					Coord y = _peaks[i].max;
					
					if (y > 0.0) {
						y = position (alt_log_meter (fast_coefficient_to_dB (y)));
					} else if (y < 0.0) {
						y = position (-alt_log_meter (fast_coefficient_to_dB (-y)));
					} else {
						y = position (0.0);
					}
					context->line_to (i + 0.5, y);
				} 
			} else {
				for (int i = 0; i < _n_peaks; ++i) {
					context->line_to (i + 0.5, position (_peaks[i].max));
				}
			}
		}

		/* from final top point, move out of the clip zone */

		context->line_to (_n_peaks + 10, position (0.0));
	
		/* bottom half, in reverse */
	
		if (_wave_view->_shape == WaveView::Rectified) {
			
			/* lower half: drop to the bottom, then a line back to
			 * beyond the left edge of the clip region 
			 */

			context->line_to (_n_peaks + 10, _wave_view->_height);
			context->line_to (-10.0, _wave_view->_height);

		} else {

			if (_wave_view->_logscaled) {
				for (int i = _n_peaks-1; i >= 0; --i) {
					Coord y = _peaks[i].min;
					if (y > 0.0) {
						context->line_to (i + 0.5, position (alt_log_meter (fast_coefficient_to_dB (y))));
					} else if (y < 0.0) {
						context->line_to (i + 0.5, position (-alt_log_meter (fast_coefficient_to_dB (-y))));
					} else {
						context->line_to (i + 0.5, position (0.0));
					}
				} 
			} else {
				for (int i = _n_peaks-1; i >= 0; --i) {
					context->line_to (i + 0.5, position (_peaks[i].min));
				}
			}
		
			/* from final bottom point, move out of the clip zone */
			
			context->line_to (-10.0, position (0.0));
		}

		context->close_path ();

		if (_wave_view->gradient_depth() != 0.0) {
			
			Cairo::RefPtr<Cairo::LinearGradient> gradient (Cairo::LinearGradient::create (0, 0, 0, _wave_view->_height));
			
			double stops[3];
			
			double r, g, b, a;

			if (_wave_view->_shape == Rectified) {
				stops[0] = 0.1;
				stops[0] = 0.3;
				stops[0] = 0.9;
			} else {
				stops[0] = 0.1;
				stops[1] = 0.5;
				stops[2] = 0.9;
			}

			color_to_rgba (_wave_view->_fill_color, r, g, b, a);
			gradient->add_color_stop_rgba (stops[0], r, g, b, a);
			gradient->add_color_stop_rgba (stops[2], r, g, b, a);
			
			/* generate a new color for the middle of the gradient */
			double h, s, v;
			color_to_hsv (_wave_view->_fill_color, h, s, v);
			/* tone down the saturation */
			s *= 1.0 - _wave_view->gradient_depth();
			Color center = hsv_to_color (h, s, v, a);
			color_to_rgba (center, r, g, b, a);
			gradient->add_color_stop_rgba (stops[1], r, g, b, a);
			
			context->set_source (gradient);
		} else {
			set_source_rgba (context, _wave_view->_fill_color);
		}

		context->fill_preserve ();
		_wave_view->setup_outline_context (context);
		context->stroke ();

		if (_wave_view->show_zero_line()) {
			set_source_rgba (context, _wave_view->_zero_color);
			context->move_to (0, position (0.0));
			context->line_to (_n_peaks, position (0.0));
			context->stroke ();
		}
	}

	return _image;
}


Coord
WaveView::CacheEntry::position (double s) const
{
	switch (_wave_view->_shape) {
	case Rectified:
		return _wave_view->_height - (s * _wave_view->_height);
	default:
		break;
	}
	return (1.0-s) * (_wave_view->_height / 2.0);
}

void
WaveView::CacheEntry::clear_image ()
{
	_image.clear ();
}
	    
void
WaveView::set_global_gradient_depth (double depth)
{
	if (_global_gradient_depth != depth) {
		_global_gradient_depth = depth;
		VisualPropertiesChanged (); /* EMIT SIGNAL */
	}
}
