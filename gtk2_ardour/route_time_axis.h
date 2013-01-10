/*
    Copyright (C) 2006 Paul Davis

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

#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/slider_controller.h>

#include "ardour/playlist.h"
#include "ardour/types.h"

#include "ardour_button.h"
#include "ardour_dialog.h"
#include "route_ui.h"
#include "enums.h"
#include "time_axis_view.h"
#include "canvas.h"
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

class RouteTimeAxisView : public RouteUI, public TimeAxisView
{
public:
 	RouteTimeAxisView (PublicEditor&, ARDOUR::Session*, ArdourCanvas::Canvas& canvas);
 	virtual ~RouteTimeAxisView ();

	void set_route (boost::shared_ptr<ARDOUR::Route>);

	void show_selection (TimeSelection&);
	void set_button_names ();

	void set_samples_per_unit (double);
 	void set_height (uint32_t h);
	void show_timestretch (framepos_t start, framepos_t end, int layers, int layer);
	void hide_timestretch ();
	void selection_click (GdkEventButton*);
	void set_selected_points (PointSelection&);
	void set_selected_regionviews (RegionSelection&);
	void get_selectables (ARDOUR::framepos_t start, ARDOUR::framepos_t end, double top, double bot, std::list<Selectable *>&);
	void get_inverted_selectables (Selection&, std::list<Selectable*>&);
	void set_layer_display (LayerDisplay d, bool apply_to_selection = false);
	LayerDisplay layer_display () const;

	boost::shared_ptr<ARDOUR::Region> find_next_region (framepos_t pos, ARDOUR::RegionPoint, int32_t dir);
	framepos_t find_next_region_boundary (framepos_t pos, int32_t dir);

	/* Editing operations */
	void cut_copy_clear (Selection&, Editing::CutCopyOp);
	bool paste (ARDOUR::framepos_t, float times, Selection&, size_t nth);
	RegionView* combine_regions ();
	void uncombine_regions ();
	void uncombine_region (RegionView*);
	void toggle_automation_track (const Evoral::Parameter& param);

	/* The editor calls these when mapping an operation across multiple tracks */
	void use_new_playlist (bool prompt, std::vector<boost::shared_ptr<ARDOUR::Playlist> > const &);
	void use_copy_playlist (bool prompt, std::vector<boost::shared_ptr<ARDOUR::Playlist> > const &);
	void clear_playlist ();

	/* group playlist name resolving */
	std::string resolve_new_group_playlist_name(std::string &, std::vector<boost::shared_ptr<ARDOUR::Playlist> > const &);

	void build_playlist_menu ();

	void add_underlay (StreamView*, bool update_xml = true);
	void remove_underlay (StreamView*);
	void build_underlay_menu(Gtk::Menu*);

	int set_state (const XMLNode&, int version);

	virtual void create_automation_child (const Evoral::Parameter& param, bool show) = 0;

	typedef std::map<Evoral::Parameter, boost::shared_ptr<AutomationTimeAxisView> > AutomationTracks;
	AutomationTracks automation_tracks() { return _automation_tracks; }

	boost::shared_ptr<AutomationTimeAxisView> automation_child(Evoral::Parameter param);
	virtual Gtk::CheckMenuItem* automation_child_menu_item (Evoral::Parameter);

	std::string         name() const;
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
	    bool                                 valid;
	    Gtk::Menu*                           menu;
	    std::vector<ProcessorAutomationNode*>     lines;

	    ProcessorAutomationInfo (boost::shared_ptr<ARDOUR::Processor> i)
		    : processor (i), valid (true), menu (0) {}

	    ~ProcessorAutomationInfo ();
	};


	void update_diskstream_display ();

	gint route_group_click  (GdkEventButton *);

	void processors_changed (ARDOUR::RouteProcessorChange);

	virtual void add_processor_to_subplugin_menu (boost::weak_ptr<ARDOUR::Processor>);
	void remove_processor_automation_node (ProcessorAutomationNode* pan);

	void processor_menu_item_toggled (RouteTimeAxisView::ProcessorAutomationInfo*,
	                                 RouteTimeAxisView::ProcessorAutomationNode*);

	void processor_automation_track_hidden (ProcessorAutomationNode*,
	                                       boost::shared_ptr<ARDOUR::Processor>);

	void automation_track_hidden (Evoral::Parameter param);

	ProcessorAutomationNode*
	find_processor_automation_node (boost::shared_ptr<ARDOUR::Processor> i, Evoral::Parameter);

	boost::shared_ptr<AutomationLine>
	find_processor_automation_curve (boost::shared_ptr<ARDOUR::Processor> i, Evoral::Parameter);

	void add_processor_automation_curve (boost::shared_ptr<ARDOUR::Processor> r, Evoral::Parameter);
	void add_existing_processor_automation_curves (boost::weak_ptr<ARDOUR::Processor>);

	void add_automation_child(Evoral::Parameter param, boost::shared_ptr<AutomationTimeAxisView> track, bool show=true);

	void reset_processor_automation_curves ();

	void take_name_changed (void *src);
	void route_property_changed (const PBD::PropertyChange&);
	void name_entry_changed ();

	void update_rec_display ();

	virtual void label_view ();

	void reset_samples_per_unit ();
	void horizontal_position_changed ();

	virtual void build_automation_action_menu (bool);
	virtual void append_extra_display_menu_items () {}
	void         build_display_menu ();

	void set_align_choice (Gtk::RadioMenuItem*, ARDOUR::AlignChoice, bool apply_to_selection = false);

	void         playlist_click ();
	void         show_playlist_selector ();
	void         playlist_changed ();

	void rename_current_playlist ();

	void         automation_click ();

	virtual void show_all_automation (bool apply_to_selection = false);
	virtual void show_existing_automation (bool apply_to_selection = false);
	virtual void hide_all_automation (bool apply_to_selection = false);

	void timestretch (framepos_t start, framepos_t end);
	void speed_changed ();
	void map_frozen ();
	void color_handler ();
	void region_view_added (RegionView*);
	void create_gain_automation_child (const Evoral::Parameter &, bool);
	void setup_processor_menu_and_curves ();
	void route_color_changed ();
        bool can_edit_name() const;

	boost::shared_ptr<AutomationTimeAxisView> gain_track;

	StreamView*           _view;
	ArdourCanvas::Canvas& parent_canvas;
	bool                  no_redraw;

	Gtk::HBox   other_button_hbox;
	Gtk::Table  button_table;
	ArdourButton route_group_button;
	ArdourButton playlist_button;
	ArdourButton automation_button;

	Gtk::Menu           subplugin_menu;
	Gtk::Menu*          automation_action_menu;
	Gtk::MenuItem*      plugins_submenu_item;
	RouteGroupMenu*     route_group_menu;
	Gtk::Menu*          playlist_action_menu;
	Gtk::MenuItem*      playlist_item;
	Gtk::Menu*          mode_menu;
	Gtk::Menu*          color_mode_menu;

	virtual Gtk::Menu* build_color_mode_menu() { return 0; }

	void use_playlist (Gtk::RadioMenuItem *item, boost::weak_ptr<ARDOUR::Playlist> wpl);

	ArdourCanvas::SimpleRect* timestretch_rect;

	void set_track_mode (ARDOUR::TrackMode, bool apply_to_selection = false);

	/** Information about all automatable processor parameters that apply to
	 *  this route.  The Amp processor is not included in this list.
	 */
	std::list<ProcessorAutomationInfo*> processor_automation;

	typedef std::vector<boost::shared_ptr<AutomationLine> > ProcessorAutomationCurves;
	ProcessorAutomationCurves processor_automation_curves;

	AutomationTracks _automation_tracks;
	typedef std::map<Evoral::Parameter, Gtk::CheckMenuItem*> ParameterMenuMap;
	/** parameter -> menu item map for the main automation menu */
	ParameterMenuMap _main_automation_menu_map;
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

private:

	void remove_child (boost::shared_ptr<TimeAxisView>);
	void update_playlist_tip ();
};

#endif /* __ardour_route_time_axis_h__ */

