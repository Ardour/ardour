/*
    Copyright (C) 2001-2004 Paul Davis 

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

    $Id$
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

namespace ARDOUR {
	class AudioRegion;
	class PeakData;
};

class AudioTimeAxisView;
class AudioRegionGainLine;
class AudioRegionEditor;
class GhostRegion;
class AutomationTimeAxisView;

class AudioRegionView : public TimeAxisViewItem
{
  public:
	AudioRegionView (ArdourCanvas::Group *, 
			 AudioTimeAxisView&,
			 ARDOUR::AudioRegion&,
			 double initial_samples_per_unit,
			 Gdk::Color& basic_color);

	~AudioRegionView ();
	
	virtual void init (double amplitude_above_axis, Gdk::Color& base_color, bool wait_for_waves);
    
    ARDOUR::AudioRegion& region;  // ok, let 'em have it
    bool is_valid() const { return valid; }
    void set_valid (bool yn) { valid = yn; }

    std::string get_item_name();
    void set_height (double);
    void set_samples_per_unit (double);
    bool set_duration (jack_nframes_t, void*);

    void set_amplitude_above_axis (gdouble spp);

    void move (double xdelta, double ydelta);

    void raise ();
    void raise_to_top ();
    void lower ();
    void lower_to_bottom ();

    bool set_position(jack_nframes_t pos, void* src, double* delta = 0);
    
    void temporarily_hide_envelope (); // dangerous
    void unhide_envelope (); // dangerous

    void set_envelope_visible (bool);
    void set_waveform_visible (bool yn);
    void set_waveform_shape (WaveformShape);

    bool waveform_rectified() const { return _flags & WaveformRectified; }
    bool waveform_visible() const { return _flags & WaveformVisible; }
    bool envelope_visible() const { return _flags & EnvelopeVisible; }
    
    void show_region_editor ();
    void hide_region_editor();

    void add_gain_point_event (ArdourCanvas::Item *item, GdkEvent *event);
    void remove_gain_point_event (ArdourCanvas::Item *item, GdkEvent *event);

    AudioRegionGainLine* get_gain_line() const { return gain_line; }

    void region_changed (ARDOUR::Change);
    void envelope_active_changed ();

    static sigc::signal<void,AudioRegionView*> AudioRegionViewGoingAway;
    sigc::signal<void> GoingAway;

    GhostRegion* add_ghost (AutomationTimeAxisView&);
    void remove_ghost (GhostRegion*);

    void reset_fade_in_shape_width (jack_nframes_t);
    void reset_fade_out_shape_width (jack_nframes_t);
    void set_fade_in_active (bool);
    void set_fade_out_active (bool);

    uint32_t get_fill_color ();

    virtual void entered ();
    virtual void exited ();

  protected:

    /* this constructor allows derived types
       to specify their visibility requirements
       to the TimeAxisViewItem parent class
    */
    
    AudioRegionView (ArdourCanvas::Group *, 
		     AudioTimeAxisView&,
		     ARDOUR::AudioRegion&,
		     double initial_samples_per_unit,
		     Gdk::Color& basic_color,
		     TimeAxisViewItem::Visibility);
    
    enum Flags {
	    EnvelopeVisible = 0x1,
	    WaveformVisible = 0x4,
	    WaveformRectified = 0x8
    };

    vector<ArdourCanvas::WaveView *> waves; /* waveviews */
    vector<ArdourCanvas::WaveView *> tmp_waves; /* see ::create_waves()*/
    ArdourCanvas::Polygon* sync_mark; /* polgyon for sync position */
    ArdourCanvas::Text* no_wave_msg; /* text */
    ArdourCanvas::SimpleLine* zero_line; /* simpleline */
    ArdourCanvas::Polygon* fade_in_shape; /* polygon */
    ArdourCanvas::Polygon* fade_out_shape; /* polygon */
    ArdourCanvas::SimpleRect* fade_in_handle; /* simplerect */
    ArdourCanvas::SimpleRect* fade_out_handle; /* simplerect */

    AudioRegionGainLine* gain_line;
    AudioRegionEditor *editor;

    vector<ControlPoint *> control_points;
    double _amplitude_above_axis;
    double current_visible_sync_position;

    uint32_t _flags;
    uint32_t fade_color;
    bool     valid; /* see StreamView::redisplay_diskstream() */
    double _pixel_width;
    double _height;
    bool    in_destructor;
    bool    wait_for_waves;
    sigc::connection peaks_ready_connection;

    void reset_fade_shapes ();
    void reset_fade_in_shape ();
    void reset_fade_out_shape ();
    void fade_in_changed ();
    void fade_out_changed ();
    void fade_in_active_changed ();
    void fade_out_active_changed ();

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
    void lock_toggle ();

    void create_waves ();
    void create_one_wave (uint32_t, bool);
    void manage_zero_line ();
    void peaks_ready_handler (uint32_t);
    void reset_name (gdouble width);
    void set_flags (XMLNode *);
    void store_flags ();

    void set_colors ();
    void compute_colors (Gdk::Color&);
    virtual void set_frame_color ();
    void reset_width_dependent_items (double pixel_width);
    void set_waveview_data_src();

    vector<GnomeCanvasWaveViewCache*> wave_caches;
    vector<GhostRegion*> ghosts;
    
    void color_handler (ColorID, uint32_t);
};

#endif /* __gtk_ardour_region_view_h__ */
