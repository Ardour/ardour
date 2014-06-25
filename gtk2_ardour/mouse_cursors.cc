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

using namespace ARDOUR_UI_UTILS;

MouseCursors::MouseCursors ()
	: cross_hair (0)
	, trimmer (0)
	, right_side_trim (0)
	, anchored_right_side_trim (0)
	, left_side_trim (0)
	, anchored_left_side_trim (0)
	, right_side_trim_left_only (0)
	, left_side_trim_right_only (0)
	, fade_in (0)
	, fade_out (0)
	, selector (0)
	, grabber (0)
	, grabber_note (0)
	, grabber_edit_point (0)
	, zoom_in (0)
	, zoom_out (0)
	, time_fx (0)
	, fader (0)
	, speaker (0)
	, midi_pencil (0)
	, midi_select (0)
	, midi_resize (0)
	, midi_erase (0)
	, up_down (0)
	, wait (0)
	, timebar (0)
	, transparent (0)
	, resize_left (0)
	, resize_top_left (0)
	, resize_top (0)
	, resize_top_right (0)
	, resize_right (0)
	, resize_bottom_right (0)
	, resize_bottom (0)
	, resize_bottom_left (0)
	, move (0)
	, expand_left_right (0)
	, expand_up_down (0)
{
}

void
MouseCursors::drop_all ()
{
	delete cross_hair; cross_hair = 0;
	delete trimmer; trimmer = 0;
	delete right_side_trim; right_side_trim = 0;
	delete anchored_right_side_trim; anchored_right_side_trim = 0;
	delete left_side_trim; left_side_trim = 0;
	delete anchored_left_side_trim; anchored_left_side_trim = 0;
	delete right_side_trim_left_only; right_side_trim_left_only = 0;
	delete left_side_trim_right_only; left_side_trim_right_only = 0;
	delete fade_in; fade_in = 0;
	delete fade_out; fade_out = 0;
	delete selector; selector = 0;
	delete grabber; grabber = 0;
	delete grabber_note; grabber_note = 0;
	delete grabber_edit_point; grabber_edit_point = 0;
	delete zoom_in; zoom_in = 0;
	delete zoom_out; zoom_out = 0;
	delete time_fx; time_fx = 0;
	delete fader; fader = 0;
	delete speaker; speaker = 0;
	delete midi_pencil; midi_pencil = 0;
	delete midi_select; midi_select = 0;
	delete midi_resize; midi_resize = 0;
	delete midi_erase; midi_erase = 0;
	delete up_down; up_down = 0;
	delete wait; wait = 0;
	delete timebar; timebar = 0;
	delete transparent; transparent = 0;
	delete resize_left; resize_left = 0;
	delete resize_top_left; resize_top_left = 0;
	delete resize_top; resize_top = 0;
	delete resize_top_right; resize_top_right = 0;
	delete resize_right; resize_right = 0;
	delete resize_bottom_right; resize_bottom_right = 0;
	delete resize_bottom; resize_bottom = 0;
	delete resize_bottom_left; resize_bottom_left = 0;
	delete move; move = 0;
	delete expand_left_right; expand_left_right = 0;
	delete expand_up_down; expand_up_down = 0;
}

void
MouseCursors::set_cursor_set (const std::string& name)
{
	using namespace Glib;
	using namespace Gdk;
	
	drop_all ();
	_cursor_set = name;

	{
		RefPtr<Pixbuf> p (::get_icon ("zoom_in_cursor", _cursor_set));
		zoom_in = new Cursor (Display::get_default(), p, 10, 5);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("zoom_out_cursor", _cursor_set));
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
		RefPtr<Pixbuf> p (::get_icon ("grabber", _cursor_set));
		grabber = new Cursor (Display::get_default(), p, 5, 0);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("grabber_note", _cursor_set));
		grabber_note = new Cursor (Display::get_default(), p, 5, 10);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("grabber_edit_point", _cursor_set));
		grabber_edit_point = new Cursor (Display::get_default(), p, 5, 17);
	}

	cross_hair = new Cursor (CROSSHAIR);
	trimmer =  new Cursor (SB_H_DOUBLE_ARROW);

	{
		RefPtr<Pixbuf> p (::get_icon ("trim_left_cursor", _cursor_set));
		left_side_trim = new Cursor (Display::get_default(), p, 5, 11);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("anchored_trim_left_cursor", _cursor_set));
		anchored_left_side_trim = new Cursor (Display::get_default(), p, 5, 11);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("trim_right_cursor", _cursor_set));
		right_side_trim = new Cursor (Display::get_default(), p, 23, 11);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("anchored_trim_right_cursor", _cursor_set));
		anchored_right_side_trim = new Cursor (Display::get_default(), p, 23, 11);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("trim_left_cursor_right_only", _cursor_set));
		left_side_trim_right_only = new Cursor (Display::get_default(), p, 5, 11);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("trim_right_cursor_left_only", _cursor_set));
		right_side_trim_left_only = new Cursor (Display::get_default(), p, 23, 11);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("fade_in_cursor", _cursor_set));
		fade_in = new Cursor (Display::get_default(), p, 0, 0);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("fade_out_cursor", _cursor_set));
		fade_out = new Cursor (Display::get_default(), p, 29, 0);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("resize_left_cursor", _cursor_set));
		resize_left = new Cursor (Display::get_default(), p, 3, 10);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("resize_top_left_cursor", _cursor_set));
		resize_top_left = new Cursor (Display::get_default(), p, 3, 3);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("resize_top_cursor", _cursor_set));
		resize_top = new Cursor (Display::get_default(), p, 10, 3);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("resize_top_right_cursor", _cursor_set));
		resize_top_right = new Cursor (Display::get_default(), p, 18, 3);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("resize_right_cursor", _cursor_set));
		resize_right = new Cursor (Display::get_default(), p, 24, 10);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("resize_bottom_right_cursor", _cursor_set));
		resize_bottom_right = new Cursor (Display::get_default(), p, 18, 18);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("resize_bottom_cursor", _cursor_set));
		resize_bottom = new Cursor (Display::get_default(), p, 10, 24);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("resize_bottom_left_cursor", _cursor_set));
		resize_bottom_left = new Cursor (Display::get_default(), p, 3, 18);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("move_cursor", _cursor_set));
		move = new Cursor (Display::get_default(), p, 11, 11);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("expand_left_right_cursor", _cursor_set));
		expand_left_right = new Cursor (Display::get_default(), p, 11, 4);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("expand_up_down_cursor", _cursor_set));
		expand_up_down = new Cursor (Display::get_default(), p, 4, 11);
	}

	{
		RefPtr<Pixbuf> p (::get_icon ("i_beam_cursor", _cursor_set));
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
