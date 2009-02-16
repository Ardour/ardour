#ifndef CANVASFLAG_H_
#define CANVASFLAG_H_

#include <string>
#include <libgnomecanvasmm/group.h>
#include <libgnomecanvasmm/widget.h>

#include <ardour/midi_model.h>

#include "simplerect.h"
#include "simpleline.h"
#include "interactive-item.h"

class MidiRegionView;

namespace Gnome {
namespace Canvas {

class CanvasFlag : public Group, public InteractiveItem
{
public:
	CanvasFlag(
			MidiRegionView& region,
			Group&          parent,
			double          height,
			guint           outline_color_rgba = 0xc0c0c0ff,
			guint           fill_color_rgba = 0x07070707,
			double          x = 0.0,
			double          y = 0.0)
		: Group(parent, x, y)
		, _text(0)
		, _height(height)
		, _outline_color_rgba(outline_color_rgba)
		, _fill_color_rgba(fill_color_rgba)
		, _region(region)
		, _line(0)
		, _rect(0)
	{}
			
	virtual ~CanvasFlag();
	
	virtual bool on_event(GdkEvent* ev);

	void set_text(const std::string& a_text);

protected:
	InteractiveText* _text;
	double           _height;
	guint            _outline_color_rgba;
	guint            _fill_color_rgba;
	MidiRegionView&  _region;
	
private:
	void delete_allocated_objects();
	
	SimpleLine*      _line;
	InteractiveRect* _rect;
};


} // namespace Canvas
} // namespace Gnome

#endif /*CANVASFLAG_H_*/
