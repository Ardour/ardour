/*
    Copyright (C) 2016 Paul Davis

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

#ifndef __ardour_vca_master_strip__
#define __ardour_vca_master_strip__

#include <boost/shared_ptr.hpp>

#include <gtkmm/box.h>

#include "ardour_button.h"
#include "axis_view.h"
#include "gain_meter.h"

namespace ARDOUR {
	class GainControl;
	class VCA;
}

class VCAMasterStrip : public AxisView, public Gtk::EventBox
{
      public:
	VCAMasterStrip (ARDOUR::Session*, boost::shared_ptr<ARDOUR::VCA>);

	std::string name() const;
	std::string state_id() const { return "VCAMasterStrip"; }
	boost::shared_ptr<ARDOUR::VCA> vca() const { return _vca; }

      private:
	boost::shared_ptr<ARDOUR::VCA> _vca;
	Gtk::HBox    vertical_padding;
	ArdourButton name_button;
	ArdourButton active_button;
	GainMeter    gain_meter;

	Gtk::Frame   global_frame;
	Gtk::VBox    global_vpacker;
	Gtk::HBox    top_padding;
	Gtk::HBox    bottom_padding;
	Gtk::HBox    width_hide_box;
	Gtk::HBox    solo_mute_box;
	ArdourButton width_button;
	ArdourButton color_button;
	ArdourButton hide_button;
	ArdourButton number_label;
	ArdourButton solo_button;
	ArdourButton mute_button;
	ArdourButton assign_button;
	bool         wide;
	PBD::ScopedConnectionList vca_connections;

	void hide_clicked();
	bool width_button_pressed (GdkEventButton *);
	void set_selected (bool);
	bool solo_release (GdkEventButton*);
	bool mute_release (GdkEventButton*);
	void set_width (bool wide);
	void set_solo_text ();
	void solo_changed ();
	void mute_changed ();
};


#endif /* __ardour_vca_master_strip__ */
