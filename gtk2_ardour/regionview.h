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
#include <ardour/region.h>

#include "time_axis_view_item.h"
#include "automation_line.h"
#include "enums.h"
#include "waveview.h"
#include "canvas.h"
#include "color.h"

class TimeAxisView;
class RegionEditor;
class GhostRegion;
class AutomationTimeAxisView;

class RegionView : public TimeAxisViewItem
{
  public:
	RegionView (ArdourCanvas::Group* parent, 
	            TimeAxisView&        time_view,
	            ARDOUR::Region&      region,
	            double               samples_per_unit,
	            Gdk::Color&          basic_color);

	~RegionView ();
	
	virtual void init (Gdk::Color& base_color, bool wait_for_waves);
    
	ARDOUR::Region& region() const { return _region; }
	
	bool is_valid() const    { return valid; }
    void set_valid (bool yn) { valid = yn; }

    virtual void set_height (double) = 0;
    void set_samples_per_unit (double);
    bool set_duration (jack_nframes_t, void*);

    void move (double xdelta, double ydelta);

    void raise ();
    void raise_to_top ();
    void lower ();
    void lower_to_bottom ();

    bool set_position(jack_nframes_t pos, void* src, double* delta = 0);

    virtual void show_region_editor () = 0;
    void hide_region_editor();

    void region_changed (ARDOUR::Change);

    virtual GhostRegion* add_ghost (AutomationTimeAxisView&) = 0;
    void                 remove_ghost (GhostRegion*);

    uint32_t get_fill_color ();

    virtual void entered () {}
    virtual void exited () {}
    
	static sigc::signal<void,RegionView*> RegionViewGoingAway;
    sigc::signal<void>                    GoingAway;

  protected:

    /** Allows derived types to specify their visibility requirements
     * to the TimeAxisViewItem parent class
	 */
    RegionView (ArdourCanvas::Group *, 
	            TimeAxisView&,
	            ARDOUR::Region&,
	            double initial_samples_per_unit,
	            Gdk::Color& basic_color,
	            TimeAxisViewItem::Visibility);
    
	ARDOUR::Region& _region;
    
    enum Flags {
	    EnvelopeVisible = 0x1,
	    WaveformVisible = 0x4,
	    WaveformRectified = 0x8
    };

    ArdourCanvas::Polygon* sync_mark; ///< polgyon for sync position 
    ArdourCanvas::Text* no_wave_msg; ///< text 

    RegionEditor *editor;

    vector<ControlPoint *> control_points;
    double current_visible_sync_position;

    uint32_t _flags;
    uint32_t fade_color;
    bool     valid; ///< see StreamView::redisplay_diskstream() 
    double  _pixel_width;
    double  _height;
    bool    in_destructor;
    bool    wait_for_waves;
    
	sigc::connection peaks_ready_connection;

    void region_resized (ARDOUR::Change);
    void region_moved (void *);
    void region_muted ();
    void region_locked ();
    void region_opacity ();
    void region_layered ();
    void region_renamed ();
    void region_sync_changed ();
    void region_scale_amplitude_changed ();

    static gint _lock_toggle (ArdourCanvas::Item*, GdkEvent*, void*);
    void        lock_toggle ();

    void peaks_ready_handler (uint32_t);
    void reset_name (gdouble width);

    void set_colors ();
    void compute_colors (Gdk::Color&);
    virtual void set_frame_color ();
    void reset_width_dependent_items (double pixel_width);

    vector<GhostRegion*> ghosts;
    
    virtual void color_handler (ColorID, uint32_t) {}
};

#endif /* __gtk_ardour_region_view_h__ */
