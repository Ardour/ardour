/*
    Copyright (C) 1999 Paul Barton-Davis 

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

#ifndef __pbd_gtk_ui_h__
#define __pbd_gtk_ui_h__

#include <string>
#include <queue>
#include <map>

#include <pthread.h>
#include <gtkmm/widget.h>
#include <gtkmm/style.h>
#include <gtkmm/textbuffer.h>
#include <gtkmm/main.h>
#include <gtkmm/tooltips.h>
#include <gdkmm/color.h>
#include <pbd/abstract_ui.h>
#include <pbd/ringbufferNPT.h>
#include <pbd/atomic.h>
#include <pbd/pool.h>
#include <pbd/error.h>
#include <pbd/lockmonitor.h>

using std::string;
using std::queue;

class Touchable;

namespace Gtkmm2ext {

class TextViewer;

class UI : public AbstractUI

{
  public:
	UI (string name, int *argc, char **argv[], string rcfile);
	virtual ~UI ();

	static UI *instance() { return theGtkUI; }

	/* Abstract UI interfaces */

	bool running ();
	void quit    ();
	void kill    ();
	int  load_rcfile (string);
	void request (RequestType); 
	void run (Receiver &old_receiver);
	void call_slot (sigc::slot<void>);
	void call_slot_locked (sigc::slot<void>);
	void touch_display (Touchable *);
	void receive (Transmitter::Channel, const char *);
	void register_thread (pthread_t, string);

	bool caller_is_gui_thread () { 
		return pthread_equal (gui_thread, pthread_self());
	}

	/* Gtk-UI specific interfaces */

	void set_tip (Gtk::Widget *, const gchar *txt, const gchar *hlp = 0);
	void set_state (Gtk::Widget *w, Gtk::StateType state);
	void idle_add (int (*)(void *), void *);
	void timeout_add (unsigned int, int (*)(void *), void *);
	void popup_error (const char *text);
	void flush_pending ();
	void toggle_errors ();

	template<class T> static bool idle_delete (T *obj) { delete obj; return false; }
	template<class T> static void delete_when_idle (T *obj) {
		Glib::signal_idle().connect (bind (slot (&UI::idle_delete<T>), obj));
	}

	Gdk::Color get_color (const string& prompt, bool& picked, Gdk::Color *initial = 0);

	/* starting is sent just before we enter the main loop,
	   stopping just after we return from it (at the top level)
	*/

	sigc::signal<void> starting;
	sigc::signal<void> stopping;

	static bool just_hide_it (GdkEventAny *, Gtk::Window *);

  protected:
	virtual void handle_fatal (const char *);
	virtual void display_message (const char *prefix, gint prefix_len, 
				      Glib::RefPtr<Gtk::TextBuffer::Tag> ptag, 
				      Glib::RefPtr<Gtk::TextBuffer::Tag> mtag, 
				      const char *msg);

	/* stuff to invoke member functions in another
	   thread so that we can keep the GUI running.
	*/

	template<class UI_CLASS> struct thread_arg {
	    UI_CLASS *ui;
	    void (UI_CLASS::*func)(void *);
	    void *arg;
	};

	template<class UI_CLASS> static void *start_other_thread (void *arg);
	template<class UI_CLASS> void other_thread (void (UI_CLASS::*func)(void *), void *arg = 0);

  private:
	struct Request {

	    /* this once used anonymous unions to merge elements
	       that are never part of the same request. that makes
	       the creation of a legal copy constructor difficult
	       because of the semantics of the slot member.
	    */

	    RequestType type;
	    Touchable *display;
	    const char *msg;
	    Gtk::StateType new_state;
	    int (*function)(void *);
	    Gtk::Widget *widget;
	    Transmitter::Channel chn;
	    void *arg;
	    const char *msg2;
	    unsigned int timeout;
	    sigc::slot<void> slot;

	    /* this is for CallSlotLocked requests */

	    pthread_mutex_t slot_lock;
	    pthread_cond_t  slot_cond;

	    Request ();
	    ~Request () { 
		    if (type == ErrorMessage && msg) {
			    /* msg was strdup()'ed */
			    free ((char *)msg);
		    }
	    }
	};

	static UI *theGtkUI;
	static pthread_t gui_thread;
	bool _active;
	string _ui_name;
	Gtk::Main *theMain;
	Gtk::Tooltips *tips;
	TextViewer *errors;
	Glib::RefPtr<Gtk::TextBuffer::Tag> error_ptag;
	Glib::RefPtr<Gtk::TextBuffer::Tag> error_mtag;
	Glib::RefPtr<Gtk::TextBuffer::Tag> fatal_ptag;
	Glib::RefPtr<Gtk::TextBuffer::Tag> fatal_mtag;
	Glib::RefPtr<Gtk::TextBuffer::Tag> info_ptag;
	Glib::RefPtr<Gtk::TextBuffer::Tag> info_mtag;
	Glib::RefPtr<Gtk::TextBuffer::Tag> warning_ptag;
	Glib::RefPtr<Gtk::TextBuffer::Tag> warning_mtag;

	int signal_pipe[2];
	PBD::Lock request_buffer_map_lock;
	typedef std::map<pthread_t,RingBufferNPT<Request>* > RequestBufferMap;
	RequestBufferMap request_buffers;
	Request* get_request(RequestType);
	pthread_key_t thread_request_buffer_key;

	int setup_signal_pipe ();

	void handle_ui_requests ();
	void do_request (Request *);
	void send_request (Request *);
	static void signal_pipe_callback (void *, gint, GdkInputCondition);
	void process_error_message (Transmitter::Channel, const char *);
	void do_quit ();

	void color_selection_done (bool status);
	bool color_selection_deleted (GdkEventAny *);
	bool color_picked;
};

template<class UI_CLASS> void *
UI::start_other_thread (void *arg)

{
	thread_arg<UI_CLASS> *ta = (thread_arg<UI_CLASS> *) arg;
	(ta->ui->*ta->func)(ta->arg);
	delete ta;
	return 0;
}

template<class UI_CLASS> void
UI::other_thread (void (UI_CLASS::*func)(void *), void *arg)

{
	pthread_t thread_id;
	thread_arg<UI_CLASS> *ta = new thread_arg<UI_CLASS>;

	ta->ui = dynamic_cast<UI_CLASS *> (this);
	if (ta->ui == 0) {
		error << "UI::other thread called illegally"
		      << endmsg;
		return;
	}
	ta->func = func;
	ta->arg = arg;
	pthread_create (&thread_id, 0, start_other_thread, ta);
}


} /* namespace */

#endif /* __pbd_gtk_ui_h__ */
