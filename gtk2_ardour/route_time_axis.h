/*
 * Copyright (C) 2006-2008 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2008-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2015 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_route_time_axis_h__
#define __ardour_route_time_axis_h__

#include <list>
#include <set>

#include <gtkmm/table.h>
#include <gtkmm/button.h>
#include <gtkmm/box.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/radiomenuitem.h>
#include <gtkmm/checkmenuitem.h>
#include <gtkmm/adjustment.h>

#include "widgets/ardour_button.h"

#include "ardour/playlist.h"
#include "ardour/types.h"

#include "ardour_dialog.h"
#include "route_ui.h"
#include "enums.h"
#include "stripable_time_axis.h"
#include "gain_meter.h"

namespace ARDOUR {
	class Session;
	class Region;
	class RouteGroup;
	class IOProcessor;
	class Processor;
	class Location;
	class Playlist;
}

namespace ArdourCanvas {
	class Rectangle;
}

class PublicEditor;
class RegionView;
class StreamView;
class Selection;
class RegionSelection;
class Selectable;
class AutomationTimeAxisView;
class AutomationLine;
class ProcessorAutomationLine;
class TimeSelection;
class RouteGroupMenu;
class ItemCounts;

class RouteTimeAxisView : public RouteUI, public StripableTimeAxisView
{
public:
	RouteTimeAxisView (PublicEditor&, ARDOUR::Session*, ArdourCanvas::Canvas& canvas);
	virtual ~RouteTimeAxisView ();

	std::string name()  const;
	Gdk::Color color () const;
	bool marked_for_display () const;
	bool set_marked_for_display (bool);

	boost::shared_ptr<ARDOUR::Stripable> stripable() const { return RouteUI::stripable(); }

	void set_route (boost::shared_ptr<ARDOUR::Route>);

	void show_selection (TimeSelection&);
	void set_button_names ();

	void set_samples_per_pixel (double);
	void set_height (uint32_t h, TrackHeightMode m = OnlySelf);
	void show_timestretch (Temporal::timepos_t const & start, Temporal::timepos_t const & end, int layers, int layer);
	void hide_timestretch ();
	void selection_click (GdkEventButton*);
	void set_selected_points (PointSelection&);
	void set_selected_regionviews (RegionSelection&);
	void get_selectables (Temporal::timepos_t const &, Temporal::timepos_t const &, double top, double bot, std::list<Selectable *>&, bool within = false);
	void get_inverted_selectables (Selection&, std::list<Selectable*>&);
	void get_regionviews_at_or_after (Temporal::timepos_t const &, RegionSelection&);
	void set_layer_display (LayerDisplay d);
	void toggle_layer_display ();
	LayerDisplay layer_display () const;

	boost::shared_ptr<ARDOUR::Region> find_next_region (ARDOUR::timepos_t const & pos, ARDOUR::RegionPoint, int32_t dir);
	ARDOUR::timepos_t find_next_region_boundary (ARDOUR::timepos_t const & pos, int32_t dir);

	/* Editing operations */
	void cut_copy_clear (Selection&, Editing::CutCopyOp);
	bool paste (Temporal::timepos_t const &, const Selection&, PasteContext& ctx);
	RegionView* combine_regions ();
	void uncombine_regions ();
	void uncombine_region (RegionView*);
	void toggle_automation_track (const Evoral::Parameter& param);
	void fade_range (TimeSelection&);

	void add_underlay (StreamView*, bool update_xml = true);
	void remove_underlay (StreamView*);
	void build_underlay_menu(Gtk::Menu*);

	int set_state (const XMLNode&, int version);

	virtual Gtk::CheckMenuItem* automation_child_menu_item (Evoral::Parameter);
	virtual boost::shared_ptr<AutomationTimeAxisView> automation_child(Evoral::Parameter param, PBD::ID ctrl_id = PBD::ID(0));

	StreamView*         view() const { return _view; }
	ARDOUR::RouteGroup* route_group() const;
	boost::shared_ptr<ARDOUR::Playlist> playlist() const;

	void fast_update ();
	void hide_meter ();
	void show_meter ();
	void reset_meter ();
	void clear_meter ();
	void io_changed (ARDOUR::IOChange, void *);
	void chan_count_changed ();
	void meter_changed ();
	void effective_gain_display () { gm.effective_gain_display(); }

	static sigc::signal<void, bool> signal_ctrl_touched;

	std::string state_id() const;

	void show_all_automation (bool apply_to_selection = false);
	void show_existing_automation (bool apply_to_selection = false);
	void hide_all_automation (bool apply_to_selection = false);

protected:
	friend class StreamView;

	struct ProcessorAutomationNode {
		Evoral::Parameter                         what;
		Gtk::CheckMenuItem*                       menu_item;
		boost::shared_ptr<AutomationTimeAxisView> view;
		RouteTimeAxisView&                        parent;

		ProcessorAutomationNode (Evoral::Parameter w, Gtk::CheckMenuItem* mitem, RouteTimeAxisView& p)
		    : what (w), menu_item (mitem), parent (p) {}

		~ProcessorAutomationNode ();
	};

	struct ProcessorAutomationInfo {
		boost::shared_ptr<ARDOUR::Processor> processor;
		bool                                  valid;
		Gtk::Menu*                            menu;
		std::vector<ProcessorAutomationNode*> lines;

		ProcessorAutomationInfo (boost::shared_ptr<ARDOUR::Processor> i)
		    : processor (i), valid (true), menu (0) {}

		~ProcessorAutomationInfo ();
	};


	void update_diskstream_display ();

	bool route_group_click  (GdkEventButton *);

	virtual void processors_changed (ARDOUR::RouteProcessorChange);

	virtual void add_processor_to_subplugin_menu (boost::weak_ptr<ARDOUR::Processor>);
	void remove_processor_automation_node (ProcessorAutomationNode* pan);

	void processor_menu_item_toggled (RouteTimeAxisView::ProcessorAutomationInfo*,
	                                 RouteTimeAxisView::ProcessorAutomationNode*);

	void processor_automation_track_hidden (ProcessorAutomationNode*,
	                                       boost::shared_ptr<ARDOUR::Processor>);


	ProcessorAutomationNode*
	find_processor_automation_node (boost::shared_ptr<ARDOUR::Processor> i, Evoral::Parameter);

	/* O(log(N)) lookup of menu-item by AC */
	Gtk::CheckMenuItem*
	find_menu_item_by_ctrl (boost::shared_ptr<ARDOUR::AutomationControl>);

	/* O(1) IFF route_owned_only == true, O(N) otherwise */
	boost::shared_ptr<AutomationTimeAxisView>
	find_atav_by_ctrl (boost::shared_ptr<ARDOUR::AutomationControl>, bool route_owned_only = true);

	boost::shared_ptr<AutomationLine>
	find_processor_automation_curve (boost::shared_ptr<ARDOUR::Processor> i, Evoral::Parameter);

	void add_processor_automation_curve (boost::shared_ptr<ARDOUR::Processor> r, Evoral::Parameter);
	void add_existing_processor_automation_curves (boost::weak_ptr<ARDOUR::Processor>);

	boost::shared_ptr<AutomationLine> automation_child_by_alist_id (PBD::ID);

	void reset_processor_automation_curves ();

	void take_name_changed (void *src);
	void route_property_changed (const PBD::PropertyChange&);
	void route_active_changed ();
	bool name_entry_changed (std::string const&);

	virtual void toggle_channel_selector () {}

	void blink_rec_display (bool onoff);

	virtual void label_view ();

	virtual void build_automation_action_menu (bool);
	virtual void append_extra_display_menu_items () {}
	void         build_display_menu ();

	void set_align_choice (Gtk::RadioMenuItem*, ARDOUR::AlignChoice, bool apply_to_selection = false);

	bool         playlist_click (GdkEventButton *);
	void         playlist_changed ();

	bool         automation_click (GdkEventButton *);

	void timestretch (samplepos_t start, samplepos_t end);
	void speed_changed ();
	void map_frozen ();
	void color_handler ();
	void region_view_added (RegionView*);
	void create_gain_automation_child (const Evoral::Parameter &, bool);
	void create_trim_automation_child (const Evoral::Parameter &, bool);
	void create_mute_automation_child (const Evoral::Parameter &, bool);
	void setup_processor_menu_and_curves ();
	void route_color_changed ();
	bool can_edit_name() const;

	StreamView*           _view;

	Gtk::HBox   other_button_hbox;
	Gtk::Table  button_table;
	ArdourWidgets::ArdourButton route_group_button;
	ArdourWidgets::ArdourButton playlist_button;
	ArdourWidgets::ArdourButton automation_button;
	ArdourWidgets::ArdourButton number_label;

	Gtk::Menu           subplugin_menu;
	Gtk::Menu*          automation_action_menu;
	Gtk::MenuItem*      plugins_submenu_item;
	RouteGroupMenu*     route_group_menu;
	Gtk::MenuItem*      overlaid_menu_item;
	Gtk::MenuItem*      stacked_menu_item;

	ArdourCanvas::Rectangle* timestretch_rect;

	/** Information about all automatable processor parameters that apply to
	 *  this route.  The Amp processor is not included in this list.
	 */
	std::list<ProcessorAutomationInfo*> processor_automation;

	std::map<boost::shared_ptr<PBD::Controllable>, Gtk::CheckMenuItem*> ctrl_item_map;

	typedef std::vector<boost::shared_ptr<AutomationLine> > ProcessorAutomationCurves;
	ProcessorAutomationCurves processor_automation_curves;
	/** parameter -> menu item map for the plugin automation menu */
	ParameterMenuMap _subplugin_menu_map;

	void post_construct ();

	GainMeterBase gm;

	XMLNode* underlay_xml_node;
	bool set_underlay_state();

	typedef std::list<StreamView*> UnderlayList;
	UnderlayList _underlay_streams;
	typedef std::list<RouteTimeAxisView*> UnderlayMirrorList;
	UnderlayMirrorList _underlay_mirrors;

	bool _ignore_set_layer_display;
	void layer_display_menu_change (Gtk::MenuItem* item);

protected:
	void update_pan_track_visibility ();

	/** Ensure that we have the appropriate automation lanes for panners.
	 *
	 *  @param show true to show any new views that we create, otherwise false.
	 */
	void ensure_pan_views (bool show = true);

	std::list<boost::shared_ptr<AutomationTimeAxisView> > pan_tracks;
	Gtk::CheckMenuItem* pan_automation_item;

private:

	void remove_child (boost::shared_ptr<TimeAxisView>);
	void update_playlist_tip ();
	void parameter_changed (std::string const & p);
	void update_track_number_visibility();
	void show_touched_automation (boost::weak_ptr<PBD::Controllable>);
	void maybe_hide_automation (bool, boost::weak_ptr<PBD::Controllable>);

	void drop_instrument_ref ();
	void reread_midnam ();
	PBD::ScopedConnectionList midnam_connection;

	PBD::ScopedConnection ctrl_touched_connection;
	sigc::connection      ctrl_autohide_connection;
};

#endif /* __ardour_route_time_axis_h__ */
