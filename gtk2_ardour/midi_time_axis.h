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
#include "midi_streamview.h"

namespace ARDOUR {
	class Session;
	class RouteGroup;
	class Processor;
	class Location;
	class MidiPlaylist;
}

class PublicEditor;
class MidiStreamView;

class MidiTimeAxisView : public RouteTimeAxisView
{
  public:
 	MidiTimeAxisView (PublicEditor&, ARDOUR::Session&, boost::shared_ptr<ARDOUR::Route>, ArdourCanvas::Canvas& canvas);
 	virtual ~MidiTimeAxisView ();

	MidiStreamView* midi_view();

	/* overridden from parent to store display state */
	guint32 show_at (double y, int& nth, Gtk::VBox *parent);
	void hide ();

	void add_controller_track ();
	void create_automation_child (ARDOUR::Parameter param, bool show);

	ARDOUR::NoteMode note_mode() const { return _note_mode; }
	
  private:
	
	void append_extra_display_menu_items ();
	void build_automation_action_menu ();
	Gtk::Menu* build_mode_menu();

	void set_note_mode(ARDOUR::NoteMode mode);
	void set_note_range(MidiStreamView::VisibleNoteRange range);

	void route_active_changed ();

	void add_insert_to_subplugin_menu (ARDOUR::Processor *);
	
	Gtk::Menu _subplugin_menu;

	ARDOUR::NoteMode    _note_mode;
	Gtk::RadioMenuItem* _note_mode_item;
	Gtk::RadioMenuItem* _percussion_mode_item;
};

#endif /* __ardour_midi_time_axis_h__ */

