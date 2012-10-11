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

#ifndef __ardour_ui_midi_channel_selector_h__
#define __ardour_ui_midi_channel_selector_h__

#include <set>
#include "boost/shared_ptr.hpp"
#include "sigc++/trackable.h"

#include "gtkmm/table.h"
#include "gtkmm/button.h"
#include "gtkmm/label.h"
#include "gtkmm2ext/stateful_button.h"

#include "ardour/types.h"

class MidiChannelSelector : public Gtk::Table
{
public:
	MidiChannelSelector(int n_rows = 4, int n_columns = 4, int start_row = 0, int start_column = 0);
	virtual ~MidiChannelSelector() = 0;

	sigc::signal<void> clicked;

	void set_channel_colors(const uint32_t new_channel_colors[16]);
	void set_default_channel_color();

protected:
	virtual void button_toggled(Gtk::ToggleButton* button, uint8_t button_nr) = 0;
	Gtk::Label                      _button_labels[4][4];
	Gtkmm2ext::StatefulToggleButton _buttons[4][4];
	int                             _recursion_counter;

	bool              was_clicked (GdkEventButton*);
};

class SingleMidiChannelSelector : public MidiChannelSelector
{
public:
	SingleMidiChannelSelector(uint8_t active_channel = 0);

	uint8_t get_active_channel() const { return _active_channel; }

	sigc::signal<void, uint8_t> channel_selected;

protected:
	virtual void button_toggled(Gtk::ToggleButton* button, uint8_t button_nr);

	Gtk::ToggleButton* _last_active_button;
	uint8_t            _active_channel;
};

class MidiMultipleChannelSelector : public MidiChannelSelector
{
public:
	MidiMultipleChannelSelector(ARDOUR::ChannelMode mode = ARDOUR::FilterChannels,
	                            uint16_t initial_selection = 0xFFFF);

	virtual ~MidiMultipleChannelSelector();

	/** The channel mode or selected channel(s) has changed.
	 *  First parameter is the new channel mode, second parameter is a bitmask
	 *  of the currently selected channels.
	 */
	sigc::signal<void, ARDOUR::ChannelMode, uint16_t> mode_changed;

	void set_channel_mode(ARDOUR::ChannelMode mode, uint16_t mask);
	ARDOUR::ChannelMode get_channel_mode () const { return _channel_mode; }

	/**
	 * @return each bit in the returned word represents a midi channel, eg.
	 *         bit 0 represents channel 0 and bit 15 represents channel 15
	 *
	 */
	uint16_t get_selected_channels() const;
	void     set_selected_channels(uint16_t selected_channels);

protected:
	ARDOUR::ChannelMode _channel_mode;
	ARDOUR::NoteMode    _note_mode;

	virtual void button_toggled(Gtk::ToggleButton* button, uint8_t button_nr);
	void force_channels_button_toggled();

	void select_all(bool on);
	void invert_selection(void);

	Gtk::Button       _select_all;
	Gtk::Button       _select_none;
	Gtk::Button       _invert_selection;
	Gtk::ToggleButton _force_channel;
};

#endif /*__ardour_ui_midi_channel_selector_h__*/
