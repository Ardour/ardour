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

*/

#ifndef __ardour_gtk_panner_ui_h__
#define __ardour_gtk_panner_ui_h__

#include <vector>

#include <gtkmm/box.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/arrow.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/button.h>

#include <gtkmm2ext/click_box.h>
#include <gtkmm2ext/slider_controller.h>

#include "ardour/session_handle.h"

#include "enums.h"

class Panner2d;
class Panner2dWindow;
class StereoPanner;
class MonoPanner;

namespace ARDOUR {
	class Session;
	class Panner;
	class PannerShell;
	class Delivery;
        class AutomationControl;
}

namespace Gtkmm2ext {
	class FastMeter;
}

namespace Gtk {
	class Menu;
	class Menuitem;
}

class PannerUI : public Gtk::HBox, public ARDOUR::SessionHandlePtr
{
  public:
	PannerUI (ARDOUR::Session*);
	~PannerUI ();

	virtual void set_panner (boost::shared_ptr<ARDOUR::PannerShell>, boost::shared_ptr<ARDOUR::Panner>);

	void panshell_changed ();

	void update_pan_sensitive ();
	void update_gain_sensitive ();

	void set_width (Width);
	void setup_pan ();

	void effective_pan_display ();

	void set_meter_strip_name (std::string name);

	void on_size_allocate (Gtk::Allocation &);

	static void setup_slider_pix ();

  private:
	friend class MixerStrip;

	boost::shared_ptr<ARDOUR::PannerShell> _panshell;
	boost::shared_ptr<ARDOUR::Panner> _panner;
	PBD::ScopedConnectionList connections;
	PBD::ScopedConnectionList _pan_control_connections;

	bool ignore_toggle;
	bool in_pan_update;
	int _current_nouts;
	int _current_nins;

	static const int pan_bar_height;

	Panner2d*       twod_panner; ///< 2D panner, or 0
	Panner2dWindow* big_window;

	Gtk::VBox           pan_bar_packer;
	Gtk::VBox           pan_vbox;
        Gtk::VBox           poswidth_box;
	Width              _width;

        StereoPanner*  _stereo_panner;
	MonoPanner*    _mono_panner;

        bool _ignore_width_change;
        bool _ignore_position_change;
        void width_adjusted ();
        void show_width ();
        void position_adjusted ();
        void show_position ();

	Gtk::Menu* pan_astate_menu;
	Gtk::Menu* pan_astyle_menu;

	Gtk::Button pan_automation_style_button;
	Gtk::ToggleButton pan_automation_state_button;

	void pan_value_changed (uint32_t which);
	void build_astate_menu ();
	void build_astyle_menu ();

	void hide_pans ();

	void panner_moved (int which);
	void panner_bypass_toggled ();

	gint start_pan_touch (GdkEventButton*);
	gint end_pan_touch (GdkEventButton*);

	bool pan_button_event (GdkEventButton*);

	Gtk::Menu* pan_menu;
	Gtk::CheckMenuItem* bypass_menu_item;
	void build_pan_menu ();
	void pan_reset ();
	void pan_bypass_toggle ();
	void pan_edit ();

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

        void start_touch (boost::weak_ptr<ARDOUR::AutomationControl>);
        void stop_touch (boost::weak_ptr<ARDOUR::AutomationControl>);
};

#endif /* __ardour_gtk_panner_ui_h__ */

