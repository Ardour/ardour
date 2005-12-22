/*
    Copyright (C) 2003 Paul Davis 

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

#ifndef __ardour_gtk_time_axis_h__
#define __ardour_gtk_time_axis_h__

#include <vector>
#include <list>

#include <gtkmm/box.h>
#include <gtkmm/frame.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/table.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>

#include <ardour/types.h>
#include <ardour/region.h>

#include "prompter.h"
#include "axis_view.h"
#include "enums.h"
#include "editing.h"
#include "canvas.h"

namespace ARDOUR {
	class Session;
	class Region;
	class Session;
	class RouteGroup;
	class Playlist;
}

namespace Gtk {
	class Menu;
}

class PublicEditor;
class AudioRegionSelection;
class TimeSelection;
class PointSelection;
class TimeAxisViewItem;
class Selection;
class Selectable;

/**
 * TimeAxisView defines the abstract base class for time-axis views.
 *
 * This class provides the basic LHS controls and display methods. This should be
 * extended to create functional time-axis based views.
 *
 */
class TimeAxisView : public virtual AxisView
{
  public:
	enum TrackHeight { 
                /* canvas units. they need to be odd
		   valued so that there is a precise
		   middle.
		*/
		Largest = 301,
		Large = 201,
		Larger = 101,
		Normal = 51,
		Smaller = 31,
		Small = 21
	};

	TimeAxisView(ARDOUR::Session& sess, PublicEditor& ed, TimeAxisView* parent, ArdourCanvas::Canvas& canvas);
	virtual ~TimeAxisView ();

	/* public data: XXX create accessor/mutators for these ?? */

	PublicEditor& editor;

	guint32 height;  /* in canvas units */
	guint32 effective_height;  /* in canvas units */
	double  y_position;
	int     order;

	
	ArdourCanvas::Group   *canvas_display;
 	Gtk::VBox       *control_parent;

	/* The Standard LHS Controls */
	Gtk::Frame    controls_frame;
	Gtk::HBox     controls_hbox;
	Gtk::EventBox controls_lhs_pad;
	Gtk::Table    controls_table;
	Gtk::EventBox controls_ebox;
	Gtk::VBox     controls_vbox;
	Gtk::HBox     name_hbox;
	Gtk::Frame    name_frame;
 	Gtk::Entry    name_entry;

	/**
	 * Display this TrackView as the nth component of the parent box, at y.
	 *
	 * @param y 
	 * @param nth
	 * @param parent the parent component
	 * @return the height of this TrackView
	 */
	virtual guint32 show_at (double y, int& nth, Gtk::VBox *parent);

	bool touched (double top, double bot);

	/**
	 * Hides this TrackView
	 */
	virtual void hide ();
	bool hidden() const { return _hidden; }

	virtual void set_selected (bool);

	/**
	 * potential handler for entered events
	 */

	virtual void entered () {}
	virtual void exited () {}

	/**
	 * Sets the height of this TrackView to one of ths TrackHeghts
	 *
	 * @param h the TrackHeight value to set
	 */
	virtual void set_height (TrackHeight h);
	void reset_height();
	/**
	 * Steps through the defined TrackHeights for this TrackView.
	 * Sets bigger to true to step up in size, set to fals eot step smaller.
	 *
	 * @param bigger true if stepping should increase in size, false otherwise
	 */
	virtual void step_height (bool bigger);

	virtual ARDOUR::RouteGroup* edit_group() const { return 0; }
	virtual ARDOUR::Playlist* playlist() const { return 0; }

	virtual void set_samples_per_unit (double);
	virtual void show_selection (TimeSelection&);
	virtual void hide_selection ();
	virtual void reshow_selection (TimeSelection&);
	virtual void show_timestretch (jack_nframes_t start, jack_nframes_t end);
	virtual void hide_timestretch ();

	virtual void hide_dependent_views (TimeAxisViewItem&) {}
	virtual void reveal_dependent_views (TimeAxisViewItem&) {}

	/* editing operations */
	
	virtual bool cut_copy_clear (Selection&, Editing::CutCopyOp) { return false; }
	virtual bool paste (jack_nframes_t, float times, Selection&, size_t nth) { return false; }
	
	virtual void set_selected_regionviews (AudioRegionSelection&) {}
	virtual void set_selected_points (PointSelection&) {}

	virtual ARDOUR::Region* find_next_region (jack_nframes_t pos, ARDOUR::RegionPoint, int32_t dir) {
		return 0;
	}

  	void order_selection_trims (ArdourCanvas::Item *item, bool put_start_on_top);

	virtual void get_selectables (jack_nframes_t start, jack_nframes_t end, double top, double bot, list<Selectable*>& results);
	virtual void get_inverted_selectables (Selection&, list<Selectable *>& results);

	/* state/serialization management */

	void set_parent (TimeAxisView& p);
	bool has_state () const;

	virtual void set_state (const XMLNode&);
	virtual XMLNode* get_state_node () { return 0; }

	/* call this on the parent */

	virtual XMLNode* get_child_xml_node (const string & childname) { return 0; }

  protected:

	string controls_base_unselected_name;
	string controls_base_selected_name;

	bool name_entry_button_press (GdkEventButton *ev);
	bool name_entry_button_release (GdkEventButton *ev);
	bool name_entry_key_release (GdkEventKey *ev);
	void name_entry_activated ();
	sigc::connection name_entry_key_timeout;
	bool name_entry_key_timed_out ();
	guint32 last_name_entry_key_press_event;
	
	/* derived classes can override these */

	virtual void name_entry_changed ();
	virtual bool name_entry_focus_in (GdkEventFocus *ev);
	virtual bool name_entry_focus_out (GdkEventFocus *ev);

	/**
	 * Handle mouse relaese on our LHS control name ebox.
	 * 
	 *@ param ev the event
	 */
	virtual gint controls_ebox_button_release (GdkEventButton *ev);

	/**
	 * Displays the standard LHS control menu at when.
	 *
	 * @param when the popup activation time
	 */
	virtual void popup_display_menu (guint32 when);

	/**
	 * Build the standard LHS control menu.
	 * Subclasses should extend this method to add their own menu options.
	 *
	 */
	virtual void build_display_menu ();

	/**
         * Do anything that needs to be done to dynamically reset
	 * the LHS control menu.
	 */
	virtual bool handle_display_menu_map_event (GdkEventAny *ev) { return false; }

	/**
	 * Build the standard LHS control size menu for the default TrackHeight options.
	 *
	 */
	virtual void build_size_menu();

	/**
	 * Displays the standard LHS controls size menu for the TrackHeight.
	 *
	 * @parem when the popup activation time
	 */
 	void popup_size_menu(guint32 when);

	/**
	 * Handle the size option of out main menu.
	 * 
	 * @param ev the event
	 */
	gint size_click(GdkEventButton *ev);

	/* The standard LHS Track control popup-menus */

	Gtk::Menu *display_menu;
	Gtk::Menu *size_menu;

	Gtk::Label    name_label;

	TimeAxisView* parent;

	/* find the parent with state */

	TimeAxisView* get_parent_with_state();

	std::vector<TimeAxisView*> children;
	bool is_child (TimeAxisView*);

	void remove_child (TimeAxisView*);
	void add_child (TimeAxisView*);

	/* selection display */

	ArdourCanvas::Group      *selection_group;

	list<SelectionRect*> free_selection_rects;
	list<SelectionRect*> used_selection_rects;

	SelectionRect* get_selection_rect(uint32_t id);

	virtual void selection_click (GdkEventButton*);

	bool _hidden;
	bool _has_state;

}; /* class TimeAxisView */

#endif /* __ardour_gtk_time_axis_h__ */

