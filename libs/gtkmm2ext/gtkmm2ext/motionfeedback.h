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

#include "pbd/signals.h"

#include <gdkmm/pixbuf.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/eventbox.h>

#include "gtkmm2ext/visibility.h"
#include "gtkmm2ext/binding_proxy.h"
#include "gtkmm2ext/prolooks-helpers.h"

namespace Gtk {
	class Adjustment;
	class SpinButton;
}

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API MotionFeedback : public Gtk::VBox
{
 public:
	enum Type {
		Rotary,
		CenterSpring,
		Endless
	};

	MotionFeedback (Glib::RefPtr<Gdk::Pixbuf>, 
			Type type,
			boost::shared_ptr<PBD::Controllable>,
			double default_value,
			double step_increment,
			double page_increment,
			const char *widget_name = NULL,
			bool with_numeric_display = true,
                        int sub_image_width = 40,
                        int sub_image_height = 40);
	virtual ~MotionFeedback ();

	Gtk::Widget& eventwin () { return pixwin; }

        boost::shared_ptr<PBD::Controllable> controllable() const;
	virtual void set_controllable (boost::shared_ptr<PBD::Controllable> c);

        static void set_lamp_color (const std::string&);
        
        static Glib::RefPtr<Gdk::Pixbuf> render_pixbuf (int size);

	void set_print_func(void (*pf)(char buf[32], const boost::shared_ptr<PBD::Controllable>&, void *),
			    void *arg) {
		print_func = pf;
		print_arg = arg;
	};

 protected:
	boost::shared_ptr<PBD::Controllable> _controllable;
	Gtk::Label* value;
        double default_value;
	double step_inc;
	double page_inc;

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

	/* map a display value (0.0 .. 1.0) to a control
	   value (controllable->lower() .. controllable()->upper)
	*/
	virtual double to_control_value (double) = 0;

	/* map a control value (controllable->lower() .. controllable()->upper)
	   to a display value (0.0 .. 1.0)
	*/
	virtual double to_display_value (double) = 0;
	
	virtual double adjust (double nominal_delta) = 0;

  private:
	Type type;
	Gtk::EventBox      pixwin;
        Gtk::EventBox*     value_packer;
	Glib::RefPtr<Gdk::Pixbuf> pixbuf;
        BindingProxy       binding_proxy;
	static Gdk::Color* base_color;

	void (*print_func) (char buf[32], const boost::shared_ptr<PBD::Controllable>&, void *);
	void *print_arg;
	static void default_printer (char buf[32], const boost::shared_ptr<PBD::Controllable>&, void *);

	bool   grab_is_fine;
	double grabbed_y;
	double grabbed_x;
        int subwidth;
        int subheight;
	void controllable_value_changed ();
	PBD::ScopedConnection controller_connection;

        static void core_draw (cairo_t*, int, double, double, double, double, const GdkColor* bright, const GdkColor* dark);
};

} /* namespace */
	
#endif // __gtkmm2ext_motion_feedback_h__
