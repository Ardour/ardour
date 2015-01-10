/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#include <cmath>
#include <stdio.h>
#include "utils.h"
#include "waves_zoom_control.h"
#include "i18n.h"
#include "dbg_msg.h"

using namespace ARDOUR_UI_UTILS;
WavesZoomControl::WavesZoomControl (Gtk::Adjustment& adjustment)
	: _adjustment (adjustment)
	, _state (StateIdle)
	, _begin_motion_zoom (1)
	, _begin_motion_y (0)
	, _state_idle_pixbuf (get_icon ("wave_zoom_control"))
	, _state_sliding_pixbuf (get_icon ("wave_zoom_control_sliding"))
	, _state_increasing_zoom_pixbuf (get_icon ("wave_zoom_control_increasing_zoom"))
	, _state_decreasing_zoom_pixbuf (get_icon ("wave_zoom_control_decreasing_zoom"))

{
}

WavesZoomControl::~WavesZoomControl()
{
}

void
WavesZoomControl::render (cairo_t* cr, cairo_rectangle_t*)
{
	Glib::RefPtr<Gdk::Pixbuf> pixbuf;

	switch (_state) {
	case StateIdle:
	case StateButtonUpLeft:
	case StateButtonDownLeft:
		pixbuf = _state_idle_pixbuf;
		break;

	case StateSliding:
	{
		double current_zoom = _adjustment.get_value ();
		if (current_zoom < _begin_motion_zoom) {
			pixbuf = _state_decreasing_zoom_pixbuf;
		} else if (current_zoom > _begin_motion_zoom) {
			pixbuf = _state_increasing_zoom_pixbuf;
		} else {
			pixbuf = _state_sliding_pixbuf;
		}
		break;
	}
	case StateButtonUpActive:
		pixbuf = _state_increasing_zoom_pixbuf;
		break;

	case StateButtonDownActive:
		pixbuf = _state_decreasing_zoom_pixbuf;
		break;

	default:
		dbg_msg("WavesZoomControl::render () : Unexpected state of WavesZoomControl!");
		break;
	}

	if (pixbuf) {
		double x = (get_width() - pixbuf->get_width())/2.0;
		double y = (get_height() - pixbuf->get_height())/2.0;

		cairo_rectangle (cr, x, y, pixbuf->get_width(), pixbuf->get_height());
		gdk_cairo_set_source_pixbuf (cr, pixbuf->gobj(), x, y);
		cairo_fill (cr);
	}
}

void
WavesZoomControl::on_size_request (Gtk::Requisition* req)
{
	CairoWidget::on_size_request (req);

	req->width = _state_idle_pixbuf-get_width ();
	req->height = _state_idle_pixbuf-get_height ();
}

bool
WavesZoomControl::on_button_press_event (GdkEventButton *ev)
{
	if (ev->type == GDK_2BUTTON_PRESS) {
		; //pending feature
	} else {

		ControlArea area = _area_by_point (ev->x, ev->y);

		switch (area) {
		case Nothing:
			break;

		case SlidingArea:
			_state = StateSliding;
			_begin_motion_zoom = _adjustment.get_value ();
			_begin_motion_y = ev->y;
			queue_draw ();
			break;

		case ButtonUp:
			_state = StateButtonUpActive;
			queue_draw ();
			break;

		case ButtonDown:
			_state = StateButtonDownActive;
			queue_draw ();
			break;

		default:
			dbg_msg("WavesZoomControl::on_button_press_event () : Unexpected area inside of WavesZoomControl!");
			break;
		}
	}
	return true;
}

bool
WavesZoomControl::on_button_release_event (GdkEventButton *ev)
{
	switch (_state) {
	case StateIdle:
	case StateButtonUpLeft:
	case StateButtonDownLeft:
	case StateSliding:
		break;

	case StateButtonUpActive:
		_adjustment.set_value (std::min (_adjustment.get_value () + _adjustment.get_page_size (),
										 _adjustment.get_upper ()));
		break;

	case StateButtonDownActive:
		_adjustment.set_value (std::max (_adjustment.get_value () - _adjustment.get_page_size (), 
										 _adjustment.get_lower ()));
		break;

	default:
		dbg_msg("WavesZoomControl::on_button_release_event () : Unexpected state of WavesZoomControl!");
		break;
	}


	if (_state != StateIdle) {
		queue_draw ();
	}

	_state = StateIdle;
	return true;
}

bool
WavesZoomControl::on_motion_notify_event (GdkEventMotion* ev)
{
	switch (_state) {
	case StateIdle:
	case StateButtonUpLeft:
	case StateButtonDownLeft:
	case StateButtonUpActive:
	case StateButtonDownActive:
		break;
	case StateSliding:
	{
		double zoom_factor = std::max (std::min (_begin_motion_zoom + _adjustment.get_step_increment () * (_begin_motion_y - ev->y),
												 _adjustment.get_upper ()),
									   _adjustment.get_lower ());
		_adjustment.set_value (zoom_factor);
		queue_draw ();
		break;
	}
	default:
		dbg_msg("WavesZoomControl::on_motion_notify_event () : Unexpected state of WavesZoomControl!");
		break;
	}
	return true;
}

WavesZoomControl::ControlArea
WavesZoomControl::_area_by_point (int x, int y)
{
	int width = get_allocation().get_width ();
	int height = get_allocation().get_height ();

	if ((x < 0) || (y < 0) ||
		(x > width) || (y > height)) {
			return Nothing;
	}

	if ((x > 3) && (x < 14)) {
		if ( y < 13) {
			return ButtonUp;
		}
		if (y > 13) {
			return ButtonDown;
		}
	}

	return SlidingArea;
}
