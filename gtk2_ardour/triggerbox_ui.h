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
	TriggerEntry (ArdourCanvas::Canvas* canvas, ARDOUR::Trigger&);
	~TriggerEntry ();

	ARDOUR::Trigger& trigger() const { return _trigger; }

	ArdourCanvas::Rectangle* play_button;
	ArdourCanvas::Rectangle* active_bar;
	ArdourCanvas::Polygon* play_shape;
	ArdourCanvas::Text*    name_text;

	void _size_allocate (ArdourCanvas::Rect const &);
	void maybe_update ();
	bool event_handler (GdkEvent*);

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
};

class TriggerBoxUI : public ArdourCanvas::Table
{
   public:
	TriggerBoxUI (ArdourCanvas::Item* parent, ARDOUR::TriggerBox&);
	~TriggerBoxUI ();

	void edit_trigger (uint64_t n);
	void start_updating ();
	void stop_updating ();

   private:
	ARDOUR::TriggerBox& _triggerbox;
	typedef std::vector<TriggerEntry*> Slots;
	Slots _slots;
	Gtk::FileChooserDialog* file_chooser;
	sigc::connection file_chooser_connection;
	Gtk::Menu* _context_menu;

	bool bang (GdkEvent*, uint64_t);
	bool text_event (GdkEvent*, uint64_t);
	bool event (GdkEvent*, uint64_t);

	void choose_sample (uint64_t n);
	void sample_chosen (int r, uint64_t n);
	void context_menu (uint64_t n);
	void set_follow_action (uint64_t slot, ARDOUR::Trigger::FollowAction);
	void set_launch_style (uint64_t slot, ARDOUR::Trigger::LaunchStyle);
	void set_quantization (uint64_t slot, Temporal::BBT_Offset const &);

	void build ();
	void rapid_update ();

	sigc::connection update_connection;
};


class TriggerBoxWidget : public ArdourCanvas::GtkCanvas
{
  public:
	TriggerBoxWidget (ARDOUR::TriggerBox& tb);
	void size_request (double& w, double& h) const;

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
