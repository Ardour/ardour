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
#include "canvas/table.h"

namespace ArdourWidgets {
	class ArdourButton;
}

namespace ArdourCanvas {
	class Text;
	class Polygon;
	class Widget;
	class Rectangle;
};

class TriggerUI : public ArdourCanvas::Table, public sigc::trackable
{
  public:
	TriggerUI (ArdourCanvas::Item* parent, ARDOUR::Trigger&);
	~TriggerUI ();

  private:
	ARDOUR::Trigger& trigger;

	ArdourWidgets::ArdourButton* _follow_action_button;
	ArdourCanvas::Widget* follow_action_button;


	ArdourWidgets::ArdourDropdown* _follow_left;
	ArdourCanvas::Widget* follow_left;
	ArdourWidgets::ArdourButton* _follow_left_percentage;
	ArdourCanvas::Widget* follow_left_percentage;

	ArdourWidgets::ArdourDropdown* _follow_right;
	ArdourCanvas::Widget* follow_right;
	ArdourWidgets::ArdourButton* _follow_right_percentage;
	ArdourCanvas::Widget* follow_right_percentage;

	ArdourCanvas::Rectangle* percentage_slider;

	ArdourCanvas::Rectangle* follow_count;
	ArdourCanvas::Text* follow_count_text;

	ArdourCanvas::Rectangle* legato;
	ArdourCanvas::Rectangle* legato_text;

	ArdourCanvas::Rectangle* quantize;
	ArdourCanvas::Rectangle* quantize_text;

	ArdourCanvas::Rectangle* velocity;
	ArdourCanvas::Rectangle* velocity_text;

	void trigger_changed (PBD::PropertyChange);

	bool follow_action_button_event (GdkEvent*);

	PBD::ScopedConnectionList trigger_connections;

	static std::string follow_action_to_string (ARDOUR::Trigger::FollowAction);
	static ARDOUR::Trigger::FollowAction  string_to_follow_action (std::string const &);
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
