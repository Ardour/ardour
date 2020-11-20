/*
 * Copyright (C) 1999-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2007 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2007-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2011 Doug McLain <doug@nostar.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#include <cmath>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#include <climits>
#include <cctype>

#include <gtkmm.h>

#include "pbd/error.h"
#include "pbd/touchable.h"
#include "pbd/failed_constructor.h"
#include "pbd/localtime_r.h"
#include "pbd/pthread_utils.h"
#include "pbd/replace_all.h"

#include "gtkmm2ext/application.h"
#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/textviewer.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/window_title.h"
#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/activatable.h"
#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/gui_thread.h"

#include "pbd/i18n.h"

using namespace Gtkmm2ext;
using namespace Gtk;
using namespace Glib;
using namespace PBD;
using std::map;

UI*   UI::theGtkUI = 0;

BaseUI::RequestType Gtkmm2ext::NullMessage = BaseUI::new_request_type();
BaseUI::RequestType Gtkmm2ext::ErrorMessage = BaseUI::new_request_type();
BaseUI::RequestType Gtkmm2ext::TouchDisplay = BaseUI::new_request_type();
BaseUI::RequestType Gtkmm2ext::StateChange = BaseUI::new_request_type();
BaseUI::RequestType Gtkmm2ext::SetTip = BaseUI::new_request_type();
BaseUI::RequestType Gtkmm2ext::AddIdle = BaseUI::new_request_type();
BaseUI::RequestType Gtkmm2ext::AddTimeout = BaseUI::new_request_type();

#include "pbd/abstract_ui.cc"  /* instantiate the template */

template class AbstractUI<Gtkmm2ext::UIRequest>;

void
UI::event_loop_precall ()
{
}

UI::UI (string application_name, string thread_name, int *argc, char ***argv)
	: AbstractUI<UIRequest> (thread_name)
	, _receiver (*this)
	, global_bindings (0)
	, errors (0)
	, event_callback (boost::bind (&UI::event_loop_precall, this))
{
	theMain = new Main (argc, argv);

	char buf[18];
	/* pthread public name has a 16 char limit */
	snprintf (buf, sizeof (buf), "%.11sGUI", PROGRAM_NAME);
	pthread_set_name (buf);

	_active = false;

	if (!theGtkUI) {
		theGtkUI = this;
	} else {
		fatal << "duplicate UI requested" << endmsg;
		abort(); /* NOTREACHED */
	}

	/* the GUI event loop runs in the main thread of the app,
	   which is assumed to have called this.
	*/

	run_loop_thread = Threads::Thread::self();

	/* store "this" as the UI-for-thread of this thread, same argument
	   as for previous line.
	*/

	set_event_loop_for_thread (this);

	/* we will be receiving requests */

	EventLoop::register_request_buffer_factory ("gui", request_buffer_factory);

	/*
	 * every time the main loop runs (i.e. before any actual event handling
	 */

	event_callback.attach (MainContext::get_default());

	/* attach our request source to the default main context */

	attach_request_source ();

	errors = new TextViewer (800,600);
	errors->text().set_editable (false);
	errors->text().set_name ("ErrorText");
	errors->signal_unmap().connect (sigc::bind (sigc::ptr_fun (&ActionManager::uncheck_toggleaction), X_("Editor/toggle-log-window")));

	Glib::set_application_name (application_name);

	WindowTitle title(Glib::get_application_name());
	title += _("Log");
	errors->set_title (title.get_string());

	errors->dismiss_button().set_name ("ErrorLogCloseButton");
	errors->signal_delete_event().connect (sigc::bind (sigc::ptr_fun (just_hide_it), (Window *) errors));
	errors->set_type_hint (Gdk::WINDOW_TYPE_HINT_UTILITY);

	//load_rcfile (rcfile);

	/* instantiate the Application singleton */

	Application::instance();
}

UI::~UI ()
{
	_receiver.hangup ();
	delete (errors);
}

bool
UI::caller_is_ui_thread ()
{
	return Threads::Thread::self() == run_loop_thread;
}

int
UI::load_rcfile (string path, bool themechange)
{
	/* Yes, pointers to Glib::RefPtr.  If these are not kept around,
	 * a segfault somewhere deep in the wonderfully robust glib will result.
	 * This does not occur if wiget.get_style is used instead of rc.get_style below,
	 * except that doesn't actually work...
	 */

	static Glib::RefPtr<Style>* fatal_style   = 0;
	static Glib::RefPtr<Style>* error_style   = 0;
	static Glib::RefPtr<Style>* warning_style = 0;
	static Glib::RefPtr<Style>* info_style    = 0;
	static Glib::RefPtr<Style>* debug_style    = 0;

	if (path.length() == 0) {
		return -1;
	}

	if (!Glib::file_test (path, Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_REGULAR)) {
		error << "UI: couldn't find rc file \""
		      << path
		      << '"'
		      << endmsg;
		return -1;
	}

	RC rc (path.c_str());
	//this is buggy in gtkmm for some reason, so use C
	//RC::reset_styles (Gtk::Settings::get_default());
	gtk_rc_reset_styles (gtk_settings_get_default());

	theme_changed.emit();

	if (themechange) {
		return 0; //Don't continue on every time there is a theme change
	}

	/* have to pack widgets into a toplevel window so that styles will stick */

	Window temp_window (WINDOW_TOPLEVEL);
	temp_window.ensure_style ();

	HBox box;
	Label fatal_widget;
	Label error_widget;
	Label warning_widget;
	Label info_widget;
	Label debug_widget;
	RefPtr<Gtk::Style> style;
	RefPtr<TextBuffer> buffer (errors->text().get_buffer());

	box.pack_start (fatal_widget);
	box.pack_start (error_widget);
	box.pack_start (warning_widget);
	box.pack_start (info_widget);
	box.pack_start (debug_widget);

	fatal_ptag = buffer->create_tag();
	fatal_mtag = buffer->create_tag();
	error_ptag = buffer->create_tag();
	error_mtag = buffer->create_tag();
	warning_ptag = buffer->create_tag();
	warning_mtag = buffer->create_tag();
	info_ptag = buffer->create_tag();
	info_mtag = buffer->create_tag();
	debug_ptag = buffer->create_tag();
	debug_mtag = buffer->create_tag();

	fatal_widget.set_name ("FatalMessage");
	delete fatal_style;

	/* This next line and the similar ones below are sketchily
	 * guessed to fix #2885.  I think maybe that problems occur
	 * because with gtk_rc_get_style (to quote its docs) "no
	 * refcount is added to the returned style".  So I've switched
	 * this to use Glib::wrap with take_copy == true, which requires
	 * all the nasty casts and calls to plain-old-C GTK.
	 *
	 * At worst I think this causes a memory leak; at least it appears
	 * to fix the bug.
	 *
	 * I could be wrong about any or all of the above.
	 */
	fatal_style = new Glib::RefPtr<Style> (Glib::wrap (gtk_rc_get_style (reinterpret_cast<GtkWidget*> (fatal_widget.gobj())), true));

	fatal_ptag->property_font_desc().set_value((*fatal_style)->get_font());
	fatal_ptag->property_foreground_gdk().set_value((*fatal_style)->get_fg(STATE_ACTIVE));
	fatal_ptag->property_background_gdk().set_value((*fatal_style)->get_bg(STATE_ACTIVE));
	fatal_mtag->property_font_desc().set_value((*fatal_style)->get_font());
	fatal_mtag->property_foreground_gdk().set_value((*fatal_style)->get_fg(STATE_NORMAL));
	fatal_mtag->property_background_gdk().set_value((*fatal_style)->get_bg(STATE_NORMAL));

	error_widget.set_name ("ErrorMessage");
	delete error_style;
	error_style = new Glib::RefPtr<Style> (Glib::wrap (gtk_rc_get_style (reinterpret_cast<GtkWidget*> (error_widget.gobj())), true));

	error_ptag->property_font_desc().set_value((*error_style)->get_font());
	error_ptag->property_foreground_gdk().set_value((*error_style)->get_fg(STATE_ACTIVE));
	error_ptag->property_background_gdk().set_value((*error_style)->get_bg(STATE_ACTIVE));
	error_mtag->property_font_desc().set_value((*error_style)->get_font());
	error_mtag->property_foreground_gdk().set_value((*error_style)->get_fg(STATE_NORMAL));
	error_mtag->property_background_gdk().set_value((*error_style)->get_bg(STATE_NORMAL));

	warning_widget.set_name ("WarningMessage");
	delete warning_style;
	warning_style = new Glib::RefPtr<Style> (Glib::wrap (gtk_rc_get_style (reinterpret_cast<GtkWidget*> (warning_widget.gobj())), true));

	warning_ptag->property_font_desc().set_value((*warning_style)->get_font());
	warning_ptag->property_foreground_gdk().set_value((*warning_style)->get_fg(STATE_ACTIVE));
	warning_ptag->property_background_gdk().set_value((*warning_style)->get_bg(STATE_ACTIVE));
	warning_mtag->property_font_desc().set_value((*warning_style)->get_font());
	warning_mtag->property_foreground_gdk().set_value((*warning_style)->get_fg(STATE_NORMAL));
	warning_mtag->property_background_gdk().set_value((*warning_style)->get_bg(STATE_NORMAL));

	info_widget.set_name ("InfoMessage");
	delete info_style;
	info_style = new Glib::RefPtr<Style> (Glib::wrap (gtk_rc_get_style (reinterpret_cast<GtkWidget*> (info_widget.gobj())), true));

	info_ptag->property_font_desc().set_value((*info_style)->get_font());
	info_ptag->property_foreground_gdk().set_value((*info_style)->get_fg(STATE_ACTIVE));
	info_ptag->property_background_gdk().set_value((*info_style)->get_bg(STATE_ACTIVE));
	info_mtag->property_font_desc().set_value((*info_style)->get_font());
	info_mtag->property_foreground_gdk().set_value((*info_style)->get_fg(STATE_NORMAL));
	info_mtag->property_background_gdk().set_value((*info_style)->get_bg(STATE_NORMAL));

	debug_widget.set_name ("DebugMessage");
	delete debug_style;
	debug_style = new Glib::RefPtr<Style> (Glib::wrap (gtk_rc_get_style (reinterpret_cast<GtkWidget*> (debug_widget.gobj())), true));

	debug_ptag->property_font_desc().set_value((*debug_style)->get_font());
	debug_ptag->property_foreground_gdk().set_value((*debug_style)->get_fg(STATE_ACTIVE));
	debug_ptag->property_background_gdk().set_value((*debug_style)->get_bg(STATE_ACTIVE));
	debug_mtag->property_font_desc().set_value((*debug_style)->get_font());
	debug_mtag->property_foreground_gdk().set_value((*debug_style)->get_fg(STATE_NORMAL));
	debug_mtag->property_background_gdk().set_value((*debug_style)->get_bg(STATE_NORMAL));

	return 0;
}

void
UI::run (Receiver &old_receiver)
{
	_receiver.listen_to (debug);
	_receiver.listen_to (info);
	_receiver.listen_to (warning);
	_receiver.listen_to (error);
	_receiver.listen_to (fatal);

	/* stop the old receiver (text/console) once we hit the first idle */

	Glib::signal_idle().connect (bind_return (mem_fun (old_receiver, &Receiver::hangup), false));

	if (starting ()) {
		return;
	}

	_active = true;
	theMain->run ();
	_active = false;

	return;
}

bool
UI::running ()
{
	return _active;
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
UI::set_tip (Widget &w, const gchar *tip)
{
	set_tip(&w, tip, "");
}

void
UI::set_tip (Widget &w, const std::string& tip)
{
	set_tip(&w, tip.c_str(), "");
}

void
UI::set_tip (Widget *w, const gchar *tip, const gchar *hlp)
{
	UIRequest *req = get_request (SetTip);

	std::string msg(tip);

	Glib::RefPtr<Gtk::Action> action = w->get_action();

	if (!action) {
		Gtkmm2ext::Activatable* activatable;
		if ((activatable = dynamic_cast<Gtkmm2ext::Activatable*>(w))) {
			action = activatable->get_related_action();
		}
	}

	if (action) {
		Bindings* bindings = (Bindings*) w->get_data ("ardour-bindings");
		if (!bindings) {
			Gtk::Window* win = (Gtk::Window*) w->get_toplevel();
			if (win) {
				bindings = (Bindings*) win->get_data ("ardour-bindings");
			}
		}

		if (!bindings) {
			bindings = global_bindings;
		}

		if (bindings) {
			Bindings::Operation op;
			KeyboardKey kb = bindings->get_binding_for_action (action, op);
			string shortcut = kb.display_label ();
			if (!shortcut.empty()) {
				replace_all (shortcut, "<", "");
				replace_all (shortcut, ">", "-");
				msg.append(_("\n\nShortcut: ")).append (shortcut);
			}
		}
	}

	if (req == 0) {
		return;
	}


	req->widget = w;
	req->msg = msg.c_str();
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

/** Create a PBD::EventLoop::InvalidationRecord and attach a callback
 *  to a given sigc::trackable so that PBD::EventLoop::invalidate_request
 *  is called when that trackable is destroyed.
 */
PBD::EventLoop::InvalidationRecord*
__invalidator (sigc::trackable& trackable, const char* file, int line)
{
        PBD::EventLoop::InvalidationRecord* ir = new PBD::EventLoop::InvalidationRecord;

        ir->file = file;
        ir->line = line;

        trackable.add_destroy_notify_callback (ir, PBD::EventLoop::invalidate_request);

        return ir;
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
#ifndef NDEBUG
		if (getenv ("DEBUG_THREADED_SIGNALS")) {
			cerr << "call slot for " << event_loop_name() << endl;
		}
#endif
		req->the_slot ();

	} else if (req->type == TouchDisplay) {

		req->display->touch ();
		if (req->display->delete_after_touch()) {
			delete req->display;
		}

	} else if (req->type == StateChange) {

		req->widget->set_state (req->new_state);

	} else if (req->type == SetTip) {

		gchar* old = gtk_widget_get_tooltip_markup (req->widget->gobj());
		if (
				(old && req->msg && strcmp (old, req->msg))
				||
				((old == NULL) != (req->msg == NULL || req->msg[0] == '\0'))
			 ) {
			gtk_widget_set_tooltip_markup (req->widget->gobj(), req->msg);
		}
		g_free (old);

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
UI::dump_errors (std::ostream& ostr, size_t limit)
{
	Glib::Threads::Mutex::Lock lm (error_lock);
	bool first = true;

	if (limit > 0) {
		/* reverse listing, Errors only */
		for (list<string>::reverse_iterator i = error_stack.rbegin(); i != error_stack.rend(); ++i) {
			if ((*i).substr (0, 9) == X_("WARNING: ") || (*i).substr (0, 6) == X_("INFO: ")) {
				continue;
			}
			if (first) {
				first = false;
			}
			ostr << *i << endl;
			if (--limit == 0) {
				ostr << "..." << endl;
				break;
			}
		}
	}

	if (first) {
		for (list<string>::const_iterator i = error_stack.begin(); i != error_stack.end(); ++i) {
			if (first) {
				ostr << endl << X_("Log Messages:") << endl;
				first = false;
			}
			ostr << *i << endl;
			if (limit > 0) {
				if (--limit == 0) {
					ostr << "..." << endl;
					break;
				}
			}
		}
	}
	ostr << endl;
}

void
UI::receive (Transmitter::Channel chn, const char *str)
{
	{
		Glib::Threads::Mutex::Lock lm (error_lock);
		switch (chn) {
		case Transmitter::Fatal:
			error_stack.push_back (string (X_("FATAL: ")) + str);
			break;
		case Transmitter::Error:
			error_stack.push_back (string (X_("ERROR: ")) + str);
			break;
		case Transmitter::Warning:
			error_stack.push_back (string (X_("WARNING: ")) + str);
			break;
		case Transmitter::Info:
			error_stack.push_back (string (X_("INFO: ")) + str);
			break;
		case Transmitter::Debug:
			error_stack.push_back (string (X_("Debug: ")) + str);
			break;
		case Transmitter::Throw:
			error_stack.push_back (string (X_("THROW: ")) + str);
			break;
		}
	}

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

void
UI::process_error_message (Transmitter::Channel chn, const char *str)
{
	RefPtr<Style> style;
	RefPtr<TextBuffer::Tag> ptag;
	RefPtr<TextBuffer::Tag> mtag;
	const char *prefix;
	size_t prefix_len;
	bool fatal_received = false;

	switch (chn) {
	case Transmitter::Fatal:
		prefix = "[FATAL]: ";
		ptag = fatal_ptag;
		mtag = fatal_mtag;
		prefix_len = 9;
		fatal_received = true;
		break;
	case Transmitter::Error:
		prefix = "[ERROR]: ";
		ptag = error_ptag;
		mtag = error_mtag;
		prefix_len = 9;
		break;
	case Transmitter::Warning:
		prefix = "[WARNING]: ";
		ptag = warning_ptag;
		mtag = warning_mtag;
		prefix_len = 11;
		break;
	case Transmitter::Info:
		prefix = "[INFO]: ";
		ptag = info_ptag;
		mtag = info_mtag;
		prefix_len = 8;
		break;
	case Transmitter::Debug:
		prefix = "[DEBUG]: ";
		ptag = debug_ptag;
		mtag = debug_mtag;
		prefix_len = 9;
		break;
	default:
		/* no choice but to use text/console output here */
		cerr << "programmer error in UI::check_error_messages (channel = " << chn << ")\n";
		::exit (EXIT_FAILURE);
	}

	errors->text().get_buffer()->begin_user_action();

	if (fatal_received) {
		handle_fatal (str);
	} else {

		if (!ptag || !mtag) {
			/* oops, message sent before we set up tags - don't crash */
			cerr << prefix << str << endl;
		} else {
			display_message (prefix, prefix_len, ptag, mtag, str);
		}
	}

	errors->text().get_buffer()->end_user_action();
}

void
UI::show_errors ()
{
	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Editor"), X_("toggle-log-window"));
	tact->set_active ();
}

void
UI::toggle_errors ()
{
	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Editor"), X_("toggle-log-window"));
	if (tact->get_active()) {
		errors->set_position (WIN_POS_MOUSE);
		errors->show ();
	} else {
		errors->hide ();
	}
}

void
UI::display_message (const char *prefix, gint /*prefix_len*/, RefPtr<TextBuffer::Tag> ptag, RefPtr<TextBuffer::Tag> mtag, const char *msg)
{
	RefPtr<TextBuffer> buffer (errors->text().get_buffer());
	Glib::DateTime tm (g_date_time_new_now_local ());

	buffer->insert_with_tag(buffer->end(), tm.format ("%FT%H:%M:%S "), ptag);
	buffer->insert_with_tag(buffer->end(), prefix, ptag);
	buffer->insert_with_tag(buffer->end(), msg, mtag);
	buffer->insert_with_tag(buffer->end(), "\n", mtag);

	errors->scroll_to_bottom ();
}

void
UI::handle_fatal (const char *message)
{
	Dialog win;
	Label label (message);
	Button quit (_("Press To Exit"));
	HBox hpacker;

	win.set_default_size (400, 100);

	WindowTitle title(Glib::get_application_name());
	title += ": Fatal Error";
	win.set_title (title.get_string());

	win.set_position (WIN_POS_MOUSE);
	win.set_border_width (12);

	win.get_vbox()->pack_start (label, true, true);
	hpacker.pack_start (quit, true, false);
	win.get_vbox()->pack_start (hpacker, false, false);

	quit.signal_clicked().connect(mem_fun(*this,&UI::quit));

	win.show_all ();
	win.set_modal (true);

	theMain->run ();

	_exit (1);
}

void
UI::popup_error (const string& text)
{
	if (!caller_is_ui_thread()) {
		error << "non-UI threads can't use UI::popup_error"
		      << endmsg;
		return;
	}

	MessageDialog msg (text);
	msg.set_title (string_compose (_("I'm sorry %1, I can't do that"), g_get_user_name()));
	msg.set_wmclass (X_("error"), Glib::get_application_name());
	msg.set_position (WIN_POS_MOUSE);
	msg.run ();
}

void
UI::flush_pending (float timeout)
{
	if (!caller_is_ui_thread()) {
		error << "non-UI threads cannot call UI::flush_pending()"
		      << endmsg;
		return;
	}

	int64_t end = g_get_monotonic_time () + timeout * 1e6;

	gtk_main_iteration();

	while (gtk_events_pending()) {
		if (timeout > 0 && end < g_get_monotonic_time ()) {
			cerr << "UI::flush_pending timed out after " << timeout << "s.\n";
			break;
		}
		gtk_main_iteration();
	}
}

bool
UI::just_hide_it (GdkEventAny* /*ev*/, Window *win)
{
	win->hide ();
	return true;
}

void
UI::color_selection_done (bool status)
{
	color_picked = status;
	Main::quit ();
}

bool
UI::color_selection_deleted (GdkEventAny* /*ev*/)
{
	Main::quit ();
	return true;
}
