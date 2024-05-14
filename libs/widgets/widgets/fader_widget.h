/*
 * Copyright (C) 2024 Robin Gareus <robin@gareus.org>
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

#pragma once

#include <gtkmm/adjustment.h>
#include "gtkmm2ext/cairo_widget.h"

#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API FaderWidget : public virtual CairoWidget
{
public:
	FaderWidget (Gtk::Adjustment&, int orient);
	virtual ~FaderWidget () {};

	sigc::signal<void,int> StartGesture;
	sigc::signal<void,int> StopGesture;
	sigc::signal<void> OnExpose;

	virtual void set_default_value (float) = 0;

	enum Tweaks {
		NoShowUnityLine = 0x1,
		NoButtonForward = 0x2,
		NoVerticalScroll = 0x4,
		DoubleClickReset = 0x8,
	};

	enum Orientation {
		VERT,
		HORIZ,
	};

	void set_tweaks (Tweaks);
	Tweaks tweaks() const { return _tweaks; }

	virtual void set_bg (Gtkmm2ext::Color) = 0;
	virtual void set_fg (Gtkmm2ext::Color) = 0;
	virtual void unset_bg () = 0;
	virtual void unset_fg () = 0;

protected:
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	bool on_enter_notify_event (GdkEventCrossing* ev);
	bool on_leave_notify_event (GdkEventCrossing* ev);
	bool on_scroll_event (GdkEventScroll* ev);
	bool on_grab_broken_event (GdkEventGrabBroken*);

	void adjustment_changed ();

	virtual void set_adjustment_from_event (GdkEventButton*) = 0;

	Gtk::Adjustment& _adjustment;

	Tweaks _tweaks;
	int    _orien;
	bool   _dragging;
	bool   _hovering;
	float  _default_value;


	GdkWindow* _grab_window;
	double     _grab_loc;
	double     _grab_start;
};

}
