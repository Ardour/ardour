/*
 * Copyright (C) 2005-2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2005-2017 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2005-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2010 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2015 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2013-2016 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2013-2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015 André Nusser <andre.nusser@googlemail.com>
 * Copyright (C) 2016-2018 Len Ovens <len@ovenwerks.net>
 * Copyright (C) 2017 Johannes Mueller <github@johannes-mueller.org>
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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#include "gtk2ardour-version.h"
#endif

#include "ardour_ui.h"
#include "debug.h"
#include "keyboard.h"
#include "public_editor.h"
#include "virtual_keyboard_window.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace ArdourWidgets;
using namespace Gtk;
using namespace std;

bool
ARDOUR_UI::key_event_handler (GdkEventKey* ev, Gtk::Window* event_window)
{
	Gtkmm2ext::Bindings* bindings = 0;
	Gtk::Window* window = 0;

	if (virtual_keyboard_window && virtual_keyboard_window->get_visible()) {
		if (gtk_window_propagate_key_event (virtual_keyboard_window->gobj(), ev)) {
			return true;
		}
	}

	/* until we get ardour bindings working, we are not handling key
	 * releases yet.
	 */

	if (ev->type != GDK_KEY_PRESS) {
		return false;
	}

	if (event_window == &_main_window) {

		window = event_window;

		/* find current tab contents */

		Gtk::Widget* w = _tabs.get_nth_page (_tabs.get_current_page());

		/* see if it uses the ardour binding system */

		if (w) {
			bindings = reinterpret_cast<Gtkmm2ext::Bindings*>(w->get_data ("ardour-bindings"));
		}

		DEBUG_TRACE (DEBUG::Accelerators, string_compose ("main window key event, bindings = %1, global = %2\n", bindings, &global_bindings));

	} else {

		window = event_window;

		/* see if window uses ardour binding system */

		bindings = reinterpret_cast<Gtkmm2ext::Bindings*>(window->get_data ("ardour-bindings"));
	}

	/* An empty binding set is treated as if it doesn't exist */

	if (bindings && bindings->empty()) {
		bindings = 0;
	}

	return key_press_focus_accelerator_handler (*window, ev, bindings);
}

static Gtkmm2ext::Bindings*
get_bindings_from_widget_hierarchy (GtkWidget** w)
{
	void* p = NULL;

	while (*w) {
		if ((p = g_object_get_data (G_OBJECT(*w), "ardour-bindings")) != 0) {
			break;
		}
		*w = gtk_widget_get_parent (*w);
	}

	return reinterpret_cast<Gtkmm2ext::Bindings*> (p);
}

bool
ARDOUR_UI::key_press_focus_accelerator_handler (Gtk::Window& window, GdkEventKey* ev, Gtkmm2ext::Bindings* top_level_bindings)
{
	GtkWindow* win = window.gobj();
	GtkWidget* focus = gtk_window_get_focus (win);
	bool special_handling_of_unmodified_accelerators = false;
	const guint mask = (Keyboard::RelevantModifierKeyMask & ~(Gdk::SHIFT_MASK|Gdk::LOCK_MASK));
	bool cutcopypaste = false;

	if (focus) {

		/* some widget has keyboard focus */

		if (GTK_IS_ENTRY(focus) || Keyboard::some_magic_widget_has_focus()) {

			/* A particular kind of focusable widget currently has keyboard
			 * focus. All unmodified key events should go to that widget
			 * first and not be used as an accelerator by default
			 */

			special_handling_of_unmodified_accelerators = true;

		}
	}

	DEBUG_TRACE (DEBUG::Accelerators, string_compose ("Win = %1 [title = %9] focus = %7 (%8) Key event: code = %2 [%10] state = %3 special handling ? %4 magic widget focus ? %5 focus widget %6 named %7 mods ? %8\n",
	                                                  win,
	                                                  ev->keyval,
	                                                  Gtkmm2ext::show_gdk_event_state (ev->state),
                                                          special_handling_of_unmodified_accelerators,
                                                          Keyboard::some_magic_widget_has_focus(),
	                                                  focus,
                                                          (focus ? gtk_widget_get_name (focus) : "no focus widget"),
                                                          ((ev->state & mask) ? "yes" : "no"),
	                                                  window.get_title(),
	                                                  gdk_keyval_name (ev->keyval)));

	if (Keyboard::some_magic_widget_has_focus() && (ev->state == Keyboard::PrimaryModifier)) {
		switch (ev->keyval) {
		case GDK_x:
		case GDK_c:
		case GDK_v:
			cutcopypaste = true;
			DEBUG_TRACE (DEBUG::Accelerators, string_compose ("seen cut/copy/paste keys with magic widget focus, Primary-%1\n", gdk_keyval_name (ev->keyval)));
			break;
		default:
			break;
		}
	}

	/* This exists to allow us to override the way GTK handles
	   key events. The normal sequence is:

	   a) event is delivered to a GtkWindow
	   b) accelerators/mnemonics are activated
	   c) if (b) didn't handle the event, propagate to
	       the focus widget and/or focus chain

	   The problem with this is that if the accelerators include
	   keys without modifiers, such as the space bar or the
	   letter "e", then pressing the key while typing into
	   a text entry widget results in the accelerator being
	   activated, instead of the desired letter appearing
	   in the text entry.

	   There is no good way of fixing this, but this
	   represents a compromise. The idea is that
	   key events involving modifiers (not Shift)
	   get routed into the activation pathway first, then
	   get propagated to the focus widget if necessary.

	   If the key event doesn't involve modifiers,
	   we deliver to the focus widget first, thus allowing
	   it to get "normal text" without interference
	   from acceleration.

	   Of course, this can also be problematic: if there
	   is a widget with focus, then it will swallow
	   all "normal text" accelerators.
	*/


	if (!cutcopypaste && (!special_handling_of_unmodified_accelerators || (ev->state & mask))) {

		/* no special handling or there are modifiers in effect: accelerate first */

		DEBUG_TRACE (DEBUG::Accelerators, "\tactivate, then propagate\n");

		KeyboardKey k (ev->state, ev->keyval);

		/* Check hierarchy from current focus widget upwards */

		while (focus) {

			Gtkmm2ext::Bindings* focus_bindings = get_bindings_from_widget_hierarchy (&focus);

			if (focus_bindings) {
				DEBUG_TRACE (DEBUG::Accelerators, string_compose ("\tusing widget (%3) bindings %1 @ %2 for this event\n", focus_bindings->name(), focus_bindings, gtk_widget_get_name (focus)));
				if (focus_bindings->activate (k, Bindings::Press)) {
					return true;
				}
			}

			if (focus) {
				focus = gtk_widget_get_parent (focus);
			}
		}

		/* Use any "top level" bindings passed to us (could be from a
		 * top level tab or a top level window)
		 */

		if (top_level_bindings) {
			DEBUG_TRACE (DEBUG::Accelerators, string_compose ("\tusing top level bindings %1 @ %2 for this event\n", top_level_bindings->name(), top_level_bindings));
		}

		if (top_level_bindings && top_level_bindings->activate (k, Bindings::Press)) {
			DEBUG_TRACE (DEBUG::Accelerators, "\t\thandled\n");
			return true;
		}

		/* Use any global bindings */

		DEBUG_TRACE (DEBUG::Accelerators, string_compose ("\tnot yet handled, try global bindings (%1)\n", global_bindings));

		if (global_bindings && global_bindings->activate (k, Bindings::Press)) {
			DEBUG_TRACE (DEBUG::Accelerators, "\t\thandled\n");
			return true;
		}

		DEBUG_TRACE (DEBUG::Accelerators, "\tnot handled by binding activation, now propagate to window\n");

		if (window.get_realized () && (!window.get_focus() || window.get_focus()->get_realized ()) && gtk_window_propagate_key_event (win, ev)) {
			DEBUG_TRACE (DEBUG::Accelerators, "\tpropagate handled\n");
			return true;
		}

	} else {

		/* no modifiers or cut/copy/paste key event, propagate first */

		DEBUG_TRACE (DEBUG::Accelerators, "\tpropagate, then activate\n");

		if (window.get_realized () && (!window.get_focus() || window.get_focus()->get_realized ()) && gtk_window_propagate_key_event (win, ev)) {
			DEBUG_TRACE (DEBUG::Accelerators, "\thandled by propagate\n");
			return true;
		}

		DEBUG_TRACE (DEBUG::Accelerators, "\tpropagation didn't handle, so activate\n");
		KeyboardKey k (ev->state, ev->keyval);

		while (focus) {

			Gtkmm2ext::Bindings* focus_bindings = get_bindings_from_widget_hierarchy (&focus);

			if (focus_bindings) {
				DEBUG_TRACE (DEBUG::Accelerators, string_compose ("\t(nomod) using widget (%3) bindings %1 @ %2 for this event\n", focus_bindings->name(), focus_bindings, gtk_widget_get_name (focus)));
				if (focus_bindings->activate (k, Bindings::Press)) {
					return true;
				}
			}

			if (focus) {
				focus = gtk_widget_get_parent (focus);
			}
		}

		/* Use any "top level" bindings passed to us (could be from a
		 * top level tab or a top level window)
		 */

		if (top_level_bindings) {
			DEBUG_TRACE (DEBUG::Accelerators, string_compose ("\t(nomod) using top level bindings %1 @ %2 for this event\n", top_level_bindings->name(), top_level_bindings));
		}

		if (top_level_bindings && top_level_bindings->activate (k, Bindings::Press)) {
			DEBUG_TRACE (DEBUG::Accelerators, "\t\thandled\n");
			return true;
		}

		DEBUG_TRACE (DEBUG::Accelerators, string_compose ("\tnot yet handled, try global bindings (%1)\n", global_bindings));

		if (global_bindings && global_bindings->activate (k, Bindings::Press)) {
			DEBUG_TRACE (DEBUG::Accelerators, "\t\thandled\n");
			return true;
		}
	}

	DEBUG_TRACE (DEBUG::Accelerators, "\tnot handled\n");
	return true;
}


gint
ARDOUR_UI::transport_numpad_timeout ()
{
	_numpad_locate_happening = false;
	if (_numpad_timeout_connection.connected() )
		_numpad_timeout_connection.disconnect();
	return 1;
}

void
ARDOUR_UI::transport_numpad_decimal ()
{
	_numpad_timeout_connection.disconnect();

	if (_numpad_locate_happening) {
		if (editor) editor->goto_nth_marker(_pending_locate_num - 1);
		_numpad_locate_happening = false;
	} else {
		_pending_locate_num = 0;
		_numpad_locate_happening = true;
		_numpad_timeout_connection = Glib::signal_timeout().connect (mem_fun(*this, &ARDOUR_UI::transport_numpad_timeout), 2*1000);
	}
}

void
ARDOUR_UI::transport_numpad_event (int num)
{
	if ( _numpad_locate_happening ) {
		_pending_locate_num = _pending_locate_num*10 + num;
	} else {
		switch (num) {
			case 0: toggle_roll(false, false);                           break;
			case 1: transport_rewind();                                  break;
			case 2: transport_forward();                                 break;
			case 3: transport_record(true);                              break;
			case 4: toggle_session_auto_loop();                          break;
			case 5: transport_record(false); toggle_session_auto_loop(); break;
			case 6: toggle_punch();                                      break;
			case 7: toggle_click();                                      break;
			case 8: toggle_auto_return();                                break;
			case 9: toggle_follow_edits();                               break;
		}
	}
}
