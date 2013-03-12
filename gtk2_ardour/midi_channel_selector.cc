/*
    Copyright (C) 2008 Paul Davis
    Author: Hans Baier

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <algorithm>
#include <sstream>

#include "midi_channel_selector.h"
#include "gtkmm/separator.h"
#include "i18n.h"
#include "rgb_macros.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;

MidiChannelSelector::MidiChannelSelector(int n_rows, int n_columns, int start_row, int start_column)
	: Table(n_rows, n_columns, true)
	, _recursion_counter(0)
{
	n_rows    = std::max(4, n_rows);
	n_rows    = std::max(4, start_row + 4);
	n_columns = std::max(4, n_columns);
	n_columns = std::max(4, start_column + 4);

	property_column_spacing() = 0;
	property_row_spacing() = 0;

	uint8_t channel_nr = 0;
	for (int row = 0; row < 4; ++row) {
		for (int column = 0; column < 4; ++column) {
			ostringstream channel;
			channel << int(++channel_nr);
			_button_labels[row][column].set_text(channel.str());
			_button_labels[row][column].set_justify(JUSTIFY_RIGHT);
			_buttons[row][column].add(_button_labels[row][column]);
			_buttons[row][column].signal_toggled().connect(
				sigc::bind(
					sigc::mem_fun(this, &MidiChannelSelector::button_toggled),
					&_buttons[row][column],
					channel_nr - 1));
			_buttons[row][column].set_widget_name (X_("MidiChannelSelectorButton"));

			_buttons[row][column].signal_button_release_event().connect(
                                sigc::mem_fun(this, &MidiChannelSelector::was_clicked), false);

			int table_row    = start_row + row;
			int table_column = start_column + column;
			attach(_buttons[row][column], table_column, table_column + 1, table_row, table_row + 1);
		}
	}
}

MidiChannelSelector::~MidiChannelSelector()
{
}

bool
MidiChannelSelector::was_clicked (GdkEventButton*)
{
        clicked ();
        return false;
}

void
MidiChannelSelector::set_channel_colors(const uint32_t new_channel_colors[16])
{
	for (int row = 0; row < 4; ++row) {
		for (int column = 0; column < 4; ++column) {
			char color_normal[8];
			char color_active[8];
			snprintf(color_normal, 8, "#%x", UINT_INTERPOLATE(new_channel_colors[row * 4 + column], 0x000000ff, 0.6));
			snprintf(color_active, 8, "#%x", new_channel_colors[row * 4 + column]);
			_buttons[row][column].modify_bg(STATE_NORMAL, Gdk::Color(color_normal));
			_buttons[row][column].modify_bg(STATE_ACTIVE, Gdk::Color(color_active));
		}
	}
}

void
MidiChannelSelector::set_default_channel_color()
{
	for (int row = 0; row < 4; ++row) {
		for (int column = 0; column < 4; ++column) {
			_buttons[row][column].unset_fg (STATE_NORMAL);
			_buttons[row][column].unset_fg (STATE_ACTIVE);
			_buttons[row][column].unset_bg (STATE_NORMAL);
			_buttons[row][column].unset_bg (STATE_ACTIVE);
		}
	}
}

SingleMidiChannelSelector::SingleMidiChannelSelector(uint8_t active_channel)
	: MidiChannelSelector()
{
	_last_active_button = 0;
	ToggleButton* button = &_buttons[active_channel / 4][active_channel % 4];
	_active_channel = active_channel;
	button->set_active(true);
	_last_active_button = button;
}

void
SingleMidiChannelSelector::button_toggled(ToggleButton* button, uint8_t channel)
{
	++_recursion_counter;
	if (_recursion_counter == 1) {
		// if the current button is active it must
		// be different from the first one
		if (button->get_active()) {
			if (_last_active_button) {
				_last_active_button->set_active(false);
				_active_channel = channel;
				_last_active_button = button;
				channel_selected.emit(channel);
			}
		} else {
			// if not, the user pressed the already active button
			button->set_active(true);
			_active_channel = channel;
		}
	}
	--_recursion_counter;
}

MidiMultipleChannelSelector::MidiMultipleChannelSelector(ChannelMode mode, uint16_t mask)
	: MidiChannelSelector(4, 6, 0, 0)
	, _channel_mode(mode)
{
	_select_all.add(*manage(new Label(_("All"))));
	_select_all.signal_clicked().connect(
			sigc::bind(sigc::mem_fun(this, &MidiMultipleChannelSelector::select_all), true));

	_select_none.add(*manage(new Label(_("None"))));
	_select_none.signal_clicked().connect(
			sigc::bind(sigc::mem_fun(this, &MidiMultipleChannelSelector::select_all), false));

	_invert_selection.add(*manage(new Label(_("Invert"))));
	_invert_selection.signal_clicked().connect(
			sigc::mem_fun(this, &MidiMultipleChannelSelector::invert_selection));

	_force_channel.add(*manage(new Label(_("Force"))));
	_force_channel.signal_toggled().connect(
			sigc::mem_fun(this, &MidiMultipleChannelSelector::force_channels_button_toggled));

	set_homogeneous(false);
	attach(*manage(new VSeparator()), 4, 5, 0, 4, SHRINK, FILL, 0, 0);
	//set_row_spacing(4, -5);
	attach(_select_all,       5, 6, 0, 1);
	attach(_select_none,      5, 6, 1, 2);
	attach(_invert_selection, 5, 6, 2, 3);
	attach(_force_channel,    5, 6, 3, 4);

	set_selected_channels(mask);
}

MidiMultipleChannelSelector::~MidiMultipleChannelSelector()
{
	mode_changed.clear();
}

void
MidiMultipleChannelSelector::set_channel_mode(ChannelMode mode, uint16_t mask)
{
	switch (mode) {
	case AllChannels:
		_force_channel.set_active(false);
		set_selected_channels(0xFFFF);
		break;
	case FilterChannels:
		_force_channel.set_active(false);
		set_selected_channels(mask);
		break;
	case ForceChannel:
		_force_channel.set_active(true);
		for (uint16_t i = 0; i < 16; i++) {
			ToggleButton* button = &_buttons[i / 4][i % 4];
			button->set_active(i == mask);
		}
	}
}

uint16_t
MidiMultipleChannelSelector::get_selected_channels() const
{
	uint16_t selected_channels = 0;
	for (uint16_t i = 0; i < 16; i++) {
		const ToggleButton* button = &_buttons[i / 4][i % 4];
		if (button->get_active()) {
			selected_channels |= (1L << i);
		}
	}

	return selected_channels;
}

void
MidiMultipleChannelSelector::set_selected_channels(uint16_t selected_channels)
{
	for (uint16_t i = 0; i < 16; i++) {
		ToggleButton* button = &_buttons[i / 4][i % 4];
		if (selected_channels & (1L << i)) {
			button->set_active(true);
		} else {
			button->set_active(false);
		}
	}
}

void
MidiMultipleChannelSelector::button_toggled(ToggleButton */*button*/, uint8_t channel)
{
	++_recursion_counter;
	if (_recursion_counter == 1) {
		if (_channel_mode == ForceChannel) {
			mode_changed.emit(_channel_mode, channel);
			set_selected_channels(1 << channel);
		} else {
			mode_changed.emit(_channel_mode, get_selected_channels());
		}
	}
	--_recursion_counter;
}

void
MidiMultipleChannelSelector::force_channels_button_toggled()
{
	if (_force_channel.get_active()) {
		_channel_mode = ForceChannel;
		bool found_first_active = false;
		// leave only the first button enabled
		uint16_t active_channel = 0;
		for (int i = 0; i <= 15; i++) {
			ToggleButton* button = &_buttons[i / 4][i % 4];
			if (button->get_active()) {
				if (found_first_active) {
					++_recursion_counter;
					button->set_active(false);
					--_recursion_counter;
				} else {
					found_first_active = true;
					active_channel = i;
				}
			}
		}

		if (!found_first_active) {
			_buttons[0][0].set_active(true);
		}

		_select_all.set_sensitive(false);
		_select_none.set_sensitive(false);
		_invert_selection.set_sensitive(false);
		mode_changed.emit(_channel_mode, active_channel);
	} else {
		_channel_mode = FilterChannels;
		_select_all.set_sensitive(true);
		_select_none.set_sensitive(true);
		_invert_selection.set_sensitive(true);
		mode_changed.emit(FilterChannels, get_selected_channels());
	}
}

void
MidiMultipleChannelSelector::select_all(bool on)
{
	if (_channel_mode == ForceChannel)
		return;

	++_recursion_counter;
	for (uint16_t i = 0; i < 16; i++) {
		ToggleButton* button = &_buttons[i / 4][i % 4];
		button->set_active(on);
	}
	--_recursion_counter;
	mode_changed.emit(_channel_mode, get_selected_channels());
}

void
MidiMultipleChannelSelector::invert_selection(void)
{
	if (_channel_mode == ForceChannel)
		return;

	++_recursion_counter;
	for (uint16_t i = 0; i < 16; i++) {
		ToggleButton* button = &_buttons[i / 4][i % 4];
		if (button->get_active()) {
			button->set_active(false);
		} else {
			button->set_active(true);
		}
	}
	--_recursion_counter;
	mode_changed.emit(_channel_mode, get_selected_channels());
}

