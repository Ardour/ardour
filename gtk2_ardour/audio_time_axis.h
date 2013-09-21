/*
    Copyright (C) 2000-2006 Paul Davis

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

#include "ardour/types.h"

#include "ardour_dialog.h"
#include "route_ui.h"
#include "enums.h"
#include "editing.h"
#include "route_time_axis.h"

namespace ARDOUR {
	class Session;
	class RouteGroup;
	class IOProcessor;
	class Processor;
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
class TimeSelection;
class AutomationTimeAxisView;

class AudioTimeAxisView : public RouteTimeAxisView
{
  public:
 	AudioTimeAxisView (PublicEditor&, ARDOUR::Session*, ArdourCanvas::Canvas& canvas);
 	virtual ~AudioTimeAxisView ();

	void set_route (boost::shared_ptr<ARDOUR::Route>);

	AudioStreamView* audio_view();

	void set_show_waveforms_recording (bool yn);

	/* Overridden from parent to store display state */
	guint32 show_at (double y, int& nth, Gtk::VBox *parent);

        void enter_internal_edit_mode ();
        void leave_internal_edit_mode ();

	void create_automation_child (const Evoral::Parameter& param, bool show);

	void first_idle ();

  private:
	friend class AudioStreamView;
	friend class AudioRegionView;

	void route_active_changed ();

	Gtk::Menu* build_mode_menu();
	void build_automation_action_menu (bool);

	void show_all_automation (bool apply_to_selection = false);
	void show_existing_automation (bool apply_to_selection = false);
	void hide_all_automation (bool apply_to_selection = false);

	void hide ();

	void gain_hidden ();
	void pan_hidden ();

	void ensure_pan_views (bool show = true);
	void update_control_names ();

	void update_gain_track_visibility ();
	void update_pan_track_visibility ();

	Gtk::CheckMenuItem* gain_automation_item;
	std::list<boost::shared_ptr<AutomationTimeAxisView> > pan_tracks;
	Gtk::CheckMenuItem* pan_automation_item;
};

#endif /* __ardour_audio_time_axis_h__ */

