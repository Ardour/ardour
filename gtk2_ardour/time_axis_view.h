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

*/

#ifndef __ardour_gtk_time_axis_h__
#define __ardour_gtk_time_axis_h__

#include <vector>
#include <list>

#include <gtkmm/box.h>
#include <gtkmm/frame.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/table.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>

#include <gtkmm2ext/focus_entry.h>

#include "pbd/stateful.h"
#include "pbd/signals.h"

#include "ardour/types.h"
#include "ardour/region.h"
#include "evoral/Parameter.hpp"

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
class RegionSelection;
class TimeSelection;
class PointSelection;
class TimeAxisViewItem;
class Selection;
class Selectable;
class RegionView;
class GhostRegion;
class StreamView;

/** Abstract base class for time-axis views (horizontal editor 'strips')
 *
 * This class provides the basic LHS controls and display methods. This should be
 * extended to create functional time-axis based views.
 */
class TimeAxisView : public virtual AxisView
{
  private:
	enum NamePackingBits {
		NameLabelPacked = 0x1,
		NameEntryPacked = 0x2
	};

  public:

	TimeAxisView(ARDOUR::Session* sess, PublicEditor& ed, TimeAxisView* parent, ArdourCanvas::Canvas& canvas);
	virtual ~TimeAxisView ();

	static PBD::Signal1<void,TimeAxisView*> CatchDeletion;

	/** @return index of this TimeAxisView within its parent */
	int order () const { return _order; }

	/** @return maximum allowable value of order */
	static int max_order () { return _max_order; }

        virtual void enter_internal_edit_mode () {}
        virtual void leave_internal_edit_mode () {}

	ArdourCanvas::Group* canvas_display () { return _canvas_display; }
	ArdourCanvas::Group* canvas_background () { return _canvas_background; }
	ArdourCanvas::Group* ghost_group () { return _ghost_group; }

	/** @return effective height (taking children into account) in canvas units, or
	    0 if this TimeAxisView has not yet been shown */
	uint32_t effective_height () const { return _effective_height; }

	/** @return y position, or -1 if hidden */
	double y_position () const { return _y_position; }

	/** @return our Editor */
	PublicEditor& editor () const { return _editor; }

	uint32_t current_height() const { return height; }

	void idle_resize (uint32_t);

	void hide_name_label ();
	void hide_name_entry ();
	void show_name_label ();
	void show_name_entry ();

	virtual guint32 show_at (double y, int& nth, Gtk::VBox *parent);
	virtual void hide ();

	void clip_to_viewport ();

	bool touched (double top, double bot);

	/** @return true if hidden, otherwise false */
	bool hidden () const { return _hidden; }

	void set_selected (bool);

	/**
	 * potential handler for entered events
	 */

	virtual void entered () {}
	virtual void exited () {}

	virtual void set_height (uint32_t h);
	void set_height_enum (Height, bool apply_to_selection = false);
	void reset_height();

	virtual void reset_visual_state ();

	std::pair<TimeAxisView*, double> covers_y_position (double);

	virtual void step_height (bool);

	virtual ARDOUR::RouteGroup* route_group() const { return 0; }
	virtual boost::shared_ptr<ARDOUR::Playlist> playlist() const { return boost::shared_ptr<ARDOUR::Playlist> (); }

	virtual void set_samples_per_unit (double);
	virtual void show_selection (TimeSelection&);
	virtual void hide_selection ();
	virtual void reshow_selection (TimeSelection&);
	virtual void show_timestretch (framepos_t start, framepos_t end);
	virtual void hide_timestretch ();

	virtual void hide_dependent_views (TimeAxisViewItem&) {}
	virtual void reveal_dependent_views (TimeAxisViewItem&) {}

	/* editing operations */

	virtual void cut_copy_clear (Selection&, Editing::CutCopyOp) {}
	virtual bool paste (ARDOUR::framepos_t, float /*times*/, Selection&, size_t /*nth*/) { return false; }

	virtual void set_selected_regionviews (RegionSelection&) {}
	virtual void set_selected_points (PointSelection&) {}

	virtual boost::shared_ptr<ARDOUR::Region> find_next_region (framepos_t /*pos*/, ARDOUR::RegionPoint, int32_t /*dir*/) {
		return boost::shared_ptr<ARDOUR::Region> ();
	}

  	void order_selection_trims (ArdourCanvas::Item *item, bool put_start_on_top);

	virtual void get_selectables (ARDOUR::framepos_t, ARDOUR::framepos_t, double, double, std::list<Selectable*>&);
	virtual void get_inverted_selectables (Selection&, std::list<Selectable *>& results);

	void add_ghost (RegionView*);
	void remove_ghost (RegionView*);
	void erase_ghost (GhostRegion*);

	/** called at load time when first GUI idle occurs. put
	    expensive data loading/redisplay code in here. */
	virtual void first_idle () {}

	TimeAxisView* get_parent () { return parent; }
	void set_parent (TimeAxisView& p);

	virtual LayerDisplay layer_display () const { return Overlaid; }
	virtual StreamView* view () const { return 0; }

	typedef std::vector<boost::shared_ptr<TimeAxisView> > Children;
	Children get_child_list ();

	SelectionRect* get_selection_rect(uint32_t id);
	
	static uint32_t preset_height (Height);

  protected:
	/* The Standard LHS Controls */
	Gtk::HBox             controls_hbox;
	Gtk::Table            controls_table;
	Gtk::EventBox         controls_ebox;
	Gtk::VBox             controls_vbox;
	Gtk::VBox             time_axis_vbox;
	Gtk::HBox             name_hbox;
	Gtk::Frame            name_frame;
 	Gtkmm2ext::FocusEntry name_entry;

	uint32_t height;  /* in canvas units */

	std::string controls_base_unselected_name;
	std::string controls_base_selected_name;

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

	/** Handle mouse relaese on our LHS control name ebox.
	 *
	 *@ param ev the event
	 */
	virtual bool controls_ebox_button_release (GdkEventButton*);
	virtual bool controls_ebox_scroll (GdkEventScroll*);
	virtual bool controls_ebox_button_press (GdkEventButton*);
	virtual bool controls_ebox_motion (GdkEventMotion*);
	virtual bool controls_ebox_leave (GdkEventCrossing*);

	/** Display the standard LHS control menu at when.
	 *
	 * @param when the popup activation time
	 */
	virtual void popup_display_menu (guint32 when);

	/** Build the standard LHS control menu.
	 * Subclasses should extend this method to add their own menu options.
	 */
	virtual void build_display_menu ();

	/** Do whatever needs to be done to dynamically reset the LHS control menu.
	 */
	virtual bool handle_display_menu_map_event (GdkEventAny * /*ev*/) { return false; }

	/* The standard LHS Track control popup-menus */

	Gtk::Menu *display_menu;

	Gtk::Label    name_label;

	TimeAxisView* parent;

	Children children;
	bool is_child (TimeAxisView*);

	virtual void remove_child (boost::shared_ptr<TimeAxisView>);
	void add_child (boost::shared_ptr<TimeAxisView>);

	/* selection display */

	ArdourCanvas::Group      *selection_group;

	std::list<GhostRegion*> ghosts;

	std::list<SelectionRect*> free_selection_rects;
	std::list<SelectionRect*> used_selection_rects;

	virtual void selection_click (GdkEventButton*);

	bool _hidden;
	bool _has_state;
	bool in_destructor;
	NamePackingBits name_packing;

	void color_handler ();

	void conditionally_add_to_selection ();

	void build_size_menu ();
	Gtk::Menu* _size_menu;

	ArdourCanvas::Group* _canvas_display;
	double _y_position;
	PublicEditor& _editor;

private:

	ArdourCanvas::Group* _canvas_background;
 	Gtk::VBox* control_parent;
	int _order;
	uint32_t _effective_height;
	double _resize_drag_start;
	GdkCursor* _preresize_cursor;
	bool       _have_preresize_cursor;
	ArdourCanvas::Group* _ghost_group;

	void compute_heights ();
	static uint32_t button_height;
	static uint32_t extra_height;

	static int const _max_order;
	
	bool maybe_set_cursor (int y);

}; /* class TimeAxisView */

#endif /* __ardour_gtk_time_axis_h__ */

