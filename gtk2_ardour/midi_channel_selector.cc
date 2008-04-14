#include "midi_channel_selector.h"
#include <sstream>

using namespace std;

MidiChannelSelector::MidiChannelSelector() :
	Gtk::Table(4,4,true)
{
	property_column_spacing() = 0;
	property_row_spacing() = 0;
	
	uint8_t channel_nr = 0;
	for(int row = 0; row < 4; ++row) {
		for(int column = 0; column < 4; ++column) {
			ostringstream channel;
			channel << int(++channel_nr);
			_button_labels[row][column].set_text(channel.str());
			_button_labels[row][column].set_justify(Gtk::JUSTIFY_RIGHT);
			_buttons[row][column].add(_button_labels[row][column]);
			_buttons[row][column].signal_toggled().connect(
				sigc::bind(
					sigc::mem_fun(this, &MidiChannelSelector::button_toggled),
					&_buttons[row][column],
					channel_nr - 1));
			attach(_buttons[row][column], column, column + 1, row, row + 1);
		}
	}
}

MidiChannelSelector::~MidiChannelSelector()
{
}

SingleMidiChannelSelector::SingleMidiChannelSelector(uint8_t active_channel)
	: MidiChannelSelector()
{
	_active_button = 0;
	Gtk::ToggleButton *button = &_buttons[active_channel / 4][active_channel % 4];
	button->set_active(true);
	_active_button = button;
	_active_channel = active_channel;
}

void
SingleMidiChannelSelector::button_toggled(Gtk::ToggleButton *button, uint8_t channel)
{
	if(button->get_active()) {
		if(_active_button) {
			_active_button->set_active(false);
		}
		_active_button = button;
		_active_channel = channel;
		channel_selected.emit(channel);
	} 
}

void
MidiMultipleChannelSelector::button_toggled(Gtk::ToggleButton *button, uint8_t channel)
{
	if(button->get_active()) {
		_selected_channels.insert(channel);
	} else {
		_selected_channels.erase(channel);
	}
}
