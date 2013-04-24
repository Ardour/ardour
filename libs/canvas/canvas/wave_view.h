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

#include "pbd/properties.h"

#include "ardour/types.h"

#include <glibmm/refptr.h>

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

class WaveView : virtual public Item, public Outline, public Fill
{
public:
        enum Shape { 
		Normal,
		Rectified,
        };

	WaveView (Group *, boost::shared_ptr<ARDOUR::AudioRegion>);

	void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box () const;
    
	void set_samples_per_pixel (double);
	void set_height (Distance);
	void set_channel (int);
	void set_region_start (ARDOUR::frameoffset_t);

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

        
        /* currently missing because we don't need them (yet):
	   set_shape_independent();
	   set_logscaled_independent()
        */

        static void set_global_gradient_depth (double);
        static void set_global_logscaled (bool);
        static void set_global_shape (Shape);
    
        static double  global_gradient_depth()  { return _global_gradient_depth; }
        static bool    global_logscaled()  { return _global_logscaled; }
        static Shape   global_shape()  { return _global_shape; }

        void set_amplitude_above_axis (double v);
        double amplitude_above_axis () const { return _amplitude_above_axis; }

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

    /** A cached, pre-rendered image of some section of a waveform.
	
	It spans a range given relative to the start of the source
	of the waveform data, so a range from N..M corresponds
	to the sample range N..M within the source.
	
	Invalidated by a changes to:

               samples_per_pixel
               colors
	       height

    */

	class CacheEntry
	{
	  public:
  	        CacheEntry (WaveView const *, double, double, int);
		~CacheEntry ();

	        double start () const {
			return _start;
		}

  	        double end () const {
			return _end;
		}

     	        boost::shared_array<ARDOUR::PeakData> peaks () const {
			return _peaks;
		}

                Cairo::RefPtr<Cairo::ImageSurface> image();
	        void clear_image ();

	private:
		Coord position (Coord) const;
		
		WaveView const * _wave_view;
	        
	        double _start;
	        double _end;
	        int _n_peaks; 

	        boost::shared_array<ARDOUR::PeakData> _peaks;
	        Cairo::RefPtr<Cairo::ImageSurface> _image;
	};

	friend class CacheEntry;
	friend class ::WaveViewTest;

	void invalidate_whole_cache ();
	void invalidate_image_cache ();

	boost::shared_ptr<ARDOUR::AudioRegion> _region;
	int    _channel;
	double _samples_per_pixel;
	Coord  _height;
	Color  _wave_color;
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

	/** The `start' value to use for the region; we can't use the region's
	 *  value as the crossfade editor needs to alter it.
	 */
	ARDOUR::frameoffset_t _region_start;

	mutable std::list<CacheEntry*> _cache;
       
        PBD::ScopedConnection invalidation_connection;

        static double _global_gradient_depth;
        static bool   _global_logscaled;
        static Shape  _global_shape;

        static PBD::Signal0<void> VisualPropertiesChanged;

        void handle_visual_property_change ();
};

}
