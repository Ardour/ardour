#include <boost/shared_ptr.hpp>
#include "pbd/properties.h"
#include "ardour/types.h"
#include "canvas/item.h"
#include "canvas/fill.h"
#include "canvas/outline.h"

namespace ARDOUR {
	class AudioRegion;
}

class WaveViewTest;
	
namespace ArdourCanvas {

class WaveView : virtual public Item, public Outline, public Fill
{
public:
	WaveView (Group *, boost::shared_ptr<ARDOUR::AudioRegion>);

	void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box () const;

	XMLNode* get_state () const;
	void set_state (XMLNode const *);

	void set_frames_per_pixel (double);
	void set_height (Distance);
	void set_channel (int);
	void set_region_start (ARDOUR::frameoffset_t);
	
	void region_resized ();

	/* XXX */
	void rebuild () {}

#ifdef CANVAS_COMPATIBILITY	
	void*& property_gain_src () {
		return _foo_void;
	}
	void*& property_gain_function () {
		return _foo_void;
	}
	bool& property_rectified () {
		return _foo_bool;
	}
	bool& property_logscaled () {
		return _foo_bool;
	}
	double& property_amplitude_above_axis () {
		return _foo_double;
	}
	Color& property_clip_color () {
		return _foo_uint;
	}
	Color& property_zero_color () {
		return _foo_uint;
	}

private:
	void* _foo_void;
	bool _foo_bool;
	int _foo_int;
	Color _foo_uint;
	double _foo_double;
#endif

	class CacheEntry
	{
	public:
		CacheEntry (WaveView const *, int, int);
		~CacheEntry ();

		int start () const {
			return _start;
		}

		int end () const {
			return _end;
		}

		ARDOUR::PeakData* peaks () const {
			return _peaks;
		}

		Glib::RefPtr<Gdk::Pixbuf> pixbuf ();
		void clear_pixbuf ();

	private:
		Coord position (float) const;
		
		WaveView const * _wave_view;
		int _start;
		int _end;
		int _n_peaks;
		ARDOUR::PeakData* _peaks;
		Glib::RefPtr<Gdk::Pixbuf> _pixbuf;
	};

	friend class CacheEntry;
	friend class ::WaveViewTest;

	void invalidate_whole_cache ();
	void invalidate_pixbuf_cache ();

	boost::shared_ptr<ARDOUR::AudioRegion> _region;
	int _channel;
	double _frames_per_pixel;
	Coord _height;
	Color _wave_color;
	/** The `start' value to use for the region; we can't use the region's
	 *  value as the crossfade editor needs to alter it.
	 */
	ARDOUR::frameoffset_t _region_start;
	
	mutable std::list<CacheEntry*> _cache;
};

}
