/*
    Copyright (C) 2014 Paul Davis

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

#include "ardour/session.h"
#include "ardour/midi_port.h"

#include "ardour_ui.h"
#include "marker_io_dialog.h"

#include "i18n.h"

MarkerIODialog::MarkerIODialog ()
        : WavesDialog ("marker_io_dialog.xml", true, false)
        , input_dropdown (get_waves_dropdown ("input_dropdown"))
        , output_dropdown (get_waves_dropdown ("output_dropdown"))
{
        add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
        populate_dropdown (input_dropdown, false);
        populate_dropdown (output_dropdown, true);

        input_dropdown.signal_menu_item_clicked.connect (sigc::mem_fun (*this, &MarkerIODialog::input_chosen));
        output_dropdown.signal_menu_item_clicked.connect (sigc::mem_fun (*this, &MarkerIODialog::output_chosen));
}

void
MarkerIODialog::on_realize ()
{
        WavesDialog::on_realize ();
        /* remove all borders, buttons, titles, etc */
        get_window()->set_decorations (Gdk::WMDecoration (0));
}

bool
MarkerIODialog::on_button_press_event (GdkEventButton*)
{
        /* button press anywhere except the dropdowns means "close dialog" */
        hide ();

        return true;
}

void
MarkerIODialog::output_chosen (WavesDropdown*, void* full_name_of_chosen_port)
{
        if (!_session) {
                return;
        }
        _session->scene_out()->disconnect_all ();

        if (full_name_of_chosen_port != 0) {
                _session->scene_out()->connect ((char *) full_name_of_chosen_port);
        }

        hide ();
}

void
MarkerIODialog::input_chosen (WavesDropdown*, void* full_name_of_chosen_port)
{
        if (!_session) {
                return;
        }
        _session->scene_in()->disconnect_all ();

        if (full_name_of_chosen_port != 0) {
                _session->scene_in()->connect ((char*) full_name_of_chosen_port);
        }

        hide ();
}

void
MarkerIODialog::populate_dropdown (WavesDropdown& dropdown, bool for_playback)
{
        using namespace ARDOUR;

        std::vector<EngineStateController::PortState> midi_states;
        static const char* midi_port_name_prefix = "system_midi:";
        const char* midi_type_suffix;
        bool have_first = false;

        if (for_playback) {
                EngineStateController::instance()->get_physical_midi_output_states(midi_states);
                midi_type_suffix = X_(" playback");
        } else {
                EngineStateController::instance()->get_physical_midi_input_states(midi_states);
                midi_type_suffix = X_(" capture");
        }

        dropdown.clear_items ();

        /* add a "none" entry */

        dropdown.add_menu_item (_("Off"), 0);
        
        std::vector<EngineStateController::PortState>::const_iterator state_iter;

        for (state_iter = midi_states.begin(); state_iter != midi_states.end(); ++state_iter) {

                // strip the device name from input port name

                std::string device_name;

                ARDOUR::remove_pattern_from_string(state_iter->name, midi_port_name_prefix, device_name);
                ARDOUR::remove_pattern_from_string(device_name, midi_type_suffix, device_name);

                if (state_iter->active) {
                        dropdown.add_menu_item (device_name, strdup (state_iter->name.c_str()));
                        if (!have_first) {
                                dropdown.set_text (device_name);
                                have_first = true;
                        }
                }
        }
}        

