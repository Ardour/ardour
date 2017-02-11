/*
    Copyright (C) 2000 Paul Davis

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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

/* this file exists solely to break compilation dependencies that
   would connect changes to the mixer or editor objects.
*/

#include <cstdio>

#include "pbd/error.h"

#include "ardour/session.h"

#include "gtkmm2ext/bindings.h"

#include "actions.h"
#include "ardour_ui.h"
#include "public_editor.h"
#include "meterbridge.h"
#include "luawindow.h"
#include "mixer_ui.h"
#include "keyboard.h"
#include "splash.h"
#include "rc_option_editor.h"
#include "route_params_ui.h"
#include "time_info_box.h"
#include "opts.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace PBD;

namespace ARDOUR {
	class Session;
	class Route;
}

using namespace ARDOUR;

void
ARDOUR_UI::we_have_dependents ()
{
	install_actions ();
	load_bindings ();

	ProcessorBox::register_actions ();

	/* Global, editor, mixer, processor box actions are defined now. Link
	   them with any bindings, so that GTK does not get a chance to define
	   the GTK accel map entries first when we ask the GtkUIManager to
	   create menus/widgets.

	   If GTK adds the actions to its accel map before we do, we lose our
	   freedom to use any keys. More precisely, we can use any keys, but
	   ones that GTK considers illegal as accelerators will not show up in
	   menus.

	   There are other dynamic actions that can be created by a monitor
	   section, by step entry dialogs. These need to be handled
	   separately. They don't tend to use GTK-illegal bindings and more
	   importantly they don't have menus showing the bindings, so it is
	   less of an issue.
	*/

	Gtkmm2ext::Bindings::associate_all ();

	editor->setup_tooltips ();
	editor->UpdateAllTransportClocks.connect (sigc::mem_fun (*this, &ARDOUR_UI::update_transport_clocks));

	/* catch up on tabbable state, in the right order to leave the editor
	 * selected by default
	 */

	tabbable_state_change (*rc_option_editor);
	tabbable_state_change (*mixer);
	tabbable_state_change (*editor);

	/* all actions are defined */

	ActionManager::load_menus (ARDOUR_COMMAND_LINE::menus_file);

	/* catch up on parameters */

	boost::function<void (std::string)> pc (boost::bind (&ARDOUR_UI::parameter_changed, this, _1));
	Config->map_parameters (pc);

	UIConfiguration::instance().reset_dpi ();
}

void
ARDOUR_UI::connect_dependents_to_session (ARDOUR::Session *s)
{
	DisplaySuspender ds;
	BootMessage (_("Setup Editor"));
	editor->set_session (s);
	BootMessage (_("Setup Mixer"));
	mixer->set_session (s);
	meterbridge->set_session (s);
	luawindow->set_session (s);

	/* its safe to do this now */

	BootMessage (_("Reload Session History"));
	s->restore_history ("");
}

/** The main editor window has been closed */
gint
ARDOUR_UI::exit_on_main_window_close (GdkEventAny * /*ev*/)
{
#ifdef TOP_MENUBAR
	/* just hide the window, and return - the top menu stays up */
	editor->hide ();
	return TRUE;
#else
	/* time to get out of here */
	finish();
	return TRUE;
#endif
}

GtkNotebook*
ARDOUR_UI::tab_window_root_drop (GtkNotebook* src,
				 GtkWidget* w,
				 gint x,
				 gint y,
				 gpointer)
{
	using namespace std;
	Gtk::Notebook* nb = 0;
	Gtk::Window* win = 0;
	Gtkmm2ext::Tabbable* tabbable = 0;


	if (w == GTK_WIDGET(editor->contents().gobj())) {
		tabbable = editor;
	} else if (w == GTK_WIDGET(mixer->contents().gobj())) {
		tabbable = mixer;
	} else if (w == GTK_WIDGET(rc_option_editor->contents().gobj())) {
		tabbable = rc_option_editor;
	} else {
		return 0;
	}

	nb = tabbable->tab_root_drop ();
	win = tabbable->own_window ();

	if (nb) {
		win->move (x, y);
		win->show_all ();
		win->present ();
		return nb->gobj();
	}

	return 0; /* what was that? */
}

bool
ARDOUR_UI::idle_ask_about_quit ()
{
	if (_session && _session->dirty()) {
		finish ();
	} else {
		/* no session or session not dirty, but still ask anyway */

		Gtk::MessageDialog msg (string_compose (_("Quit %1?"), PROGRAM_NAME),
		                        false, /* no markup */
		                        Gtk::MESSAGE_INFO,
		                        Gtk::BUTTONS_YES_NO,
		                        true); /* modal */
		msg.set_default_response (Gtk::RESPONSE_YES);

		if (msg.run() == Gtk::RESPONSE_YES) {
			finish ();
		}
	}

	/* not reached but keep the compiler happy */

	return false;
}

bool
ARDOUR_UI::main_window_delete_event (GdkEventAny* ev)
{
	/* quit the application as soon as we go idle. If we call this here,
	 * the window manager/desktop can think we're taking too longer to
	 * handle the "delete" event
	 */

	Glib::signal_idle().connect (sigc::mem_fun (*this, &ARDOUR_UI::idle_ask_about_quit));

	return true;
}

static GtkNotebook*
tab_window_root_drop (GtkNotebook* src,
		      GtkWidget* w,
		      gint x,
		      gint y,
		      gpointer user_data)
{
	return ARDOUR_UI::instance()->tab_window_root_drop (src, w, x, y, user_data);
}

int
ARDOUR_UI::setup_windows ()
{
	/* actions do not need to be defined when we load keybindings. They
	 * will be lazily discovered. But bindings do need to exist when we
	 * create windows/tabs with their own binding sets.
	 */

	keyboard->setup_keybindings ();

	_tabs.set_show_border(false);
	_tabs.signal_switch_page().connect (sigc::mem_fun (*this, &ARDOUR_UI::tabs_switch));
	_tabs.signal_page_added().connect (sigc::mem_fun (*this, &ARDOUR_UI::tabs_page_added));
	_tabs.signal_page_removed().connect (sigc::mem_fun (*this, &ARDOUR_UI::tabs_page_removed));

	rc_option_editor = new RCOptionEditor;
	rc_option_editor->StateChange.connect (sigc::mem_fun (*this, &ARDOUR_UI::tabbable_state_change));

	if (create_editor ()) {
		error << _("UI: cannot setup editor") << endmsg;
		return -1;
	}

	if (create_mixer ()) {
		error << _("UI: cannot setup mixer") << endmsg;
		return -1;
	}

	if (create_meterbridge ()) {
		error << _("UI: cannot setup meterbridge") << endmsg;
		return -1;
	}

	if (create_luawindow ()) {
		error << _("UI: cannot setup luawindow") << endmsg;
		return -1;
	}

	/* order of addition affects order seen in initial window display */

	rc_option_editor->add_to_notebook (_tabs, _("Preferences"));
	mixer->add_to_notebook (_tabs, _("Mixer"));
	editor->add_to_notebook (_tabs, _("Editor"));

	time_info_box = new TimeInfoBox (false);
	/* all other dialogs are created conditionally */

	we_have_dependents ();

#ifdef TOP_MENUBAR
	EventBox* status_bar_event_box = manage (new EventBox);

	status_bar_event_box->add (status_bar_label);
	status_bar_event_box->add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	status_bar_label.set_size_request (300, -1);

	status_bar_label.show ();
	status_bar_event_box->show ();

	status_bar_event_box->signal_button_press_event().connect (mem_fun (*this, &ARDOUR_UI::status_bar_button_press));

	status_bar_hpacker.pack_start (*status_bar_event_box, true, true, 6);
	status_bar_hpacker.pack_start (menu_bar_base, false, false, 2);
#else
	top_packer.pack_start (menu_bar_base, false, false);
#endif

	main_vpacker.pack_start (top_packer, false, false);

	/* now add the transport frame to the top of main window */

	main_vpacker.pack_start (transport_frame, false, false);
	main_vpacker.pack_start (_tabs, true, true);

#ifdef TOP_MENUBAR
	main_vpacker.pack_start (status_bar_hpacker, false, false);
#endif

	for (int i = 0; i < 9; ++i) {
		std::string const a = string_compose (X_("script-action-%1"), i + 1);
		Glib::RefPtr<Action> act = ActionManager::get_action(X_("Editor"), a.c_str());
		assert (act);
		action_script_call_btn[i].set_text (string_compose ("%1", i+1));
		action_script_call_btn[i].set_related_action (act);
		if (act->get_sensitive ()) {
			action_script_call_btn[i].set_visual_state (Gtkmm2ext::VisualState (action_script_call_btn[i].visual_state() & ~Gtkmm2ext::Insensitive));
		} else {
			action_script_call_btn[i].set_visual_state (Gtkmm2ext::VisualState (action_script_call_btn[i].visual_state() | Gtkmm2ext::Insensitive));
		}
		const int row = i % 2;
		const int col = i / 2;
		action_script_table.attach (action_script_call_btn[i], col, col + 1, row, row + 1, EXPAND, EXPAND, 1, 0);
		action_script_call_btn[i].set_no_show_all ();
	}
	action_script_table.show ();

	setup_transport();
	build_menu_bar ();
	setup_tooltips ();

	_main_window.signal_delete_event().connect (sigc::mem_fun (*this, &ARDOUR_UI::main_window_delete_event));

	/* pack the main vpacker into the main window and show everything
	 */

	_main_window.add (main_vpacker);
	transport_frame.show_all ();

	const XMLNode* mnode = main_window_settings ();

	if (mnode) {
		XMLProperty const * prop;
		gint x = -1;
		gint y = -1;
		gint w = -1;
		gint h = -1;

		if ((prop = mnode->property (X_("x"))) != 0) {
			x = atoi (prop->value());
		}

		if ((prop = mnode->property (X_("y"))) != 0) {
			y = atoi (prop->value());
		}

		if ((prop = mnode->property (X_("w"))) != 0) {
			w = atoi (prop->value());
		}

		if ((prop = mnode->property (X_("h"))) != 0) {
			h = atoi (prop->value());
		}

		if (x >= 0 && y >= 0 && w >= 0 && h >= 0) {
			_main_window.set_position (Gtk::WIN_POS_NONE);
		}

		if (x >= 0 && y >= 0) {
			_main_window.move (x, y);
		}

		if (w > 0 && h > 0) {
			_main_window.set_default_size (w, h);
		}

		std::string current_tab;

		if ((prop = mnode->property (X_("current-tab"))) != 0) {
			current_tab = prop->value();
		} else {
			current_tab = "editor";
		}
		if (mixer && current_tab == "mixer") {
			_tabs.set_current_page (_tabs.page_num (mixer->contents()));
		} else if (rc_option_editor && current_tab == "preferences") {
			_tabs.set_current_page (_tabs.page_num (rc_option_editor->contents()));
		} else if (editor) {
			_tabs.set_current_page (_tabs.page_num (editor->contents()));
		}
	}

	setup_toplevel_window (_main_window, "", this);
	_main_window.show_all ();

	_tabs.set_show_tabs (false);

	/* It would be nice if Gtkmm had wrapped this rather than just
	 * deprecating the old set_window_creation_hook() method, but oh well...
	 */
	g_signal_connect (_tabs.gobj(), "create-window", (GCallback) ::tab_window_root_drop, this);

	return 0;
}
