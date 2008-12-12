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

class CanvasFlagRect;
class CanvasFlagText;

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
	
	virtual bool on_event(GdkEvent* ev);

	void set_text(string& a_text);

protected:
	CanvasFlagText*                         _text;
	double                            _height;
	guint                             _outline_color_rgba;
	guint                             _fill_color_rgba;
	
private:
	void delete_allocated_objects();
	
	MidiRegionView&                   _region;
	SimpleLine*                       _line;
	CanvasFlagRect*                   _rect;
};

class CanvasFlagText: public Text
{
public:
	CanvasFlagText(Group& parent, double x, double y, const Glib::ustring& text) 
		: Text(parent, x, y, text) {
		_parent = dynamic_cast<CanvasFlag*>(&parent);
;
	}
	
	virtual bool on_event(GdkEvent* ev) {
		if(_parent) {
			return _parent->on_event(ev);
		} else {
			return false;
		}
	}

private:
	CanvasFlag* _parent;
};

class CanvasFlagRect: public SimpleRect
{
public:
	CanvasFlagRect(Group& parent, double x1, double y1, double x2, double y2) 
		: SimpleRect(parent, x1, y1, x2, y2) {
		_parent = dynamic_cast<CanvasFlag*>(&parent);
	}
	
	virtual bool on_event(GdkEvent* ev) {
		if(_parent) {
			return _parent->on_event(ev);
		} else {
			return false;
		}
	}

private:
	CanvasFlag* _parent;
};


} // namespace Canvas
} // namespace Gnome

#endif /*CANVASFLAG_H_*/
