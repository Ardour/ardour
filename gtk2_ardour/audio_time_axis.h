/*
    Copyright (C) 2000 Paul Davis 

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

    $Id$
*/

#ifndef __ardour_trackview_h__
#define __ardour_trackview_h__

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
#include <ardour/region.h>

#include "ardour_dialog.h"
#include "route_ui.h"
#include "enums.h"
#include "time_axis_view.h"
#include "canvas.h"

namespace ALSA {
	class MultiChannelDevice;
}

namespace ARDOUR {
	class Session;
	class DiskStream;
	class RouteGroup;
	class Redirect;
	class Insert;
	class Location;
	class AudioPlaylist;
}

namespace LADSPA {
	class Manager;
	class Plugin;
}

class PublicEditor;
class AudioThing;
class StreamView;
class Selection;
class Selectable;
class AudioRegionView;
class AutomationLine;
class AutomationGainLine;
class AutomationPanLine;
class RedirectAutomationLine;
class TimeSelection;
class AutomationTimeAxisView;

class AudioTimeAxisView : public RouteUI, public TimeAxisView
{
  public:
 	AudioTimeAxisView (PublicEditor&, ARDOUR::Session&, ARDOUR::Route&, ArdourCanvas::Canvas& canvas);
 	virtual ~AudioTimeAxisView ();

	void show_selection (TimeSelection&);
	void automation_control_point_changed (ARDOUR::AutomationType);

	void set_samples_per_unit (double);
 	void set_height (TimeAxisView::TrackHeight);
	void set_show_waveforms (bool yn);
	void set_show_waveforms_recording (bool yn);
	void show_timestretch (jack_nframes_t start, jack_nframes_t end);
	void hide_timestretch ();
	void selection_click (GdkEventButton*);
	void set_selected_regionviews (AudioRegionSelection&);
	void set_selected_points (PointSelection&);
	void get_selectables (jack_nframes_t start, jack_nframes_t end, double top, double bot, list<Selectable *>&);
	void get_inverted_selectables (Selection&, list<Selectable*>&);
	void show_all_xfades ();
	void hide_all_xfades ();
	void hide_dependent_views (TimeAxisViewItem&);
	void reveal_dependent_views (TimeAxisViewItem&);
		
	ARDOUR::Region* find_next_region (jack_nframes_t pos, ARDOUR::RegionPoint, int32_t dir);

	string name() const;

	ARDOUR::RouteGroup* edit_group() const;

	void build_playlist_menu (Gtk::Menu *);
	ARDOUR::Playlist* playlist() const;

	/* overridden from parent to store display state */
	guint32 show_at (double y, int& nth, Gtk::VBox *parent);
	void hide ();
	
	/* need accessors/mutators */

	StreamView      *view;

	/* editing operations */
	
	bool cut_copy_clear (Selection&, Editing::CutCopyOp);
	bool paste (jack_nframes_t, float times, Selection&, size_t nth);

	list<TimeAxisView*>get_child_list();

	void set_state (const XMLNode&);
	XMLNode* get_child_xml_node (std::string childname);

  private:
	friend class StreamView;
	friend class AudioRegionView;

	ArdourCanvas::Canvas& parent_canvas;

	bool         no_redraw;
  
	AutomationTimeAxisView *gain_track;
	AutomationTimeAxisView *pan_track;

	void update_automation_view (ARDOUR::AutomationType);
	void reset_redirect_automation_curves ();

	Gtk::HBox  other_button_hbox;

	Gtk::Table button_table;

	Gtk::Button       redirect_button;
	Gtk::Button       edit_group_button;
	Gtk::Button       playlist_button;
	Gtk::Button       size_button;
	Gtk::Button       automation_button;
	Gtk::Button       hide_button;
	Gtk::Button       visual_button;

	void route_active_changed ();

	void diskstream_changed (void *src);
	void update_diskstream_display ();
	
	gint edit_click  (GdkEventButton *);

	// variables to get the context menu
	// automation buttons correctly initialized
	bool show_gain_automation;
	bool show_pan_automation;

	void build_redirect_window ();
	void redirect_click ();
	void redirect_add ();
	void redirect_remove ();
	void redirect_edit ();
	void redirect_relist ();
	void redirect_row_selected (gint row, gint col, GdkEvent *ev);
	void add_to_redirect_display (ARDOUR::Redirect *);
	void redirects_changed (void *);

	sigc::connection modified_connection;
	sigc::connection state_changed_connection;

	void take_name_changed (void *);
	void route_name_changed (void *);
	void name_entry_activated ();
	void name_entry_changed ();
	gint name_entry_key_release_handler (GdkEventKey*);
	gint name_entry_button_release_handler (GdkEventButton*);
	gint name_entry_button_press_handler (GdkEventButton*);
	void on_area_realize ();

	virtual void label_view ();

	Gtk::Menu edit_group_menu;
	Gtk::RadioMenuItem::Group edit_group_menu_radio_group;

	void add_edit_group_menu_item (ARDOUR::RouteGroup *);
	void set_edit_group_from_menu (ARDOUR::RouteGroup *);

	void reset_samples_per_unit ();

	void select_track_color();
	
	virtual void build_display_menu ();

	Gtk::CheckMenuItem* waveform_item;
	Gtk::RadioMenuItem* traditional_item;
	Gtk::RadioMenuItem* rectified_item;
	
	Gtk::RadioMenuItem* align_existing_item;
	Gtk::RadioMenuItem* align_capture_item;
	
	void align_style_changed ();
	void set_align_style (ARDOUR::AlignStyle);

	void toggle_show_waveforms ();

	void set_waveform_shape (WaveformShape);
	void toggle_waveforms ();

	Gtk::Menu *playlist_menu;
	Gtk::Menu *playlist_action_menu;
	Gtk::MenuItem *playlist_item;
	
	/* playlist */

	void set_playlist (ARDOUR::AudioPlaylist *);
	void playlist_click ();
	void show_playlist_selector ();

	void playlist_changed ();
	void playlist_state_changed (ARDOUR::Change);
	void playlist_modified ();

	void add_playlist_to_playlist_menu (ARDOUR::Playlist*);
	void playlist_selected (ARDOUR::AudioPlaylist*);
	void use_new_playlist ();
	void use_copy_playlist ();
	void clear_playlist ();
	void rename_current_playlist ();

	/* automation stuff */
	
	Gtk::Menu* automation_action_menu;
	Gtk::CheckMenuItem* gain_automation_item;
	Gtk::CheckMenuItem* pan_automation_item;

	void automation_click ();
	void clear_automation ();
	void hide_all_automation ();
	void show_all_automation ();
	void show_existing_automation ();

	struct RedirectAutomationNode {
	    uint32_t     what;
	    Gtk::CheckMenuItem* menu_item;
	    AutomationTimeAxisView* view;
	    AudioTimeAxisView& parent;

	    RedirectAutomationNode (uint32_t w, Gtk::CheckMenuItem* mitem, AudioTimeAxisView& p)
		    : what (w), menu_item (mitem), view (0), parent (p) {}

	    ~RedirectAutomationNode ();
	};

	struct RedirectAutomationInfo {
	    ARDOUR::Redirect* redirect;
	    bool valid;
	    Gtk::Menu* menu;
	    vector<RedirectAutomationNode*> lines;

	    RedirectAutomationInfo (ARDOUR::Redirect* r) 
		    : redirect (r), valid (true) {}

	    ~RedirectAutomationInfo ();
	};

	list<RedirectAutomationInfo*> redirect_automation;
	RedirectAutomationNode* find_redirect_automation_node (ARDOUR::Redirect *redirect, uint32_t what);
	
	Gtk::Menu subplugin_menu;
	void add_redirect_to_subplugin_menu (ARDOUR::Redirect *);

	void remove_ran (RedirectAutomationNode* ran);

	void redirect_menu_item_toggled (AudioTimeAxisView::RedirectAutomationInfo*,
					 AudioTimeAxisView::RedirectAutomationNode*);
	void redirect_automation_track_hidden (RedirectAutomationNode*, ARDOUR::Redirect*);
	
	vector<RedirectAutomationLine*> redirect_automation_curves;
	RedirectAutomationLine *find_redirect_automation_curve (ARDOUR::Redirect*,uint32_t);
	void add_redirect_automation_curve (ARDOUR::Redirect*, uint32_t);
	void add_existing_redirect_automation_curves (ARDOUR::Redirect*);

	ArdourCanvas::SimpleRect *timestretch_rect;

	void timestretch (jack_nframes_t start, jack_nframes_t end);

	void visual_click ();
	void hide_click ();
	gint when_displayed (GdkEventAny*);

	void speed_changed ();
	
	void add_gain_automation_child ();
	void add_pan_automation_child ();
	void add_parameter_automation_child ();

	void toggle_gain_track ();
	void toggle_pan_track ();

	void gain_hidden ();
	void pan_hidden ();

	void update_pans ();

	void region_view_added (AudioRegionView*);
	void add_ghost_to_redirect (AudioRegionView*, AutomationTimeAxisView*);

	void map_frozen ();
};

#endif /* __ardour_trackview_h__ */

