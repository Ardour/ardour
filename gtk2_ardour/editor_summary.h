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

#ifndef __gtk_ardour_editor_summary_h__
#define __gtk_ardour_editor_summary_h__

#include "gtkmm2ext/cairo_widget.h"
#include "editor_component.h"

namespace ARDOUR {
	class Session;
}

class Editor;

/** Class to provide a visual summary of the contents of an editor window; represents
 *  the whole session as a set of lines, one per region view.
 */
class EditorSummary : public CairoWidget, public EditorComponent, public ARDOUR::SessionHandlePtr, public PBD::ScopedConnectionList
{
public:
	EditorSummary (Editor *);

	void set_session (ARDOUR::Session *);
	void set_overlays_dirty ();
	void routes_added (std::list<RouteTimeAxisView*> const &);

private:

	enum Position {
		LEFT,
		LEFT_TOP,
		TOP,
		RIGHT_TOP,
		RIGHT,
		RIGHT_BOTTOM,
		BOTTOM,
		LEFT_BOTTOM,
		INSIDE,
		BELOW_OR_ABOVE,
		TO_LEFT_OR_RIGHT,
		OTHERWISE_OUTSIDE
	};
      
	void on_size_request (Gtk::Requisition *);
	bool on_button_press_event (GdkEventButton *);
	bool on_button_release_event (GdkEventButton *);
	bool on_motion_notify_event (GdkEventMotion *);
	bool on_scroll_event (GdkEventScroll *);
        bool on_key_press_event (GdkEventKey*);
        bool on_key_release_event (GdkEventKey*);
        bool on_enter_notify_event (GdkEventCrossing*); 
        bool on_leave_notify_event (GdkEventCrossing*); 

	void centre_on_click (GdkEventButton *);
	void render (cairo_t *);
	void render_region (RegionView*, cairo_t*, double) const;
	void get_editor (std::pair<double, double> *, std::pair<double, double> *) const;
	void set_editor (double, double);
	void set_editor (std::pair<double, double>, double);
	void set_editor (std::pair<double, double>, std::pair<double, double>);
	void set_editor_x (double);
	void set_editor_x (std::pair<double, double>);
	void set_editor_y (double);
	void set_editor_y (std::pair<double, double>);
	void playhead_position_changed (framepos_t);
	double summary_y_to_editor (double) const;
	double editor_y_to_summary (double) const;
	Position get_position (double, double) const;
	void set_cursor (Position);
	void route_gui_changed (std::string);
	bool suspending_editor_updates () const;
	double playhead_frame_to_position (framepos_t) const;
        framepos_t position_to_playhead_frame_to_position (double pos) const;
	void set_overlays_dirty (int, int, int, int);

	framepos_t _start; ///< start frame of the overview
	framepos_t _end; ///< end frame of the overview

	/** fraction of the session length by which the overview size should extend past the start and end markers */
	double _overhang_fraction;

	double _x_scale; ///< pixels per frame for the x axis of the pixmap
	double _track_height;
	double _last_playhead;

	std::pair<double, double> _start_editor_x;
	std::pair<double, double> _start_editor_y;
	double _start_mouse_x;
	double _start_mouse_y;

	Position _start_position;

	bool _move_dragging;
	bool _moved;
	std::pair<double, double> _view_rectangle_x;
	std::pair<double, double> _view_rectangle_y;

	std::pair<double, double> _pending_editor_x;
	std::pair<double, double> _pending_editor_y;
	bool _pending_editor_changed;

	bool _zoom_dragging;
	Position _zoom_position;

	bool _old_follow_playhead;

	PBD::ScopedConnectionList position_connection;
	PBD::ScopedConnectionList region_property_connection;
};

#endif
