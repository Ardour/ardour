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

#ifndef __gtk_ardour_canvas_note_h__
#define __gtk_ardour_canvas_note_h__

#include <iostream>
#include "simplerect.h"
#include "canvas-midi-event.h"
#include "midi_util.h"

namespace Gnome {
namespace Canvas {

class CanvasNote : public SimpleRect, public CanvasMidiEvent {
public:
	CanvasNote(MidiRegionView& region, Group& group, const ARDOUR::MidiModel::Note* note=NULL)
		: SimpleRect(group), CanvasMidiEvent(region, this, note)
	{
	}
	
	virtual void selected(bool yn) {
		if (!_note)
			return;
		else if (yn)
			property_outline_color_rgba()
					= ARDOUR_UI::config()->canvasvar_MidiNoteSelectedOutline.get();
		else
			property_outline_color_rgba() = note_outline_color(_note->velocity());
	}
	
	bool on_event(GdkEvent* ev) { return CanvasMidiEvent::on_event(ev); }
};

} // namespace Gnome
} // namespace Canvas

#endif /* __gtk_ardour_canvas_note_h__ */
