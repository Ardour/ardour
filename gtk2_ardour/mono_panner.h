/*
 * Copyright (C) 2011-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2011 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __gtk_ardour_mono_panner_h__
#define __gtk_ardour_mono_panner_h__

#include "pbd/signals.h"

#include <boost/shared_ptr.hpp>

#include "widgets/binding_proxy.h"

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

	boost::weak_ptr<PBD::Controllable> proxy_controllable () const
	{
		return boost::weak_ptr<PBD::Controllable> (position_binder.get_controllable());
	}

private:
	PannerEditor* editor ();
	boost::shared_ptr<ARDOUR::PannerShell> _panner_shell;

	boost::shared_ptr<PBD::Controllable> position_control;
	PBD::ScopedConnectionList panvalue_connections;
	PBD::ScopedConnectionList panshell_connections;
	int drag_start_x;
	int last_drag_x;
	double accumulated_delta;
	bool detented;

	ArdourWidgets::BindingProxy position_binder;

	void set_tooltip ();

	struct ColorScheme {
		uint32_t outline;
		uint32_t fill;
		uint32_t text;
		uint32_t background;
		uint32_t pos_outline;
		uint32_t pos_fill;
		uint32_t send_bg;
		uint32_t send_pan;
	};

	bool _dragging;

	static Pango::AttrList panner_font_attributes;
	static bool have_font;

	static ColorScheme colors;
	static void set_colors ();
	static bool have_colors;
	void color_handler ();
	void bypass_handler ();
	void pannable_handler ();
};

#endif /* __gtk_ardour_mono_panner_h__ */
