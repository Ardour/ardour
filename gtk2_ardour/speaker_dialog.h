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

#ifndef __ardour_gtk_speaker_dialog_h__
#define __ardour_gtk_speaker_dialog_h__

#include <gtkmm/drawingarea.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/box.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/aspectframe.h>

#include "ardour/speakers.h"

#include "ardour_window.h"

class SpeakerDialog  : public ArdourWindow
{
public:
	SpeakerDialog ();

	boost::shared_ptr<ARDOUR::Speakers> get_speakers() const;
	void set_speakers (boost::shared_ptr<ARDOUR::Speakers>);

private:
	boost::weak_ptr<ARDOUR::Speakers> _speakers;
	Gtk::HBox        hbox;
	Gtk::VBox        side_vbox;
	Gtk::AspectFrame aspect_frame;
	Gtk::DrawingArea darea;
	Gtk::Adjustment  azimuth_adjustment;
	Gtk::SpinButton  azimuth_spinner;
	Gtk::Button      add_speaker_button;
	Gtk::Button      remove_speaker_button;
	int32_t          selected_speaker;
	int              width;         ///< width of the circle
	int              height;        ///< height of the circle
	int              x_origin;      ///< x origin of our stuff within the drawing area
	int              y_origin;      ///< y origin of our stuff within the drawing area
	/** distance from the centre of the object being dragged to the mouse pointer
	 *  when the drag was started (ie start_pointer - object_position).
	 */
	double           drag_offset_x;
	double           drag_offset_y;
	int              drag_index;
	int              selected_index; ///< index of any selected speaker, or -1
	PBD::ScopedConnection selected_speaker_connection;
	bool             ignore_speaker_position_change;
	bool             ignore_azimuth_change;

	bool darea_expose_event (GdkEventExpose*);
	void darea_size_allocate (Gtk::Allocation& alloc);
	bool darea_motion_notify_event (GdkEventMotion *ev);
	bool handle_motion (gint evx, gint evy, GdkModifierType state);
	bool darea_button_press_event (GdkEventButton *ev);
	bool darea_button_release_event (GdkEventButton *ev);

	void clamp_to_circle (double& x, double& y);
	void gtk_to_cart (PBD::CartesianVector& c) const;
	void cart_to_gtk (PBD::CartesianVector& c) const;
	int find_closest_object (gdouble x, gdouble y);

	void add_speaker ();
	void remove_speaker ();
	void azimuth_changed ();
	void set_selected (int);
	void speaker_position_changed ();
};

#endif /* __ardour_gtk_speaker_dialog_h__ */
