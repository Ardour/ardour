/*
 * Copyright (C) 2007-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2017 Johannes Mueller <github@johannes-mueller.org>
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

#ifndef __ardour_gtk_processor_box__
#define __ardour_gtk_processor_box__

#include <cmath>
#include <vector>

#include <boost/function.hpp>

#include <gtkmm/box.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/menu.h>
#include <gtkmm/scrolledwindow.h>

#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/dndtreeview.h"
#include "gtkmm2ext/dndvbox.h"
#include "gtkmm2ext/persistent_tooltip.h"

#include "pbd/stateful.h"
#include "pbd/signals.h"

#include "ardour/types.h"
#include "ardour/ardour.h"
#include "ardour/plugin_insert.h"
#include "ardour/luaproc.h"
#include "ardour/port_insert.h"
#include "ardour/processor.h"
#include "ardour/route.h"
#include "ardour/session_handle.h"

#include "pbd/fastlog.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_fader.h"
#include "widgets/slider_controller.h"

#ifdef HAVE_BEATBOX
#include "beatbox_gui.h"
#endif
#include "plugin_interest.h"
#include "plugin_display.h"
#include "io_selector.h"
#include "send_ui.h"
#include "enums.h"
#include "window_manager.h"

class MotionController;
class PluginSelector;
class PluginUIWindow;
class ProcessorSelection;
class MixerStrip;

namespace ARDOUR {
	class Connection;
	class IO;
	class Insert;
	class Plugin;
	class PluginInsert;
	class PortInsert;
	class Route;
	class Session;
}

class ProcessorBox;

class ProcessorWindowProxy : public WM::ProxyBase
{
public:
	ProcessorWindowProxy (std::string const &, ProcessorBox *, boost::weak_ptr<ARDOUR::Processor>);
	~ProcessorWindowProxy();

	Gtk::Window* get (bool create = false);

	boost::weak_ptr<ARDOUR::Processor> processor () const {
		return _processor;
	}

	ARDOUR::SessionHandlePtr* session_handle();
	void show_the_right_window (bool show_not_toggle = false);
	void set_custom_ui_mode(bool use_custom) { want_custom = use_custom; }

	int set_state (const XMLNode&, int);
	XMLNode& get_state ();

private:
	ProcessorBox* _processor_box;
	boost::weak_ptr<ARDOUR::Processor> _processor;
	bool is_custom;
	bool want_custom;

	void processor_going_away ();
	PBD::ScopedConnection going_away_connection;
	PBD::ScopedConnectionList gui_connections;
};


class PluginPinWindowProxy : public WM::ProxyBase
{
  public:
	PluginPinWindowProxy (std::string const &, boost::weak_ptr<ARDOUR::Processor>);
	~PluginPinWindowProxy();

	Gtk::Window* get (bool create = false);
	ARDOUR::SessionHandlePtr* session_handle();

  private:
	boost::weak_ptr<ARDOUR::Processor> _processor;

	void processor_going_away ();
	PBD::ScopedConnection going_away_connection;
};



class ProcessorEntry : public Gtkmm2ext::DnDVBoxChild, public sigc::trackable
{
public:
	ProcessorEntry (ProcessorBox *, boost::shared_ptr<ARDOUR::Processor>, Width);
	~ProcessorEntry ();

	Gtk::EventBox& action_widget ();
	Gtk::Widget& widget ();
	std::string drag_text () const;
	void set_visual_state (Gtkmm2ext::VisualState, bool);

	bool is_selectable() const {return _selectable;}
	void set_selectable(bool s) { _selectable = s; }

	bool drag_data_get (Glib::RefPtr<Gdk::DragContext> const, Gtk::SelectionData &);
	bool can_copy_state (Gtkmm2ext::DnDVBoxChild*) const;

	enum ProcessorPosition {
		PreFader,
		Fader,
		PostFader
	};

	void set_position (ProcessorPosition, uint32_t);
	bool unknown_processor () const { return _unknown_processor; } ;
	boost::shared_ptr<ARDOUR::Processor> processor () const;
	void set_enum_width (Width);

	/** Hide any widgets that should be hidden */
	virtual void hide_things ();

	void toggle_inline_display_visibility ();
	void show_all_controls ();
	void hide_all_controls ();
	void add_control_state (XMLNode *) const;
	void set_control_state (XMLNode const *);
	std::string state_id () const;
	Gtk::Menu* build_controls_menu ();
	Gtk::Menu* build_send_options_menu ();

protected:
	ArdourWidgets::ArdourButton _button;
	Gtk::VBox _vbox;
	ProcessorPosition _position;
	uint32_t _position_num;
	ProcessorBox* _parent;

	virtual void setup_visuals ();

private:
	bool _selectable;
	bool _unknown_processor;
	void led_clicked(GdkEventButton *);
	void processor_active_changed ();
	void processor_property_changed (const PBD::PropertyChange&);
	void processor_configuration_changed (const ARDOUR::ChanCount in, const ARDOUR::ChanCount out);
	std::string name (Width) const;
	void setup_tooltip ();

	boost::shared_ptr<ARDOUR::Processor> _processor;
	Width _width;
	PBD::ScopedConnection active_connection;
	PBD::ScopedConnection name_connection;
	PBD::ScopedConnection config_connection;
	ARDOUR::PluginPresetPtr _plugin_preset_pointer;

	class Control : public sigc::trackable {
	public:
		Control (ProcessorEntry&, boost::shared_ptr<ARDOUR::AutomationControl>, std::string const &);
		~Control ();

		void set_visible (bool);
		void add_state (XMLNode *) const;
		void set_state (XMLNode const *);
		void hide_things ();

		bool visible () const {
			return _visible;
		}

		std::string name () const {
			return _name;
		}

		Gtk::Alignment box;

	private:
		void slider_adjusted ();
		void button_clicked ();
		void button_clicked_event (GdkEventButton *);
		void control_changed ();
		void control_automation_state_changed ();
		std::string state_id () const;
		void set_tooltip ();

		void start_touch ();
		void end_touch ();

		bool button_released (GdkEventButton*);

		ProcessorEntry& _entry;
		boost::weak_ptr<ARDOUR::AutomationControl> _control;
		/* things for a slider */
		Gtk::Adjustment _adjustment;
		ArdourWidgets::HSliderController _slider;
		Gtkmm2ext::PersistentTooltip _slider_persistant_tooltip;
		/* things for a button */
		ArdourWidgets::ArdourButton _button;
		bool _ignore_ui_adjustment;
		PBD::ScopedConnectionList _connections;
		bool _visible;
		std::string _name;
	};

	std::list<Control*> _controls;

	friend class Control;
	void toggle_control_visibility (Control *);

	void toggle_panner_link ();
	void toggle_allow_feedback ();

	class PluginInlineDisplay : public PluginDisplay {
	public:
		PluginInlineDisplay(ProcessorEntry&, boost::shared_ptr<ARDOUR::Plugin>, uint32_t max_height = 80);
		~PluginInlineDisplay() {}
	protected:
		void on_size_request (Gtk::Requisition* req);
		bool on_button_press_event (GdkEventButton *ev);
		void update_height_alloc (uint32_t inline_height);

		void display_frame (cairo_t* cr, double w, double h);

		ProcessorEntry& _entry;
		bool _scroll;
		const uint32_t _given_max_height;
	};

	class LuaPluginDisplay : public PluginInlineDisplay {
	public:
		LuaPluginDisplay(ProcessorEntry&, boost::shared_ptr<ARDOUR::LuaProc>, uint32_t max_height = 80);
		~LuaPluginDisplay();
	protected:
		virtual uint32_t render_inline (cairo_t *, uint32_t width);
	private:
		boost::shared_ptr<ARDOUR::LuaProc> _luaproc;
		LuaState lua_gui;
		luabridge::LuaRef * _lua_render_inline;
	};

	class PortIcon : public Gtk::DrawingArea {
	public:
		PortIcon(bool input);
		void set_ports(ARDOUR::ChanCount const ports) { _ports = ports; }
	private:
		bool on_expose_event (GdkEventExpose *);
		bool _input;
		ARDOUR::ChanCount _ports;
	};

	class RoutingIcon : public Gtk::DrawingArea {
	public:
		RoutingIcon(bool inputrouting = true);
		void set (
				const ARDOUR::ChanCount&,
				const ARDOUR::ChanCount&,
				const ARDOUR::ChanCount&,
				const ARDOUR::ChanCount&,
				const ARDOUR::ChanMapping&,
				const ARDOUR::ChanMapping&,
				const ARDOUR::ChanMapping&);
		void set_fed_by (
				const ARDOUR::ChanCount&,
				const ARDOUR::ChanCount&,
				const ARDOUR::ChanMapping&,
				const ARDOUR::ChanMapping&);

		void set_feeding (
				const ARDOUR::ChanCount&,
				const ARDOUR::ChanCount&,
				const ARDOUR::ChanMapping&,
				const ARDOUR::ChanMapping&);

		void set_terminal (bool b);

		void copy_state (const RoutingIcon& other) {
			_in         = other._in;
			_out        = other._out;
			_sources    = other._sources;
			_sinks      = other._sinks;
			_in_map     = other._in_map;
			_out_map    = other._out_map;
			_thru_map   = other._thru_map;
			_f_out      = other._f_out;
			_f_out_map  = other._f_out_map;
			_f_thru_map = other._f_thru_map;
			_f_sources  = other._f_sources;
			_i_in       = other._i_in;
			_i_in_map   = other._i_in_map;
			_i_thru_map = other._i_thru_map;
			_i_sinks    = other._i_sinks;
			_fed_by     = other._fed_by;
			_feeding    = other._feeding;
		}

		void unset_fed_by () { _fed_by  = false ; }
		void unset_feeding () { _feeding  = false ; }
		bool in_identity () const;
		bool out_identity () const;
		bool can_coalesce () const;

		static double pin_x_pos (uint32_t, double, uint32_t, uint32_t, bool);
		static void draw_connection (cairo_t*, double, double, double, double, bool, bool dashed = false);
		static void draw_gnd (cairo_t*, double, double, double, bool);
		static void draw_sidechain (cairo_t*, double, double, double, bool);
		static void draw_thru_src (cairo_t*, double, double, double, bool);
		static void draw_thru_sink (cairo_t*, double, double, double, bool);

	private:
		bool on_expose_event (GdkEventExpose *);
		void expose_input_map (cairo_t*, const double, const double);
		void expose_coalesced_input_map (cairo_t*, const double, const double);
		void expose_output_map (cairo_t*, const double, const double);

		ARDOUR::ChanCount   _in;
		ARDOUR::ChanCount   _out;
		ARDOUR::ChanCount   _sources;
		ARDOUR::ChanCount   _sinks;
		ARDOUR::ChanMapping _in_map;
		ARDOUR::ChanMapping _out_map;
		ARDOUR::ChanMapping _thru_map;
		ARDOUR::ChanCount   _f_out;
		ARDOUR::ChanMapping _f_out_map;
		ARDOUR::ChanMapping _f_thru_map;
		ARDOUR::ChanCount   _f_sources;
		ARDOUR::ChanCount   _i_in;
		ARDOUR::ChanMapping _i_in_map;
		ARDOUR::ChanMapping _i_thru_map;
		ARDOUR::ChanCount   _i_sinks;
		bool _fed_by;
		bool _feeding;
		bool _input;
		bool _terminal;
	};

public:
	PortIcon input_icon;
	PortIcon output_icon;
	RoutingIcon routing_icon; // sits on top of every processor (input routing)
	RoutingIcon output_routing_icon; // only used by last processor in the chain

protected:
	PluginDisplay *_plugin_display ;
};

class PluginInsertProcessorEntry : public ProcessorEntry
{
public:
	PluginInsertProcessorEntry (ProcessorBox *, boost::shared_ptr<ARDOUR::PluginInsert>, Width);

	void hide_things ();

private:
	void iomap_changed ();
	boost::shared_ptr<ARDOUR::PluginInsert> _plugin_insert;

	PBD::ScopedConnectionList _iomap_connection;
};

class ProcessorBox : public Gtk::HBox, public PluginInterestedObject, public ARDOUR::SessionHandlePtr
{
public:
	enum ProcessorOperation {
		ProcessorsCut,
		ProcessorsCopy,
		ProcessorsPaste,
		ProcessorsDelete,
		ProcessorsSelectAll,
		ProcessorsSelectNone,
		ProcessorsToggleActive,
		ProcessorsAB,
	};

	ProcessorBox (ARDOUR::Session*, boost::function<PluginSelector*()> get_plugin_selector,
	              ProcessorSelection&, MixerStrip* parent, bool owner_is_mixer = false);
	~ProcessorBox ();

	void set_route (boost::shared_ptr<ARDOUR::Route>);
	void set_width (Width);

	bool processor_operation (ProcessorOperation);

	void select_all_processors ();
	void deselect_all_processors ();
	void select_all_plugins ();
	void select_all_inserts ();
	void select_all_sends ();

	void all_visible_processors_active(bool state);
	void setup_routing_feeds ();

	void hide_things ();

	bool edit_aux_send (boost::shared_ptr<ARDOUR::Processor>);
	bool edit_triggerbox (boost::shared_ptr<ARDOUR::Processor>);

	/* Everything except a WindowProxy object should use this to get the window */
	Gtk::Window* get_processor_ui (boost::shared_ptr<ARDOUR::Processor>) const;

	/* a WindowProxy object can use this */
	Gtk::Window* get_editor_window (boost::shared_ptr<ARDOUR::Processor>, bool);
	Gtk::Window* get_generic_editor_window (boost::shared_ptr<ARDOUR::Processor>);

	void manage_pins (boost::shared_ptr<ARDOUR::Processor>);
	void edit_processor (boost::shared_ptr<ARDOUR::Processor>);
	void generic_edit_processor (boost::shared_ptr<ARDOUR::Processor>);

	void update_gui_object_state (ProcessorEntry *);

	sigc::signal<void,boost::shared_ptr<ARDOUR::Processor> > ProcessorSelected;
	sigc::signal<void,boost::shared_ptr<ARDOUR::Processor> > ProcessorUnselected;

	static Glib::RefPtr<Gtk::ActionGroup> processor_box_actions;
	static Gtkmm2ext::Bindings* bindings;
	static void register_actions();

	typedef std::vector<boost::shared_ptr<ARDOUR::Processor> > ProcSelection;

	static ProcSelection current_processor_selection ()
	{
		ProcSelection ps;
		if (_current_processor_box) {
			_current_processor_box->get_selected_processors (ps);
		}
		return ps;
	}

#ifndef NDEBUG
	static bool show_all_processors;
#endif

private:
	/* prevent copy construction */
	ProcessorBox (ProcessorBox const &);

	boost::shared_ptr<ARDOUR::Route>  _route;
	MixerStrip*         _parent_strip; // null if in RouteParamsUI
	bool                _owner_is_mixer;
	bool                 ab_direction;
	PBD::ScopedConnectionList _mixer_strip_connections;
	PBD::ScopedConnectionList _route_connections;

	boost::function<PluginSelector*()> _get_plugin_selector;

	boost::shared_ptr<ARDOUR::Processor> _processor_being_created;

	/** Index at which to place a new plugin (based on where the menu was opened), or -1 to
	 *  put at the end of the plugin list.
	 */
	int _placement;

	ProcessorSelection& _p_selection;

	static void load_bindings ();

	void route_going_away ();

	bool is_editor_mixer_strip() const;

	Gtkmm2ext::DnDVBox<ProcessorEntry> processor_display;
	Gtk::ScrolledWindow    processor_scroller;

	boost::shared_ptr<ARDOUR::Processor> find_drop_position (ProcessorEntry* position);

	void _drop_plugin_preset (Gtk::SelectionData const &, ARDOUR::Route::ProcessorList &);
	void _drop_plugin (Gtk::SelectionData const &, ARDOUR::Route::ProcessorList &);

	void plugin_drop (Gtk::SelectionData const &, ProcessorEntry* position, Glib::RefPtr<Gdk::DragContext> const & context);
	void object_drop (Gtkmm2ext::DnDVBox<ProcessorEntry> *, ProcessorEntry *, Glib::RefPtr<Gdk::DragContext> const &);

	Width _width;
	bool  _redisplay_pending;

	Gtk::Menu *processor_menu;
	gint processor_menu_map_handler (GdkEventAny *ev);
	Gtk::Menu * build_processor_menu ();
	void show_processor_menu (int);
	Gtk::Menu* build_possible_aux_menu();
	Gtk::Menu* build_possible_listener_menu();
	Gtk::Menu* build_possible_remove_listener_menu();

	void choose_aux (boost::weak_ptr<ARDOUR::Route>);
	void remove_aux (boost::weak_ptr<ARDOUR::Route>);
	void choose_send ();
	void send_io_finished (IOSelector::Result, boost::weak_ptr<ARDOUR::Processor>, IOSelectorWindow*);
	void return_io_finished (IOSelector::Result, boost::weak_ptr<ARDOUR::Processor>, IOSelectorWindow*);
	void choose_insert ();
	void choose_plugin ();
	bool use_plugins (const SelectedPlugins&);

	bool no_processor_redisplay;

	bool enter_notify (GdkEventCrossing *ev);
	bool leave_notify (GdkEventCrossing *ev);
	bool processor_button_press_event (GdkEventButton *, ProcessorEntry *);
	bool processor_button_release_event (GdkEventButton *, ProcessorEntry *);
	void redisplay_processors ();
	void add_processor_to_display (boost::weak_ptr<ARDOUR::Processor>);
	void reordered ();
	void report_failed_reorder ();
	void route_processors_changed (ARDOUR::RouteProcessorChange);
	void processor_menu_unmapped ();

	void processors_reordered (const Gtk::TreeModel::Path&, const Gtk::TreeModel::iterator&, int*);
	void compute_processor_sort_keys ();

	void ab_plugins ();

	void cut_processors (const ProcSelection&);
	void copy_processors (const ProcSelection&);
	void delete_processors (const ProcSelection&);
	void paste_processors ();
	void paste_processors (boost::shared_ptr<ARDOUR::Processor> before);

	void delete_dragged_processors (const std::list<boost::shared_ptr<ARDOUR::Processor> >&);
	void clear_processors ();
	void clear_processors (ARDOUR::Placement);
	void rename_processors ();

	void for_selected_processors (void (ProcessorBox::*pmf)(boost::shared_ptr<ARDOUR::Processor>));
	void get_selected_processors (ProcSelection&) const;

	void set_disk_io_position (ARDOUR::DiskIOPoint);
	void toggle_custom_loudness_pos ();

	bool can_cut() const;
	bool stub_processor_selected() const;

	static Glib::RefPtr<Gtk::Action> cut_action;
	static Glib::RefPtr<Gtk::Action> copy_action;
	static Glib::RefPtr<Gtk::Action> paste_action;
	static Glib::RefPtr<Gtk::Action> rename_action;
	static Glib::RefPtr<Gtk::Action> delete_action;
	static Glib::RefPtr<Gtk::Action> backspace_action;
	static Glib::RefPtr<Gtk::Action> manage_pins_action;
	static Glib::RefPtr<Gtk::Action> disk_io_action;
	static Glib::RefPtr<Gtk::Action> edit_action;
	static Glib::RefPtr<Gtk::Action> edit_generic_action;
	void paste_processor_state (const XMLNodeList&, boost::shared_ptr<ARDOUR::Processor>);

	void hide_processor_editor (boost::shared_ptr<ARDOUR::Processor>);
	void rename_processor (boost::shared_ptr<ARDOUR::Processor>);

	gint idle_delete_processor (boost::weak_ptr<ARDOUR::Processor>);

	void weird_plugin_dialog (ARDOUR::Plugin& p, ARDOUR::Route::ProcessorStreams streams);

	void setup_entry_positions ();

	static ProcessorBox* _current_processor_box;

	static void rb_choose_aux (boost::weak_ptr<ARDOUR::Route>);
	static void rb_remove_aux (boost::weak_ptr<ARDOUR::Route>);
	static void rb_choose_plugin ();
	static void rb_choose_insert ();
	static void rb_choose_send ();
	static void rb_clear ();
	static void rb_clear_pre ();
	static void rb_clear_post ();
	static void rb_cut ();
	static void rb_copy ();
	static void rb_paste ();
	static void rb_delete ();
	static void rb_rename ();
	static void rb_select_all ();
	static void rb_deselect_all ();
	static void rb_activate_all ();
	static void rb_deactivate_all ();
	static void rb_ab_plugins ();
	static void rb_manage_pins ();
	static void rb_set_disk_io_position (ARDOUR::DiskIOPoint);
	static void rb_toggle_custom_loudness_pos ();
	static void rb_edit ();
	static void rb_edit_generic ();

	void route_property_changed (const PBD::PropertyChange&);
	std::string generate_processor_title (boost::shared_ptr<ARDOUR::PluginInsert> pi);

	//typedef std::list<ProcessorWindowProxy*> ProcessorWindowProxies;
	//ProcessorWindowProxies _processor_window_info;

	ProcessorWindowProxy* find_window_proxy (boost::shared_ptr<ARDOUR::Processor>) const;

	void set_processor_ui (boost::shared_ptr<ARDOUR::Processor>, Gtk::Window *);
	void maybe_add_processor_to_ui_list (boost::weak_ptr<ARDOUR::Processor>);
	void maybe_add_processor_pin_mgr (boost::weak_ptr<ARDOUR::Processor>);

	bool one_processor_can_be_edited ();
	bool processor_can_be_edited (boost::shared_ptr<ARDOUR::Processor>);

	void mixer_strip_delivery_changed (boost::weak_ptr<ARDOUR::Delivery>);

	XMLNode* entry_gui_object_state (ProcessorEntry *);
	PBD::ScopedConnection amp_config_connection;

	static bool _ignore_rb_change;
};

#endif /* __ardour_gtk_processor_box__ */
