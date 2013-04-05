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
#include "gtkmm/radiobutton.h"
#include "gtkmm/label.h"
#include "gtkmm2ext/stateful_button.h"

#include "ardour/types.h"

#include "ardour_window.h"

namespace ARDOUR {
	class MidiTrack;
}

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

class MidiChannelSelectorWindow : public ArdourWindow, public PBD::ScopedConnectionList
{
  public:
    MidiChannelSelectorWindow (boost::shared_ptr<ARDOUR::MidiTrack>);
    ~MidiChannelSelectorWindow ();

    void set_channel_colors (const uint32_t new_channel_colors[16]);
    void set_default_channel_color();

  private:
    boost::shared_ptr<ARDOUR::MidiTrack> track;
    std::vector<Gtk::ToggleButton*> playback_buttons;
    std::vector<Gtk::ToggleButton*> capture_buttons;

    std::vector<Gtk::Widget*> playback_mask_controls;
    std::vector<Gtk::Widget*> capture_mask_controls;

    Gtk::HBox         capture_mask_box;
    Gtk::HBox         playback_mask_box;
    Gtk::RadioButtonGroup playback_button_group;
    Gtk::RadioButton playback_all_button;
    Gtk::RadioButton playback_filter_button;
    Gtk::RadioButton playback_force_button;
    Gtk::RadioButtonGroup capture_button_group;
    Gtk::RadioButton capture_all_button;
    Gtk::RadioButton capture_filter_button;
    Gtk::RadioButton capture_force_button;

    ARDOUR::ChannelMode last_drawn_capture_mode;
    ARDOUR::ChannelMode last_drawn_playback_mode;

    void build();
    void set_capture_selected_channels (uint16_t);
    void set_playback_selected_channels (uint16_t);

    void fill_playback_mask ();
    void zero_playback_mask ();
    void invert_playback_mask ();

    void fill_capture_mask ();
    void zero_capture_mask ();
    void invert_capture_mask ();

    void playback_mask_changed ();
    void capture_mask_changed ();
    void playback_mode_changed ();
    void capture_mode_changed ();

    void playback_channel_clicked (uint16_t);
    void capture_channel_clicked (uint16_t);

    void playback_all_clicked();
    void playback_none_clicked();
    void playback_invert_clicked();

    void capture_all_clicked();
    void capture_none_clicked();
    void capture_invert_clicked();

    void capture_mode_toggled (ARDOUR::ChannelMode);
    void playback_mode_toggled (ARDOUR::ChannelMode);
};

#endif /*__ardour_ui_midi_channel_selector_h__*/
