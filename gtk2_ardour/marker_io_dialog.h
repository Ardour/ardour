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

#include <gtkmm/box.h>
#include <gtkmm/label.h>

#include "ardour/engine_state_controller.h"
#include "ardour/utils.h"

#include "waves_dialog.h"

class MidiPortDropdown;
class WavesDropdown;

class MarkerIODialog : public WavesDialog
{
    public:
        MarkerIODialog ();

    private:
        WavesDropdown& input_dropdown;
        WavesDropdown& output_dropdown;

        void populate_dropdown (WavesDropdown& dropdown, bool for_playback);

        void input_chosen (WavesDropdown*,void*);
        void output_chosen (WavesDropdown*,void*);
        void on_realize ();
        bool on_button_press_event (GdkEventButton*);
};
