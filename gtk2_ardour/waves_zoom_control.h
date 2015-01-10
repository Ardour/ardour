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

#ifndef __gtk2_waves_zoom_control_h__
#define __gtk2_waves_zoom_control_h__

#include <gtkmm/adjustment.h>
#include "gtkmm2ext/cairo_widget.h"

class WavesZoomControl : public CairoWidget
{
  public:
	WavesZoomControl (Gtk::Adjustment& adjustment);
	virtual ~WavesZoomControl ();

	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);

  protected:
	void render (cairo_t *, cairo_rectangle_t*);
	void on_size_request (Gtk::Requisition* req);
	bool on_motion_notify_event (GdkEventMotion*);
 
private:

	Gtk::Adjustment& _adjustment;

	enum State {
		StateIdle = 0,
		StateSliding,
		StateButtonUpActive,
		StateButtonUpLeft,
		StateButtonDownActive,
		StateButtonDownLeft
	} _state;

	enum ControlArea {
		Nothing = 0,
		SlidingArea,
		ButtonUp,
		ButtonDown
	};

	double _begin_motion_zoom;
	double _begin_motion_y;

	Glib::RefPtr<Gdk::Pixbuf>   _state_idle_pixbuf;
	Glib::RefPtr<Gdk::Pixbuf>   _state_sliding_pixbuf;
	Glib::RefPtr<Gdk::Pixbuf>   _state_increasing_zoom_pixbuf;
	Glib::RefPtr<Gdk::Pixbuf>   _state_decreasing_zoom_pixbuf;

	ControlArea _area_by_point (int x, int y);
};

#endif /* __gtk2_waves_zoom_control_h__ */
