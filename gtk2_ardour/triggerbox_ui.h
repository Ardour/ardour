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

#include "canvas/box.h"
#include "canvas/canvas.h"
#include "canvas/rectangle.h"

namespace ARDOUR {
	class Trigger;
	class TriggerBox;
}

namespace ArdourCanvas {
	class Text;
	class Polygon;
};

class TriggerEntry : public ArdourCanvas::Rectangle
{
  public:
	TriggerEntry (ArdourCanvas::Item* parent, ARDOUR::Trigger&);
	~TriggerEntry ();

	ARDOUR::Trigger& trigger() const { return _trigger; }

	ArdourCanvas::Polygon* play_button;
	ArdourCanvas::Text*    name_text;

  private:
	ARDOUR::Trigger& _trigger;
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

	bool bang (GdkEvent*, size_t);
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
};

#endif /* __ardour_gtk_triggerbox_ui_h__ */
