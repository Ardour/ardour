/*
    Copyright (C) 2000-2007 Paul Davis

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

#include <iostream>
#include <iomanip>
#include <cstring>

#include "pbd/controllable.h"

#include "gtkmm2ext/gui_thread.h"

#include "ardour/panner.h"
#include "stereo_panner.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;

StereoPanner::StereoPanner (boost::shared_ptr<PBD::Controllable> position, boost::shared_ptr<PBD::Controllable> width)
        : position_control (position)
        , width_control (width)
        , dragging (false)
        , dragging_position (false)
        , drag_start_x (0)
        , last_drag_x (0)
{
        set_size_request (-1, 15);

        position_control->Changed.connect (connections, invalidator(*this), boost::bind (&DrawingArea::queue_draw, this), gui_context());
        width_control->Changed.connect (connections, invalidator(*this), boost::bind (&DrawingArea::queue_draw, this), gui_context());
        
        add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::SCROLL_MASK|Gdk::POINTER_MOTION_MASK);
}

StereoPanner::~StereoPanner ()
{
}

bool
StereoPanner::on_expose_event (GdkEventExpose* ev)
{
	Glib::RefPtr<Gdk::Window> win (get_window());
	Glib::RefPtr<Gdk::GC> gc (get_style()->get_base_gc (get_state()));

        cairo_t* cr = gdk_cairo_create (win->gobj());
       
        int x1, x2;
        int width, height;
        double pos = position_control->get_value (); /* 0..1 */
        double swidth = width_control->get_value (); /* -1..+1 */
        const int pos_box_size = 5;

        width = get_width();
        height = get_height ();

        /* compute where the central box is */

        x1 = (int) floor (width * pos);
        x1 -= pos_box_size/2;

	cairo_set_source_rgb (cr, 255, 0, 0);
	cairo_rectangle (cr, x1, 4, pos_box_size, pos_box_size);
	cairo_fill (cr);

        /* compute & draw the line through the box */

        x2 = x1 - (int) floor ((fabs (swidth) * width)/2.0); // center, then back up half the swidth value

	cairo_set_source_rgb (cr, 0, 255, 0);
        cairo_move_to (cr, x2, 4+(pos_box_size/2));
        cairo_line_to (cr, x2 + floor ((fabs (swidth * width))), 4+(pos_box_size/2));
        cairo_stroke (cr);

        cairo_destroy (cr);
	return true;
}

bool
StereoPanner::on_button_press_event (GdkEventButton* ev)
{
        drag_start_x = ev->x;
        last_drag_x = ev->x;

        /* center 8 pixels are for position drag */

        int w = get_width();
        double pos = position_control->get_value ();

        if ((ev->x >= (int) floor ((pos * w)-4)) && (ev->x <= (int) floor ((pos * w)+4))) {
                dragging_position = true;
        } else {
                dragging_position = false;
        }

        if (ev->type == GDK_2BUTTON_PRESS) {
                if (dragging_position) {
                        cerr << "Reset pos\n";
                        position_control->set_value (0.5); // reset position to center
                } else {
                        cerr << "Reset width\n";
                        width_control->set_value (1.0); // reset position to full, LR
                }
                dragging = false;
        } else {
                dragging = true;
        }

        return true;
}

bool
StereoPanner::on_button_release_event (GdkEventButton* ev)
{
        dragging = false;
        dragging_position = false;
        return true;
}

bool
StereoPanner::on_motion_notify_event (GdkEventMotion* ev)
{
        if (!dragging) {
                return false;
        }

        int w = get_width();
        float delta = (abs (ev->x - last_drag_x)) / (double) (w/2);

        if (!dragging_position) {
                double wv = width_control->get_value();        
                
                if (((drag_start_x < w/2) && ev->x > last_drag_x) || // start left of center, move towards it
                    ((drag_start_x > w/2) && ev->x < last_drag_x)) { // start right of center, move towards it
                        wv = wv  * (1.0 - delta);
                } else {
                        /* moving out, so increase the width */
                        wv = wv * (1.0 + delta);
                }
                
                width_control->set_value (wv);

        } else {

                double pv = position_control->get_value(); // 0..1.0 ; 0 = left
                
                if (ev->x > last_drag_x) { // increasing 
                        pv = pv * (1.0 + delta);
                } else {
                        pv = pv * (1.0 - delta);
                }

                position_control->set_value (pv);
        }

        last_drag_x = ev->x;
        return true;
}
