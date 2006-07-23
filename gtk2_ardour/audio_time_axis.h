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

#ifndef __ardour_audio_time_axis_h__
#define __ardour_audio_time_axis_h__

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
#include "route_time_axis.h"
#include "canvas.h"
#include "color.h"

namespace ARDOUR {
	class Session;
	class AudioDiskstream;
	class RouteGroup;
	class Redirect;
	class Insert;
	class Location;
	class AudioPlaylist;
}

class PublicEditor;
class AudioThing;
class AudioStreamView;
class Selection;
class Selectable;
class RegionView;
class AudioRegionView;
class AutomationLine;
class AutomationGainLine;
class AutomationPanLine;
class RedirectAutomationLine;
class TimeSelection;
class AutomationTimeAxisView;

class AudioTimeAxisView : public RouteTimeAxisView
{
  public:
 	AudioTimeAxisView (PublicEditor&, ARDOUR::Session&, ARDOUR::Route&, ArdourCanvas::Canvas& canvas);
 	virtual ~AudioTimeAxisView ();
	
	AudioStreamView* audio_view();

	void set_show_waveforms (bool yn);
	void set_show_waveforms_recording (bool yn);
	void show_all_xfades ();
	void hide_all_xfades ();
	void set_selected_regionviews (RegionSelection&);
	void hide_dependent_views (TimeAxisViewItem&);
	void reveal_dependent_views (TimeAxisViewItem&);
		
	/* overridden from parent to store display state */
	guint32 show_at (double y, int& nth, Gtk::VBox *parent);
	void hide ();
	
	void set_state (const XMLNode&);
	XMLNode* get_child_xml_node (const string & childname);

  private:
	friend class AudioStreamView;
	friend class AudioRegionView;
	
	void route_active_changed ();

	AutomationTimeAxisView *gain_track;
	AutomationTimeAxisView *pan_track;

	void update_automation_view (ARDOUR::AutomationType);
	void reset_redirect_automation_curves ();

	// variables to get the context menu
	// automation buttons correctly initialized
	bool show_gain_automation;
	bool show_pan_automation;

	void redirects_changed (void *);

	void build_display_menu ();

	Gtk::CheckMenuItem* waveform_item;
	Gtk::RadioMenuItem* traditional_item;
	Gtk::RadioMenuItem* rectified_item;
	
	void toggle_show_waveforms ();

	void set_waveform_shape (WaveformShape);
	void toggle_waveforms ();

	/* automation stuff */
	
	Gtk::Menu*          automation_action_menu;
	Gtk::CheckMenuItem* gain_automation_item;
	Gtk::CheckMenuItem* pan_automation_item;

	void automation_click ();
	void clear_automation ();
	void hide_all_automation ();
	void show_all_automation ();
	void show_existing_automation ();

	struct RedirectAutomationNode {
	    uint32_t what;
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

	void add_gain_automation_child ();
	void add_pan_automation_child ();
	void add_parameter_automation_child ();

	void toggle_gain_track ();
	void toggle_pan_track ();

	void gain_hidden ();
	void pan_hidden ();

	void update_pans ();

	void region_view_added (RegionView*);
	void add_ghost_to_redirect (RegionView*, AutomationTimeAxisView*);
};

#endif /* __ardour_audio_time_axis_h__ */

