/*
    Copyright (C) 2010 Paul Davis

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

#ifndef __gtk_ardour_stereo_panner_h__
#define __gtk_ardour_stereo_panner_h__

#include "pbd/signals.h"
#include "gtkmm2ext/binding_proxy.h"
#include "panner_interface.h"

namespace PBD {
        class Controllable;
}

namespace ARDOUR {
        class Panner;
}

class StereoPanner : public PannerInterface
{
  public:
	StereoPanner (boost::shared_ptr<ARDOUR::Panner>);
	~StereoPanner ();

        boost::shared_ptr<PBD::Controllable> get_position_controllable() const { return position_control; }
        boost::shared_ptr<PBD::Controllable> get_width_controllable() const { return width_control; }

	sigc::signal<void> StartPositionGesture;
	sigc::signal<void> StopPositionGesture;
	sigc::signal<void> StartWidthGesture;
	sigc::signal<void> StopWidthGesture;

  protected:
	bool on_expose_event (GdkEventExpose*);
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	bool on_motion_notify_event (GdkEventMotion*);
        bool on_scroll_event (GdkEventScroll*);
        bool on_key_press_event (GdkEventKey*);

  private:
	PannerEditor* editor ();
		   
        boost::shared_ptr<PBD::Controllable> position_control;
        boost::shared_ptr<PBD::Controllable> width_control;
        PBD::ScopedConnectionList connections;
        bool dragging;
        bool dragging_position;
        bool dragging_left;
        bool dragging_right;
        int drag_start_x;
        int last_drag_x;
        double accumulated_delta;
        bool detented;

        BindingProxy position_binder;
        BindingProxy width_binder;

        void set_tooltip ();

        struct ColorScheme {
            uint32_t outline;
            uint32_t fill;
            uint32_t text;
            uint32_t background;
            uint32_t rule;
        };

        enum State {
                Normal,
                Mono,
                Inverted
        };

	bool _dragging;

        static ColorScheme colors[3];
        static void set_colors ();
        static bool have_colors;
	void color_handler ();
};

#endif /* __gtk_ardour_stereo_panner_h__ */
