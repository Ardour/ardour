/*
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2017-2019 Ben Loftis <ben@harrisonconsoles.com>
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
	~EditorSummary ();

	void set_session (ARDOUR::Session *);
	void set_overlays_dirty ();
	void set_background_dirty ();
	void routes_added (std::list<RouteTimeAxisView*> const &);

private:
	void parameter_changed (std::string);
	void on_size_allocate (Gtk::Allocation& alloc);

	enum SummaryPosition {
		LEFT,
		RIGHT,
		BOTTOM,
		INSIDE,
		TO_LEFT_OR_RIGHT
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

	void reset_to_extents ();

	void centre_on_click (GdkEventButton *);
	void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);
	void render_region (RegionView*, cairo_t*, double) const;
	void get_editor (std::pair<double, double>* x, std::pair<double, double>* y = NULL) const;
	void set_editor (double);
	void set_editor (std::pair<double, double>);
	void set_editor_x (double);
	void set_editor_x (std::pair<double, double>);
	void playhead_position_changed (samplepos_t);
	double editor_y_to_summary (double) const;
	SummaryPosition get_position (double, double) const;
	void set_cursor (SummaryPosition);
	void route_gui_changed (PBD::PropertyChange const&);
	bool suspending_editor_updates () const;
	double playhead_sample_to_position (samplepos_t) const;
	samplepos_t position_to_playhead_sample_to_position (double pos) const;
	void set_overlays_dirty_rect (int, int, int, int);

	void summary_zoom_step (  int steps );

	samplepos_t _start; ///< start sample of the overview
	samplepos_t _end; ///< end sample of the overview

	samplepos_t _leftmost; ///< the earliest sample we ever viewed
	samplepos_t _rightmost; ///< the latest sample we ever viewed

	double _x_scale; ///< pixels per sample for the x axis of the pixmap
	double _track_height;
	double _last_playhead;

	std::pair<double, double> _start_editor_x;
	double _start_mouse_x;
	double _start_mouse_y;

	SummaryPosition _start_position;

	bool _move_dragging;

	void set_colors ();
	uint32_t _phead_color;

	//used for zooming
	int _last_mx;
	int _last_my;
	int _last_dx;
	int _last_dy;
	int _last_y_delta;

	std::pair<double, double> _view_rectangle_x;
	std::pair<double, double> _view_rectangle_y;

	std::pair<double, double> _pending_editor_x;
	std::pair<double, double> _pending_editor_y;
	bool _pending_editor_changed;

	bool _zoom_trim_dragging;
	SummaryPosition _zoom_trim_position;

	bool _old_follow_playhead;
	cairo_surface_t* _image;
	void render_background_image ();
	bool _background_dirty;

	PBD::ScopedConnectionList position_connection;
	PBD::ScopedConnection route_ctrl_id_connection;
	PBD::ScopedConnectionList region_property_connection;
};

#endif
