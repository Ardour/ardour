#ifndef CANVAS_SYSEX_H_
#define CANVAS_SYSEX_H_

#include "canvas-flag.h"
#include <evoral/MIDIEvent.hpp>

class MidiRegionView;

namespace Gnome {
namespace Canvas {

template<typename Time>
class CanvasSysEx : public CanvasFlag
{
public:
	CanvasSysEx(
		MidiRegionView&                       region,
		Group&                                parent,
		string&                               text,
		double                                height,
		double                                x,
		double                                y);
	
	virtual ~CanvasSysEx();
	
	virtual bool on_event(GdkEvent* ev);
	

private:
};

} // namespace Canvas
} // namespace Gnome

#endif /*CANVAS_SYSEX_H_*/
