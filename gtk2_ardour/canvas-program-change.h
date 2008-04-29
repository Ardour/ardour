#ifndef CANVASPROGRAMCHANGE_H_
#define CANVASPROGRAMCHANGE_H_

#include <libgnomecanvasmm/group.h>
#include "simplerect.h"
#include "simpleline.h"
#include "midi_region_view.h"
#include <libgnomecanvasmm/text.h>
#include <libgnomecanvasmm/widget.h>
#include <ardour/midi_model.h>

namespace Gnome
{
namespace Canvas
{

class CanvasProgramChange : public Group
{
public:
	CanvasProgramChange(
		MidiRegionView&                       region,
		Group&                                parent,
		boost::shared_ptr<MIDI::Event>        event,
		double                                height,
		double                                x = 0.0,
		double                                y = 0.0
	);
	
	virtual ~CanvasProgramChange();
	
private:
	MidiRegionView&                   _region;
	boost::shared_ptr<MIDI::Event>    _event;
	Text*                             _text;
	SimpleLine*                       _line;
	SimpleRect*                       _rect;
	Widget*                           _widget;
};

} // namespace Canvas
} // namespace Gnome

#endif /*CANVASPROGRAMCHANGE_H_*/
