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

#ifndef __ardour_gtk_triggerbox_ui_h__
#define __ardour_gtk_triggerbox_ui_h__

#include <map>

#include <gtkmm/window.h>

#include "pbd/properties.h"

#include "ardour/triggerbox.h"

#include "canvas/table.h"
#include "canvas/canvas.h"
#include "canvas/rectangle.h"

#include "fitted_canvas_widget.h"

namespace Gtk {
class FileChooserDialog;
class Menu;
}

namespace Temporal {
	struct BBT_Offset;
}

namespace ArdourCanvas {
	class Text;
	class Polygon;
};

class TriggerEntry : public ArdourCanvas::Rectangle
{
  public:
	TriggerEntry (ArdourCanvas::Item* item, ARDOUR::Trigger&);
	~TriggerEntry ();

	ARDOUR::Trigger& trigger() const { return _trigger; }

	ArdourCanvas::Rectangle* play_button;
	ArdourCanvas::Polygon* play_shape;

	ArdourCanvas::Rectangle* name_button;
	ArdourCanvas::Text*    name_text;

	void render (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> context) const;

	void _size_allocate (ArdourCanvas::Rect const &);
	void maybe_update ();

	void selection_change ();

	void set_default_colors();

  private:
	ARDOUR::Trigger& _trigger;
	double poly_size;
	double poly_margin;

	PBD::ScopedConnection trigger_prop_connection;
	void prop_change (PBD::PropertyChange const & change);
	void shape_play_button ();

	PBD::ScopedConnection owner_prop_connection;
	void owner_prop_change (PBD::PropertyChange const &);
	void owner_color_changed ();

	void ui_parameter_changed (std::string const& p);
};

class TriggerBoxUI : public ArdourCanvas::Rectangle
{
   public:
	TriggerBoxUI (ArdourCanvas::Item* parent, ARDOUR::TriggerBox&);
	~TriggerBoxUI ();

	void edit_trigger (uint64_t n);
	void start_updating ();
	void stop_updating ();

	static Glib::RefPtr<Gtk::ActionGroup> trigger_actions;
	static void setup_actions_and_bindings ();

	static void trigger_scene (int32_t);

	void _size_allocate (ArdourCanvas::Rect const &);

private:
	ARDOUR::TriggerBox& _triggerbox;
	typedef std::vector<TriggerEntry*> Slots;
	Slots _slots;
	Gtk::FileChooserDialog* file_chooser;
	sigc::connection file_chooser_connection;
	Gtk::Menu* _context_menu;

	static Gtkmm2ext::Bindings* bindings;
	static void load_bindings ();
	static void register_actions ();

	bool play_button_event (GdkEvent*, uint64_t);
	bool text_button_event (GdkEvent*, uint64_t);

	void choose_sample (uint64_t n);
	void sample_chosen (int r, uint64_t n);
	void context_menu (uint64_t n);
	void set_follow_action (uint64_t slot, ARDOUR::Trigger::FollowAction);
	void set_launch_style (uint64_t slot, ARDOUR::Trigger::LaunchStyle);
	void set_quantization (uint64_t slot, Temporal::BBT_Offset const &);
	void set_from_selection (uint64_t slot);

	void build ();
	void rapid_update ();

	void selection_changed ();

	void drag_data_received (Glib::RefPtr<Gdk::DragContext> const&, int, int, Gtk::SelectionData const&, guint, guint);

	sigc::connection update_connection;
	sigc::connection selection_connection;
};

class TriggerBoxWidget : public FittedCanvasWidget
{
  public:
	TriggerBoxWidget (ARDOUR::TriggerBox& tb, float w, float h);

	void on_map ();
	void on_unmap ();

  private:
	TriggerBoxUI* ui;
};

/* XXX probably for testing only */

class TriggerBoxWindow : public Gtk::Window
{
    public:
	TriggerBoxWindow (ARDOUR::TriggerBox&);

	bool on_key_press_event (GdkEventKey*);
	bool on_key_release_event (GdkEventKey*);
};

#endif /* __ardour_gtk_triggerbox_ui_h__ */
