/*
    Copyright (C) 2000-2010 Paul Davis

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

#ifndef __gtk2_ardour_mouse_cursors__
#define __gtk2_ardour_mouse_cursors__

/** @file Handling of bitmaps to be used for mouse cursors.
 *
 *  Held centrally by the Editor because some cursors are used in several places.
 */

class MouseCursors
{
public:
	MouseCursors ();

	Gdk::Cursor* cross_hair;
	Gdk::Cursor* trimmer;
	Gdk::Cursor* right_side_trim;
	Gdk::Cursor* left_side_trim;
	Gdk::Cursor* right_side_trim_left_only;
	Gdk::Cursor* left_side_trim_right_only;
	Gdk::Cursor* fade_in;
	Gdk::Cursor* fade_out;
	Gdk::Cursor* selector;
	Gdk::Cursor* grabber;
	Gdk::Cursor* grabber_note;
	Gdk::Cursor* grabber_edit_point;
	Gdk::Cursor* zoom_in;
	Gdk::Cursor* zoom_out;
	Gdk::Cursor* time_fx;
	Gdk::Cursor* fader;
	Gdk::Cursor* speaker;
	Gdk::Cursor* midi_pencil;
	Gdk::Cursor* midi_select;
	Gdk::Cursor* midi_resize;
	Gdk::Cursor* midi_erase;
	Gdk::Cursor* up_down;
	Gdk::Cursor* wait;
	Gdk::Cursor* timebar;
	Gdk::Cursor* transparent;
	Gdk::Cursor* resize_left;
	Gdk::Cursor* resize_top_left;
	Gdk::Cursor* resize_top;
	Gdk::Cursor* resize_top_right;
	Gdk::Cursor* resize_right;
	Gdk::Cursor* resize_bottom_right;
	Gdk::Cursor* resize_bottom;
	Gdk::Cursor* resize_bottom_left;
	Gdk::Cursor* move;
	Gdk::Cursor* expand_left_right;
	Gdk::Cursor* expand_up_down;
};

#endif /* __gtk2_ardour_mouse_cursors__ */
