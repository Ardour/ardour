/*
    Copyright (C) 2001, 2006 Paul Davis

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

#ifndef __ardour_streamview_h__
#define __ardour_streamview_h__

#include <list>
#include <cmath>

#include "ardour/location.h"
#include "enums.h"
#include "simplerect.h"
#include "canvas.h"

namespace Gdk {
	class Color;
}

namespace ARDOUR {
	class Route;
	class Diskstream;
	class Crossfade;
	class PeakData;
	class Region;
	class Source;
}

struct RecBoxInfo {
	ArdourCanvas::SimpleRect* rectangle;
	nframes_t                 start;
	nframes_t                 length;
};

class Selectable;
class RouteTimeAxisView;
class RegionView;
class RegionSelection;
class CrossfadeView;
class Selection;

class StreamView : public sigc::trackable
{
public:
	virtual ~StreamView ();

	RouteTimeAxisView&       trackview()       { return _trackview; }
	const RouteTimeAxisView& trackview() const { return _trackview; }

	void attach ();

	void set_zoom_all();

	int set_position (gdouble x, gdouble y);
	virtual int set_height (double);

	virtual int set_samples_per_unit (gdouble spp);
	gdouble     get_samples_per_unit () { return _samples_per_unit; }

 	void set_layer_display (LayerDisplay);
	LayerDisplay layer_display () const { return _layer_display; }

	ArdourCanvas::Group* background_group() { return _background_group; }
	ArdourCanvas::Group* canvas_item() { return canvas_group; }

	enum ColorTarget {
		RegionColor,
		StreamBaseColor
	};

	Gdk::Color get_region_color () const { return region_color; }
	void       apply_color (Gdk::Color&, ColorTarget t);

	RegionView*  find_view (boost::shared_ptr<const ARDOUR::Region>);
	void         foreach_regionview (sigc::slot<void,RegionView*> slot);

	void set_selected_regionviews (RegionSelection&);
	void get_selectables (nframes_t, nframes_t, double, double, std::list<Selectable* >&);
	void get_inverted_selectables (Selection&, std::list<Selectable* >& results);

	virtual void update_contents_metrics(boost::shared_ptr<ARDOUR::Region>) {}

	void add_region_view_weak (boost::weak_ptr<ARDOUR::Region> r);
	void add_region_view (boost::shared_ptr<ARDOUR::Region>);

	void region_layered (RegionView*);
	virtual void update_contents_height ();

	virtual void redisplay_diskstream () = 0;
	double child_height () const;
	ARDOUR::layer_t layers () const { return _layers; }

	virtual RegionView* create_region_view (boost::shared_ptr<ARDOUR::Region>, bool, bool) {
		return 0;
	}

	sigc::signal<void,RegionView*> RegionViewAdded;
	sigc::signal<void> HeightChanged;

protected:
	StreamView (RouteTimeAxisView&, ArdourCanvas::Group* group = NULL);

	void         transport_changed();
	void         transport_looped();
	void         rec_enable_changed();
	void         sess_rec_enable_changed();
	virtual void setup_rec_box () = 0;
	void         update_rec_box ();

	virtual RegionView* add_region_view_internal (boost::shared_ptr<ARDOUR::Region>,
		      bool wait_for_waves, bool recording = false) = 0;
	virtual void remove_region_view (boost::weak_ptr<ARDOUR::Region> );

	void         display_diskstream (boost::shared_ptr<ARDOUR::Diskstream>);
	virtual void undisplay_diskstream ();
	void         diskstream_changed ();
	void         layer_regions ();

	virtual void playlist_changed_weak (boost::weak_ptr<ARDOUR::Diskstream>);
	virtual void playlist_changed (boost::shared_ptr<ARDOUR::Diskstream>);
	virtual void playlist_modified_weak (boost::weak_ptr<ARDOUR::Diskstream>);
	virtual void playlist_modified (boost::shared_ptr<ARDOUR::Diskstream>);

	virtual void color_handler () = 0;

	RouteTimeAxisView&        _trackview;
	bool                      owns_canvas_group;
	ArdourCanvas::Group*      _background_group;
	ArdourCanvas::Group*      canvas_group;
	ArdourCanvas::SimpleRect* canvas_rect; /* frame around the whole thing */

	typedef std::list<RegionView* > RegionViewList;
	RegionViewList  region_views;

	double _samples_per_unit;

	sigc::connection       screen_update_connection;
	std::vector<RecBoxInfo>     rec_rects;
	std::list< std::pair<boost::shared_ptr<ARDOUR::Region>,RegionView* > > rec_regions;
	bool                   rec_updating;
	bool                   rec_active;
	bool                   use_rec_regions;

	Gdk::Color region_color;      ///< Contained region color
	uint32_t   stream_base_color; ///< Background color

	std::vector<sigc::connection> playlist_connections;
	sigc::connection         playlist_change_connection;

	ARDOUR::layer_t _layers;
	LayerDisplay    _layer_display;

	double height;

	std::list<sigc::connection> rec_data_ready_connections;
	jack_nframes_t         last_rec_data_frame;

private:
	void update_coverage_frames ();
};

#endif /* __ardour_streamview_h__ */

