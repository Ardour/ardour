#ifndef __ardour_gtk_enums_h__
#define __ardour_gtk_enums_h__

#include <ardour/types.h>

enum WaveformShape {
	Traditional,
	Rectified
};

enum WaveformScale {
	LinearWaveform=0,
	LogWaveform,
};


enum Width {
	Wide,
	Narrow,
};

namespace Gnome {
	namespace Canvas {
		class SimpleRect;
	}
}

struct SelectionRect {
    Gnome::Canvas::SimpleRect *rect;
    Gnome::Canvas::SimpleRect *end_trim;
    Gnome::Canvas::SimpleRect *start_trim;
    uint32_t id;
};

#endif /* __ardour_gtk_enums_h__ */
