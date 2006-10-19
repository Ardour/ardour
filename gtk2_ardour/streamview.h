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
#include <map>
#include <cmath>

#include <ardour/location.h>
#include "enums.h"
#include "simplerect.h"
#include "canvas.h"
#include "color.h"

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
	nframes_t            start;
	nframes_t            length;
};

class PublicEditor;
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

	RouteTimeAxisView& trackview() { return _trackview; }

	void attach ();

	void set_zoom_all();

	int set_position (gdouble x, gdouble y);
	virtual int set_height (gdouble);

	virtual int set_samples_per_unit (gdouble spp);
	gdouble     get_samples_per_unit () { return _samples_per_unit; }

	ArdourCanvas::Item* canvas_item() { return canvas_group; }

	enum ColorTarget {
		RegionColor,
		StreamBaseColor
	};

	Gdk::Color get_region_color () const { return region_color; }
	void       apply_color (Gdk::Color&, ColorTarget t);

	RegionView*  find_view (boost::shared_ptr<const ARDOUR::Region>);
	void         foreach_regionview (sigc::slot<void,RegionView*> slot);

	void set_selected_regionviews (RegionSelection&);
	void get_selectables (nframes_t start, nframes_t end, list<Selectable* >&);
	void get_inverted_selectables (Selection&, list<Selectable* >& results);

	void add_region_view (boost::shared_ptr<ARDOUR::Region>);
	void region_layered (RegionView*);
	
	sigc::signal<void,RegionView*> RegionViewAdded;

protected:
	StreamView (RouteTimeAxisView&);
	
//private: (FIXME?)

	void         transport_changed();
	void         rec_enable_changed();
	void         sess_rec_enable_changed();
	virtual void setup_rec_box () = 0;
	void         update_rec_box ();
	virtual void update_rec_regions () = 0;
	
	virtual void add_region_view_internal (boost::shared_ptr<ARDOUR::Region>, bool wait_for_waves) = 0;
	virtual void remove_region_view (boost::weak_ptr<ARDOUR::Region> );
	//void         remove_rec_region (boost::shared_ptr<ARDOUR::Region>); (unused)

	void         display_diskstream (boost::shared_ptr<ARDOUR::Diskstream>);
	virtual void undisplay_diskstream ();
	virtual void redisplay_diskstream () = 0;
	void         diskstream_changed ();
	
	virtual void playlist_changed (boost::shared_ptr<ARDOUR::Diskstream>);
	virtual void playlist_modified ();
	
	virtual void color_handler (ColorID, uint32_t) = 0;


	RouteTimeAxisView&        _trackview;
	ArdourCanvas::Group*      canvas_group;
	ArdourCanvas::SimpleRect* canvas_rect; /* frame around the whole thing */

	typedef list<RegionView* > RegionViewList;
	RegionViewList  region_views;

	double _samples_per_unit;

	sigc::connection       screen_update_connection;
	vector<RecBoxInfo>     rec_rects;
	list<boost::shared_ptr<ARDOUR::Region> > rec_regions;
	bool                   rec_updating;
	bool                   rec_active;
	bool                   use_rec_regions;
	
	Gdk::Color region_color;      ///< Contained region color
	uint32_t   stream_base_color; ///< Background color

	vector<sigc::connection> playlist_connections;
	sigc::connection         playlist_change_connection;
};

#endif /* __ardour_streamview_h__ */

