/*
    Copyright (C) 2013 Paul Davis

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

#ifndef __CANVAS_IMAGE__
#define __CANVAS_IMAGE__

#include <stdint.h>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>

#include "canvas/item.h"

namespace ArdourCanvas {

class Image : public Item
{
public:
    Image (Group *, Cairo::Format, int width, int height);
    
    struct Data {
	Data (boost::shared_array<uint8_t> d, int w, int h, int s, Cairo::Format fmt)
		: data (d)
		, width (w)
		, height (h)
		, stride (s)
		, format (fmt)
	{}

	boost::shared_array<uint8_t> data;
	int width;
	int height;
	int stride;
	Cairo::Format format;
    };

    boost::shared_ptr<Data> get_image ();
    void put_image (boost::shared_ptr<Data>);

    void render (Rect const &, Cairo::RefPtr<Cairo::Context>) const;
    void compute_bounding_box () const;
    
private:
    Cairo::Format            _format;
    int                      _width;
    int                      _height;
    int                      _data;
    mutable boost::shared_ptr<Data>  _current;
    boost::shared_ptr<Data>  _pending;
    mutable bool             _need_render;
    mutable Cairo::RefPtr<Cairo::Surface> _surface;

    void accept_data ();
    PBD::Signal0<void> DataReady;
    PBD::ScopedConnectionList data_connections;
};

}
#endif
