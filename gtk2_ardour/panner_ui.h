/*
    Copyright (C) 2004 Paul Davis

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

    $Id$
*/

#ifndef __ardour_gtk_panner_ui_h__
#define __ardour_gtk_panner_ui_h__

#include <vector>

#include <gtkmm/box.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/viewport.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/arrow.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/button.h>

#include <gtkmm2ext/slider_controller.h>
#include <gtkmm2ext/click_box.h>

#include "enums.h"

class Panner2d;

namespace ARDOUR {
	class IO;
	class Session;
}
namespace Gtkmm2ext {
	class FastMeter;
	class BarController;
}

namespace Gtk {
	class Menu;
	class Menuitem;
}

class PannerUI : public Gtk::HBox
{
  public:
	PannerUI (ARDOUR::IO&, ARDOUR::Session&);
	~PannerUI ();

	void pan_changed (void *);

	void update_pan_sensitive ();
	void update_gain_sensitive ();

	void set_width (Width);
	void setup_pan ();

	void effective_pan_display ();

	void set_meter_strip_name (string name);

  private:
	friend class MixerStrip;
	ARDOUR::IO& _io;
	ARDOUR::Session& _session;

	bool ignore_toggle;
	bool in_pan_update;

	Panner2d*   panner;

	Gtk::VBox           pan_bar_packer;
	Gtk::Adjustment	    hAdjustment;
	Gtk::Adjustment     vAdjustment;
	Gtk::Viewport       panning_viewport;
	Gtk::EventBox       panning_up;
	Gtk::Arrow          panning_up_arrow;
	Gtk::EventBox       panning_down;
	Gtk::Arrow          panning_down_arrow;
	Gtk::VBox           pan_vbox;
	Width              _width;
	gint panning_scroll_button_press_event (GdkEventButton*, int32_t dir);
	gint panning_scroll_button_release_event (GdkEventButton*, int32_t dir);
	
	Gtk::ToggleButton   panning_link_button;
	Gtk::Button         panning_link_direction_button;
	Gtk::HBox           panning_link_box;

	Gtk::Menu pan_astate_menu;
	Gtk::Menu pan_astyle_menu;

	Gtk::Button pan_automation_style_button;
	Gtk::ToggleButton pan_automation_state_button;


	gint panning_link_button_press (GdkEventButton*);
	gint panning_link_button_release (GdkEventButton*);
	void panning_link_direction_clicked ();

	vector<Gtk::Adjustment*> pan_adjustments;
	vector<Gtkmm2ext::BarController*> pan_bars;

	void pan_adjustment_changed (uint32_t which);
	void pan_value_changed (uint32_t which);
	void pan_printer (char* buf, uint32_t, Gtk::Adjustment*);
	void update_pan_bars (bool only_if_aplay);
	void update_pan_linkage ();
	void update_pan_state ();

	void panner_changed ();
	
	void hide_pans ();

	void panner_moved (int which);
	void panner_bypass_toggled ();

	gint start_pan_touch (GdkEventButton*);
	gint end_pan_touch (GdkEventButton*);

	gint pan_button_event (GdkEventButton*, uint32_t which);
	Gtk::Menu* pan_menu;
	Gtk::CheckMenuItem* bypass_menu_item;
	void build_pan_menu (uint32_t which);
	void pan_mute (uint32_t which);
	void pan_reset ();
	void pan_bypass_toggle ();

	void pan_automation_state_changed();
	void pan_automation_style_changed();
	gint pan_automation_style_button_event (GdkEventButton *);
	gint pan_automation_state_button_event (GdkEventButton *);
	sigc::connection pan_watching;

	std::string astate_string (ARDOUR::AutoState);
	std::string short_astate_string (ARDOUR::AutoState);
	std::string _astate_string (ARDOUR::AutoState, bool);

	std::string astyle_string (ARDOUR::AutoStyle);
	std::string short_astyle_string (ARDOUR::AutoStyle);
	std::string _astyle_string (ARDOUR::AutoStyle, bool);
};

#endif /* __ardour_gtk_panner_ui_h__ */

