/*
 * Copyright (C) 2005-2008 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2005-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2015 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ardour_gtk_time_axis_h__
#define __ardour_gtk_time_axis_h__

#include <vector>
#include <list>

#include <gtkmm/box.h>
#include <gtkmm/fixed.h>
#include <gtkmm/frame.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/table.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/sizegroup.h>

#include "pbd/stateful.h"
#include "pbd/signals.h"

#include "evoral/Parameter.h"

#include "ardour/types.h"
#include "ardour/presentation_info.h"
#include "ardour/region.h"

#include "canvas/line.h"

#include "widgets/focus_entry.h"

#include "axis_view.h"
#include "enums.h"
#include "editing.h"

namespace ARDOUR {
	class Session;
	class Region;
	class Session;
	class RouteGroup;
	class Playlist;
	class Stripable;
}

namespace Gtk {
	class Menu;
}

namespace ArdourCanvas {
	class Canvas;
	class Container;
	class Item;
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
class ArdourDialog;
class ItemCounts;
class PasteContext;

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

	static void setup_sizes ();

	/** @return index of this TimeAxisView within its parent */
	int order () const { return _order; }

	/** @return maximum allowable value of order */
	static int max_order () { return _max_order; }

	ArdourCanvas::Container* canvas_display () { return _canvas_display; }
	ArdourCanvas::Container* ghost_group () { return _ghost_group; }

	/** @return effective height (taking children into account) in canvas units, or
		0 if this TimeAxisView has not yet been shown */
	uint32_t effective_height () const { return _effective_height; }

	/** @return y position, or -1 if hidden */
	double y_position () const { return _y_position; }

	/** @return our Editor */
	PublicEditor& editor () const { return _editor; }

	uint32_t current_height() const { return height; }

	void idle_resize (int32_t);

	virtual guint32 show_at (double y, int& nth, Gtk::VBox *parent);
	virtual void hide ();

	bool touched (double top, double bot);

	/** @return true if hidden, otherwise false */
	bool hidden () const { return _hidden; }

	void set_selected (bool);

	virtual bool selectable() const { return true; }

	/**
	 * potential handler for entered events
	 */

	virtual void entered () {}
	virtual void exited () {}

	enum TrackHeightMode {
		OnlySelf,
		TotalHeight,
		HeightPerLane
	};

	virtual void set_height (uint32_t h, TrackHeightMode m = OnlySelf);
	void set_height_enum (Height, bool apply_to_selection = false);
	void reset_height();

	virtual void reset_visual_state ();

	std::pair<TimeAxisView*, double> covers_y_position (double) const;
	bool covered_by_y_range (double y0, double y1) const;

	virtual void step_height (bool);

	virtual ARDOUR::RouteGroup* route_group() const { return 0; }
	virtual boost::shared_ptr<ARDOUR::Playlist> playlist() const { return boost::shared_ptr<ARDOUR::Playlist> (); }

	virtual void set_samples_per_pixel (double);
	virtual void show_selection (TimeSelection&);
	virtual void hide_selection ();
	virtual void reshow_selection (TimeSelection&);
	virtual void show_timestretch (Temporal::timepos_t const & start, Temporal::timepos_t const & end, int layers, int layer);
	virtual void hide_timestretch ();

	/* editing operations */

	virtual void cut_copy_clear (Selection&, Editing::CutCopyOp) {}

	/** Paste a selection.
	 *  @param pos Position to paste to (session samples).
	 *  @param selection Selection to paste.
	 *  @param ctx Paste context.
	 *  @param sub_num music-time sub-division: \c -1: snap to bar, \c 1: exact beat, \c >1: \c (1 \c / \p sub_num \c ) beat-divisions
	 */
	virtual bool paste (Temporal::timepos_t const & pos,
	                    const Selection&    selection,
	                    PasteContext&       ctx)
	{
		return false;
	}


	virtual void set_selected_regionviews (RegionSelection&) {}
	virtual void set_selected_points (PointSelection&);

	virtual void fade_range (TimeSelection&) {}

	virtual boost::shared_ptr<ARDOUR::Region> find_next_region (ARDOUR::timepos_t const & /*pos*/, ARDOUR::RegionPoint, int32_t /*dir*/) {
		return boost::shared_ptr<ARDOUR::Region> ();
	}

	void order_selection_trims (ArdourCanvas::Item *item, bool put_start_on_top);

	virtual void get_selectables (Temporal::timepos_t const &, Temporal::timepos_t  const &, double, double, std::list<Selectable*>&, bool within = false);
	virtual void get_inverted_selectables (Selection&, std::list<Selectable *>& results);
	virtual void get_regionviews_at_or_after (Temporal::timepos_t const &, RegionSelection&) {}

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
	Children get_child_list () const;

	static uint32_t preset_height (Height);

protected:
	static Glib::RefPtr<Gtk::SizeGroup> controls_meters_size_group;
	static Glib::RefPtr<Gtk::SizeGroup> midi_scroomer_size_group;
	static unsigned int name_width_px;
	/* The Standard LHS Controls */
	Gtk::Table             controls_table;
	Glib::RefPtr<Gtk::SizeGroup> controls_button_size_group;
	Gtk::EventBox          controls_ebox;
	Gtk::VBox              controls_vbox;
	Gtk::VBox              time_axis_vbox;
	Gtk::HBox              time_axis_hbox;
	Gtk::Frame             time_axis_frame;
	Gtk::HBox              top_hbox;
	Gtk::Fixed             scroomer_placeholder;
	bool                  _name_editing;
	uint32_t               height;  /* in canvas units */
	std::string            controls_base_unselected_name;
	std::string            controls_base_selected_name;
	Gtk::Menu*             display_menu; /* The standard LHS Track control popup-menus */
	TimeAxisView*          parent;
	ArdourCanvas::Container*   selection_group;
	ArdourCanvas::Container*  _ghost_group;
	std::list<GhostRegion*> ghosts;
	std::list<SelectionRect*> free_selection_rects;
	std::list<SelectionRect*> used_selection_rects;
	bool                  _hidden;
	bool                   in_destructor;
	Gtk::Menu*            _size_menu;
	ArdourCanvas::Line*       _canvas_separator;
	ArdourCanvas::Container*  _canvas_display;
	double                _y_position;
	PublicEditor&         _editor;

	virtual bool can_edit_name() const;

	void begin_name_edit ();
	void end_name_edit (std::string, int);
	virtual std::string name () const { return name_label.get_text (); }

	/* derived classes can override these */

	virtual bool name_entry_changed (std::string const&);

	/** Handle mouse release on our LHS control name ebox.
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

	Children children;
	bool is_child (TimeAxisView*);
	virtual bool propagate_time_selection () const { return false; }

	virtual void remove_child (boost::shared_ptr<TimeAxisView>);
	void add_child (boost::shared_ptr<TimeAxisView>);

	/* selection display */

	virtual void selection_click (GdkEventButton*);

	void color_handler ();
	void parameter_changed (std::string const &);

	void conditionally_add_to_selection ();

	void build_size_menu ();

private:
	Gtk::VBox*            control_parent;
	int                  _order;
	uint32_t             _effective_height;
	double               _resize_drag_start;
	bool                 _did_resize;
	GdkCursor*           _preresize_cursor;
	bool                 _have_preresize_cursor;
	bool                 _ebox_release_can_act;

	static uint32_t button_height;
	static uint32_t extra_height;
	static int const _max_order;

	SelectionRect* get_selection_rect(uint32_t id);

	void compute_heights ();
	bool maybe_set_cursor (int y);

}; /* class TimeAxisView */

#endif /* __ardour_gtk_time_axis_h__ */
