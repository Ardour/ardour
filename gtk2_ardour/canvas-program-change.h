#ifndef CANVASPROGRAMCHANGE_H_
#define CANVASPROGRAMCHANGE_H_

#include "canvas-flag.h"

class MidiRegionView;

namespace Gnome {
namespace Canvas {

class CanvasProgramChange : public CanvasFlag
{
public:
	CanvasProgramChange(
		MidiRegionView&                       region,
		Group&                                parent,
		string&                               text,
		double                                height,
		double                                x = 0.0,
		double                                y = 0.0
	);
	
	virtual ~CanvasProgramChange();
	
private:
};

} // namespace Canvas
} // namespace Gnome

#endif /*CANVASPROGRAMCHANGE_H_*/
