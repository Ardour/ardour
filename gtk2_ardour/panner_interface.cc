/*
    Copyright (C) 2011 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <gtkmm.h>
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/persistent_tooltip.h"
#include "panner_interface.h"
#include "panner_editor.h"
#include "global_signals.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;
using namespace Gtkmm2ext;

PannerInterface::PannerInterface (boost::shared_ptr<Panner> p)
	: _panner (p)
	, _tooltip (this)
	, _editor (0)
{
        set_flags (Gtk::CAN_FOCUS);

        add_events (Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK|
                    Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK|
                    Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|
                    Gdk::SCROLL_MASK|
                    Gdk::POINTER_MOTION_MASK);

}

PannerInterface::~PannerInterface ()
{
	delete _editor;
}

bool
PannerInterface::on_enter_notify_event (GdkEventCrossing *)
{
	grab_focus ();
	Keyboard::magic_widget_grab_focus ();
	return false;
}

bool
PannerInterface::on_leave_notify_event (GdkEventCrossing *)
{
	Keyboard::magic_widget_drop_focus ();
	return false;
}

bool
PannerInterface::on_key_release_event (GdkEventKey*)
{
	return false;
}

void
PannerInterface::value_change ()
{
	set_tooltip ();
	queue_draw ();
}

bool
PannerInterface::on_button_press_event (GdkEventButton* ev)
{
	if (Gtkmm2ext::Keyboard::is_edit_event (ev)) {
		edit ();
		return true;
	}

	return false;
}

bool
PannerInterface::on_button_release_event (GdkEventButton* ev)
{
	if (Gtkmm2ext::Keyboard::is_edit_event (ev)) {
		/* We edited on the press, so claim the release */
		return true;
	}

	return false;
}

void
PannerInterface::edit ()
{
	delete _editor;
	_editor = editor ();
	_editor->show ();
}

PannerPersistentTooltip::PannerPersistentTooltip (Gtk::Widget* w)
	: PersistentTooltip (w)
	, _dragging (false)
{

}

void
PannerPersistentTooltip::target_start_drag ()
{
	_dragging = true;
}

void
PannerPersistentTooltip::target_stop_drag ()
{
	_dragging = false;
}

bool
PannerPersistentTooltip::dragging () const
{
	return _dragging;
}
