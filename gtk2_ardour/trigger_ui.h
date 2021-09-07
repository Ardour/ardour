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

#ifndef __ardour_gtk_trigger_ui_h__
#define __ardour_gtk_trigger_ui_h__

#include "ardour/triggerbox.h"

#include "canvas/box.h"
#include "canvas/canvas.h"
#include "canvas/rectangle.h"

namespace ArdourCanvas {
	class Text;
	class Polygon;
};

class TriggerUI : public ArdourCanvas::Box
{
  public:
	TriggerUI (ArdourCanvas::Item* parent, ARDOUR::Trigger&);
	~TriggerUI ();

  private:
	ARDOUR::Trigger& trigger;

	ArdourCanvas::Rectangle*  follow_label;
	ArdourCanvas::Text*  follow_text;

	ArdourCanvas::Rectangle* follow_left;
	ArdourCanvas::Text* follow_left_text;
	ArdourCanvas::Rectangle* follow_left_percentage;
	ArdourCanvas::Text* follow_left_percentage_text;

	ArdourCanvas::Rectangle* follow_right;
	ArdourCanvas::Text* follow_right_text;
	ArdourCanvas::Rectangle* follow_right_percentage;
	ArdourCanvas::Text* follow_right_percentage_text;

	ArdourCanvas::Rectangle* percentage_slider;

	ArdourCanvas::Rectangle* follow_count;
	ArdourCanvas::Text* follow_count_text;

	ArdourCanvas::Rectangle* legato;
	ArdourCanvas::Rectangle* legato_text;

	ArdourCanvas::Rectangle* quantize;
	ArdourCanvas::Rectangle* quantize_text;

	ArdourCanvas::Rectangle* velocity;
	ArdourCanvas::Rectangle* velocity_text;
};

class TriggerWidget : public ArdourCanvas::GtkCanvas
{
  public:
	TriggerWidget (ARDOUR::Trigger& tb);
	void size_request (double& w, double& h) const;

  private:
	TriggerUI* ui;
};

/* XXX probably for testing only */

class TriggerWindow : public Gtk::Window
{
    public:
	TriggerWindow (ARDOUR::Trigger&);

	bool on_key_press_event (GdkEventKey*);
	bool on_key_release_event (GdkEventKey*);
};

#endif /* __ardour_gtk_trigger_ui_h__ */
