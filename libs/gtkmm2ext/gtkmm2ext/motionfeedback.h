/*
    Copyright (C) 1998-99 Paul Barton-Davis 
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

    $Id: motionfeedback.h,v 1.1.1.1 2001/11/24 00:44:46 pbd Exp $
*/

#ifndef __gtkmm2ext_motion_feedback_h__
#define __gtkmm2ext_motion_feedback_h__

#include <gdkmm/pixbuf.h>
#include <gtkmm/box.h>
#include <gtkmm/eventbox.h>

#include "gtkmm2ext/binding_proxy.h"
#include "gtkmm2ext/prolooks-helpers.h"

namespace Gtk {
	class Adjustment;
	class SpinButton;
}

namespace Gtkmm2ext {

class MotionFeedback : public Gtk::VBox
{
 public:
	enum Type {
		Rotary,
		CenterSpring,
		Endless
	};

	MotionFeedback (Glib::RefPtr<Gdk::Pixbuf>, 
			Type type,
			const char *widget_name = NULL,
			Gtk::Adjustment *adj = NULL, 
			bool with_numeric_display = true,
                        int sub_image_width = 40,
                        int sub_image_height = 40);
	virtual ~MotionFeedback ();

	void set_adjustment (Gtk::Adjustment *adj);
	Gtk::Adjustment *get_adjustment () { return adjustment; }

	Gtk::Widget&     eventwin () { return pixwin; }
        Gtk::SpinButton& spinner() const { return *value; }

	gfloat lower () { return _lower; }
	gfloat upper () { return _upper; }
	gfloat range () { return _range; }

	void set_controllable (boost::shared_ptr<PBD::Controllable> c) { binding_proxy.set_controllable (c); }
        void set_lamp_color (const Gdk::Color&);

        void render_file (const std::string& path, int width, int height);

 protected:
	gfloat _range;
	gfloat _lower;
	gfloat _upper;

	void pixwin_size_request (GtkRequisition *);

	bool pixwin_button_press_event (GdkEventButton *);
	bool pixwin_button_release_event (GdkEventButton *);
	bool pixwin_motion_notify_event (GdkEventMotion *);
	bool pixwin_key_press_event (GdkEventKey *);
	bool pixwin_enter_notify_event (GdkEventCrossing *);
	bool pixwin_leave_notify_event (GdkEventCrossing *);
	bool pixwin_focus_in_event (GdkEventFocus*);
	bool pixwin_focus_out_event (GdkEventFocus *);
	bool pixwin_expose_event (GdkEventExpose*);
	bool pixwin_scroll_event (GdkEventScroll*);
	void pixwin_realized ();

  private:
	Type type;
	Gtk::EventBox      pixwin;
        Gtk::HBox*         value_packer;
	Gtk::SpinButton*   value;
	Gtk::Adjustment*   adjustment;
	Glib::RefPtr<Gdk::Pixbuf> pixbuf;
        BindingProxy       binding_proxy;

        double default_value;
	double  step_inc;
	double page_inc;
	bool   grab_is_fine;
	double grabbed_y;
	double grabbed_x;
	bool i_own_my_adjustment;
        int subwidth;
        int subheight;
	void adjustment_changed ();

	ProlooksHSV* lamp_hsv;
        Gdk::Color _lamp_color;
	GdkColor lamp_bright;
	GdkColor lamp_dark;

        void core_draw (cairo_t*, int, double, double, double);
};

} /* namespace */
	
#endif // __gtkmm2ext_motion_feedback_h__
