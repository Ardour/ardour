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

namespace Gtk
{
	class FileChooserDialog;
	class Menu;
}

namespace Temporal
{
	struct BBT_Offset;
}

namespace ArdourCanvas
{
	class Text;
	class Polygon;
}

class TriggerEntry : public ArdourCanvas::Rectangle
{
public:
	TriggerEntry (ArdourCanvas::Item* item, ARDOUR::TriggerReference rf);
	~TriggerEntry ();

	boost::shared_ptr<ARDOUR::Trigger> trigger () const
	{
		return tref.trigger();
	}
	ARDOUR::TriggerReference trigger_reference() const { return tref; }

	ArdourCanvas::Rectangle* play_button;
	ArdourCanvas::Rectangle* name_button;
	ArdourCanvas::Rectangle* follow_button;
	ArdourCanvas::Text*      name_text;

	void draw_launch_icon (Cairo::RefPtr<Cairo::Context> context, float size, float scale) const;
	void draw_follow_icon (Cairo::RefPtr<Cairo::Context> context, ARDOUR::Trigger::FollowAction icon, float size, float scale) const;

	void render (ArdourCanvas::Rect const& area, Cairo::RefPtr<Cairo::Context> context) const;

	void _size_allocate (ArdourCanvas::Rect const&);
	void maybe_update ();

	void selection_change ();

	void set_default_colors ();

private:
	ARDOUR::TriggerReference tref;
	double           _poly_size;
	double           _poly_margin;

	PBD::ScopedConnection trigger_prop_connection;
	PBD::ScopedConnection trigger_swap_connection;
	void                  prop_change (PBD::PropertyChange const& change);

	void                  trigger_swap (uint32_t);

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

	void toggle_trigger_isolated (uint64_t n);
	void clear_trigger (uint64_t n);
	void edit_trigger (uint64_t n);
	void start_updating ();
	void stop_updating ();

	void _size_allocate (ArdourCanvas::Rect const&);

private:
	typedef std::vector<TriggerEntry*> Slots;

	ARDOUR::TriggerBox&     _triggerbox;
	Slots                   _slots;
	Gtk::FileChooserDialog* _file_chooser;
	sigc::connection        _file_chooser_connection;
	Gtk::Menu*              _launch_context_menu;
	Gtk::Menu*              _follow_context_menu;
	Gtk::Menu*              _context_menu;
	bool                    _ignore_menu_action;

	bool play_button_event (GdkEvent*, uint64_t);
	bool name_button_event (GdkEvent*, uint64_t);
	bool follow_button_event (GdkEvent*, uint64_t);

	void choose_sample (uint64_t n);
	void sample_chosen (int r, uint64_t n);

	void launch_context_menu (uint64_t n);
	void follow_context_menu (uint64_t n);
	void context_menu (uint64_t n);

	Gtk::ColorSelectionDialog _color_dialog;
	void pick_color (uint64_t n);
	void set_follow_action (uint64_t slot, ARDOUR::Trigger::FollowAction);
	void set_launch_style (uint64_t slot, ARDOUR::Trigger::LaunchStyle);
	void set_quantization (uint64_t slot, Temporal::BBT_Offset const&);
	void set_from_selection (uint64_t slot);

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
