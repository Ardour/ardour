/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _WIDGETS_ARDOUR_ICON_H_
#define _WIDGETS_ARDOUR_ICON_H_

#include <stdint.h>
#include <cairo.h>
#include <gtkmm/widget.h>

#include "gtkmm2ext/widget_state.h"
#include "widgets/visibility.h"

namespace ArdourWidgets { namespace ArdourIcon {
	enum Icon {
		RecButton,
		ZoomIn,
		ZoomOut,
		ZoomFull,
		TransportPanic,
		TransportStop,
		TransportPlay,
		TransportLoop,
		TransportRange,
		TransportStart,
		TransportEnd,
		TransportMetronom,
		ToolGrab,
		ToolRange,
		ToolCut,
		ToolStretch,
		ToolAudition,
		ToolDraw,
		ToolContent,
		ZoomExpand,
		TimeAxisShrink,
		TimeAxisExpand,
		StripWidth,
		CloseCross,
		HideEye,
		PlusSign,
		ScrollLeft,
		ScrollRight,
		NudgeLeft,
		NudgeRight,
		DinMidi,
		PsetAdd,
		PsetSave,
		PsetDelete,
		PsetBrowse,
		PluginReset,
		PluginBypass,
		PluginPinout,
		LatencyClock,
		Config,
		ConfigReset,
		PowerOnOff,
		ShadedPlusSign,
		Folder,
		NoIcon //< Last
	};

	LIBWIDGETS_API bool render (cairo_t *cr,
	                            const enum Icon icon,
	                            const int width, const int height,
	                            const Gtkmm2ext::ActiveState state,
	                            const uint32_t fg_color);

	LIBWIDGETS_API bool expose (GdkEventExpose* ev,
	                            Gtk::Widget* w,
	                            const enum Icon icon);

	LIBWIDGETS_API bool expose_with_text (GdkEventExpose* ev,
	                                      Gtk::Widget* w,
	                                      const enum Icon icon,
	                                      std::string const&);

}; } /* end namespace */

#endif
