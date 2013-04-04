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

#include "pbd/signals.h"

#include "ardour/location.h"
#include "enums.h"

namespace Gdk {
	class Color;
}

namespace ARDOUR {
	class Crossfade;
	class Region;
	class Route;
	class Source;
	class Track;
	struct PeakData;
}

namespace ArdourCanvas {
	class Rectangle;
	class Group;
}

struct RecBoxInfo {
	ArdourCanvas::Rectangle*   rectangle;
	framepos_t                 start;
	ARDOUR::framecnt_t         length;
};

class Selectable;
class RouteTimeAxisView;
class RegionView;
class RegionSelection;
class CrossfadeView;
class Selection;

class StreamView : public sigc::trackable, public PBD::ScopedConnectionList
{
public:
	virtual ~StreamView ();

	RouteTimeAxisView&       trackview()       { return _trackview; }
	const RouteTimeAxisView& trackview() const { return _trackview; }

	void attach ();

	void set_zoom_all();

	int set_position (gdouble x, gdouble y);
	virtual int set_height (double);

	virtual int set_frames_per_pixel (double);
	gdouble     get_frames_per_pixel () const { return _frames_per_pixel; }
	virtual void horizontal_position_changed () {}

        virtual void enter_internal_edit_mode ();
        virtual void leave_internal_edit_mode ();

 	void set_layer_display (LayerDisplay);
	LayerDisplay layer_display () const { return _layer_display; }

	ArdourCanvas::Group* background_group() { return _background_group; }
	ArdourCanvas::Group* canvas_item() { return _canvas_group; }

	enum ColorTarget {
		RegionColor,
		StreamBaseColor
	};

	Gdk::Color get_region_color () const { return region_color; }
	void       apply_color (Gdk::Color, ColorTarget t);

	uint32_t     num_selected_regionviews () const;

	RegionView*  find_view (boost::shared_ptr<const ARDOUR::Region>);
	void         foreach_regionview (sigc::slot<void,RegionView*> slot);
	void         foreach_selected_regionview (sigc::slot<void,RegionView*> slot);

	void set_selected_regionviews (RegionSelection&);
	void get_selectables (ARDOUR::framepos_t, ARDOUR::framepos_t, double, double, std::list<Selectable* >&);
	void get_inverted_selectables (Selection&, std::list<Selectable* >& results);

	virtual void update_contents_metrics(boost::shared_ptr<ARDOUR::Region>) {}

	void add_region_view (boost::weak_ptr<ARDOUR::Region>);

	void region_layered (RegionView*);
	virtual void update_contents_height ();

	virtual void redisplay_track () = 0;
	double child_height () const;
	ARDOUR::layer_t layers () const { return _layers; }

	virtual RegionView* create_region_view (boost::shared_ptr<ARDOUR::Region>, bool, bool) {
		return 0;
	}

	void check_record_layers (boost::shared_ptr<ARDOUR::Region>, ARDOUR::framepos_t);

	virtual void playlist_layered (boost::weak_ptr<ARDOUR::Track>);
	
	sigc::signal<void, RegionView*> RegionViewAdded;
	sigc::signal<void> RegionViewRemoved;
	/** Emitted when the height of regions has changed */
	sigc::signal<void> ContentsHeightChanged;

protected:
	StreamView (RouteTimeAxisView&, ArdourCanvas::Group* background_group = 0, ArdourCanvas::Group* canvas_group = 0);

	void         transport_changed();
	void         transport_looped();
	void         rec_enable_changed();
	void         sess_rec_enable_changed();
	virtual void setup_rec_box () = 0;
	virtual void update_rec_box ();

	virtual RegionView* add_region_view_internal (boost::shared_ptr<ARDOUR::Region>,
		      bool wait_for_waves, bool recording = false) = 0;
	virtual void remove_region_view (boost::weak_ptr<ARDOUR::Region> );

	void         display_track (boost::shared_ptr<ARDOUR::Track>);
	virtual void undisplay_track ();
	void         diskstream_changed ();
	void         layer_regions ();

	void playlist_switched (boost::weak_ptr<ARDOUR::Track>);

	virtual void color_handler () = 0;

	RouteTimeAxisView&        _trackview;
	bool                      owns_background_group;
	bool                      owns_canvas_group;
	ArdourCanvas::Group*      _background_group;
	ArdourCanvas::Group*      _canvas_group;
	ArdourCanvas::Rectangle*  canvas_rect; /* frame around the whole thing */

	typedef std::list<RegionView* > RegionViewList;
	RegionViewList  region_views;

	double _frames_per_pixel;

	sigc::connection       screen_update_connection;
	std::vector<RecBoxInfo>     rec_rects;
	std::list< std::pair<boost::shared_ptr<ARDOUR::Region>,RegionView* > > rec_regions;
	bool                   rec_updating;
	bool                   rec_active;

	Gdk::Color region_color;      ///< Contained region color
	uint32_t   stream_base_color; ///< Background color

	PBD::ScopedConnectionList playlist_connections;
	PBD::ScopedConnection playlist_switched_connection;

	ARDOUR::layer_t _layers;
	LayerDisplay    _layer_display;

	double height;

	PBD::ScopedConnectionList rec_data_ready_connections;
	framepos_t                last_rec_data_frame;

	/* When recording, the session time at which a new layer must be created for the region
	   being recorded, or max_framepos if not applicable.
	*/
	framepos_t _new_rec_layer_time;
	void setup_new_rec_layer_time (boost::shared_ptr<ARDOUR::Region>);

private:
	void update_coverage_frames ();
};

#endif /* __ardour_streamview_h__ */

