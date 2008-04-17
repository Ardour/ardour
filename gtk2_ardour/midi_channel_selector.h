#ifndef __ardour_ui_midi_channel_selector_h__
#define __ardour_ui_midi_channel_selector_h__

#include "boost/shared_ptr.hpp"
#include "gtkmm/table.h"
#include "sigc++/trackable.h"
#include "gtkmm/button.h"
#include "gtkmm/togglebutton.h"
#include "gtkmm/label.h"
#include <set>

class MidiChannelSelector : public Gtk::Table
{
public:
	MidiChannelSelector(int no_rows = 4, int no_columns = 4, int start_row = 0, int start_column = 0);
	virtual ~MidiChannelSelector() = 0;
	
protected:
	virtual void button_toggled(Gtk::ToggleButton *button, uint8_t button_nr) = 0;
	Gtk::Label        _button_labels[4][4];
	Gtk::ToggleButton _buttons[4][4];
};

class SingleMidiChannelSelector : public MidiChannelSelector
{
public:
	SingleMidiChannelSelector(uint8_t active_channel = 0);
	
	const uint8_t get_active_channel() const { return _active_channel; }
	
	sigc::signal<void, uint8_t> channel_selected;
	
protected:
	virtual void button_toggled(Gtk::ToggleButton *button, uint8_t button_nr);

	Gtk::ToggleButton *_active_button;
	uint8_t _active_channel;
};

class MidiMultipleChannelSelector : public MidiChannelSelector
{
public:
	MidiMultipleChannelSelector(uint16_t initial_selection = 1);
	
	const uint16_t get_selected_channels() const { return _selected_channels; }
	
	sigc::signal<void, uint16_t> selection_changed;

protected:
	virtual void button_toggled(Gtk::ToggleButton *button, uint8_t button_nr);
	
	void select_all(bool on);
	void invert_selection(void);
	
	Gtk::Button            _select_all;
	Gtk::Button            _select_none;
	Gtk::Button            _invert_selection;
	uint16_t               _selected_channels;
};

#endif /*__ardour_ui_midi_channel_selector_h__*/
