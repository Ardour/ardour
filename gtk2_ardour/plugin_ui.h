/*
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2009 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2007-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2017 Julien "_FrnchFrgg_" RIVAUD <frnchfrgg@free.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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

#include <gtkmm/adjustment.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/expander.h>
#include <gtkmm/filechooserbutton.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/menu.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/socket.h>
#include <gtkmm/table.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/viewport.h>

#include "ardour/types.h"
#include "ardour/plugin.h"
#include "ardour/variant.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"
#include "widgets/ardour_spinner.h"

#include "ardour_window.h"
#include "automation_controller.h"
#include "pianokeyboard.h"

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

namespace ArdourWidgets {
	class FastMeter;
}

class LatencyGUI;
class ArdourWindow;
class PluginEqGui;
class PluginLoadStatsGui;
class PluginPresetsUI;
class VSTPluginUI;

class PlugUIBase : public virtual sigc::trackable, public PBD::ScopedConnectionList
{
public:
	PlugUIBase (boost::shared_ptr<ARDOUR::PluginInsert>);
	virtual ~PlugUIBase();

	virtual gint get_preferred_height () = 0;
	virtual gint get_preferred_width () = 0;
	virtual bool resizable () { return true; }
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
	virtual void grab_focus () {}
	virtual bool non_gtk_gui() const { return false; }

	sigc::signal<void,bool> KeyboardFocused;

protected:
	boost::shared_ptr<ARDOUR::PluginInsert> insert;
	boost::shared_ptr<ARDOUR::Plugin> plugin;

	void add_common_widgets (Gtk::HBox*, bool with_focus = true);

	/* UI elements that subclasses can add to their widgets */

	/** a ComboBoxText which lists presets and manages their selection */
	ArdourWidgets::ArdourDropdown _preset_combo;
	/** a label which has a * in if the current settings are different from the preset being shown */
	Gtk::Label _preset_modified;
	/** a button to add a preset */
	ArdourWidgets::ArdourButton _add_button;
	/** a button to save the current settings as a new user preset */
	ArdourWidgets::ArdourButton _save_button;
	/** a button to delete the current preset (if it is a user one) */
	ArdourWidgets::ArdourButton _delete_button;
	/** a button to show a preset browser */
	ArdourWidgets::ArdourButton _preset_browser_button;
	/** a button to delete the reset the plugin params */
	ArdourWidgets::ArdourButton _reset_button;
	/** a button to bypass the plugin */
	ArdourWidgets::ArdourButton _bypass_button;
	/** and self-explaining button :) */
	ArdourWidgets::ArdourButton _pin_management_button;
	/** a button to acquire keyboard focus */
	Gtk::EventBox _focus_button;
	/** an expander containing the plugin description */
	Gtk::Expander description_expander;
	/** an expander containing the plugin analysis graph */
	Gtk::Expander plugin_analysis_expander;
	/** an expander containing the plugin cpu profile */
	Gtk::Expander cpuload_expander;
	/** a button which, when clicked, opens the latency GUI */
	ArdourWidgets::ArdourButton _latency_button;
	/** a button which sets all controls' automation setting to Manual */
	ArdourWidgets::ArdourButton automation_manual_all_button;
	/** a button which sets all controls' automation setting to Play */
	ArdourWidgets::ArdourButton automation_play_all_button;
	/** a button which sets all controls' automation setting to Write */
	ArdourWidgets::ArdourButton automation_write_all_button;
	/** a button which sets all controls' automation setting to Touch */
	ArdourWidgets::ArdourButton automation_touch_all_button;
	/** a button which sets all controls' automation setting to Latch */
	ArdourWidgets::ArdourButton automation_latch_all_button;

	void set_latency_label ();
	LatencyGUI* latency_gui;
	ArdourWindow* latency_dialog;

	PluginEqGui* eqgui;
	PluginLoadStatsGui* stats_gui;
	PluginPresetsUI* preset_gui;
	ArdourWindow* preset_dialog;

	int _no_load_preset;

	virtual void preset_selected (ARDOUR::Plugin::PresetRecord preset);
	void add_plugin_setting ();
	void save_plugin_setting ();
	void delete_plugin_setting ();
	void reset_plugin_parameters ();
	void browse_presets ();
	void manage_pins ();
	bool focus_toggled(GdkEventButton*);
	bool bypass_button_release(GdkEventButton*);
	void toggle_description ();
	void toggle_plugin_analysis ();
	void toggle_cpuload_display ();
	void processor_active_changed (boost::weak_ptr<ARDOUR::Processor> p);
	void plugin_going_away ();
	void automation_state_changed ();
	void preset_added_or_removed ();
	void update_preset_modified ();

	bool has_descriptive_presets () const;

	PBD::ScopedConnection death_connection;
	PBD::ScopedConnection active_connection;
	PBD::ScopedConnection preset_added_connection;
	PBD::ScopedConnection preset_removed_connection;
	PBD::ScopedConnectionList control_connections;

private:
	Gtk::Image* _focus_out_image;
	Gtk::Image* _focus_in_image;
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
	Gtk::VBox settings_box;
	Gtk::HBox hpacker;
	Gtk::Menu* automation_menu;

	gint prefheight;
	bool is_scrollable;

	struct MeterInfo {
		ArdourWidgets::FastMeter* meter;
		bool packed;

		MeterInfo () {
			meter = 0;
			packed = false;
		}
	};

	/* FIXME: Unify with AutomationController */
	struct ControlUI : public Gtk::HBox {

		const Evoral::Parameter parameter() const { return param; }

		Evoral::Parameter                            param;
		boost::shared_ptr<ARDOUR::AutomationControl> control;

		/* input */

		boost::shared_ptr<ARDOUR::ScalePoints>  scale_points;
		boost::shared_ptr<AutomationController> controller;

		ArdourWidgets::ArdourButton             automate_button;
		Gtk::Label                              label;
		ArdourWidgets::ArdourDropdown*          combo;
		Gtk::FileChooserButton*                 file_button;
		ArdourWidgets::ArdourSpinner*           spin_box;

		bool                                    button;
		bool                                    update_pending;
		bool                                    ignore_change;


		/* output */

		Gtk::EventBox* display;
		Gtk::Label*    display_label;

		Gtk::HBox*     hbox;
		Gtk::VBox*     vbox;
		MeterInfo*     meterinfo;

		ControlUI (const Evoral::Parameter& param);
		~ControlUI ();

		/* layout */
		Gtk::Table* knobtable;
		int x0, x1, y0, y1;

		bool short_autostate; // modify with set_short_autostate below
	};

	void set_short_autostate(ControlUI* cui, bool value);

	std::vector<ControlUI*>   input_controls; // workaround for preset load
	std::vector<ControlUI*>   input_controls_with_automation;
	std::vector<ControlUI*>   output_controls;

	sigc::connection screen_update_connection;

	void output_update();

	void build ();
	void automatic_layout (const std::vector<ControlUI *>& control_uis);

	ControlUI* build_control_ui (const Evoral::Parameter&                     param,
	                             const ARDOUR::ParameterDescriptor&           desc,
	                             boost::shared_ptr<ARDOUR::AutomationControl> mcontrol,
	                             float                                        value,
	                             bool                                         is_input,
	                             bool                                         use_knob = false);

	void ui_parameter_changed (ControlUI* cui);
	void update_control_display (ControlUI* cui);
	void update_input_displays (); // workaround for preset load
	void control_combo_changed (ControlUI* cui, float value);

	bool astate_button_event (GdkEventButton* ev, ControlUI*);
	void automation_state_changed (ControlUI*);
	void set_automation_state (ARDOUR::AutoState state, ControlUI* cui);
	void set_all_automation (ARDOUR::AutoState state);

	void knob_size_request(Gtk::Requisition* req, ControlUI* cui);

	typedef std::map<uint32_t, Gtk::FileChooserButton*> FilePathControls;
	FilePathControls _filepath_controls;
	void set_path_property (const ARDOUR::ParameterDescriptor& desc,
	                        Gtk::FileChooserButton*            widget);
	void path_property_changed (uint32_t key, const ARDOUR::Variant& value);

	void scroller_size_request (Gtk::Requisition*);
	Gtk::ScrolledWindow scroller;

	Gtk::Expander   _plugin_pianokeyboard_expander;
	APianoKeyboard* _piano;
	Gtk::VBox       _pianobox;
	Gtk::SpinButton _piano_velocity;
	Gtk::SpinButton _piano_channel;

	void note_on_event_handler (int, int);
	void note_off_event_handler (int);

	void toggle_pianokeyboard ();
	void build_midi_table ();
	void midi_refill_patches ();
	void midi_bank_patch_change (uint8_t chn);
	void midi_bank_patch_select (uint8_t chn, uint32_t bankpgm);
	std::vector<ArdourWidgets::ArdourDropdown*> midi_pgmsel;
	PBD::ScopedConnectionList midi_connections;
	std::map<uint32_t, std::string> pgm_names;
};

class PluginUIWindow : public ArdourWindow
{
public:
	PluginUIWindow (boost::shared_ptr<ARDOUR::PluginInsert> insert,
	                bool scrollable=false,
	                bool editor=true);
	~PluginUIWindow ();

	PlugUIBase& pluginui() { return *_pluginui; }

	void resize_preferred();
	void set_parent (Gtk::Window*);
	void set_title(const std::string& title);


	bool on_key_press_event (GdkEventKey*);
	bool on_key_release_event (GdkEventKey*);
	void on_show ();
	void on_hide ();

private:
	std::string _title;
	PlugUIBase* _pluginui;
	PBD::ScopedConnection death_connection;
	Gtk::Window* parent;
	Gtk::VBox vbox;
	bool was_visible;
	bool _keyboard_focused;
#ifdef AUDIOUNIT_SUPPORT
	int pre_deactivate_x;
	int pre_deactivate_y;
#endif

	void keyboard_focused (bool yn);

	void app_activated (bool);
	void plugin_going_away ();

	bool create_windows_vst_editor (boost::shared_ptr<ARDOUR::PluginInsert>);
	bool create_lxvst_editor(boost::shared_ptr<ARDOUR::PluginInsert>);
	bool create_mac_vst_editor(boost::shared_ptr<ARDOUR::PluginInsert>);
	bool create_audiounit_editor (boost::shared_ptr<ARDOUR::PluginInsert>);
	bool create_lv2_editor (boost::shared_ptr<ARDOUR::PluginInsert>);
	bool create_vst3_editor (boost::shared_ptr<ARDOUR::PluginInsert>);

	static PluginUIWindow* the_plugin_window;
};

#ifdef MACVST_SUPPORT
/* this function has to be in a .mm file
 * because MacVSTPluginUI has Cocoa members
 */
extern VSTPluginUI* create_mac_vst_gui (boost::shared_ptr<ARDOUR::PluginInsert>);
#endif

#ifdef AUDIOUNIT_SUPPORT
/* this function has to be in a .mm file */
extern PlugUIBase* create_au_gui (boost::shared_ptr<ARDOUR::PluginInsert>, Gtk::VBox**);
#endif

#endif /* __ardour_plugin_ui_h__ */
