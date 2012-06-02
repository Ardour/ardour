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

#include <gdkmm/cursor.h>
#include "utils.h"
#include "mouse_cursors.h"
#include "editor_xpms"

MouseCursors::MouseCursors ()
{
	using namespace Glib;
	using namespace Gdk;

	{
		RefPtr<Pixbuf> p (::get_icon ("zoom_in_cursor"));
		zoom_in = new Cursor (Display::get_default(), p, 10, 5);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("zoom_out_cursor"));
		zoom_out = new Cursor (Display::get_default(), p, 5, 5);
	}

	Color fbg ("#ffffff");
	Color ffg ("#000000");

	{
		RefPtr<Bitmap> source = Bitmap::create ((char const *) fader_cursor_bits, fader_cursor_width, fader_cursor_height);
		RefPtr<Bitmap> mask = Bitmap::create ((char const *) fader_cursor_mask_bits, fader_cursor_width, fader_cursor_height);
		fader = new Cursor (source, mask, ffg, fbg, fader_cursor_x_hot, fader_cursor_y_hot);
	}

	{
		RefPtr<Bitmap> source = Bitmap::create ((char const *) speaker_cursor_bits, speaker_cursor_width, speaker_cursor_height);
		RefPtr<Bitmap> mask = Bitmap::create ((char const *) speaker_cursor_mask_bits, speaker_cursor_width, speaker_cursor_height);
		speaker = new Cursor (source, mask, ffg, fbg, speaker_cursor_width >> 1, speaker_cursor_height >> 1);
	}

	{
		char pix[4] = { 0, 0, 0, 0 };
		RefPtr<Bitmap> bits = Bitmap::create (pix, 2, 2);
		Color c;
		transparent = new Cursor (bits, bits, c, c, 0, 0);
	}

	{
		char pix[4] = { 0, 0, 0, 0 };
		RefPtr<Bitmap> bits = Bitmap::create (pix, 2, 2);
		Color c;
		transparent = new Cursor (bits, bits, c, c, 0, 0);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("grabber"));
		grabber = new Cursor (Display::get_default(), p, 5, 0);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("grabber_note"));
		grabber_note = new Cursor (Display::get_default(), p, 5, 10);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("grabber_edit_point"));
		grabber_edit_point = new Cursor (Display::get_default(), p, 5, 17);
	}

	cross_hair = new Cursor (CROSSHAIR);
	trimmer =  new Cursor (SB_H_DOUBLE_ARROW);

	{
		RefPtr<Pixbuf> p (::get_icon ("trim_left_cursor"));
		left_side_trim = new Cursor (Display::get_default(), p, 5, 11);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("trim_right_cursor"));
		right_side_trim = new Cursor (Display::get_default(), p, 23, 11);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("trim_left_cursor_right_only"));
		left_side_trim_right_only = new Cursor (Display::get_default(), p, 5, 11);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("trim_right_cursor_left_only"));
		right_side_trim_left_only = new Cursor (Display::get_default(), p, 23, 11);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("fade_in_cursor"));
		fade_in = new Cursor (Display::get_default(), p, 0, 0);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("fade_out_cursor"));
		fade_out = new Cursor (Display::get_default(), p, 29, 0);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("resize_left_cursor"));
		resize_left = new Cursor (Display::get_default(), p, 3, 10);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("resize_top_left_cursor"));
		resize_top_left = new Cursor (Display::get_default(), p, 3, 3);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("resize_top_cursor"));
		resize_top = new Cursor (Display::get_default(), p, 10, 3);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("resize_top_right_cursor"));
		resize_top_right = new Cursor (Display::get_default(), p, 18, 3);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("resize_right_cursor"));
		resize_right = new Cursor (Display::get_default(), p, 24, 10);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("resize_bottom_right_cursor"));
		resize_bottom_right = new Cursor (Display::get_default(), p, 18, 18);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("resize_bottom_cursor"));
		resize_bottom = new Cursor (Display::get_default(), p, 10, 24);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("resize_bottom_left_cursor"));
		resize_bottom_left = new Cursor (Display::get_default(), p, 3, 18);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("move_cursor"));
		move = new Cursor (Display::get_default(), p, 11, 11);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("expand_left_right_cursor"));
		expand_left_right = new Cursor (Display::get_default(), p, 11, 4);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("expand_up_down_cursor"));
		expand_up_down = new Cursor (Display::get_default(), p, 4, 11);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("i_beam_cursor"));
		selector = new Cursor (Display::get_default(), p, 4, 11);
	}

	time_fx = new Cursor (SIZING);
	wait = new Cursor (WATCH);
	timebar = new Cursor(LEFT_PTR);
	midi_pencil = new Cursor (PENCIL);
	midi_select = new Cursor (CENTER_PTR);
	midi_resize = new Cursor (SIZING);
	midi_erase = new Cursor (DRAPED_BOX);
	up_down = new Cursor (SB_V_DOUBLE_ARROW);
}
