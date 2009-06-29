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

#include <vector>

#include <cmath>
#include <gtkmm/box.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/menu.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm2ext/dndtreeview.h>
#include <gtkmm2ext/auto_spin.h>
#include <gtkmm2ext/click_box.h>
#include <gtkmm2ext/dndtreeview.h>

#include "pbd/stateful.h"

#include "ardour/types.h"
#include "ardour/ardour.h"
#include "ardour/plugin_insert.h"
#include "ardour/port_insert.h"
#include "ardour/processor.h"
#include "ardour/route.h"

#include "pbd/fastlog.h"

#include "plugin_interest.h"
#include "io_selector.h"
#include "send_ui.h"
#include "enums.h"

class MotionController;
class PluginSelector;
class PluginUIWindow;
class RouteRedirectSelection;
class MixerStrip;

namespace ARDOUR {
	class Connection;
	class IO;
	class Insert;
	class Plugin;
	class PluginInsert;
	class PortInsert;
	class Route;
	class Send;
	class Session;
}

class ProcessorBox : public Gtk::HBox, public PluginInterestedObject
{
  public:
	ProcessorBox (ARDOUR::Placement, ARDOUR::Session&, PluginSelector &, RouteRedirectSelection &, MixerStrip* parent, bool owner_is_mixer = false);
	~ProcessorBox ();

	void set_route (boost::shared_ptr<ARDOUR::Route>);
	void set_width (Width);

	void update();

	void select_all_processors ();
	void deselect_all_processors ();
	void select_all_plugins ();
	void select_all_inserts ();
	void select_all_sends ();
	
	sigc::signal<void,boost::shared_ptr<ARDOUR::Processor> > ProcessorSelected;
	sigc::signal<void,boost::shared_ptr<ARDOUR::Processor> > ProcessorUnselected;
	
	static void register_actions();

  private:
	boost::shared_ptr<ARDOUR::Route>  _route;
	ARDOUR::Session &   _session;
	MixerStrip*         _parent_strip; // null if in RouteParamsUI
	bool                _owner_is_mixer;
	bool                 ab_direction;
	std::vector<sigc::connection> connections;

	boost::shared_ptr<ARDOUR::Processor> _processor_being_created;

	ARDOUR::Placement   _placement;

	PluginSelector     & _plugin_selector;
	RouteRedirectSelection  & _rr_selection;

	void route_going_away ();

	struct ModelColumns : public Gtk::TreeModel::ColumnRecord {
	    ModelColumns () {
		    add (text);
		    add (processor);
		    add (color);
	    }
	    Gtk::TreeModelColumn<std::string>       text;
	    Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Processor> > processor;
	    Gtk::TreeModelColumn<Gdk::Color>        color;
	};

	ModelColumns columns;
	Glib::RefPtr<Gtk::ListStore> model;
	
	void selection_changed ();

	static bool get_colors;
	static Gdk::Color* active_processor_color;
	static Gdk::Color* inactive_processor_color;
	
	Gtk::EventBox	       processor_eventbox;
	Gtk::HBox              processor_hpacker;
	Gtkmm2ext::DnDTreeView<boost::shared_ptr<ARDOUR::Processor> > processor_display;
	Gtk::ScrolledWindow    processor_scroller;

	void object_drop (const std::list<boost::shared_ptr<ARDOUR::Processor> >&, Gtk::TreeView*,
			  Glib::RefPtr<Gdk::DragContext>& context);

	Width _width;
	
	Gtk::Menu *send_action_menu;
	void build_send_action_menu ();

	void new_send ();
	void show_send_controls ();

	Gtk::Menu *processor_menu;
	gint processor_menu_map_handler (GdkEventAny *ev);
	Gtk::Menu * build_processor_menu ();
	void build_processor_tooltip (Gtk::EventBox&, std::string);
	void show_processor_menu (gint arg);

	void choose_send ();
	void send_io_finished (IOSelector::Result, boost::weak_ptr<ARDOUR::Processor>, IOSelectorWindow*);
	void choose_return ();
	void return_io_finished (IOSelector::Result, boost::weak_ptr<ARDOUR::Processor>, IOSelectorWindow*);
	void choose_insert ();
	void choose_plugin ();
	void use_plugins (const SelectedPlugins&);

	bool no_processor_redisplay;
	bool ignore_delete;

	bool processor_button_press_event (GdkEventButton *);
	bool processor_button_release_event (GdkEventButton *);
	void redisplay_processors ();
	void add_processor_to_display (boost::weak_ptr<ARDOUR::Processor>);
	void row_deleted (const Gtk::TreeModel::Path& path);
	void show_processor_active (boost::weak_ptr<ARDOUR::Processor>);
	void show_processor_name (boost::weak_ptr<ARDOUR::Processor>);
	std::string processor_name (boost::weak_ptr<ARDOUR::Processor>);

	void remove_processor_gui (boost::shared_ptr<ARDOUR::Processor>);

	void processors_reordered (const Gtk::TreeModel::Path&, const Gtk::TreeModel::iterator&, int*);
	void compute_processor_sort_keys ();
	std::vector<sigc::connection> processor_active_connections;
	std::vector<sigc::connection> processor_name_connections;
	
	bool processor_drag_in_progress;
	void processor_drag_begin (GdkDragContext*);
	void processor_drag_end (GdkDragContext*);
	void all_processors_active(bool state);
	void all_plugins_active(bool state);
	void ab_plugins ();

	void cut_processors ();
	void copy_processors ();
	void paste_processors ();
	void delete_processors ();
	void delete_dragged_processors (const std::list<boost::shared_ptr<ARDOUR::Processor> >&);
	void clear_processors ();
	void rename_processors ();

	typedef std::vector<boost::shared_ptr<ARDOUR::Processor> > ProcSelection;

	void for_selected_processors (void (ProcessorBox::*pmf)(boost::shared_ptr<ARDOUR::Processor>));
	void get_selected_processors (ProcSelection&);

	static Glib::RefPtr<Gtk::Action> paste_action;
	void paste_processor_state (const XMLNodeList&);
	
	void activate_processor (boost::shared_ptr<ARDOUR::Processor>);
	void deactivate_processor (boost::shared_ptr<ARDOUR::Processor>);
	void edit_processor (boost::shared_ptr<ARDOUR::Processor>);
	void hide_processor_editor (boost::shared_ptr<ARDOUR::Processor>);
	void rename_processor (boost::shared_ptr<ARDOUR::Processor>);

	gint idle_delete_processor (boost::weak_ptr<ARDOUR::Processor>);

	void weird_plugin_dialog (ARDOUR::Plugin& p, ARDOUR::Route::ProcessorStreams streams);

	static ProcessorBox* _current_processor_box;
	static bool enter_box (GdkEventCrossing*, ProcessorBox*);
	static bool leave_box (GdkEventCrossing*, ProcessorBox*);

	static void rb_choose_plugin ();
	static void rb_choose_insert ();
	static void rb_choose_send ();
	static void rb_choose_return ();
	static void rb_clear ();
	static void rb_cut ();
	static void rb_copy ();
	static void rb_paste ();
	static void rb_delete ();
	static void rb_rename ();
	static void rb_select_all ();
	static void rb_deselect_all ();
	static void rb_activate ();
	static void rb_deactivate ();
	static void rb_activate_all ();
	static void rb_deactivate_all ();
	static void rb_ab_plugins ();
	static void rb_edit ();
	
	void route_name_changed ();
	std::string generate_processor_title (boost::shared_ptr<ARDOUR::PluginInsert> pi);
};

#endif /* __ardour_gtk_processor_box__ */
