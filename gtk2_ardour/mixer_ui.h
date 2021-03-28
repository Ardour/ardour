/*
 * Copyright (C) 2005-2007 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2007-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef __ardour_mixer_ui_h__
#define __ardour_mixer_ui_h__

#include <list>

#include <gtkmm/box.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/label.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/button.h>
#include <gtkmm/frame.h>
#include <gtkmm/menu.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>
#include <gtkmm/liststore.h>

#include "pbd/stateful.h"
#include "pbd/signals.h"

#include "ardour/ardour.h"
#include "ardour/types.h"
#include "ardour/session_handle.h"
#include "ardour/plugin.h"
#include "ardour/plugin_manager.h"

#include <gtkmm2ext/bindings.h>
#include "gtkmm2ext/dndtreeview.h"
#include "gtkmm2ext/treeutils.h"

#include "widgets/pane.h"
#include "widgets/tabbable.h"

#include "axis_provider.h"
#include "enums.h"
#include "monitor_section.h"
#include "route_processor_selection.h"

namespace ARDOUR {
	class Route;
	class RouteGroup;
	class VCA;
};

class AxisView;
class FoldbackStrip;
class MixerStrip;
class PluginSelector;
class MixerGroupTabs;
class MonitorSection;
class VCAMasterStrip;

class PluginTreeStore : public Gtk::TreeStore
{
public:
	static Glib::RefPtr<PluginTreeStore> create(const Gtk::TreeModelColumnRecord& columns) {
		return Glib::RefPtr<PluginTreeStore> (new PluginTreeStore (columns));
	}

protected:
	PluginTreeStore (const Gtk::TreeModelColumnRecord& columns) : Gtk::TreeStore (columns) {}
	virtual bool row_draggable_vfunc (const Gtk::TreeModel::Path&) const { return true; }
	virtual bool row_drop_possible_vfunc (const Gtk::TreeModel::Path&, const Gtk::SelectionData&) const;
};

class Mixer_UI : public ArdourWidgets::Tabbable, public PBD::ScopedConnectionList, public ARDOUR::SessionHandlePtr, public AxisViewProvider
{
public:
	static Mixer_UI* instance();
	~Mixer_UI();

	Gtk::Window* use_own_window (bool and_fill_it);
	void show_window ();

	void set_session (ARDOUR::Session *);

	PluginSelector* plugin_selector();

	void  set_strip_width (Width, bool save = false);
	Width get_strip_width () const { return _strip_width; }

	XMLNode& get_state ();
	int set_state (const XMLNode&, int /* version */);

	void save_plugin_order_file ();

	void show_strip (MixerStrip *);
	void hide_strip (MixerStrip *);

	void maximise_mixer_space();
	void restore_mixer_space();

	MonitorSection& monitor_section() { return _monitor_section; }

	void deselect_all_strip_processors();
	void delete_processors();
	void select_none ();

	void select_next_strip ();
	void select_prev_strip ();

	void do_vca_assign (boost::shared_ptr<ARDOUR::VCA>);
	void do_vca_unassign (boost::shared_ptr<ARDOUR::VCA>);
	void show_spill (boost::shared_ptr<ARDOUR::Stripable>);
	bool showing_spill_for (boost::shared_ptr<ARDOUR::Stripable>) const;
	void fan_out (boost::weak_ptr<ARDOUR::Route>, bool to_busses, bool group);

	sigc::signal1<void,boost::shared_ptr<ARDOUR::Stripable> > show_spill_change;

	RouteProcessorSelection& selection() { return _selection; }

	void show_editor_window () const;

	void register_actions ();

	void load_bindings ();
	Gtkmm2ext::Bindings*  bindings;

	void toggle_mixer_list ();
	void showhide_mixer_list (bool yn);

	void toggle_monitor_section ();
	void showhide_monitor_section (bool);

	void toggle_foldback_strip ();
	void showhide_foldback_strip (bool);

	void toggle_vcas ();
	void showhide_vcas (bool on);

#ifdef MIXBUS
	void toggle_mixbuses ();
	void showhide_mixbusses (bool on);
#endif

	bool screenshot (std::string const&);

	void toggle_monitor_action (ARDOUR::MonitorChoice monitor_choice, bool group_override = false, bool all = false);

protected:
	void set_axis_targets_for_operation ();
	ARDOUR::AutomationControlSet selected_gaincontrols ();

private:
	Mixer_UI ();
	static Mixer_UI*     _instance;
	Gtk::VBox            _content;
	Gtk::HBox             global_hpacker;
	Gtk::VBox             global_vpacker;
	Gtk::ScrolledWindow   scroller;
	Gtk::EventBox         scroller_base;
	Gtk::HBox             scroller_hpacker;
	Gtk::VBox             mixer_scroller_vpacker;
	Gtk::VBox             list_vpacker;
	Gtk::Label            group_display_button_label;
	Gtk::ScrolledWindow   track_display_scroller;
	Gtk::ScrolledWindow   group_display_scroller;
	Gtk::ScrolledWindow   favorite_plugins_scroller;
	Gtk::VBox             group_display_vbox;
	Gtk::Frame            track_display_frame;
	Gtk::Frame            group_display_frame;
	Gtk::Frame            favorite_plugins_frame;
	Gtk::VBox             favorite_plugins_vbox;
	Gtk::HBox             favorite_plugins_search_hbox;
	Gtk::ComboBoxText     favorite_plugins_mode_combo;
	Gtk::Entry            plugin_search_entry;
	Gtk::Button           plugin_search_clear_button;
	ArdourWidgets::VPane  rhs_pane1;
	ArdourWidgets::VPane  rhs_pane2;
	ArdourWidgets::HPane  inner_pane;
	Gtk::VBox             strip_group_box;
	Gtk::HBox             strip_packer;
	Gtk::ScrolledWindow   vca_scroller;
	Gtk::HBox             vca_hpacker;
	Gtk::VBox             vca_vpacker;
	Gtk::EventBox         vca_label_bar;
	Gtk::Label            vca_label;
	Gtk::EventBox         vca_scroller_base;
	Gtk::HBox             out_packer;
	ArdourWidgets::HPane  list_hpane;

	MixerGroupTabs* _group_tabs;

	bool on_scroll_event (GdkEventScroll*);
	bool on_vca_scroll_event (GdkEventScroll*);

	std::list<MixerStrip *> strips;

	void scroller_drag_data_received (const Glib::RefPtr<Gdk::DragContext>&, int, int, const Gtk::SelectionData&, guint, guint);
	bool strip_scroller_button_event (GdkEventButton*);
	bool masters_scroller_button_release (GdkEventButton*);
	void scroll_left ();
	void scroll_right ();
	void vca_scroll_left ();
	void vca_scroll_right ();
	void toggle_midi_input_active (bool flip_others);

	void move_vca_into_view (boost::shared_ptr<ARDOUR::Stripable>);
	void move_stripable_into_view (boost::shared_ptr<ARDOUR::Stripable>);

	void add_stripables (ARDOUR::StripableList&);

	void add_routes (ARDOUR::RouteList&);
	void remove_strip (MixerStrip *);
	void remove_foldback (FoldbackStrip *);
	void add_masters (ARDOUR::VCAList&);
	void remove_master (VCAMasterStrip*);
	void new_masters_created ();

	MixerStrip* strip_by_route (boost::shared_ptr<ARDOUR::Route>) const;
	MixerStrip* strip_by_stripable (boost::shared_ptr<ARDOUR::Stripable>) const;

	AxisView* axis_view_by_stripable (boost::shared_ptr<ARDOUR::Stripable>) const;
	AxisView* axis_view_by_control (boost::shared_ptr<ARDOUR::AutomationControl>) const;

	gint start_updating ();
	gint stop_updating ();

	void session_going_away ();

	sigc::connection fast_screen_update_connection;
	void fast_update_strips ();

	void track_name_changed (MixerStrip *);

	void redisplay_track_list ();
	void spill_redisplay (boost::shared_ptr<ARDOUR::Stripable>);
	bool no_track_list_redisplay;
	bool track_display_button_press (GdkEventButton*);
	void strip_width_changed ();

	void track_list_delete (const Gtk::TreeModel::Path&);
	void track_list_reorder (const Gtk::TreeModel::Path& path, const Gtk::TreeModel::iterator& iter, int* new_order);

	void plugin_row_activated (const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column);
	bool plugin_row_button_press (GdkEventButton*);
	void popup_note_context_menu (GdkEventButton*);
	void plugin_drop (const Glib::RefPtr<Gdk::DragContext>&, const Gtk::SelectionData& data);
	bool plugin_drag_motion (Glib::RefPtr<Gdk::DragContext> const&, int, int, guint);

	enum ProcessorPosition {
		AddTop,
		AddPreFader,
		AddPostFader,
		AddBottom
	};

	void add_selected_processor (ProcessorPosition);
	void add_favorite_processor (ARDOUR::PluginPresetPtr, ProcessorPosition);
	void remove_selected_from_favorites ();
	void delete_selected_preset ();
	ARDOUR::PluginPresetPtr selected_plugin ();

	void initial_track_display ();

	void set_all_strips_visibility (bool yn);
	void set_all_audio_midi_visibility (int, bool);
	void track_visibility_changed (std::string const & path);
	void update_track_visibility ();

	void hide_all_routes ();
	void show_all_routes ();
	void show_all_audiobus ();
	void hide_all_audiobus ();
	void show_all_audiotracks();
	void hide_all_audiotracks ();
	void show_all_miditracks();
	void hide_all_miditracks ();

	bool in_group_row_change;

	void group_selected (gint row, gint col, GdkEvent *ev);
	void group_unselected (gint row, gint col, GdkEvent *ev);
	void group_display_active_clicked();
	void new_route_group ();
	void remove_selected_route_group ();
	void activate_all_route_groups ();
	void disable_all_route_groups ();
	void add_route_group (ARDOUR::RouteGroup *);
	void route_groups_changed ();
	void route_group_name_edit (const std::string&, const std::string&);
	void route_group_row_change (const Gtk::TreeModel::Path& path,const Gtk::TreeModel::iterator& iter);
	void route_group_row_deleted (Gtk::TreeModel::Path const &);

	Gtk::Menu *track_menu;
	void track_column_click (gint);
	void build_track_menu ();

	MonitorSection   _monitor_section;
	PluginSelector *_plugin_selector;
	FoldbackStrip * foldback_strip;
	bool _show_foldback_strip;

	void stripable_property_changed (const PBD::PropertyChange& what_changed, boost::weak_ptr<ARDOUR::Stripable> ws);
	void route_group_property_changed (ARDOUR::RouteGroup *, const PBD::PropertyChange &);

	/* various treeviews */

	struct StripableDisplayModelColumns : public Gtk::TreeModel::ColumnRecord {
		StripableDisplayModelColumns () {
			add (text);
			add (visible);
			add (stripable);
			add (strip);
		}
		Gtk::TreeModelColumn<bool>         visible;
		Gtk::TreeModelColumn<std::string>  text;
		Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Stripable> > stripable;
		Gtk::TreeModelColumn<AxisView*>    strip;
	};

	struct GroupDisplayModelColumns : public Gtk::TreeModel::ColumnRecord {
		GroupDisplayModelColumns() {
			add (visible);
			add (text);
			add (group);
		}
		Gtk::TreeModelColumn<bool>            visible;
		Gtk::TreeModelColumn<std::string>         text;
		Gtk::TreeModelColumn<ARDOUR::RouteGroup*> group;
	};

	struct PluginsDisplayModelColumns : public Gtk::TreeModel::ColumnRecord {
		PluginsDisplayModelColumns() {
			add (name);
			add (plugin);
		}
		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<ARDOUR::PluginPresetPtr> plugin;
	};

	ARDOUR::PluginInfoList plugin_list;

	std::list<std::string>      favorite_ui_order;
	std::map<std::string, bool> favorite_ui_state;

	StripableDisplayModelColumns stripable_columns;
	GroupDisplayModelColumns     group_columns;
	PluginsDisplayModelColumns   favorite_plugins_columns;

	Gtk::TreeView track_display;
	Gtk::TreeView group_display;
	Gtkmm2ext::DnDTreeView<ARDOUR::PluginPresetPtr> favorite_plugins_display;

	Glib::RefPtr<Gtk::ListStore> track_model;
	Glib::RefPtr<Gtk::ListStore> group_model;
	Glib::RefPtr<PluginTreeStore> favorite_plugins_model;

	bool group_display_button_press (GdkEventButton*);
	void group_display_selection_changed ();

	bool strip_button_release_event (GdkEventButton*, MixerStrip*);
	bool vca_button_release_event (GdkEventButton*, VCAMasterStrip*);

	Width _strip_width;
	double _spill_scroll_position;

	void presentation_info_changed (PBD::PropertyChange const &);
	void sync_treeview_from_presentation_info (PBD::PropertyChange const &);
	void sync_presentation_info_from_treeview ();

	bool ignore_track_reorder;
	bool ignore_plugin_refill;
	bool ignore_plugin_reorder;

	void parameter_changed (std::string const &);
	void set_route_group_activation (ARDOUR::RouteGroup *, bool);

	void setup_track_display ();
	void new_track_or_bus ();

	static const int32_t default_width = 478;
	static const int32_t default_height = 765;

	/** true if we are rebuilding the route group list, or clearing
	 * it during a session teardown.
	 */
	bool _in_group_rebuild_or_clear;
	bool _route_deletion_in_progress;

	void update_title ();
	MixerStrip* strip_by_x (int x);

	friend class MixerGroupTabs;

	void monitor_section_going_away ();
	void monitor_section_attached ();
	void monitor_section_detached ();

	enum PluginListMode {
		PLM_Favorite,
		PLM_Recent,
		PLM_TopHits
	};

	void refiller (ARDOUR::PluginInfoList& result, const ARDOUR::PluginInfoList& plugs);
	void refill_favorite_plugins ();
	void maybe_refill_favorite_plugins (PluginListMode);
	void store_current_favorite_order();
	enum PluginListMode plugin_list_mode () const;
	void plugin_list_mode_changed ();
	void plugin_search_entry_changed ();
	void plugin_search_clear_button_clicked ();
	void favorite_plugins_deleted (const Gtk::TreeModel::Path&);
	void sync_treeview_from_favorite_order ();
	void sync_treeview_favorite_ui_state (const Gtk::TreeModel::Path&, const Gtk::TreeModel::iterator&);
	void save_favorite_ui_state (const Gtk::TreeModel::iterator& iter, const Gtk::TreeModel::Path& path);

	/// true if we are in fullscreen mode
	bool _maximised;

	bool _strip_selection_change_without_scroll;

	mutable boost::weak_ptr<ARDOUR::Stripable> spilled_strip;

	void escape ();

	RouteProcessorSelection _selection;
	AxisViewSelection _axis_targets;

	void spill_nothing ();
	PBD::ScopedConnection _spill_gone_connection;

	void vca_assign (boost::shared_ptr<ARDOUR::VCA>);
	void vca_unassign (boost::shared_ptr<ARDOUR::VCA>);

	template<class T> void control_action (boost::shared_ptr<T> (ARDOUR::Stripable::*get_control)() const);
	void solo_action ();
	void mute_action ();
	void rec_enable_action ();
	void step_gain_up_action ();
	void step_gain_down_action ();
	void unity_gain_action ();

	void copy_processors ();
	void cut_processors ();
	void paste_processors ();
	void select_all_processors ();
	void toggle_processors ();
	void ab_plugins ();
};

#endif /* __ardour_mixer_ui_h__ */
