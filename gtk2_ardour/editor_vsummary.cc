/*
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2013 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
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

#include "ardour/session.h"

#include <ydk/gdk.h>

#include <ytkmm/menu.h>
#include <ytkmm/menuitem.h>

#include "context_menu_helper.h"
#include "streamview.h"
#include "editor_vsummary.h"
#include "gui_thread.h"
#include "editor.h"
#include "region_view.h"
#include "rgb_macros.h"
#include "keyboard.h"
#include "editor_routes.h"
#include "editor_cursors.h"
#include "mouse_cursors.h"
#include "route_time_axis.h"
#include "vca_time_axis.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using Gtkmm2ext::Keyboard;

/** Construct an EditorVSummary.
 *  @param e Editor to represent.
 */
EditorVSummary::EditorVSummary (Editor& e)
	: EditorComponent (e),
	  _move_dragging (false),
	  _view_rectangle (0, 0),
	  _image (0),
	  _background_dirty (true)
{
	CairoWidget::use_nsglview (UIConfiguration::instance().get_nsgl_view_mode () == NSGLHiRes);
	add_events (Gdk::POINTER_MOTION_MASK|Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);
	set_can_focus ();

	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &EditorVSummary::parameter_changed));
}

EditorVSummary::~EditorVSummary ()
{
	cairo_surface_destroy (_image);
}

void
EditorVSummary::parameter_changed (string p)
{
	// TODO delete
	// if (p == "color-regions-using-track-color") {
	// 	set_background_dirty ();
	// }
}

void
EditorVSummary::presentation_info_changed (PBD::PropertyChange const & what_changed)
{
	if (what_changed.contains (Properties::color) || what_changed.contains (Properties::order)) {
		set_background_dirty();
	}
}

/** Handle a size allocation.
 *  @param alloc GTK allocation.
 */
void
EditorVSummary::on_size_allocate (Gtk::Allocation& alloc)
{
	CairoWidget::on_size_allocate (alloc);
	set_background_dirty ();
}


/** Connect to a session.
 *  @param s Session.
 */
void
EditorVSummary::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	if (_session) {
		/* watch for route color changes */
		PresentationInfo::Change.connect (route_ctrl_id_connection, invalidator (*this), std::bind (&EditorVSummary::presentation_info_changed, this, _1), gui_context());
		/* watch for group changes
		 * XXX RouteGroupPropertyChanged does not provide what_changed, not ideal
		 */
		_session->RouteGroupPropertyChanged.connect (_session_connections, invalidator (*this), std::bind (&EditorVSummary::set_background_dirty, this), gui_context());
		_session->RouteAddedToRouteGroup.connect (_session_connections, invalidator (*this), std::bind (&EditorVSummary::set_background_dirty, this), gui_context());
		_session->RouteRemovedFromRouteGroup.connect (_session_connections, invalidator (*this), std::bind (&EditorVSummary::set_background_dirty, this), gui_context());
		_session->route_group_removed.connect (_session_connections, invalidator (*this), std::bind (&EditorVSummary::set_background_dirty, this), gui_context());
	}

	UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &EditorVSummary::set_colors));

	set_colors();
	set_dirty ();

}

void
EditorVSummary::render_background_image ()
{
	cairo_surface_destroy (_image); // passing NULL is safe
	_image = cairo_image_surface_create (CAIRO_FORMAT_RGB24, get_width (), get_height ());

	cairo_t* cr = cairo_create (_image);

	/* background (really just the dividing lines between tracks */

	Gtkmm2ext::set_source_rgba (cr, _background_color);
	cairo_rectangle (cr, 0, 0, get_width(), get_height());
	cairo_fill (cr);

	/* render tracks / groups */

	cairo_push_group (cr);

	double y = 0;
	double track_height = 0;
	Gdk::Color track_color;
	Gdk::Color group_color;
	AudioTimeAxisView* audio;
	MidiTimeAxisView* midi;
	RouteTimeAxisView* route;
	VCATimeAxisView* vca;
	std::shared_ptr<RouteGroup> prev_group = nullptr;
	std::shared_ptr<RouteGroup> group = nullptr;


	for (TrackViewList::const_reverse_iterator i = _editor.track_views.crbegin(); i != _editor.track_views.crend(); ++i) {
		vca = dynamic_cast<VCATimeAxisView*>(*i);
		route = dynamic_cast<RouteTimeAxisView*>(*i);
		if ((!vca || vca->hidden()) && (!route || route->hidden())) {
			continue;
		}
		/* The editor's scrollable height depends on the last track's height, store it now */
		_editor_scroll_height = _editor._full_canvas_height + _editor.visible_canvas_height() - _editor.ruler_separator->position().y - (*i)->effective_height();
		break;
	}


	for (TrackViewList::const_iterator i = _editor.track_views.begin(); i != _editor.track_views.end(); ++i) {

		audio = dynamic_cast<AudioTimeAxisView*>(*i);
		midi = dynamic_cast<MidiTimeAxisView*>(*i);
		route = dynamic_cast<RouteTimeAxisView*>(*i);
		vca = dynamic_cast<VCATimeAxisView*>(*i);


		if ((!vca || vca->hidden()) && (!route || route->hidden())) {
			continue;
		}


		group = vca ? nullptr : route->route_group();

		track_height = editor_y_to_summary((*i)->effective_height());

		/* Route color */

		bool use_route_colors = true;
		double alpha;
		if (use_route_colors) {
			alpha = 0.75;
			if (route && route->is_master()) {
				alpha = 1.0;
				track_color = Gtkmm2ext::gdk_color_from_rgba(_bus_color);;
			} else if (vca) {
				track_color = vca->color();
			} else {
				track_color = (*i)->color();
			}
		} else {
			alpha = 1.0;
			if (vca) {
				track_color = Gtkmm2ext::gdk_color_from_rgba(_vca_color);
			} else if (midi) {
				track_color = Gtkmm2ext::gdk_color_from_rgba(_miditrack_color);
			} else if (audio && audio->is_audio_track()) {
				track_color = Gtkmm2ext::gdk_color_from_rgba(_audiotrack_color);
			} else {
				track_color = Gtkmm2ext::gdk_color_from_rgba(_bus_color);
			}

		}

		cairo_set_source_rgba (cr, track_color.get_red_p (), track_color.get_green_p (), track_color.get_blue_p (), alpha);
		cairo_set_line_width (cr, track_height - 1);
		cairo_move_to (cr, group ? 5 : 0, y + track_height / 2);
		cairo_line_to (cr, get_width(), y + track_height / 2);
		cairo_stroke (cr);

		/* Group color */

		if (group) {
			group_color = Gtkmm2ext::gdk_color_from_rgba(group->rgba());

			cairo_set_source_rgba (cr, group_color.get_red_p (), group_color.get_green_p (), group_color.get_blue_p (), 1);
			cairo_set_line_width (cr, track_height + 1 );
			cairo_move_to (cr, 0, y + track_height / 2);
			cairo_line_to (cr, 4, y + track_height / 2);
			cairo_stroke (cr);
		}

		/* Group separator */

		if (group != prev_group) {
			Gtkmm2ext::set_source_rgba (cr, _background_color);
			cairo_set_line_width (cr, 1);
			cairo_move_to (cr, 0, y);
			cairo_line_to (cr, 4, y);
			cairo_stroke (cr);
			prev_group = group;
		}

		y += track_height;
	}

	cairo_pop_group_to_source (cr);
	cairo_paint_with_alpha (cr, 0.75);

	cairo_destroy (cr);
}

/** Render the required regions to a cairo context.
 *  @param cr Context.
 */
void
EditorVSummary::render (Cairo::RefPtr<Cairo::Context> const& ctx, cairo_rectangle_t*)
{
	cairo_t* cr = ctx->cobj();

	if (_session == 0) {
		return;
	}

	/* draw the background (regions, markers, etc) if they've changed */
	if (!_image || _background_dirty) {
		render_background_image ();
		_background_dirty = false;
	}

	/* Fill with the background image */

	cairo_rectangle (cr, 0, 0, get_width(), get_height());
	cairo_set_source_surface (cr, _image, 0, 0);
	cairo_fill (cr);

	/* Render the view rectangle.  If there is an editor visual pending, don't update
	 * the view rectangle now --- wait until the expose event that we'll get after
	 * the visual change.  This prevents a flicker.
	 */

	if (_editor.pending_visual_change.idle_handler_id < 0) {
		get_editor (&_view_rectangle);
	}

	int32_t height = _view_rectangle.second - _view_rectangle.first;
	cairo_rectangle (cr, 0, _view_rectangle.first, get_width(), height);
	Gtkmm2ext::set_source_rgba (cr, _viewrect_color);
	cairo_fill (cr);

	cairo_rectangle (cr, 0.5, (int) (_view_rectangle.first) + 0.5, get_width () - 1, (int)height);
	cairo_set_line_width (cr, 1);
	cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(_viewrect_color), UINT_RGBA_G_FLT(_viewrect_color), UINT_RGBA_B_FLT(_viewrect_color), 0.5);
	cairo_stroke (cr);

}

void
EditorVSummary::set_colors ()
{
	_viewrect_color = UIConfiguration::instance().color_mod ("summary: view rect", "summary view rect alpha");

	_background_color = UIConfiguration::instance().color ("summary: base");
	_basetrack_color = UIConfiguration::instance().color ("gtk_background");
	_audiotrack_color = UIConfiguration::instance().color ("gtk_audio_track");
	_miditrack_color = UIConfiguration::instance().color ("gtk_midi_track");
	_bus_color = UIConfiguration::instance().color ("gtk_audio_bus");
	_vca_color = UIConfiguration::instance().color ("gtk_control_master");
}


void
EditorVSummary::set_background_dirty ()
{
	if (!_background_dirty) {
		_background_dirty = true;
		set_dirty ();
	}
}

/** Set the summary so that just the overlays (viewbox) will be re-rendered */
void
EditorVSummary::set_overlays_dirty ()
{
	ENSURE_GUI_THREAD (*this, &EditorVSummary::set_overlays_dirty);
	queue_draw ();
}



/** Fill in y with the editor's current viewable area in summary coordinates */
void
EditorVSummary::get_editor (pair<double, double>* y) const
{
	y->first = editor_y_to_summary (_editor.vertical_adjustment.get_value ());
	y->second = editor_y_to_summary (_editor.vertical_adjustment.get_value () + _editor.visible_canvas_height() - _editor.get_trackview_group()->canvas_origin().y);
}

/** Handle a button press.
 *  @param ev GTK event.
 */
bool
EditorVSummary::on_button_press_event (GdkEventButton* ev)
{

	if (ev->button != 1) {
		return true;
	}

	_last_editor_y = -1;

	/* scroll to y position if clicking outside the view rect */
	if (ev->y < _view_rectangle.first || ev->y > _view_rectangle.second) {
		/* center view rect on mouse */
		int target_y = ev->y - (_view_rectangle.second - _view_rectangle.first) / 2;
		double h = _view_rectangle.second - _view_rectangle.first;
		if (target_y < 0) {
			target_y = 0;
		} else if (target_y + h > get_height()) {
			target_y = get_height() - h;
		}
		set_editor(target_y);
		/* set drag reference with new view rect coordinate */
		_offset_y = target_y - ev->y;
	} else {
		/* set drag reference with current view rect coordinate */
		_offset_y = _view_rectangle.first - ev->y;
	}


	/* start dragging */
	_last_y = ev->y;
	_move_dragging = true;
	get_window()->set_cursor (*_editor._cursors->expand_up_down);

	return true;
}

bool
EditorVSummary::on_motion_notify_event (GdkEventMotion* ev)
{
	if (_move_dragging && ev->y != _last_y) {
		set_editor (ev->y + _offset_y);
		_last_y = ev->y;
	}

	return true;
}

bool
EditorVSummary::on_button_release_event (GdkEventButton* ev)
{
	_move_dragging = false;
	get_window()->set_cursor ();
	return true;
}

/** Set the top of the y range visible in the editor.
 *  @param y new y left position in summary coordinates.
 */
void
EditorVSummary::set_editor (double y)
{
	if (_editor.pending_visual_change.idle_handler_id >= 0 && _editor.pending_visual_change.being_handled == true) {
		/* See comment in EditorVummary::set_editor */
		return;
	}

	double h = _view_rectangle.second - _view_rectangle.first;

	if (y < 0) {
		y = 0;
	} else if (y + h > get_height()) {
		y = get_height() - h;
	}

	double editor_y = y / get_height() * _editor_scroll_height;

	if (editor_y != _last_editor_y) {
		_editor.scroll_to_track_at_y (editor_y);
		_last_editor_y = editor_y;
	}

}

double
EditorVSummary::editor_y_to_summary (double y) const
{
	return y / (_editor_scroll_height) * get_height ();
}
