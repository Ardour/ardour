/*
    Copyright (C) 2011 Paul Davis

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

#include "pbd/cartesian.h"

#include "gtkmm2ext/keyboard.h"

#include "speaker_dialog.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

SpeakerDialog::SpeakerDialog ()
        : ArdourWindow (_("Speaker Configuration"))
        , aspect_frame ("", 0.5, 0.5, 1.5, false)
        , azimuth_adjustment (0, 0.0, 360.0, 10.0, 1.0)
        , azimuth_spinner (azimuth_adjustment)
        , add_speaker_button (_("Add Speaker"))
	, remove_speaker_button (_("Remove Speaker"))
	/* initialize to 0 so that set_selected works below */
	, selected_index (0)
	, ignore_speaker_position_change (false)
	, ignore_azimuth_change (false)
{
        side_vbox.set_homogeneous (false);
        side_vbox.set_border_width (6);
        side_vbox.set_spacing (6);
        side_vbox.pack_start (add_speaker_button, false, false);

        aspect_frame.set_size_request (300, 200);
        aspect_frame.set_shadow_type (SHADOW_NONE);
        aspect_frame.add (darea);

        hbox.set_spacing (6);
        hbox.set_border_width (6);
        hbox.pack_start (aspect_frame, true, true);
        hbox.pack_start (side_vbox, false, false);

	HBox* current_speaker_hbox = manage (new HBox);
	current_speaker_hbox->set_spacing (4);
	current_speaker_hbox->pack_start (*manage (new Label (_("Azimuth:"))), false, false);
	current_speaker_hbox->pack_start (azimuth_spinner, true, true);
	current_speaker_hbox->pack_start (remove_speaker_button, true, true);

	VBox* vbox = manage (new VBox);
	vbox->pack_start (hbox);
	vbox->pack_start (*current_speaker_hbox, true, true);
	vbox->show_all ();
	add (*vbox);

        darea.add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::POINTER_MOTION_MASK);

        darea.signal_size_allocate().connect (sigc::mem_fun (*this, &SpeakerDialog::darea_size_allocate));
        darea.signal_expose_event().connect (sigc::mem_fun (*this, &SpeakerDialog::darea_expose_event));
        darea.signal_button_press_event().connect (sigc::mem_fun (*this, &SpeakerDialog::darea_button_press_event));
        darea.signal_button_release_event().connect (sigc::mem_fun (*this, &SpeakerDialog::darea_button_release_event));
        darea.signal_motion_notify_event().connect (sigc::mem_fun (*this, &SpeakerDialog::darea_motion_notify_event));

	add_speaker_button.signal_clicked().connect (sigc::mem_fun (*this, &SpeakerDialog::add_speaker));
	remove_speaker_button.signal_clicked().connect (sigc::mem_fun (*this, &SpeakerDialog::remove_speaker));
	azimuth_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &SpeakerDialog::azimuth_changed));

        drag_index = -1;

	/* selected index initialised to 0 above; this will set `no selection' and
	   sensitize widgets accordingly.
	*/
	set_selected (-1);
}

void
SpeakerDialog::set_speakers (boost::shared_ptr<Speakers> s)
{
        _speakers = s;
}

boost::shared_ptr<Speakers>
SpeakerDialog::get_speakers () const
{
        return _speakers.lock ();
}

bool
SpeakerDialog::darea_expose_event (GdkEventExpose* event)
{
	boost::shared_ptr<Speakers> speakers = _speakers.lock ();
	if (!speakers) {
		return false;
	}

	gint x, y;
	cairo_t* cr;

	cr = gdk_cairo_create (darea.get_window()->gobj());

	cairo_set_line_width (cr, 1.0);

	cairo_rectangle (cr, event->area.x, event->area.y, event->area.width, event->area.height);
        cairo_set_source_rgba (cr, 0.1, 0.1, 0.1, 1.0);
	cairo_fill_preserve (cr);
	cairo_clip (cr);

	cairo_translate (cr, x_origin, y_origin);

	/* horizontal line of "crosshairs" */

	cairo_set_source_rgb (cr, 0.0, 0.1, 0.7);
	cairo_move_to (cr, 0.5, height/2.0+0.5);
	cairo_line_to (cr, width+0.5, height/2+0.5);
	cairo_stroke (cr);

	/* vertical line of "crosshairs" */

	cairo_move_to (cr, width/2+0.5, 0.5);
	cairo_line_to (cr, width/2+0.5, height+0.5);
	cairo_stroke (cr);

	/* the circle on which signals live */

	cairo_arc (cr, width/2, height/2, height/2, 0, 2.0 * M_PI);
	cairo_stroke (cr);

        float arc_radius;

        cairo_select_font_face (cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

        if (height < 100) {
                cairo_set_font_size (cr, 10);
                arc_radius = 2.0;
        } else {
                cairo_set_font_size (cr, 16);
                arc_radius = 4.0;
        }

        int n = 0;
        for (vector<Speaker>::iterator i = speakers->speakers().begin(); i != speakers->speakers().end(); ++i) {

                Speaker& s (*i);
                CartesianVector c (s.coords());

                cart_to_gtk (c);

		/* We have already moved our plotting origin to x_origin, y_origin,
		   so compensate for that.
		*/
		c.x -= x_origin;
		c.y -= y_origin;

                x = (gint) floor (c.x);
                y = (gint) floor (c.y);

                /* XXX need to shift circles so that they are centered on the circle */

                cairo_arc (cr, x, y, arc_radius, 0, 2.0 * M_PI);
		if (selected_index == n) {
			cairo_set_source_rgb (cr, 0.8, 0.8, 0.2);
		} else {
			cairo_set_source_rgb (cr, 0.8, 0.2, 0.1);
		}
                cairo_close_path (cr);
                cairo_fill (cr);

                cairo_move_to (cr, x + 6, y + 6);

                char buf[256];
		if (n == selected_index) {
			snprintf (buf, sizeof (buf), "%d:%d", n+1, (int) lrint (s.angles().azi));
		} else {
			snprintf (buf, sizeof (buf), "%d", n + 1);
		}
                cairo_show_text (cr, buf);
                ++n;
        }

        cairo_destroy (cr);

	return true;

}

void
SpeakerDialog::cart_to_gtk (CartesianVector& c) const
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

	c.x = (width / 2) * (c.x + 1) + x_origin;
	c.y = (height / 2) * (1 - c.y) + y_origin;

	/* XXX z-axis not handled - 2D for now */
}

void
SpeakerDialog::gtk_to_cart (CartesianVector& c) const
{
	c.x = ((c.x - x_origin) / (width / 2.0)) - 1.0;
	c.y = -(((c.y - y_origin) / (height / 2.0)) - 1.0);

	/* XXX z-axis not handled - 2D for now */
}

void
SpeakerDialog::clamp_to_circle (double& x, double& y)
{
	double azi, ele;
	double z = 0.0;
        double l;

	PBD::cartesian_to_spherical (x, y, z, azi, ele, l);
	PBD::spherical_to_cartesian (azi, ele, 1.0, x, y, z);
}

void
SpeakerDialog::darea_size_allocate (Gtk::Allocation& alloc)
{
  	width = alloc.get_width();
  	height = alloc.get_height();

	/* The allocation will (should) be rectangualar, but make the basic
	   drawing square; space to the right of the square is for over-hanging
	   text labels.
	*/
	width = height;

	if (height > 100) {
		width -= 20;
		height -= 20;
	}

	/* Put the x origin to the left of the rectangular allocation */
	x_origin = (alloc.get_width() - width) / 3;
	y_origin = (alloc.get_height() - height) / 2;
}

bool
SpeakerDialog::darea_button_press_event (GdkEventButton *ev)
{
	boost::shared_ptr<Speakers> speakers = _speakers.lock ();
	if (!speakers) {
		return false;
	}

	GdkModifierType state;

	if (ev->type == GDK_2BUTTON_PRESS && ev->button == 1) {
		return false;
	}

        drag_index = -1;

	switch (ev->button) {
	case 1:
	case 2:
	{
		int const index = find_closest_object (ev->x, ev->y);
		set_selected (index);

		drag_index = index;
		int const drag_x = (int) floor (ev->x);
		int const drag_y = (int) floor (ev->y);
		state = (GdkModifierType) ev->state;

		if (drag_index >= 0) {
			CartesianVector c;
			speakers->speakers()[drag_index].angles().cartesian (c);
			cart_to_gtk (c);
			drag_offset_x = drag_x - x_origin - c.x;
			drag_offset_y = drag_y - y_origin - c.y;
		}

		return handle_motion (drag_x, drag_y, state);
		break;
	}

	default:
		break;
	}

	return false;
}

bool
SpeakerDialog::darea_button_release_event (GdkEventButton *ev)
{
	boost::shared_ptr<Speakers> speakers = _speakers.lock ();
	if (!speakers) {
		return false;
	}

	gint x, y;
	GdkModifierType state;
	bool ret = false;

	switch (ev->button) {
	case 1:
		x = (int) floor (ev->x);
		y = (int) floor (ev->y);
		state = (GdkModifierType) ev->state;

		if (Keyboard::modifier_state_contains (state, Keyboard::TertiaryModifier)) {

			for (vector<Speaker>::iterator i = speakers->speakers().begin(); i != speakers->speakers().end(); ++i) {
				/* XXX DO SOMETHING TO SET SPEAKER BACK TO "normal" */
			}

			queue_draw ();
			ret = true;

		} else {
			ret = handle_motion (x, y, state);
		}

		break;

	case 2:
		x = (int) floor (ev->x);
		y = (int) floor (ev->y);
		state = (GdkModifierType) ev->state;

                ret = handle_motion (x, y, state);
		break;

	case 3:
		break;

	}

        drag_index = -1;

	return ret;
}

int
SpeakerDialog::find_closest_object (gdouble x, gdouble y)
{
	boost::shared_ptr<Speakers> speakers = _speakers.lock ();
	if (!speakers) {
		return -1;
	}

	float distance;
	float best_distance = FLT_MAX;
	int n = 0;
        int which = -1;

	for (vector<Speaker>::iterator i = speakers->speakers().begin(); i != speakers->speakers().end(); ++i, ++n) {

		Speaker& candidate (*i);
		CartesianVector c;

		candidate.angles().cartesian (c);
		cart_to_gtk (c);

		distance = sqrt ((c.x - x) * (c.x - x) +
		                 (c.y - y) * (c.y - y));


		if (distance < best_distance) {
			best_distance = distance;
			which = n;
		}
	}

	if (best_distance > 20) { // arbitrary
                return -1;
	}

        return which;
}

bool
SpeakerDialog::darea_motion_notify_event (GdkEventMotion *ev)
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
SpeakerDialog::handle_motion (gint evx, gint evy, GdkModifierType state)
{
	boost::shared_ptr<Speakers> speakers = _speakers.lock ();
	if (!speakers) {
		return false;
	}

	if (drag_index < 0) {
		return false;
	}

	if ((state & (GDK_BUTTON1_MASK|GDK_BUTTON2_MASK)) == 0) {
		return false;
	}

	/* correct event coordinates to have their origin at the corner of our graphic
	   rather than the corner of our allocation */

	double obx = evx - x_origin;
	double oby = evy - y_origin;

	/* and compensate for any distance between the mouse pointer and the centre
	   of the object being dragged */

	obx -= drag_offset_x;
	oby -= drag_offset_y;

	if (state & GDK_BUTTON1_MASK && !(state & GDK_BUTTON2_MASK)) {
		CartesianVector c;
		bool need_move = false;
                Speaker& moving (speakers->speakers()[drag_index]);

		moving.angles().cartesian (c);
		cart_to_gtk (c);

		if (obx != c.x || oby != c.y) {
			need_move = true;
		}

		if (need_move) {
			CartesianVector cp (obx, oby, 0.0);

			/* canonicalize position */

			gtk_to_cart (cp);

			/* position actual signal on circle */

			clamp_to_circle (cp.x, cp.y);

			/* generate an angular representation and set drag target (GUI) position */

                        AngularVector a;

			cp.angular (a);

                        moving.move (a);

			queue_draw ();
		}
	}

	return true;
}

void
SpeakerDialog::add_speaker ()
{
	boost::shared_ptr<Speakers> speakers = _speakers.lock ();
	if (!speakers) {
		return;
	}

	speakers->add_speaker (PBD::AngularVector (0, 0, 0));
	queue_draw ();
}

void
SpeakerDialog::set_selected (int i)
{
	boost::shared_ptr<Speakers> speakers = _speakers.lock ();
	if (!speakers) {
		return;
	}

	if (i == selected_index) {
		return;
	}

	selected_index = i;
	queue_draw ();

	selected_speaker_connection.disconnect ();

	azimuth_spinner.set_sensitive (selected_index != -1);
	remove_speaker_button.set_sensitive (selected_index != -1);

	if (selected_index != -1) {
		azimuth_adjustment.set_value (speakers->speakers()[selected_index].angles().azi);
		speakers->speakers()[selected_index].PositionChanged.connect (
			selected_speaker_connection, MISSING_INVALIDATOR,
			boost::bind (&SpeakerDialog::speaker_position_changed, this),
			gui_context ()
			);
	}
}

void
SpeakerDialog::azimuth_changed ()
{
	boost::shared_ptr<Speakers> speakers = _speakers.lock ();
	if (!speakers) {
		return;
	}

	assert (selected_index != -1);

	if (ignore_azimuth_change) {
		return;
	}

	ignore_speaker_position_change = true;
	speakers->move_speaker (speakers->speakers()[selected_index].id, PBD::AngularVector (azimuth_adjustment.get_value (), 0, 0));
	ignore_speaker_position_change = false;

	queue_draw ();
}

void
SpeakerDialog::speaker_position_changed ()
{
	boost::shared_ptr<Speakers> speakers = _speakers.lock ();
	if (!speakers) {
		return;
	}

	assert (selected_index != -1);

	if (ignore_speaker_position_change) {
		return;
	}

	ignore_azimuth_change = true;
	azimuth_adjustment.set_value (speakers->speakers()[selected_index].angles().azi);
	ignore_azimuth_change = false;

	queue_draw ();
}

void
SpeakerDialog::remove_speaker ()
{
	boost::shared_ptr<Speakers> speakers = _speakers.lock ();
	if (!speakers) {
		return;
	}

	assert (selected_index != -1);

	speakers->remove_speaker (speakers->speakers()[selected_index].id);
	set_selected (-1);

	queue_draw ();
}
