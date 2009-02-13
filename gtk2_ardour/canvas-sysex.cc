#include <iostream>

#include "ardour_ui.h"

#include "canvas-sysex.h"

using namespace Gnome::Canvas;
using namespace std;

template<typename Time>
CanvasSysEx<Time>::CanvasSysEx(
		MidiRegionView&                       region,
		Group&                                parent,
		string&                               text,
		double                                height,
		double                                x,
		double                                y,
		boost::shared_ptr<Evoral::MIDIEvent<Time> > event
		)
	: CanvasFlag(
			region, 
			parent, 
			height, 
			ARDOUR_UI::config()->canvasvar_MidiSysExOutline.get(), 
			ARDOUR_UI::config()->canvasvar_MidiSysExFill.get(),
			x,
			y
		)
{
	set_text(text);
}

template<typename Time>
CanvasSysEx<Time>::~CanvasSysEx()
{
}

template<typename Time>
bool
CanvasSysEx<Time>::on_event(GdkEvent* ev)
{
	switch (ev->type) {
	case GDK_BUTTON_PRESS:
		if (ev->button.button == 3) {
			return true;
		}
		break;
		
	case GDK_SCROLL:
		if (ev->scroll.direction == GDK_SCROLL_UP) {
			return true;
		} else if (ev->scroll.direction == GDK_SCROLL_DOWN) {
			return true;
		} 
		break;
		
	default:
		break;
	}
	
	return false;
}

template class CanvasSysEx<nframes_t>;
