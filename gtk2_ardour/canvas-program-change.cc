#include "canvas-program-change.h"
#include <iostream>
#include "ardour_ui.h"
#include "midi_region_view.h"

using namespace Gnome::Canvas;
using namespace std;

CanvasProgramChange::CanvasProgramChange(
		MidiRegionView&                       region,
		Group&                                parent,
		string&                               text,
		double                                height,
		double                                x,
		double                                y)
	: CanvasFlag(
			region, 
			parent, 
			height, 
			ARDOUR_UI::config()->canvasvar_MidiProgramChangeOutline.get(), 
			ARDOUR_UI::config()->canvasvar_MidiProgramChangeFill.get(),
			x,
			y
		)
{
	set_text(text);
}

CanvasProgramChange::~CanvasProgramChange()
{
}

bool
CanvasProgramChange::on_event(GdkEvent* ev)
{
	switch (ev->type) {
	case GDK_SCROLL:
		if (ev->scroll.direction == GDK_SCROLL_UP) {
			_region.previous_program(*this);
			return true;
		} else if (ev->scroll.direction == GDK_SCROLL_DOWN) {
			_region.next_program(*this);
			return true;
		} 
	default:
		break;
	}
	
	return false;
}
