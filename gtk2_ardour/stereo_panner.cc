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
#include <cmath>

#include <gtkmm/window.h>

#include "pbd/controllable.h"
#include "pbd/compose.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/keyboard.h"

#include "ardour/panner.h"

#include "ardour_ui.h"
#include "global_signals.h"
#include "stereo_panner.h"
#include "rgb_macros.h"
#include "utils.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

static const int pos_box_size = 10;
static const int lr_box_size = 15;
static const int step_down = 10;
static const int top_step = 2;

StereoPanner::ColorScheme StereoPanner::colors[3];
bool StereoPanner::have_colors = false;
PBD::Signal0<void> StereoPanner::color_change;

StereoPanner::StereoPanner (boost::shared_ptr<PBD::Controllable> position, boost::shared_ptr<PBD::Controllable> width)
        : position_control (position)
        , width_control (width)
        , dragging (false)
        , dragging_position (false)
        , drag_start_x (0)
        , last_drag_x (0)
{
        if (!have_colors) {
                set_colors ();
                have_colors = true;
        }

        position_control->Changed.connect (connections, invalidator(*this), boost::bind (&StereoPanner::value_change, this), gui_context());
        width_control->Changed.connect (connections, invalidator(*this), boost::bind (&StereoPanner::value_change, this), gui_context());
        set_tooltip ();

        set_flags (Gtk::CAN_FOCUS);

        add_events (Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK|
                    Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK|
                    Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|
                    Gdk::SCROLL_MASK|
                    Gdk::POINTER_MOTION_MASK);

        color_change.connect (connections, invalidator (*this), boost::bind (&DrawingArea::queue_draw, this), gui_context());
}

StereoPanner::~StereoPanner ()
{
}

void
StereoPanner::set_tooltip ()
{
        double pos = position_control->get_value(); // 0..1
        double w = width_control->get_value (); // -1..+1
        int lpos = (int) lrint ((pos - (w/2.0)) * 100.0);
        int rpos = (int) lrint ((pos + (w/2.0)) * 100.0);
                                
        Gtkmm2ext::UI::instance()->set_tip (this, string_compose (_("L:%1 R:%2 Width: %3%%"), 
                                                                  lpos, rpos,
                                                                  (int) floor (w * 100.0)).c_str());
}

void
StereoPanner::value_change ()
{
        set_tooltip ();
        queue_draw ();
}

bool
StereoPanner::on_expose_event (GdkEventExpose* ev)
{
	Glib::RefPtr<Gdk::Window> win (get_window());
	Glib::RefPtr<Gdk::GC> gc (get_style()->get_base_gc (get_state()));

        cairo_t* cr = gdk_cairo_create (win->gobj());
       
        int width, height;
        double pos = position_control->get_value (); /* 0..1 */
        double swidth = width_control->get_value (); /* -1..+1 */
        double fswidth = fabs (swidth);
        uint32_t o, f, t, b;
        State state;

        width = get_width();
        height = get_height ();

        if (swidth == 0.0) {
                state = Mono;
        } else if (swidth < 0.0) {
                state = Inverted;
        } else { 
                state = Normal;
        }

        o = colors[state].outline;
        f = colors[state].fill;
        t = colors[state].text;
        b = colors[state].background;

        /* background */

        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(b), UINT_RGBA_G_FLT(b), UINT_RGBA_B_FLT(b), UINT_RGBA_A_FLT(b));
        cairo_rectangle (cr, 0, 0, width, height);
        cairo_fill (cr);

        /* compute the centers of the L/R boxes based on the current stereo width */
        
        int usable_width = width - lr_box_size;
        int center = lr_box_size/2 + (int) floor (usable_width * pos);
        int left = center - (int) floor (fswidth * usable_width / 2.0); // center of leftmost box
        int right = center + (int) floor (fswidth * usable_width / 2.0); // center of rightmost box

        // cerr << "pos " << pos << " width = " << width << " swidth = " << swidth << " center @ " << center << " L = " << left << " R = " << right << endl;

        /* compute & draw the line through the box */
        
        cairo_set_line_width (cr, 2);
        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(o), UINT_RGBA_G_FLT(o), UINT_RGBA_B_FLT(o), UINT_RGBA_A_FLT(o));
        cairo_move_to (cr, left, top_step+(pos_box_size/2)+step_down);
        cairo_line_to (cr, left, top_step+(pos_box_size/2));
        cairo_line_to (cr, right, top_step+(pos_box_size/2));
        cairo_line_to (cr, right, top_step+(pos_box_size/2) + step_down);
        cairo_stroke (cr);

        if (swidth < 0.0) {
                /* flip where the L/R boxes are drawn */
                swap (left, right);
        }

        /* left box */

        left -= lr_box_size/2;
        right -= lr_box_size/2;

        cairo_rectangle (cr, 
                         left,
                         (lr_box_size/2)+step_down, 
                         lr_box_size, lr_box_size);
        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(o), UINT_RGBA_G_FLT(o), UINT_RGBA_B_FLT(o), UINT_RGBA_A_FLT(o));
        cairo_stroke_preserve (cr);
        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(f), UINT_RGBA_G_FLT(f), UINT_RGBA_B_FLT(f), UINT_RGBA_A_FLT(f));
	cairo_fill (cr);
        
        /* add text */

        cairo_move_to (cr, 
                       left + 3,
                       (lr_box_size/2) + step_down + 13);
        cairo_select_font_face (cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        if (state != Mono) {
                cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(t), UINT_RGBA_G_FLT(t), UINT_RGBA_B_FLT(t), UINT_RGBA_A_FLT(t));
                cairo_show_text (cr, "L");
        }

        /* right box */

        cairo_rectangle (cr, 
                         right,
                         (lr_box_size/2)+step_down, 
                         lr_box_size, lr_box_size);
        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(o), UINT_RGBA_G_FLT(o), UINT_RGBA_B_FLT(o), UINT_RGBA_A_FLT(o));
        cairo_stroke_preserve (cr);
        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(f), UINT_RGBA_G_FLT(f), UINT_RGBA_B_FLT(f), UINT_RGBA_A_FLT(f));
	cairo_fill (cr);

        /* add text */

        cairo_move_to (cr, 
                       right + 3,
                       (lr_box_size/2)+step_down + 13);
        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(t), UINT_RGBA_G_FLT(t), UINT_RGBA_B_FLT(t), UINT_RGBA_A_FLT(t));
        if (state == Mono) {
                cairo_show_text (cr, "M");
        } else {
                cairo_show_text (cr, "R");
        }

        /* draw the central box */

        cairo_set_line_width (cr, 1);
	cairo_rectangle (cr, center - (pos_box_size/2), top_step, pos_box_size, pos_box_size);
        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(o), UINT_RGBA_G_FLT(o), UINT_RGBA_B_FLT(o), UINT_RGBA_A_FLT(o));
        cairo_stroke_preserve (cr);
        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(f), UINT_RGBA_G_FLT(f), UINT_RGBA_B_FLT(f), UINT_RGBA_A_FLT(f));
	cairo_fill (cr);

        /* done */

        cairo_destroy (cr);
	return true;
}

bool
StereoPanner::on_button_press_event (GdkEventButton* ev)
{
        drag_start_x = ev->x;
        last_drag_x = ev->x;

        if (ev->y < 20) {
                /* top section of widget is for position drags */
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
StereoPanner::on_scroll_event (GdkEventScroll* ev)
{
        double one_degree = 1.0/180.0;
        double pv = position_control->get_value(); // 0..1.0 ; 0 = left
        double wv = width_control->get_value(); // 0..1.0 ; 0 = left
        double step;
        
        if (Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier)) {
                step = one_degree;
        } else {
                step = one_degree * 5.0;
        }

        switch (ev->direction) {
        case GDK_SCROLL_LEFT:
                wv += step;
                width_control->set_value (wv);
                break;
        case GDK_SCROLL_UP:
                pv -= step;
                position_control->set_value (pv);
                break;
        case GDK_SCROLL_RIGHT:
                wv -= step;
                width_control->set_value (wv);
                break;
        case GDK_SCROLL_DOWN:
                pv += step;
                position_control->set_value (pv);
                break;
        }

        return true;
}

bool
StereoPanner::on_motion_notify_event (GdkEventMotion* ev)
{
        if (!dragging) {
                return false;
        }

        int w = get_width();
        double delta = (abs (ev->x - last_drag_x)) / (w/2.0);
        int drag_dir = 0;

        if (!dragging_position) {
                double wv = width_control->get_value();        
                int inc;
                double old_wv;
                double opx; // compute the operational x-coordinate given the current pos+width
                
                if (wv > 0) {
                        /* positive value: increasing width means adding */
                        inc = 1;
                } else {
                        /* positive value: increasing width means subtracting */
                        inc = -1;
                }

                if (drag_start_x < w/2) {
                        /* started left of center */

                        opx = position_control->get_value() - (wv/2.0);

                        if (opx < 0.5) {
                                /* still left */
                                if (ev->x > last_drag_x) {
                                        /* motion to left */
                                        drag_dir = -inc;
                                } else {
                                        drag_dir = inc;
                                }
                        } else {
                                /* now right */
                                if (ev->x > last_drag_x) {
                                        /* motion to left */
                                        drag_dir = inc;
                                } else {
                                        drag_dir = -inc;
                                }
                        }
                } else {
                        /* started right of center */
                        
                        opx = position_control->get_value() + (wv/2.0);

                        if (opx > 0.5) {
                                /* still right */
                                if (ev->x < last_drag_x) {
                                        /* motion to right */
                                        drag_dir = -inc;
                                } else {
                                        drag_dir = inc;
                                }
                        } else {
                                /* now left */
                                if (ev->x < last_drag_x) {
                                        /* motion to right */
                                        drag_dir = inc;
                                } else {
                                        drag_dir = -inc;
                                }
                        }

                }

                old_wv = wv;
                wv = wv + (drag_dir * delta);
                
                width_control->set_value (wv);

        } else {

                double pv = position_control->get_value(); // 0..1.0 ; 0 = left
                
                if (ev->x > last_drag_x) { // increasing 
                        pv = pv + delta;
                } else {
                        pv = pv - delta;
                }

                position_control->set_value (pv);
        }

        last_drag_x = ev->x;
        return true;
}

bool
StereoPanner::on_key_press_event (GdkEventKey* ev)
{
        double one_degree = 1.0/180.0;
        double pv = position_control->get_value(); // 0..1.0 ; 0 = left
        double wv = width_control->get_value(); // 0..1.0 ; 0 = left
        double step;

        if (Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier)) {
                step = one_degree;
        } else {
                step = one_degree * 5.0;
        }

        /* up/down control width because we consider pan position more "important"
           (and thus having higher "sense" priority) than width.
        */

        switch (ev->keyval) {
        case GDK_Up:
                if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {
                        width_control->set_value (1.0);
                } else {
                        width_control->set_value (wv + step);
                }
                break;
        case GDK_Down:
                if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {
                        width_control->set_value (-1.0);
                } else {
                        width_control->set_value (wv - step);
                }

        case GDK_Left:
                pv -= step;
                position_control->set_value (pv);
                break;
        case GDK_Right:
                pv += step;
                position_control->set_value (pv);
                break;

                break;
        case GDK_0:
        case GDK_KP_0:
                width_control->set_value (0.0);
                break;

        default: 
                return false;
        }
                
        return true;
}

bool
StereoPanner::on_key_release_event (GdkEventKey* ev)
{
        return false;
}

bool
StereoPanner::on_enter_notify_event (GdkEventCrossing* ev)
{
	grab_focus ();
	Keyboard::magic_widget_grab_focus ();
	return false;
}

bool
StereoPanner::on_leave_notify_event (GdkEventCrossing*)
{
	Keyboard::magic_widget_drop_focus ();
	return false;
}

void
StereoPanner::set_colors ()
{
        colors[Normal].fill = ARDOUR_UI::config()->canvasvar_StereoPannerFill.get();
        colors[Normal].outline = ARDOUR_UI::config()->canvasvar_StereoPannerOutline.get();
        colors[Normal].text = ARDOUR_UI::config()->canvasvar_StereoPannerText.get();
        colors[Normal].background = ARDOUR_UI::config()->canvasvar_StereoPannerBackground.get();

        colors[Mono].fill = ARDOUR_UI::config()->canvasvar_StereoPannerMonoFill.get();
        colors[Mono].outline = ARDOUR_UI::config()->canvasvar_StereoPannerMonoOutline.get();
        colors[Mono].text = ARDOUR_UI::config()->canvasvar_StereoPannerMonoText.get();
        colors[Mono].background = ARDOUR_UI::config()->canvasvar_StereoPannerMonoBackground.get();

        colors[Inverted].fill = ARDOUR_UI::config()->canvasvar_StereoPannerInvertedFill.get();
        colors[Inverted].outline = ARDOUR_UI::config()->canvasvar_StereoPannerInvertedOutline.get();
        colors[Inverted].text = ARDOUR_UI::config()->canvasvar_StereoPannerInvertedText.get();
        colors[Inverted].background = ARDOUR_UI::config()->canvasvar_StereoPannerInvertedBackground.get();

        color_change (); /* EMIT SIGNAL */
}
