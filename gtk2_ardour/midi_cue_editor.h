/*
 * Copyright (C) 2023 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __gtk_ardour_midi_cue_editor_h__
#define __gtk_ardour_midi_cue_editor_h__

#include <gtkmm/adjustment.h>

#include "cue_editor.h"

namespace Gtk {
	class Widget;
}

namespace ArdourCanvas {
	class Canvas;
	class Container;
	class GtkCanvasViewport;
	class ScrollGroup;
}

class MidiCueEditor : public CueEditor
{
  public:
	MidiCueEditor ();
	~MidiCueEditor ();

	ArdourCanvas::Container* get_noscroll_group() const { return no_scroll_group; }
	Gtk::Widget& viewport() { return *_canvas_viewport; }

 private:
	Gtk::Adjustment vertical_adjustment;
	Gtk::Adjustment horizontal_adjustment;
	ArdourCanvas::GtkCanvasViewport* _canvas_viewport;
	ArdourCanvas::Canvas* _canvas;

	ArdourCanvas::Container* tempo_group;

	/* The group containing all other groups that are scrolled vertically
	   and horizontally.
	*/
	ArdourCanvas::ScrollGroup* hv_scroll_group;

	/* The group containing all other groups that are scrolled horizontally ONLY
	*/
	ArdourCanvas::ScrollGroup* h_scroll_group;

	/* Scroll group for cursors, scrolled horizontally, above everything else
	*/
	ArdourCanvas::ScrollGroup* cursor_scroll_group;

	/* The group containing all trackviews. */
	ArdourCanvas::Container* no_scroll_group;

	/* The group containing all trackviews. */
	ArdourCanvas::Container* _trackview_group;
	ArdourCanvas::Container* global_rect_group;
	ArdourCanvas::Container* time_line_group;

	ArdourCanvas::Rectangle* transport_loop_range_rect;

	void build_canvas ();
};


#endif /* __gtk_ardour_midi_cue_editor_h__ */
