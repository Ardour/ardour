/*
    Copyright (C) 2008-2013 Paul Davis
    Original Author: Hans Baier

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
#include <gtkmm/separator.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/table.h>

#include "pbd/compose.h"

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "ardour/midi_track.h"

#include "midi_channel_selector.h"
#include "rgb_macros.h"

#include "i18n.h"

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

/*-----------------------------------------*/

MidiChannelSelectorWindow::MidiChannelSelectorWindow (boost::shared_ptr<MidiTrack> mt)
	: ArdourWindow (_("MIDI Channel Control"))
	, track (mt)
	, playback_all_button (playback_button_group, _("Playback all channels"))
	, playback_filter_button (playback_button_group, _("Play only selected channels"))
	, playback_force_button (playback_button_group, _("Use a single fixed channel for all playback"))
	, capture_all_button (capture_button_group, _("Record all channels"))
	, capture_filter_button (capture_button_group, _("Record only selected channels"))
	, capture_force_button (capture_button_group, _("Force all channels to 1 channel"))
	, last_drawn_capture_mode (AllChannels)
	, last_drawn_playback_mode (AllChannels)
{
	build ();

	playback_mode_changed ();
	capture_mode_changed ();

	playback_mask_changed ();
	capture_mask_changed ();

	track->PlaybackChannelMaskChanged.connect (*this, MISSING_INVALIDATOR, boost::bind (&MidiChannelSelectorWindow::playback_mask_changed, this), gui_context());
	track->PlaybackChannelModeChanged.connect (*this, MISSING_INVALIDATOR, boost::bind (&MidiChannelSelectorWindow::playback_mode_changed, this), gui_context());
	track->CaptureChannelMaskChanged.connect (*this, MISSING_INVALIDATOR, boost::bind (&MidiChannelSelectorWindow::capture_mask_changed, this), gui_context());
	track->CaptureChannelModeChanged.connect (*this, MISSING_INVALIDATOR, boost::bind (&MidiChannelSelectorWindow::capture_mode_changed, this), gui_context());
}

MidiChannelSelectorWindow::~MidiChannelSelectorWindow()
{
}

void
MidiChannelSelectorWindow::build ()
{
	VBox* vpacker;
	HBox* capture_controls;
	HBox* playback_controls;
        Button* b;
        Label* l;

        vpacker = manage (new VBox);
        vpacker->set_spacing (6);
        vpacker->set_border_width (12);

	l = manage (new Label (string_compose (("<span size=\"larger\" weight=\"bold\">%1: %2</span>"), _("MIDI Channel Control"), track->name())));
	l->set_use_markup (true);
	l->set_alignment (0.5, 0.0);

	vpacker->pack_start (*l, true, true);

        l = manage (new Label (string_compose ("<span size=\"large\" weight=\"bold\">%1</span>", _("Inbound"))));
	l->set_use_markup (true);
        vpacker->pack_start (*l);


	vpacker->pack_start (capture_all_button);
	capture_all_button.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &MidiChannelSelectorWindow::capture_mode_toggled), AllChannels));
	
	vpacker->pack_start (capture_filter_button);
	capture_filter_button.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &MidiChannelSelectorWindow::capture_mode_toggled), FilterChannels));
	
	vpacker->pack_start (capture_force_button);
	capture_force_button.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &MidiChannelSelectorWindow::capture_mode_toggled), ForceChannel));

        vpacker->pack_start (capture_mask_box);
	
	capture_controls = manage (new HBox);
	capture_controls->set_spacing (6);

        b = manage (new Button (_("All")));
	Gtkmm2ext::UI::instance()->set_tip (*b, _("Click to enable recording all channels"));
	capture_controls->pack_start (*b);
	capture_mask_controls.push_back (b);
	b->signal_clicked().connect (sigc::mem_fun (*this, &MidiChannelSelectorWindow::fill_capture_mask)); 
        b = manage (new Button (_("None")));
	Gtkmm2ext::UI::instance()->set_tip (*b, _("Click to disable recording all channels"));
	capture_controls->pack_start (*b);
	capture_mask_controls.push_back (b);
	b->signal_clicked().connect (sigc::mem_fun (*this, &MidiChannelSelectorWindow::zero_capture_mask)); 
        b = manage (new Button (_("Invert")));
	Gtkmm2ext::UI::instance()->set_tip (*b, _("Click to invert currently selected recording channels"));
	capture_controls->pack_start (*b);
	capture_mask_controls.push_back (b);
	b->signal_clicked().connect (sigc::mem_fun (*this, &MidiChannelSelectorWindow::invert_capture_mask)); 

        vpacker->pack_start (*capture_controls);

        l = manage (new Label (string_compose ("<span size=\"large\" weight=\"bold\">%1</span>", _("Playback"))));
	l->set_use_markup (true);
        vpacker->pack_start (*l);

	vpacker->pack_start (playback_all_button);
	playback_all_button.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &MidiChannelSelectorWindow::playback_mode_toggled), AllChannels));
	
	vpacker->pack_start (playback_filter_button);
	playback_filter_button.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &MidiChannelSelectorWindow::playback_mode_toggled), FilterChannels));
	
	vpacker->pack_start (playback_force_button);
	playback_force_button.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &MidiChannelSelectorWindow::playback_mode_toggled), ForceChannel));

	vpacker->pack_start (playback_mask_box);

	playback_controls = manage (new HBox);
	playback_controls->set_spacing (6);

        b = manage (new Button (_("All")));
	Gtkmm2ext::UI::instance()->set_tip (*b, _("Click to enable playback of all channels"));
	playback_controls->pack_start (*b);
	playback_mask_controls.push_back (b);
	b->signal_clicked().connect (sigc::mem_fun (*this, &MidiChannelSelectorWindow::fill_playback_mask)); 
        b = manage (new Button (_("None")));
	Gtkmm2ext::UI::instance()->set_tip (*b, _("Click to disable playback of all channels"));
	playback_controls->pack_start (*b);
	playback_mask_controls.push_back (b);
	b->signal_clicked().connect (sigc::mem_fun (*this, &MidiChannelSelectorWindow::zero_playback_mask)); 
	b = manage (new Button (_("Invert")));
	Gtkmm2ext::UI::instance()->set_tip (*b, _("Click to invert current selected playback channels"));
	playback_controls->pack_start (*b);
	playback_mask_controls.push_back (b);
	b->signal_clicked().connect (sigc::mem_fun (*this, &MidiChannelSelectorWindow::invert_playback_mask));

        vpacker->pack_start (*playback_controls);

        add (*vpacker);
}

void
MidiChannelSelectorWindow::fill_playback_mask ()
{
	if (track->get_playback_channel_mode() == FilterChannels) {
		track->set_playback_channel_mask (0xffff);
	}
}

void
MidiChannelSelectorWindow::zero_playback_mask ()
{
	if (track->get_playback_channel_mode() == FilterChannels) {
		track->set_playback_channel_mask (0);
	}
}

void
MidiChannelSelectorWindow::invert_playback_mask ()
{
	if (track->get_playback_channel_mode() == FilterChannels) {
		track->set_playback_channel_mask (~track->get_playback_channel_mask());
	}
}

void
MidiChannelSelectorWindow::fill_capture_mask ()
{
	if (track->get_capture_channel_mode() == FilterChannels) {
		track->set_capture_channel_mask (0xffff);
	}
}

void
MidiChannelSelectorWindow::zero_capture_mask ()
{
	if (track->get_capture_channel_mode() == FilterChannels) {
		track->set_capture_channel_mask (0);
	}
}

void
MidiChannelSelectorWindow::invert_capture_mask ()
{
	if (track->get_capture_channel_mode() == FilterChannels) {
		track->set_capture_channel_mask (~track->get_capture_channel_mask());
	}
}
		
void
MidiChannelSelectorWindow::set_playback_selected_channels (uint16_t mask)
{
	switch (track->get_playback_channel_mode()) {
	case AllChannels:
		/* they are insensitive, so we don't care */
		break;

	case FilterChannels:
		for (uint16_t i = 0; i < 16; i++) {
			playback_buttons[i]->set_active ((1<<i) & mask);
		}
		break;
		
	case ForceChannel:
		/* only set the lowest set channel in the mask as active */
		for (uint16_t i = 0; i < 16; i++) {
			playback_buttons[i]->set_active (i == (PBD::ffs (mask) - 1));
		}
		break;
	}
}

void
MidiChannelSelectorWindow::set_capture_selected_channels (uint16_t mask)
{
	switch (track->get_capture_channel_mode()) {
	case AllChannels:
		/* they are insensitive, so we don't care */
		break;

	case FilterChannels:
		for (uint16_t i = 0; i < 16; i++) {
			capture_buttons[i]->set_active ((1<<i) & mask);
		}
		break;
		
	case ForceChannel:
		/* only set the lowest set channel in the mask as active */
		for (uint16_t i = 0; i < 16; i++) {
			capture_buttons[i]->set_active (i == (PBD::ffs (mask) - 1));
		}
		break;
	}
}

void
MidiChannelSelectorWindow::playback_mask_changed ()
{
	set_playback_selected_channels (track->get_playback_channel_mask());
}

void
MidiChannelSelectorWindow::capture_mask_changed ()
{
	set_capture_selected_channels (track->get_capture_channel_mask());
}

void
MidiChannelSelectorWindow::playback_mode_changed ()
{
	uint32_t first_channel = 0;
	ChannelMode mode = track->get_playback_channel_mode();

	switch (mode) {
	case AllChannels:
		if (last_drawn_playback_mode == ForceChannel) {
			/* force mode used radio buttons. not what we want,
			 * though one could argue that we want no buttons
			 * at since they are insensitive
			 */
			playback_buttons.clear ();
		}
		for (vector<Widget*>::iterator i = playback_mask_controls.begin(); i != playback_mask_controls.end(); ++i) {
			(*i)->set_sensitive (false);
		}
		playback_all_button.set_active ();
		break;

	case FilterChannels:
		if (last_drawn_playback_mode == ForceChannel) {
			playback_buttons.clear ();
		} else if (last_drawn_playback_mode == AllChannels) {
			for (vector<ToggleButton*>::iterator i = playback_buttons.begin(); i != playback_buttons.end(); ++i) {
				(*i)->set_sensitive (true);
			}
		}
		for (vector<Widget*>::iterator i = playback_mask_controls.begin(); i != playback_mask_controls.end(); ++i) {
			(*i)->set_sensitive (true);
		}
		playback_filter_button.set_active ();
		break;

	case ForceChannel:
		if (last_drawn_playback_mode == AllChannels || last_drawn_playback_mode == FilterChannels) {
			playback_buttons.clear ();
			first_channel = PBD::ffs (track->get_playback_channel_mask()) - 1;
		}
		for (vector<Widget*>::iterator i = playback_mask_controls.begin(); i != playback_mask_controls.end(); ++i) {
			(*i)->set_sensitive (false);
		}
		playback_force_button.set_active ();
		break;
	}

	if (playback_buttons.empty()) {

		Gtkmm2ext::container_clear (playback_mask_box);
		
		ToggleButton* tb;
		RadioButtonGroup group;
		
		for (uint32_t n = 0; n < 16; ++n) {
			char buf[3];
			snprintf (buf, sizeof (buf), "%d", n+1);

			switch (mode) {
			case AllChannels:
			case FilterChannels:
				tb = manage (new ToggleButton (buf));
				Gtkmm2ext::UI::instance()->set_tip (*tb, string_compose (_("Click to toggle playback of channel %1"), n+1));
				break;
			case ForceChannel:
				tb = manage (new RadioButton (group, buf));
				tb->property_draw_indicator() = false;
				if (n == first_channel) {
					tb->set_active (true);
				}
				Gtkmm2ext::UI::instance()->set_tip (*tb, string_compose (_("Click to force all MIDI channel messages to channel %1"), n+1));
				break;
			}
			playback_buttons.push_back (tb);
			tb->set_name (X_("MidiChannelSelectorButton"));
			playback_mask_box.pack_start (*tb);
			tb->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &MidiChannelSelectorWindow::playback_channel_clicked), n));
			tb->show ();

			if (mode == AllChannels) {
				tb->set_sensitive (false);
			}
		}
		
		if (mode != ForceChannel) {
			set_playback_selected_channels (track->get_playback_channel_mask());
		}
	}

	if (mode == AllChannels) {
		for (vector<ToggleButton*>::iterator i = playback_buttons.begin(); i != playback_buttons.end(); ++i) {
			(*i)->set_sensitive (false);
		}
	}

	last_drawn_playback_mode = mode;
}

void
MidiChannelSelectorWindow::capture_mode_changed ()
{
	uint32_t first_channel = 0;
	ChannelMode mode = track->get_capture_channel_mode();

	switch (mode) {
	case AllChannels:
		if (last_drawn_capture_mode == ForceChannel) {
			/* force mode used radio buttons. not what we want,
			 * though one could argue that we want no buttons
			 * at since they are insensitive
			 */
			capture_buttons.clear ();
		}
		for (vector<Widget*>::iterator i = capture_mask_controls.begin(); i != capture_mask_controls.end(); ++i) {
			(*i)->set_sensitive (false);
		}
		capture_all_button.set_active ();
		break;

	case FilterChannels:
		if (last_drawn_capture_mode == ForceChannel) {
			capture_buttons.clear ();
		} else if (last_drawn_capture_mode == AllChannels) {
			for (vector<ToggleButton*>::iterator i = capture_buttons.begin(); i != capture_buttons.end(); ++i) {
				(*i)->set_sensitive (true);
			}
		}
		for (vector<Widget*>::iterator i = capture_mask_controls.begin(); i != capture_mask_controls.end(); ++i) {
			(*i)->set_sensitive (true);
		}
		capture_filter_button.set_active ();
		break;

	case ForceChannel:
		if (last_drawn_capture_mode == AllChannels || last_drawn_capture_mode == FilterChannels) {
			capture_buttons.clear ();
			first_channel = PBD::ffs (track->get_capture_channel_mask()) - 1;
		}
		for (vector<Widget*>::iterator i = capture_mask_controls.begin(); i != capture_mask_controls.end(); ++i) {
			(*i)->set_sensitive (false);
		}
		capture_force_button.set_active ();
		break;
	}

	if (capture_buttons.empty()) {

		Gtkmm2ext::container_clear (capture_mask_box);
		
		ToggleButton* tb;
		RadioButtonGroup group;
		
		for (uint32_t n = 0; n < 16; ++n) {
			char buf[3];
			snprintf (buf, sizeof (buf), "%d", n+1);

			switch (mode) {
			case AllChannels:
			case FilterChannels:
				tb = manage (new ToggleButton (buf));
				Gtkmm2ext::UI::instance()->set_tip (*tb, string_compose (_("Click to toggle recording of channel %1"), n+1));
				break;
			case ForceChannel:
				tb = manage (new RadioButton (group, buf));
				tb->property_draw_indicator() = false;
				if (n == first_channel) {
					tb->set_active (true);
				}
				Gtkmm2ext::UI::instance()->set_tip (*tb, string_compose (_("Click to force all recorded channels to %1"), n+1));
				break;
			}
			capture_buttons.push_back (tb);
			tb->set_name (X_("MidiChannelSelectorButton"));
			capture_mask_box.pack_start (*tb);
			tb->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &MidiChannelSelectorWindow::capture_channel_clicked), n));
			tb->show ();

			if (mode == AllChannels) {
				tb->set_sensitive (false);
			}
		}
		
		if (mode != ForceChannel) {
			set_capture_selected_channels (track->get_capture_channel_mask());
		} 
	}

	if (mode == AllChannels) {
		for (vector<ToggleButton*>::iterator i = capture_buttons.begin(); i != capture_buttons.end(); ++i) {
			(*i)->set_sensitive (false);
		}
	}

	last_drawn_capture_mode = mode;
}

void
MidiChannelSelectorWindow::playback_channel_clicked (uint16_t n)
{
	if (playback_buttons[n]->get_active()) {
		switch (track->get_playback_channel_mode()) {
		case AllChannels:
			break;
		case FilterChannels:
			track->set_playback_channel_mask (track->get_playback_channel_mask() | (1<<n));
			break;
		case ForceChannel:
			track->set_playback_channel_mask (1<<n);
			break;
		}
	} else {
		if (track->get_playback_channel_mode() == FilterChannels) {
			track->set_playback_channel_mask (track->get_playback_channel_mask() & ~(1<<n));
		}
	}
}

void
MidiChannelSelectorWindow::capture_channel_clicked (uint16_t n)
{
	if (capture_buttons[n]->get_active()) {
		switch (track->get_capture_channel_mode()) {
		case AllChannels:
			break;
		case FilterChannels:
			track->set_capture_channel_mask (track->get_capture_channel_mask() | (1<<n));
			break;
		case ForceChannel:
			track->set_capture_channel_mask (1<<n);
			break;
		}
	} else {
		if (track->get_capture_channel_mode() == FilterChannels) {
			track->set_capture_channel_mask (track->get_capture_channel_mask() & ~(1<<n));
		}
	}
}

void
MidiChannelSelectorWindow::capture_mode_toggled (ChannelMode mode)
{
	/* this is called twice for every radio button change. the first time
	   is for the button/mode that has been turned off, and the second is for the
	   button/mode that has been turned on.

	   so we take action only if the button is active (i.e it is the one
	   just clicked on)
	*/
	
	switch (mode) {
	case AllChannels:
		if (capture_all_button.get_active()) {
			track->set_capture_channel_mode (AllChannels, track->get_capture_channel_mask());
		}
		break;
	case FilterChannels:
		if (capture_filter_button.get_active()) {
			track->set_capture_channel_mode (FilterChannels, track->get_capture_channel_mask());
		}
		break;
	case ForceChannel:
		if (capture_force_button.get_active()) {
			track->set_capture_channel_mode (ForceChannel, track->get_capture_channel_mask());
		}
		break;
	}
}

void
MidiChannelSelectorWindow::playback_mode_toggled (ChannelMode mode)
{
	/* this is called twice for every radio button change. the first time
	   is for the button/mode that has been turned off, and the second is for the
	   button/mode that has been turned on.

	   so we take action only if the button is active (i.e it is the one
	   just clicked on)
	*/
	
	switch (mode) {
	case AllChannels:
		if (playback_all_button.get_active()) {
			track->set_playback_channel_mode (AllChannels, track->get_playback_channel_mask());
		}
		break;
	case FilterChannels:
		if (playback_filter_button.get_active()) {
			track->set_playback_channel_mode (FilterChannels, track->get_playback_channel_mask());
		}
		break;
	case ForceChannel:
		if (playback_force_button.get_active()) {
			track->set_playback_channel_mode (ForceChannel, track->get_playback_channel_mask());
		}
		break;
	}
}

void
MidiChannelSelectorWindow::set_channel_colors (const uint32_t new_channel_colors[16])
{
	for (uint32_t n = 0; n < 16; ++n) {

		char color_normal[8];
		char color_active[8];
		
		snprintf(color_normal, 8, "#%x", UINT_INTERPOLATE(new_channel_colors[n], 0x000000ff, 0.6));
		snprintf(color_active, 8, "#%x", new_channel_colors[n]);

		playback_buttons[n]->modify_bg(STATE_NORMAL, Gdk::Color(color_normal));
		playback_buttons[n]->modify_bg(STATE_ACTIVE, Gdk::Color(color_active));

		capture_buttons[n]->modify_bg(STATE_NORMAL, Gdk::Color(color_normal));
		capture_buttons[n]->modify_bg(STATE_ACTIVE, Gdk::Color(color_active));
	}
}

void
MidiChannelSelectorWindow::set_default_channel_color()
{
	for (uint32_t n = 0; n < 16; ++n) {
		playback_buttons[n]->unset_fg (STATE_NORMAL);
		playback_buttons[n]->unset_bg (STATE_NORMAL);
		playback_buttons[n]->unset_fg (STATE_ACTIVE);
		playback_buttons[n]->unset_bg (STATE_ACTIVE);

		capture_buttons[n]->unset_fg (STATE_NORMAL);
		capture_buttons[n]->unset_bg (STATE_NORMAL);
		capture_buttons[n]->unset_fg (STATE_ACTIVE);
		capture_buttons[n]->unset_bg (STATE_ACTIVE);
	}
}
