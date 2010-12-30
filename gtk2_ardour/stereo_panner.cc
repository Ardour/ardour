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
        , dragging_left (false)
        , dragging_right (false)
        , drag_start_x (0)
        , last_drag_x (0)
        , accumulated_delta (0)
        , detented (false)
        , drag_data_window (0)
        , drag_data_label (0)
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
        delete drag_data_window;
}

void
StereoPanner::set_tooltip ()
{
        Gtkmm2ext::UI::instance()->set_tip (this, 
                                            string_compose (_("0 -> set width to zero (mono)\n%1-uparrow -> set width to 100\n%1-downarrow -> set width to -100"), 
                                                            
                                                            Keyboard::secondary_modifier_name()).c_str());
}

void
StereoPanner::unset_tooltip ()
{
        Gtkmm2ext::UI::instance()->set_tip (this, "");
}

void
StereoPanner::set_drag_data ()
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

        drag_data_label->set_markup (string_compose (_("L:%1 R:%2 Width: %3%%"),
                                                     (int) rint (100.0 * (1.0 - pos)),
                                                     (int) rint (100.0 * pos),
                                                     (int) floor (100.0 * width_control->get_value())));
}

void
StereoPanner::value_change ()
{
        set_drag_data ();
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

        /* the usable width is reduced from the real width, because we need space for 
           the two halves of LR boxes that will extend past the actual left/right
           positions (indicated by the vertical line segment above them).
        */

        double usable_width = width - lr_box_size;

        /* compute the centers of the L/R boxes based on the current stereo width */

        if (fmod (usable_width,2.0) == 0) {
                /* even width, but we need odd, so that there is an exact center.
                   So, offset cairo by 1, and reduce effective width by 1 
                */
                usable_width -= 1.0;
                cairo_translate (cr, 1.0, 0.0);
        }

        double center = (lr_box_size/2.0) + (usable_width * pos);
        const double pan_spread = (fswidth * usable_width)/2.0;
        const double half_lr_box = lr_box_size/2.0;
        int left;
        int right;

        left = center - pan_spread;  // center of left box
        right = center + pan_spread; // center of right box

        /* compute & draw the line through the box */
        
        cairo_set_line_width (cr, 2);
        cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(o), UINT_RGBA_G_FLT(o), UINT_RGBA_B_FLT(o), UINT_RGBA_A_FLT(o));
        cairo_move_to (cr, left, top_step+(pos_box_size/2)+step_down);
        cairo_line_to (cr, left, top_step+(pos_box_size/2));
        cairo_line_to (cr, right, top_step+(pos_box_size/2));
        cairo_line_to (cr, right, top_step+(pos_box_size/2) + step_down);
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

        if (state != Mono) {
                cairo_set_source_rgba (cr, UINT_RGBA_R_FLT(t), UINT_RGBA_G_FLT(t), UINT_RGBA_B_FLT(t), UINT_RGBA_A_FLT(t));
                if (swidth < 0.0) {
                        cairo_show_text (cr, _("R"));
                } else {
                        cairo_show_text (cr, _("L"));
                }
        }

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

        if (state == Mono) {
                cairo_show_text (cr, _("M"));
        } else {
                if (swidth < 0.0) {
                        cairo_show_text (cr, _("L"));
                } else {
                        cairo_show_text (cr, _("R"));
                }
        }

        /* draw the central box */

        cairo_set_line_width (cr, 1);
	cairo_rectangle (cr, lrint (center - (pos_box_size/2.0)), top_step, pos_box_size, pos_box_size);
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
        
        dragging_position = false;
        dragging_left = false;
        dragging_right = false;
        dragging = false;
        accumulated_delta = 0;
        detented = false;

        if (ev->button != 1) {
                return false;
        }

        if (ev->type == GDK_2BUTTON_PRESS) {
                int width = get_width();

                if (Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier)) {
                        /* handled by button release */
                        return true;
                }

                if (ev->y < 20) {
                        
                        /* upper section: adjusts position, constrained by width */

                        const double w = width_control->get_value ();
                        const double max_pos = 1.0 - (w/2.0);
                        const double min_pos = w/2.0;

                        if (ev->x <= width/3) {
                                /* left side dbl click */
                                if (Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier)) {
                                        /* 2ndary-double click on left, collapse to hard left */
                                        width_control->set_value (0);
                                        position_control->set_value (0);
                                } else {
                                        position_control->set_value (min_pos);
                                }
                        } else if (ev->x > 2*width/3) {
                                if (Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier)) {
                                        /* 2ndary-double click on right, collapse to hard right */
                                        width_control->set_value (0);
                                        position_control->set_value (1.0);
                                }
                                position_control->set_value (max_pos);
                        } else {
                                position_control->set_value (0.5);
                        }

                } else {

                        /* lower section: adjusts width, constrained by position */

                        const double p = position_control->get_value ();
                        const double max_width = 2.0 * min ((1.0 - p), p);

                        if (ev->x <= width/3) {
                                /* left side dbl click */
                                width_control->set_value (max_width); // reset width to 100%
                        } else if (ev->x > 2*width/3) {
                                /* right side dbl click */
                                width_control->set_value (-max_width); // reset width to inverted 100%
                        } else {
                                /* center dbl click */
                                width_control->set_value (0); // collapse width to 0%
                        }
                }

                dragging = false;

        } else if (ev->type == GDK_BUTTON_PRESS) {

                if (Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier)) {
                        /* handled by button release */
                        return true;
                }

                if (ev->y < 20) {
                        /* top section of widget is for position drags */
                        dragging_position = true;
                } else {
                        /* lower section is for dragging width */
                        
                        double pos = position_control->get_value (); /* 0..1 */
                        double swidth = width_control->get_value (); /* -1..+1 */
                        double fswidth = fabs (swidth);
                        int usable_width = get_width() - lr_box_size;
                        double center = (lr_box_size/2.0) + (usable_width * pos);
                        int left = lrint (center - (fswidth * usable_width / 2.0)); // center of leftmost box
                        int right = lrint (center +  (fswidth * usable_width / 2.0)); // center of rightmost box
                        const int half_box = lr_box_size/2;
                        
                        if (ev->x >= (left - half_box) && ev->x < (left + half_box)) {
                                dragging_left = true;
                        } else if (ev->x >= (right - half_box) && ev->x < (right + half_box)) {
                                dragging_right = true;
                        }
                        
                }

                dragging = true;
        }

        return true;
}

bool
StereoPanner::on_button_release_event (GdkEventButton* ev)
{
        if (ev->button != 1) {
                return false;
        }

        dragging = false;
        dragging_position = false;
        dragging_left = false;
        dragging_right = false;
        accumulated_delta = 0;
        detented = false;

        if (drag_data_window) {
                drag_data_window->hide ();
        }
        
        if (Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier)) {
                /* reset to default */
                position_control->set_value (0.5);
                width_control->set_value (1.0);
        }

        set_tooltip ();

        return true;
}

bool
StereoPanner::on_scroll_event (GdkEventScroll* ev)
{
        double one_degree = 1.0/180.0; // one degree as a number from 0..1, since 180 degrees is the full L/R axis
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
                unset_tooltip ();
        }

        int w = get_width();
        double delta = (ev->x - last_drag_x) / (double) w;
        
        if (dragging_left) {
                delta = -delta;
        }

        if (dragging_left || dragging_right) {

                /* maintain position as invariant as we change the width */

                double current_width = width_control->get_value ();

                /* create a detent close to the center */

                if (!detented && fabs (current_width) < 0.02) {
                        detented = true;
                        /* snap to zero */
                        width_control->set_value (0);
                }
                
                if (detented) {

                        accumulated_delta += delta;

                        /* have we pulled far enough to escape ? */

                        if (fabs (accumulated_delta) >= 0.025) {
                                width_control->set_value (current_width + accumulated_delta);
                                detented = false;
                                accumulated_delta = false;
                        }
                                
                } else {
                        width_control->set_value (current_width + delta);
                }

        } else if (dragging_position) {

                double pv = position_control->get_value(); // 0..1.0 ; 0 = left
                position_control->set_value (pv + delta);
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
