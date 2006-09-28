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

    $Id: audio_time_axis.h 664 2006-07-05 19:47:25Z drobilla $
*/

#ifndef __ardour_route_time_axis_h__
#define __ardour_route_time_axis_h__

#include <gtkmm/table.h>
#include <gtkmm/button.h>
#include <gtkmm/box.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/radiomenuitem.h>
#include <gtkmm/checkmenuitem.h>

#include <gtkmm2ext/selector.h>
#include <list>

#include <ardour/types.h>

#include "ardour_dialog.h"
#include "route_ui.h"
#include "enums.h"
#include "time_axis_view.h"
#include "canvas.h"
#include "color.h"

namespace ARDOUR {
	class Session;
	class Region;
	class Diskstream;
	class RouteGroup;
	class Redirect;
	class Insert;
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
class RedirectAutomationLine;
class TimeSelection;

class RouteTimeAxisView : public RouteUI, public TimeAxisView
{
public:
 	RouteTimeAxisView (PublicEditor&, ARDOUR::Session&, boost::shared_ptr<ARDOUR::Route>, ArdourCanvas::Canvas& canvas);
 	virtual ~RouteTimeAxisView ();

	void show_selection (TimeSelection&);

	void set_samples_per_unit (double);
 	void set_height (TimeAxisView::TrackHeight);
	void show_timestretch (nframes_t start, nframes_t end);
	void hide_timestretch ();
	void selection_click (GdkEventButton*);
	void set_selected_points (PointSelection&);
	void set_selected_regionviews (RegionSelection&);
	void get_selectables (nframes_t start, nframes_t end, double top, double bot, list<Selectable *>&);
	void get_inverted_selectables (Selection&, list<Selectable*>&);
		
	boost::shared_ptr<ARDOUR::Region> find_next_region (nframes_t pos, ARDOUR::RegionPoint, int32_t dir);

	/* Editing operations */
	bool cut_copy_clear (Selection&, Editing::CutCopyOp);
	bool paste (nframes_t, float times, Selection&, size_t nth);

	list<TimeAxisView*> get_child_list();

	/* The editor calls these when mapping an operation across multiple tracks */
	void use_new_playlist (bool prompt);
	void use_copy_playlist (bool prompt);
	void clear_playlist ();
	
	void build_playlist_menu (Gtk::Menu *);
	
	string              name() const;
	StreamView*         view() const { return _view; }
	ARDOUR::RouteGroup* edit_group() const;
	ARDOUR::Playlist*   playlist() const;

protected:
	friend class StreamView;
	
	struct RedirectAutomationNode {
	    uint32_t                what;
	    Gtk::CheckMenuItem*     menu_item;
	    AutomationTimeAxisView* view;
	    RouteTimeAxisView&      parent;

	    RedirectAutomationNode (uint32_t w, Gtk::CheckMenuItem* mitem, RouteTimeAxisView& p)
		    : what (w), menu_item (mitem), view (0), parent (p) {}

	    ~RedirectAutomationNode ();
	};

	struct RedirectAutomationInfo {
	    boost::shared_ptr<ARDOUR::Redirect> redirect;
	    bool                                valid;
	    Gtk::Menu*                          menu;
	    vector<RedirectAutomationNode*>     lines;

	    RedirectAutomationInfo (boost::shared_ptr<ARDOUR::Redirect> r) 
		    : redirect (r), valid (true), menu (0) {}

	    ~RedirectAutomationInfo ();
	};
	

	void diskstream_changed ();
	void update_diskstream_display ();
	
	gint edit_click  (GdkEventButton *);

	void build_redirect_window ();
	void redirect_click ();
	void redirect_add ();
	void redirect_remove ();
	void redirect_edit ();
	void redirect_relist ();
	void redirect_row_selected (gint row, gint col, GdkEvent *ev);
	void add_to_redirect_display (ARDOUR::Redirect *);
	void redirects_changed (void *);
	
	void add_redirect_to_subplugin_menu (boost::shared_ptr<ARDOUR::Redirect>);
	void remove_ran (RedirectAutomationNode* ran);

	void redirect_menu_item_toggled (RouteTimeAxisView::RedirectAutomationInfo*,
	                                 RouteTimeAxisView::RedirectAutomationNode*);
	
	void redirect_automation_track_hidden (RedirectAutomationNode*,
	                                       boost::shared_ptr<ARDOUR::Redirect>);

	RedirectAutomationNode*
	find_redirect_automation_node (boost::shared_ptr<ARDOUR::Redirect> r, uint32_t);
	
	RedirectAutomationLine*
	find_redirect_automation_curve (boost::shared_ptr<ARDOUR::Redirect> r, uint32_t);

	void add_redirect_automation_curve (boost::shared_ptr<ARDOUR::Redirect> r, uint32_t);
	void add_existing_redirect_automation_curves (boost::shared_ptr<ARDOUR::Redirect>);
	
	void reset_redirect_automation_curves ();

	void update_automation_view (ARDOUR::AutomationType);
	
	void take_name_changed (void *);
	void route_name_changed (void *);
	void name_entry_changed ();

	void on_area_realize ();

	virtual void label_view ();
	
	void add_edit_group_menu_item (ARDOUR::RouteGroup *, Gtk::RadioMenuItem::Group*);
	void set_edit_group_from_menu (ARDOUR::RouteGroup *);

	void reset_samples_per_unit ();

	void select_track_color();
	
	virtual void build_automation_action_menu ();
	virtual void append_extra_display_menu_items () {}
	void         build_display_menu ();
	
	void align_style_changed ();
	void set_align_style (ARDOUR::AlignStyle);
	
	virtual void set_playlist (ARDOUR::Playlist *);
	void         playlist_click ();
	void         show_playlist_selector ();
	void         playlist_changed ();
	void         playlist_state_changed (ARDOUR::Change);
	void         playlist_modified ();

	void add_playlist_to_playlist_menu (ARDOUR::Playlist*);
	void rename_current_playlist ();
	
	void         automation_click ();
	virtual void show_all_automation ();
	virtual void show_existing_automation ();
	virtual void hide_all_automation ();

	void timestretch (nframes_t start, nframes_t end);

	void visual_click ();
	void hide_click ();
	gint when_displayed (GdkEventAny*);

	void speed_changed ();
	
	void map_frozen ();

	void color_handler (ColorID, uint32_t);

	void region_view_added (RegionView*);
	void add_ghost_to_redirect (RegionView*, AutomationTimeAxisView*);
	
	
	StreamView*           _view;
	ArdourCanvas::Canvas& parent_canvas;
	bool                  no_redraw;
  
	Gtk::HBox   other_button_hbox;
	Gtk::Table  button_table;
	Gtk::Button redirect_button;
	Gtk::Button edit_group_button;
	Gtk::Button playlist_button;
	Gtk::Button size_button;
	Gtk::Button automation_button;
	Gtk::Button hide_button;
	Gtk::Button visual_button;
	
	Gtk::Menu           subplugin_menu;
	Gtk::Menu*          automation_action_menu;
	Gtk::Menu           edit_group_menu;
	Gtk::RadioMenuItem* align_existing_item;
	Gtk::RadioMenuItem* align_capture_item;
	Gtk::Menu*          playlist_menu;
	Gtk::Menu*          playlist_action_menu;
	Gtk::MenuItem*      playlist_item;

	ArdourCanvas::SimpleRect* timestretch_rect;
	
	list<RedirectAutomationInfo*>   redirect_automation;
	vector<RedirectAutomationLine*> redirect_automation_curves;

	sigc::connection modified_connection;
	sigc::connection state_changed_connection;
};

#endif /* __ardour_route_time_axis_h__ */

