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

typedef void (*ImageReleaseCallback)(uint8_t *d, void *arg);

namespace ArdourCanvas {


class Image : public Item
{
public:
    Image (Group *, Cairo::Format, int width, int height);
    
    struct Data {
	Data (uint8_t *d, int w, int h, int s, Cairo::Format fmt)
		: data (d)
		, width (w)
		, height (h)
		, stride (s)
		, format (fmt)
		, destroy_callback(NULL)
		, destroy_arg(NULL)
	{}

	virtual ~Data () {
		if (destroy_callback) {
			destroy_callback(data, destroy_arg);
		} else {
			free(data);
		}
	}

	uint8_t* data;
	int width;
	int height;
	int stride;
	Cairo::Format format;
	ImageReleaseCallback  destroy_callback;
	void* destroy_arg;
    };

    /** 
     * Returns a shared_ptr to a Data object that can be used to 
     * write image data to. The Data object will contain a pointer
     * to the buffer, along with image properties that may be
     * useful during the data writing.
     * 
     * Can be called from any thread BUT ..
     *
     * ... to avoid collisions with Image deletion, some synchronization method
     * may be required or the use of shared_ptr<Image> or similar.
     */
    boost::shared_ptr<Data> get_image (bool allocate_data = true);


    /**
     * Queues a Data object to be used to redraw this Image item
     * at the earliest possible opportunity.
     *
     * May be called from any thread BUT ...
     *
     * ... to avoid collisions with Image deletion, some synchronization method
     * may be required or the use of shared_ptr<Image> or similar.
     */
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
