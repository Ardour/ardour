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

#include "canvas/box.h"
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
	ArdourCanvas::Polygon* play_shape;
	ArdourCanvas::Text*    name_text;

  private:
	ARDOUR::Trigger& _trigger;
	double poly_size;
	double poly_margin;

	PBD::ScopedConnection trigger_prop_connection;
	void prop_change (PBD::PropertyChange const & change);
	void draw_play_button ();
};

class TriggerBoxUI : public ArdourCanvas::Box
{
   public:
	TriggerBoxUI (ArdourCanvas::Item* parent, ARDOUR::TriggerBox&);
	~TriggerBoxUI ();

   private:
	ARDOUR::TriggerBox& _triggerbox;
	typedef std::vector<TriggerEntry*> Slots;
	Slots _slots;
	Gtk::FileChooserDialog* file_chooser;
	sigc::connection file_chooser_connection;
	Gtk::Menu* _context_menu;

	bool bang (GdkEvent*, size_t);
	bool text_event (GdkEvent*, size_t);
	bool event (GdkEvent*, size_t);

	void choose_sample (size_t n);
	void sample_chosen (int r, size_t n);
	void context_menu (size_t n);
	void set_follow_action (size_t slot, ARDOUR::Trigger::FollowAction);
	void set_launch_style (size_t slot, ARDOUR::Trigger::LaunchStyle);
	void set_quantization (size_t slot, Temporal::BBT_Offset const &);

	void build ();
};


class TriggerBoxWidget : public ArdourCanvas::GtkCanvas
{
  public:
	TriggerBoxWidget (ARDOUR::TriggerBox& tb);
	void size_request (double& w, double& h) const;

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
