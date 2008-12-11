#include "canvas-program-change.h"
#include <iostream>
#include "ardour_ui.h"

using namespace Gnome::Canvas;
using namespace std;

CanvasProgramChange::CanvasProgramChange(
		MidiRegionView&                       region,
		Group&                                parent,
		string                                text,
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

