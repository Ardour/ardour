/*
    Copyright (C) 1999-2005 Paul Barton-Davis 

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

    $Id$
*/

#include <cmath>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#include <climits>
#include <cctype>

#include <gtkmm.h>
#include <pbd/error.h>
#include <pbd/touchable.h>
#include <pbd/failed_constructor.h>
#include <pbd/pthread_utils.h>
#include <pbd/stacktrace.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/textviewer.h>
#include <gtkmm2ext/popup.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/window_title.h>

#include "i18n.h"

using namespace Gtkmm2ext;
using namespace Gtk;
using namespace Glib;
using namespace PBD;
using std::map;

pthread_t UI::gui_thread;
UI       *UI::theGtkUI = 0;

BaseUI::RequestType Gtkmm2ext::ErrorMessage = BaseUI::new_request_type();
BaseUI::RequestType Gtkmm2ext::Quit = BaseUI::new_request_type();
BaseUI::RequestType Gtkmm2ext::TouchDisplay = BaseUI::new_request_type();
BaseUI::RequestType Gtkmm2ext::StateChange = BaseUI::new_request_type();
BaseUI::RequestType Gtkmm2ext::SetTip = BaseUI::new_request_type();
BaseUI::RequestType Gtkmm2ext::AddIdle = BaseUI::new_request_type();
BaseUI::RequestType Gtkmm2ext::AddTimeout = BaseUI::new_request_type();

#include <pbd/abstract_ui.cc>  /* instantiate the template */


UI::UI (string namestr, int *argc, char ***argv) 
	: AbstractUI<UIRequest> (namestr, true)
{
	theMain = new Main (argc, argv);
#ifndef GTK_NEW_TOOLTIP_API
	tips = new Tooltips;
#endif

	_active = false;

	if (!theGtkUI) {
		theGtkUI = this;
		gui_thread = pthread_self ();
	} else {
		fatal << "duplicate UI requested" << endmsg;
		/* NOTREACHED */
	}

	/* add the pipe to the select/poll loop that GDK does */

	gdk_input_add (signal_pipe[0],
		       GDK_INPUT_READ,
		       UI::signal_pipe_callback,
		       this);

	errors = new TextViewer (850,100);
	errors->text().set_editable (false); 
	errors->text().set_name ("ErrorText");

	Glib::set_application_name(namestr);

	WindowTitle title(Glib::get_application_name());
	title += _("Log");
	errors->set_title (title.get_string());

	errors->dismiss_button().set_name ("ErrorLogCloseButton");
	errors->signal_delete_event().connect (bind (sigc::ptr_fun (just_hide_it), (Window *) errors));
	errors->set_type_hint (Gdk::WINDOW_TYPE_HINT_UTILITY);

	register_thread (pthread_self(), X_("GUI"));

	//load_rcfile (rcfile);
}

UI::~UI ()
{
}


bool
UI::caller_is_ui_thread ()
{
	return pthread_equal (gui_thread, pthread_self());
}

int
UI::load_rcfile (string path, bool themechange)
{
	if (path.length() == 0) {
		return -1;
	}

	if (access (path.c_str(), R_OK)) {
		error << "UI: couldn't find rc file \"" 
		      << path
		      << '"'
		      << endmsg;
		return -1;
	}
	
	RC rc (path.c_str());
	// RC::reset_styles (Gtk::Settings::get_default());
	gtk_rc_reset_styles (gtk_settings_get_default());
	theme_changed.emit();

	if (themechange) {
		return 0; //Don't continue on every time there is a theme change
	}

	/* have to pack widgets into a toplevel window so that styles will stick */

	Window temp_window (WINDOW_TOPLEVEL);
	HBox box;
	Label a_widget1;
	Label a_widget2;
	Label a_widget3;
	Label a_widget4;
	RefPtr<Gtk::Style> style;
	RefPtr<TextBuffer> buffer (errors->text().get_buffer());

	box.pack_start (a_widget1);
	box.pack_start (a_widget2);
	box.pack_start (a_widget3);
	box.pack_start (a_widget4);

	error_ptag = buffer->create_tag();
	error_mtag = buffer->create_tag();
	fatal_ptag = buffer->create_tag();
	fatal_mtag = buffer->create_tag();
	warning_ptag = buffer->create_tag();
	warning_mtag = buffer->create_tag();
	info_ptag = buffer->create_tag();
	info_mtag = buffer->create_tag();

	a_widget1.set_name ("FatalMessage");
	a_widget1.ensure_style ();
	style = a_widget1.get_style();

	fatal_ptag->property_font_desc().set_value(style->get_font());
	fatal_ptag->property_foreground_gdk().set_value(style->get_fg(STATE_ACTIVE));
	fatal_ptag->property_background_gdk().set_value(style->get_bg(STATE_ACTIVE));
	fatal_mtag->property_font_desc().set_value(style->get_font());
	fatal_mtag->property_foreground_gdk().set_value(style->get_fg(STATE_NORMAL));
	fatal_mtag->property_background_gdk().set_value(style->get_bg(STATE_NORMAL));

	a_widget2.set_name ("ErrorMessage");
	a_widget2.ensure_style ();
	style = a_widget2.get_style();

	error_ptag->property_font_desc().set_value(style->get_font());
	error_ptag->property_foreground_gdk().set_value(style->get_fg(STATE_ACTIVE));
	error_ptag->property_background_gdk().set_value(style->get_bg(STATE_ACTIVE));
	error_mtag->property_font_desc().set_value(style->get_font());
	error_mtag->property_foreground_gdk().set_value(style->get_fg(STATE_NORMAL));
	error_mtag->property_background_gdk().set_value(style->get_bg(STATE_NORMAL));

	a_widget3.set_name ("WarningMessage");
	a_widget3.ensure_style ();
	style = a_widget3.get_style();

	warning_ptag->property_font_desc().set_value(style->get_font());
	warning_ptag->property_foreground_gdk().set_value(style->get_fg(STATE_ACTIVE));
	warning_ptag->property_background_gdk().set_value(style->get_bg(STATE_ACTIVE));
	warning_mtag->property_font_desc().set_value(style->get_font());
	warning_mtag->property_foreground_gdk().set_value(style->get_fg(STATE_NORMAL));
	warning_mtag->property_background_gdk().set_value(style->get_bg(STATE_NORMAL));

	a_widget4.set_name ("InfoMessage");
	a_widget4.ensure_style ();
	style = a_widget4.get_style();

	info_ptag->property_font_desc().set_value(style->get_font());
	info_ptag->property_foreground_gdk().set_value(style->get_fg(STATE_ACTIVE));
	info_ptag->property_background_gdk().set_value(style->get_bg(STATE_ACTIVE));
	info_mtag->property_font_desc().set_value(style->get_font());
	info_mtag->property_foreground_gdk().set_value(style->get_fg(STATE_NORMAL));
	info_mtag->property_background_gdk().set_value(style->get_bg(STATE_NORMAL));

	return 0;
}

void
UI::run (Receiver &old_receiver)
{
	listen_to (error);
	listen_to (info);
	listen_to (warning);
	listen_to (fatal);

	old_receiver.hangup ();
	starting ();
	_active = true;	
	theMain->run ();
	_active = false;
	stopping ();
	hangup ();
	return;
}

bool
UI::running ()
{
	return _active;
}

void
UI::kill ()
{
	if (_active) {
		pthread_kill (gui_thread, SIGKILL);
	} 
}

void
UI::quit ()
{
	UIRequest *req = get_request (Quit);

	if (req == 0) {
		return;
	}

	send_request (req);
}

static bool idle_quit ()
{
	Main::quit ();
	return true;
}

void
UI::do_quit ()
{
	if (getenv ("ARDOUR_RUNNING_UNDER_VALGRIND")) {
		Main::quit ();
	} else {
		Glib::signal_idle().connect (sigc::ptr_fun (idle_quit));
	}
}

void
UI::touch_display (Touchable *display)
{
	UIRequest *req = get_request (TouchDisplay);

	if (req == 0) {
		return;
	}

	req->display = display;

	send_request (req);
}	

void
UI::set_tip (Widget *w, const gchar *tip, const gchar *hlp)
{
	UIRequest *req = get_request (SetTip);

	if (req == 0) {
		return;
	}

	req->widget = w;
	req->msg = tip;
	req->msg2 = hlp;

	send_request (req);
}

void
UI::set_state (Widget *w, StateType state)
{
	UIRequest *req = get_request (StateChange);
	
	if (req == 0) {
		return;
	}

	req->new_state = state;
	req->widget = w;

	send_request (req);
}

void
UI::idle_add (int (*func)(void *), void *arg)
{
	UIRequest *req = get_request (AddIdle);

	if (req == 0) {
		return;
	}

	req->function = func;
	req->arg = arg;

	send_request (req);
}

/* END abstract_ui interfaces */

void
UI::signal_pipe_callback (void *arg, int fd, GdkInputCondition cond)
{
	char buf[256];
	
	/* flush (nonblocking) pipe */
	
	while (read (fd, buf, 256) > 0);
	
	((UI *) arg)->handle_ui_requests ();
}

void
UI::do_request (UIRequest* req)
{
	if (req->type == ErrorMessage) {

		process_error_message (req->chn, req->msg);
		free (const_cast<char*>(req->msg)); /* it was strdup'ed */
		req->msg = 0; /* don't free it again in the destructor */

	} else if (req->type == Quit) {

		do_quit ();

	} else if (req->type == CallSlot) {

		req->slot ();

	} else if (req->type == TouchDisplay) {

		req->display->touch ();
		if (req->display->delete_after_touch()) {
			delete req->display;
		}

	} else if (req->type == StateChange) {

		req->widget->set_state (req->new_state);

	} else if (req->type == SetTip) {

#ifdef GTK_NEW_TOOLTIP_API
		/* even if the installed GTK is up to date,
		   at present (November 2008) our included
		   version of gtkmm is not. so use the GTK
		   API that we've verified has the right function.
		*/
		gtk_widget_set_tooltip_text (req->widget->gobj(), req->msg);
#else
		tips->set_tip (*req->widget, req->msg, "");
#endif

	} else {

		error << "GtkUI: unknown request type "
		      << (int) req->type
		      << endmsg;
	}	       
}

/*======================================================================
  Error Display
  ======================================================================*/

void
UI::receive (Transmitter::Channel chn, const char *str)
{
	if (caller_is_ui_thread()) {
		process_error_message (chn, str);
	} else {
		UIRequest* req = get_request (ErrorMessage);

		if (req == 0) {
			return;
		}

		req->chn = chn;
		req->msg = strdup (str);

		send_request (req);
	}
}

#define OLD_STYLE_ERRORS 1

void
UI::process_error_message (Transmitter::Channel chn, const char *str)
{
	RefPtr<Style> style;
	RefPtr<TextBuffer::Tag> ptag;
	RefPtr<TextBuffer::Tag> mtag;
	const char *prefix;
	size_t prefix_len;
	bool fatal_received = false;
#ifndef OLD_STYLE_ERRORS
	PopUp* popup = new PopUp (WIN_POS_CENTER, 0, true);
#endif

	switch (chn) {
	case Transmitter::Fatal:
		prefix = "[FATAL]: ";
		ptag = fatal_ptag;
		mtag = fatal_mtag;
		prefix_len = 9;
		fatal_received = true;
		break;
	case Transmitter::Error:
#if OLD_STYLE_ERRORS
		prefix = "[ERROR]: ";
		ptag = error_ptag;
		mtag = error_mtag;
		prefix_len = 9;
#else
		popup->set_name ("ErrorMessage");
		popup->set_text (str);
		popup->touch ();
		return;
#endif
		break;
	case Transmitter::Info:
#if OLD_STYLE_ERRORS	
		prefix = "[INFO]: ";
		ptag = info_ptag;
		mtag = info_mtag;
		prefix_len = 8;
#else
		popup->set_name ("InfoMessage");
		popup->set_text (str);
		popup->touch ();
		return;
#endif

		break;
	case Transmitter::Warning:
#if OLD_STYLE_ERRORS
		prefix = "[WARNING]: ";
		ptag = warning_ptag;
		mtag = warning_mtag;
		prefix_len = 11;
#else
		popup->set_name ("WarningMessage");
		popup->set_text (str);
		popup->touch ();
		return;
#endif
		break;
	default:
		/* no choice but to use text/console output here */
		cerr << "programmer error in UI::check_error_messages (channel = " << chn << ")\n";
		::exit (1);
	}
	
	errors->text().get_buffer()->begin_user_action();

	if (fatal_received) {
		handle_fatal (str);
	} else {
		
		display_message (prefix, prefix_len, ptag, mtag, str);
		
		if (!errors->is_visible()) {
			toggle_errors();
		}
	}

	errors->text().get_buffer()->end_user_action();
}

void
UI::toggle_errors ()
{
	if (!errors->is_visible()) {
		errors->set_position (WIN_POS_MOUSE);
		errors->show ();
	} else {
		errors->hide ();
	}
}

void
UI::display_message (const char *prefix, gint prefix_len, RefPtr<TextBuffer::Tag> ptag, RefPtr<TextBuffer::Tag> mtag, const char *msg)
{
	RefPtr<TextBuffer> buffer (errors->text().get_buffer());

	buffer->insert_with_tag(buffer->end(), prefix, ptag);
	buffer->insert_with_tag(buffer->end(), msg, mtag);
	buffer->insert_with_tag(buffer->end(), "\n", mtag);

	errors->scroll_to_bottom ();
}	

void
UI::handle_fatal (const char *message)
{
	Window win (WINDOW_POPUP);
	VBox packer;
	Label label (message);
	Button quit (_("Press To Exit"));

	win.set_default_size (400, 100);
	
	string title;
	title = name();
	title += ": Fatal Error";
	win.set_title (title);

	win.set_position (WIN_POS_MOUSE);
	win.add (packer);

	packer.pack_start (label, true, true);
	packer.pack_start (quit, false, false);
	quit.signal_clicked().connect(mem_fun(*this,&UI::quit));
	
	win.show_all ();
	win.set_modal (true);

	theMain->run ();
	
	exit (1);
}

void
UI::popup_error (const char *text)
{
	PopUp *pup;

	if (!caller_is_ui_thread()) {
		error << "non-UI threads can't use UI::popup_error" 
		      << endmsg;
		return;
	}
	
	pup = new PopUp (WIN_POS_MOUSE, 0, true);
	pup->set_text (text);
	pup->touch ();
}

#ifdef GTKOSX
extern "C" {
	int gdk_quartz_in_carbon_menu_event_handler ();
}
#endif

void
UI::flush_pending ()
{
#ifdef GTKOSX
	/* as of february 11th 2008, gtk/osx has a problem in that mac menu events
	   are handled using Carbon with an "internal" event handling system that 
	   doesn't pass things back to the glib/gtk main loop. this makes
	   gtk_main_iteration() block if we call it while in a menu event handler 
	   because glib gets confused and thinks there are two threads running
	   g_main_poll_func(). 

	   this hack (relies on code in gtk2_ardour/sync-menu.c) works
	   around that.
	*/

	if (gdk_quartz_in_carbon_menu_event_handler()) {
		return;
	}
#endif
	if (!caller_is_ui_thread()) {
		error << "non-UI threads cannot call UI::flush_pending()"
		      << endmsg;
		return;
	}

	gtk_main_iteration();

	while (gtk_events_pending()) {
		gtk_main_iteration();
	}
}

bool
UI::just_hide_it (GdkEventAny *ev, Window *win)
{
	win->hide ();
	return true;
}

Gdk::Color
UI::get_color (const string& prompt, bool& picked, const Gdk::Color* initial)
{
	Gdk::Color color;

	ColorSelectionDialog color_dialog (prompt);

	color_dialog.set_modal (true);
	color_dialog.get_cancel_button()->signal_clicked().connect (bind (mem_fun (*this, &UI::color_selection_done), false));
	color_dialog.get_ok_button()->signal_clicked().connect (bind (mem_fun (*this, &UI::color_selection_done), true));
	color_dialog.signal_delete_event().connect (mem_fun (*this, &UI::color_selection_deleted));

	if (initial) {
		color_dialog.get_colorsel()->set_current_color (*initial);
	}

	color_dialog.show_all ();
	color_picked = false;
	picked = false;

	Main::run();

	color_dialog.hide_all ();

	if (color_picked) {
		Gdk::Color f_rgba = color_dialog.get_colorsel()->get_current_color ();
		color.set_red(f_rgba.get_red());
		color.set_green(f_rgba.get_green());
		color.set_blue(f_rgba.get_blue());

		picked = true;
	}

	return color;
}

void
UI::color_selection_done (bool status)
{
	color_picked = status;
	Main::quit ();
}

bool
UI::color_selection_deleted (GdkEventAny *ev)
{
	Main::quit ();
	return true;
}
