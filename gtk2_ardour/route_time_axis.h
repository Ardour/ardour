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
	void show_timestretch (samplepos_t start, samplepos_t end, int layers, int layer);
	void hide_timestretch ();
	void selection_click (GdkEventButton*);
	void set_selected_points (PointSelection&);
	void set_selected_regionviews (RegionSelection&);
	void get_selectables (ARDOUR::samplepos_t start, ARDOUR::samplepos_t end, double top, double bot, std::list<Selectable *>&, bool within = false);
	void get_inverted_selectables (Selection&, std::list<Selectable*>&);
	void set_layer_display (LayerDisplay d, bool apply_to_selection = false);
	LayerDisplay layer_display () const;

	boost::shared_ptr<ARDOUR::Region> find_next_region (samplepos_t pos, ARDOUR::RegionPoint, int32_t dir);
	samplepos_t find_next_region_boundary (samplepos_t pos, int32_t dir);

	/* Editing operations */
	void cut_copy_clear (Selection&, Editing::CutCopyOp);
	bool paste (ARDOUR::samplepos_t, const Selection&, PasteContext& ctx, const int32_t sub_num);
	RegionView* combine_regions ();
	void uncombine_regions ();
	void uncombine_region (RegionView*);
	void toggle_automation_track (const Evoral::Parameter& param);
	void fade_range (TimeSelection&);

	/* The editor calls these when mapping an operation across multiple tracks */
	void use_new_playlist (bool prompt, std::vector<boost::shared_ptr<ARDOUR::Playlist> > const &, bool copy);
	void clear_playlist ();

	/* group playlist name resolving */
	std::string resolve_new_group_playlist_name(std::string &, std::vector<boost::shared_ptr<ARDOUR::Playlist> > const &);

	void build_playlist_menu ();

	void add_underlay (StreamView*, bool update_xml = true);
	void remove_underlay (StreamView*);
	void build_underlay_menu(Gtk::Menu*);

	int set_state (const XMLNode&, int version);

	virtual Gtk::CheckMenuItem* automation_child_menu_item (Evoral::Parameter);

	StreamView*         view() const { return _view; }
	ARDOUR::RouteGroup* route_group() const;
	boost::shared_ptr<ARDOUR::Playlist> playlist() const;

	void fast_update ();
	void hide_meter ();
	void show_meter ();
	void reset_meter ();
	void clear_meter ();
	void io_changed (ARDOUR::IOChange, void *);
	void meter_changed ();
	void effective_gain_display () { gm.effective_gain_display(); }

	std::string state_id() const;

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

	boost::shared_ptr<AutomationLine>
	find_processor_automation_curve (boost::shared_ptr<ARDOUR::Processor> i, Evoral::Parameter);

	void add_processor_automation_curve (boost::shared_ptr<ARDOUR::Processor> r, Evoral::Parameter);
	void add_existing_processor_automation_curves (boost::weak_ptr<ARDOUR::Processor>);


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
	void         show_playlist_selector ();
	void         playlist_changed ();

	void rename_current_playlist ();

	bool         automation_click (GdkEventButton *);

	virtual void show_all_automation (bool apply_to_selection = false);
	virtual void show_existing_automation (bool apply_to_selection = false);
	virtual void hide_all_automation (bool apply_to_selection = false);

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
	Gtk::Menu*          playlist_action_menu;
	Gtk::MenuItem*      playlist_item;

	void use_playlist (Gtk::RadioMenuItem *item, boost::weak_ptr<ARDOUR::Playlist> wpl);

	ArdourCanvas::Rectangle* timestretch_rect;

	/** Information about all automatable processor parameters that apply to
	 *  this route.  The Amp processor is not included in this list.
	 */
	std::list<ProcessorAutomationInfo*> processor_automation;

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

	void drop_instrument_ref ();
	void reread_midnam ();
	PBD::ScopedConnectionList midnam_connection;
};

#endif /* __ardour_route_time_axis_h__ */
