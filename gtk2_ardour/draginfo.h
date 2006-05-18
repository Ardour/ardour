#ifndef __gtk2_ardour_drag_info_h_
#define __gtk2_ardour_drag_info_h_

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
    jack_nframes_t last_frame_position;
    int32_t pointer_frame_offset;
    jack_nframes_t grab_frame;
    jack_nframes_t last_pointer_frame;
    jack_nframes_t current_pointer_frame;
    double grab_x, grab_y;
    double cumulative_x_drag;
    double cumulative_y_drag;
    double current_pointer_x;
    double current_pointer_y;
    void (Editor::*motion_callback)(ArdourCanvas::Item*, GdkEvent*);
    void (Editor::*finished_callback)(ArdourCanvas::Item*, GdkEvent*);
    TimeAxisView* last_trackview;
    bool x_constrained;
    bool y_constrained;
    bool copy;
    bool was_rolling;
    bool first_move;
    bool move_threshold_passsed;
    bool want_move_threshold;
    bool brushing;
    ARDOUR::Location* copied_location;
};

struct LineDragInfo {
    uint32_t before;
    uint32_t after;
};

#endif /* __gtk2_ardour_drag_info_h_ */

