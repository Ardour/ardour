/*
    Copyright (C) 2000 Paul Davis

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

#ifndef __ardour_mixer_strip__
#define __ardour_mixer_strip__

#include <vector>

#include <cmath>
#include <gtkmm.h>
#include <gtkmm2ext/auto_spin.h>
#include <gtkmm2ext/slider_controller.h>
#include <gtkmm2ext/click_box.h>

#include <ardour/types.h>
#include <ardour/ardour.h>
#include <ardour/io.h>
#include <ardour/insert.h>
#include <ardour/stateful.h>
#include <ardour/redirect.h>

#include <pbd/fastlog.h>

#include "route_ui.h"
#include "io_selector.h"
#include "gain_meter.h"
#include "panner_ui.h"
#include "enums.h"
#include "redirect_box.h"

class MotionController;


namespace Gtkmm2ext {
	class SliderController;
}

namespace ARDOUR {
	class Route;
	class Send;
	class Insert;
	class Session;
	class PortInsert;
	class Connection;
	class Plugin;
}

class Mixer_UI;

class MixerStrip : public RouteUI, public Gtk::EventBox
{
  public:
	MixerStrip (Mixer_UI&, ARDOUR::Session&, ARDOUR::Route &, bool in_mixer = true);
	~MixerStrip ();

	void set_width (Width);
	Width get_width() const { return _width; }

	void update ();
	void fast_update ();
	void set_embedded (bool);

  protected:
	friend class Mixer_UI;
	void set_packed (bool yn);
	bool packed () { return _packed; }

	void set_selected(bool yn);
	void set_stuff_from_route ();

  private:
	Mixer_UI& _mixer;

	bool  _embedded;
	bool  _packed;
	Width _width;

	Gtk::Button         hide_button;
	Gtk::Button         width_button;
	Gtk::HBox           width_hide_box;

	void hide_clicked();
	void width_clicked ();

	Gtk::Frame          global_frame;
	Gtk::VBox           global_vpacker;

	RedirectBox pre_redirect_box;
	RedirectBox post_redirect_box;
	GainMeter   gpm;
	PannerUI    panners;
	
	Gtk::Table button_table;

	Gtk::Button diskstream_button;
	Gtk::Label  diskstream_label;

	Gtk::Button input_button;
	Gtk::Label  input_label;
	Gtk::Button output_button;
	Gtk::Label  output_label;

	Gtk::Button gain_automation_style_button;
	Gtk::ToggleButton gain_automation_state_button;

	Gtk::Button pan_automation_style_button;
	Gtk::ToggleButton pan_automation_state_button;

	Gtk::Menu gain_astate_menu;
	Gtk::Menu gain_astyle_menu;
	Gtk::Menu pan_astate_menu;
	Gtk::Menu pan_astyle_menu;

	Gtk::ToggleButton polarity_button;

	sigc::connection newplug_connection;
    
	gint    mark_update_safe ();
	guint32 mode_switch_in_progress;
	
	Gtk::Button   name_button;

	Gtk::Window*  comment_window;
	Gtk::TextView comment_area;
	Gtk::Button   comment_button;

	void setup_comment_editor ();
	void comment_button_clicked ();

	Gtk::Button   group_button;
	Gtk::Label    group_label;
	Gtk::Menu    *group_menu;

	gint input_press (GdkEventButton *);
	gint output_press (GdkEventButton *);

	Gtk::Menu  input_menu;
	void add_connection_to_input_menu (ARDOUR::Connection *);

	Gtk::Menu output_menu;
	void add_connection_to_output_menu (ARDOUR::Connection *);
	
	void stream_input_chosen (ARDOUR::DiskStream*);
	void select_stream_input ();
	void connection_input_chosen (ARDOUR::Connection *);
	void connection_output_chosen (ARDOUR::Connection *);

	void edit_input_configuration ();
	void edit_output_configuration ();

	void diskstream_changed (void *src);

	Gtk::Menu *send_action_menu;
	void build_send_action_menu ();

	void new_send ();
	void show_send_controls ();


	gint gain_automation_style_button_event (GdkEventButton *);
	gint gain_automation_state_button_event (GdkEventButton *);
	gint pan_automation_style_button_event (GdkEventButton *);
	gint pan_automation_state_button_event (GdkEventButton *);

	void input_changed (ARDOUR::IOChange, void *);
	void output_changed (ARDOUR::IOChange, void *);
	void gain_automation_state_changed();
	void pan_automation_state_changed();
	void gain_automation_style_changed();
	void pan_automation_style_changed();

	sigc::connection panstate_connection;
	sigc::connection panstyle_connection;
	void connect_to_pan ();

	std::string astate_string (ARDOUR::AutoState);
	std::string short_astate_string (ARDOUR::AutoState);
	std::string _astate_string (ARDOUR::AutoState, bool);

	std::string astyle_string (ARDOUR::AutoStyle);
	std::string short_astyle_string (ARDOUR::AutoStyle);
	std::string _astyle_string (ARDOUR::AutoStyle, bool);

	void update_diskstream_display ();
	void update_input_display ();
	void update_output_display ();

	void set_automated_controls_sensitivity (bool yn);

	Gtk::Menu *route_ops_menu;
	void build_route_ops_menu ();
	gint name_button_button_release (GdkEventButton*);
	void list_route_operations ();

	gint comment_key_release_handler (GdkEventKey*);
	void comment_changed (void *src);
	void comment_edited ();
	bool ignore_comment_edit;

	void set_mix_group (ARDOUR::RouteGroup *);
	void add_mix_group_to_menu (ARDOUR::RouteGroup *);
	gint select_mix_group (GdkEventButton *);
	void mix_group_changed (void *);

	void polarity_toggled ();

	IOSelectorWindow *input_selector;
	IOSelectorWindow *output_selector;

	Gtk::Style *passthru_style;

	void route_gui_changed (string, void*);
	void show_route_color ();
	void show_passthru_color ();

	void route_active_changed ();

	/* speed control (for tracks only) */

	Gtk::Adjustment    speed_adjustment;
	Gtkmm2ext::ClickBox speed_spinner;
	Gtk::Label         speed_label;
	Gtk::Frame         speed_frame;

	void speed_adjustment_changed ();
	void speed_changed ();
	void name_changed (void *src);
	void update_speed_display ();
	void map_frozen ();
	void hide_redirect_editor (ARDOUR::Redirect* redirect);

	sigc::connection gain_watching;
	sigc::connection pan_watching;
	bool ignore_speed_adjustment;

	string solo_button_name () const { return "MixerSoloButton"; }
	string safe_solo_button_name () const { return "MixerSafeSoloButton"; }
};

#endif /* __ardour_mixer_strip__ */
