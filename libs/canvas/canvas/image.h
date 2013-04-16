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
    XMLNode* get_state () const;
    void set_state (XMLNode const *);
    
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
