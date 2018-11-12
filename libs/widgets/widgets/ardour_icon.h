#ifndef _WIDGETS_ARDOUR_ICON_H_
#define _WIDGETS_ARDOUR_ICON_H_

#include <stdint.h>
#include <cairo.h>

#include "gtkmm2ext/widget_state.h"
#include "widgets/visibility.h"

namespace ArdourWidgets { namespace ArdourIcon {
	enum Icon {
		NoIcon,
		RecButton,
		RecTapeMode,
		CloseCross,
		StripWidth,
		DinMidi,
		TransportStop,
		TransportPlay,
		TransportLoop,
		TransportRange,
		TransportStart,
		TransportEnd,
		TransportPanic,
		TransportMetronom,
		NudgeLeft,
		NudgeRight,
		ZoomIn,
		ZoomOut,
		ZoomFull,
		ZoomExpand,
		TimeAxisShrink,
		TimeAxisExpand,
		ToolGrab,
		ToolRange,
		ToolCut,
		ToolStretch,
		ToolAudition,
		ToolDraw,
		ToolContent,
	};

	LIBWIDGETS_API bool render (cairo_t *cr,
	                            const enum Icon icon,
	                            const int width, const int height,
	                            const Gtkmm2ext::ActiveState state,
	                            const uint32_t fg_color);
}; } /* end namespace */

#endif
