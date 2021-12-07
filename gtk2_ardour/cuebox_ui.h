/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
 * Author: Ben Loftis 2021 <ben@harrisonconsoles.com>
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

#ifndef __cuebox_ui_h__
#define __cuebox_ui_h__

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

class CueEntry : public ArdourCanvas::Rectangle
{
  public:
	CueEntry (ArdourCanvas::Item* item, uint64_t cue_index );
	~CueEntry ();

	ArdourCanvas::Rectangle* play_button;
	ArdourCanvas::Polygon* play_shape;

	ArdourCanvas::Rectangle* name_button;
	ArdourCanvas::Text*    name_text;

	void render (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> context) const;

	void _size_allocate (ArdourCanvas::Rect const &);
	bool event_handler (GdkEvent*);

  private:
	uint64_t _cue_idx;

	double poly_size;
	double poly_margin;

	void shape_play_button ();

	void ui_parameter_changed (std::string const& p);
};


class CueBoxUI : public ArdourCanvas::Rectangle
{
   public:
	CueBoxUI (ArdourCanvas::Item* parent);
	~CueBoxUI ();

	void trigger_scene (uint64_t n);

	static Glib::RefPtr<Gtk::ActionGroup> trigger_actions;
	static void setup_actions_and_bindings ();

	void _size_allocate (ArdourCanvas::Rect const &);

   private:

	bool event (GdkEvent*, uint64_t);
	bool text_event (GdkEvent*, uint64_t);

	typedef std::vector<ArdourCanvas::Rectangle*> Slots;
	Slots _slots;

	static Gtkmm2ext::Bindings* bindings;
	static void load_bindings ();
	static void register_actions ();

	void build ();
};

class CueBoxWidget : public FittedCanvasWidget
{
  public:
	CueBoxWidget (float w, float h);

	void on_map ();
	void on_unmap ();

  private:
	CueBoxUI* ui;
};

/* XXX probably for testing only */

class CueBoxWindow : public Gtk::Window
{
  public:
	CueBoxWindow ();

	bool on_key_press_event (GdkEventKey*);
	bool on_key_release_event (GdkEventKey*);
};

#endif /* __cuebox_ui_h__ */
