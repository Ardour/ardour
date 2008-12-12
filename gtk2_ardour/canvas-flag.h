#ifndef CANVASFLAG_H_
#define CANVASFLAG_H_

#include <libgnomecanvasmm/group.h>
#include <libgnomecanvasmm/text.h>
#include <libgnomecanvasmm/widget.h>

#include <ardour/midi_model.h>

#include "simplerect.h"
#include "simpleline.h"

class MidiRegionView;

namespace Gnome {
namespace Canvas {

class CanvasFlag : public Group
{
public:
	CanvasFlag(
		MidiRegionView&                       region,
		Group&                                parent,
		double                                height,
		guint                                 outline_color_rgba = 0xc0c0c0ff,
		guint                                 fill_color_rgba = 0x07070707,
		double                                x = 0.0,
		double                                y = 0.0
	) 	: Group(parent, x, y)
	, _text(0)
	, _height(height)
	, _outline_color_rgba(outline_color_rgba)
	, _fill_color_rgba(fill_color_rgba)
	, _region(region)
	, _line(0)
	, _rect(0)
	{}
	
	virtual ~CanvasFlag();
	
	void set_text(string& a_text);

protected:
	Text*                             _text;
	double                            _height;
	guint                             _outline_color_rgba;
	guint                             _fill_color_rgba;
	
private:
	MidiRegionView&                   _region;
	SimpleLine*                       _line;
	SimpleRect*                       _rect;
};

} // namespace Canvas
} // namespace Gnome

#endif /*CANVASFLAG_H_*/
