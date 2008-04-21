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
	int               _recursion_counter;
};

class SingleMidiChannelSelector : public MidiChannelSelector
{
public:
	SingleMidiChannelSelector(uint8_t active_channel = 0);
	
	const uint8_t get_active_channel() const { return _active_channel; }
	
	sigc::signal<void, uint8_t> channel_selected;
	
protected:
	virtual void button_toggled(Gtk::ToggleButton *button, uint8_t button_nr);

	Gtk::ToggleButton *_last_active_button;
	uint8_t _active_channel;
};

class MidiMultipleChannelSelector : public MidiChannelSelector
{
public:
	MidiMultipleChannelSelector(uint16_t initial_selection = 1);
	virtual ~MidiMultipleChannelSelector();
	
	/**
	 * @return each bit in the returned word represents a midi channel, eg. 
	 *         bit 0 represents channel 0 and bit 15 represents channel 15
	 *          
	 */
	const uint16_t get_selected_channels() const;
	void set_selected_channels(uint16_t selected_channels);
	
	sigc::signal<void, uint16_t> selection_changed;
	sigc::signal<void, int8_t>   force_channel_changed;
	
	const int8_t get_force_channel() const;
protected:
	enum Mode {
		FILTERING_MULTIPLE_CHANNELS,
		FORCING_SINGLE_CHANNEL
	}; 

	Mode _mode;
	
	virtual void button_toggled(Gtk::ToggleButton *button, uint8_t button_nr);
	void force_channels_button_toggled();
	
	void select_all(bool on);
	void invert_selection(void);
	
	Gtk::Button            _select_all;
	Gtk::Button            _select_none;
	Gtk::Button            _invert_selection;
	Gtk::ToggleButton      _force_channel;
};

#endif /*__ardour_ui_midi_channel_selector_h__*/
