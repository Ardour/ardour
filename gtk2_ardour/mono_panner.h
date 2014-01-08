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

#ifndef __gtk_ardour_mono_panner_h__
#define __gtk_ardour_mono_panner_h__

#include "pbd/signals.h"

#include <boost/shared_ptr.hpp>

#include "gtkmm2ext/binding_proxy.h"

#include "panner_interface.h"

namespace ARDOUR {
	class PannerShell;
}

namespace PBD {
        class Controllable;
}

class MonoPanner : public PannerInterface
{
  public:
	MonoPanner (boost::shared_ptr<ARDOUR::PannerShell>);
	~MonoPanner ();

        boost::shared_ptr<PBD::Controllable> get_controllable() const { return position_control; }

	sigc::signal<void> StartGesture;
	sigc::signal<void> StopGesture;

  protected:
	bool on_expose_event (GdkEventExpose*);
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	bool on_motion_notify_event (GdkEventMotion*);
        bool on_scroll_event (GdkEventScroll*);
        bool on_key_press_event (GdkEventKey*);

  private:
	PannerEditor* editor ();
	boost::shared_ptr<ARDOUR::PannerShell> _panner_shell;
	
        boost::shared_ptr<PBD::Controllable> position_control;
        PBD::ScopedConnectionList connections;
        int drag_start_x;
        int last_drag_x;
        double accumulated_delta;
        bool detented;

        BindingProxy position_binder;

        void set_tooltip ();

        struct ColorScheme {
            uint32_t outline;
            uint32_t fill;
            uint32_t text;
            uint32_t background;
            uint32_t pos_outline;
            uint32_t pos_fill;
        };

	bool _dragging;

        static ColorScheme colors;
        static void set_colors ();
        static bool have_colors;
	void color_handler ();
	void bypass_handler ();
};

#endif /* __gtk_ardour_mono_panner_h__ */
