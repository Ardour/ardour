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

#ifndef __ardour_gtk_processor_box__
#define __ardour_gtk_processor_box__

#include <cmath>
#include <vector>

#include <boost/function.hpp>

#include <gtkmm/box.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/menu.h>
#include <gtkmm/scrolledwindow.h>
#include "gtkmm2ext/dndtreeview.h"
#include "gtkmm2ext/auto_spin.h"
#include "gtkmm2ext/click_box.h"
#include "gtkmm2ext/dndvbox.h"
#include "gtkmm2ext/pixfader.h"
#include "gtkmm2ext/persistent_tooltip.h"

#include "pbd/stateful.h"
#include "pbd/signals.h"

#include "ardour/types.h"
#include "ardour/ardour.h"
#include "ardour/plugin_insert.h"
#include "ardour/port_insert.h"
#include "ardour/processor.h"
#include "ardour/route.h"
#include "ardour/session_handle.h"

#include "pbd/fastlog.h"

#include "plugin_interest.h"
#include "io_selector.h"
#include "send_ui.h"
#include "enums.h"
#include "ardour_button.h"
#include "window_manager.h"

class MotionController;
class PluginSelector;
class PluginUIWindow;
class RouteProcessorSelection;
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
    void toggle();
    void set_custom_ui_mode(bool use_custom) { want_custom = use_custom; }

    bool marked;
    bool valid () const;

    void set_state (const XMLNode&);
    XMLNode& get_state () const;

  private:
    ProcessorBox* _processor_box;
    boost::weak_ptr<ARDOUR::Processor> _processor;
    bool is_custom;
    bool want_custom;
    bool _valid;

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

	enum Position {
		PreFader,
		Fader,
		PostFader
	};

	void set_position (Position, uint32_t);
	boost::shared_ptr<ARDOUR::Processor> processor () const;
	void set_enum_width (Width);

	/** Hide any widgets that should be hidden */
	virtual void hide_things ();

	void show_all_controls ();
	void hide_all_controls ();
	void add_control_state (XMLNode *) const;
	void set_control_state (XMLNode const *);
	std::string state_id () const;
	Gtk::Menu* build_controls_menu ();

protected:
	ArdourButton _button;
	Gtk::VBox _vbox;
	Position _position;
	uint32_t _position_num;

	virtual void setup_visuals ();

private:
	void led_clicked();
	void processor_active_changed ();
	void processor_property_changed (const PBD::PropertyChange&);
	void processor_configuration_changed (const ARDOUR::ChanCount in, const ARDOUR::ChanCount out);
	std::string name (Width) const;
	void setup_tooltip ();

	ProcessorBox* _parent;
	boost::shared_ptr<ARDOUR::Processor> _processor;
	Width _width;
	Gtk::StateType _visual_state;
	PBD::ScopedConnection active_connection;
	PBD::ScopedConnection name_connection;
	PBD::ScopedConnection config_connection;

	class Control : public sigc::trackable {
	public:
		Control (boost::shared_ptr<ARDOUR::AutomationControl>, std::string const &);

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
		void control_changed ();
		std::string state_id () const;
		void set_tooltip ();

		boost::weak_ptr<ARDOUR::AutomationControl> _control;
		/* things for a slider */
		Gtk::Adjustment _adjustment;
		Gtkmm2ext::HSliderController _slider;
		Gtkmm2ext::PersistentTooltip _slider_persistant_tooltip;
		/* things for a button */
		ArdourButton _button;
		bool _ignore_ui_adjustment;
		PBD::ScopedConnection _connection;
		bool _visible;
		std::string _name;
	};

	std::list<Control*> _controls;

	void toggle_control_visibility (Control *);

	class PortIcon : public Gtk::DrawingArea {
	public:
		PortIcon(bool input) {
			_input = input;
			_ports = ARDOUR::ChanCount(ARDOUR::DataType::AUDIO, 1);
			set_size_request (-1, 2);
		}
		void set_ports(ARDOUR::ChanCount const ports) { _ports = ports; }
	private:
		bool on_expose_event (GdkEventExpose *);
		bool _input;
		ARDOUR::ChanCount _ports;
	};

	class RoutingIcon : public Gtk::DrawingArea {
	public:
		RoutingIcon() {
			_sources = ARDOUR::ChanCount(ARDOUR::DataType::AUDIO, 1);
			_sinks = ARDOUR::ChanCount(ARDOUR::DataType::AUDIO, 1);
			_splitting = false;
			set_size_request (-1, 4);
		}
		void set_sources(ARDOUR::ChanCount const sources) { _sources = sources; }
		void set_sinks(ARDOUR::ChanCount const sinks) { _sinks = sinks; }
		void set_splitting(const bool splitting) { _splitting = splitting; }
	private:
		bool on_expose_event (GdkEventExpose *);
		ARDOUR::ChanCount _sources; // signals available to feed into the processor(s)
		ARDOUR::ChanCount _sinks;   // combined number of outputs of the processor
		bool _splitting;
	};

protected:
	RoutingIcon _routing_icon;
	PortIcon _input_icon;
	PortIcon _output_icon;
};

class PluginInsertProcessorEntry : public ProcessorEntry
{
public:
	PluginInsertProcessorEntry (ProcessorBox *, boost::shared_ptr<ARDOUR::PluginInsert>, Width);

	void hide_things ();

private:
	void plugin_insert_splitting_changed ();
	boost::shared_ptr<ARDOUR::PluginInsert> _plugin_insert;

	PBD::ScopedConnection _splitting_connection;
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
		ProcessorsToggleActive,
		ProcessorsAB,
	};

	ProcessorBox (ARDOUR::Session*, boost::function<PluginSelector*()> get_plugin_selector,
		      RouteProcessorSelection&, MixerStrip* parent, bool owner_is_mixer = false);
	~ProcessorBox ();

	void set_route (boost::shared_ptr<ARDOUR::Route>);
	void set_width (Width);

	void processor_operation (ProcessorOperation);

	void select_all_processors ();
	void deselect_all_processors ();
	void select_all_plugins ();
	void select_all_inserts ();
	void select_all_sends ();

	void hide_things ();

	bool edit_aux_send(boost::shared_ptr<ARDOUR::Processor>);

        /* Everything except a WindowProxy object should use this to get the window */
	Gtk::Window* get_processor_ui (boost::shared_ptr<ARDOUR::Processor>) const;

        /* a WindowProxy object can use this */
        Gtk::Window* get_editor_window (boost::shared_ptr<ARDOUR::Processor>, bool);
        Gtk::Window* get_generic_editor_window (boost::shared_ptr<ARDOUR::Processor>);

        void edit_processor (boost::shared_ptr<ARDOUR::Processor>);
        void generic_edit_processor (boost::shared_ptr<ARDOUR::Processor>);

	void update_gui_object_state (ProcessorEntry *);
	
	sigc::signal<void,boost::shared_ptr<ARDOUR::Processor> > ProcessorSelected;
	sigc::signal<void,boost::shared_ptr<ARDOUR::Processor> > ProcessorUnselected;

	static void register_actions();

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
	uint32_t                  _visible_prefader_processors;

	RouteProcessorSelection& _rr_selection;

	void route_going_away ();

        bool is_editor_mixer_strip() const;

	Gtkmm2ext::DnDVBox<ProcessorEntry> processor_display;
	Gtk::ScrolledWindow    processor_scroller;

	void object_drop (Gtkmm2ext::DnDVBox<ProcessorEntry> *, ProcessorEntry *, Glib::RefPtr<Gdk::DragContext> const &);

	Width _width;
        bool  _redisplay_pending;

	Gtk::Menu *processor_menu;
	gint processor_menu_map_handler (GdkEventAny *ev);
	Gtk::Menu * build_processor_menu ();
	void show_processor_menu (int);
	Gtk::Menu* build_possible_aux_menu();

	void choose_aux (boost::weak_ptr<ARDOUR::Route>);
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
	void help_count_visible_prefader_processors (boost::weak_ptr<ARDOUR::Processor>, uint32_t*, bool*);
	void reordered ();
	void report_failed_reorder ();
	void route_processors_changed (ARDOUR::RouteProcessorChange);
	void processor_menu_unmapped ();

	void processors_reordered (const Gtk::TreeModel::Path&, const Gtk::TreeModel::iterator&, int*);
	void compute_processor_sort_keys ();

	void all_visible_processors_active(bool state);
	void ab_plugins ();

	typedef std::vector<boost::shared_ptr<ARDOUR::Processor> > ProcSelection;

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

	bool can_cut() const;

	static Glib::RefPtr<Gtk::Action> cut_action;
	static Glib::RefPtr<Gtk::Action> paste_action;
	static Glib::RefPtr<Gtk::Action> rename_action;
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
	static void rb_edit ();
	static void rb_edit_generic ();

	void route_property_changed (const PBD::PropertyChange&);
	std::string generate_processor_title (boost::shared_ptr<ARDOUR::PluginInsert> pi);

        typedef std::list<ProcessorWindowProxy*> ProcessorWindowProxies;
        ProcessorWindowProxies _processor_window_info;

        ProcessorWindowProxy* find_window_proxy (boost::shared_ptr<ARDOUR::Processor>) const;

	void set_processor_ui (boost::shared_ptr<ARDOUR::Processor>, Gtk::Window *);
	void maybe_add_processor_to_ui_list (boost::weak_ptr<ARDOUR::Processor>);

	bool one_processor_can_be_edited ();
	bool processor_can_be_edited (boost::shared_ptr<ARDOUR::Processor>);

	void mixer_strip_delivery_changed (boost::weak_ptr<ARDOUR::Delivery>);

	XMLNode* entry_gui_object_state (ProcessorEntry *);
	PBD::ScopedConnection amp_config_connection;
};

#endif /* __ardour_gtk_processor_box__ */
