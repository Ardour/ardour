/*
    Copyright (C) 2007 Paul Davis 
    Author: Dave Robillard

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

#ifndef __gtk_ardour_midi_util_h__
#define __gtk_ardour_midi_util_h__

#include "rgb_macros.h"
#include "ardour_ui.h"
#include "ui_config.h"

inline static uint32_t note_outline_color(uint8_t vel)
{
	if (vel < 64) {
		return UINT_INTERPOLATE(
				ARDOUR_UI::config()->canvasvar_MidiNoteOutlineMin.get(),
				ARDOUR_UI::config()->canvasvar_MidiNoteOutlineMid.get(),
				(vel / (double)63.0));
	} else {
		return UINT_INTERPOLATE(
				ARDOUR_UI::config()->canvasvar_MidiNoteOutlineMid.get(),
				ARDOUR_UI::config()->canvasvar_MidiNoteOutlineMax.get(),
				((vel-64) / (double)63.0));
	}
}

inline static uint32_t note_fill_color(uint8_t vel)
{
	if (vel < 64) {
		return UINT_INTERPOLATE(
				ARDOUR_UI::config()->canvasvar_MidiNoteOutlineMin.get(),
				ARDOUR_UI::config()->canvasvar_MidiNoteOutlineMid.get(),
				(vel / (double)63.0));
	} else {
		return UINT_INTERPOLATE(
				ARDOUR_UI::config()->canvasvar_MidiNoteOutlineMid.get(),
				ARDOUR_UI::config()->canvasvar_MidiNoteOutlineMax.get(),
				((vel-64) / (double)63.0));
	}
}

#endif /* __gtk_ardour_midi_util_h__ */

