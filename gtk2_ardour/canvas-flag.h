/*
    Copyright (C) 2012 Paul Davis 

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

#ifndef CANVASFLAG_H_
#define CANVASFLAG_H_

#include <string>
#include <libgnomecanvasmm/pixbuf.h>
#include <libgnomecanvasmm/group.h>
#include <libgnomecanvasmm/widget.h>

#include "simplerect.h"
#include "simpleline.h"
#include "canvas.h"

class MidiRegionView;

namespace Gnome {
namespace Canvas {

class CanvasFlag : public Group
{
public:
	CanvasFlag (MidiRegionView& region,
	            Group&          parent,
	            double         height,
	            guint           outline_color_rgba = 0xc0c0c0ff,
	            guint           fill_color_rgba = 0x07070707,
	            double         x = 0.0,
	            double         y = 0.0);

	virtual ~CanvasFlag();

	virtual void set_text(const std::string& a_text);
	virtual void set_height (double);

	int width () const { return name_pixbuf_width + 10.0; }

protected:
	ArdourCanvas::Pixbuf* _name_pixbuf;
	double           _height;
	guint            _outline_color_rgba;
	guint            _fill_color_rgba;
	MidiRegionView&  _region;
	int name_pixbuf_width;

private:
	void delete_allocated_objects();

	SimpleLine*      _line;
	SimpleRect*      _rect;
};


} // namespace Canvas
} // namespace Gnome

#endif /*CANVASFLAG_H_*/
