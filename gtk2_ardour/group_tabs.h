/*
    Copyright (C) 2009 Paul Davis

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

#include <gtkmm/menu.h>
#include "editor_component.h"
#include "gtkmm2ext/cairo_widget.h"

namespace ARDOUR {
	class Session;
	class RouteGroup;
}

class Editor;

/** Parent class for tabs which represent route groups as colored tabs;
 *  Currently used on the left-hand side of the editor and at the top of the mixer.
 *
 *  This class also contains a fair bit of code to handle changes to route
 *  group colours; it seems a bit out of place, but I could not really think
 *  of a better place to put it.
 */
class GroupTabs : public CairoWidget, public ARDOUR::SessionHandlePtr
{
public:
	GroupTabs ();
	virtual ~GroupTabs ();

	void set_session (ARDOUR::Session *);

	/** @param g Route group, or 0.
         *  @return Menu to be popped up on right-click over the given route group.
	 */
	Gtk::Menu* get_menu (ARDOUR::RouteGroup* g);

	void run_new_group_dialog (ARDOUR::RouteList const &);

	static void set_group_color (ARDOUR::RouteGroup *, Gdk::Color);
	static std::string group_gui_id (ARDOUR::RouteGroup *);
	static Gdk::Color group_color (ARDOUR::RouteGroup *);

protected:

	struct Tab {
		Tab () : group (0) {}

		double from;
		double to;
		Gdk::Color color; ///< color
		ARDOUR::RouteGroup* group; ///< route group
	};

private:
	static void emit_gui_changed_for_members (ARDOUR::RouteGroup *);
	
	/** Compute all the tabs for this widget.
	 *  @return Tabs.
	 */
	virtual std::list<Tab> compute_tabs () const = 0;

	/** Draw a tab.
	 *  @param cr Cairo context.
	 *  @param t Tab.
	 */
	virtual void draw_tab (cairo_t* cr, Tab const & t) const = 0;

	/** @param x x coordinate
	 *  @param y y coordinate
	 *  @return x or y, depending on which is the primary coordinate for this widget.
	 */
	virtual double primary_coordinate (double, double) const = 0;

	virtual ARDOUR::RouteList routes_for_tab (Tab const * t) const = 0;

	/** @return Size of the widget along the primary axis */
	virtual double extent () const = 0;

	virtual void add_menu_items (Gtk::Menu *, ARDOUR::RouteGroup *) {}
	virtual PBD::PropertyList default_properties () const = 0;
        virtual ARDOUR::RouteSortOrderKey order_key () const = 0;
	virtual ARDOUR::RouteList selected_routes () const = 0;
	virtual void sync_order_keys () = 0;

	void new_from_selection ();
	void new_from_rec_enabled ();
	void new_from_soloed ();
	ARDOUR::RouteGroup* create_and_add_group () const;
	void collect (ARDOUR::RouteGroup *);
	void set_activation (ARDOUR::RouteGroup *, bool);
	void edit_group (ARDOUR::RouteGroup *);
	void subgroup (ARDOUR::RouteGroup *, bool, ARDOUR::Placement);
	void activate_all ();
	void disable_all ();
	void remove_group (ARDOUR::RouteGroup *);

	void render (cairo_t *);
	void on_size_request (Gtk::Requisition *);
	bool on_button_press_event (GdkEventButton *);
	bool on_motion_notify_event (GdkEventMotion *);
	bool on_button_release_event (GdkEventButton *);

	Tab * click_to_tab (double, std::list<Tab>::iterator *, std::list<Tab>::iterator *);

	void route_group_property_changed (ARDOUR::RouteGroup *);
	void route_added_to_route_group (ARDOUR::RouteGroup *, boost::weak_ptr<ARDOUR::Route>);
	void route_removed_from_route_group (ARDOUR::RouteGroup *, boost::weak_ptr<ARDOUR::Route>);

	Gtk::Menu* _menu;
	std::list<Tab> _tabs; ///< current list of tabs
	Tab* _dragging; ///< tab being dragged, or 0
	/** routes that were in the tab that is being dragged when the drag started */
	ARDOUR::RouteList _initial_dragging_routes;
	bool _dragging_new_tab; ///< true if we're dragging a new tab
	bool _drag_moved; ///< true if there has been movement during any current drag
	double _drag_fixed; ///< the position of the fixed end of the tab being dragged
	double _drag_moving; ///< the position of the moving end of the tab being dragged
	double _drag_offset; ///< offset from the mouse to the end of the tab being dragged
	double _drag_min; ///< minimum position for drag
	double _drag_max; ///< maximum position for drag
	double _drag_first; ///< first mouse pointer position during drag

	/** colors that have been used for new route group tabs */
	static std::list<Gdk::Color> _used_colors;
};
