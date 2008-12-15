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
	
	virtual bool on_event(GdkEvent* ev);
	
	nframes_t event_time() const { return _event_time; }
	void set_event_time(nframes_t new_time) { _event_time = new_time; };

	uint8_t program() const { return _program; }
	void set_program(uint8_t new_program) { _program = new_program; };

	uint8_t channel() const { return _channel; }
	void set_channel(uint8_t new_channel) { _channel = new_channel; };
	

private:
	nframes_t _event_time;
	uint8_t   _program;
	uint8_t   _channel;
};

} // namespace Canvas
} // namespace Gnome

#endif /*CANVASPROGRAMCHANGE_H_*/
