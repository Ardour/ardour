/*
 * Copyright (C) 2012-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk2_ardour_mouse_cursors__
#define __gtk2_ardour_mouse_cursors__

/** @file mouse_cursors.h
 * Handling of bitmaps to be used for mouse cursors.
 *
 *  Held centrally by the Editor because some cursors are used in several places.
 */

class MouseCursors
{
public:
	MouseCursors ();
	~MouseCursors ();

	void set_cursor_set (const std::string& name);
	std::string cursor_set() const { return _cursor_set; }

	Gdk::Cursor* cross_hair;
	Gdk::Cursor* scissors;
	Gdk::Cursor* trimmer;
	Gdk::Cursor* right_side_trim;
	Gdk::Cursor* anchored_right_side_trim;
	Gdk::Cursor* left_side_trim;
	Gdk::Cursor* anchored_left_side_trim;
	Gdk::Cursor* right_side_trim_left_only;
	Gdk::Cursor* left_side_trim_right_only;
	Gdk::Cursor* fade_in;
	Gdk::Cursor* fade_out;
	Gdk::Cursor* selector;
	Gdk::Cursor* grabber;
	Gdk::Cursor* grabber_note;
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

	/* This cursor is not intended to be used directly, it just
	   serves as an out-of-bounds value when we need to indicate
	   "no cursor". NULL/0 doesn't work for this, because it
	   is actually a valid value for a Gdk::Cursor - it indicates
	   "use the parent window's cursor"
	*/

	static bool is_invalid (Gdk::Cursor* c) { if (!_invalid) { create_invalid(); } return c == _invalid; }
	static Gdk::Cursor* invalid_cursor() { if (!_invalid) { create_invalid(); } return _invalid; }

    private:
	std::string _cursor_set;
	void drop_all ();

	Gdk::Cursor* make_cursor (const char* name, int hotspot_x = 0, int hotspot_y = 0);
	static Gdk::Cursor* _invalid;
	static void create_invalid ();
};

#endif /* __gtk2_ardour_mouse_cursors__ */
