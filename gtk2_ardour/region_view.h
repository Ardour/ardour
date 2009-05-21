/*
    Copyright (C) 2001-2006 Paul Davis 

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

#ifndef __gtk_ardour_region_view_h__
#define __gtk_ardour_region_view_h__

#include <vector>

#include <libgnomecanvasmm.h>
#include <libgnomecanvasmm/polygon.h>
#include <sigc++/signal.h>
#include "ardour/region.h"
#include "ardour/beats_frames_converter.h"

#include "time_axis_view_item.h"
#include "automation_line.h"
#include "enums.h"
#include "canvas.h"

class TimeAxisView;
class RegionEditor;
class GhostRegion;
class AutomationTimeAxisView;
class AutomationRegionView;

class RegionView : public TimeAxisViewItem
{
  public:
	RegionView (ArdourCanvas::Group* parent, 
	            TimeAxisView&        time_view,
	            boost::shared_ptr<ARDOUR::Region> region,
	            double               samples_per_unit,
	            Gdk::Color&          basic_color);

	RegionView (const RegionView& other);
	RegionView (const RegionView& other, boost::shared_ptr<ARDOUR::Region> other_region);

	~RegionView ();
	
	virtual void init (Gdk::Color& base_color, bool wait_for_data);
    
	boost::shared_ptr<ARDOUR::Region> region() const { return _region; }
	
	bool is_valid() const    { return valid; }

	void set_valid (bool yn) { valid = yn; }
	
	virtual void set_height (double);
	virtual void set_samples_per_unit (double);
	virtual bool set_duration (nframes_t, void*);
	
	void move (double xdelta, double ydelta);
	
	void raise_to_top ();
	void lower_to_bottom ();

	bool set_position(nframes_t pos, void* src, double* delta = 0);
	void fake_set_opaque (bool yn);
	
	virtual void show_region_editor () {}
	virtual void hide_region_editor();
	
	virtual void region_changed (ARDOUR::Change);
	
	virtual GhostRegion* add_ghost (TimeAxisView&) = 0;
	void remove_ghost_in (TimeAxisView&);
	void remove_ghost (GhostRegion*);
	
	uint32_t get_fill_color ();

	virtual void entered () {}
	virtual void exited () {}

	void enable_display(bool yn) { _enable_display = yn; }
	void update_coverage_frames (LayerDisplay);
	
	static sigc::signal<void,RegionView*> RegionViewGoingAway;
	
  protected:
	
	/** Allows derived types to specify their visibility requirements
     * to the TimeAxisViewItem parent class
     */
    RegionView (ArdourCanvas::Group *, 
		TimeAxisView&,
		boost::shared_ptr<ARDOUR::Region>,
		double      samples_per_unit,
		Gdk::Color& basic_color,
		bool recording,
		TimeAxisViewItem::Visibility);
    
    virtual void region_resized (ARDOUR::Change);
    virtual void region_muted ();
    void         region_locked ();
    void         region_opacity ();
    virtual void region_renamed ();
    void         region_sync_changed ();

    Glib::ustring make_name () const;

    static gint _lock_toggle (ArdourCanvas::Item*, GdkEvent*, void*);
    void        lock_toggle ();

    virtual void set_colors ();
    virtual void compute_colors (Gdk::Color&);
    virtual void set_frame_color ();
    virtual void reset_width_dependent_items (double pixel_width);

    virtual void color_handler () {}
	
    boost::shared_ptr<ARDOUR::Region> _region;
    
    ArdourCanvas::Polygon* sync_mark; ///< polgyon for sync position 
    ArdourCanvas::Line* sync_line; ///< polgyon for sync position 

    RegionEditor* editor;

    std::vector<ControlPoint *> control_points;
    double current_visible_sync_position;

    bool    valid; ///< see StreamView::redisplay_diskstream() 
    bool    _enable_display; ///< see StreamView::redisplay_diskstream() 
    double  _pixel_width;
    double  _height;
    bool    in_destructor;
    
    bool             wait_for_data;
    sigc::connection data_ready_connection;
    
    std::vector<GhostRegion*> ghosts;

	/** a list of rectangles which are used in stacked display mode to colour
	    different bits of regions according to whether or not they are the one
	    that will be played at any given time.
	*/
	std::list<ArdourCanvas::SimpleRect*> _coverage_frames;

	ARDOUR::BeatsFramesConverter _time_converter;
};

#endif /* __gtk_ardour_region_view_h__ */
