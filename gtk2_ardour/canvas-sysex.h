/*
    Copyright (C) 2009 Paul Davis
    Author: Hans Baier

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

#ifndef CANVAS_SYSEX_H_
#define CANVAS_SYSEX_H_

#include <string>

#include "canvas-flag.h"
#include "ardour/midi_model.h"

class MidiRegionView;

namespace Gnome {
namespace Canvas {

class CanvasSysEx : public CanvasFlag
{
public:
	CanvasSysEx(
			MidiRegionView& region,
			Group&          parent,
			std::string&    text,
			double          height,
			double          x,
			double          y,
			ARDOUR::MidiModel::SysExPtr sysex);

	virtual ~CanvasSysEx();

	const ARDOUR::MidiModel::SysExPtr sysex() const { return _sysex; }
	const string text() const { return _text; }

	virtual bool on_event(GdkEvent* ev);

private:
	const ARDOUR::MidiModel::SysExPtr _sysex;

	string _text;
};

} // namespace Canvas
} // namespace Gnome

#endif /* CANVAS_SYSEX_H_ */
