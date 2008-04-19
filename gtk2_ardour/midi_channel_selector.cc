#include "midi_channel_selector.h"
#include "gtkmm/separator.h"
#include "i18n.h"
#include <sstream>

using namespace std;
using namespace Gtk;
using namespace sigc;

MidiChannelSelector::MidiChannelSelector(int no_rows, int no_columns, int start_row, int start_column) :
	Table(no_rows, no_columns, true)
{	
	assert(no_rows >= 4);
	assert(no_rows >= start_row + 4);
	assert(no_columns >=4);
	assert(no_columns >= start_column + 4);
	
	property_column_spacing() = 0;
	property_row_spacing() = 0;
	
	uint8_t channel_nr = 0;
	for(int row = 0; row < 4; ++row) {
		for(int column = 0; column < 4; ++column) {
			ostringstream channel;
			channel << int(++channel_nr);
			_button_labels[row][column].set_text(channel.str());
			_button_labels[row][column].set_justify(JUSTIFY_RIGHT);
			_buttons[row][column].add(_button_labels[row][column]);
			_buttons[row][column].signal_toggled().connect(
				bind(
					mem_fun(this, &MidiChannelSelector::button_toggled),
					&_buttons[row][column],
					channel_nr - 1));

			int table_row    = start_row + row;
			int table_column = start_column + column;
			attach(_buttons[row][column], table_column, table_column + 1, table_row, table_row + 1);
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
	ToggleButton *button = &_buttons[active_channel / 4][active_channel % 4];
	button->set_active(true);
	_active_button = button;
	_active_channel = active_channel;
}

void
SingleMidiChannelSelector::button_toggled(ToggleButton *button, uint8_t channel)
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

MidiMultipleChannelSelector::MidiMultipleChannelSelector(uint16_t initial_selection)
	: MidiChannelSelector(4, 6, 0, 0)
{
	_select_all.add(*manage(new Label(_("All"))));
	_select_all.signal_clicked().connect(
			bind(mem_fun(this, &MidiMultipleChannelSelector::select_all), true));
	
	_select_none.add(*manage(new Label(_("None"))));
	_select_none.signal_clicked().connect(
			bind(mem_fun(this, &MidiMultipleChannelSelector::select_all), false));
	
	_invert_selection.add(*manage(new Label(_("Invert"))));
	_invert_selection.signal_clicked().connect(
			mem_fun(this, &MidiMultipleChannelSelector::invert_selection));
	
	_force_channel.add(*manage(new Label(_("Force"))));

	set_homogeneous(false);
	attach(*manage(new VSeparator()), 4, 5, 0, 4, SHRINK, FILL, 0, 0);
	//set_row_spacing(4, -5);
	attach(_select_all,       5, 6, 0, 1);
	attach(_select_none,      5, 6, 1, 2);
	attach(_invert_selection, 5, 6, 2, 3);
	attach(_force_channel,    5, 6, 3, 4);
	
	_selected_channels = 0;
	for(uint16_t i = 0; i < 16; i++) {
		ToggleButton *button = &_buttons[i / 4][i % 4];
		if(initial_selection & (1L << i)) {
			button->set_active(true);
		} else {
			button->set_active(false);
		}
	}
}

void
MidiMultipleChannelSelector::button_toggled(ToggleButton *button, uint8_t channel)
{
	_selected_channels = _selected_channels ^ (1L << channel); 
	selection_changed.emit(_selected_channels);
}

void 
MidiMultipleChannelSelector::select_all(bool on)
{
	for(uint16_t i = 0; i < 16; i++) {
		ToggleButton *button = &_buttons[i / 4][i % 4];
		button->set_active(on);
	}
	selection_changed.emit(_selected_channels);
}

void 
MidiMultipleChannelSelector::invert_selection(void)
{
	for(uint16_t i = 0; i < 16; i++) {
		ToggleButton *button = &_buttons[i / 4][i % 4];
		if(button->get_active()) {
			button->set_active(false);
		} else {
			button->set_active(true);
		}
	}
	selection_changed.emit(_selected_channels);
}

