#include "midi_channel_selector.h"
#include "gtkmm/separator.h"
#include "i18n.h"
#include <sstream>

using namespace std;

MidiChannelSelector::MidiChannelSelector(int no_rows, int no_columns, int start_row, int start_column) :
	Gtk::Table(no_rows, no_columns, true)
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
			_button_labels[row][column].set_justify(Gtk::JUSTIFY_RIGHT);
			_buttons[row][column].add(_button_labels[row][column]);
			_buttons[row][column].signal_toggled().connect(
				sigc::bind(
					sigc::mem_fun(this, &MidiChannelSelector::button_toggled),
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

MidiMultipleChannelSelector::MidiMultipleChannelSelector(uint16_t initial_selection)
	: MidiChannelSelector(6, 4, 0, 0)
{
	_select_all.add(*new Gtk::Label(_("All")));
	_select_all.signal_clicked().connect(
			sigc::bind(sigc::mem_fun(this, &MidiMultipleChannelSelector::select_all), true));
	
	_select_none.add(*new Gtk::Label(_("None")));
	_select_none.signal_clicked().connect(
			sigc::bind(sigc::mem_fun(this, &MidiMultipleChannelSelector::select_all), false));
	
	_invert_selection.add(*new Gtk::Label(_("Invert")));
	_invert_selection.signal_clicked().connect(
			sigc::mem_fun(this, &MidiMultipleChannelSelector::invert_selection));

	set_homogeneous(false);
	attach(*new Gtk::HSeparator(), 0, 4, 4, 5, Gtk::FILL, Gtk::SHRINK, 0, 0);
	set_col_spacing(4, -5);
	attach(_select_all,       0, 2, 5, 6);
	attach(_select_none,      2, 4, 5, 6);
	attach(_invert_selection, 0, 4, 6, 7);
	
	_selected_channels = 0;
	for(uint16_t i = 0; i < 16; i++) {
		Gtk::ToggleButton *button = &_buttons[i / 4][i % 4];
		if(initial_selection & (1L << i)) {
			button->set_active(true);
		} else {
			button->set_active(false);
		}
	}
}

void
MidiMultipleChannelSelector::button_toggled(Gtk::ToggleButton *button, uint8_t channel)
{
	_selected_channels = _selected_channels ^ (1L << channel); 
}

void 
MidiMultipleChannelSelector::select_all(bool on)
{
	for(uint16_t i = 0; i < 16; i++) {
		Gtk::ToggleButton *button = &_buttons[i / 4][i % 4];
		button->set_active(on);
	}
}

void 
MidiMultipleChannelSelector::invert_selection(void)
{
	for(uint16_t i = 0; i < 16; i++) {
		Gtk::ToggleButton *button = &_buttons[i / 4][i % 4];
		if(button->get_active()) {
			button->set_active(false);
		} else {
			button->set_active(true);
		}
	}
}

