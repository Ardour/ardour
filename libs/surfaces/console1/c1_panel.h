/*
 * Copyright (C) 2026 Holger Dehnhardt <holger@dehnhardt.org>
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

#ifndef __ardour_console1_panel_h__
#define __ardour_console1_panel_h__

#include <string>
#include <vector>

#include <ytkmm/drawingarea.h>
#include <ydkmm/pixbuf.h>
#include <ydkmm/general.h>

#include "pbd/signals.h"

#include "console1.h"

namespace ArdourSurface {

/* Centre of the printed name of a control in the picture and the width available for the text */
struct C1LabelPos
{
	Console1::ControllerID id;
	double x;
	double y;
	double w;
	double h = 18;
};

/* The picture of the controller with the current function of every control drawn over its printed name */
class C1Panel : public Gtk::DrawingArea
{
public:
	C1Panel (Console1&, std::string const& image_path);

	/* in edit mode the labels can be dragged (wheel: width, shift+wheel: height); positions are saved per user */
	void set_edit_mode (bool);
	void reset_positions ();

protected:
	bool on_expose_event (GdkEventExpose*);
	void on_size_request (Gtk::Requisition*);
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	bool on_motion_notify_event (GdkEventMotion*);
	bool on_scroll_event (GdkEventScroll*);

private:
	Console1& c1;
	Glib::RefPtr<Gdk::Pixbuf> pixbuf;
	PBD::ScopedConnectionList _connections;
	void labels_changed ();

	std::vector<C1LabelPos> positions;
	bool edit_mode;
	int dragging;
	double drag_dx;
	double drag_dy;
	double offset_x;

	int hit_test (double x, double y) const;
	std::string positions_file () const;
	void load_positions ();
	void save_positions () const;
};

}

#endif /* __ardour_console1_panel_h__ */
