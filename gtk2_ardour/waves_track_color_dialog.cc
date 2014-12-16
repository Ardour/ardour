/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#include "waves_track_color_dialog.h"

WavesTrackColorDialog::WavesTrackColorDialog ()
	: WavesUI ("waves_track_color_dialog.xml", *this)
{
	color_button[0] = &get_waves_button ("color_button_1");
	color_button[1] = &get_waves_button ("color_button_2");
	color_button[2] = &get_waves_button ("color_button_3");
	color_button[3] = &get_waves_button ("color_button_4");
	color_button[4] = &get_waves_button ("color_button_5");
	color_button[5] = &get_waves_button ("color_button_6");
	color_button[6] = &get_waves_button ("color_button_7");
	color_button[7] = &get_waves_button ("color_button_8");
	color_button[8] = &get_waves_button ("color_button_9");
	color_button[9] = &get_waves_button ("color_button_10");
	color_button[10] = &get_waves_button ("color_button_11");
	color_button[11] = &get_waves_button ("color_button_12");
	color_button[12] = &get_waves_button ("color_button_13");
	color_button[13] = &get_waves_button ("color_button_14");
	color_button[14] = &get_waves_button ("color_button_15");

	_init ();
}
