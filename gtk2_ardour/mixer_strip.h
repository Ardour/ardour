/*
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2006 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2007-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2017 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef __ardour_mixer_strip__
#define __ardour_mixer_strip__

#include <vector>

#include <cmath>

#include <gtkmm/adjustment.h>
#include <gtkmm/button.h>
#include <gtkmm/box.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/frame.h>
#include <gtkmm/label.h>
#include <gtkmm/menu.h>
#include <gtkmm/sizegroup.h>
#include <gtkmm/textview.h>
#include <gtkmm/togglebutton.h>

#include "pbd/stateful.h"

#include "ardour/types.h"
#include "ardour/ardour.h"
#include "ardour/processor.h"

#include "pbd/fastlog.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_knob.h"

#include "axis_view.h"
#include "control_slave_ui.h"
#include "io_button.h"
#include "route_ui.h"
#include "gain_meter.h"
#include "panner_ui.h"
#include "enums.h"
#include "processor_box.h"
#include "triggerbox_ui.h"
#include "trigger_master.h"
#include "visibility_group.h"

namespace ARDOUR {
	class Route;
	class Send;
	class Processor;
	class Session;
	class PortInsert;
	class Bundle;
	class Plugin;
	class TriggerBox;
}
namespace Gtk {
	class Window;
	class Style;
}

class Mixer_UI;
class MotionController;
class RouteGroupMenu;
class ArdourWindow;
class AutomationController;
class TriggerBoxWidget;

class MixerStrip : public AxisView, public RouteUI, public Gtk::EventBox
{
public:
	MixerStrip (Mixer_UI&, ARDOUR::Session*, boost::shared_ptr<ARDOUR::Route>, bool in_mixer = true);
	MixerStrip (Mixer_UI&, ARDOUR::Session*, bool in_mixer = true);
	~MixerStrip ();

	std::string name()  const;
	Gdk::Color color () const;
	bool marked_for_display () const;
	bool set_marked_for_display (bool);

	boost::shared_ptr<ARDOUR::Stripable> stripable() const { return RouteUI::stripable(); }

	void set_width_enum (Width, void* owner);
	Width get_width_enum () const { return _width; }
	void* width_owner () const { return _width_owner; }

	GainMeter&      gain_meter()      { return gpm; }
	PannerUI&       panner_ui()       { return panners; }
	PluginSelector* plugin_selector();

	void fast_update ();
	void set_embedded (bool);

	void set_route (boost::shared_ptr<ARDOUR::Route>);
	void set_button_names ();
	void show_send (boost::shared_ptr<ARDOUR::Send>);
	void revert_to_default_display ();

	/** @return the delivery that is being edited using our fader; it will be the
	 *  last send passed to \ref show_send() , or our route's main out delivery.
	 */
	boost::shared_ptr<ARDOUR::Delivery> current_delivery () const {
		return _current_delivery;
	}

	bool mixer_owned () const {
		return _mixer_owned;
	}

	/* used for screenshots */
	void hide_master_spacer (bool);

	void hide_things ();

	sigc::signal<void> WidthChanged;

	/** The delivery that we are handling the level for with our fader has changed */
	PBD::Signal1<void, boost::weak_ptr<ARDOUR::Delivery> > DeliveryChanged;

	static PBD::Signal1<void,MixerStrip*> CatchDeletion;

	std::string state_id() const;

	void parameter_changed (std::string);
	void route_active_changed ();

	void copy_processors ();
	void cut_processors ();
	void paste_processors ();
	void select_all_processors ();
	void deselect_all_processors ();
	bool delete_processors ();  //note: returns false if nothing was deleted
	void toggle_processors ();
	void ab_plugins ();

	void set_selected (bool yn);

	void set_trigger_display (boost::shared_ptr<ARDOUR::TriggerBox>);

	static MixerStrip* entered_mixer_strip() { return _entered_mixer_strip; }

protected:
	friend class Mixer_UI;
	void set_packed (bool yn);
	bool packed () { return _packed; }

	void set_stuff_from_route ();

private:
	Mixer_UI& _mixer;

	void init ();

	bool  _embedded;
	bool  _packed;
	bool  _mixer_owned;
	Width _width;
	void*  _width_owner;

	ArdourWidgets::ArdourButton hide_button;
	ArdourWidgets::ArdourButton width_button;
	ArdourWidgets::ArdourButton number_label;
	Gtk::HBox                   width_hide_box;
	Gtk::EventBox               spacer;

	void hide_clicked();
	bool width_button_pressed (GdkEventButton *);

	Gtk::Frame          global_frame;
	Gtk::VBox           global_vpacker;

	ProcessorBox processor_box;
	GainMeter    gpm;
	PannerUI     panners;
	TriggerBoxWidget trigger_display;

	Glib::RefPtr<Gtk::SizeGroup> button_size_group;

	Gtk::Table rec_mon_table;
	Gtk::Table solo_iso_table;
	Gtk::Table mute_solo_table;
	Gtk::Table master_volume_table;
	Gtk::Table bottom_button_table;

	void vca_assign (boost::shared_ptr<ARDOUR::VCA>);
	void vca_unassign (boost::shared_ptr<ARDOUR::VCA>);

	void meter_changed ();
	void monitor_changed ();
	void monitor_section_added_or_removed ();

	IOButton input_button;
	IOButton output_button;

	ArdourWidgets::ArdourButton* monitor_section_button;

	void comment_button_resized (Gtk::Allocation&);

	ArdourWidgets::ArdourButton midi_input_enable_button;
	Gtk::HBox input_button_box;

	std::string longest_label;

	void midi_input_status_changed ();
	bool input_active_button_press (GdkEventButton*);
	bool input_active_button_release (GdkEventButton*);

	gint    mark_update_safe ();
	guint32 mode_switch_in_progress;

	/*Trigger widget*/
	FittedCanvasWidget _tmaster_widget;
	TriggerMaster*     _tmaster;

	ArdourWidgets::ArdourButton name_button;
	ArdourWidgets::ArdourButton _comment_button;
	ArdourWidgets::ArdourKnob   trim_control;

	Gtk::Menu* _master_volume_menu;
	ArdourWidgets::ArdourButton* _loudess_analysis_button;
	boost::shared_ptr<AutomationController> _volume_controller;

	void trim_start_touch ();
	void trim_end_touch ();

	void setup_comment_button ();

	void loudess_analysis_button_clicked ();
	bool volume_controller_button_pressed (GdkEventButton*);

	ArdourWidgets::ArdourButton group_button;
	RouteGroupMenu*             group_menu;

	void io_changed_proxy ();

	Gtk::Menu *send_action_menu;
	void build_send_action_menu ();

	PBD::ScopedConnection panstate_connection;
	PBD::ScopedConnection panstyle_connection;
	void connect_to_pan ();
	void update_panner_choices ();
	void update_trim_control ();

	void update_diskstream_display ();
	void update_input_display ();
	void update_output_display ();

	void set_automated_controls_sensitivity (bool yn);

	Gtk::Menu* route_ops_menu;
	void build_route_ops_menu ();
	gboolean name_button_button_press (GdkEventButton*);
	gboolean number_button_button_press (GdkEventButton*);
	void list_route_operations ();

	bool select_route_group (GdkEventButton *);
	void route_group_changed ();

	Gtk::Style *passthru_style;

	void route_color_changed ();
	void show_passthru_color ();

	void route_property_changed (const PBD::PropertyChange&);
	void name_button_resized (Gtk::Allocation&);
	void name_changed ();
	void update_speed_display ();
	void map_frozen ();
	void hide_processor_editor (boost::weak_ptr<ARDOUR::Processor> processor);
	void hide_redirect_editors ();

	bool ignore_speed_adjustment;

	static MixerStrip* _entered_mixer_strip;

	virtual void bus_send_display_changed (boost::shared_ptr<ARDOUR::Route>);

	void set_current_delivery (boost::shared_ptr<ARDOUR::Delivery>);

	void drop_send ();
	PBD::ScopedConnection send_gone_connection;

	void reset_strip_style ();
	void update_sensitivity ();

	bool mixer_strip_enter_event ( GdkEventCrossing * );
	bool mixer_strip_leave_event ( GdkEventCrossing * );

	/** A VisibilityGroup to manage the visibility of some of our controls.
	 *  We fill it with the controls that are being managed, using the same names
	 *  as those used with _mixer_strip_visibility in RCOptionEditor.  Then
	 *  this VisibilityGroup is configured by changes to the RC variable
	 *  mixer-element-visibility, which happen when the user makes changes in
	 *  the RC option editor.
	 */
	VisibilityGroup _visibility;
	boost::optional<bool> override_solo_visibility () const;
	boost::optional<bool> override_rec_mon_visibility () const;

	PBD::ScopedConnectionList _config_connection;

	void add_input_port (ARDOUR::DataType);
	void add_output_port (ARDOUR::DataType);

	bool _suspend_menu_callbacks;
	bool level_meter_button_press (GdkEventButton *);
	void popup_level_meter_menu (GdkEventButton *);
	void add_level_meter_item_point (Gtk::Menu_Helpers::MenuList &, Gtk::RadioMenuItem::Group &, std::string const &, ARDOUR::MeterPoint);
	void add_level_meter_item_type (Gtk::Menu_Helpers::MenuList &, Gtk::RadioMenuItem::Group &, std::string const &, ARDOUR::MeterType);
	void set_meter_point (ARDOUR::MeterPoint);
	void set_meter_type (ARDOUR::MeterType);
	PBD::ScopedConnection _level_meter_connection;

	std::string meter_point_string (ARDOUR::MeterPoint);

	void update_track_number_visibility ();

	ControlSlaveUI control_slave_ui;
};

#endif /* __ardour_mixer_strip__ */
