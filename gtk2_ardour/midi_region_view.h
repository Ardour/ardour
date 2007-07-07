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

#ifndef __gtk_ardour_midi_region_view_h__
#define __gtk_ardour_midi_region_view_h__

#include <vector>

#include <libgnomecanvasmm.h>
#include <libgnomecanvasmm/polygon.h>
#include <sigc++/signal.h>
#include <ardour/midi_region.h>
#include <ardour/types.h>

#include "region_view.h"
#include "route_time_axis.h"
#include "time_axis_view_item.h"
#include "automation_line.h"
#include "enums.h"
#include "canvas.h"

namespace ARDOUR {
	class MidiRegion;
};

class MidiTimeAxisView;
class GhostRegion;
class AutomationTimeAxisView;

class MidiRegionView : public RegionView
{
  public:
	MidiRegionView (ArdourCanvas::Group *, 
	                RouteTimeAxisView&,
	                boost::shared_ptr<ARDOUR::MidiRegion>,
	                double initial_samples_per_unit,
	                Gdk::Color& basic_color);

	~MidiRegionView ();
	
	virtual void init (Gdk::Color& base_color, bool wait_for_data = false);
	
	boost::shared_ptr<ARDOUR::MidiRegion> midi_region() const;
	
	void set_y_position_and_height (double, double);
    
    void show_region_editor ();

    GhostRegion* add_ghost (AutomationTimeAxisView&);

	void add_event(const ARDOUR::MidiEvent& ev);

	void begin_write();
	void end_write();
	void extend_active_notes();

  protected:

    /* this constructor allows derived types
       to specify their visibility requirements
       to the TimeAxisViewItem parent class
    */
    
    MidiRegionView (ArdourCanvas::Group *, 
	                RouteTimeAxisView&,
	                boost::shared_ptr<ARDOUR::MidiRegion>,
	                double samples_per_unit,
	                Gdk::Color& basic_color,
	                TimeAxisViewItem::Visibility);
    
    void region_resized (ARDOUR::Change);

    void set_flags (XMLNode *);
    void store_flags ();
    
	void reset_width_dependent_items (double pixel_width);

  private:

	void display_events();
	void clear_events();

	std::vector<ArdourCanvas::Item*> _events;
	ArdourCanvas::SimpleRect**       _active_notes;
};

#endif /* __gtk_ardour_midi_region_view_h__ */
