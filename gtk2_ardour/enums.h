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

#include <libgnomecanvas/libgnomecanvas.h>

struct SelectionRect {
    GnomeCanvasItem *rect;
    GnomeCanvasItem *end_trim;
    GnomeCanvasItem *start_trim;
    uint32_t id;
};

#endif /* __ardour_gtk_enums_h__ */
