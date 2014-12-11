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
#include <string>

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

class IOSelectorWindow;
class MotionController;
class RouteGroupMenu;
class ArdourWindow;

class MixerStrip : public RouteUI
{
  public:
	MixerStrip (ARDOUR::Session*, boost::shared_ptr<ARDOUR::Route>, const std::string& layout_script_file, size_t max_name_size = 0);
	MixerStrip (ARDOUR::Session*, const std::string& layout_script_file, size_t max_name_size = 0);
	~MixerStrip ();

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

    void route_rec_enable_changed();
	void route_color_changed ();
    
  protected:
	void set_packed (bool yn);
	bool packed () { return _packed; }

	void set_selected(bool yn);
	void set_stuff_from_route ();
    
    PublicEditor& _editor;

    PBD::ScopedConnectionList _input_output_channels_update;
    
  private:

	void init ();

	bool  _embedded;
	bool  _packed;
	bool  _mixer_owned;
    
    size_t _max_name_size;

	Gtk::EventBox&       panners_home;
	//ProcessorBox processor_box;
	GainMeter    gpm;
	PannerUI     panners;

	void meter_changed ();

	Gtk::Box& gain_meter_home;
	WavesButton &midi_input_enable_button;

	void midi_input_status_changed ();
	bool input_active_button_press (GdkEventButton*);
	bool input_active_button_release (GdkEventButton*);

	gint    mark_update_safe ();
	guint32 mode_switch_in_progress;

    Gtk::Container& _name_button_home;
	bool controls_ebox_button_press (GdkEventButton*);
    bool controls_ebox_button_release (GdkEventButton* ev);
    void selection_click (GdkEventButton* ev);
	
    WavesButton&   name_button;
    void on_record_state_changed ();
    
    Gtk::Entry& _name_entry;
    Gtk::EventBox& _name_entry_eventbox;
    void begin_name_edit ();
    void end_name_edit (int);
    
    bool name_entry_key_release (GdkEventKey *ev);
	bool name_entry_key_press (GdkEventKey *ev);
 	bool name_entry_focus_out (GdkEventFocus *ev);
    void name_entry_changed ();
    
	ArdourWindow*  comment_window;
	Gtk::TextView* comment_area;
	WavesButton&   _comment_button;

	void comment_editor_done_editing ();
	void setup_comment_editor ();
	void open_comment_editor ();
	void toggle_comment_editor (WavesButton*);
	void setup_comment_button ();

	WavesButton&   group_button;
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
	void io_changed_proxy ();

	Gtk::Menu *send_action_menu;
	Gtk::MenuItem* rename_menu_item;
	void build_send_action_menu ();

	void new_send ();
	void show_send_controls ();

	PBD::ScopedConnection panstate_connection;
	PBD::ScopedConnection panstyle_connection;
	void connect_to_pan ();
	void update_panner_choices ();

	void update_diskstream_display ();
	void update_input_display ();
	void update_output_display ();

	void set_automated_controls_sensitivity (bool yn);

	gboolean name_button_button_press (GdkEventButton*);

	gint comment_key_release_handler (GdkEventKey*);
	void comment_changed (void *src);
	void comment_edited ();
	bool ignore_comment_edit;

	bool select_route_group (GdkEventButton *);
	void route_group_changed ();

	IOSelectorWindow *input_selector;
	IOSelectorWindow *output_selector;

	Gtk::Style *passthru_style;

	void show_passthru_color ();

	void property_changed (const PBD::PropertyChange&);
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

//	void update_io_button (boost::shared_ptr<ARDOUR::Route> route, Width width, bool input_button);
	void update_io_button (boost::shared_ptr<ARDOUR::Route> route, bool input_button);
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
	void add_level_meter_item_point (Gtk::Menu_Helpers::MenuList &, Gtk::RadioMenuItem::Group &, std::string const &, ARDOUR::MeterPoint);
	void add_level_meter_item_type (Gtk::Menu_Helpers::MenuList &, Gtk::RadioMenuItem::Group &, std::string const &, ARDOUR::MeterType);
	void set_meter_point (ARDOUR::MeterPoint);
	void set_meter_type (ARDOUR::MeterType);
	PBD::ScopedConnection _level_meter_connection;

	std::string meter_point_string (ARDOUR::MeterPoint);
    
    bool deletion_in_progress;
};

#endif /* __ardour_mixer_strip__ */

// How to see comment_area on the screen?
