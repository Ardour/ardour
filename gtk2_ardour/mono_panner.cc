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
#include "mono_panner.h"
#include "rgb_macros.h"
#include "utils.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

static const int pos_box_size = 9;
static const int lr_box_size = 15;
static const int step_down = 10;
static const int top_step = 2;

MonoPanner::ColorScheme MonoPanner::colors;
bool MonoPanner::have_colors = false;

MonoPanner::MonoPanner (boost::shared_ptr<PBD::Controllable> position)
        : position_control (position)
        , dragging (false)
        , drag_start_x (0)
        , last_drag_x (0)
        , accumulated_delta (0)
        , detented (false)
        , drag_data_window (0)
        , drag_data_label (0)
        , position_binder (position)
{
        if (!have_colors) {
                set_colors ();
                have_colors = true;
        }

        position_control->Changed.connect (connections, invalidator(*this), boost::bind (&MonoPanner::value_change, this), gui_context());

        set_flags (Gtk::CAN_FOCUS);

        add_events (Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK|
                    Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK|
                    Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|
                    Gdk::SCROLL_MASK|
                    Gdk::POINTER_MOTION_MASK);

        ColorsChanged.connect (sigc::mem_fun (*this, &MonoPanner::color_handler));
}

MonoPanner::~MonoPanner ()
{
        delete drag_data_window;
}

void
MonoPanner::set_drag_data ()
{
        if (!drag_data_label) {
                return;
        }

        double pos = position_control->get_value(); // 0..1
        
        /* We show the position of the center of the image relative to the left & right.
           This is expressed as a pair of percentage values that ranges from (100,0) 
           (hard left) through (50,50) (hard center) to (0,100) (hard right).

           This is pretty wierd, but its the way audio engineers expect it. Just remember that
           the center of the USA isn't Kansas, its (50LA, 50NY) and it will all make sense.
        */

        drag_data_label->set_markup (string_compose (_("L:%1 R:%2"),
                                                     (int) rint (100.0 * (1.0 - pos)),
                                                     (int) rint (100.0 * pos)));
}

void
MonoPanner::value_change ()
{
        set_drag_data ();
        queue_draw ();
}

bool
MonoPanner::on_expose_event (GdkEventExpose* ev)
{
	Glib::RefPtr<Gdk::Window> win (get_window());
	Glib::RefPtr<Gdk::GC> gc (get_style()->get_base_gc (get_state()));

        cairo_t* cr = gdk_cairo_create (win->gobj());
       
        int width, height;
        double pos = position_control->get_value (); /* 0..1 */
        uint32_t o, f, t, b, pf, po;

        width = get_width();
        height = get_height ();

        o = colors.outline;
        f = colors.fill;
        t = colors.text;
        b = colors.background;
        pf = colors.pos_fill;
        po = colors.pos_outline;

        /* background */

        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(b), UINT_RGBA_G_FLT(b), UINT_RGBA_B_FLT(b), UINT_RGBA_A_FLT(b));
        cairo_rectangle (cr, 0, 0, width, height);
        cairo_fill (cr);

        double usable_width = width - pos_box_size;

        /* compute the centers of the L/R boxes based on the current stereo width */

        if (fmod (usable_width,2.0) == 0) {
                /* even width, but we need odd, so that there is an exact center.
                   So, offset cairo by 1, and reduce effective width by 1 
                */
                usable_width -= 1.0;
                cairo_translate (cr, 1.0, 0.0);
        }

        const double half_lr_box = lr_box_size/2.0;
        double left;
        double right;

        left = 4 + half_lr_box; // center of left box
        right = width  - 4 - half_lr_box; // center of right box

        /* center line */
        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(o), UINT_RGBA_G_FLT(o), UINT_RGBA_B_FLT(o), UINT_RGBA_A_FLT(o));
        cairo_set_line_width (cr, 1.0);
        cairo_move_to (cr, (pos_box_size/2.0) + (usable_width/2.0), 0);
        cairo_line_to (cr, (pos_box_size/2.0) + (usable_width/2.0), height);
        cairo_stroke (cr);
        
        /* left box */

        cairo_rectangle (cr, 
                         left - half_lr_box,
                         half_lr_box+step_down, 
                         lr_box_size, lr_box_size);
        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(o), UINT_RGBA_G_FLT(o), UINT_RGBA_B_FLT(o), UINT_RGBA_A_FLT(o));
        cairo_stroke_preserve (cr);
        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(f), UINT_RGBA_G_FLT(f), UINT_RGBA_B_FLT(f), UINT_RGBA_A_FLT(f));
	cairo_fill (cr);
        
        /* add text */

        cairo_move_to (cr, 
                       left - half_lr_box + 3,
                       (lr_box_size/2) + step_down + 13);
        cairo_select_font_face (cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(t), UINT_RGBA_G_FLT(t), UINT_RGBA_B_FLT(t), UINT_RGBA_A_FLT(t));
        cairo_show_text (cr, _("L"));

        /* right box */

        cairo_rectangle (cr, 
                         right - half_lr_box,
                         half_lr_box+step_down, 
                         lr_box_size, lr_box_size);
        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(o), UINT_RGBA_G_FLT(o), UINT_RGBA_B_FLT(o), UINT_RGBA_A_FLT(o));
        cairo_stroke_preserve (cr);
        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(f), UINT_RGBA_G_FLT(f), UINT_RGBA_B_FLT(f), UINT_RGBA_A_FLT(f));
	cairo_fill (cr);

        /* add text */

        cairo_move_to (cr, 
                       right - half_lr_box + 3,
                       (lr_box_size/2)+step_down + 13);
        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(t), UINT_RGBA_G_FLT(t), UINT_RGBA_B_FLT(t), UINT_RGBA_A_FLT(t));
        cairo_show_text (cr, _("R"));

        /* 2 lines that connect them both */
        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(o), UINT_RGBA_G_FLT(o), UINT_RGBA_B_FLT(o), UINT_RGBA_A_FLT(o));
        cairo_set_line_width (cr, 1.0);
        cairo_move_to (cr, left + half_lr_box, half_lr_box+step_down);
        cairo_line_to (cr, right - half_lr_box, half_lr_box+step_down);
        cairo_stroke (cr);


        cairo_move_to (cr, left + half_lr_box, half_lr_box+step_down+lr_box_size);
        cairo_line_to (cr, right - half_lr_box, half_lr_box+step_down+lr_box_size);
        cairo_stroke (cr);

        /* draw the position indicator */

        double spos = (pos_box_size/2.0) + (usable_width * pos);

        cairo_set_line_width (cr, 2.0);
	cairo_move_to (cr, spos + (pos_box_size/2.0), top_step); /* top right */
        cairo_rel_line_to (cr, 0.0, pos_box_size); /* lower right */
        cairo_rel_line_to (cr, -pos_box_size/2.0, 4.0); /* bottom point */
        cairo_rel_line_to (cr, -pos_box_size/2.0, -4.0); /* lower left */
        cairo_rel_line_to (cr, 0.0, -pos_box_size); /* upper left */
        cairo_close_path (cr);


        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(po), UINT_RGBA_G_FLT(po), UINT_RGBA_B_FLT(po), UINT_RGBA_A_FLT(po));
        cairo_stroke_preserve (cr);
        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(pf), UINT_RGBA_G_FLT(pf), UINT_RGBA_B_FLT(pf), UINT_RGBA_A_FLT(pf));
	cairo_fill (cr);

        /* marker line */

        cairo_set_line_width (cr, 1.0);
        cairo_move_to (cr, spos, pos_box_size+4);
        cairo_rel_line_to (cr, 0, height - (pos_box_size+4));
        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(po), UINT_RGBA_G_FLT(po), UINT_RGBA_B_FLT(po), UINT_RGBA_A_FLT(po));
        cairo_stroke (cr);

        /* done */

        cairo_destroy (cr);
	return true;
}

bool
MonoPanner::on_button_press_event (GdkEventButton* ev)
{
        drag_start_x = ev->x;
        last_drag_x = ev->x;
        
        dragging = false;
        accumulated_delta = 0;
        detented = false;

        /* Let the binding proxies get first crack at the press event
         */

        if (ev->y < 20) {
                if (position_binder.button_press_handler (ev)) {
                        return true;
                }
        }
        
        if (ev->button != 1) {
                return false;
        }

        if (ev->type == GDK_2BUTTON_PRESS) {
                int width = get_width();

                if (Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier)) {
                        /* handled by button release */
                        return true;
                }

                        
                if (ev->x <= width/3) {
                        /* left side dbl click */
                        position_control->set_value (0);
                } else if (ev->x > 2*width/3) {
                        position_control->set_value (1.0);
                } else {
                        position_control->set_value (0.5);
                }

                dragging = false;

        } else if (ev->type == GDK_BUTTON_PRESS) {

                if (Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier)) {
                        /* handled by button release */
                        return true;
                } 

                dragging = true;
                StartGesture ();
        }

        return true;
}

bool
MonoPanner::on_button_release_event (GdkEventButton* ev)
{
        if (ev->button != 1) {
                return false;
        }

        dragging = false;
        accumulated_delta = 0;
        detented = false;
        
        if (drag_data_window) {
                drag_data_window->hide ();
        }
        
        if (Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier)) {
                /* reset to default */
                position_control->set_value (0.5);
        } else {
                StopGesture ();
        }

        return true;
}

bool
MonoPanner::on_scroll_event (GdkEventScroll* ev)
{
        double one_degree = 1.0/180.0; // one degree as a number from 0..1, since 180 degrees is the full L/R axis
        double pv = position_control->get_value(); // 0..1.0 ; 0 = left
        double step;
        
        if (Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier)) {
                step = one_degree;
        } else {
                step = one_degree * 5.0;
        }

        switch (ev->direction) {
        case GDK_SCROLL_UP:
        case GDK_SCROLL_LEFT:
                pv -= step;
                position_control->set_value (pv);
                break;
        case GDK_SCROLL_DOWN:
        case GDK_SCROLL_RIGHT:
                pv += step;
                position_control->set_value (pv);
                break;
        }

        return true;
}

bool
MonoPanner::on_motion_notify_event (GdkEventMotion* ev)
{
        if (!dragging) {
                return false;
        }

        if (!drag_data_window) {
                drag_data_window = new Window (WINDOW_POPUP);
                drag_data_window->set_position (WIN_POS_MOUSE);
                drag_data_window->set_decorated (false);
                
                drag_data_label = manage (new Label);
                drag_data_label->set_use_markup (true);

                drag_data_window->set_border_width (6);
                drag_data_window->add (*drag_data_label);
                drag_data_label->show ();
                
                Window* toplevel = dynamic_cast<Window*> (get_toplevel());
                if (toplevel) {
                        drag_data_window->set_transient_for (*toplevel);
                }
        }

        if (!drag_data_window->is_visible ()) {
                /* move the window a little away from the mouse */
                drag_data_window->move (ev->x_root+30, ev->y_root+30);
                drag_data_window->present ();
        }

        int w = get_width();
        double delta = (ev->x - last_drag_x) / (double) w;
        
        /* create a detent close to the center */
        
        if (!detented && ARDOUR::Panner::equivalent (position_control->get_value(), 0.5)) {
                detented = true;
                /* snap to center */
                position_control->set_value (0.5);
        }
        
        if (detented) {
                accumulated_delta += delta;
                
                /* have we pulled far enough to escape ? */
                
                if (fabs (accumulated_delta) >= 0.025) {
                        position_control->set_value (position_control->get_value() + accumulated_delta);
                        detented = false;
                        accumulated_delta = false;
                }
        } else {
                double pv = position_control->get_value(); // 0..1.0 ; 0 = left
                position_control->set_value (pv + delta);
        }

        last_drag_x = ev->x;
        return true;
}

bool
MonoPanner::on_key_press_event (GdkEventKey* ev)
{
        double one_degree = 1.0/180.0;
        double pv = position_control->get_value(); // 0..1.0 ; 0 = left
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
        case GDK_Left:
                pv -= step;
                position_control->set_value (pv);
                break;
        case GDK_Right:
                pv += step;
                position_control->set_value (pv);
                break;
        default: 
                return false;
        }
                
        return true;
}

bool
MonoPanner::on_key_release_event (GdkEventKey* ev)
{
        return false;
}

bool
MonoPanner::on_enter_notify_event (GdkEventCrossing* ev)
{
	grab_focus ();
	Keyboard::magic_widget_grab_focus ();
	return false;
}

bool
MonoPanner::on_leave_notify_event (GdkEventCrossing*)
{
	Keyboard::magic_widget_drop_focus ();
	return false;
}

void
MonoPanner::set_colors ()
{
        colors.fill = ARDOUR_UI::config()->canvasvar_MonoPannerFill.get();
        colors.outline = ARDOUR_UI::config()->canvasvar_MonoPannerOutline.get();
        colors.text = ARDOUR_UI::config()->canvasvar_MonoPannerText.get();
        colors.background = ARDOUR_UI::config()->canvasvar_MonoPannerBackground.get();
        colors.pos_outline = ARDOUR_UI::config()->canvasvar_MonoPannerPositionOutline.get();
        colors.pos_fill = ARDOUR_UI::config()->canvasvar_MonoPannerPositionFill.get();
}

void
MonoPanner::color_handler ()
{
        set_colors ();
        queue_draw ();
}
