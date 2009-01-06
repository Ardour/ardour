/*
    Copyright (C) 2000-2007 Paul Davis 

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

#ifndef __gtk2_ardour_drag_info_h_
#define __gtk2_ardour_drag_info_h_

#include <list>

#include <gdk/gdk.h>
#include <stdint.h>

#include "canvas.h"
#include "editor_items.h"

#include <ardour/types.h>

namespace ARDOUR {
	class Location;
}

class Editor;
class TimeAxisView;

struct DragInfo {
    ArdourCanvas::Item* item;
    ItemType            item_type;
    void* data;
    nframes64_t last_frame_position;
    nframes64_t pointer_frame_offset;
    nframes64_t grab_frame;
    nframes64_t last_pointer_frame;
    nframes64_t current_pointer_frame;
    double original_x, original_y;
    double grab_x, grab_y;
    double cumulative_x_drag;
    double cumulative_y_drag;
    double current_pointer_x;
    double current_pointer_y;
    double last_pointer_x;
    double last_pointer_y;
    void (Editor::*motion_callback)(ArdourCanvas::Item*, GdkEvent*);
    void (Editor::*finished_callback)(ArdourCanvas::Item*, GdkEvent*);
    TimeAxisView* source_trackview;
    ARDOUR::layer_t source_layer;
    TimeAxisView* dest_trackview;
    ARDOUR::layer_t dest_layer;
    bool x_constrained;
    bool y_constrained;
    bool copy;
    bool was_rolling;
    bool first_move;
    bool move_threshold_passed;
    bool want_move_threshold;
    bool brushing;
    std::list<ARDOUR::Location*> copied_locations;

    void clear_copied_locations ();
};

struct LineDragInfo {
    uint32_t before;
    uint32_t after;
};

#endif /* __gtk2_ardour_drag_info_h_ */

