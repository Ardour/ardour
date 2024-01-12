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

class TriggerStrip;

class TriggerEntry : public ArdourCanvas::Rectangle, public TriggerUI
{
public:
	TriggerEntry (ArdourCanvas::Item* item, TriggerStrip&, ARDOUR::TriggerReference rf);
	~TriggerEntry ();

	ArdourCanvas::Rectangle* play_button;
	ArdourCanvas::Rectangle* name_button;
	ArdourCanvas::Rectangle* follow_button;
	ArdourCanvas::Text*      name_text;

	void draw_launch_icon (Cairo::RefPtr<Cairo::Context> context, float size, float scale) const;
	void draw_follow_icon (Cairo::RefPtr<Cairo::Context> context, ARDOUR::FollowAction const & icon, float size, float scale) const;

	void render (ArdourCanvas::Rect const& area, Cairo::RefPtr<Cairo::Context> context) const;

	void _size_allocate (ArdourCanvas::Rect const&);

	void on_trigger_changed (PBD::PropertyChange const& change);

	void selection_change ();

	enum EnteredState {
		PlayEntered,
		NameEntered,
		FollowEntered,
		NoneEntered
	};

	void set_widget_colors (TriggerEntry::EnteredState es = NoneEntered);

	bool name_button_event (GdkEvent*);

	TriggerStrip& strip() const { return _strip; }

private:
	TriggerStrip& _strip;
	bool   _grabbed;
	double _poly_size;
	double _poly_margin;

	int  _drag_start_x;
	int  _drag_start_y;
	bool _drag_active;

	bool event (GdkEvent*);
	void drag_begin (Glib::RefPtr<Gdk::DragContext> const&);
	void drag_end (Glib::RefPtr<Gdk::DragContext> const&);
	void drag_data_get (Glib::RefPtr<Gdk::DragContext> const&, Gtk::SelectionData&, guint, guint);

	void ui_parameter_changed (std::string const& p);

	bool play_button_event (GdkEvent*);
	bool follow_button_event (GdkEvent*);

	void owner_prop_change (PBD::PropertyChange const&);
	void owner_color_changed ();

	PBD::ScopedConnection _owner_prop_connection;
};

class TriggerBoxUI : public ArdourCanvas::Rectangle
{
public:
	TriggerBoxUI (ArdourCanvas::Item* parent, TriggerStrip&, ARDOUR::TriggerBox&);
	~TriggerBoxUI ();

	void _size_allocate (ArdourCanvas::Rect const&);

	TriggerStrip& strip() const { return _strip; }

	static Glib::RefPtr<Gtk::TargetList> dnd_src ()
	{
		return _dnd_src;
	}

private:
	typedef std::vector<TriggerEntry*> Slots;

	ARDOUR::TriggerBox& _triggerbox;
	Slots               _slots;
	TriggerStrip&       _strip;

	static Glib::RefPtr<Gtk::TargetList> _dnd_src;

	void build ();

	void selection_changed ();

	bool drag_motion (Glib::RefPtr<Gdk::DragContext> const&, int, int, guint);
	void drag_leave (Glib::RefPtr<Gdk::DragContext> const&, guint);
	void drag_data_received (Glib::RefPtr<Gdk::DragContext> const&, int, int, Gtk::SelectionData const&, guint, guint);

	bool triggerbox_event (GdkEvent*);

	uint64_t slot_at_y (int) const;

	sigc::connection _selection_connection;
};

class TriggerBoxWidget : public FittedCanvasWidget
{
public:
	TriggerBoxWidget (TriggerStrip&, float w, float h);

	void set_triggerbox (ARDOUR::TriggerBox* tb);
	TriggerStrip& strip() const { return _strip; }

private:
	TriggerBoxUI* ui;
	TriggerStrip& _strip;
};

#endif
