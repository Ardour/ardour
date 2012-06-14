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
		    double          height,
		    guint           outline_color_rgba = 0xc0c0c0ff,
		    guint           fill_color_rgba = 0x07070707,
		    double          x = 0.0,
		    double          y = 0.0);

	virtual ~CanvasFlag();

	virtual bool on_event(GdkEvent* ev);

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
