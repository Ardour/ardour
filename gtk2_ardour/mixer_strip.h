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

#ifndef __ardour_mixer_strip__
#define __ardour_mixer_strip__

#include <vector>

#include <cmath>

#include <gtkmm/eventbox.h>
#include <gtkmm/button.h>
#include <gtkmm/box.h>
#include <gtkmm/frame.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/menu.h>
#include <gtkmm/textview.h>
#include <gtkmm/adjustment.h>

#include "gtkmm2ext/auto_spin.h"
#include "gtkmm2ext/click_box.h"
#include "gtkmm2ext/bindable_button.h"
#include "gtkmm2ext/stateful_button.h"

#include "pbd/stateful.h"

#include "ardour/types.h"
#include "ardour/ardour.h"
#include "ardour/processor.h"

#include "pbd/fastlog.h"

#include "route_ui.h"
#include "gain_meter.h"
#include "panner_ui.h"
#include "enums.h"
#include "processor_box.h"
#include "visibility_group.h"

namespace ARDOUR {
	class Route;
	class Send;
	class Processor;
	class Session;
	class PortInsert;
	class Bundle;
	class Plugin;
}
namespace Gtk {
	class Window;
	class Style;
}

class Mixer_UI;
class IOSelectorWindow;
class MotionController;
class RouteGroupMenu;
class ArdourWindow;

class MixerStrip : public RouteUI, public Gtk::EventBox
{
  public:
	MixerStrip (Mixer_UI&, ARDOUR::Session*, boost::shared_ptr<ARDOUR::Route>, bool in_mixer = true);
	MixerStrip (Mixer_UI&, ARDOUR::Session*, bool in_mixer = true);
	~MixerStrip ();

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
	 *  last send passed to ::show_send, or our route's main out delivery.
	 */
	boost::shared_ptr<ARDOUR::Delivery> current_delivery () const {
		return _current_delivery;
	}

	bool mixer_owned () const {
		return _mixer_owned;
	}

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
	void delete_processors ();
	void toggle_processors ();
	void ab_plugins ();

  protected:
	friend class Mixer_UI;
	void set_packed (bool yn);
	bool packed () { return _packed; }

	void set_selected(bool yn);
	void set_stuff_from_route ();

  private:
	Mixer_UI& _mixer;

	void init ();

	bool  _embedded;
	bool  _packed;
	bool  _mixer_owned;
	Width _width;
	void*  _width_owner;

	ArdourButton         hide_button;
	ArdourButton         width_button;
	Gtk::HBox           width_hide_box;
	Gtk::VBox           whvbox;
	Gtk::EventBox       top_event_box;
	Gtk::EventBox*      spacer;

	void hide_clicked();
	bool width_button_pressed (GdkEventButton *);

	Gtk::Frame          global_frame;
	Gtk::VBox           global_vpacker;

	ProcessorBox processor_box;
	GainMeter    gpm;
	PannerUI     panners;

	Glib::RefPtr<Gtk::SizeGroup> button_size_group;

	Gtk::Table button_table;
	Gtk::Table rec_solo_table;
	Gtk::Table top_button_table;
	Gtk::Table middle_button_table;
	Gtk::Table bottom_button_table;

	ArdourButton                 meter_point_button;

	void meter_changed ();

	ArdourButton input_button;
	ArdourButton output_button;

	ArdourButton* midi_input_enable_button;
	Gtk::HBox   input_button_box;

	std::string longest_label;

	void midi_input_status_changed ();
	bool input_active_button_press (GdkEventButton*);
	bool input_active_button_release (GdkEventButton*);

	gint    mark_update_safe ();
	guint32 mode_switch_in_progress;

	ArdourButton   name_button;

	ArdourWindow*  comment_window;
	Gtk::TextView* comment_area;
	ArdourButton   _comment_button;

	void comment_editor_done_editing ();
	void setup_comment_editor ();
	void open_comment_editor ();
	void toggle_comment_editor ();
	void setup_comment_button ();

	ArdourButton   group_button;
	RouteGroupMenu *group_menu;

	gint input_press (GdkEventButton *);
	gint output_press (GdkEventButton *);

	Gtk::Menu input_menu;
	std::list<boost::shared_ptr<ARDOUR::Bundle> > input_menu_bundles;
	void maybe_add_bundle_to_input_menu (boost::shared_ptr<ARDOUR::Bundle>, ARDOUR::BundleList const &);

	Gtk::Menu output_menu;
	std::list<boost::shared_ptr<ARDOUR::Bundle> > output_menu_bundles;
	void maybe_add_bundle_to_output_menu (boost::shared_ptr<ARDOUR::Bundle>, ARDOUR::BundleList const &);

	void bundle_input_chosen (boost::shared_ptr<ARDOUR::Bundle>);
	void bundle_output_chosen (boost::shared_ptr<ARDOUR::Bundle>);

	void edit_input_configuration ();
	void edit_output_configuration ();

	void diskstream_changed ();

	Gtk::Menu *send_action_menu;
	Gtk::MenuItem* rename_menu_item;
	void build_send_action_menu ();

	void new_send ();
	void show_send_controls ();

	PBD::ScopedConnection panstate_connection;
	PBD::ScopedConnection panstyle_connection;
	void connect_to_pan ();

	void update_diskstream_display ();
	void update_input_display ();
	void update_output_display ();

	void set_automated_controls_sensitivity (bool yn);

	Gtk::Menu* route_ops_menu;
	void build_route_ops_menu ();
	gboolean name_button_button_press (GdkEventButton*);
	void list_route_operations ();

	gint comment_key_release_handler (GdkEventKey*);
	void comment_changed (void *src);
	void comment_edited ();
	bool ignore_comment_edit;

	bool select_route_group (GdkEventButton *);
	void route_group_changed ();

	IOSelectorWindow *input_selector;
	IOSelectorWindow *output_selector;

	Gtk::Style *passthru_style;

	void route_color_changed ();
	void show_passthru_color ();

	void property_changed (const PBD::PropertyChange&);
	void name_button_resized (Gtk::Allocation&);
	void name_changed ();
	void update_speed_display ();
	void map_frozen ();
	void hide_processor_editor (boost::weak_ptr<ARDOUR::Processor> processor);
	void hide_redirect_editors ();

	bool ignore_speed_adjustment;

	void engine_running();
	void engine_stopped();

	virtual void bus_send_display_changed (boost::shared_ptr<ARDOUR::Route>);

	void set_current_delivery (boost::shared_ptr<ARDOUR::Delivery>);
	boost::shared_ptr<ARDOUR::Delivery> _current_delivery;

	void drop_send ();
	PBD::ScopedConnection send_gone_connection;

	void reset_strip_style ();

	static int scrollbar_height;

	void update_io_button (boost::shared_ptr<ARDOUR::Route> route, Width width, bool input_button);
	void port_connected_or_disconnected (boost::weak_ptr<ARDOUR::Port>, boost::weak_ptr<ARDOUR::Port>);

	/** A VisibilityGroup to manage the visibility of some of our controls.
	 *  We fill it with the controls that are being managed, using the same names
	 *  as those used with _mixer_strip_visibility in RCOptionEditor.  Then
	 *  this VisibilityGroup is configured by changes to the RC variable
	 *  mixer-strip-visibility, which happen when the user makes changes in
	 *  the RC option editor.
	 */
	VisibilityGroup _visibility;
	boost::optional<bool> override_solo_visibility () const;

	PBD::ScopedConnection _config_connection;

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
};

#endif /* __ardour_mixer_strip__ */
