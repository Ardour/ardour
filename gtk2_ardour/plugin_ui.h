/*
    Copyright (C) 2000-2006 Paul Davis

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

#ifndef __ardour_plugin_ui_h__
#define __ardour_plugin_ui_h__

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <vector>
#include <map>
#include <list>

#include <sigc++/signal.h>

#include <gtkmm/button.h>
#include <gtkmm/box.h>
#include <gtkmm/table.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/viewport.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/label.h>
#include <gtkmm/menu.h>
#include <gtkmm/image.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/socket.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/socket.h>

#include "ardour/types.h"
#include "ardour/plugin.h"

#include "automation_controller.h"
#include "ardour_button.h"

namespace ARDOUR {
	class PluginInsert;
	class Plugin;
	class WindowsVSTPlugin;
	class LXVSTPlugin;
	class IOProcessor;
	class AUPlugin;
	class Processor;
}

namespace PBD {
	class Controllable;
}

namespace Gtkmm2ext {
	class HSliderController;
	class BarController;
	class ClickBox;
	class FastMeter;
	class PixmapButton;
}

class LatencyGUI;
class ArdourWindow;
class PluginEqGui;

class PlugUIBase : public virtual sigc::trackable, public PBD::ScopedConnectionList
{
  public:
	PlugUIBase (boost::shared_ptr<ARDOUR::PluginInsert>);
	virtual ~PlugUIBase();

	virtual gint get_preferred_height () = 0;
	virtual gint get_preferred_width () = 0;
	virtual bool start_updating(GdkEventAny*) = 0;
	virtual bool stop_updating(GdkEventAny*) = 0;

	virtual void activate () {}
	virtual void deactivate () {}

	void update_preset_list ();
	void update_preset ();

	void latency_button_clicked ();

	virtual bool on_window_show(const std::string& /*title*/) { return true; }
	virtual void on_window_hide() {}

	virtual void forward_key_event (GdkEventKey*) {}
        virtual bool non_gtk_gui() const { return false; }

	sigc::signal<void,bool> KeyboardFocused;

  protected:
	boost::shared_ptr<ARDOUR::PluginInsert> insert;
	boost::shared_ptr<ARDOUR::Plugin> plugin;

	/* UI elements that can subclasses can add to their widgets */

	/** a ComboBoxText which lists presets and manages their selection */
	Gtk::ComboBoxText _preset_combo;
	/** a label which has a * in if the current settings are different from the preset being shown */
	Gtk::Label _preset_modified;
	/** a button to add a preset */
	Gtk::Button add_button;
	/** a button to save the current settings as a new user preset */
	Gtk::Button save_button;
	/** a button to delete the current preset (if it is a user one) */
	Gtk::Button delete_button;
	/** a button to bypass the plugin */
	ArdourButton bypass_button;
	/** a button to acquire keyboard focus */
	Gtk::EventBox focus_button;
	/** an expander containing the plugin analysis graph */
	Gtk::Expander plugin_analysis_expander;
	/** a label indicating the plugin latency */
	Gtk::Label latency_label;
	/** a button which, when clicked, opens the latency GUI */
	Gtk::Button latency_button;
	
	void set_latency_label ();

	LatencyGUI* latency_gui;
	ArdourWindow* latency_dialog;

	PluginEqGui* eqgui;
	Gtk::Requisition pre_eq_size;

	Gtk::Image* focus_out_image;
	Gtk::Image* focus_in_image;
	int _no_load_preset;

	virtual void preset_selected ();
	void add_plugin_setting ();
	void save_plugin_setting ();
	void delete_plugin_setting ();
	bool focus_toggled(GdkEventButton*);
	bool bypass_button_release(GdkEventButton*);
	void toggle_plugin_analysis ();
	void processor_active_changed (boost::weak_ptr<ARDOUR::Processor> p);
	void plugin_going_away ();
	virtual void parameter_changed (uint32_t, float);
	void preset_added_or_removed ();
	void update_preset_modified ();

	PBD::ScopedConnection death_connection;
	PBD::ScopedConnection active_connection;
	PBD::ScopedConnection preset_added_connection;
	PBD::ScopedConnection preset_removed_connection;
	PBD::ScopedConnectionList control_connections;
};

class GenericPluginUI : public PlugUIBase, public Gtk::VBox
{
  public:
	GenericPluginUI (boost::shared_ptr<ARDOUR::PluginInsert> plug, bool scrollable=false);
	~GenericPluginUI ();

	gint get_preferred_height () { return prefheight; }
	gint get_preferred_width () { return -1; }

	bool start_updating(GdkEventAny*);
	bool stop_updating(GdkEventAny*);

  private:
	Gtk::VBox main_contents;

	Gtk::HBox settings_box;
	Gtk::HBox hpacker;

	Gtk::Table button_table;
	Gtk::Table output_table;

	Gtk::ScrolledWindow scroller;
	Gtk::Adjustment hAdjustment;
	Gtk::Adjustment vAdjustment;
	Gtk::Viewport scroller_view;
	Gtk::Menu* automation_menu;

	gint prefheight;
	bool is_scrollable;

	struct MeterInfo {
		Gtkmm2ext::FastMeter *meter;

		float           min;
		float           max;
		bool            min_unbound;
		bool            max_unbound;
		bool packed;

		MeterInfo (int /*i*/) {
			meter = 0;
			packed = false;
			min = 1.0e10;
			max = -1.0e10;
			min_unbound = false;
			max_unbound = false;
		}
	};

	static const int32_t initial_button_rows = 12;
	static const int32_t initial_button_cols = 1;
	static const int32_t initial_output_rows = 1;
	static const int32_t initial_output_cols = 4;

	/* FIXME: Unify with AutomationController */
	struct ControlUI : public Gtk::HBox {

		uint32_t port_index;
		boost::shared_ptr<ARDOUR::AutomationControl> control;

		Evoral::Parameter parameter() { return control->parameter(); }

		/* input */

		Gtk::ComboBoxText*                      combo;
		boost::shared_ptr<ARDOUR::Plugin::ScalePoints> scale_points;
		Gtk::ToggleButton*                      button;
		boost::shared_ptr<AutomationController> controller;
		Gtkmm2ext::ClickBox*                    clickbox;
		Gtk::Label                              label;
		bool                                    update_pending;
		char                                    ignore_change;
		Gtk::Button                             automate_button;

		/* output */

		Gtk::EventBox* display;
		Gtk::Label*    display_label;

		Gtk::HBox*     hbox;
		Gtk::VBox*     vbox;
		MeterInfo*     meterinfo;

		ControlUI ();
		~ControlUI ();
	};

	std::vector<ControlUI*>   input_controls;
	std::vector<ControlUI*>   output_controls;
	sigc::connection screen_update_connection;
	void output_update();

	void build ();
	ControlUI* build_control_ui (guint32 port_index, boost::shared_ptr<ARDOUR::AutomationControl>);
	void ui_parameter_changed (ControlUI* cui);
	void toggle_parameter_changed (ControlUI* cui);
	void update_control_display (ControlUI* cui);
	void control_port_toggled (ControlUI* cui);
	void control_combo_changed (ControlUI* cui);

	void astate_clicked (ControlUI*, uint32_t parameter);
	void automation_state_changed (ControlUI*);
	void set_automation_state (ARDOUR::AutoState state, ControlUI* cui);
	void start_touch (ControlUI*);
	void stop_touch (ControlUI*);

	void print_parameter (char *buf, uint32_t len, uint32_t param);
};

class PluginUIWindow : public Gtk::Window
{
  public:
	PluginUIWindow (Gtk::Window*,
	                boost::shared_ptr<ARDOUR::PluginInsert> insert,
	                bool scrollable=false,
	                bool editor=true);
	~PluginUIWindow ();

	PlugUIBase& pluginui() { return *_pluginui; }

	void resize_preferred();
	void set_parent (Gtk::Window*);
	void set_title(const std::string& title);


	bool on_enter_notify_event (GdkEventCrossing*);
	bool on_leave_notify_event (GdkEventCrossing*);
	bool on_focus_in_event (GdkEventFocus*);
	bool on_focus_out_event (GdkEventFocus*);
	bool on_key_press_event (GdkEventKey*);
	bool on_key_release_event (GdkEventKey*);
	void on_show ();
	void on_hide ();
	void on_map ();

  private:
	std::string _title;
	PlugUIBase* _pluginui;
	PBD::ScopedConnection death_connection;
	Gtk::Window* parent;
	Gtk::VBox vbox;
	bool was_visible;
	bool _keyboard_focused;
	void keyboard_focused (bool yn);

	void app_activated (bool);
	void plugin_going_away ();

	bool create_windows_vst_editor (boost::shared_ptr<ARDOUR::PluginInsert>);
	bool create_lxvst_editor(boost::shared_ptr<ARDOUR::PluginInsert>);
	bool create_audiounit_editor (boost::shared_ptr<ARDOUR::PluginInsert>);
	bool create_lv2_editor (boost::shared_ptr<ARDOUR::PluginInsert>);
};

#ifdef AUDIOUNIT_SUPPORT
/* this function has to be in a .mm file */
extern PlugUIBase* create_au_gui (boost::shared_ptr<ARDOUR::PluginInsert>, Gtk::VBox**);
#endif

#endif /* __ardour_plugin_ui_h__ */
