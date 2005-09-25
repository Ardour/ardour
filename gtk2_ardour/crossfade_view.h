/*
    Copyright (C) 2003 Paul Davis 

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

#ifndef __gtk_ardour_crossfade_view_h__
#define __gtk_ardour_crossfade_view_h__

#include <vector>
#include <gtkmm.h>
#include <gtk-canvas.h>
#include <sigc++/signal.h>
#include <ardour/crossfade.h>

#include "time_axis_view_item.h"

class AudioTimeAxisView;
class AudioRegionView;

struct CrossfadeView : public TimeAxisViewItem
{
    CrossfadeView (GtkCanvasGroup*, 
		   AudioTimeAxisView&,
		   ARDOUR::Crossfade&,
		   double initial_samples_per_unit, 
		   GdkColor& basic_color,
		   AudioRegionView& leftview,
		   AudioRegionView& rightview);
    ~CrossfadeView ();

    ARDOUR::Crossfade& crossfade;  // ok, let 'em have it
    AudioRegionView& left_view;    // and these too
    AudioRegionView& right_view;

    std::string get_item_name();
    void set_height (double);

    bool valid() const { return _valid; }
    bool visible() const { return _visible; }
    void set_valid (bool yn);

    static sigc::signal<void,CrossfadeView*> GoingAway;

    AudioRegionView& upper_regionview () const;

    void fake_hide ();
    void hide ();
    void show ();
    
  protected:
    void reset_width_dependent_items (double pixel_width);

  private:
    bool _valid;
    bool _visible;

    double spu;

    GtkCanvasItem *overlap_rect;
    GtkCanvasItem *fade_in;
    GtkCanvasItem *fade_out;
    GtkCanvasItem *active_button;

    void crossfade_changed (ARDOUR::Change);
    void active_changed ();
    void redraw_curves ();
};

#endif /* __gtk_ardour_crossfade_view_h__ */
