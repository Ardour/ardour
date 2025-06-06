/*
 * Copyright (C) 2005-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#pragma once

#include <list>
#include <cmath>

#include "pbd/signals.h"

#include "ardour/location.h"

#include "enums.h"
#include "selectable.h"
#include "time_axis_view_item.h"
#include "view_background.h"

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
	class Container;
}

struct RecBoxInfo {
	ArdourCanvas::Rectangle* rectangle;
	samplepos_t              start;
	ARDOUR::samplecnt_t      length;
};

class RouteTimeAxisView;
class RegionView;
class RegionSelection;
class CrossfadeView;
class Selection;

class StreamView : public PBD::ScopedConnectionList, public virtual ViewBackground, public SelectableOwner
{
public:
	virtual ~StreamView ();

	RouteTimeAxisView&       trackview()       { return _trackview; }
	const RouteTimeAxisView& trackview() const { return _trackview; }

	void attach ();

	void set_zoom_all();

	int height() const;
	int width() const;
	int contents_height() const {
		return child_height() - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE - 2;
	}
	int y_position () const;

	int set_position (gdouble x, gdouble y);
	virtual int set_height (double);

	virtual int set_samples_per_pixel (double);
	gdouble     get_samples_per_pixel () const { return _samples_per_pixel; }

	virtual void set_layer_display (LayerDisplay);
	virtual bool can_change_layer_display() const { return true; }
	LayerDisplay layer_display () const { return _layer_display; }

	virtual ArdourCanvas::Container* region_canvas () const { return _canvas_group; }

	enum ColorTarget {
		RegionColor,
		StreamBaseColor
	};

	uint32_t get_region_color () const { return region_color; }
	void     apply_color (uint32_t, ColorTarget t);
	void     apply_color (Gdk::Color const &, ColorTarget t);

	uint32_t     num_selected_regionviews () const;

	RegionView*  find_view (std::shared_ptr<const ARDOUR::Region>);
	void         foreach_regionview (sigc::slot<void,RegionView*> slot);
	void         foreach_selected_regionview (sigc::slot<void,RegionView*> slot);

	void set_selected_regionviews (RegionSelection&);
	void _get_selectables (Temporal::timepos_t const &, Temporal::timepos_t const &, double, double, std::list<Selectable* >&, bool within);
	void get_inverted_selectables (Selection&, std::list<Selectable* >& results);
	void get_regionviews_at_or_after (Temporal::timepos_t const &, RegionSelection&);

	virtual void update_contents_metrics(std::shared_ptr<ARDOUR::Region>) {}

	void add_region_view (std::weak_ptr<ARDOUR::Region>);

	void region_layered (RegionView*);
	virtual void update_contents_height ();

	virtual void redisplay_track () = 0;
	double child_height () const;
	ARDOUR::layer_t layers () const { return _layers; }

	virtual RegionView* create_region_view (std::shared_ptr<ARDOUR::Region>, bool, bool) {
		return 0;
	}

	void check_record_layers (std::shared_ptr<ARDOUR::Region>, ARDOUR::samplepos_t);

	virtual void playlist_layered (std::weak_ptr<ARDOUR::Track>);
	void update_coverage_frame ();

	sigc::signal<void, RegionView*> RegionViewAdded;
	sigc::signal<void> RegionViewRemoved;
	/** Emitted when the height of regions has changed */
	sigc::signal<void> ContentsHeightChanged;

	virtual void parameter_changed (std::string const &);

protected:
	StreamView (RouteTimeAxisView&, ArdourCanvas::Container* canvas_group = 0);

	void transport_changed();
	void transport_looped();
	void rec_enable_changed();
	void sess_rec_enable_changed();
	void create_rec_box(samplepos_t sample_pos, double width);
	void cleanup_rec_box ();

	virtual void setup_rec_box () = 0;
	virtual void update_rec_box ();

	virtual RegionView* add_region_view_internal (std::shared_ptr<ARDOUR::Region>,
		      bool wait_for_waves, bool recording = false) = 0;
	virtual void remove_region_view (std::weak_ptr<ARDOUR::Region> );

	void         display_track (std::shared_ptr<ARDOUR::Track>);
	virtual void undisplay_track ();
	void layer_regions ();

	void playlist_switched (std::weak_ptr<ARDOUR::Track>);

	virtual void color_handler () = 0;

	RouteTimeAxisView&       _trackview;
	ArdourCanvas::Container* _canvas_group;
	ArdourCanvas::Rectangle*  canvas_rect; /* frame around the whole thing */

	typedef std::list<RegionView* > RegionViewList;
	RegionViewList region_views;

	double _samples_per_pixel;

	sigc::connection        screen_update_connection;
	std::vector<RecBoxInfo> rec_rects;
	std::list< std::pair<std::shared_ptr<ARDOUR::Region>,RegionView* > > rec_regions;
	bool                    rec_updating;
	bool                    rec_active;

	uint32_t region_color;      ///< Contained region color
	uint32_t stream_base_color; ///< Background color

	PBD::ScopedConnectionList playlist_connections;
	PBD::ScopedConnection playlist_switched_connection;

	ARDOUR::layer_t _layers;
	LayerDisplay    _layer_display;

	double _height;

	PBD::ScopedConnectionList rec_data_ready_connections;
	samplepos_t               last_rec_data_sample;

	/* When recording, the session time at which a new layer must be created for the region
	   being recorded, or max_samplepos if not applicable.
	*/
	samplepos_t _new_rec_layer_time;
	void setup_new_rec_layer_time (std::shared_ptr<ARDOUR::Region>);
};


