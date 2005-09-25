/*
    Copyright (C) 2004 Paul Davis 

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

#ifndef __ardour_gtk_ghost_region_h__
#define __ardour_gtk_ghost_region_h__

#include <vector>
#include <sigc++/signal_system.h>
#include <gtk-canvas.h>

class AutomationTimeAxisView;

struct GhostRegion : public SigC::Object
{
    AutomationTimeAxisView& trackview;
    GtkCanvasItem* group;
    GtkCanvasItem* base_rect;
    std::vector<GtkCanvasItem*> waves;

    GhostRegion (AutomationTimeAxisView& tv, double initial_unit_pos);
    ~GhostRegion ();

    void set_samples_per_unit (double spu);
    void set_duration (double units);
    void set_height ();

    SigC::Signal1<void,GhostRegion*> GoingAway;
};

#endif /* __ardour_gtk_ghost_region_h__ */
