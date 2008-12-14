#include "canvas-program-change.h"
#include <iostream>
#include "ardour_ui.h"

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
	cerr << "CanvasProgramChange::on_event(GdkEvent* ev) type " << ev->type << endl;
	switch (ev->type) {
	case GDK_SCROLL:
		if (ev->scroll.direction == GDK_SCROLL_UP) {
			previous_patch();
			return true;
		} else if (ev->scroll.direction == GDK_SCROLL_DOWN) {
			next_patch();
			return true;
		} 
	default:
		break;
	}
	
	return false;
}

void 
CanvasProgramChange::previous_patch()
{
	cerr << "decreasing program" <<  endl;
}

void 
CanvasProgramChange::next_patch()
{
	cerr << "increasing program" <<  endl;
}
