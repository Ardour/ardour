/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef _gtk_ardour_triggerbox_ui_h_
#define _gtk_ardour_triggerbox_ui_h_

#include <gtkmm/colorselection.h>

#include "pbd/properties.h"

#include "ardour/triggerbox.h"

#include "canvas/canvas.h"
#include "canvas/rectangle.h"

#include "fitted_canvas_widget.h"

#include "trigger_ui.h"

namespace Temporal
{
	struct BBT_Offset;
}

namespace ArdourCanvas
{
	class Text;
	class Polygon;
}

class TriggerEntry : public ArdourCanvas::Rectangle, public TriggerUI
{
public:
	TriggerEntry (ArdourCanvas::Item* item, ARDOUR::TriggerReference rf);
	~TriggerEntry ();

	ArdourCanvas::Rectangle* play_button;
	ArdourCanvas::Rectangle* name_button;
	ArdourCanvas::Rectangle* follow_button;
	ArdourCanvas::Text*      name_text;

	void draw_launch_icon (Cairo::RefPtr<Cairo::Context> context, float size, float scale) const;
	void draw_follow_icon (Cairo::RefPtr<Cairo::Context> context, ARDOUR::Trigger::FollowAction icon, float size, float scale) const;

	void render (ArdourCanvas::Rect const& area, Cairo::RefPtr<Cairo::Context> context) const;

	void _size_allocate (ArdourCanvas::Rect const&);
	void maybe_update ();

	void on_trigger_changed (PBD::PropertyChange const& change);

	void selection_change ();

	void set_default_colors ();

	bool play_button_event (GdkEvent*);
	bool name_button_event (GdkEvent*);
	bool follow_button_event (GdkEvent*);

private:
	double           _poly_size;
	double           _poly_margin;

	PBD::ScopedConnection owner_prop_connection;
	void                  owner_prop_change (PBD::PropertyChange const&);
	void                  owner_color_changed ();

	void ui_parameter_changed (std::string const& p);
};

class TriggerBoxUI : public ArdourCanvas::Rectangle
{
public:
	TriggerBoxUI (ArdourCanvas::Item* parent, ARDOUR::TriggerBox&);
	~TriggerBoxUI ();

	void start_updating ();
	void stop_updating ();

	void _size_allocate (ArdourCanvas::Rect const&);

private:
	typedef std::vector<TriggerEntry*> Slots;

	ARDOUR::TriggerBox&     _triggerbox;
	Slots                   _slots;

	void build ();
	void rapid_update ();

	void selection_changed ();

	bool drag_motion (Glib::RefPtr<Gdk::DragContext> const&, int, int, guint);
	void drag_leave (Glib::RefPtr<Gdk::DragContext> const&, guint);
	void drag_data_received (Glib::RefPtr<Gdk::DragContext> const&, int, int, Gtk::SelectionData const&, guint, guint);

	uint64_t slot_at_y (int) const;

	sigc::connection _update_connection;
	sigc::connection _selection_connection;
};

class TriggerBoxWidget : public FittedCanvasWidget
{
public:
	TriggerBoxWidget (float w, float h);

	void set_triggerbox (ARDOUR::TriggerBox* tb);

	void on_map ();
	void on_unmap ();

private:
	TriggerBoxUI* ui;
};

#endif
