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

#ifndef __ardour_midi_time_axis_h__
#define __ardour_midi_time_axis_h__

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
	class MidiDiskstream;
	class RouteGroup;
	class Redirect;
	class Insert;
	class Location;
	class MidiPlaylist;
}

class PublicEditor;
class Selection;
class Selectable;
class AutomationLine;
class AutomationGainLine;
class AutomationPanLine;
class RedirectAutomationLine;
class TimeSelection;
class AutomationTimeAxisView;

class MidiTimeAxisView : public RouteTimeAxisView
{
  public:
 	MidiTimeAxisView (PublicEditor&, ARDOUR::Session&, ARDOUR::Route&, ArdourCanvas::Canvas& canvas);
 	virtual ~MidiTimeAxisView ();

	/* overridden from parent to store display state */
	guint32 show_at (double y, int& nth, Gtk::VBox *parent);
	void hide ();
	
	void set_state (const XMLNode&);
	XMLNode* get_child_xml_node (const string & childname);

  private:
	void region_view_added (RegionView*) {}

	void route_active_changed ();

	void redirects_changed (void *);

	void build_display_menu ();

	void add_redirect_to_subplugin_menu (ARDOUR::Redirect *);
	
	Gtk::Menu subplugin_menu;
};

#endif /* __ardour_midi_time_axis_h__ */

