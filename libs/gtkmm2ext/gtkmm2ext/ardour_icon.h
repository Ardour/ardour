#ifndef _gtkmm2ext_ardour_icon_h_
#define _gtkmm2ext_ardour_icon_h_

#include <stdint.h>
#include <cairo.h>
#include "gtkmm2ext/widget_state.h"

namespace Gtkmm2ext { namespace ArdourIcon {
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

	LIBGTKMM2EXT_API bool render (cairo_t *cr,
	                              const enum Icon icon,
	                              const int width, const int height,
	                              const Gtkmm2ext::ActiveState state,
	                              const uint32_t fg_color);
}; };

#endif
