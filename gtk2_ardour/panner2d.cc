/*
    Copyright (C) 2002 Paul Davis

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
#include <climits>
#include <string.h>

#include <cairo.h>
#include <gtkmm/menu.h>

#include "gtkmm2ext/gtk_ui.h"

#include "pbd/error.h"
#include "pbd/cartesian.h"
#include "ardour/panner.h"
#include "ardour/pannable.h"
#include "ardour/speakers.h"

#include "panner2d.h"
#include "keyboard.h"
#include "gui_thread.h"
#include "utils.h"
#include "public_editor.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;
using namespace PBD;
using Gtkmm2ext::Keyboard;

Panner2d::Target::Target (const AngularVector& a, const char *txt)
	: position (a)
	, text (txt)
	, _selected (false)
{
}

Panner2d::Target::~Target ()
{
}

void
Panner2d::Target::set_text (const char* txt)
{
	text = txt;
}

Panner2d::Panner2d (boost::shared_ptr<Panner> p, int32_t h)
	: panner (p)
        , position (AngularVector (0.0, 0.0), "")
        , width (0)
        , height (h)
        , last_width (0)
{
	panner->StateChanged.connect (connections, invalidator (*this), boost::bind (&Panner2d::handle_state_change, this), gui_context());

        panner->pannable()->pan_azimuth_control->Changed.connect (connections, invalidator(*this), boost::bind (&Panner2d::handle_position_change, this), gui_context());
        panner->pannable()->pan_width_control->Changed.connect (connections, invalidator(*this), boost::bind (&Panner2d::handle_position_change, this), gui_context());

	drag_target = 0;
	set_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::POINTER_MOTION_MASK);

        handle_position_change ();
}

Panner2d::~Panner2d()
{
	for (Targets::iterator i = targets.begin(); i != targets.end(); ++i) {
		delete *i;
	}
}

void
Panner2d::reset (uint32_t n_inputs)
{
        uint32_t nouts = panner->out().n_audio();

	/* pucks */

	while (pucks.size() < n_inputs) {
		add_puck ("", AngularVector());
	}

	if (pucks.size() > n_inputs) {
		for (uint32_t i = pucks.size(); i < n_inputs; ++i) {
			delete pucks[i];
		}

		pucks.resize (n_inputs);
	}
						
        label_pucks ();

        for (uint32_t i = 0; i < n_inputs; ++i) {
                pucks[i]->position = panner->signal_position (i);
        }

	/* add all outputs */

	while (targets.size() < nouts) {
		add_target (AngularVector());
	}

	if (targets.size() > nouts) {
		for (uint32_t i = nouts; i < targets.size(); ++i) {
			delete targets[i];
		}

		targets.resize (nouts);
	}

	for (Targets::iterator x = targets.begin(); x != targets.end(); ++x) {
		(*x)->visible = false;
	}

        vector<Speaker>& speakers (panner->get_speakers()->speakers());

	for (uint32_t n = 0; n < nouts; ++n) {
		char buf[16];

		snprintf (buf, sizeof (buf), "%d", n+1);
		targets[n]->set_text (buf);
		targets[n]->position = speakers[n].angles();
		targets[n]->visible = true;
	}

	queue_draw ();
}

void
Panner2d::on_size_allocate (Gtk::Allocation& alloc)
{
  	width = alloc.get_width();
  	height = alloc.get_height();

        /* our idea of our width/height must be "square
         */

	if (height > 100) {
		width -= 20;
		height -= 20;
	}

        dimen = min (width, height);
        
	DrawingArea::on_size_allocate (alloc);
}

int
Panner2d::add_puck (const char* text, const AngularVector& a)
{
	Target* puck = new Target (a, text);
	pucks.push_back (puck);
	puck->visible = true;

	return 0;
}

int
Panner2d::add_target (const AngularVector& a)
{
	Target* target = new Target (a, "");
	targets.push_back (target);
	target->visible = true;
	queue_draw ();

	return targets.size() - 1;
}

void
Panner2d::handle_state_change ()
{
	queue_draw ();
}

void
Panner2d::label_pucks ()
{
        double w = panner->pannable()->pan_width_control->get_value();
        uint32_t sz = pucks.size();

	switch (sz) {
	case 0:
		break;

	case 1:
		pucks[0]->set_text ("");
		break;

	case 2:
                if (w  >= 0.0) {
                        pucks[0]->set_text ("R");
                        pucks[1]->set_text ("L");
                } else {
                        pucks[0]->set_text ("L");
                        pucks[1]->set_text ("R");
                }
		break;

	default:
		for (uint32_t i = 0; i < sz; ++i) {
			char buf[64];
                        if (w >= 0.0) {
                                snprintf (buf, sizeof (buf), "%" PRIu32, i + 1);
                        } else {
                                snprintf (buf, sizeof (buf), "%" PRIu32, sz - i);
                        }
			pucks[i]->set_text (buf);
		}
		break;
	}
}

void
Panner2d::handle_position_change ()
{
	uint32_t n;
        double w = panner->pannable()->pan_width_control->get_value();

        position.position = AngularVector (panner->pannable()->pan_azimuth_control->get_value() * 360.0, 0.0);

        for (uint32_t i = 0; i < pucks.size(); ++i) {
                pucks[i]->position = panner->signal_position (i);
        }

        if (w * last_width <= 0) {
                /* changed sign */
                label_pucks ();
        }

        last_width = w;

        vector<Speaker>& speakers (panner->get_speakers()->speakers());

	for (n = 0; n < targets.size(); ++n) {
		targets[n]->position = speakers[n].angles();
	}

	queue_draw ();
}

void
Panner2d::move_puck (int which, const AngularVector& a)
{
	if (which >= int (targets.size())) {
		return;
	}
	
	targets[which]->position = a;
	queue_draw ();
}

Panner2d::Target *
Panner2d::find_closest_object (gdouble x, gdouble y)
{
	Target *closest = 0;
	Target *candidate;
	float distance;
	float best_distance = FLT_MAX;
        CartesianVector c;

        /* start with the position itself
         */

        position.position.cartesian (c);
        cart_to_gtk (c);
        best_distance = sqrt ((c.x - x) * (c.x - x) +
                         (c.y - y) * (c.y - y));
        closest = &position;
        
	for (Targets::const_iterator i = pucks.begin(); i != pucks.end(); ++i) {
		candidate = *i;

		candidate->position.cartesian (c);
		cart_to_gtk (c);

		distance = sqrt ((c.x - x) * (c.x - x) +
		                 (c.y - y) * (c.y - y));

                if (distance < best_distance) {
			closest = candidate;
			best_distance = distance;
		}
	}

        if (height > 100) {
                /* "big" */
                if (best_distance > 30) { // arbitrary 
                        return 0;
                }
        } else {
                /* "small" */
                if (best_distance > 10) { // arbitrary 
                        return 0;
                }
        }

	return closest;
}

bool
Panner2d::on_motion_notify_event (GdkEventMotion *ev)
{
	gint x, y;
	GdkModifierType state;

	if (ev->is_hint) {
		gdk_window_get_pointer (ev->window, &x, &y, &state);
	} else {
		x = (int) floor (ev->x);
		y = (int) floor (ev->y);
		state = (GdkModifierType) ev->state;
	}

	return handle_motion (x, y, state);
}

bool
Panner2d::on_expose_event (GdkEventExpose *event)
{
        CartesianVector c;
	cairo_t* cr;
        bool small;

	cr = gdk_cairo_create (get_window()->gobj());

	cairo_set_line_width (cr, 1.0);

	cairo_rectangle (cr, event->area.x, event->area.y, event->area.width, event->area.height);
	if (!panner->bypassed()) {
		cairo_set_source_rgba (cr, 0.1, 0.1, 0.1, 1.0);
	} else {
		cairo_set_source_rgba (cr, 0.1, 0.1, 0.1, 0.2);
	}
	cairo_fill_preserve (cr);
	cairo_clip (cr);

        small = !(height > 100);

	if (!small) {
		cairo_translate (cr, 10.0, 10.0);
	}

	/* horizontal line of "crosshairs" */

        cairo_set_source_rgba (cr, 0.282, 0.517, 0.662, 1.0);
	cairo_move_to (cr, 0.5, height/2.0+0.5);
	cairo_line_to (cr, width+0.5, height/2+0.5);
	cairo_stroke (cr);

	/* vertical line of "crosshairs" */
	
	cairo_move_to (cr, width/2+0.5, 0.5);
	cairo_line_to (cr, width/2+0.5, height+0.5);
	cairo_stroke (cr);

	/* the circle on which signals live */

	cairo_set_line_width (cr, 2.0);
        cairo_set_source_rgba (cr, 0.517, 0.772, 0.882, 1.0);
	cairo_arc (cr, width/2, height/2, dimen/2, 0, 2.0 * M_PI);
	cairo_stroke (cr);

	/* 3 other circles of smaller diameter circle on which signals live */

	cairo_set_line_width (cr, 1.0);
        cairo_set_source_rgba (cr, 0.282, 0.517, 0.662, 1.0);
	cairo_arc (cr, width/2, height/2, (dimen/2.0) * 0.75, 0, 2.0 * M_PI);
	cairo_stroke (cr);
        cairo_set_source_rgba (cr, 0.282, 0.517, 0.662, 0.85);
	cairo_arc (cr, width/2, height/2, (dimen/2.0) * 0.50, 0, 2.0 * M_PI);
	cairo_stroke (cr);
	cairo_arc (cr, width/2, height/2, (dimen/2.0) * 0.25, 0, 2.0 * M_PI);
	cairo_stroke (cr);

        if (pucks.size() > 1) {
                /* arc to show "diffusion" */

                double width_angle = fabs (panner->pannable()->pan_width_control->get_value()) * 2 * M_PI;
                double position_angle = (2 * M_PI) - panner->pannable()->pan_azimuth_control->get_value() * 2 * M_PI;

                cairo_save (cr);
                cairo_translate (cr, width/2, height/2);
                cairo_rotate (cr, position_angle - width_angle);
                cairo_move_to (cr, 0, 0);
                cairo_arc_negative (cr, 0, 0, dimen/2.0, width_angle * 2.0, 0.0);
                cairo_close_path (cr);
                cairo_set_source_rgba (cr, 1.0, 0.419, 0.419, 0.45);
                cairo_fill (cr);
                cairo_restore (cr);
        }

	if (!panner->bypassed()) {

		double arc_radius;

		cairo_select_font_face (cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
                
		if (small) {
			arc_radius = 4.0;
		} else {
			cairo_set_font_size (cr, 10);
			arc_radius = 12.0;
		}

                /* signals */

                if (pucks.size() > 1) {
                        for (Targets::iterator i = pucks.begin(); i != pucks.end(); ++i) {
                                Target* puck = *i;
                                
                                if (puck->visible) {
                                        
                                        puck->position.cartesian (c);
                                        cart_to_gtk (c);
                                        
                                        cairo_new_path (cr);
                                        cairo_arc (cr, c.x, c.y, arc_radius, 0, 2.0 * M_PI);
                                        cairo_set_source_rgba (cr, 0.282, 0.517, 0.662, 0.85);
                                        cairo_fill_preserve (cr);
                                        cairo_set_source_rgba (cr, 0.517, 0.772, 0.882, 1.0);
                                        cairo_stroke (cr);
                                        
                                        if (!small && !puck->text.empty()) {
                                                cairo_set_source_rgb (cr, 0.517, 0.772, 0.882);
                                                /* the +/- adjustments are a hack to try to center the text in the circle */
                                                if (small) {
                                                        cairo_move_to (cr, c.x - 1, c.y + 1);
                                                } else {
                                                        cairo_move_to (cr, c.x - 4, c.y + 4);
                                                }
                                                cairo_show_text (cr, puck->text.c_str());
                                        }
                                }
                        }
                }

                /* speakers */

		int n = 0;

		for (Targets::iterator i = targets.begin(); i != targets.end(); ++i) {
			Target *target = *i;
			char buf[256];
			++n;

			if (target->visible) {

				CartesianVector c;
                                
				target->position.cartesian (c);
				cart_to_gtk (c);

				snprintf (buf, sizeof (buf), "%d", n);

                                /* stroke out a speaker shape */
                                
                                cairo_move_to (cr, c.x, c.y);
                                cairo_save (cr);
                                cairo_rotate (cr, -(target->position.azi/360.0) * (2.0 * M_PI));
                                if (small) {
                                        cairo_scale (cr, 0.8, 0.8);
                                } else {
                                        cairo_scale (cr, 1.2, 1.2);
                                }
                                cairo_rel_line_to (cr, 4, -2);
                                cairo_rel_line_to (cr, 0, -7);
                                cairo_rel_line_to (cr, 5, +5);
                                cairo_rel_line_to (cr, 5, 0);
                                cairo_rel_line_to (cr, 0, 5);
                                cairo_rel_line_to (cr, -5, 0);
                                cairo_rel_line_to (cr, -5, +5);
                                cairo_rel_line_to (cr, 0, -7);
                                cairo_close_path (cr);
				cairo_set_source_rgba (cr, 0.282, 0.517, 0.662, 1.0);
				cairo_fill (cr);
                                cairo_restore (cr);

                                if (!small) {
                                        cairo_set_font_size (cr, 16);

                                        /* move the text in just a bit */
                                        
                                        AngularVector textpos (target->position.azi, target->position.ele, 0.85);
                                        textpos.cartesian (c);
                                        cart_to_gtk (c);
                                        cairo_move_to (cr, c.x, c.y);
                                        cairo_show_text (cr, buf);
                                }

			}
		}

                /* draw position puck */
                
                position.position.cartesian (c);
                cart_to_gtk (c);

                cairo_new_path (cr);
                cairo_arc (cr, c.x, c.y, arc_radius, 0, 2.0 * M_PI);
                cairo_set_source_rgba (cr, 1.0, 0.419, 0.419, 0.85);
                cairo_fill_preserve (cr);
                cairo_set_source_rgba (cr, 1.0, 0.905, 0.905, 0.85);
                cairo_stroke (cr);
	}

	cairo_destroy (cr);

	return true;
}

bool
Panner2d::on_button_press_event (GdkEventButton *ev)
{
	GdkModifierType state;

	if (ev->type == GDK_2BUTTON_PRESS && ev->button == 1) {
		return false;
	}

	switch (ev->button) {
	case 1:
	case 2:
		if ((drag_target = find_closest_object (ev->x, ev->y)) != 0) {
			drag_target->set_selected (true);
		}

		drag_x = (int) floor (ev->x);
		drag_y = (int) floor (ev->y);
		state = (GdkModifierType) ev->state;

		return handle_motion (drag_x, drag_y, state);
		break;

	default:
		break;
	}

	return false;
}

bool
Panner2d::on_button_release_event (GdkEventButton *ev)
{
	gint x, y;
	GdkModifierType state;
	bool ret = false;

	switch (ev->button) {
	case 1:
		x = (int) floor (ev->x);
		y = (int) floor (ev->y);
		state = (GdkModifierType) ev->state;

		if (Keyboard::modifier_state_contains (state, Keyboard::TertiaryModifier)) {
                        
			for (Targets::iterator i = pucks.begin(); i != pucks.end(); ++i) {
				//Target* puck = i->second;
				/* XXX DO SOMETHING TO SET PUCK BACK TO "normal" */
			}

			queue_draw ();
			PuckMoved (-1);
			ret = true;

		} else {
			ret = handle_motion (x, y, state);
		}

		drag_target = 0;
		break;

	case 2:
		x = (int) floor (ev->x);
		y = (int) floor (ev->y);
		state = (GdkModifierType) ev->state;

		if (Keyboard::modifier_state_contains (state, Keyboard::TertiaryModifier)) {
			toggle_bypass ();
			ret = true;
		} else {
			ret = handle_motion (x, y, state);
		}

		drag_target = 0;
		break;

	case 3:
		break;

	}

	return ret;
}

gint
Panner2d::handle_motion (gint evx, gint evy, GdkModifierType state)
{
	if (drag_target == 0) {
		return false;
	}

	if ((state & (GDK_BUTTON1_MASK|GDK_BUTTON2_MASK)) == 0) {
		return false;
	}


	if (state & GDK_BUTTON1_MASK && !(state & GDK_BUTTON2_MASK)) {
		CartesianVector c;
		bool need_move = false;
                
		drag_target->position.cartesian (c);
		cart_to_gtk (c);

		if ((evx != c.x) || (evy != c.y)) {
			need_move = true;
		}

		if (need_move) {
			CartesianVector cp (evx, evy, 0.0);
                        AngularVector av;

			/* canonicalize position */

			gtk_to_cart (cp);
                        
                        // cerr << "Mouse at " << cp.x << ", " << cp.y << endl;
                        
			/* position actual signal on circle */

			clamp_to_circle (cp.x, cp.y);

			/* generate an angular representation of the current mouse position */


			cp.angular (av);
                        
                        if (drag_target == &position) {
                                // cerr << "Angle of mouse = " << av.azi << endl;
                                double degree_fract = av.azi / 360.0;
                                panner->set_position (degree_fract);
                        }
		}
	} 

	return true;
}

bool
Panner2d::on_scroll_event (GdkEventScroll* ev)
{
        switch (ev->direction) {
        case GDK_SCROLL_UP:
        case GDK_SCROLL_RIGHT:
                panner->set_position (panner->pannable()->pan_azimuth_control->get_value() - 1.0/360.0);
                break;

        case GDK_SCROLL_DOWN:
        case GDK_SCROLL_LEFT:
                panner->set_position (panner->pannable()->pan_azimuth_control->get_value() + 1.0/360.0);
                break;
        }
        return true;
}

void
Panner2d::cart_to_gtk (CartesianVector& c) const
{
	/* "c" uses a coordinate space that is:
            
	   center = 0.0
	   dimension = 2.0 * 2.0
	   so max values along each axis are -1..+1

	   GTK uses a coordinate space that is:

	   top left = 0.0
	   dimension = width * height
	   so max values along each axis are 0,width and
	   0,height
	*/
        
        const uint32_t hoffset = (width - dimen)/2;
        const uint32_t voffset = (height - dimen)/2;

	c.x = hoffset + ((dimen / 2) * (c.x + 1));
	c.y = voffset + ((dimen / 2) * (1 - c.y));

	/* XXX z-axis not handled - 2D for now */
}

void
Panner2d::gtk_to_cart (CartesianVector& c) const
{
	c.x = ((c.x / (dimen / 2.0)) - 1.0);
	c.y = -((c.y / (dimen / 2.0)) - 1.0);

	/* XXX z-axis not handled - 2D for now */
}

void
Panner2d::clamp_to_circle (double& x, double& y)
{
	double azi, ele;
	double z = 0.0;
        double l;

	PBD::cartesian_to_spherical (x, y, z, azi, ele, l);
	PBD::spherical_to_cartesian (azi, ele, 1.0, x, y, z);
}

void
Panner2d::toggle_bypass ()
{
	panner->set_bypassed (!panner->bypassed());
}

Panner2dWindow::Panner2dWindow (boost::shared_ptr<Panner> p, int32_t h, uint32_t inputs)
	: ArdourDialog (_("Panner (2D)"))
        , widget (p, h)
	, bypass_button (_("Bypass"))
{
	widget.set_name ("MixerPanZone");

	set_title (_("Panner"));
	widget.set_size_request (h, h);

        bypass_button.signal_toggled().connect (sigc::mem_fun (*this, &Panner2dWindow::bypass_toggled));

	button_box.set_spacing (6);
	button_box.pack_start (bypass_button, false, false);

	spinner_box.set_spacing (6);
	left_side.set_spacing (6);

	left_side.pack_start (button_box, false, false);
	left_side.pack_start (spinner_box, false, false);

	bypass_button.show ();
	button_box.show ();
	spinner_box.show ();
	left_side.show ();

	hpacker.set_spacing (6);
	hpacker.set_border_width (12);
	hpacker.pack_start (widget, false, false);
	hpacker.pack_start (left_side, false, false);
	hpacker.show ();

	get_vbox()->pack_start (hpacker);
	reset (inputs);
	widget.show ();
}

void
Panner2dWindow::reset (uint32_t n_inputs)
{
	widget.reset (n_inputs);

#if 0
	while (spinners.size() < n_inputs) {
		// spinners.push_back (new Gtk::SpinButton (widget.azimuth (spinners.size())));
		//spinner_box.pack_start (*spinners.back(), false, false);
		//spinners.back()->set_digits (4);
		spinners.back()->show ();
	}

	while (spinners.size() > n_inputs) {
		spinner_box.remove (*spinners.back());
		delete spinners.back();
		spinners.erase (--spinners.end());
	}
#endif
}

void
Panner2dWindow::bypass_toggled ()
{
        bool view = bypass_button.get_active ();
        bool model = widget.get_panner()->bypassed ();
        
        if (model != view) {
                widget.get_panner()->set_bypassed (view);
        }
}

bool
Panner2dWindow::on_key_press_event (GdkEventKey* event)
{
        return relay_key_press (event, &PublicEditor::instance());
}

bool
Panner2dWindow::on_key_release_event (GdkEventKey *event)
{
        return true;
}
