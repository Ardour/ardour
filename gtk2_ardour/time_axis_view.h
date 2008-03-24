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
#include <gtkmm/eventbox.h>
#include <gtkmm/table.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>

#include <gtkmm2ext/focus_entry.h>

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
class RegionSelection;
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
  private:
	enum NamePackingBits {
		NameLabelPacked = 0x1,
		NameEntryPacked = 0x2
	};

  public:
	enum TrackHeight { 
		Largest,
		Large,
		Larger,
		Normal,
		Smaller,
		Small
	};
	
	static uint32_t hLargest;
	static uint32_t hLarge;
	static uint32_t hLarger;
	static uint32_t hNormal;
	static uint32_t hSmaller;
	static uint32_t hSmall;

	static uint32_t height_to_pixels (TrackHeight);

	TimeAxisView(ARDOUR::Session& sess, PublicEditor& ed, TimeAxisView* parent, ArdourCanvas::Canvas& canvas);
	virtual ~TimeAxisView ();

	/* public data: XXX create accessor/mutators for these ?? */

	PublicEditor& editor;
	
	TrackHeight height_style; 
	uint32_t height;  /* in canvas units */
	uint32_t effective_height;  /* in canvas units */
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
 	Gtkmm2ext::FocusEntry name_entry;

	void hide_name_label ();
	void hide_name_entry ();
	void show_name_label ();
	void show_name_entry ();

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

	virtual void set_height (TrackHeight h);
	void reset_height();

	/**
	 * Returns a TimeAxisView* if this object covers y, or one of its children does.
	 *  If the covering object is a child axis, then the child is returned.
	 * Returns 0 otherwise.
	 */

	TimeAxisView* covers_y_position (double y);

	/**
	 * Steps through the defined heights for this TrackView.
	 * Sets bigger to true to step up in size, set to fals eot step smaller.
	 *
	 * @param bigger true if stepping should increase in size, false otherwise
	 */
	virtual void step_height (bool bigger);

	virtual ARDOUR::RouteGroup* edit_group() const { return 0; }
	virtual boost::shared_ptr<ARDOUR::Playlist> playlist() const { return boost::shared_ptr<ARDOUR::Playlist> (); }

	virtual void show_feature_lines (const ARDOUR::AnalysisFeatureList&);
	virtual void hide_feature_lines ();

	virtual void set_samples_per_unit (double);
	virtual void show_selection (TimeSelection&);
	virtual void hide_selection ();
	virtual void reshow_selection (TimeSelection&);
	virtual void show_timestretch (nframes_t start, nframes_t end);
	virtual void hide_timestretch ();

	virtual void hide_dependent_views (TimeAxisViewItem&) {}
	virtual void reveal_dependent_views (TimeAxisViewItem&) {}

	/* editing operations */
	
	virtual bool cut_copy_clear (Selection&, Editing::CutCopyOp) { return false; }
	virtual bool paste (nframes_t, float times, Selection&, size_t nth) { return false; }
	
	virtual void set_selected_regionviews (RegionSelection&) {}
	virtual void set_selected_points (PointSelection&) {}

	virtual boost::shared_ptr<ARDOUR::Region> find_next_region (nframes_t pos, ARDOUR::RegionPoint, int32_t dir) {
		return boost::shared_ptr<ARDOUR::Region> ();
	}

  	void order_selection_trims (ArdourCanvas::Item *item, bool put_start_on_top);

	virtual void get_selectables (nframes_t start, nframes_t end, double top, double bot, list<Selectable*>& results);
	virtual void get_inverted_selectables (Selection&, list<Selectable *>& results);

	/* called at load time when first GUI idle occurs. put
	   expensive data loading/redisplay code in here.
	*/
	
	virtual void first_idle () {}

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
	virtual bool controls_ebox_button_release (GdkEventButton *ev);
	virtual bool controls_ebox_scroll (GdkEventScroll *ev);

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
	 * Build the standard LHS control size menu for the default heights options.
	 *
	 */
	virtual void build_size_menu();

	/**
	 * Displays the standard LHS controls size menu for the track heights
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
	NamePackingBits name_packing;

	static void compute_controls_size_info ();
	static bool need_size_info;

	void set_heights (TrackHeight);
	void set_height_pixels (uint32_t h);
	void color_handler ();

	list<ArdourCanvas::SimpleLine*> feature_lines;
	ARDOUR::AnalysisFeatureList analysis_features;
	void reshow_feature_lines ();

	void conditionally_add_to_selection ();

}; /* class TimeAxisView */

#endif /* __ardour_gtk_time_axis_h__ */

