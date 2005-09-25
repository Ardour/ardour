#ifndef __ardour_gtk_enums_h__
#define __ardour_gtk_enums_h__

#include <ardour/types.h>

enum WaveformShape {
	Traditional,
	Rectified
};


enum Width {
	Wide,
	Narrow,
};

#include <gtk-canvas/gtk-canvas.h>

struct SelectionRect {
    GtkCanvasItem *rect;
    GtkCanvasItem *end_trim;
    GtkCanvasItem *start_trim;
    uint32_t id;
};

#endif /* __ardour_gtk_enums_h__ */
