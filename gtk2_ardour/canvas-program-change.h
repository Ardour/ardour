#ifndef CANVASPROGRAMCHANGE_H_
#define CANVASPROGRAMCHANGE_H_

#include "canvas-flag.h"

class MidiRegionView;

namespace MIDI {
	namespace Name {
		struct PatchPrimaryKey;
	}
}

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
		double                                x,
		double                                y,
		string&                               model_name,
		string&                               custom_device_mode,
		nframes_t                             event_time,
		uint8_t                               channel,
		uint8_t                               program
	);
	
	virtual ~CanvasProgramChange();
	
	virtual bool on_event(GdkEvent* ev);
	
	string model_name() const { return _model_name; }
	void set_model_name(string model_name) { _model_name = model_name; }
	
	string custom_device_mode() const { return _custom_device_mode; }
	void set_custom_device_mode(string custom_device_mode) { _custom_device_mode = custom_device_mode; }

	nframes_t event_time() const { return _event_time; }
	void set_event_time(nframes_t new_time) { _event_time = new_time; };

	uint8_t program() const { return _program; }
	void set_program(uint8_t new_program) { _program = new_program; };

	uint8_t channel() const { return _channel; }
	void set_channel(uint8_t new_channel) { _channel = new_channel; };
	
	void initialize_popup_menus();
	
	void on_patch_menu_selected(const MIDI::Name::PatchPrimaryKey& key);

private:
	string        _model_name;
	string        _custom_device_mode;   
	nframes_t     _event_time;
	uint8_t       _channel;
	uint8_t       _program;
	Gtk::Menu     _popup;
};

} // namespace Canvas
} // namespace Gnome

#endif /*CANVASPROGRAMCHANGE_H_*/
