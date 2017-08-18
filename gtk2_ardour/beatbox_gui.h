/*
    Copyright (C) 2017 Paul Davis

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

#ifndef __gtk2_ardour_beatbox_gui_h__
#define __gtk2_ardour_beatbox_gui_h__

#include <boost/shared_ptr.hpp>

#include <gtkmm/radiobutton.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/button.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/box.h>

#include "ardour_dialog.h"

namespace ARDOUR {
class BeatBox;
}

class BBGUI : public ArdourDialog {
  public:
	BBGUI (boost::shared_ptr<ARDOUR::BeatBox> bb);
	~BBGUI ();

  private:
	boost::shared_ptr<ARDOUR::BeatBox> bbox;

	Gtk::RadioButtonGroup quantize_group;
	Gtk::RadioButton quantize_off;
	Gtk::RadioButton quantize_32nd;
	Gtk::RadioButton quantize_16th;
	Gtk::RadioButton quantize_8th;
	Gtk::RadioButton quantize_quarter;
	Gtk::RadioButton quantize_half;
	Gtk::RadioButton quantize_whole;

	Gtk::ToggleButton play_button;
	Gtk::Button clear_button;

	Gtk::Adjustment tempo_adjustment;
	Gtk::SpinButton tempo_spinner;

	Gtk::VBox quantize_button_box;
	Gtk::HBox misc_button_box;


	void set_quantize (int divisor);
	void toggle_play ();
	void clear ();
	void tempo_changed ();
};

#endif /* __gtk2_ardour_beatbox_gui_h__ */
