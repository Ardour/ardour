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

#ifdef interface
#undef interface
#endif

#include <vector>

#include <sigc++/signal.h>
#include "ardour/region.h"
#include "ardour/beats_frames_converter.h"

#include "canvas/fwd.h"

#include "time_axis_view_item.h"
#include "automation_line.h"
#include "enums.h"

class TimeAxisView;
class RegionEditor;
class GhostRegion;
class AutomationTimeAxisView;
class AutomationRegionView;

namespace ArdourCanvas {
	class Polygon;
	class Text;
}

class RegionView : public TimeAxisViewItem
{
  public:
	RegionView (ArdourCanvas::Container* parent,
	            TimeAxisView&        time_view,
	            boost::shared_ptr<ARDOUR::Region> region,
	            double               samples_per_pixel,
	            uint32_t             base_color,
		    bool 		 automation = false);

	RegionView (const RegionView& other);
	RegionView (const RegionView& other, boost::shared_ptr<ARDOUR::Region> other_region);

	~RegionView ();

	virtual void init (bool wait_for_data);

	boost::shared_ptr<ARDOUR::Region> region() const { return _region; }

	bool is_valid() const    { return valid; }

	void set_valid (bool yn) { valid = yn; }

	virtual void set_height (double);
	virtual void set_samples_per_pixel (double);
	virtual bool set_duration (framecnt_t, void*);

	void move (double xdelta, double ydelta);

	void raise_to_top ();
	void lower_to_bottom ();

	bool set_position(framepos_t pos, void* src, double* delta = 0);

	virtual void show_region_editor ();
	void hide_region_editor ();

	virtual void region_changed (const PBD::PropertyChange&);

	uint32_t get_fill_color () const;

	virtual GhostRegion* add_ghost (TimeAxisView&) = 0;
	void remove_ghost_in (TimeAxisView&);
	void remove_ghost (GhostRegion*);

	virtual void entered () {}
	virtual void exited () {}

	virtual void enable_display(bool yn) { _enable_display = yn; }
	virtual void update_coverage_frames (LayerDisplay);

	static PBD::Signal1<void,RegionView*> RegionViewGoingAway;

	/** Called when a front trim is about to begin */
	virtual void trim_front_starting () {}

	bool trim_front (framepos_t, bool, const int32_t sub_num);

	/** Called when a start trim has finished */
	virtual void trim_front_ending () {}

	bool trim_end (framepos_t, bool, const int32_t sub_num);
        void move_contents (ARDOUR::frameoffset_t);
	virtual void thaw_after_trim ();

        void set_silent_frames (const ARDOUR::AudioIntervalResult&, double threshold);
        void drop_silent_frames ();
        void hide_silent_frames ();

	struct PositionOrder {
		bool operator()(const RegionView* a, const RegionView* b) {
			return a->region()->position() < b->region()->position();
		}
	};

	ARDOUR::MusicFrame snap_frame_to_frame (ARDOUR::frameoffset_t, bool ensure_snap = false) const;

  protected:

	/** Allows derived types to specify their visibility requirements
	 * to the TimeAxisViewItem parent class
	 */
	RegionView (ArdourCanvas::Container *,
		    TimeAxisView&,
		    boost::shared_ptr<ARDOUR::Region>,
		    double samples_per_pixel,
		    uint32_t basic_color,
		    bool recording,
		    TimeAxisViewItem::Visibility);

        bool canvas_group_event (GdkEvent*);

	virtual void region_resized (const PBD::PropertyChange&);
	virtual void region_muted ();
	void         region_locked ();
	void         region_opacity ();
	virtual void region_renamed ();
	void         region_sync_changed ();

	std::string make_name () const;

	static gint _lock_toggle (ArdourCanvas::Item*, GdkEvent*, void*);
	void        lock_toggle ();

	virtual void set_colors ();
	virtual void set_sync_mark_color ();
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
	bool    in_destructor;

	bool wait_for_data;

	std::vector<GhostRegion*> ghosts;

	/** a list of rectangles which are used in stacked display mode to colour
	    different bits of regions according to whether or not they are the one
	    that will be played at any given time.
	*/
	std::list<ArdourCanvas::Rectangle*> _coverage_frames;

	/** a list of rectangles used to show silent segments
	*/
	std::list<ArdourCanvas::Rectangle*> _silent_frames;
	/** a list of rectangles used to show the current silence threshold
	*/
	std::list<ArdourCanvas::Rectangle*> _silent_threshold_frames;
        /** a text item to display strip silence statistics
         */
        ArdourCanvas::Text* _silence_text;
};

#endif /* __gtk_ardour_region_view_h__ */
