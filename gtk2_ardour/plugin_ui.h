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
#include <gtkmm/togglebutton.h>
#include <gtkmm/socket.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/socket.h>

#include <ardour/types.h>

#include "ardour_dialog.h"
#include "latency_gui.h"
#include "automation_controller.h"

namespace ARDOUR {
	class PluginInsert;
	class Plugin;
	class VSTPlugin;
	class IOProcessor;
	class AUPlugin;
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

class PlugUIBase : public virtual sigc::trackable
{
  public:
	PlugUIBase (boost::shared_ptr<ARDOUR::PluginInsert>);
	virtual ~PlugUIBase() {}

	virtual gint get_preferred_height () = 0;
	virtual gint get_preferred_width () = 0;
	virtual bool start_updating(GdkEventAny*) = 0;
	virtual bool stop_updating(GdkEventAny*) = 0;
	
	virtual void activate () {}
	virtual void deactivate () {}

	virtual void update_presets ();

  protected:
	boost::shared_ptr<ARDOUR::PluginInsert> insert;
	boost::shared_ptr<ARDOUR::Plugin> plugin;
	Gtk::ComboBoxText preset_combo;
	Gtk::Button save_button;
	Gtk::ToggleButton bypass_button;
	LatencyGUI latency_gui;

	void setting_selected();
	void save_plugin_setting (void);
	void bypass_toggled();
	void processor_active_changed (boost::weak_ptr<ARDOUR::Processor> p);
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
		
		MeterInfo(int i) { 
			meter = 0;
			packed = false;
			min = 1.0e10;
			max = -1.0e10;
			min_unbound = false;
			max_unbound = false;
		}
	};
	
	static const int32_t initial_button_rows = 6;
	static const int32_t initial_button_cols = 1;
	static const int32_t initial_output_rows = 1;
	static const int32_t initial_output_cols = 4;

	/* FIXME: Unify with AutomationController */
	struct ControlUI : public Gtk::HBox {

		boost::shared_ptr<ARDOUR::AutomationControl> control;

		ARDOUR::Parameter parameter() { return control->parameter(); }
	    
	    /* input */
	    
	    Gtk::ComboBoxText* 	      combo;
  	    std::map<string, float>*  combo_map;
	    Gtk::ToggleButton*        button;
		boost::shared_ptr<AutomationController>  controller;
	    Gtkmm2ext::ClickBox*       clickbox;
	    Gtk::Label         label;
	    bool               logarithmic;
	    bool               update_pending;
	    char               ignore_change;
	    Gtk::Button        automate_button;
	    
	    /* output */

	    Gtk::EventBox *display;
	    Gtk::Label*    display_label;

		Gtk::HBox  *    hbox;
		Gtk::VBox  *    vbox;
	    MeterInfo  *    meterinfo;

	    ControlUI ();
	    ~ControlUI(); 
	};
	
	std::vector<ControlUI*>   output_controls;
	sigc::connection screen_update_connection;
	void output_update();
	
	void build ();
	ControlUI* build_control_ui (guint32 port_index, boost::shared_ptr<ARDOUR::AutomationControl>);
	std::vector<string> setup_scale_values(guint32 port_index, ControlUI* cui);
	void parameter_changed (ControlUI* cui);
	void update_control_display (ControlUI* cui);
	void control_port_toggled (ControlUI* cui);
	void control_combo_changed (ControlUI* cui);

	void processor_active_changed (boost::weak_ptr<ARDOUR::Processor>);

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
	PluginUIWindow (Gtk::Window*, boost::shared_ptr<ARDOUR::PluginInsert> insert, bool scrollable=false);
	~PluginUIWindow ();

	PlugUIBase& pluginui() { return *_pluginui; }

	void resize_preferred();
	void set_parent (Gtk::Window*);

	bool on_key_press_event (GdkEventKey*);
	bool on_key_release_event (GdkEventKey*);
	void on_show ();
	void on_hide ();
	void on_map ();
  private:
	PlugUIBase* _pluginui;
	Gtk::Window* parent;
	Gtk::VBox vbox;
	bool non_gtk_gui;
	bool was_visible;

	void app_activated (bool);
	void plugin_going_away ();

	bool create_vst_editor (boost::shared_ptr<ARDOUR::PluginInsert>);
	bool create_audiounit_editor (boost::shared_ptr<ARDOUR::PluginInsert>);
};

#ifdef VST_SUPPORT
class VSTPluginUI : public PlugUIBase, public Gtk::VBox
{
  public:
	VSTPluginUI (boost::shared_ptr<ARDOUR::PluginInsert>, boost::shared_ptr<ARDOUR::VSTPlugin>);
	~VSTPluginUI ();

	gint get_preferred_height ();
	gint get_preferred_width ();
	bool start_updating(GdkEventAny*) {return false;}
	bool stop_updating(GdkEventAny*) {return false;}

	int package (Gtk::Window&);

  private:
	boost::shared_ptr<ARDOUR::VSTPlugin>  vst;
	Gtk::Socket socket;
	Gtk::HBox   preset_box;
	Gtk::VBox   vpacker;
	
	bool configure_handler (GdkEventConfigure*, Gtk::Socket*);
	void save_plugin_setting ();
};
#endif // VST_SUPPORT

#ifdef HAVE_AUDIOUNITS
/* this function has to be in a .mm file */
extern PlugUIBase* create_au_gui (boost::shared_ptr<ARDOUR::PluginInsert>, Gtk::VBox**);
#endif

#endif /* __ardour_plugin_ui_h__ */
